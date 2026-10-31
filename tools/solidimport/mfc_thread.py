"""Public MFC worker-thread startup for a non-MFC Python executable.

Use the public v14 AMD64 ABI, verified against Microsoft's matching public PDB.
The installed runtime exports these functions by ordinal (NONAME).
Never fabricate a CWinThread, alter TLS, or access CWinThread data members.
COM interfaces must be obtained inside the supplied callback's thread.
"""
import ctypes as C
import hashlib
from pathlib import Path
import struct
import threading


GET_THREAD_ORDINAL = 2222
BEGIN_WORKER_ORDINAL = 2149
GET_THREAD_NAME = '?AfxGetThread@@YAPEAVCWinThread@@XZ'
BEGIN_WORKER_NAME = '?AfxBeginThread@@YAPEAVCWinThread@@P6AIPEAX@Z0HIKPEAU_SECURITY_ATTRIBUTES@@@Z'


def read_pe_exports(data):
    """Read bounded PE32+ exports, including NONAME entries."""
    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError('Truncated PE data')
        return struct.unpack_from(fmt, data, offset)[0]

    u16 = lambda offset: unpack('<H', offset)
    u32 = lambda offset: unpack('<I', offset)
    if data[:2] != b'MZ':
        raise ValueError('Missing DOS signature')
    pe = u32(0x3C)
    if data[pe:pe + 4] != b'PE\0\0' or u16(pe + 4) != 0x8664:
        raise ValueError('Expected AMD64 PE')
    optional = pe + 24
    if u16(optional) != 0x20B or u16(pe + 20) < 120:
        raise ValueError('Expected PE32+ optional header')
    count = u16(pe + 6)
    if not 1 <= count <= 96:
        raise ValueError('Unexpected PE section count')
    sections = []
    for i in range(count):
        pos = optional + u16(pe + 20) + 40 * i
        sections.append((u32(pos + 12), u32(pos + 16), u32(pos + 20), u32(pos + 36)))

    def offset(rva, size=1):
        for start, raw_size, raw, flags in sections:
            if start <= rva and rva + size <= start + raw_size:
                result = raw + rva - start
                if result + size <= len(data):
                    return result
        raise ValueError('Export RVA outside file-backed sections')

    export_rva = u32(optional + 112)
    export_size = u32(optional + 116)
    if not export_rva or export_size < 40:
        raise ValueError('Missing export directory')
    directory = offset(export_rva, 40)
    base, functions, names = u32(directory + 16), u32(directory + 20), u32(directory + 24)
    if not 0 <= names <= functions <= 100000:
        raise ValueError('Unexpected export count')
    function_table = offset(u32(directory + 28), functions * 4)
    entries = {base + i: u32(function_table + i * 4) for i in range(functions)
               if u32(function_table + i * 4)}
    executable = {ordinal for ordinal, rva in entries.items()
                  if not export_rva <= rva < export_rva + export_size
                  and any(start <= rva < start + raw_size and flags & 0x20000000
                          for start, raw_size, raw, flags in sections)}
    table = offset(u32(directory + 32), names * 4) if names else 0
    ordinals = offset(u32(directory + 36), names * 2) if names else 0
    result = {}
    for i in range(names):
        pos = offset(u32(table + i * 4))
        # Resolve every byte via RVA so an invalid name cannot cross sections.
        end = data.find(b'\0', pos, min(pos + 1024, len(data)))
        if end < 0:
            raise ValueError('Unterminated export name')
        offset(u32(table + i * 4), end - pos + 1)
        name = data[pos:end].decode('ascii')
        index = u16(ordinals + i * 2)
        if not name or index >= functions or name in result:
            raise ValueError('Invalid or duplicate named export')
        result[name] = base + index
    return {'names': result, 'ordinals': entries, 'executable': executable}


def worker_api_names(exports):
    names = exports['names']
    required = {GET_THREAD_NAME: GET_THREAD_ORDINAL, BEGIN_WORKER_NAME: BEGIN_WORKER_ORDINAL}
    for name, ordinal in required.items():
        if ordinal not in exports['executable']:
            raise RuntimeError('Required MFC export is missing, forwarded or not executable: ' + name)
        if name in names and names[name] != ordinal:
            raise RuntimeError('Named MFC export disagrees with public ordinal ABI: ' + name)
    return GET_THREAD_ORDINAL, BEGIN_WORKER_ORDINAL


