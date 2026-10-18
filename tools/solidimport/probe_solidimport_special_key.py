#!/usr/bin/env python3
"""Test 11: original SolidImport Init consuming the real CLS SpecialKey mask.

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
from read_standalone_dependencies import process_names
from test_step_conversion import powershell_path

HERE = Path(__file__).resolve().parent
IMPORTER_SHA256 = 'b143da4e3f8771e04aebb7d20334244900866148dc83d242a3e0ad6938c5f82c'


def expected_files(root):
    return {root / 'Acam.exe': ACAM_2025_4_SHA256,
            root / 'Add-Ins' / 'SolidImport' / 'SolidImport.dll': IMPORTER_SHA256,
            **{root / 'CLS' / n: h for n, h in EXPECTED.items()}}


def confirmed_projection(w):
    if (not isinstance(w, dict)
        or not all(w.get(k) is True for k in ['special_key_consumer_checked',
            'special_key_projection_ready', 'special_key_supplied'])
        or w.get('special_key_full_value_verified') is not False
        or type(w.get('special_key_supported_mask')) is not int or w['special_key_supported_mask'] != 18
        or type(w.get('special_key_consumer_mask')) is not int or w['special_key_consumer_mask'] != 18):
        return False
    queries=w.get('special_key_cls_queries')
    if not isinstance(queries,list) or len(queries) not in (2,3):
        return False
    if not all(isinstance(q,dict) for q in queries):
        return False
    if any(type(q.get('access_code')) is not int or type(q.get('granted')) is not bool
           or type(q.get('validation_before')) is not int or q['validation_before'] != 101
           or type(q.get('validation_after')) is not int or q['validation_after'] != 101
           or type(q.get('thread_id')) is not int or q['thread_id'] != w.get('mfc_thread_id')
           or q.get('error_type') for q in queries):
        return False
    expected_codes=[348,347] if queries[1]['granted'] else [348,347,349]
    if [q['access_code'] for q in queries] != expected_codes:
        return False
    flag2=queries[1]['granted'] or (len(queries)==3 and queries[2]['granted'])
    value=(16 if queries[0]['granted'] else 0) | (2 if flag2 else 0)
    if type(w.get('special_key_projection_value')) is not int or w['special_key_projection_value'] != value:
        return False
    releases=w.get('special_key_release_results')
    if not isinstance(releases,list) or not all(isinstance(r,dict) for r in releases):
        return False
    if [r.get('access_code') for r in releases] != [q['access_code'] for q in queries if q['granted']]:
        return False
    return all(type(r.get('return_code')) is int
        and (r['return_code'],r.get('return_name')) in {(201,'Ok'),(202,'NotOptional'),(207,'CapacityReleasedOk')}
        and r.get('individual_release_confirmed') is (r['return_code'] in (201,207))
        and not r.get('error_type') for r in releases)


def confirmed_special_key_bridge(report, *, worker_schema='solidimport-special-key-worker-11-v1', step_capture=False):
    w = report.get('worker', {})
    boundary = w.get('host_boundary', '')
    stopped_later = (report.get('worker_exit_code') == 80 and w.get('phase') == 'host_boundary'
                     and w.get('init_returned') is False
                     and w.get('post_license_host_request') is True
                     and isinstance(boundary, str)
                     and boundary.startswith('IFrame.')
                     and w.get('post_special_key_host_request') is True)
    returned = (((report.get('worker_exit_code') == 83 and w.get('phase') == 'parasolid_captured')
                 if step_capture else (report.get('worker_exit_code') == 0 and w.get('phase') == 'completed'))
                and w.get('init_returned') is True and type(w.get('init_return_code')) is int and w['init_return_code'] == 0
                and w.get('init_succeeded') is True)
    release = (w.get('individual_release_return_code'), w.get('individual_release_return_name'))
    return (not any(report.get(k) for k in ['error', 'timeout', 'bootstrap_error', 'report_read_error', 'verification_error'])
            and type(report.get('worker_exit_code')) is int and ((stopped_later and not step_capture) or returned)
            and report.get('acam_processes_before') == report.get('acam_processes_after') == []
            and report.get('acam_observed_during_worker') is False
            and type(report.get('acam_monitor_samples')) is int and report['acam_monitor_samples'] > 0
            and report.get('source_files_unchanged') is True
            and w.get('schema') == worker_schema
            and not any(w.get(k) for k in ['error_type', 'cls_cleanup_error_type', 'cls_close_error_type'])
            and all(w.get(k) is True for k in ['source_fingerprints_checked', 'importer_loaded',
                'mfc_entry_import_matches_getter', 'mfc_thread_ready', 'cls_contract_checked',
                'init_started', 'license_getter_called', 'license_request_attempted',
                'license_request_granted', 'cls_close_attempted', 'cls_close_completed'])
            and type(w.get('mfc_thread_id')) is int and w['mfc_thread_id'] > 0
            and w.get('init_thread_id') == w['mfc_thread_id']
            and (not stopped_later or w.get('boundary_thread_id') == w['mfc_thread_id'])
            and type(w.get('sta_hresult')) is int and w['sta_hresult'] in (0, 1)
            and all(type(w.get(k)) is int and w[k] == v for k, v in {
                'host_program_level': 14, 'cls_open_return_code': 1, 'cls_reported_product': 2,
                'cls_check_month': -1, 'cls_check_year': -1, 'cls_validation_before_init': 101,
                'cls_validation_after_init': 101, 'license_validation_before': 101,
                'license_validation_after': 101, 'license_request_count': 1}.items())
            and w.get('host_profile') == 'acamLevelCORE'
            and w.get('program_level_is_selected_host_profile') is True
            and confirmed_projection(w)
            and w.get('step_conversion_attempted') is step_capture
            and w.get('full_host_verified') is False
            and w.get('release_version_verified') is False
            and not w.get('license_error_type')
            and type(w.get('individual_release_return_code')) is int
            and release in {(201, 'Ok'), (202, 'NotOptional'), (207, 'CapacityReleasedOk')}
            and w.get('individual_release_confirmed') is (release[0] in (201, 207)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / 'Hexagon' / 'ALPHACAM 2025')
    p.add_argument('--out', type=Path)
    p.add_argument('--timeout', type=int, default=90)
    a = p.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        p.error('Wymagany Windows i Python x64')
    if not 5 <= a.timeout <= 180:
        p.error('Limit: 5..180 sekund')
    out = (a.out or HERE / ('solidimport_special_key_test_11_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    temp = out / 'temp'
    temp.mkdir()
    root = a.root.resolve()
    files = expected_files(root)
    report = {'schema': 'solidimport-special-key-test-11-v1', 'captured_utc': datetime.now(timezone.utc).isoformat(),
              'importer_special_key_bridge_verified': False, 'initializer_succeeded': False,
              'step_conversion_attempted': False, 'parasolid_created': False, 'ard_created': False,
              'acam_observed_during_worker': False, 'acam_monitor_samples': 0}
    print('TEST 11: oryginalny SolidImport.dll + rzeczywiste dwie flagi SpecialKey z CLS.', flush=True)
    print('ALPHACAM musi pozostac zamkniety. To jeszcze nie jest konwersja STEP.', flush=True)
    try:
        report['acam_processes_before'] = process_names()
        if report['acam_processes_before']:
            raise RuntimeError('Zamknij ALPHACAM przed proba.')
        report['source_fingerprints'] = {str(path): digest(path) for path in files}
        if any(report['source_fingerprints'][str(path)] != h for path, h in files.items()):
            raise RuntimeError('Nieoczekiwana wersja jednego z pieciu plikow instalacji.')
        command = [str(powershell_path()), '-NoProfile', '-NonInteractive', '-STA', '-ExecutionPolicy', 'Bypass',
                   '-File', str(HERE / 'solidimport_special_key_worker.ps1'), '-Root', str(root), '-OutputDirectory', str(out)]
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
        for name, key in [('worker_result.json', 'worker'), ('bootstrap_error.json', 'bootstrap_error')]:
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
        report['importer_special_key_bridge_verified'] = confirmed_special_key_bridge(report)
        w = report.get('worker', {})
        report['initializer_succeeded'] = bool(report['importer_special_key_bridge_verified'] and w.get('init_succeeded') is True)
        (out / 'solidimport_special_key_result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        archive = Path(str(out)+'.zip')
        with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as z:
            for name in ['solidimport_special_key_result.json', 'worker_result.json', 'bootstrap_error.json',
                         'worker_checkpoints.jsonl', 'native_host_calls.jsonl', 'worker_stdout.txt', 'worker_stderr.txt']:
                if (out/name).is_file():
                    z.write(out/name, name)
    print('Dwie flagi SpecialKey przyjete przez importer: ' + ('POTWIERDZONY' if report['importer_special_key_bridge_verified'] else 'NIEPOTWIERDZONY'))
    print('Pelna inicjalizacja importera: ' + ('POTWIERDZONA' if report['initializer_succeeded'] else 'NIEPOTWIERDZONA'))
    print('Ostatnia brakujaca metoda: ' + str(report.get('worker', {}).get('host_boundary', 'brak / sprawdz raport')))
    print('PRZESLIJ ZIP: ' + str(archive))
    return 0 if report['importer_special_key_bridge_verified'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
