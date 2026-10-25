"""Isolated Windows experiment: whole ARD payload -> STEP via original SolidLink.

This is a diagnostic, not a MiniCam import backend. V4 uses SLInterop with an
ordinary live CLS session. Native execution and reverse conversion must be
verified on the user's station. Older low-level helpers are retained for tests.
"""
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import re
import subprocess
import sys
import time
import zipfile

HERE = Path(__file__).resolve().parent
SCHEMA = 'minicam-solidlink-bulk-probe-v4'
PINS = {
    'SPAIInterop.dll': 'bb3ec37e9a2c96017bbb3234c2764af10e5fad1bae679a2ba5485ac0336eec62',
    'SPAIInteropC.dll': '6d4fa624e2ee70c1979c512ae0cc1ca0c1c79b5d1b7ffb03eb9b67cb49184bad',
}
SYMBOLS = {
    'initialize': ('cpp', '?Initialize@SPAISystem@@SAX_N@Z', None, [C.c_bool]),
    'shutdown': ('cpp', '?Shutdown@SPAISystem@@SAXXZ', None, []),
    'document': ('cpp', '??0SPAIDocument@@QEAA@PEB_W@Z', C.c_void_p, [C.c_void_p, C.c_wchar_p]),
    'converter': ('cpp', '??0SPAIConverter@@QEAA@XZ', C.c_void_p, [C.c_void_p]),
    'file': ('cpp', '??0SPAIFile@@QEAA@PEB_W@Z', C.c_void_p, [C.c_void_p, C.c_wchar_p]),
    'document_destroy': ('cpp', '??1SPAIDocument@@UEAA@XZ', None, [C.c_void_p]),
    'converter_destroy': ('cpp', '??1SPAIConverter@@UEAA@XZ', None, [C.c_void_p]),
    'file_destroy': ('cpp', '??1SPAIFile@@UEAA@XZ', None, [C.c_void_p]),
    'get_data': ('cpp', '?GetData@SPAIObject@@QEBAPEAXXZ', C.c_void_p, [C.c_void_p]),
    'set_type': ('c', '?SPAICDocument_SetType@@YAJPEAXPEBD@Z', C.c_int32, [C.c_void_p, C.c_char_p]),
    'convert_cpp': ('cpp', '?Convert@SPAIConverter@@QEAA?AVSPAIResult@@AEBVSPAIDocument@@0@Z',
                    C.c_void_p, [C.c_void_p, C.POINTER(C.c_int32), C.c_void_p, C.c_void_p]),
    'start_log_cpp': ('cpp', '?StartLog@SPAIConverter@@QEAA?AVSPAIResult@@AEBVSPAIFile@@@Z',
                      C.c_void_p, [C.c_void_p, C.POINTER(C.c_int32), C.c_void_p]),
    'stop_log_cpp': ('cpp', '?StopLog@SPAIConverter@@QEAA?AVSPAIResult@@AEBVSPAIFile@@@Z',
                     C.c_void_p, [C.c_void_p, C.POINTER(C.c_int32), C.c_void_p]),
    'success': ('c', '?SPAICResult_IsSuccess@@YA_NJ@Z', C.c_bool, [C.c_int32]),
    'partial': ('c', '?SPAICResult_IsPartialSuccess@@YA_NJ@Z', C.c_bool, [C.c_int32]),
    'exception': ('c', '?SPAICDidCallHaveException@@YA_NXZ', C.c_bool, []),
    'message': ('c', '?SPAICResult_GetMessage@@YAPEBDJ@Z', C.c_char_p, [C.c_int32]),
}


