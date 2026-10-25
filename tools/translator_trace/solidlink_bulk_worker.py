"""Whole-document conversion through the original SLInterop entry point.

No ALPHACAM Automation, Feature traversal, authorization-string extraction or
vendor-code changes. An ordinary live CLS connection is required by the parent.
Windows execution of this experimental pinned adapter remains to be verified.
"""
import ctypes as C
from pathlib import Path
import sys
import time
import json

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bulk_conversion_probe as support

SCHEMA = 'minicam-solidlink-bulk-probe-v4'
SL_SHA256 = '30c67178c5224d24ce1ab13ff759e5d4795919314d8637d3b4c472224fc56442'
RUN = '?RunInteropStandard@SLInterop@@QEAA_NV?$CStringT@_WV?$StrTraitMFC_DLL@_WV?$ChTraitsCRT@_W@ATL@@@@@ATL@@00@Z'
NAMED = '?SetNamedParameters@SLInterop@@QEAAXPEAV?$vector@UNamedParameter@@V?$allocator@UNamedParameter@@@std@@@std@@@Z'


def validate_ready(state):
    if (state.get('schema') != 'feature-cls-session-14-v1' or state.get('ready') is not True
            or state.get('argument_bridge_selftest') is not True
            or state.get('open_return_code') != 1 or state.get('validation_before') != 101
            or state.get('validation_after') != 101):
        raise RuntimeError('Brak potwierdzonej biezacej sesji CLS.')


def validate_finished(state, exit_code):
    validate_ready(state)
    if (exit_code != 0 or state.get('close_completed') is not True
            or state.get('validation_after_feature') != 101
            or any(state.get(key) for key in ('error_type', 'release_error', 'close_error', 'lease_expired'))):
        raise RuntimeError('Niepotwierdzone prawidlowe zakonczenie sesji CLS.')
    if state.get('classification_flag') is True and state.get('release_return_code') not in (201, 202, 207):
        raise RuntimeError('Niepotwierdzone zwolnienie zasobu CLS.')


class InteropStorage(support.OpaqueWrapper):
    # Pinned ctor/copy/dtor use offsets through 0x5c, with 8-byte alignment.
    # No fields are read or written by this adapter; use original accessors.
    SIZE = 96


class CStringStorage(support.OpaqueWrapper):
    # Original MFC CStringW has one data pointer, as used by RunInteropStandard.
    SIZE = 8


class SolidLinkAPI(support.NativeAPI):
    def __init__(self, directory):
        if support.digest(directory / 'SLInterop.dll') != SL_SHA256:
            raise RuntimeError('Nieobslugiwana wersja SLInterop.dll.')
        super().__init__(directory)
        self.sl = C.CDLL(str(directory / 'SLInterop.dll'), winmode=0x1100)
        self.storage = InteropStorage()
        self.constructed = False
        symbols = {
            '_construct': ('??0SLInterop@@QEAA@XZ', C.c_void_p, [C.c_void_p], 0x24550),
            '_destroy': ('??1SLInterop@@UEAA@XZ', None, [C.c_void_p], 0x24620),
            '_named': (NAMED, None, [C.c_void_p, C.c_void_p], 0x2e20),
            '_run': (RUN, C.c_bool, [C.c_void_p] * 4, 0x23030),
            '_result': ('?GetInteropResult@SLInterop@@QEAAJXZ', C.c_int32, [C.c_void_p], 0x24300),
        }
        for alias, (symbol, result, args, rva) in symbols.items():
            function = getattr(self.sl, symbol)
            if C.cast(function, C.c_void_p).value - self.sl._handle != rva:
                raise RuntimeError('Nieoczekiwany adres funkcji SLInterop.')
            function.restype, function.argtypes = result, args
            setattr(self, alias, function)
        self.bind_mfc_strings()

    def bind_mfc_strings(self):
        kernel = C.WinDLL('kernel32.dll', use_last_error=True)
        kernel.GetModuleHandleExW.argtypes = [C.c_uint32, C.c_void_p, C.POINTER(C.c_void_p)]
        kernel.GetModuleHandleExW.restype = C.c_int32
        kernel.GetModuleFileNameW.argtypes = [C.c_void_p, C.c_wchar_p, C.c_uint32]
        kernel.GetModuleFileNameW.restype = C.c_uint32
        kernel.GetProcAddress.argtypes = [C.c_void_p, C.c_void_p]
        kernel.GetProcAddress.restype = C.c_void_p
        address = C.c_void_p.from_address(self.sl._handle + 0x2ceb8).value
        module = C.c_void_p()
        if not kernel.GetModuleHandleExW(6, address, C.byref(module)):
            raise C.WinError(C.get_last_error())
        name = C.create_unicode_buffer(32768)
        length = kernel.GetModuleFileNameW(module, name, len(name))
        if not length or length >= len(name) or Path(name.value).name.lower() != 'mfc140u.dll':
            raise RuntimeError('Importy CString nie wskazuja na mfc140u.dll.')
        bindings = {
            'string_construct': (0x2ceb8, 290, C.c_void_p, [C.c_void_p, C.c_char_p]),
            'string_length': (0x2cf30, 5674, C.c_int32, [C.c_void_p]),
            'string_destroy': (0x2cf80, 1033, None, [C.c_void_p]),
        }
        for alias, (slot, ordinal, result, args) in bindings.items():
            address = C.c_void_p.from_address(self.sl._handle + slot).value
            if not address or address != kernel.GetProcAddress(module, ordinal):
                raise RuntimeError('Niezgodny import MFC CString, ordinal ' + str(ordinal))
            setattr(self, alias, C.CFUNCTYPE(result, *args)(address))
        self.mfc_reference = {'path': name.value, 'sha256': support.digest(name.value),
                              'original_imports_verified': True}

    def prepare(self):
        returned = self._construct(self.storage.address)
        self.storage.check()
        if returned != self.storage.address:
            raise RuntimeError('Niepotwierdzony konstruktor SLInterop.')
        self.constructed = True
        # Public setter, empty optional named-parameter list. No license fields.
        self._named(self.storage.address, None)

    def run_standard(self, source, destination, log):
        strings = []
        transferred = False
        try:
            for path in (source, destination, log):
                text = str(path)
                encoded = text.encode('mbcs', errors='strict')
                if encoded.decode('mbcs') != text or b'\0' in encoded:
                    raise RuntimeError('Sciezka nie przechodzi bezstratnie przez Windows ACP.')
                value = CStringStorage()
                returned = self.string_construct(value.address, encoded)
                value.check()
                if returned != value.address:
                    raise RuntimeError('Niepotwierdzony konstruktor CString.')
                strings.append(value)
                length = self.string_length(value.address)
                pointer = C.c_void_p.from_address(value.address).value
                if not pointer or not 0 < length <= 32767 or C.wstring_at(pointer, length) != text:
                    raise RuntimeError('Oryginalny CString nie odpowiada sciezce pliku.')
            # MSVC x64 by-value CString arguments are indirect. The original
            # callee destroys all three (verified at 0x232e7..0x232fe, including
            # its early-return path); never destroy them twice.
            transferred = True
            okay = bool(self._run(self.storage.address, *(value.address for value in strings)))
            for value in strings:
                value.check()
            self.storage.check()
            return okay, int(self._result(self.storage.address))
        finally:
            if not transferred:
                for value in reversed(strings):
                    self.string_destroy(value.address)
                    value.check()

    def close(self):
        if self.constructed:
            self.constructed = False
            self._destroy(self.storage.address)
            self.storage.check()


