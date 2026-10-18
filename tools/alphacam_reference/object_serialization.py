"""Diagnostic ObjectQueryWrite2 / ObjectDoWrite2 call for the matched 2025 DLL.

This writes the add-in's record, not an entire ARD document. FILE* and fwrite
come from the same CRT imports already bound by Feature.dll. No machine code
or license state is modified. Native signatures were traced in this exact DLL.
"""
import ctypes as C
import hashlib
from pathlib import Path
import struct
from feature_builds import OLD_SHA256, profile

PTR = C.c_void_p
LONG = C.c_int32
SIZE = C.c_size_t
IAT_RVAS = {"_wfopen_s": 0x13CE28, "fwrite": 0x13CE38,
            "fflush": 0x13CE20, "fclose": 0x13CE68}


def parse_fragment(data):
    """Read the traced v3 prefix and bounded X_B span; leave attributes opaque."""
    position = 0

    def take(count):
        nonlocal position
        if count < 0 or position + count > len(data):
            raise ValueError("Truncated object record at byte " + str(position))
        value = data[position:position + count]
        position += count
        return value

    def u32():
        return struct.unpack("<I", take(4))[0]

    def string_bytes():
        count = u32()
        if count > 32768:
            raise ValueError("Object source string exceeds the diagnostic limit")
        raw = take(count)
        if not raw or raw[-1:] != b"\0":
            raise ValueError("Object source string is not NUL terminated")
        return {"bytes_hex": raw.hex(),
                "display_ascii_escaped": raw[:-1].decode("ascii", errors="backslashreplace")}

    version = u32()
    if version != 3:
        raise ValueError("Only the traced object record version 3 is recognized; got " + str(version))
    count = u32()
    if count > 1024:
        raise ValueError("Object source record count exceeds the diagnostic limit")
    records = []
    for unused in range(count):
        records.append({"id": u32(), "source_path": string_bytes(),
                        "source_format": string_bytes(), "word_at_0x18": u32(),
                        "byte_at_0x1c": take(1)[0], "word_at_0x20": u32()})
    size = u32()
    offset = position
    geometry = take(size)
    if size == 0 or not geometry.startswith(b"**") or b"FORMAT=binary" not in geometry[:4096]:
        raise ValueError("The declared geometry span has no recognized binary Parasolid header")
    tail = data[position:]
    if len(tail) < 4:
        raise ValueError("The object record has no trailing body-metadata count")
    body_count = struct.unpack_from("<I", tail)[0]
    return {"recognized_prefix": True, "record_version": version,
            "source_record_count": count, "source_records": records,
            "geometry_offset": offset, "geometry_size_bytes": size,
            "geometry_sha256": hashlib.sha256(geometry).hexdigest(),
            "has_parasolid_header": True, "trailing_body_metadata_count": body_count,
            "trailing_size_bytes": len(tail), "trailing_preview_hex": tail[:256].hex(),
            "trailing_metadata_fully_decoded": False}


class NativeObjectWriter:
    def __init__(self, module, get_export, callback_factory=None, build_sha256=OLD_SHA256):
        self.factory = callback_factory or getattr(C, "WINFUNCTYPE", None)
        if self.factory is None or C.sizeof(PTR) != 8:
            raise RuntimeError("Windows x64 is required")
        self.module = int(module)
        self.iat_rvas = profile(build_sha256)['crt_iat']
        self.get_export = get_export
        self.open_file = self.imported("_wfopen_s", LONG, C.POINTER(PTR), C.c_wchar_p, C.c_wchar_p)
        self.flush_file = self.imported("fflush", LONG, PTR)
        self.close_file = self.imported("fclose", LONG, PTR)
        self.fwrite_address = self.import_address("fwrite")

    def import_address(self, name):
        pointer = PTR.from_address(self.module + self.iat_rvas[name]).value
        if not pointer:
            raise RuntimeError("Unbound CRT import: " + name)
        return pointer

    def imported(self, name, result, *arguments):
        return self.factory(result, *arguments)(self.import_address(name))

    def exported(self, name, result, *arguments):
        address = self.get_export(name)
        if not address:
            raise RuntimeError("Missing native export: " + name)
        return self.factory(result, *arguments)(address)

    def write(self, context, destination, emit, error_reader=None):
        destination = Path(destination)
        if destination.exists():
            raise FileExistsError("Refusing to overwrite " + str(destination))
        # Query writes at most 63 bytes. Independent canaries guard its buffer.
        buffer = (C.c_ubyte * 80)(*([0xA5] * 8 + [0] * 64 + [0x5A] * 8))
        query = self.exported("ObjectQueryWrite2", LONG, PTR, PTR, LONG)
        emit("before_object_query_write", target_version_argument=0)
        query_code = query(C.byref(context), C.byref(buffer, 8), 0)
        if list(buffer[:8]) != [0xA5] * 8 or list(buffer[72:]) != [0x5A] * 8:
            raise RuntimeError("ObjectQueryWrite2 overwrote its name buffer")
        raw_name = bytes(buffer[8:72])
        if b"\0" not in raw_name:
            raise ValueError("ObjectQueryWrite2 name is not NUL terminated")
        name = raw_name.split(b"\0", 1)[0].decode("ascii")
        result = {"query_return_code": int(query_code), "object_name": name,
                  "target_version_argument": 0, "query_buffer_guards_intact": True,
                  "complete_ard": False}
        emit("after_object_query_write", result=dict(result))
        if query_code != 1 or not name:
            result.update(ok=False, skipped="The native query did not accept a nonempty object")
            return result

        stream = PTR()
        # 'x' requires a new file, including if another process creates it after
        # the exists() check. This stream belongs to Feature.dll's own CRT.
        errno = self.open_file(C.byref(stream), str(destination), "wbx")
        if errno != 0 or not stream.value:
            raise OSError(errno, "_wfopen_s could not create a new object record")
        close_code = None
        try:
            write = self.exported("ObjectDoWrite2", LONG, PTR, PTR, PTR)
            emit("before_object_do_write", output=str(destination),
                 crt_imports=list(self.iat_rvas), callback="Feature.dll IAT fwrite")
            if error_reader:
                result['kernel_error_before_write'] = error_reader()
                emit('kernel_error_before_object_write', result=result['kernel_error_before_write'])
            result["write_return_code"] = int(write(C.byref(context), stream, self.fwrite_address))
            if error_reader:
                result['kernel_error_after_write'] = error_reader()
                emit('kernel_error_after_object_write', result=result['kernel_error_after_write'])
            # ObjectDoWrite2 returns 0 for success and 1 for failure; not HRESULT.
            result["flush_return_code"] = int(self.flush_file(stream))
        finally:
            close_code = int(self.close_file(stream))
            stream = PTR()
        result["close_return_code"] = close_code
        data = destination.read_bytes()
        result["output"] = {"filename": destination.name, "size_bytes": len(data),
                            "sha256": hashlib.sha256(data).hexdigest()}
        result["ok"] = (result["write_return_code"] == 0
                        and result["flush_return_code"] == 0 and close_code == 0 and len(data) > 0)
        try:
            result["fragment"] = parse_fragment(data)
        except ValueError as error:
            result["parse_error"] = str(error)
        emit("after_object_do_write", result=result)
        return result
