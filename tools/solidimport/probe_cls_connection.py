#!/usr/bin/env python3
"""Test 6.2: genuine PlanitCls.License session, without launching Acam.exe.

No STEP conversion, access-code checkout, Core:Solids claim or ILicense emulation.
Uses installed, hash-matched vendor modules through their public managed API.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time

from read_standalone_dependencies import process_names
from test_step_conversion import powershell_path

HERE = Path(__file__).resolve().parent
ACAM_2025_4_SHA256 = '02997d0f6c15d0e6c807d1be19621e128bdbf3a55ae1846e0196872766bf205d'
EXPECTED = {
    'LicManaged.dll': '0b30282755e72feefdc3b43e50c0628749e49a5bd97fa06dabc002a0851a4827',
    'CLS.dll': '5d1e001654690774794b2d6093491e6b0b51aa670b6694f6c98883c29b8f02c8',
    'ClsInit.exe': 'd594c321383c33ca4eb12eedf3f95cb2adfc269484cdea986fbe5973737ef252',
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_modules(cls_root):
    result = {}
    for name, expected in EXPECTED.items():
        path = Path(cls_root) / name
        actual = digest(path)
        if actual != expected:
            raise ValueError('Nieoczekiwana wersja ' + name)
        result[name] = {'path': str(path), 'sha256': actual, 'size_bytes': path.stat().st_size}
    return result


def confirmed_session(report):
    """Connection success is narrowly scoped and requires completed cleanup."""
    worker = report.get('worker', {})
    return (not report.get('error') and not report.get('timeout')
            and type(report.get('worker_exit_code')) is int and report['worker_exit_code'] == 0
            and report.get('acam_processes_before') == []
            and report.get('acam_processes_after') == []
            and report.get('acam_observed_during_worker') is False
            and type(report.get('acam_monitor_samples')) is int
            and report['acam_monitor_samples'] > 0
            and report.get('source_modules_unchanged') is True
            and worker.get('schema') == 'cls-managed-worker-v2'
            and worker.get('phase') == 'completed'
            and not worker.get('error') and not worker.get('cleanup_error')
            and worker.get('runtime_contract_checked') is True
            and worker.get('core_solids_verified') is False
            and worker.get('step_conversion_attempted') is False
            and worker.get('set_cls_path') is True
            and type(worker.get('observed_session_id')) is int
            and worker.get('release_version_verified') is False
            and worker.get('open_method') == 'OpenLicenseConnection'
            and worker.get('request_profile') == 'installed_cls_defaults'
            and type(worker.get('check_month')) is int and worker['check_month'] == -1
            and type(worker.get('check_year')) is int and worker['check_year'] == -1
            and worker.get('requested_product') == 2
            and type(worker.get('open_return_code')) is int
            and worker['open_return_code'] == 1
            and worker.get('open_return_name') == 'Ok'
            and type(worker.get('validation_return_code')) is int
            and worker['validation_return_code'] == 101
            and worker.get('validation_return_name') == 'Ok'
            and worker.get('reported_product') == 2
            and worker.get('close_attempted') is True
            and worker.get('close_completed') is True)


def confirmed_solids_access_request(report):
    """One genuine CLS request, never a complete ILicense/translation claim."""
    worker = report.get('worker', {})
    feature = worker.get('feature', {})
    return (confirmed_session(report)
            and report.get('source_acam_unchanged') is True
            and worker.get('source_acam_sha256') == ACAM_2025_4_SHA256
            and feature.get('api_contract_checked') is True
            and type(feature.get('requested_code')) is int and feature['requested_code'] == 390
            and feature.get('checkout_attempted') is True
            and feature.get('checkout_granted') is True
            and feature.get('release_attempted') is True
            and feature.get('release_completed') is True
            and type(feature.get('release_return_code')) is int
            and (feature['release_return_code'], feature.get('release_return_name')) in
                {(201, 'Ok'), (207, 'CapacityReleasedOk')}
            and not feature.get('release_error')
            and feature.get('validated_after_request') is True
            and type(feature.get('validation_after_return_code')) is int
            and feature['validation_after_return_code'] == 101)


def solids_access_outcome(report):
    """Separate reported API observations from the strict whole-test verdict.

    NotOptional is preserved as its own result, never rewritten as a release.
    These observations alone do not verify a session, host or entitlement policy.
    """
    worker = report.get('worker', {})
    feature = worker.get('feature', {})
    return {
        'grant_reported': feature.get('checkout_granted') is True,
        'individual_release_reported': feature.get('release_completed') is True,
        'individual_release_not_optional':
            (type(feature.get('release_return_code')) is int
             and feature['release_return_code'] == 202
             and feature.get('release_return_name') == 'NotOptional'),
        'cls_close_call_completed': worker.get('close_completed') is True,
        'strict_probe_confirmed': confirmed_solids_access_request(report),
    }


def confirmed_license_adapter(report):
    """Narrow native COM adapter evidence; no complete host/STEP assertion."""
    worker = report.get('worker', {})
    a, f = worker.get('license_adapter', {}), worker.get('feature', {})
    ordinary_release = (f.get('release_completed') is True
                        and type(f.get('release_return_code')) is int
                        and (f['release_return_code'], f.get('release_return_name')) in
                        {(201, 'Ok'), (207, 'CapacityReleasedOk')})
    non_optional = (f.get('release_completed') is False
                    and f.get('release_not_optional') is True
                    and type(f.get('release_return_code')) is int
                    and f['release_return_code'] == 202
                    and f.get('release_return_name') == 'NotOptional')
    return (confirmed_session(report)
            and report.get('source_acam_unchanged') is True
            and worker.get('source_acam_sha256') == ACAM_2025_4_SHA256
            and f.get('api_contract_checked') is True
            and type(f.get('requested_code')) is int and f['requested_code'] == 390
            and f.get('checkout_attempted') is True and f.get('checkout_granted') is True
            and f.get('release_attempted') is True and not f.get('release_error')
            and (ordinary_release or non_optional)
            and f.get('validated_after_request') is True
            and type(f.get('validation_after_return_code')) is int and f['validation_after_return_code'] == 101
            and a.get('native_roundtrip_verified') is True
            and a.get('interface_identity_matches') is True
            and a.get('owned_com_references_released') is True
            and a.get('checkout_attempted') is True and a.get('checkout_granted') is True
            and not a.get('upstream_error_type')
            and all(type(a.get(k)) is int and a[k] == v for k, v in {
                'query_interface_hresult': 0, 'core_hresult': 0, 'core_variant_bool': -1,
                'access_code': 390, 'request_count': 1,
                'validation_before': 101, 'validation_after': 101,
                'unsupported_hresult': -2147467263, 'unsupported_variant_bool': 0,
                'null_name_hresult': -2147024809, 'null_name_variant_bool': 0}.items())
            and all(a.get(k) is True for k in ['core_output_width_verified',
                'unsupported_output_width_verified', 'null_name_output_width_verified',
                'unsupported_did_not_request_cls']))


def main(*, feature_probe=False, adapter_probe=False):
    feature_probe = feature_probe or adapter_probe
    description = ('Test 9: adapter COM ILicense z rzeczywistym zapytaniem CLS, bez ALPHACAM-a.'
                   if adapter_probe else
                   'Test 8: jedno rzeczywiste zapytanie CLS o kod 390, bez uruchamiania ALPHACAM-a.'
                   if feature_probe else __doc__)
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('--root', type=Path, default=Path(os.environ.get('ProgramFiles', r'C:\Program Files')) /
                        'Hexagon' / 'ALPHACAM 2025')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--timeout', type=int, default=90)
    args = parser.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        parser.error('Wymagany Windows i Python x64')
    if not 5 <= args.timeout <= 180:
        parser.error('Timeout: 5..180 sekund')
    prefix = 'cls_adapter_test_9_' if adapter_probe else 'cls_solids_test_8_' if feature_probe else 'cls_test_6_2_'
    out = (args.out or HERE / (prefix + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    temp = out / 'temp'
    temp.mkdir()
    root, cls_root = args.root.resolve(), (args.root / 'CLS').resolve()
    report = {'schema': 'cls-connection-test-6-2-v1', 'captured_utc': datetime.now(timezone.utc).isoformat(),
              'cls_session_confirmed': False, 'core_solids_verified': False, 'release_version_verified': False,
              'step_conversion_attempted': False, 'parasolid_created': False, 'ard_created': False,
              'application_com_activated_by_test': False, 'access_code_checkout_attempted': False,
              'acam_observed_during_worker': False, 'acam_monitor_samples': 0,
              'installation_root': str(root), 'cls_directory': str(cls_root)}
    if feature_probe:
        report['schema'] = 'cls-solids-access-test-8-v1'
        report['solids_access_request_confirmed'] = False
    if adapter_probe:
        report['schema'] = 'cls-license-adapter-test-9-v1'
        report['license_adapter_verified'] = False
    modules = {}
    acam = root / 'Acam.exe'
    print('TEST 9: natywny adapter ILicense z rzeczywistym CLS.' if adapter_probe else
          'TEST 8: rzeczywiste zapytanie CLS o uprawnienie 390.' if feature_probe else
          'TEST 6.2: rzeczywista sesja CLS bez ALPHACAM-a.', flush=True)
    print('Ta proba jeszcze nie konwertuje STEP. ALPHACAM musi pozostac zamkniety.', flush=True)
    try:
        report['acam_processes_before'] = process_names()
        if report['acam_processes_before']:
            raise RuntimeError('Zamknij wszystkie wersje ALPHACAM i uruchom test ponownie.')
        modules = verify_modules(cls_root)
        report['source_modules'] = modules
        if feature_probe:
            report['source_acam_sha256'] = digest(acam)
            if report['source_acam_sha256'] != ACAM_2025_4_SHA256:
                raise RuntimeError('Acam.exe nie odpowiada przeanalizowanej wersji 2025.4.1.104.')
        command = [str(powershell_path()), '-NoProfile', '-NonInteractive', '-STA',
                   '-ExecutionPolicy', 'Bypass', '-File', str(HERE / 'cls_connection_worker.ps1'),
                   '-ClsRoot', str(cls_root), '-OutputDirectory', str(out)]
        if feature_probe:
            command += ['-ProbeSolidsAccess', '-AcamExecutable', str(acam)]
        if adapter_probe:
            command += ['-ProbeLicenseAdapter']
        env = dict(os.environ, TEMP=str(temp), TMP=str(temp))
        print('Limit: %d s. Wyniki: %s' % (args.timeout, out), flush=True)
        with (out / 'worker_stdout.txt').open('xb') as stdout, (out / 'worker_stderr.txt').open('xb') as stderr:
            child = subprocess.Popen(command, cwd=out, env=env, stdout=stdout, stderr=stderr)
            started = time.monotonic()
            try:
                while True:
                    report['acam_monitor_samples'] += 1
                    observed = process_names()
                    if observed:
                        report['acam_observed_during_worker'] = True
                        raise RuntimeError('Wykryto Acam.exe podczas proby. Wynik nie jest samodzielnym testem.')
                    if child.poll() is not None:
                        break
                    if time.monotonic() - started >= args.timeout:
                        report['timeout'] = True
                        raise TimeoutError('Przekroczony czas proby CLS; sprzatanie sesji niepotwierdzone.')
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
                # Only this diagnostic worker is terminated. Other processes,
                # including the vendor's CLS backend, are never forcibly closed.
        target = out / 'worker_result.json'
        if target.is_file():
            report['worker'] = json.loads(target.read_text(encoding='utf-8-sig'))
        else:
            raise RuntimeError('Brak raportu procesu; sprawdz logi i ostatni punkt wykonania.')
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
    finally:
        target = out / 'worker_result.json'
        if 'worker' not in report and target.is_file():
            try:
                report['worker'] = json.loads(target.read_text(encoding='utf-8-sig'))
            except (OSError, ValueError) as error:
                report['worker_report_error'] = str(error)
        try:
            report['acam_processes_after'] = process_names()
            unchanged = {name: digest(item['path']) == EXPECTED[name] for name, item in modules.items()}
            report['modules_unchanged'] = unchanged
            report['source_modules_unchanged'] = len(unchanged) == len(EXPECTED) and all(unchanged.values())
        except OSError as error:
            report['verification_error'] = str(error)
            report['source_modules_unchanged'] = False
        report['cls_session_confirmed'] = confirmed_session(report)
        if feature_probe:
            try:
                report['source_acam_unchanged'] = digest(acam) == ACAM_2025_4_SHA256
            except OSError:
                report['source_acam_unchanged'] = False
            report['access_code_checkout_attempted'] = report.get('worker', {}).get('feature', {}).get('checkout_attempted') is True
            report['solids_access_request_confirmed'] = confirmed_solids_access_request(report)
            report['solids_access_outcome'] = solids_access_outcome(report)
        if adapter_probe:
            report['license_adapter_verified'] = confirmed_license_adapter(report)
        with (out / 'cls_connection_result.json').open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
        # No contents of vendor-generated TEMP files, license files or modules
        # are put in the archive; only the diagnostic's own text files.
        import zipfile
        archive = Path(str(out) + '.zip')
        with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as zipped:
            for name in ['cls_connection_result.json', 'worker_result.json',
                         'worker_checkpoints.jsonl', 'worker_stdout.txt', 'worker_stderr.txt']:
                path = out / name
                if path.is_file():
                    zipped.write(path, name)
    if report.get('error'):
        print('Diagnostyka: ' + report['error']['message'])
    worker = report.get('worker', {})
    if worker.get('error'):
        print('Blad procesu CLS: ' + worker['error'].get('message', 'brak opisu'))
    print('Open: %s; Valid: %s' % (worker.get('open_return_name', 'brak'),
                                     worker.get('validation_return_name', 'brak')))
    print('Sesja CLS: ' + ('potwierdzona i zamknieta' if report['cls_session_confirmed'] else 'niepotwierdzona'))
    if feature_probe:
        feature = worker.get('feature', {})
        print('CLS 390: %s; zwolnienie: %s' % (feature.get('checkout_granted', 'brak'),
                                               feature.get('release_return_name', 'nie dotyczy / brak')))
        outcome = report['solids_access_outcome']
        if outcome['grant_reported']:
            print('GetAccessCode(390) zwrocilo True: CLS przyznalo uprawnienie.')
        if outcome['individual_release_not_optional']:
            print('ReleaseAccessCode(390): NotOptional (202). Indywidualne zwolnienie niepotwierdzone.')
        print('Zamkniecie polaczenia CLS bez wyjatku: ' + str(outcome['cls_close_call_completed']))
        if not adapter_probe:
            print('Caly test z indywidualnym zwolnieniem: ' + ('POTWIERDZONY' if report['solids_access_request_confirmed'] else 'NIEPOTWIERDZONY'))
    print('Pelny host Core:Solids / wydanie / STEP / ARD: NIE POTWIERDZONO')
    if adapter_probe:
        print('Natywny adapter COM ILicense: ' + ('POTWIERDZONY' if report['license_adapter_verified'] else 'NIEPOTWIERDZONY'))
    print('PRZESLIJ ZIP: ' + str(archive))
    confirmed = (report['license_adapter_verified'] if adapter_probe else
                 report['solids_access_request_confirmed'] if feature_probe else report['cls_session_confirmed'])
    return 0 if confirmed else 1


if __name__ == '__main__':
    raise SystemExit(main())
