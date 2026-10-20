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
            'alphacam_reference/com_typeinfo.py', 'alphacam_reference/feature_dispatch.py',
            'alphacam_reference/feature_geometry.py',
            'step_ard/feature_native_14.py', 'step_ard/build_step_ard_14.py')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def feature_record(data):
    """Walk recognized V1.94 records; never search arbitrary payloads for tags."""
    if len(data) < 34 or len(data) > 128 * 1024 * 1024:
        raise ValueError('Nieprawidlowa wielkosc ARD (limit 128 MiB).')
    if not data.startswith(b'Licom-APS File V1.94\r\n\x1a\x30'):
        raise ValueError('Nieobslugiwany naglowek ARD.')
    position = 24

    def take(n):
        nonlocal position
        if n < 0 or n > len(data) - position:
            raise ValueError('Niepelny ARD przy bajcie ' + str(position))
        result = data[position:position + n]
        position += n
        return result

    def u32(): return struct.unpack('<I', take(4))[0]
    def u8(): return take(1)[0]
    def flag():
        value = u32()
        if value not in (0, 1): raise ValueError('Nieznany znacznik listy ARD.')
        return value
    def string16():
        n = struct.unpack('<H', take(2))[0]
        if n < 1: raise ValueError('Nieprawidlowa dlugosc nazwy ARD.')
        text = take(n)
        if text[-1:] != b'\0' or b'\0' in text[:-1]: raise ValueError('Nieprawidlowa nazwa ARD.')
        return text[:-1]
    def attributes():
        seen = set()
        while True:
            key = u32()
            if not key: return
            if key in seen or len(seen) >= 4096: raise ValueError('Nieprawidlowa lista atrybutow ARD.')
            seen.add(key)
            kind = u32()
            if kind not in (3, 5): raise ValueError('Nieobslugiwany typ atrybutu ARD: ' + str(kind))
            take(4 if kind == 3 else 8)

    take(u32())  # bounded thumbnail, even if it contains fake record markers
    records = []
    identifier = None
    ended = False
    seen = set()
    while position < len(data):
        tag = u8()
        if tag not in (0x10, 0x22):
            if tag in seen: raise ValueError('Powtorzony rekord ARD.')
            seen.add(tag)
        if tag == 0x1d:
            for count in range(4097):
                n = u32()
                if not n: break
                if n > 4096 or count == 4096: raise ValueError('Za duzo nazw atrybutow ARD.')
                take(n * 2 + 4)
        elif tag == 0x01:
            take(44); flag(); attributes()
            for count in range(100000):
                if u32() not in (0x31, 0x431): raise ValueError('Nieznany rekord pomocniczy ARD.')
                take(76); more = flag(); take(8)
                if not more: break
            else: raise ValueError('Za duzo rekordow pomocniczych ARD.')
        elif tag == 0x02:
            for count in range(100000):
                take(52); more = flag(); take(181); attributes(); take(48)
                for _ in range(1000000):
                    kind = u8()
                    if not kind: break
                    if kind not in (1, 2): raise ValueError('Nieobslugiwany element geometrii ARD: ' + str(kind))
                    take(44 if kind == 1 else 108); attributes()
                else: raise ValueError('Za duzo elementow ARD.')
                if not more: break
            else: raise ValueError('Za duzo geometrii ARD.')
        elif tag == 0x2e:
            identifier = position
            string16()
        elif tag == 0x10:
            take(8); string16(); take(20); attributes(); take(92)
        elif tag == 0x15: take(68)
        elif tag == 0x21: take(8)
        elif tag == 0x22:
            name = string16()
            count = u32()
            if count > 10000: raise ValueError('Za duzo obiektow dodatku ARD.')
            for _ in range(count):
                length, reserved = u32(), u32()
                record = take(length)
                if name == b'LicomUKRJMParaSolid':
                    if reserved or count != 1: raise ValueError('Nieobslugiwana obwiednia Feature w ARD.')
                    records.append(record)
        elif tag == 0x3e: take(u32())
        elif tag == 0x2f:
            if identifier is None or u32() != identifier or u8() != 0 or position != len(data):
                raise ValueError('Nieprawidlowa koncowka ARD.')
            ended = True
        else: raise ValueError('Nieobslugiwany rekord ARD: ' + hex(tag))
    if not ended or len(records) != 1:
        raise ValueError('Brak jednoznacznego rekordu bryly Feature w ARD.')
    return records[0]


