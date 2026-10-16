#!/usr/bin/env python3
"""Test 4: captured STEP-derived Parasolid -> Feature record -> diagnostic ARD.

Reuses the verified Feature 4.9 probe unchanged. Windows x64, stdlib only.
The original STEP conversion was performed by test 3.2 with ALPHACAM open.
This stage uses its captured Parasolid while ALPHACAM is closed.
"""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

HERE = Path(__file__).resolve().parent
REFERENCE = HERE.parent / 'alphacam_reference'
sys.path.insert(0, str(REFERENCE))
from object_serialization import parse_fragment

MODEL_SHA256 = 'ab2fdda5033b63c2097c243c970211b59cdcf3229e4392601a1ad644997017c1'
BASE_SHA256 = '34fa7f821416866f5705c3ee9f845caabd653118105dc49b116b7f4db2783caa'
FEATURE_SHA256 = '4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec'
OBJECT_NAME = b'LicomUKRJMParaSolid\0'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def wrap_ard(base, fragment):
    """Insert one native record without changing existing drawing records."""
    if hashlib.sha256(base).hexdigest() != BASE_SHA256:
        raise ValueError('Expected the supplied, previously tested 1_test.ard base')
    if base[-6] != 0x2F or base[-1] != 0:
        raise ValueError('Unexpected ARD footer')
    pointer = struct.unpack_from('<I', base, len(base) - 5)[0]
    if not 1 <= pointer < len(base) - 6 or base[pointer - 1] != 0x2E:
        raise ValueError('Unexpected ARD identifier back-pointer')
    parsed = parse_fragment(fragment)
    if parsed['trailing_body_metadata_count'] != 1:
        raise ValueError('This diagnostic expects exactly one body metadata entry')
    if len(fragment) >= 2 ** 32:
        raise ValueError('Native record is too large for the ARD wrapper')
    envelope = b'\x22' + struct.pack('<H', len(OBJECT_NAME)) + OBJECT_NAME
    envelope += struct.pack('<III', 1, len(fragment), 0)
    assert len(envelope) == 35
    result = base[:-6] + envelope + fragment + base[-6:]
    offset = len(base) - 6 + len(envelope)
    assert result[offset:offset + len(fragment)] == fragment
    assert result[:len(base) - 6] == base[:-6] and result[-6:] == base[-6:]
    return result, {'fragment_offset': offset, 'fragment_bytes': len(fragment),
                    'base_records_unchanged': True, 'footer_unchanged': True,
                    'identifier_offset_unchanged': True, 'fragment': parsed}


