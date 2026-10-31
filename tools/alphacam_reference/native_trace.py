"""First-chance x64 access-violation diagnostics in this probe process only.

The handler always returns EXCEPTION_CONTINUE_SEARCH. It does not alter the
exception's CONTEXT. Stack unwinding operates on a separate aligned copy.
"""
import ctypes as C
import json
import ntpath
import os
import struct

CONTEXT_SIZE = 1232
CONTEXT_REGISTERS = {name: 120 + i * 8 for i, name in enumerate([
    "Rax", "Rcx", "Rdx", "Rbx", "Rsp", "Rbp", "Rsi", "Rdi",
    "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15", "Rip"])}
ACCESS_VIOLATION = 0xC0000005


def decode_exception(data):
    code, flags, nested, address, count = struct.unpack_from("<IIQQI", data)
    params = struct.unpack_from("<15Q", data, 32)[:min(count, 15)]
    return {"code": "0x%08X" % code, "flags": flags,
            "exception_address": hex(address), "information": [hex(p) for p in params],
            "operation": {0: "read", 1: "write", 8: "execute"}.get(params[0]) if params else None,
            "fault_address": hex(params[1]) if len(params) >= 2 else None}


def decode_registers(data):
    return {name: struct.unpack_from("<Q", data, offset)[0]
            for name, offset in CONTEXT_REGISTERS.items()}


class WindowsTraceAPI:
    def __init__(self):
        if os.name != "nt" or C.sizeof(C.c_void_p) != 8:
            raise RuntimeError("Native exception tracing requires Windows x64")
        self.k = C.WinDLL("kernel32")
        ptr, u32, u64 = C.c_void_p, C.c_uint32, C.c_uint64
        self.k.ReadProcessMemory.argtypes = [ptr, ptr, ptr, C.c_size_t, C.POINTER(C.c_size_t)]
        self.k.ReadProcessMemory.restype = C.c_int32
        self.k.GetModuleHandleExW.argtypes = [u32, ptr, C.POINTER(ptr)]
        self.k.GetModuleHandleExW.restype = C.c_int32
        self.k.GetModuleFileNameW.argtypes = [ptr, C.c_wchar_p, u32]
        self.k.GetModuleFileNameW.restype = u32
        self.k.GetLastError.argtypes = []
        self.k.GetLastError.restype = u32
        self.k.SetLastError.argtypes = [u32]
        self.k.SetLastError.restype = None
        self.k.AddVectoredExceptionHandler.argtypes = [u32, ptr]
        self.k.AddVectoredExceptionHandler.restype = ptr
        self.k.RemoveVectoredExceptionHandler.argtypes = [ptr]
        self.k.RemoveVectoredExceptionHandler.restype = u32
        self.k.RtlLookupFunctionEntry.argtypes = [u64, C.POINTER(u64), ptr]
        self.k.RtlLookupFunctionEntry.restype = ptr
        self.k.RtlVirtualUnwind.argtypes = [u32, u64, u64, ptr, ptr,
                                          C.POINTER(ptr), C.POINTER(u64), ptr]
        self.k.RtlVirtualUnwind.restype = ptr
        self.callback = None

    def read(self, address, size):
        if not address or not 0 < size <= 4096:
            raise ValueError("Invalid bounded memory-read request")
        data = (C.c_ubyte * size)()
        count = C.c_size_t()
        if not self.k.ReadProcessMemory(C.c_void_p(-1), C.c_void_p(address),
                                       data, size, C.byref(count)) or count.value != size:
            raise OSError("ReadProcessMemory failed at " + hex(address))
        return bytes(data)

    def where(self, address):
        result = {"address": hex(address), "module": None, "rva": None}
        module = C.c_void_p()
        # FROM_ADDRESS | UNCHANGED_REFCOUNT; no DLL is loaded by this lookup.
        if address and self.k.GetModuleHandleExW(6, C.c_void_p(address), C.byref(module)):
            name = C.create_unicode_buffer(1024)
            self.k.GetModuleFileNameW(module, name, len(name))
            result.update(module=ntpath.basename(name.value), module_base=hex(module.value),
                          rva=hex(address - module.value))
        return result

    def stack_frames(self, original, limit=16):
        buffer = C.create_string_buffer(CONTEXT_SIZE + 15)
        address = (C.addressof(buffer) + 15) & ~15
        C.memmove(address, original, CONTEXT_SIZE)
        frames = []
        stop = "frame_limit"
        for _ in range(limit):
            rip = C.c_uint64.from_address(address + CONTEXT_REGISTERS["Rip"]).value
            rsp = C.c_uint64.from_address(address + CONTEXT_REGISTERS["Rsp"]).value
            if not rip or not rsp:
                stop = "end_of_stack"
                break
            frames.append({**self.where(rip), "rsp": hex(rsp)})
            try:
                self.read(rsp, 8)
                image = C.c_uint64()
                entry = self.k.RtlLookupFunctionEntry(rip, C.byref(image), None)
                if entry:
                    handler = C.c_void_p()
                    establisher = C.c_uint64()
                    self.k.RtlVirtualUnwind(0, image.value, rip, entry, address,
                                            C.byref(handler), C.byref(establisher), None)
                else:
                    C.c_uint64.from_address(address + CONTEXT_REGISTERS["Rip"]).value = \
                        struct.unpack("<Q", self.read(rsp, 8))[0]
                    C.c_uint64.from_address(address + CONTEXT_REGISTERS["Rsp"]).value = rsp + 8
                next_rsp = C.c_uint64.from_address(address + CONTEXT_REGISTERS["Rsp"]).value
                if next_rsp <= rsp or next_rsp - rsp > 16 * 1024 * 1024:
                    stop = "unreliable_stack_pointer"
                    break
            except Exception as error:
                stop = "unwind_error: " + str(error)
                break
        return {"method": "RtlVirtualUnwind", "frames": frames, "stop_reason": stop}

    def register(self, function):
        self.callback = C.WINFUNCTYPE(C.c_int32, C.c_void_p)(function)
        handle = self.k.AddVectoredExceptionHandler(1, C.cast(self.callback, C.c_void_p))
        if not handle:
            raise OSError("AddVectoredExceptionHandler failed")
        return handle

    def unregister(self, handle):
        return bool(self.k.RemoveVectoredExceptionHandler(handle))

    def last_error(self):
        return self.k.GetLastError()

    def restore_error(self, value):
        self.k.SetLastError(value)


