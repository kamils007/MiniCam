"""Windows test: extract the supplied ARD's X_B and measure it through Feature.

Uses a private copy of the user's installed, working export runtime. It does
not translate STEP, save a body, write ARD, or modify the installed runtime.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import zipfile

SAMPLE_SHA256 = '13b01c7743bf2e97ca1b503362a476b08706ce562a49c17f3ce1a22c8974e93f'
FEATURE_SHA256 = '5bbfbdbd41c32dde36dfc81db3a7a022b9e3688622f7005130cc207bc6638438'
HERE = Path(__file__).resolve().parent
OVERLAYS = ('alphacam_reference/probe_feature_2025.py',
            'step_ard/feature_native_14.py', 'step_ard/build_step_ard_14.py')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def extract_sample(data, expected_sha256=SAMPLE_SHA256):
    # Diagnostic for this exact supplied sample, not a general ARD parser.
    # Pinning avoids treating marker scanning as a production format reader.
    if len(data) > 128 * 1024 * 1024 or digest(data) != expected_sha256:
        raise ValueError('Wybierz oryginalny testFAz.ard przeslany w tej rozmowie.')
    if not data.startswith(b'Licom-APS File V1.94\r\n\x1a\x30'):
        raise ValueError('Nieobslugiwany naglowek ARD.')
    start = 28 + struct.unpack_from('<I', data, 24)[0]
    if start > len(data) - 6 or data[-6] != 0x2f or data[-1] != 0:
        raise ValueError('Nieprawidlowa koncowka ARD.')
    marker = b'\x22\x14\x00LicomUKRJMParaSolid\x00'
    pos = data.find(marker, start)
    if pos < 0 or data.find(marker, pos + 1) >= 0:
        raise ValueError('Nie znaleziono jednoznacznego rekordu Feature.')
    pos += len(marker)
    count, length, reserved = struct.unpack_from('<III', data, pos)
    pos += 12
    if count != 1 or reserved != 0 or length > len(data) - 6 - pos:
        raise ValueError('Nieprawidlowa obwiednia bryly ARD.')
    record = data[pos:pos + length]
    cursor = 0

    def take(n):
        nonlocal cursor
        if n < 0 or n > len(record) - cursor:
            raise ValueError('Niepelny rekord Feature.')
        result = record[cursor:cursor + n]
        cursor += n
        return result

    def u32():
        return struct.unpack('<I', take(4))[0]

    if u32() != 3:
        raise ValueError('Nieobslugiwana wersja Feature.')
    sources = u32()
    if sources > 1024:
        raise ValueError('Za duzo opisow zrodel.')
    for _ in range(sources):
        u32()
        for _ in range(2):
            n = u32()
            if not 1 <= n <= 32768:
                raise ValueError('Nieprawidlowy opis zrodla.')
            text = take(n)
            if text[-1:] != b'\0':
                raise ValueError('Brak zakonczenia opisu.')
        take(9)
    payload = take(u32())
    bodies = u32()
    if bodies != 1 or not payload.startswith(b'**') or b'FORMAT=binary' not in payload[:4096]:
        raise ValueError('Nieprawidlowy Parasolid albo zakres inny niz jedna bryla.')
    return payload, {'source_ard_sha256': digest(data), 'record_sha256': digest(record),
                     'parasolid_sha256': digest(payload), 'parasolid_size_bytes': len(payload),
                     'declared_body_count': bodies}


def validate_read(native, expected_hash, validate_cls, validate_measurement, validate_schema_path):
    validate_cls(native.get('cls_ready', {}), native.get('cls_result', {}),
                 native.get('cls_exit_code'), require_argument_bridge=True)
    proof = native.get('feature', {})
    stages = proof.get('stages', [])
    if len(stages) != 1:
        raise ValueError('Brak jednoznacznego wyniku Feature.')
    stage = stages[0]
    summary = stage.get('summary', {})
    if (stage.get('process_exit_code') != 0 or summary.get('stop_reason') != 'completed_measurement_only'
            or summary.get('native_error') or proof.get('native_exceptions') or proof.get('trace_read_errors')
            or proof.get('original_input_unchanged') is not True
            or proof.get('no_acam_process_observed_at_checkpoints') is not True
            or summary.get('bodies_after_count') != 1
            or any(summary.get(k) is not True for k in ('host_selftest_ok', 'host_context_bound',
                'text_table_initialized', 'import_succeeded_with_bodies'))):
        raise ValueError('Odczyt bryly niepotwierdzony; sprawdz raport Feature.')
    result = stage.get('result', {})
    model = result.get('model_probe', {})
    if model.get('measurement_only_completed') is not True or 'save' in model or 'object_serialization' in model:
        raise ValueError('Niepotwierdzony zakres tylko do odczytu.')
    child = result.get('input_copy', {})
    if child.get('unchanged') is not True or any(child.get(k) != expected_hash for k in ('sha256_before', 'sha256_after')):
        raise ValueError('Feature nie potwierdzil bajtow wydzielonych z ARD.')
    validate_schema_path(stage)
    return validate_measurement(model, reference_dimensions_mm=None)


def run(ard, runtime, root, out):
    out.mkdir(parents=True, exist_ok=False)
    report = {'schema': 'minicam-ard-feature-read-v1', 'read_confirmed': False,
              'converted_to_occt': False, 'geometry_verified': False, 'volume_verified': False,
              'save_requested': False, 'installed_runtime_unchanged': False, 'error': None}
    original_runtime_hashes = {}
    original_ard_hash = None
    try:
        if sys.platform != 'win32' or struct.calcsize('P') != 8:
            raise RuntimeError('Wymagany Windows i Python x64.')
        if ard.stat().st_size > 128 * 1024 * 1024:
            raise ValueError('Plik ARD jest za duzy.')
        source = ard.read_bytes()
        original_ard_hash = digest(source)
        payload, report['extraction'] = extract_sample(source)
        model = out / 'extracted.x_b'
        model.write_bytes(payload)
        private = out / 'runtime'
        # Retain private catalogs locally, never include them in the report ZIP.
        for folder in ('alphacam_reference', 'step_ard', 'solidimport'):
            shutil.copytree(runtime / folder, private / folder, ignore=shutil.ignore_patterns('__pycache__'))
        for name in OVERLAYS:
            original_runtime_hashes[name] = digest((runtime / name).read_bytes())
            shutil.copyfile(HERE / 'overlay' / name, private / name)
        for folder in ('alphacam_reference', 'solidimport', 'step_ard'):
            sys.path.insert(0, str(private / folder))
        from build_step_ard_14 import (INSTALLATION, run_native, collect_feature, validate_cls,
                                      validate_measurement, validate_schema_path)
        from read_standalone_dependencies import process_names
        pins = dict(INSTALLATION)
        pins['Add-Ins/Feature/Feature.dll'] = FEATURE_SHA256
        for name, expected in pins.items():
            if digest((root / name).read_bytes()) != expected:
                raise ValueError('Inna niz obslugiwana wersja pliku: ' + name)
        native = out / 'feature'
        native.mkdir()
        state = {'measure_only': True, 'configure_schema_path': True, 'acam_observed': False,
                 'process_samples': 0, 'model_expected_sha256': digest(payload)}
        report['native'] = state

        def observe():
            state['process_samples'] += 1
            if process_names():
                state['acam_observed'] = True
                raise RuntimeError('Zamknij ALPHACAM na czas tego testu, tak jak przy zapisie ARD.')

        observe()
        try:
            run_native(root, model, native, state, observe, feature_sha256=FEATURE_SHA256)
        finally:
            state['inputs_unchanged'] = digest(model.read_bytes()) == digest(payload)
            collect_feature(native, state, expected_input=digest(payload))
        report['measurement'] = validate_read(state, digest(payload), validate_cls,
                                             validate_measurement, validate_schema_path)
        if any((native / 'native' / name).exists() for name in ('roundtrip.x_b', 'feature_object.bin')):
            raise ValueError('Nieoczekiwany zapis podczas testu odczytu.')
        report['read_confirmed'] = True
    except Exception as error:
        report['error'] = str(error)
    finally:
        try:
            report['installed_runtime_unchanged'] = bool(original_runtime_hashes) and all(
                digest((runtime / name).read_bytes()) == value for name, value in original_runtime_hashes.items())
            report['source_ard_unchanged'] = original_ard_hash is not None and digest(ard.read_bytes()) == original_ard_hash
            if report['read_confirmed'] and (not report['installed_runtime_unchanged'] or not report['source_ard_unchanged']):
                report['read_confirmed'] = False
                report['error'] = 'Zmienilo sie zrodlo lub runtime.'
        except Exception as error:
            report['read_confirmed'] = False
            report['error'] = str(error)
        (out / 'ard_read_result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        with zipfile.ZipFile(out / 'raport_odczytu.zip', 'x', zipfile.ZIP_DEFLATED) as archive:
            archive.write(out / 'ard_read_result.json', 'ard_read_result.json')
            for path in (out / 'feature').rglob('*'):
                if path.is_file() and path.suffix.lower() in ('.json', '.jsonl', '.txt'):
                    archive.write(path, path.relative_to(out).as_posix())
        print('Raport: ' + str(out / 'raport_odczytu.zip'), flush=True)
        print('Odczyt bryly potwierdzony.' if report['read_confirmed'] else 'Odczyt niepotwierdzony: ' + str(report['error']), flush=True)
    return 0 if report['read_confirmed'] else 1


if __name__ == '__main__':
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8', errors='backslashreplace')
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('ard', 'runtime', 'root', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    raise SystemExit(run(*(getattr(args, k).resolve() for k in ('ard', 'runtime', 'root', 'out'))))