def require_native_success(report, record, expected_input):
    if report.get('input', {}).get('sha256') != expected_input:
        raise ValueError('Feature report refers to a different Parasolid input')
    if report.get('original_input_unchanged') is not True:
        raise ValueError('Parasolid input was not preserved')
    if report.get('no_acam_process_observed_at_checkpoints') is not True:
        raise ValueError('ALPHACAM process was observed during the Feature test')
    stages = report.get('stages', [])
    if len(stages) != 1 or stages[0].get('process_exit_code') != 0:
        raise ValueError('Feature child process did not finish normally')
    summary = stages[0].get('summary', {})
    if (summary.get('stop_reason') != 'completed' or summary.get('native_error')
            or summary.get('bodies_after_count') != 1 or summary.get('saved_body_count') != 1
            or any(summary.get(key) is not True for key in
                   ['import_succeeded_with_bodies', 'save_succeeded_with_file',
                    'object_record_written_and_prefix_recognized'])):
        raise ValueError('Feature import, one-body save or native record was not confirmed')
    if report.get('native_exceptions') or report.get('trace_read_errors'):
        raise ValueError('Native exception diagnostics are not clean')
    obj = summary.get('object_serialization', {})
    if (obj.get('object_name') != OBJECT_NAME[:-1].decode('ascii')
            or obj.get('query_return_code') != 1
            or any(type(obj.get(key)) is not int or obj[key] != 0 for key in
                   ['write_return_code', 'flush_return_code', 'close_return_code'])
            or obj.get('output', {}).get('sha256') != hashlib.sha256(record).hexdigest()
            or obj.get('output', {}).get('size_bytes') != len(record)):
        raise ValueError('Native record bytes or writer status disagree with the report')
    return parse_fragment(record)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        parser.error('Wymagany Windows i Python x64')
    out = (args.out or HERE / ('step_ard_test_4_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    report = {'test': '4-step-derived-parasolid-to-ard', 'ard_created': False,
              'opened_in_alphacam': False, 'geometry_dimensions_validated': False,
              'step_conversion_stage': 'Test 3.2, captured with ALPHACAM 2025 open',
              'base_ard_source': 'Previously tested user 1_test.ard; existing records retained'}
    model, base = HERE / 'data' / 'converted.x_t', HERE / 'data' / '1_test.ard'
    environment = HERE / 'data' / 'host_environment_2025.json'
    dll = None
    try:
        from probe_feature_2025 import acam_processes
        from host_environment import load_environment
        if acam_processes():
            raise RuntimeError('Zamknij ALPHACAM na czas testu 4 i uruchom ponownie')
        settings = load_environment(environment, FEATURE_SHA256)
        dll = (args.dll or Path(settings['feature_dll_path'])).resolve()
        if digest(dll) != FEATURE_SHA256 or digest(model) != MODEL_SHA256 or digest(base) != BASE_SHA256:
            raise ValueError('Feature.dll, captured Parasolid or base ARD differs from the expected file')
        native_report = out / 'feature_probe_2025_host.json'
        command = [sys.executable, str(REFERENCE / 'probe_feature_2025.py'), str(dll),
                   '--model', str(model.resolve()), '--host-environment', str(environment.resolve()),
                   '--timeout', '90', '--out', str(native_report)]
        print('TEST 4: Parasolid z Twojego STEP-a -> Feature.dll -> ARD', flush=True)
        print('ALPHACAM powinien pozostac zamkniety do konca testu.', flush=True)
        with (out / 'probe_stdout.txt').open('xb') as stdout, (out / 'probe_stderr.txt').open('xb') as stderr:
            process = subprocess.run(command, cwd=out, stdout=stdout, stderr=stderr,
                                     timeout=150, check=False)
        if process.returncode != 0 or not native_report.is_file():
            raise RuntimeError('Feature probe failed; see probe_stdout.txt / probe_stderr.txt')
        proof = json.loads(native_report.read_text(encoding='utf-8-sig'))
        directories = list(out.glob('feature_test_4_9_2025_*'))
        if len(directories) != 1:
            raise ValueError('Expected one isolated Feature output directory')
        record = (directories[0] / 'feature_object.bin').read_bytes()
        parsed = require_native_success(proof, record, MODEL_SHA256)
        report['native_fragment'] = parsed
        for key, path, expected in [('model_unchanged', model, MODEL_SHA256),
                                    ('base_unchanged', base, BASE_SHA256),
                                    ('feature_dll_unchanged', dll, FEATURE_SHA256)]:
            report[key] = digest(path) == expected
            if not report[key]:
                raise ValueError('Input verification failed: ' + key)
        if acam_processes():
            raise ValueError('ALPHACAM was reopened before the test completed')
        data, wrapped = wrap_ard(base.read_bytes(), record)
        target = out / 'STEP_test.ard'
        with target.open('xb') as stream:
            stream.write(data)
        report.update(ard_created=True, wrapper=wrapped,
                      ard={'filename': target.name, 'size_bytes': len(data),
                           'sha256': hashlib.sha256(data).hexdigest()})
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        print('Wynik diagnostyczny: ' + str(error), flush=True)
    finally:
        for key, path, expected in [('model_unchanged', model, MODEL_SHA256),
                                    ('base_unchanged', base, BASE_SHA256),
                                    ('feature_dll_unchanged', dll, FEATURE_SHA256)]:
            try:
                report[key] = bool(path and digest(path) == expected)
            except OSError:
                report[key] = False
        with (out / 'step_ard_result.json').open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
        archive = shutil.make_archive(str(out), 'zip', root_dir=out)
    if report['ard_created']:
        print('UTWORZONO: ' + str(out / 'STEP_test.ard'))
        print('Teraz otworz STEP_test.ard w ALPHACAM, sprawdz bryle i zapisz jako STEP_test_po_otwarciu.ard.')
    print('PRZESLIJ ZIP: ' + archive)
    return 0 if report['ard_created'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