def summarize_log(path):
    """Return fixed diagnostics only; raw logs/options remain on the station."""
    if not path.is_file():
        return {'exists': False}
    with path.open('rb') as stream:
        data = stream.read(1024 * 1024)
    encoding = 'utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig'
    text = data.decode(encoding, errors='replace').lower()
    words = ('error', 'failure', 'failed', 'unsupported', 'not supported', 'not found',
             'reader', 'writer', 'converter', 'license', 'licence', 'licensing',
             'parasolid', 'step', 'acis', 'no reader', 'no writer')
    # Counts are observations, never a diagnosis based on a word alone.
    return {'exists': True, 'size_bytes': path.stat().st_size,
            'scan_truncated': path.stat().st_size > len(data),
            'term_counts': {word: len(re.findall(r'\b' + re.escape(word) + r'\b', text)) for word in words},
            'format_library_warnings': [name + ' library not loaded' for name in
                ('SPAXParasolid', 'SPAXStep') if re.search(r'\b' + name.lower() + r' library not loaded\b', text)],
            'unauthorized_products': [name for name in ('GENERIC_R', 'STEP_W', 'STEP_GRAPH_R')
                if ('product "' + name.lower() + '" is not authorized') in text],
            'raw_log_in_report': False}


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
        # add_dll_directory alone does not cover legacy LoadLibrary calls made
        # inside a vendor DLL. Apply this policy only in the disposable worker.
        kernel = C.WinDLL('kernel32.dll', use_last_error=True)
        kernel.SetDefaultDllDirectories.argtypes = [C.c_uint32]
        kernel.SetDefaultDllDirectories.restype = C.c_int32
        if not kernel.SetDefaultDllDirectories(0x1000):
            raise C.WinError(C.get_last_error())
        self.search = os.add_dll_directory(str(directory))
        self.directory = directory
        self.format_libraries = []
        # Absolute paths, original installation, default Windows/system search.
        libraries = {'cpp': C.CDLL(str(directory / 'SPAIInterop.dll'), winmode=0x1100),
                     'c': C.CDLL(str(directory / 'SPAIInteropC.dll'), winmode=0x1100)}
        self.libraries = libraries
        for alias, (library, symbol, result, args) in SYMBOLS.items():
            function = getattr(libraries[library], symbol)
            function.argtypes = args
            function.restype = result
            setattr(self, alias, function)

    def load_format_libraries(self):
        report = {'search_policy': 'worker_default_directories_and_registered_vendor_bin', 'modules': {}}
        for name in ('SPAXParasolid.dll', 'SPAXStep.dll'):
            path = self.directory / name
            info = {'path': str(path), 'exists': path.is_file(), 'loaded': False}
            report['modules'][name] = info
            if not info['exists']:
                info['error'] = 'Brak pliku w wybranym folderze SolidLink.'
                continue
            try:
                info['sha256'] = digest(path)
                library = C.CDLL(str(path), winmode=0x1100)
                self.format_libraries.append(library)
                info['loaded'] = True
            except OSError as error:
                info['winerror'] = getattr(error, 'winerror', None)
                info['error'] = str(error)
        return report

    @staticmethod
    def cpp_result(function, owner, *arguments):
        # Verified MSVC x64 hidden result pointer: RCX=this, RDX=&SPAIResult,
        # R8/R9=documents. SPAIResult ctor RVA 0xa8f0 writes exactly one int32;
        # Convert returns that pointer (RVA 0x51fa / 0x5218).
        value = C.c_int32(0x1000002)
        returned = function(owner, C.byref(value), *arguments)
        if returned != C.addressof(value):
            raise RuntimeError('Niepotwierdzony wskaznik wyniku funkcji C++.')
        return value.value

    def convert_document(self, owner, source, destination):
        return self.cpp_result(self.convert_cpp, owner, source, destination)

    def start_log(self, owner, file):
        return self.cpp_result(self.start_log_cpp, owner, file)

    def stop_log(self, owner, file):
        return self.cpp_result(self.stop_log_cpp, owner, file)


def worker(directory, source, destination, result_path, api_factory=NativeAPI):
    started = time.perf_counter()
    state = {'schema': SCHEMA, 'native_conversion_succeeded': False, 'geometry_verified': False,
             'phase': 'starting', 'timings_seconds': {}, 'results': {}, 'error': None}
    api = None
    initialized = False
    owned = []
    logging = None
    log_path = result_path.parent / 'translator_native.log'

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
        return handle, storage.address

    try:
        state['source_sha256'] = digest(source)
        if destination.exists():
            raise RuntimeError('Plik wyjsciowy juz istnieje; test go nie nadpisze.')
        api = timed('load_libraries', api_factory, directory)
        # Same true flag as original SLInterop's SPAISystemInitGuard call.
        timed('initialize', api.initialize, True)
        initialized = True
        state['format_library_loading'] = timed('load_format_libraries', api.load_format_libraries)
        checkpoint('format_library_load_results')
        for name, info in state['format_library_loading']['modules'].items():
            if info.get('loaded') is not True:
                raise RuntimeError('Nie udalo sie zaladowac ' + name + ': ' + info.get('error', 'brak szczegolow'))
        source_handle, source_object = construct('document', source)
        target_handle, target_object = construct('document', destination)
        converter_handle, converter_object = construct('converter')
        result('source_type', timed('source_type', api.set_type, source_handle, b'PARASOLID'))
        result('target_type', timed('target_type', api.set_type, target_handle, b'STEP'))
        _, log_object = construct('file', log_path)
        result('start_log', timed('start_log', api.start_log, converter_object, log_object))
        logging = (converter_object, log_object)
        state['conversion_entry'] = 'original_SPAIConverter_Convert_CPP'
        result('convert', timed('convert', api.convert_document, converter_object, source_object, target_object))
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
        if logging:
            try:
                result('stop_log', timed('stop_log', api.stop_log, *logging))
            except Exception as error:
                cleanup_errors.append(str(error))
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
        try:
            state['translator_log_summary'] = summarize_log(log_path)
        except OSError as error:
            state['translator_log_summary'] = {'error': str(error)}
        if cleanup_errors:
            state['native_conversion_succeeded'] = False
        checkpoint('finished')
    return 0 if state['native_conversion_succeeded'] else 1


