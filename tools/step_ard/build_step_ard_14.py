"""Test 14: the verified test-13 Parasolid -> measured Feature body -> ARD.

Windows x64, installed vendor modules and ordinary current CLS connection.
This test does not repeat STEP translation or certify full geometry validity.
"""
import argparse
from datetime import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import zipfile

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'solidimport'))
from read_standalone_dependencies import process_names
from test_step_conversion import powershell_path, inspect_output
from build_step_ard import BASE_SHA256, FEATURE_SHA256, wrap_ard, require_native_success, check_feature_stage
from probe_feature_2025 import parse_worker_output, summarize_stage

MODEL_SHA256 = 'bec7767b6fdd940965e79153975ea4db993510dd9abf3b74dabbf19801a0dca2'
STEP_SHA256 = '88541daf8ce0f68780ab615550c87a0417933083577ca2d63d6f40a1722735d7'
PO_SHA256 = '5ebfd4fedec46583fa72fd2e95862a0b3fbe7eb41b6a9e49ae12895176130776'
INSTALLATION = {
    'Acam.exe': '02997d0f6c15d0e6c807d1be19621e128bdbf3a55ae1846e0196872766bf205d',
    'Add-Ins/Feature/Feature.dll': FEATURE_SHA256,
    'CLS/LicManaged.dll': '0b30282755e72feefdc3b43e50c0628749e49a5bd97fa06dabc002a0851a4827',
    'CLS/CLS.dll': '5d1e001654690774794b2d6093491e6b0b51aa670b6694f6c98883c29b8f02c8',
    'CLS/ClsInit.exe': 'd594c321383c33ca4eb12eedf3f95cb2adfc269484cdea986fbe5973737ef252',
}
ALIGNMENT = ('AlignOnImport', 'AlignTopWithOrigin', 'AlignLongestEdgeWithXAxis',
             'UseLatheAlign', 'UsePanelAlignment', 'UseMinBoundingBoxAlignment')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def validate_cls(ready, final, exit_code, require_argument_bridge=False):
    for state in (ready, final):
        if require_argument_bridge and state.get('argument_bridge_selftest') is not True:
            raise ValueError('PowerShell argument bridge self-test not confirmed')
        if (state.get('schema') != 'feature-cls-session-14-v1'
                or state.get('ready') is not True or state.get('open_return_code') != 1
                or state.get('validation_before') != 101 or state.get('validation_after') != 101
                or state.get('access_code') != 348 or state.get('request_count') != 1
                or type(state.get('classification_flag')) is not bool
                or type(state.get('special_key_projection')) is not int
                or state['special_key_projection'] != (16 if state['classification_flag'] else 0)
                or state.get('supported_special_key_mask') != 16
                or state.get('full_special_key_verified') is not False):
            raise ValueError('Current CLS classification contract not confirmed')
    if (exit_code != 0 or final.get('validation_after_feature') != 101
            or final.get('close_completed') is not True
            or any(final.get(k) for k in ('error_type', 'release_error', 'close_error', 'lease_expired'))
            or any(ready[k] != final[k] for k in ('classification_flag', 'special_key_projection'))):
        raise ValueError('CLS session or normal cleanup not confirmed')
    if final['classification_flag']:
        code = final.get('release_return_code')
        if code not in (201, 202, 207) or final.get('individual_release_confirmed') is not (code in (201, 207)):
            raise ValueError('CLS release status not confirmed')
    return ready['special_key_projection']


