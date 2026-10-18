"""Test 15: one invocation, fresh STEP -> Parasolid -> one-body diagnostic ARD.

Reuses the verified test-13 capture and test-14.5 writer without replaying a
previous Parasolid. Installed Windows vendor modules and current CLS required.
"""
import argparse
from contextlib import redirect_stdout
from datetime import datetime
import json
from pathlib import Path
import shutil
import struct
import sys
import zipfile

from build_step_ard_14 import (HERE, INSTALLATION, STEP_SHA256, PO_SHA256,
    digest, read_json, run_native, collect_feature, validate_cls,
    validate_projection, validate_measurement, validate_schema_path)
from build_step_ard import BASE_SHA256, check_feature_stage, require_native_success, wrap_ard
from feature_builds import CURRENT_SHA256
from parasolid_errors import KERNEL_SHA256
from read_standalone_dependencies import process_names
from test_step_conversion import inspect_output
import probe_solidimport_step_capture as capture_stage
from step_capture_support import confirmed_capture, SERVER_SHA256
from text_catalog import sources_unchanged

CAPTURE_FILES = ('solidimport_step_capture_result.json', 'worker_result.json',
    'bootstrap_error.json', 'worker_checkpoints.jsonl', 'native_host_calls.jsonl',
    'worker_stdout.txt', 'worker_stderr.txt', 'catalog_report.json', 'captured.x_t', 'captured.x_b')
FEATURE_FILES = ('cls_ready.json', 'cls_result.json', 'cls_stdout.txt', 'cls_stderr.txt',
    'feature_stdout.txt', 'feature_stderr.txt', 'native/host_calls.jsonl',
    'native/native_exceptions.jsonl', 'native/feature_object.bin',
    'native/roundtrip.x_b', 'native/ard_geometry.x_b')


def validate_step(path):
    path = Path(path)
    if not path.is_file() or not 0 < path.stat().st_size <= 256 * 1024 * 1024:
        raise ValueError('Brak pliku STEP lub rozmiar ponad 256 MiB')
    with path.open('rb') as stream:
        header = stream.read(4096)
    if not header.lstrip(b'\xef\xbb\xbf \r\n\t').startswith(b'ISO-10303-21;'):
        raise ValueError('Nie rozpoznano naglowka STEP')
    return digest(path)


def validate_fresh_capture(directory, source_sha256, exit_code):
    directory = Path(directory)
    report = read_json(directory / 'solidimport_step_capture_result.json')
    if (type(exit_code) is not int or exit_code != 0 or report.get('input_sha256') != source_sha256
            or report.get('parasolid_capture_verified') is not True
            or report.get('initializer_succeeded') is not True or not confirmed_capture(report)):
        raise ValueError('Swieza konwersja STEP niepotwierdzona; zobacz raport step_capture')
    if report['worker'] != read_json(directory / 'worker_result.json'):
        raise ValueError('Capture worker report differs from parent report')
    if report.get('catalog') != read_json(directory / 'catalog_report.json'):
        raise ValueError('Capture catalog report differs from parent report')
    if digest(directory / 'input/model.stp') != source_sha256:
        raise ValueError('Actual STEP copy differs from selected source')
    name = report['worker']['capture']['filename']
    if name not in ('captured.x_t', 'captured.x_b'):
        raise ValueError('Unexpected captured filename')
    path = directory / name
    actual = inspect_output(path)
    expected = report['captured_file']
    if (actual.get('recognized_parasolid') is not True
            or any(actual.get(key) != expected.get(key) for key in ('size_bytes', 'sha256', 'format'))):
        raise ValueError('Actual fresh Parasolid bytes differ from capture proof')
    return report, path, actual


def validate_feature_input(stage, private_model, expected_sha256):
    actual = stage.get('result', {}).get('input_copy', {})
    if (actual.get('unchanged') is not True or actual.get('sha256_before') != expected_sha256
            or actual.get('sha256_after') != expected_sha256
            or actual.get('path') != str(Path(private_model).resolve())
            or digest(private_model) != expected_sha256):
        raise ValueError('Feature child did not confirm the exact fresh Parasolid input')


