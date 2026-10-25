"""Isolated Windows experiment: original Spatial objects, ARD payload -> STEP.

This is a diagnostic, not a MiniCam import backend. Native execution and the
availability of the reverse conversion must be verified on the user's station.
"""
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import zipfile

HERE = Path(__file__).resolve().parent
SCHEMA = 'minicam-spatial-bulk-probe-v1'
PINS = {
    'SPAIInterop.dll': 'bb3ec37e9a2c96017bbb3234c2764af10e5fad1bae679a2ba5485ac0336eec62',
    'SPAIInteropC.dll': '6d4fa624e2ee70c1979c512ae0cc1ca0c1c79b5d1b7ffb03eb9b67cb49184bad',
}
SYMBOLS = {
    'initialize': ('cpp', '?Initialize@SPAISystem@@SAX_N@Z', None, [C.c_bool]),
    'shutdown': ('cpp', '?Shutdown@SPAISystem@@SAXXZ', None, []),
    'document': ('cpp', '??0SPAIDocument@@QEAA@PEB_W@Z', C.c_void_p, [C.c_void_p, C.c_wchar_p]),
    'converter': ('cpp', '??0SPAIConverter@@QEAA@XZ', C.c_void_p, [C.c_void_p]),
    'document_destroy': ('cpp', '??1SPAIDocument@@UEAA@XZ', None, [C.c_void_p]),
    'converter_destroy': ('cpp', '??1SPAIConverter@@UEAA@XZ', None, [C.c_void_p]),
    'get_data': ('cpp', '?GetData@SPAIObject@@QEBAPEAXXZ', C.c_void_p, [C.c_void_p]),
    'set_type': ('c', '?SPAICDocument_SetType@@YAJPEAXPEBD@Z', C.c_int32, [C.c_void_p, C.c_char_p]),
    'convert': ('c', '?SPAICConverter_Convert@@YAJPEAX00@Z', C.c_int32, [C.c_void_p] * 3),
    'success': ('c', '?SPAICResult_IsSuccess@@YA_NJ@Z', C.c_bool, [C.c_int32]),
    'partial': ('c', '?SPAICResult_IsPartialSuccess@@YA_NJ@Z', C.c_bool, [C.c_int32]),
    'exception': ('c', '?SPAICDidCallHaveException@@YA_NXZ', C.c_bool, []),
    'message': ('c', '?SPAICResult_GetMessage@@YAPEBDJ@Z', C.c_char_p, [C.c_int32]),
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, data):
    # A native crash must leave the last completed checkpoint readable.
    temporary = Path(str(path) + '.tmp')
    temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


def step_envelope(path):
    """Check the file envelope only; never claim geometry validity from this."""
    path = Path(path)
    if not path.is_file() or not 128 <= path.stat().st_size <= 256 * 1024 * 1024:
        return False
    data = path.read_bytes().strip()
    return (data.startswith(b'ISO-10303-21;') and data.endswith(b'END-ISO-10303-21;')
            and b'HEADER;' in data and b'DATA;' in data and b'ENDSEC;' in data)


def extract_ard(path):
    reader = HERE / 'ard_payload_reader.py'
    if not reader.is_file():
        reader = HERE.parent / 'ard_import' / 'read_ard_body.py'
    spec = importlib.util.spec_from_file_location('minicam_bulk_payload_reader', reader)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.extract_sample(Path(path).read_bytes())


class OpaqueWrapper:
    """Placement storage for the pinned 24-byte vendor wrapper, with guards.

    Layout evidence: SPAIObject constructor RVA 0x9e30 writes three 8-byte
    slots; document/converter constructors add no fields. Native SLInterop
    places document objects at 24-byte intervals. No field is read here:
    exported GetData obtains the implementation handle and original destructors
    release it. All entry points require the exact verified DLL fingerprint.
    """
    SIZE = 24
    PAD = 64

    def __init__(self):
        self.storage = C.create_string_buffer(self.PAD + self.SIZE + self.PAD)
        C.memset(C.addressof(self.storage), 0xA5, C.sizeof(self.storage))
        self.address = C.addressof(self.storage) + self.PAD
        C.memset(self.address, 0, self.SIZE)

    def check(self):
        raw = self.storage.raw
        if raw[:self.PAD] != b'\xa5' * self.PAD or raw[-self.PAD:] != b'\xa5' * self.PAD:
            raise RuntimeError('Native wrapper storage guard changed; stop this profile.')