def extract_sample(data, expected_sha256=None):
    # Accept changed/new documents only after structural validation. Native
    # module fingerprints and payload correlation remain mandatory below.
    if expected_sha256 is not None and digest(data) != expected_sha256:
        raise ValueError('ARD nie odpowiada oczekiwanej wersji pliku.')
    record = feature_record(data)
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


def run(ard, runtime, root, out, geometry_snapshot=False):
    out.mkdir(parents=True, exist_ok=False)
    report = {'schema': 'minicam-ard-feature-read-v1', 'read_confirmed': False,
              'converted_to_occt': False, 'geometry_verified': False, 'volume_verified': False,
              'save_requested': False, 'installed_runtime_unchanged': False, 'error': None}
    report['geometry_snapshot_requested'] = geometry_snapshot
    report['geometry_snapshot_available'] = False
    original_runtime_hashes = {}
    original_ard_hash = None
    try:
        if sys.platform != 'win32' or struct.calcsize('P') != 8:
            raise RuntimeError('Wymagany Windows i Python x64.')
        if ard.stat().st_size > 128 * 1024 * 1024:
            raise ValueError('Plik ARD jest za duzy.')
        source = ard.read_bytes()
        original_ard_hash = digest(source)
        report['input'] = {'filename': ard.name, 'size_bytes': len(source), 'sha256': original_ard_hash}
        payload, report['extraction'] = extract_sample(source)
        model = out / 'extracted.x_b'
        model.write_bytes(payload)
        private = out / 'runtime'
        # Retain private catalogs locally, never include them in the report ZIP.
        for folder in ('alphacam_reference', 'step_ard', 'solidimport'):
            shutil.copytree(runtime / folder, private / folder, ignore=shutil.ignore_patterns('__pycache__'))
        for name in OVERLAYS:
            source_file = runtime / name
            if not source_file.is_file() and name not in (
                    'alphacam_reference/feature_dispatch.py', 'alphacam_reference/feature_geometry.py'):
                raise ValueError('Brak wymaganego pliku runtime eksportu: ' + name)
            original_runtime_hashes[name] = digest(source_file.read_bytes()) if source_file.is_file() else None
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
        state = {'measure_only': True, 'geometry_snapshot': geometry_snapshot,
                 'configure_schema_path': True, 'acam_observed': False,
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
        if geometry_snapshot:
            from feature_geometry import validate_geometry
            descriptor = report['measurement'].get('geometry_snapshot', {})
            geometry_file = native / 'native' / 'feature_geometry.json'
            data = geometry_file.read_bytes()
            geometry = json.loads(data)
            validate_geometry(geometry)
            counts = {name: len(geometry[key]) for name, key in
                      [('Faces', 'faces'), ('Edges', 'edges'), ('Vertices', 'vertices')]}
            if (descriptor.get('filename') != 'feature_geometry.json' or descriptor.get('sha256') != digest(data)
                    or counts != report['measurement']['topology_counts'] or descriptor.get('counts') != counts
                    or descriptor.get('topology_read_complete') is not True
                    or geometry.get('topology_read_complete') is not True):
                raise ValueError('Dane geometrii nie odpowiadaja odczytanej bryle.')
            report['geometry_snapshot_available'] = True
            report['geometry_snapshot'] = dict(descriptor, size_bytes=len(data))
        if any((native / 'native' / name).exists() for name in ('roundtrip.x_b', 'feature_object.bin')):
            raise ValueError('Nieoczekiwany zapis podczas testu odczytu.')
        report['read_confirmed'] = True
    except Exception as error:
        report['error'] = str(error)
    finally:
        try:
            report['installed_runtime_unchanged'] = bool(original_runtime_hashes) and all(
                (digest((runtime / name).read_bytes()) == value if value is not None else not (runtime / name).exists())
                for name, value in original_runtime_hashes.items())
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
    parser.add_argument('--geometry-snapshot', action='store_true')
    args = parser.parse_args()
    raise SystemExit(run(*(getattr(args, k).resolve() for k in ('ard', 'runtime', 'root', 'out')),
                         geometry_snapshot=args.geometry_snapshot))
