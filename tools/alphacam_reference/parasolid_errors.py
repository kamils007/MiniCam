"""Read-only PK_ERROR_ask_last diagnostics for the reviewed 2025.4 DLL.

No callback registration, error clearing, import redirection or kernel options.
The two output-pointer ABI and code offset 0x20 are traced in this exact DLL.
Inline function/code-token arrays are verified against the supplied PSKERNEL.
"""
import ctypes as C
import hashlib
from pathlib import Path
import struct

from feature_builds import CURRENT_SHA256

ERROR_IAT_RVA = 0x13F7B8
STORAGE_BYTES = 1024
KERNEL_SHA256 = 'c900fa3f430fe67c2adb15a50f9604d9812ed81a02c5006b66689dac28263073'


def error_fields(raw, was_error, query_code, read_text=None):
    if query_code != 0 or not was_error:
        return {}
    if len(raw) < 128:
        raise ValueError('Short error structure copy')
    # PK_ERROR_ask_last -> 0x8f7ae0 copies 0x74 bytes. Error construction
    # 0x8fb300/0x8fb3c3 uses 32-byte arrays at 0 and 0x24, not pointers.
    tokens, fields = [], {}
    for name, offset in (('function', 0), ('code_token', 0x24)):
        value = raw[offset:offset + 32]
        if b'\0' not in value:
            continue
        try:
            value = value.split(b'\0', 1)[0].decode('ascii')
        except UnicodeDecodeError:
            continue
        if value.startswith(('PK_', 'KI_')) and all(c.isalnum() or c == '_' for c in value):
            fields[name] = value
            tokens.append({'structure_offset': offset, 'text': value})
    return {'error_code_at_verified_offset_0x20': struct.unpack_from('<i', raw, 0x20)[0],
            **fields, 'pk_tokens': tokens, 'remaining_structure_fields_decoded': False}


class ParasolidErrors:
    def __init__(self, feature_module, feature_sha256):
        if feature_sha256 != CURRENT_SHA256 or C.sizeof(C.c_void_p) != 8:
            raise ValueError('PK error reader requires the reviewed 2025.4 x64 DLL')
        self.kernel = C.WinDLL('kernel32')
        k = self.kernel
        k.GetModuleHandleW.argtypes, k.GetModuleHandleW.restype = [C.c_wchar_p], C.c_void_p
        k.GetProcAddress.argtypes, k.GetProcAddress.restype = [C.c_void_p, C.c_char_p], C.c_void_p
        k.GetModuleFileNameW.argtypes = [C.c_void_p, C.c_wchar_p, C.c_uint32]
        k.GetModuleFileNameW.restype = C.c_uint32
        k.GetLastError.argtypes, k.GetLastError.restype = [], C.c_uint32
        k.SetLastError.argtypes, k.SetLastError.restype = [C.c_uint32], None
        module = k.GetModuleHandleW('PSKERNEL.dll')
        imported = C.c_void_p.from_address(int(feature_module) + ERROR_IAT_RVA).value
        if not module or not imported or imported != k.GetProcAddress(module, b'PK_ERROR_ask_last'):
            raise RuntimeError('PK_ERROR_ask_last IAT/export identity mismatch')
        path = C.create_unicode_buffer(32768)
        length = k.GetModuleFileNameW(module, path, len(path))
        if not 0 < length < len(path):
            raise RuntimeError('Cannot identify loaded PSKERNEL.dll')
        self.path = Path(path.value)
        self.digest = hashlib.sha256(self.path.read_bytes()).hexdigest()
        if self.digest != KERNEL_SHA256:
            raise ValueError('Unreviewed PSKERNEL.dll; error structure layout not verified')
        self.info = {'module_path': str(self.path), 'module_sha256': self.digest,
                     'iat_export_identity_matches': True, 'iat_rva': hex(ERROR_IAT_RVA),
                     'read_only_api': 'PK_ERROR_ask_last', 'error_clear_called': False}
        self.ask = C.WINFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p)(imported)

    def read(self):
        previous = self.kernel.GetLastError()
        try:
            # Use aligned, overallocated outputs surrounded by canaries. The
            # original code supplies a byte logical and a stack structure.
            storage = (C.c_uint64 * ((STORAGE_BYTES + 32) // 8))()
            flags = (C.c_ubyte * 40)()
            address = C.addressof(storage)
            C.memset(address, 0xA5, 16)
            C.memset(address + 16 + STORAGE_BYTES, 0x5A, 16)
            C.memset(C.addressof(flags), 0xA5, 16)
            C.memset(C.addressof(flags) + 24, 0x5A, 16)
            code = self.ask(C.byref(flags, 16), C.c_void_p(address + 16))
            guards = (C.string_at(address, 16) == b'\xA5' * 16
                      and C.string_at(address + 16 + STORAGE_BYTES, 16) == b'\x5A' * 16
                      and bytes(flags[:16]) == b'\xA5' * 16 and bytes(flags[24:]) == b'\x5A' * 16)
            if not guards or flags[16] not in (0, 1) or any(flags[17:24]):
                raise RuntimeError('PK error output layout/canary check failed')
            result = {'query_return_code': int(code), 'was_error': bool(flags[16]),
                      'output_guards_intact': True, 'error_clear_called': False}
            result.update(error_fields(C.string_at(address + 16, STORAGE_BYTES),
                                       result['was_error'], code))
            return result
        finally:
            self.kernel.SetLastError(previous)

    def unchanged(self):
        return hashlib.sha256(self.path.read_bytes()).hexdigest() == self.digest