class NativeAPI:
    def __init__(self, directory):
        if sys.platform != 'win32' or struct.calcsize('P') != 8:
            raise RuntimeError('Ten test wymaga Python 3 x64 w Windows.')
        for name, expected in PINS.items():
            if digest(directory / name) != expected:
                raise RuntimeError('Nieobslugiwana wersja biblioteki: ' + name)
        self.search = os.add_dll_directory(str(directory))
        # Absolute paths, original installation, default Windows/system search.
        libraries = {'cpp': C.CDLL(str(directory / 'SPAIInterop.dll'), winmode=0x1100),
                     'c': C.CDLL(str(directory / 'SPAIInteropC.dll'), winmode=0x1100)}
        self.libraries = libraries
        for alias, (library, symbol, result, args) in SYMBOLS.items():
            function = getattr(libraries[library], symbol)
            function.argtypes = args
            function.restype = result
            setattr(self, alias, function)


def worker(directory, source, destination, result_path, api_factory=NativeAPI):
    started = time.perf_counter()
    state = {'schema': SCHEMA, 'native_conversion_succeeded': False, 'geometry_verified': False,
             'phase': 'starting', 'timings_seconds': {}, 'results': {}, 'error': None}
    api = None
    initialized = False
    owned = []

    def checkpoint(phase):
        state['phase'] = phase
        state['elapsed_seconds'] = round(time.perf_counter() - started, 6)
        write_json(result_path, state)

    def timed(phase, function, *args):
        checkpoint('before_' + phase)
        clock = time.perf_counter()
        answer = function(*args)
        state['timings_seconds'][phase] = round(time.perf_counter() - clock, 6)
        checkpoint('after_' + phase)
        return answer

    def result(label, value):
        # Read exception state before another ordinary vendor operation clears it.
        exception = bool(api.exception())
        okay = bool(api.success(value))
        partial = bool(api.partial(value))
        state['results'][label] = {'raw': value, 'success': okay, 'partial': partial,
                                   'exception': exception}
        checkpoint('result_' + label)
        if exception or partial or not okay:
            message = api.message(value)
            detail = message.decode('utf-8', errors='replace')[:512] if message else str(value)
            raise RuntimeError(label + ': ' + detail)

    def construct(kind, path=None):
        storage = OpaqueWrapper()
        function = getattr(api, kind)
        args = [storage.address] + ([str(path)] if path is not None else [])
        returned = timed(kind + '_' + str(len(owned)), function, *args)
        storage.check()
        if returned != storage.address:
            raise RuntimeError('Niepotwierdzony wynik konstruktora ' + kind)
        # Original constructors preserve their own product-context preparation.
        owned.append((kind, storage))
        handle = api.get_data(storage.address)
        if not handle:
            raise RuntimeError('Brak uchwytu obiektu ' + kind)
        return handle

    try:
        state['source_sha256'] = digest(source)
        if destination.exists():
            raise RuntimeError('Plik wyjsciowy juz istnieje; test go nie nadpisze.')
        api = timed('load_libraries', api_factory, directory)
        # Same true flag as original SLInterop's SPAISystemInitGuard call.
        timed('initialize', api.initialize, True)
        initialized = True
        source_handle = construct('document', source)
        target_handle = construct('document', destination)
        converter_handle = construct('converter')
        result('source_type', timed('source_type', api.set_type, source_handle, b'PARASOLID'))
        result('target_type', timed('target_type', api.set_type, target_handle, b'STEP'))
        result('convert', timed('convert', api.convert, converter_handle, source_handle, target_handle))
        state['step_envelope_verified'] = step_envelope(destination)
        if not state['step_envelope_verified']:
            raise RuntimeError('Konwerter nie utworzyl kompletnego pliku STEP.')
        state['output_sha256'] = digest(destination)
        state['output_size_bytes'] = destination.stat().st_size
        state['native_conversion_succeeded'] = True
    except Exception as error:
        state['error'] = str(error)
    finally:
        cleanup_errors = []
        while owned:
            kind, storage = owned.pop()
            try:
                timed('destroy_' + kind + '_' + str(len(owned)), getattr(api, kind + '_destroy'), storage.address)
                storage.check()
            except Exception as error:
                cleanup_errors.append(str(error))
        if initialized:
            try:
                timed('shutdown', api.shutdown)
            except Exception as error:
                cleanup_errors.append(str(error))
        state['cleanup_errors'] = cleanup_errors
        if cleanup_errors:
            state['native_conversion_succeeded'] = False
        checkpoint('finished')
    return 0 if state['native_conversion_succeeded'] else 1