def validate_measurement(model, reference_dimensions_mm=(1200.0, 2300.0, 10.0)):
    settings = model.get('alignment_configuration', [])
    if (len(settings) != len(ALIGNMENT) or {s.get('property') for s in settings} != set(ALIGNMENT)
            or any(s.get('enabled') is not False or s.get('readback_matches') is not True for s in settings)):
        raise ValueError('Automatic alignment was not verifiably disabled')
    measurement = model.get('body_measurement', {})
    bounds = measurement.get('bounds', {})
    dimensions = []
    for axis in 'XYZ':
        values = [bounds.get(side + axis) for side in ('Min', 'Max')]
        if any(type(v) not in (int, float) or not math.isfinite(v) for v in values) or values[1] <= values[0]:
            raise ValueError('Invalid measured bounds: ' + axis)
        dimensions.append(values[1] - values[0])
    if measurement.get('dimensions_api_units') != dimensions:
        raise ValueError('Dimensions disagree with measured bounds')
    if any(type(measurement.get('topology_counts', {}).get(k)) is not int
           or measurement['topology_counts'][k] < 1 for k in ('Faces', 'Edges', 'Vertices')):
        raise ValueError('Missing actual topology counts')
    if (measurement.get('alignment_disabled') is not True
            or measurement.get('full_topology_validity_verified') is not False
            or measurement.get('volume_verified') is not False):
        raise ValueError('Unexpected measurement scope')
    # This installation's millimetre profile; no inferred unit conversion or fitting.
    reference = None
    if reference_dimensions_mm is not None:
        if (len(reference_dimensions_mm) != 3
                or any(type(v) not in (int, float) or not math.isfinite(v) or v <= 0 for v in reference_dimensions_mm)):
            raise ValueError('Expected three finite positive reference dimensions')
        reference = sorted(reference_dimensions_mm)
        if any(abs(a - b) > 0.02 for a, b in zip(sorted(dimensions), reference)):
            raise ValueError('Measured bounds differ from reference dimensions in mm: ' + str(dimensions))
    return dict(measurement, dimensions_match_reference=reference is not None, reference_dimensions_mm=reference,
                tolerance_mm=0.02 if reference is not None else None,
                api_unit_profile='millimetres; no scale adjustment', geometry_verified=False)


def validate_projection(stage, value):
    events = stage.get('checkpoints', [])
    calls = [e for e in events if e.get('checkpoint') == 'host_method'
             and e.get('interface') == 'IAlphaCamApp' and e.get('method') == 'SpecialKey']
    if not calls or any(e.get('host_phase') != 'save' or e.get('value') != value for e in calls):
        raise ValueError('SpecialKey was consumed outside the verified save scope or had a different value')
    projections = [e for e in events if e.get('checkpoint') == 'live_save_projection']
    if (len(projections) != 1 or projections[0].get('mask') != 16
            or projections[0].get('value') != value
            or projections[0].get('source') != 'current_CLS_GetAccessCode_348'):
        raise ValueError('Missing live CLS save projection')


def validate_schema_path(stage):
    result = stage.get('result', {})
    configured = result.get('model_probe', {}).get('schema_path', {})
    cleanup = result.get('schema_path_cleanup', {})
    before, after = configured.get('files_before'), cleanup.get('files_after')
    if (configured.get('ok') is not True or configured.get('native_readback_matches') is not True
            or configured.get('original_schema_callbacks_retained') is not True
            or cleanup.get('ok') is not True or cleanup.get('previous_path_restored') is not True
            or cleanup.get('configured_value_unchanged') is not True
            or cleanup.get('files_unchanged') is not True
            or not isinstance(before, list) or not before or before != after
            or result.get('schema_path_cleanup_error')):
        raise ValueError('Native schema path/readback/restoration/catalog stability not confirmed')
    return {'configuration': configured, 'cleanup': cleanup}