class MFCWorker:
    """Own callback pins; run only a process-terminating diagnostic callback."""
    def __init__(self, solidimport_module, event):
        self.event = event
        self.pins = []
        ptr, u32 = C.c_void_p, C.c_uint32
        self.k = C.WinDLL('kernel32', use_last_error=True)
        for name, result, args in [
            ('GetModuleHandleExW', C.c_int32, [u32, ptr, C.POINTER(ptr)]),
            ('GetModuleFileNameW', u32, [ptr, C.c_wchar_p, u32]),
            ('GetProcAddress', ptr, [ptr, ptr]),
            ('GetCurrentThreadId', u32, []),
        ]:
            fn = getattr(self.k, name)
            fn.restype, fn.argtypes = result, args
        # This IAT location is guarded by the SolidImport SHA-256 in the caller.
        self.entry_getter_address = ptr.from_address(solidimport_module + 0x18558).value
        if not self.entry_getter_address:
            raise RuntimeError('SolidImport MFC entry import is null')
        module = ptr()
        if not self.k.GetModuleHandleExW(6, self.entry_getter_address, C.byref(module)):
            raise C.WinError(C.get_last_error())
        path = C.create_unicode_buffer(32768)
        length = self.k.GetModuleFileNameW(module, path, len(path))
        if not 0 < length < len(path):
            raise RuntimeError('Cannot resolve the actually loaded MFC runtime')
        data = Path(path.value).read_bytes()
        if Path(path.value).name.casefold() != 'mfc140u.dll':
            raise RuntimeError('Expected installed Unicode MFC v14 runtime')
        exports = read_pe_exports(data)
        get_ordinal, begin_ordinal = worker_api_names(exports)
        self.event('mfc_thread_exports', path=path.value,
                   get_thread_ordinal=get_ordinal, begin_worker_ordinal=begin_ordinal,
                   name_count=len(exports['names']),
                   abi_reference='mfc_thread_api_reference.json, Microsoft 14.44.35211.0 public PDB')
        address = self.k.GetProcAddress(module, get_ordinal)
        # Refuse an inferred getter signature if the actual IAT disagrees.
        self.entry_import_matches_AfxGetThread = address == self.entry_getter_address
        if not self.entry_import_matches_AfxGetThread:
            raise RuntimeError('SolidImport entry import differs from AfxGetThread; inspect runtime exports')
        self.get_thread = C.WINFUNCTYPE(ptr)(address)
        begin = self.k.GetProcAddress(module, begin_ordinal)
        if not begin:
            raise RuntimeError('GetProcAddress(AfxBeginThread) failed')
        self.begin = C.WINFUNCTYPE(ptr, ptr, ptr, C.c_int32, u32, u32, ptr)(begin)
        self.event('mfc_runtime', path=path.value, sha256=hashlib.sha256(data).hexdigest(),
                   getter_name=GET_THREAD_NAME, begin_name=BEGIN_WORKER_NAME,
                   entry_import_matches_AfxGetThread=True,
                   caller_thread_id=self.k.GetCurrentThreadId(),
                   caller_has_mfc_thread=bool(self.get_thread()))

    def run_terminal(self, callback, timeout):
        """Callback must terminate the process; returning is an explicit failure.

        Never dereference the AfxBeginThread return: default MFC auto-delete can
        destroy it once the controlling callback exits. No guessed field offsets.
        """
        callback_type = C.CFUNCTYPE(C.c_uint32, C.c_void_p)
        self.event('before_AfxBeginThread')

        def controlling(unused):
            try:
                present = bool(self.get_thread())
                self.event('mfc_worker_entered', thread_id=self.k.GetCurrentThreadId(),
                           has_mfc_thread=present)
                if not present:
                    raise RuntimeError('AfxBeginThread did not establish an MFC thread')
                callback()
                raise RuntimeError('Terminal diagnostic callback unexpectedly returned')
            except BaseException as error:
                self.event('mfc_worker_error', type=type(error).__name__, message=str(error))
                import os
                os._exit(84)
            return 84  # Unreachable, retained for the UINT callback contract.

        native = callback_type(controlling)
        self.pins.append(native)
        if not self.begin(C.cast(native, C.c_void_p), None, 0, 0, 0, None):
            raise RuntimeError('AfxBeginThread failed')
        self.event('AfxBeginThread_returned_nonnull')
        # The controller process also enforces the timeout. Waiting here releases
        # Python's GIL, allowing the MFC callback to enter the Python interpreter.
        threading.Event().wait(timeout)
        raise TimeoutError('MFC diagnostic thread did not terminate the process')
