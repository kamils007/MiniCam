#!/usr/bin/env python3
"""Test 7: ordinary SolidLink translation while a genuine CLS session is open.

No claim that the session itself supplies exporter/feature authorization.
Only installed public APIs; the original STEP is read only.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import uuid
import zipfile

from probe_cls_connection import EXPECTED, digest, verify_modules
from read_standalone_dependencies import process_names
from test_step_conversion import (CLSID, PROGID, SERVER_SHA256, Registry,
                                  inspect_output, powershell_path, referenced_file)
from read_solidlink_environment import file_metadata

HERE = Path(__file__).resolve().parent
TARGET_PROGID = 'SolidLink.Document.2025.4'


def checked_registration(registry, record):
    """Read exact known keys, retaining evidence even when the check fails.

    A ProgID is an optional alias when activating a known CLSID. Its absence
    does not invalidate a correctly registered, hash-matched local server.
    A conflicting alias, mixed server registration or unreadable key is refused.
    """
    record['requested_progid'] = PROGID
    record['expected_clsid'] = CLSID
    record['progid_clsid'] = registry.value(PROGID + r'\CLSID')
    record['progid_curver'] = registry.value(PROGID + r'\CurVer')
    class_root = 'CLSID\\' + CLSID
    record['class_progid'] = registry.value(class_root + r'\ProgID')
    record['class_version_independent_progid'] = registry.value(class_root + r'\VersionIndependentProgID')
    record['local_server_command'] = registry.value(class_root + r'\LocalServer32')
    record['inproc_server_command'] = registry.value(class_root + r'\InprocServer32')
    record['registry_errors'] = list(registry.errors)
    if registry.errors:
        raise RuntimeError('Blad odczytu rejestru SolidLink; szczegoly sa w registration.registry_errors.')
    registered = record['progid_clsid']
    if registered is not None:
        try:
            matches = uuid.UUID(registered.strip()) == uuid.UUID(CLSID)
        except (ValueError, AttributeError):
            matches = False
        if not matches:
            raise RuntimeError('ProgID SolidLink wskazuje inna lub niepoprawna klase; wartosc zapisano w registration.progid_clsid.')
        record['activation_route'] = 'verified_progid_and_clsid'
    else:
        record['activation_route'] = 'known_clsid_without_progid_alias'
    if record['inproc_server_command']:
        raise RuntimeError('Nieoczekiwana rejestracja InprocServer32 dla klasy SolidLink; przerwano przed aktywacja.')
    server_text = referenced_file(record['local_server_command'])
    if not server_text:
        raise RuntimeError('Brak czytelnego LocalServer32 znanej klasy SolidLink; odczyt zapisano w registration.')
    server = Path(server_text)
    record['server_file'] = file_metadata(record['local_server_command'])
    if digest(server) != SERVER_SHA256:
        raise RuntimeError('Nieoczekiwana wersja SolidLink.exe; sciezke i skrot zapisano w registration.server_file.')
    record['server_hash_matched'] = True
    return server


def checked_current_registration(registry, record, expected_server):
    """Resolve 2025.4 to the exact installed executable supplied by the user.

    This trial uses public Automation only. The new build is fingerprinted
    before/after; the worker checks its version and metadata before activation.
    No private ABI or offsets from SolidLink 2025.3 are reused.
    """
    expected_server = Path(expected_server).resolve()
    if expected_server.name.casefold() != 'solidlink.exe' or not expected_server.is_file():
        raise RuntimeError('Nie znaleziono wskazanego SolidLink.exe: ' + str(expected_server))
    record['requested_progid'] = TARGET_PROGID
    record['expected_server_path'] = str(expected_server)
    record['progid_clsid'] = registry.value(TARGET_PROGID + r'\CLSID')
    record['progid_curver'] = registry.value(TARGET_PROGID + r'\CurVer')
    record['registry_errors'] = list(registry.errors)
    if registry.errors:
        raise RuntimeError('Blad odczytu rejestracji SolidLink 2025.4; szczegoly sa w registration.')
    try:
        selected_clsid = '{' + str(uuid.UUID((record['progid_clsid'] or '').strip())).upper() + '}'
    except ValueError:
        raise RuntimeError('Brak poprawnego CLSID dla SolidLink.Document.2025.4; odczyt zapisano w registration.') from None
    if selected_clsid == '{00000000-0000-0000-0000-000000000000}':
        raise RuntimeError('Pusty identyfikator klasy SolidLink.')
    record['selected_clsid'] = selected_clsid
    class_root = 'CLSID\\' + selected_clsid
    record['local_server_command'] = registry.value(class_root + r'\LocalServer32')
    record['inproc_server_command'] = registry.value(class_root + r'\InprocServer32')
    record['registry_errors'] = list(registry.errors)
    if registry.errors:
        raise RuntimeError('Blad odczytu rejestracji SolidLink 2025.4; szczegoly sa w registration.')
    if record['inproc_server_command']:
        raise RuntimeError('Nieoczekiwana rejestracja InprocServer32 dla SolidLink 2025.4.')
    server_text = referenced_file(record['local_server_command'])
    if not server_text or Path(server_text).resolve() != expected_server:
        raise RuntimeError('Rejestracja SolidLink 2025.4 nie wskazuje podanego pliku; sciezki zapisano w registration.')
    record['server_file'] = file_metadata(record['local_server_command'])
    fingerprint = digest(expected_server)
    record['server_path_matched'] = True
    record['baseline_sha256'] = fingerprint
    return expected_server, selected_clsid, fingerprint


def cls_session_confirmed(report):
    """Confirm the connection across the trial, independent of translation."""
    w = report.get('worker', {})
    return (not report.get('error') and not report.get('timeout')
            and not report.get('verification_error') and not report.get('worker_report_error')
            and type(report.get('worker_exit_code')) is int and report['worker_exit_code'] == 0
            and report.get('acam_processes_before') == []
            and report.get('acam_processes_after') == []
            and report.get('solidlink_processes_before') == []
            and report.get('acam_observed_during_worker') is False
            and type(report.get('acam_monitor_samples')) is int
            and report['acam_monitor_samples'] > 0
            and report.get('source_modules_unchanged') is True
            and w.get('schema') == 'cls-solidlink-worker-v1'
            and w.get('phase') == 'completed' and not w.get('error')
            and not w.get('cleanup_error') and not w.get('acam_check_error')
            and w.get('runtime_contract_checked') is True
            and w.get('set_cls_path') is True
            and w.get('open_method') == 'OpenLicenseConnection'
            and w.get('request_profile') == 'installed_cls_defaults'
            and type(w.get('check_month')) is int and w['check_month'] == -1
            and type(w.get('check_year')) is int and w['check_year'] == -1
            and type(w.get('observed_session_id')) is int
            and w.get('requested_product') == 2
            and type(w.get('open_return_code')) is int and w['open_return_code'] == 1
            and w.get('open_return_name') == 'Ok'
            and type(w.get('validation_return_code')) is int and w['validation_return_code'] == 101
            and w.get('validation_return_name') == 'Ok'
            and type(w.get('validation_after_return_code')) is int
            and w['validation_after_return_code'] == 101
            and w.get('validation_after_return_name') == 'Ok'
            and w.get('reported_product') == 2 and w.get('reported_product_after') == 2
            and w.get('cls_validated_before_translation') is True
            and w.get('cls_validated_after_translation') is True
            and w.get('core_solids_verified') is False
            and w.get('release_version_verified') is False
            and w.get('close_attempted') is True and w.get('close_completed') is True)


def conversion_confirmed(report):
    w = report.get('worker', {})
    c = w.get('conversion', {})
    return (cls_session_confirmed(report)
            and w.get('step_conversion_attempted') is True
            and c.get('activated') is True and c.get('translation_attempted') is True
            and not c.get('error') and not c.get('cleanup_error')
            and c.get('com_release_completed') is True
            and c.get('application_argument') == 'Alphacam'
            and w.get('solidlink_api_contract_verified') is True
            and w.get('solidlink_file_major') == 2025 and w.get('solidlink_file_minor') == 4
            and type(c.get('translate_return_code')) is int and c['translate_return_code'] == 0
            and type(c.get('interop_return_code')) is int
            and c.get('output_exists') is True
            and report.get('output', {}).get('recognized_parasolid') is True
            and report.get('output_absent_before') is True
            and report.get('source_unchanged') is True
            and report.get('input_copy_unchanged') is True
            and report.get('server_unchanged') is True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(os.environ.get('ProgramFiles', r'C:\Program Files')) /
                        'Hexagon' / 'ALPHACAM 2025')
    parser.add_argument('--model', type=Path, default=HERE / 'input_model.stp')
    parser.add_argument('--solidlink', type=Path,
                        default=Path(os.environ.get('CommonProgramFiles', r'C:\Program Files\Common Files')) /
                        'Hexagon' / 'SolidLink 2025.4' / 'code' / 'bin' / 'SolidLink.exe')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--timeout', type=int, default=90)
    args = parser.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        parser.error('Wymagany Windows i Python x64.')
    if not 5 <= args.timeout <= 180:
        parser.error('Timeout: 5..180 sekund.')
    model = args.model.resolve()
    if not model.is_file() or model.suffix.lower() not in {'.stp', '.step'}:
        parser.error('Brak pliku STEP: ' + str(model))
    out = (args.out or HERE / ('cls_solidlink_test_7_2_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    temp = out / 'temp'
    temp.mkdir()
    source_hash = digest(model)
    input_copy, target = out / 'input.stp', out / 'converted.x_t'
    root, cls_root = args.root.resolve(), (args.root / 'CLS').resolve()
    report = {'schema': 'cls-solidlink-test-7-2-v1', 'captured_utc': datetime.now(timezone.utc).isoformat(),
              'cls_session_confirmed': False, 'conversion_succeeded': False,
              'step_conversion_attempted': False, 'parasolid_created': False, 'ard_created': False,
              'core_solids_verified': False, 'release_version_verified': False,
              'independent_geometry_validation': False,
              'license_configuration_modified': False, 'alphacam_com_activated_by_test': False,
              'license_material_injected': False, 'access_code_checkout_attempted_by_test': False,
              'acam_observed_during_worker': False, 'acam_monitor_samples': 0,
              'input': {'filename': model.name, 'sha256': source_hash, 'size_bytes': model.stat().st_size},
              'progid': TARGET_PROGID, 'installation_root': str(root),
              'requested_solidlink_executable': str(args.solidlink.resolve())}
    modules, server, server_hash = {}, None, None
    print('TEST 7.2: STEP -> Parasolid, CLS + SolidLink 2025.4.', flush=True)
    print('ALPHACAM musi pozostac zamkniety. Limit: %d s.' % args.timeout, flush=True)
    try:
        report['acam_processes_before'] = process_names()
        report['solidlink_processes_before'] = process_names(('solidlink.exe',))
        if report['acam_processes_before']:
            raise RuntimeError('Zamknij wszystkie wersje ALPHACAM i uruchom probe ponownie.')
        if report['solidlink_processes_before']:
            raise RuntimeError('Pozostal proces SolidLink.exe. Zamknij go po zakonczeniu innych prac i ponow probe.')
        modules = verify_modules(cls_root)
        report['source_modules'] = modules
        import winreg
        registry = Registry(winreg, winreg.KEY_WOW64_64KEY)
        report['registration'] = {}
        server, selected_clsid, server_hash = checked_current_registration(registry, report['registration'], args.solidlink)
        report['server'] = {'path': str(server), 'sha256': server_hash, 'clsid': selected_clsid}
        with model.open('rb') as source, input_copy.open('xb') as dest:
            shutil.copyfileobj(source, dest)
        if digest(input_copy) != source_hash:
            raise RuntimeError('STEP zmienil sie podczas kopiowania.')
        report['output_absent_before'] = not target.exists()
        if not report['output_absent_before']:
            raise RuntimeError('Plik wynikowy juz istnieje.')
        command = [str(powershell_path()), '-NoProfile', '-NonInteractive', '-STA',
                   '-ExecutionPolicy', 'Bypass', '-File', str(HERE / 'cls_solidlink_worker.ps1'),
                   '-ClsRoot', str(cls_root), '-OutputDirectory', str(out),
                   '-SolidLinkServer', str(server), '-ClassId', selected_clsid,
                   '-ExpectedServerSha256', server_hash,
                   '-InputModel', str(input_copy), '-OutputModel', str(target)]
        env = dict(os.environ, TEMP=str(temp), TMP=str(temp))
        print('Folder wynikow: ' + str(out), flush=True)
        with (out / 'worker_stdout.txt').open('xb') as stdout, (out / 'worker_stderr.txt').open('xb') as stderr:
            child = subprocess.Popen(command, cwd=out, env=env, stdout=stdout, stderr=stderr)
            started = time.monotonic()
            try:
                while True:
                    report['acam_monitor_samples'] += 1
                    if process_names():
                        report['acam_observed_during_worker'] = True
                        raise RuntimeError('Wykryto Acam.exe podczas proby; wynik nie potwierdza konwersji bez aplikacji.')
                    if child.poll() is not None:
                        break
                    if time.monotonic() - started >= args.timeout:
                        report['timeout'] = True
                        raise TimeoutError('Limit czasu. Zamkniecie sesji CLS i COM niepotwierdzone.')
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
        if not (out / 'worker_result.json').is_file():
            raise RuntimeError('Brak raportu procesu; sprawdz logi i ostatni punkt wykonania.')
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        if 'registration' in report:
            # Metadata only for the previously observed installation location.
            # Never launch an unregistered executable or alter COM registration.
            known_exe = args.solidlink.resolve()
            report['known_installation_executable'] = file_metadata('"' + str(known_exe) + '"')
            registry32 = Registry(winreg, winreg.KEY_WOW64_32KEY)
            report['registration_32bit'] = {
                'progid_clsid': registry32.value(TARGET_PROGID + r'\CLSID'),
                'progid_curver': registry32.value(TARGET_PROGID + r'\CurVer'),
                'errors': registry32.errors,
            }
    finally:
        worker_path = out / 'worker_result.json'
        if worker_path.is_file():
            try:
                report['worker'] = json.loads(worker_path.read_text(encoding='utf-8-sig'))
            except (OSError, ValueError) as error:
                report['worker_report_error'] = str(error)
        report['step_conversion_attempted'] = report.get('worker', {}).get('step_conversion_attempted') is True
        try:
            report['acam_processes_after'] = process_names()
            report['solidlink_processes_after'] = process_names(('solidlink.exe',))
            report['source_modules_unchanged'] = (len(modules) == len(EXPECTED) and
                all(digest(v['path']) == EXPECTED[k] for k, v in modules.items()))
            report['source_unchanged'] = digest(model) == source_hash
            report['input_copy_unchanged'] = input_copy.is_file() and digest(input_copy) == source_hash
            report['server_unchanged'] = bool(server and server_hash and digest(server) == server_hash)
            report['output'] = inspect_output(target)
        except OSError as error:
            report['verification_error'] = str(error)
            report['source_modules_unchanged'] = False
        report['cls_session_confirmed'] = cls_session_confirmed(report)
        report['conversion_succeeded'] = conversion_confirmed(report)
        report['parasolid_created'] = report['conversion_succeeded']
        with (out / 'cls_solidlink_result.json').open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
        archive = Path(str(out) + '.zip')
        # Deliberate whitelist: no license files, vendor logs, TEMP, DLLs or input copies.
        with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as zipped:
            for name in ['cls_solidlink_result.json', 'worker_result.json', 'worker_checkpoints.jsonl',
                         'worker_stdout.txt', 'worker_stderr.txt', 'converted.x_t']:
                path = out / name
                if path.is_file():
                    zipped.write(path, name)
    worker = report.get('worker', {})
    if report.get('error'):
        print('Diagnostyka: ' + report['error']['message'])
    for scope in [worker, worker.get('conversion', {})]:
        if scope.get('error'):
            print('Blad procesu: ' + scope['error'].get('message', 'brak opisu'))
    print('Sesja CLS przez probe: ' + ('potwierdzona' if report['cls_session_confirmed'] else 'niepotwierdzona'))
    print('TranslateSolid: ' + str(worker.get('conversion', {}).get('translate_return_code', 'brak')))
    print('STEP -> Parasolid: ' + ('potwierdzono kod 0 i naglowek pliku' if report['conversion_succeeded'] else 'niepotwierdzone'))
    print('ARD nie byl tworzony. Geometria wyniku wymaga osobnej weryfikacji.')
    print('PRZESLIJ ZIP: ' + str(archive))
    return 0 if report['conversion_succeeded'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