_PINNED_ON_REMOVE_FAILURE = []


class NativeExceptionTrace:
    def __init__(self, path, host_snapshot=None, api=None):
        self.api = api or WindowsTraceAPI()
        self.host_snapshot = host_snapshot or (lambda: {})
        self.records = []
        self.phase = "import"
        self.handle = None
        self.handling = False
        self.closed = False
        self.fd = os.open(str(path), os.O_CREAT | os.O_EXCL | os.O_WRONLY |
                          getattr(os, "O_BINARY", 0), 0o600)

    def install(self):
        self.handle = self.api.register(self._handler)

    def _persist(self, value):
        data = (json.dumps(value, ensure_ascii=True) + "\n").encode("utf-8")
        while data:
            written = os.write(self.fd, data)
            if written <= 0:
                raise OSError("Short diagnostic write")
            data = data[written:]

    def _handler(self, pointers):
        if self.closed or self.handling or len(self.records) >= 4:
            return 0
        self.handling = True
        try:
            saved_error = self.api.last_error()
        except BaseException:
            self.handling = False
            return 0
        try:
            record_ptr, context_ptr = struct.unpack("<QQ", self.api.read(pointers, 16))
            raw_record = self.api.read(record_ptr, 152)
            if struct.unpack_from("<I", raw_record)[0] != ACCESS_VIOLATION:
                return 0
            raw_context = self.api.read(context_ptr, CONTEXT_SIZE)
            registers = decode_registers(raw_context)
            item = {"event": "native_access_violation", "first_chance": True,
                    "phase": self.phase, **decode_exception(raw_record),
                    "location": self.api.where(registers["Rip"]),
                    "registers": {k: hex(v) for k, v in registers.items()},
                    "host_objects": self.host_snapshot()}
            for name, action in [
                ("instruction_bytes", lambda: self.api.read(registers["Rip"], 24).hex()),
                ("raw_stack_qwords", lambda: [hex(x) for x in struct.unpack(
                    "<64Q", self.api.read(registers["Rsp"], 512))]),
                ("stack", lambda: self.api.stack_frames(raw_context))]:
                try:
                    item[name] = action()
                except Exception as error:
                    item[name + "_error"] = str(error)
            self.records.append(item)
            self._persist(item)
        except BaseException as error:
            try:
                self._persist({"event": "native_trace_error", "error": str(error)})
            except BaseException:
                pass
        finally:
            try:
                self.api.restore_error(saved_error)
            except BaseException:
                pass
            self.handling = False
        # Preserve normal Windows/ctypes exception handling, including failures.
        return 0

    def close(self):
        if self.closed:
            return
        if self.handle and not self.api.unregister(self.handle):
            # An installed callback must remain alive even if removal failed.
            _PINNED_ON_REMOVE_FAILURE.append(self)
        self.closed = True
        os.close(self.fd)