def validate_kernel(report):
    diagnostics = report.get('kernel_diagnostics', {})
    api = diagnostics.get('api') or {}
    if (api.get('module_sha256') != KERNEL_SHA256 or api.get('iat_export_identity_matches') is not True
            or diagnostics.get('pskernel_file_unchanged') is not True):
        raise ValueError('Loaded Parasolid kernel identity/stability not confirmed')
    for name in ('before_save', 'after_save', 'before_record', 'after_record'):
        value = diagnostics.get(name) or {}
        if (value.get('query_return_code') != 0 or value.get('was_error') is not False
                or value.get('output_guards_intact') is not True or value.get('error_clear_called') is not False):
            raise ValueError('Parasolid reported an error or invalid diagnostics at ' + name)


def archive_result(out, success):
    names = ['step_ard_result.json', 'step_capture_stage.txt']
    names += ['step_capture/' + name for name in CAPTURE_FILES]
    names += ['feature/' + name for name in FEATURE_FILES]
    if success:
        names.append('STEP_TEST_15.ard')
    target = out.with_suffix('.zip')
    with zipfile.ZipFile(target, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for name in names:
            path = out / name
            if path.is_file():
                archive.write(path, name)
    return target


def convert(model, root, server, out, expected_dimensions=None, catalog_dir=None, language='pl-PL'):
    model, root, server, out = (Path(p).resolve() for p in (model, root, server, out))
    out.mkdir(parents=True, exist_ok=False)
    base, po = HERE / 'data/1_test.ard', HERE.parent / 'alphacam_reference/Feature.po'
    files = {root / name: sha for name, sha in INSTALLATION.items()}
    files.update({root / 'Add-Ins/Feature/Feature.dll': CURRENT_SHA256,
                  root / 'Add-Ins/Feature/PSKERNEL.dll': KERNEL_SHA256,
                  root / 'Add-Ins/SolidImport/SolidImport.dll': capture_stage.IMPORTER_SHA256,
                  server: SERVER_SHA256, base: BASE_SHA256, po: PO_SHA256})
    report = {'schema': 'step-to-ard-integrated-15-v1', 'test': '15', 'ard_created': False,
              'step_repeated_in_this_test': False, 'fresh_capture_verified': False,
              'acam_observed': False, 'process_samples': 0, 'geometry_verified': False,
              'volume_verified': False, 'opened_in_alphacam': False,
              'single_body_only': True, 'source_step': str(model)}
    native = {'direct_record': True, 'kernel_error_diagnostics': True,
              'configure_schema_path': True, 'acam_observed': False}
    capture, captured, private_model, captured_sha = None, None, None, None
    feature_dir = out / 'feature'
    target = out / 'STEP_TEST_15.ard'
    own_ard = False

    def fingerprints():
        return [{'path': str(path), 'expected_sha256': expected,
                 'sha256': digest(path) if path.is_file() else None} for path, expected in files.items()]

    def observe():
        report['process_samples'] += 1
        found = process_names()
        if found:
            report['acam_observed'] = True
            report['observed_acam'] = found
            raise RuntimeError('ALPHACAM musi pozostac zamkniety do konca testu')

    def final_audit():
        report['files_after'] = fingerprints()
        if (report.get('files_before') != report['files_after']
                or any(item['sha256'] != item['expected_sha256'] for item in report['files_after'])):
            raise ValueError('Installation or packaged source files changed during the test')
        if digest(model) != report.get('source_step_sha256'):
            raise ValueError('Original STEP changed during the test')
        if capture is not None:
            if (not sources_unchanged(capture['catalog'])
                    or digest(out / 'step_capture/input/model.stp') != report['source_step_sha256']
                    or digest(captured) != captured_sha or digest(private_model) != captured_sha):
                raise ValueError('Fresh STEP/capture/catalog chain changed during the test')
        observe()
        report['inputs_and_installation_unchanged'] = True

    try:
        observe()
        report['files_before'] = fingerprints()
        if any(item['sha256'] != item['expected_sha256'] for item in report['files_before']):
            raise ValueError('Inna wersja lub brak pliku instalacji/wejscia; zobacz files_before')
        report['source_step_sha256'] = validate_step(model)
        reference = expected_dimensions
        if reference is None and report['source_step_sha256'] == STEP_SHA256:
            reference = [1200.0, 2300.0, 10.0]
        report['expected_dimensions_mm'] = reference
        print('ETAP 1/2: swiezy STEP -> Parasolid (SolidImport / SolidLink).', flush=True)
        args = ['--root', str(root), '--out', str(out / 'step_capture'),
                '--model', str(model), '--solidlink-server', str(server), '--language', language]
        if catalog_dir:
            args += ['--catalog-dir', str(Path(catalog_dir).resolve())]
        with (out / 'step_capture_stage.txt').open('x', encoding='utf-8') as log, redirect_stdout(log):
            code = capture_stage.main(args)
        capture_report, captured, actual = validate_fresh_capture(out / 'step_capture', report['source_step_sha256'], code)
        captured_sha = actual['sha256']
        feature_dir.mkdir()
        private_model = feature_dir / ('input_copy' + captured.suffix)
        shutil.copyfile(captured, private_model)
        if digest(private_model) != captured_sha:
            raise ValueError('Fresh Parasolid copy hash mismatch')
        capture = capture_report
        report.update(fresh_capture_verified=True, step_repeated_in_this_test=True,
                      capture=actual, capture_exit_code=code,
                      capture_worker_exit_code=capture['worker_exit_code'],
                      capture_acam_monitor_samples=capture['acam_monitor_samples'])
        observe()
        print('ETAP 2/2: swiezy Parasolid -> Feature.dll -> ARD.', flush=True)
        native['model_expected_sha256'] = captured_sha
        try:
            run_native(root, private_model, feature_dir, native, observe, CURRENT_SHA256)
        finally:
            native['inputs_unchanged'] = digest(private_model) == captured_sha
            native['acam_observed'] = report['acam_observed']
            proof = collect_feature(feature_dir, native, expected_input=captured_sha)
            report['feature_stage'] = native
        value = validate_cls(native.get('cls_ready', {}), native.get('cls_result', {}),
                             native.get('cls_exit_code'), require_argument_bridge=True)
        if not proof:
            raise ValueError('Missing Feature child report')
        check_feature_stage(proof, record_only=True)
        stage = proof['stages'][0]
        validate_feature_input(stage, private_model, captured_sha)
        validate_projection(stage, value)
        report['schema_path_verification'] = validate_schema_path(stage)
        validate_kernel(native)
        report['body_measurement'] = validate_measurement(stage.get('result', {}).get('model_probe', {}), reference)
        record = (feature_dir / 'native/feature_object.bin').read_bytes()
        fragment = require_native_success(proof, record, captured_sha, record_only=True)
        if fragment['source_record_count'] != 1:
            raise ValueError('Expected one fresh Parasolid source in native object record')
        expected_path = str(private_model).encode('ascii') if str(private_model).isascii() else None
        # Source filenames use the vendor's current ANSI encoding. The child
        # digest, not decoding this metadata, proves input identity for all paths.
        if expected_path is not None and bytes.fromhex(fragment['source_records'][0]['source_path']['bytes_hex']) != expected_path + b'\0':
            raise ValueError('Native object refers to a different Parasolid source path')
        report['native_fragment'] = fragment
        report['roundtrip'] = inspect_output(feature_dir / 'native/roundtrip.x_b')
        output = stage['summary'].get('output') or {}
        if (stage['summary'].get('save_succeeded_with_file') is not True
                or report['roundtrip'].get('recognized_parasolid') is not True
                or any(report['roundtrip'].get(k) != output.get(k) for k in ('size_bytes', 'sha256'))):
            raise ValueError('Standalone saved Parasolid bytes do not match Feature output')
        geometry = record[fragment['geometry_offset']:fragment['geometry_offset'] + fragment['geometry_size_bytes']]
        extracted = feature_dir / 'native/ard_geometry.x_b'
        with extracted.open('xb') as stream:
            stream.write(geometry)
        report['embedded_geometry'] = inspect_output(extracted)
        if (report['embedded_geometry'].get('recognized_parasolid') is not True
                or report['embedded_geometry'].get('sha256') != fragment['geometry_sha256']):
            raise ValueError('Embedded native Parasolid not recognized')
        final_audit()
        data, wrapper = wrap_ard(base.read_bytes(), record)
        with target.open('xb') as stream:
            stream.write(data)
        own_ard = True
        report.update(ard_created=True, wrapper=wrapper,
                      ard={'filename': target.name, 'size_bytes': len(data), 'sha256': digest(target)})
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        print('Wynik diagnostyczny: ' + str(error), flush=True)
    finally:
        capture_report_path = out / 'step_capture/solidimport_step_capture_result.json'
        if capture_report_path.is_file():
            try:
                observed_capture = read_json(capture_report_path)
                report['step_repeated_in_this_test'] = observed_capture.get('step_conversion_attempted') is True
                report['capture_stage_status'] = {
                    'parasolid_capture_verified': observed_capture.get('parasolid_capture_verified'),
                    'worker_exit_code': observed_capture.get('worker_exit_code'),
                    'error': observed_capture.get('error'),
                    'acam_observed': observed_capture.get('acam_observed_during_worker')}
                if observed_capture.get('acam_observed_during_worker'):
                    report['acam_observed'] = True
            except (OSError, ValueError) as error:
                report['capture_report_read_error'] = str(error)
                report['ard_created'] = False
        try:
            final_audit()
        except Exception as error:
            report['final_check_error'] = str(error)
            report['inputs_and_installation_unchanged'] = False
            report['ard_created'] = False
        if report['acam_observed']:
            report['ard_created'] = False
        if not report['ard_created'] and own_ard:
            target.unlink()
        (out / 'step_ard_result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        archive = archive_result(out, report['ard_created'])
    if report['ard_created']:
        print('UTWORZONO: ' + str(target), flush=True)
        print('Teraz mozesz otworzyc ARD w ALPHACAM i porownac z wybranym STEP-em.', flush=True)
    print('PRZESLIJ ZIP: ' + str(archive), flush=True)
    return 0 if report['ard_created'] else 1


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('step', nargs='?', type=Path, help='STEP dragged onto the launcher; defaults to packaged original')
    p.add_argument('--root', type=Path, default=Path(r'C:\Program Files\Hexagon\ALPHACAM 2025'))
    p.add_argument('--solidlink-server', type=Path,
        default=Path(r'C:\Program Files\Common Files\Hexagon\SolidLink 2025.4\code\bin\SolidLink.exe'))
    p.add_argument('--out', type=Path)
    p.add_argument('--expected-size', nargs=3, type=float, metavar=('X', 'Y', 'Z'))
    p.add_argument('--catalog-dir', type=Path)
    p.add_argument('--language', default='pl-PL')
    a = p.parse_args(argv)
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        p.error('Wymagany Windows i Python x64')
    out = a.out or HERE / ('step_ard_test_15_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    print('TEST 15: STEP -> Parasolid -> ARD w jednym uruchomieniu.', flush=True)
    print('ALPHACAM musi pozostac zamkniety do konca testu.', flush=True)
    return convert(a.step or HERE / 'data/input_model.stp', a.root, a.solidlink_server, out,
                   a.expected_size, a.catalog_dir, a.language)


if __name__ == '__main__':
    raise SystemExit(main())
