#!/usr/bin/env python3
"""Test 13: original SolidImport StepInputFile; capture before ActiveDrawing.

Stops at unsupported host services. Supplies only the verified consumed mask,
never a guessed full SpecialKey. Calls the original STEP export after Init=0.
Windows x64 / Python standard library only. Geometry validation is separate.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import struct
import shutil
import subprocess
import sys
import time
import zipfile

from probe_cls_connection import ACAM_2025_4_SHA256, EXPECTED, digest
from step_capture_support import (SERVER_SHA256,check_registration,file_digest,process_image,confirmed_capture)
from test_step_conversion import inspect_output
from read_solidlink_environment import Registry
from text_catalog import prepare_catalogs, sources_unchanged
from read_standalone_dependencies import process_names
from test_step_conversion import powershell_path

HERE = Path(__file__).resolve().parent
IMPORTER_SHA256 = 'b143da4e3f8771e04aebb7d20334244900866148dc83d242a3e0ad6938c5f82c'


def expected_files(root):
    return {root / 'Acam.exe': ACAM_2025_4_SHA256,
            root / 'Add-Ins' / 'SolidImport' / 'SolidImport.dll': IMPORTER_SHA256,
            **{root / 'CLS' / n: h for n, h in EXPECTED.items()}}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / 'Hexagon' / 'ALPHACAM 2025')
    p.add_argument('--out', type=Path)
    p.add_argument('--model',type=Path,default=HERE/'input_model.stp')
    p.add_argument('--solidlink-server',type=Path,default=Path(os.environ.get('CommonProgramFiles',r'C:\Program Files\Common Files'))/'Hexagon'/'SolidLink 2025.4'/'code'/'bin'/'SolidLink.exe')
    p.add_argument('--catalog-dir',type=Path,help='Optional exact language folder')
    p.add_argument('--language',default='pl-PL')
    p.add_argument('--timeout', type=int, default=120)
    a = p.parse_args(argv)
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        p.error('Wymagany Windows i Python x64')
    if not 5 <= a.timeout <= 300:
        p.error('Limit: 5..300 sekund')
    out = (a.out or HERE / ('solidimport_step_capture_test_13_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    temp = out / 'temp'
    temp.mkdir()
    root = a.root.resolve()
    files = expected_files(root)
    report = {'schema': 'solidimport-step-capture-test-13-v1', 'captured_utc': datetime.now(timezone.utc).isoformat(),
              'parasolid_capture_verified': False, 'initializer_succeeded': False,
              'step_conversion_attempted': False, 'parasolid_created': False, 'ard_created': False,
              'acam_observed_during_worker': False, 'acam_monitor_samples': 0,
              'observed_servers':[], 'geometry_verified':False}
    print('TEST 13: oryginalne Init -> StepInputFile -> przechwycenie Parasolidu.', flush=True)
    print('ALPHACAM pozostaje zamkniety. Pracujemy na kopii STEP-a; ARD jeszcze nie powstaje.', flush=True)
    server=None;model=None;copy=None;seen_pids=set()
    try:
        report['acam_processes_before'] = process_names()
        if report['acam_processes_before']:
            raise RuntimeError('Zamknij ALPHACAM przed proba.')
        report['solidlink_processes_before']=process_names(('solidlink.exe',))
        if report['solidlink_processes_before']:
            raise RuntimeError('SolidLink.exe juz dziala. Zamknij go przed nowa izolowana proba.')
        import winreg
        report['registration']={}
        server=check_registration(Registry(winreg,winreg.KEY_WOW64_64KEY),a.solidlink_server,report['registration'])
        model=a.model.resolve()
        if not model.is_file() or model.stat().st_size>256*1024*1024:
            raise ValueError('Brak pliku STEP lub rozmiar ponad 256 MiB')
        with model.open('rb') as stream:header=stream.read(4096)
        if not header.lstrip(b'\xef\xbb\xbf \r\n\t').startswith(b'ISO-10303-21;'):
            raise ValueError('Nie rozpoznano naglowka STEP')
        report['input_sha256']=file_digest(model);report['input_source']=str(model)
        (out/'input').mkdir();copy=out/'input'/'model.stp';shutil.copyfile(model,copy)
        if file_digest(copy)!=report['input_sha256']:raise ValueError('Kopia STEP niezgodna')
        (temp/'step_request.json').write_text(json.dumps({'schema':'solidimport-step-request-13-v1',
            'input_path':str(copy),'sha256':report['input_sha256']}),encoding='utf-8')
        report['source_fingerprints'] = {str(path): digest(path) for path in files}
        if any(report['source_fingerprints'][str(path)] != h for path, h in files.items()):
            raise RuntimeError('Nieoczekiwana wersja jednego z pieciu plikow instalacji.')
        report['catalog']=prepare_catalogs(root,out,a.language,a.catalog_dir)
        command = [str(powershell_path()), '-NoProfile', '-NonInteractive', '-STA', '-ExecutionPolicy', 'Bypass',
                   '-File', str(HERE / 'solidimport_step_capture_worker.ps1'), '-Root', str(root), '-OutputDirectory', str(out)]
        with (out / 'worker_stdout.txt').open('xb') as stdout, (out / 'worker_stderr.txt').open('xb') as stderr:
            child = subprocess.Popen(command, cwd=out, env=dict(os.environ, TEMP=str(temp), TMP=str(temp)), stdout=stdout, stderr=stderr)
            started = time.monotonic()
            try:
                while True:
                    report['acam_monitor_samples'] += 1
                    processes=process_names(('acam.exe','solidlink.exe'))
                    for process in processes:
                        if process['name'].casefold()!='solidlink.exe' or process['pid'] in seen_pids:continue
                        try:
                            image=process_image(process['pid'])
                            item={'pid':process['pid'],'path':str(image),'sha256':file_digest(image),
                                  'path_matches_registration':image==server}
                        except OSError as error:
                            report.setdefault('server_query_errors',[]).append(str(error));continue
                        seen_pids.add(process['pid']);report['observed_servers'].append(item)
                        if item['sha256']!=SERVER_SHA256 or not item['path_matches_registration']:
                            report['server_monitor_error']='Unexpected SolidLink image'
                            raise RuntimeError('Wykryto inny SolidLink.exe niz oczekiwany')
                    if any(process['name'].casefold()=='acam.exe' for process in processes):
                        report['acam_observed_during_worker'] = True
                        raise RuntimeError('Wykryto Acam.exe w czasie proby.')
                    if child.poll() is not None:
                        break
                    if time.monotonic() - started >= a.timeout:
                        report['timeout'] = True
                        raise TimeoutError('Limit czasu. Zamkniecie sesji CLS niepotwierdzone.')
                    time.sleep(0.1)
            finally:
                if child.poll() is None:
                    child.terminate()
                    try:
                        child.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        child.kill()
                        child.wait(timeout=5)
                report['worker_exit_code'] = child.returncode
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
    finally:
        for name, key in [('worker_result.json', 'worker'), ('bootstrap_error.json', 'bootstrap_error'), ('catalog_report.json','catalog')]:
            try:
                path = out / name
                if path.is_file():
                    report[key] = json.loads(path.read_text(encoding='utf-8-sig'))
            except (OSError, ValueError) as error:
                report['report_read_error'] = str(error)
        try:
            report['acam_processes_after'] = process_names()
            report['source_files_unchanged'] = all(digest(path) == h for path, h in files.items())
        except OSError as error:
            report['verification_error'] = str(error)
            report['source_files_unchanged'] = False
        try:
            report['catalog_sources_unchanged']=sources_unchanged(report.get('catalog',{}))
        except OSError as error:
            report['catalog_sources_unchanged']=False
            report['catalog_verification_error']=str(error)
        try:
            report['source_unchanged']=bool(model and file_digest(model)==report.get('input_sha256'))
            report['input_copy_unchanged']=bool(copy and file_digest(copy)==report.get('input_sha256'))
            report['server_unchanged']=bool(server and file_digest(server)==SERVER_SHA256)
        except OSError as error:
            report['verification_error']=str(error)
        w=report.get('worker',{})
        capture_name=w.get('capture',{}).get('filename')
        if capture_name in ('captured.x_t','captured.x_b'):
            try:report['captured_file']=inspect_output(out/capture_name)
            except OSError as error:report['verification_error']=str(error)
        report['step_conversion_attempted']=w.get('step_conversion_attempted') is True
        report['parasolid_capture_verified']=confirmed_capture(report)
        report['parasolid_created']=report['parasolid_capture_verified']
        w = report.get('worker', {})
        report['initializer_succeeded'] = bool(w.get('init_returned') is True and type(w.get('init_return_code')) is int
            and w['init_return_code']==0 and w.get('init_succeeded') is True and report.get('source_files_unchanged') is True
            and report.get('acam_observed_during_worker') is False and report.get('acam_processes_before')==report.get('acam_processes_after')==[])
        (out / 'solidimport_step_capture_result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        archive = Path(str(out)+'.zip')
        with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as z:
            for name in ['solidimport_step_capture_result.json', 'worker_result.json', 'bootstrap_error.json',
                         'worker_checkpoints.jsonl', 'native_host_calls.jsonl', 'worker_stdout.txt', 'worker_stderr.txt','catalog_report.json','captured.x_t','captured.x_b']:
                if (out/name).is_file():
                    z.write(out/name, name)
    print('Przechwycenie Parasolidu przed rysunkiem: ' + ('POTWIERDZONY' if report['parasolid_capture_verified'] else 'NIEPOTWIERDZONY'))
    print('Pelna inicjalizacja importera: ' + ('POTWIERDZONA' if report['initializer_succeeded'] else 'NIEPOTWIERDZONA'))
    print('Ostatnia brakujaca metoda: ' + str(report.get('worker', {}).get('host_boundary', 'brak / sprawdz raport')))
    if report.get('error'):print('Powod: '+report['error']['message'])
    if report.get('worker',{}).get('error_message'):print('Proces: '+report['worker']['error_message'])
    print('PRZESLIJ ZIP: ' + str(archive))
    return 0 if report['parasolid_capture_verified'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