def worker(directory, source, destination, result_path, cls_ready, api_factory=SolidLinkAPI):
    started = time.perf_counter()
    state = {'schema': SCHEMA, 'phase': 'starting', 'whole_body_conversion': True,
             'acam_used': False, 'geometry_verified': False, 'native_conversion_succeeded': False,
             'error': None, 'timings_seconds': {}}
    api = None
    log = result_path.parent / 'translator_native.log'

    def timed(phase, function, *args):
        state['phase'] = 'before_' + phase
        support.write_json(result_path, state)
        clock = time.perf_counter()
        result = function(*args)
        state['timings_seconds'][phase] = round(time.perf_counter() - clock, 6)
        state['phase'] = 'after_' + phase
        support.write_json(result_path, state)
        return result

    try:
        validate_ready(json.loads(cls_ready.read_text(encoding='utf-8-sig')))
        state['source_sha256'] = support.digest(source)
        if destination.exists():
            raise RuntimeError('Plik wyjsciowy juz istnieje.')
        api = timed('load_original_libraries', api_factory, directory)
        state['mfc_reference'] = api.mfc_reference
        timed('original_SLInterop_constructor', api.prepare)
        state['format_library_loading'] = timed('load_format_libraries', api.load_format_libraries)
        for name, info in state['format_library_loading']['modules'].items():
            if info.get('loaded') is not True:
                raise RuntimeError(name + ': ' + info.get('error', 'Brak zaladowanej biblioteki.'))
        state['conversion_entry'] = 'original_SLInterop_RunInteropStandard'
        okay, raw = timed('whole_document_conversion', api.run_standard, source, destination, log)
        success, partial = bool(api.success(raw)), bool(api.partial(raw))
        state['result'] = {'original_return': okay, 'raw': raw, 'success': success, 'partial': partial}
        if not okay or not success or partial:
            text = api.message(raw)
            detail = text.decode('utf-8', errors='replace')[:512] if text else str(raw)
            raise RuntimeError('SLInterop: ' + detail + '; return=' + str(okay))
        if not support.step_envelope(destination):
            raise RuntimeError('SLInterop nie utworzyl kompletnego STEP.')
        state['output_sha256'] = support.digest(destination)
        state['output_size_bytes'] = destination.stat().st_size
        state['native_conversion_succeeded'] = True
    except Exception as error:
        state['error'] = str(error)
    finally:
        if api:
            try:
                timed('original_SLInterop_destructor', api.close)
            except Exception as error:
                state['cleanup_error'] = str(error)
                state['native_conversion_succeeded'] = False
        try:
            state['translator_log_summary'] = support.summarize_log(log)
        except OSError as error:
            state['translator_log_summary'] = {'error': str(error)}
        state['elapsed_seconds'] = round(time.perf_counter() - started, 6)
        state['phase'] = 'finished'
        support.write_json(result_path, state)
    return 0 if state['native_conversion_succeeded'] else 1