def run(ard, directory, out, timeout=90):
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        raise RuntimeError('Uruchom ten test w Windows, przez TEST.cmd.')
    out.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    report = {'schema': SCHEMA, 'native_conversion_succeeded': False, 'geometry_verified': False,
              'error': None, 'timings_seconds': {}}
    source_hash = None
    try:
        source_hash = digest(ard)
        payload, descriptor = extract_ard(ard)
        report['source'] = descriptor
        source = out / 'source.x_b'
        source.write_bytes(payload)
        report['timings_seconds']['extract_payload'] = round(time.perf_counter() - started, 6)
        destination = out / 'converted.step'
        result_path = out / 'native_result.json'
        command = [sys.executable, '-I', '-u', str(Path(__file__).resolve()), '--worker',
                   '--bin', str(directory), '--source', str(source), '--destination', str(destination),
                   '--result', str(result_path)]
        print('Proba konwersji calej bryly. Poczekaj na wynik (maksymalnie 90 sekund).', flush=True)
        with (out / 'worker_stdout.txt').open('w', encoding='utf-8') as stdout, \
                (out / 'worker_stderr.txt').open('w', encoding='utf-8') as stderr:
            try:
                process = subprocess.run(command, cwd=out, stdout=stdout, stderr=stderr,
                                         timeout=timeout, check=False)
                report['worker_exit_code'] = process.returncode
            except subprocess.TimeoutExpired:
                report['timed_out'] = True
                report['error'] = 'Proces testowy przekroczyl 90 sekund i zostal zatrzymany.'
        native = json.loads(result_path.read_text(encoding='utf-8')) if result_path.is_file() else {}
        report['native'] = native
        report['native_conversion_succeeded'] = bool(
            report.get('worker_exit_code') == 0 and native.get('schema') == SCHEMA
            and native.get('phase') == 'finished' and native.get('native_conversion_succeeded') is True
            and native.get('source_sha256') == descriptor['parasolid_sha256']
            and step_envelope(destination) and digest(destination) == native.get('output_sha256'))
        if not report['native_conversion_succeeded'] and not report['error']:
            report['error'] = native.get('error') or ('Niepotwierdzona konwersja; ostatni etap: '
                                                     + native.get('phase', 'brak raportu procesu'))
        report['payload_unchanged'] = digest(source) == descriptor['parasolid_sha256']
    except Exception as error:
        report['error'] = str(error)
    finally:
        try:
            report['source_ard_unchanged'] = source_hash is not None and digest(ard) == source_hash
        except OSError:
            report['source_ard_unchanged'] = False
        if not report['source_ard_unchanged'] or report.get('payload_unchanged') is not True:
            report['native_conversion_succeeded'] = False
            if not report['error']:
                report['error'] = 'Niepotwierdzona niezmiennosc zrodla ARD lub Parasolid.'
        report['timings_seconds']['total'] = round(time.perf_counter() - started, 6)
        report['interpretation'] = ('Success requires a fresh complete STEP and successful native result; '
            'geometry must still be checked independently. Failure does not prove the installed '
            'product cannot export STEP: standalone prerequisites and licensed formats are unverified.')
        write_json(out / 'bulk_result.json', report)
        with zipfile.ZipFile(out / 'raport_konwersji.zip', 'x', zipfile.ZIP_DEFLATED) as archive:
            archive.write(out / 'bulk_result.json', 'bulk_result.json')
            if report['native_conversion_succeeded']:
                archive.write(out / 'converted.step', 'converted.step')
        print('Raport: ' + str(out / 'raport_konwersji.zip'), flush=True)
        print('Utworzono STEP; trzeba jeszcze sprawdzic geometrie.' if report['native_conversion_succeeded']
              else 'Konwersja niepotwierdzona: ' + str(report['error']), flush=True)
    return 0 if report['native_conversion_succeeded'] else 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', action='store_true')
    for name in ('ard', 'bin', 'out', 'source', 'destination', 'result'):
        parser.add_argument('--' + name, type=Path)
    arguments = parser.parse_args()
    if arguments.worker:
        if any(getattr(arguments, key) is None for key in ('bin', 'source', 'destination', 'result')):
            parser.error('Worker requires bin/source/destination/result')
        raise SystemExit(worker(*(getattr(arguments, key).resolve() for key in
                                  ('bin', 'source', 'destination', 'result'))))
    if any(getattr(arguments, key) is None for key in ('ard', 'bin', 'out')):
        parser.error('Requires ard/bin/out')
    raise SystemExit(run(*(getattr(arguments, key).resolve() for key in ('ard', 'bin', 'out'))))