def run_native(root, model, out, report, observe, feature_sha256=FEATURE_SHA256):
    """Keep the real CLS owner alive throughout the disposable native child."""
    native = out / 'native'
    native.mkdir()
    private_temp = out / 'private_temp'
    private_temp.mkdir()
    env = dict(os.environ, TEMP=str(private_temp), TMP=str(private_temp), PYTHONIOENCODING='utf-8')
    helper = child = None
    try:
        with (out / 'cls_stdout.txt').open('xb') as stdout, (out / 'cls_stderr.txt').open('xb') as stderr:
            helper = subprocess.Popen([str(powershell_path()), '-NoProfile', '-NonInteractive',
                '-ExecutionPolicy', 'Bypass', '-File', str(HERE / 'feature_cls_session_14.ps1'),
                '-Root', str(root), '-OutputDirectory', str(out)], cwd=out, env=env,
                stdout=stdout, stderr=stderr)
        deadline = time.monotonic() + 20
        while not (out / 'cls_ready.json').is_file():
            observe()
            if helper.poll() is not None:
                raise RuntimeError('CLS initialization failed; see cls_result.json and cls_stderr.txt')
            if time.monotonic() > deadline:
                raise TimeoutError('CLS did not become ready within 20 seconds')
            time.sleep(0.1)
        report['cls_ready'] = read_json(out / 'cls_ready.json')
        with (out / 'feature_stdout.txt').open('xb') as stdout, (out / 'feature_stderr.txt').open('xb') as stderr:
            child = subprocess.Popen([sys.executable, str(HERE / 'feature_native_14.py'),
                '--dll', str(root / 'Add-Ins/Feature/Feature.dll'), '--model', str(model),
                '--work-dir', str(native), '--cls-ready', str(out / 'cls_ready.json'),
                '--build-sha256', feature_sha256] + (['--direct-record'] if report.get('direct_record') else [])
                + (['--kernel-errors'] if report.get('kernel_error_diagnostics') else [])
                + (['--schema-path'] if report.get('configure_schema_path') else [])
                + (['--measure-only'] if report.get('measure_only') else [])
                + (['--model-sha256', report['model_expected_sha256']]
                   if report.get('model_expected_sha256') else []),
                cwd=out, env=env, stdout=stdout, stderr=stderr)
        deadline = time.monotonic() + 90
        while child.poll() is None:
            observe()
            if helper.poll() is not None:
                raise RuntimeError('CLS session ended before the Feature child')
            if time.monotonic() > deadline:
                report['native_timeout'] = True
                raise TimeoutError('Feature exceeded 90 seconds')
            time.sleep(0.1)
        observe()
    finally:
        if child is not None:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
            report['feature_exit_code'] = child.returncode
        if helper is not None:
            (out / 'feature_done.signal').touch(exist_ok=False)
            deadline = time.monotonic() + 10
            while helper.poll() is None and time.monotonic() < deadline:
                # Record Acam even while closing; do not interrupt ordinary CLS cleanup.
                try:
                    observe()
                except RuntimeError:
                    pass
                time.sleep(0.1)
            if helper.poll() is None:
                report['cls_cleanup_timeout'] = True
                helper.terminate()
                helper.wait(timeout=10)
            report['cls_exit_code'] = helper.returncode
            if (out / 'cls_result.json').is_file():
                report['cls_result'] = read_json(out / 'cls_result.json')


def collect_feature(out, report, expected_input=MODEL_SHA256):
    if not (out / 'feature_stdout.txt').is_file():
        return None
    stage = parse_worker_output((out / 'feature_stdout.txt').read_text(encoding='utf-8', errors='replace'),
                                (out / 'feature_stderr.txt').read_text(encoding='utf-8', errors='replace'))
    stage.update(stage='model', process_exit_code=report.get('feature_exit_code'), direct_record=bool(report.get('direct_record')))
    if report.get('native_timeout'):
        stage['timeout_seconds'] = 90
    stage['summary'] = summarize_stage(stage)
    exceptions = []
    trace = out / 'native/native_exceptions.jsonl'
    if trace.is_file():
        exceptions = [json.loads(line) for line in trace.read_text(encoding='utf-8').splitlines() if line.strip()]
    proof = {'input': {'sha256': expected_input}, 'original_input_unchanged': report.get('inputs_unchanged'),
             'no_acam_process_observed_at_checkpoints': not report.get('acam_observed'),
             'native_exceptions': exceptions, 'trace_read_errors': [], 'stages': [stage]}
    report['feature'] = proof
    model = stage.get('result', {}).get('model_probe', {})
    if report.get('kernel_error_diagnostics'):
        report['kernel_diagnostics'] = {
            'api': model.get('parasolid_error_api'),
            'before_save': model.get('kernel_error_before_save'),
            'after_save': model.get('kernel_error_after_save'),
            'before_record': model.get('object_serialization', {}).get('kernel_error_before_write'),
            'after_record': model.get('object_serialization', {}).get('kernel_error_after_write'),
            'pskernel_file_unchanged': stage.get('result', {}).get('pskernel_file_unchanged')}
    return proof


