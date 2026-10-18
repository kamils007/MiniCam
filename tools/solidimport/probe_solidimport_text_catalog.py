#!/usr/bin/env python3
"""Test 12: original SolidImport Init consuming actual installed PO resources.

Stops at unsupported host services. Supplies only the verified consumed mask,
never a guessed full SpecialKey. Does not attempt STEP conversion.
Windows x64 / Python standard library only.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import zipfile

from probe_cls_connection import ACAM_2025_4_SHA256, EXPECTED, digest
from probe_solidimport_special_key import confirmed_special_key_bridge
from text_catalog import prepare_catalogs, sources_unchanged
from read_standalone_dependencies import process_names
from test_step_conversion import powershell_path

HERE = Path(__file__).resolve().parent
IMPORTER_SHA256 = 'b143da4e3f8771e04aebb7d20334244900866148dc83d242a3e0ad6938c5f82c'


def expected_files(root):
    return {root / 'Acam.exe': ACAM_2025_4_SHA256,
            root / 'Add-Ins' / 'SolidImport' / 'SolidImport.dll': IMPORTER_SHA256,
            **{root / 'CLS' / n: h for n, h in EXPECTED.items()}}


def confirmed_catalog_bridge(report, *, worker_schema='solidimport-text-catalog-worker-12-v1', step_capture=False):
    if not confirmed_special_key_bridge(report,worker_schema=worker_schema,step_capture=step_capture):
        return False
    w=report['worker'];catalog=report.get('catalog',{})
    if (report.get('catalog_sources_unchanged') is not True
        or w.get('text_catalog_sources_unchanged') is not True
        or w.get('text_catalog_map_checked') is not True
        or w.get('text_catalog_error_type')):
        return False
    selected=catalog.get('selected',{});reads=w.get('text_catalog_reads')
    if (not isinstance(selected,dict) or 'solidimport' not in selected
        or not isinstance(reads,list) or not reads
        or type(w.get('text_catalog_served_count')) is not int or w['text_catalog_served_count']!=len(reads)):
        return False
    for entry in reads:
        if not isinstance(entry,dict):return False
        source=selected.get(entry.get('catalog'),{})
        expected=source.get('entry_sha256',{}).get(entry.get('key'))
        if (not expected or entry.get('value_utf8_sha256')!=expected
            or entry.get('source_sha256')!=source.get('sha256')
            or type(entry.get('thread_id')) is not int or entry['thread_id']!=w['mfc_thread_id']
            or type(entry.get('character_count')) is not int or entry['character_count']<1):
            return False
        names={'solidimport':{'SolidImport.po'},'aedit':{'aedit.ctx','aedit.po'}}
        if str(entry.get('requested_file','')).casefold() not in {n.casefold() for n in names.get(entry.get('catalog'),set())}:
            return False
    if not any(entry.get('catalog')=='solidimport' and entry.get('key')=='10-1' for entry in reads):
        return False
    if not w.get('init_returned') and w.get('post_text_catalog_host_request') is not True:
        return False
    return True


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / 'Hexagon' / 'ALPHACAM 2025')
    p.add_argument('--out', type=Path)
    p.add_argument('--catalog-dir',type=Path,help='Optional exact language folder')
    p.add_argument('--language',default='pl-PL')
    p.add_argument('--timeout', type=int, default=90)
    a = p.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        p.error('Wymagany Windows i Python x64')
    if not 5 <= a.timeout <= 180:
        p.error('Limit: 5..180 sekund')
    out = (a.out or HERE / ('solidimport_text_catalog_test_12_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    temp = out / 'temp'
    temp.mkdir()
    root = a.root.resolve()
    files = expected_files(root)
    report = {'schema': 'solidimport-text-catalog-test-12-v1', 'captured_utc': datetime.now(timezone.utc).isoformat(),
              'text_catalog_bridge_verified': False, 'initializer_succeeded': False,
              'step_conversion_attempted': False, 'parasolid_created': False, 'ard_created': False,
              'acam_observed_during_worker': False, 'acam_monitor_samples': 0}
    print('TEST 12: oryginalny SolidImport.dll + rzeczywiste katalogi tekstow PO.', flush=True)
    print('ALPHACAM musi pozostac zamkniety. To jeszcze nie jest konwersja STEP.', flush=True)
    try:
        report['acam_processes_before'] = process_names()
        if report['acam_processes_before']:
            raise RuntimeError('Zamknij ALPHACAM przed proba.')
        report['source_fingerprints'] = {str(path): digest(path) for path in files}
        if any(report['source_fingerprints'][str(path)] != h for path, h in files.items()):
            raise RuntimeError('Nieoczekiwana wersja jednego z pieciu plikow instalacji.')
        report['catalog']=prepare_catalogs(root,out,a.language,a.catalog_dir)
        command = [str(powershell_path()), '-NoProfile', '-NonInteractive', '-STA', '-ExecutionPolicy', 'Bypass',
                   '-File', str(HERE / 'solidimport_text_catalog_worker.ps1'), '-Root', str(root), '-OutputDirectory', str(out)]
        with (out / 'worker_stdout.txt').open('xb') as stdout, (out / 'worker_stderr.txt').open('xb') as stderr:
            child = subprocess.Popen(command, cwd=out, env=dict(os.environ, TEMP=str(temp), TMP=str(temp)), stdout=stdout, stderr=stderr)
            started = time.monotonic()
            try:
                while True:
                    report['acam_monitor_samples'] += 1
                    if process_names():
                        report['acam_observed_during_worker'] = True
                        raise RuntimeError('Wykryto Acam.exe w czasie proby.')
                    if child.poll() is not None:
                        break
                    if time.monotonic() - started >= a.timeout:
                        report['timeout'] = True
                        raise TimeoutError('Limit czasu. Zamkniecie sesji CLS niepotwierdzone.')
                    time.sleep(0.25)
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
        report['text_catalog_bridge_verified'] = confirmed_catalog_bridge(report)
        w = report.get('worker', {})
        report['initializer_succeeded'] = bool(report['text_catalog_bridge_verified'] and w.get('init_succeeded') is True)
        (out / 'solidimport_text_catalog_result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        archive = Path(str(out)+'.zip')
        with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as z:
            for name in ['solidimport_text_catalog_result.json', 'worker_result.json', 'bootstrap_error.json',
                         'worker_checkpoints.jsonl', 'native_host_calls.jsonl', 'worker_stdout.txt', 'worker_stderr.txt','catalog_report.json']:
                if (out/name).is_file():
                    z.write(out/name, name)
    print('Rzeczywiste teksty PO przyjete przez importer: ' + ('POTWIERDZONY' if report['text_catalog_bridge_verified'] else 'NIEPOTWIERDZONY'))
    print('Pelna inicjalizacja importera: ' + ('POTWIERDZONA' if report['initializer_succeeded'] else 'NIEPOTWIERDZONA'))
    print('Ostatnia brakujaca metoda: ' + str(report.get('worker', {}).get('host_boundary', 'brak / sprawdz raport')))
    print('PRZESLIJ ZIP: ' + str(archive))
    return 0 if report['text_catalog_bridge_verified'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