def run_with_cls(command, root, out, report, timeout):
    """Hold the existing ordinary CLS session until the native worker ends."""
    helper = child = None
    session_script = HERE / 'feature_cls_session_14.ps1'
    if not session_script.is_file():
        session_script = HERE.parent / 'step_ard' / 'feature_cls_session_14.ps1'
    powershell = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    try:
        with (out / 'cls_stdout.txt').open('wb') as stdout, (out / 'cls_stderr.txt').open('wb') as stderr:
            helper = subprocess.Popen([str(powershell), '-NoProfile', '-NonInteractive',
                '-ExecutionPolicy', 'Bypass', '-File', str(session_script),
                '-Root', str(root), '-OutputDirectory', str(out)], cwd=out, stdout=stdout, stderr=stderr)
        deadline = time.monotonic() + 20
        while not (out / 'cls_ready.json').is_file():
            if helper.poll() is not None:
                raise RuntimeError('Nie udalo sie otworzyc sesji CLS; zobacz raport CLS.')
            if time.monotonic() >= deadline:
                raise TimeoutError('Sesja CLS nie byla gotowa w ciagu 20 sekund.')
            time.sleep(0.1)
        from solidlink_bulk_worker import validate_ready
        report['cls_ready'] = json.loads((out / 'cls_ready.json').read_text(encoding='utf-8-sig'))
        validate_ready(report['cls_ready'])
        if helper.poll() is not None:
            raise RuntimeError('Sesja CLS zakonczyla sie przed konwersja.')
        with (out / 'worker_stdout.txt').open('wb') as stdout, (out / 'worker_stderr.txt').open('wb') as stderr:
            child = subprocess.Popen(command, cwd=out, stdout=stdout, stderr=stderr)
        deadline = time.monotonic() + timeout
        while child.poll() is None:
            if helper.poll() is not None:
                raise RuntimeError('Sesja CLS zakonczyla sie podczas konwersji.')
            if time.monotonic() >= deadline:
                report['timed_out'] = True
                raise TimeoutError('Konwersja przekroczyla 90 sekund.')
            time.sleep(0.1)
        report['worker_exit_code'] = child.returncode
    finally:
        if child is not None and child.poll() is None:
            child.terminate()
            child.wait(timeout=10)
            report['worker_exit_code'] = child.returncode
        if helper is not None:
            (out / 'feature_done.signal').touch(exist_ok=False)
            try:
                helper.wait(timeout=15)
            except subprocess.TimeoutExpired:
                report['cls_cleanup_timeout'] = True
                helper.terminate()
                helper.wait(timeout=10)
            report['cls_exit_code'] = helper.returncode
            path = out / 'cls_result.json'
            if path.is_file():
                report['cls_result'] = json.loads(path.read_text(encoding='utf-8-sig'))


def run(ard, directory, out, root, timeout=90):
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
                   '--result', str(result_path), '--cls-ready', str(out / 'cls_ready.json')]
        print('Proba konwersji calej bryly. Poczekaj na wynik (maksymalnie 90 sekund).', flush=True)
        run_with_cls(command, root, out, report, timeout)
        from solidlink_bulk_worker import validate_finished
        validate_finished(report.get('cls_result', {}), report.get('cls_exit_code'))
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
        # Include the last native checkpoint even after an early process/CLS failure.
        native_path = out / 'native_result.json'
        if 'native' not in report and native_path.is_file():
            try:
                report['native'] = json.loads(native_path.read_text(encoding='utf-8'))
            except (OSError, ValueError):
                pass
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
    sys.path.insert(0, str(HERE))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', action='store_true')
    for name in ('ard', 'bin', 'out', 'source', 'destination', 'result', 'root'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--cls-ready', type=Path)
    arguments = parser.parse_args()
    if arguments.worker:
        if any(getattr(arguments, key) is None for key in ('bin', 'source', 'destination', 'result', 'cls_ready')):
            parser.error('Worker requires bin/source/destination/result/cls-ready')
        from solidlink_bulk_worker import worker as solidlink_worker
        raise SystemExit(solidlink_worker(*(getattr(arguments, key).resolve() for key in
                                  ('bin', 'source', 'destination', 'result', 'cls_ready'))))
    if any(getattr(arguments, key) is None for key in ('ard', 'bin', 'out', 'root')):
        parser.error('Requires ard/bin/out/root')
    raise SystemExit(run(*(getattr(arguments, key).resolve() for key in ('ard', 'bin', 'out', 'root'))))