def main(feature_sha256=FEATURE_SHA256, test_id='14', direct_record=False, kernel_errors=False,
         schema_path=False):
    from feature_builds import profile
    build = profile(feature_sha256)
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(r'C:\Program Files\Hexagon\ALPHACAM 2025'))
    p.add_argument('--out', type=Path)
    args = p.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        p.error('Wymagany Windows i Python x64')
    out = (args.out or HERE / ('step_ard_test_' + test_id + '_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    report = {'test': test_id + '-test13-parasolid-to-measured-ard', 'ard_created': False,
              'direct_record': direct_record,
              'kernel_error_diagnostics': kernel_errors,
              'configure_schema_path': schema_path,
              'expected_feature_sha256': feature_sha256, 'expected_feature_version': build['version'],
              'geometry_verified': False, 'volume_verified': False, 'opened_in_alphacam': False,
              'step_repeated_in_this_test': False, 'acam_observed': False, 'process_samples': 0,
              'source_step_sha256': STEP_SHA256, 'captured_test13_sha256': MODEL_SHA256,
              'source_test13_archive_sha256': '2f912d1468e3a7913b8dbb6a1cd30ffe0839d76d23594e54b4fe7dbedbb9d2f9'}
    model, base = HERE / 'data/captured.x_t', HERE / 'data/1_test.ard'
    root = args.root.resolve()
    files = {root / name: expected for name, expected in INSTALLATION.items()}
    files[root / 'Add-Ins/Feature/Feature.dll'] = feature_sha256
    if schema_path:
        from parasolid_errors import KERNEL_SHA256
        files[root / 'Add-Ins/Feature/PSKERNEL.dll'] = KERNEL_SHA256
    files.update({model: MODEL_SHA256, base: BASE_SHA256, HERE.parent / 'alphacam_reference/Feature.po': PO_SHA256})

    def observe():
        found = process_names()
        report['process_samples'] += 1
        if found:
            report['acam_observed'] = True
            report['observed_acam'] = found
            raise RuntimeError('ALPHACAM musi pozostac zamkniety do konca testu')

    def verify_files():
        results = [{'path': str(path), 'expected_sha256': expected,
                    'sha256': digest(path) if path.is_file() else None} for path, expected in files.items()]
        return results, all(r['sha256'] == r['expected_sha256'] for r in results)

    try:
        print('TEST ' + test_id.replace('_', '.') + ': Parasolid z testu 13 -> pomiar Feature.dll ' + build['version'] + ' -> ARD', flush=True)
        print('ALPHACAM powinien pozostac zamkniety do konca testu.', flush=True)
        observe()
        report['files_before'], okay = verify_files()
        if not okay:
            raise ValueError('Inny lub brakujacy plik instalacji/wejscia; zobacz files_before w raporcie')
        private_model = out / 'input_copy.x_t'
        shutil.copyfile(model, private_model)
        if digest(private_model) != MODEL_SHA256:
            raise ValueError('Private Parasolid copy differs')
        run_native(root, private_model, out, report, observe, feature_sha256)
        report['files_after'], report['files_match_profile_after'] = verify_files()
        report['inputs_unchanged'] = (report['files_before'] == report['files_after']
                                      and digest(private_model) == MODEL_SHA256)
        if not report['files_match_profile_after']:
            raise ValueError('Installation or input no longer matches the reviewed profile')
        observe()
        proof = collect_feature(out, report)
        value = validate_cls(report.get('cls_ready', {}), report.get('cls_result', {}), report.get('cls_exit_code'),
                             require_argument_bridge=test_id in ('14_2','14_3','14_4','14_5'))
        check_feature_stage(proof or {}, record_only=direct_record)  # Cause before a possibly missing .bin.
        if kernel_errors and proof['stages'][0].get('result', {}).get('pskernel_file_unchanged') is not True:
            raise ValueError('Loaded PSKERNEL file stability was not confirmed')
        stage = proof['stages'][0]
        if schema_path:
            report['schema_path_verification'] = validate_schema_path(stage)
        validate_projection(stage, value)
        measurement = validate_measurement(stage.get('result', {}).get('model_probe', {}))
        report['body_measurement'] = measurement
        record = (out / 'native/feature_object.bin').read_bytes()
        report['native_fragment'] = require_native_success(proof, record, MODEL_SHA256, record_only=direct_record)
        report['roundtrip'] = inspect_output(out / 'native/roundtrip.x_b')
        if not direct_record and (not report['roundtrip'].get('recognized_parasolid')
                or report['roundtrip'].get('sha256') != stage['summary']['output']['sha256']):
            raise ValueError('Saved Parasolid bytes disagree with native report')
        if direct_record:
            parsed = report['native_fragment']
            geometry = record[parsed['geometry_offset']:parsed['geometry_offset'] + parsed['geometry_size_bytes']]
            extracted = out / 'native/ard_geometry.x_b'
            with extracted.open('xb') as stream:
                stream.write(geometry)
            report['embedded_geometry'] = inspect_output(extracted)
            if (not report['embedded_geometry'].get('recognized_parasolid')
                    or report['embedded_geometry'].get('sha256') != parsed['geometry_sha256']):
                raise ValueError('Native record geometry was not independently recognized')
        observe()
        data, wrapper = wrap_ard(base.read_bytes(), record)
        target = out / ('STEP_TEST_' + test_id + '.ard')
        with target.open('xb') as stream:
            stream.write(data)
        report.update(ard_created=True, wrapper=wrapper,
                      ard={'filename': target.name, 'size_bytes': len(data), 'sha256': digest(target)})
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        print('Wynik diagnostyczny: ' + str(error), flush=True)
    finally:
        try:
            report['files_after'], report['files_match_profile_after'] = verify_files()
            report['inputs_unchanged'] = report.get('files_before') == report['files_after']
            if (out / 'input_copy.x_t').is_file():
                report['inputs_unchanged'] &= digest(out / 'input_copy.x_t') == MODEL_SHA256
            observe()
        except Exception as error:
            report['final_check_error'] = str(error)
            report['inputs_unchanged'] = False
        try:
            collect_feature(out, report)
        except Exception as error:
            report['trace_read_error'] = str(error)
        if (report.get('acam_observed') or not report.get('inputs_unchanged') or not report.get('files_match_profile_after')
                or report.get('final_check_error') or report.get('trace_read_error')):
            report['ard_created'] = False
        with (out / 'step_ard_result.json').open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
        archive = out.with_suffix('.zip')
        # Only known diagnostics and outputs; no private TEMP, installation DLLs or license data.
        names = ['step_ard_result.json', 'cls_ready.json', 'cls_result.json', 'cls_stdout.txt', 'cls_stderr.txt',
                 'feature_stdout.txt', 'feature_stderr.txt', 'native/host_calls.jsonl',
                 'native/native_exceptions.jsonl', 'native/feature_object.bin', 'native/roundtrip.x_b', 'native/ard_geometry.x_b']
        if report['ard_created']:
            names.append('STEP_TEST_' + test_id + '.ard')
        with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as zipped:
            for name in names:
                path = out / name
                if path.is_file():
                    zipped.write(path, name)
    if report['ard_created']:
        print('UTWORZONO: ' + str(out / report['ard']['filename']))
        print('Teraz otworz ARD w ALPHACAM i naloz oryginalny STEP. Zapisz jako STEP_TEST_' + test_id + '_po_otwarciu.ard.')
    print('PRZESLIJ ZIP: ' + str(archive))
    return 0 if report['ard_created'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
