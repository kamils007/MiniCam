"""Bounded record parser and CRT/ABI adapter checks; no Feature.dll execution."""
import ctypes as C
from pathlib import Path
import struct
import tempfile
import unittest

from com_host import Variant
import object_serialization as S
from feature_builds import OLD_SHA256, CURRENT_SHA256, profile


def fragment():
    def text(value):
        return struct.pack("<I", len(value) + 1) + value + b"\0"
    geometry = b"**PARASOLID\nFORMAT=binary\n\0\x80BODY"
    data = (struct.pack("<III", 3, 1, 7) + text(b"C:\\test\\bry\xb3a.x_b")
            + text(b"Parasolid") + struct.pack("<IBII", 123, 1, 1, len(geometry))
            + geometry + struct.pack("<III", 1, 7, 0))
    return data, geometry


class FakeNative:
    """Real local callback pointers and an actual temporary Python-owned file."""
    def __init__(self, write_code=0, flush_code=0, close_code=0, query_code=1, build_sha256=OLD_SHA256):
        self.build_sha256 = build_sha256
        self.memory = (C.c_ubyte * 0x140000)()
        self.module = C.addressof(self.memory)
        self.callbacks, self.events, self.errors = [], [], []
        self.file = None
        self.write_code, self.flush_code = write_code, flush_code
        self.close_code, self.query_code = close_code, query_code
        self.data = fragment()[0]

        def install(function, result, types):
            def guarded(*args):
                try:
                    return function(*args)
                except BaseException as error:
                    self.errors.append(str(error))
                    return 1 if result is S.LONG else 0
            callback = C.CFUNCTYPE(result, *types)(guarded)
            self.callbacks.append(callback)
            return C.cast(callback, S.PTR).value

        def opened(output, path, mode):
            self.events.append("open")
            if mode != "wbx":
                raise AssertionError("Output is not opened exclusively in binary mode")
            self.file = Path(path).open("xb")
            output[0] = 0x9876
            return 0

        def fwrite(buffer, size, count, stream):
            if stream != 0x9876:
                raise AssertionError("The CRT stream pointer changed")
            self.file.write(C.string_at(buffer, size * count))
            return count

        def flush(stream):
            self.events.append("flush")
            self.file.flush()
            return self.flush_code

        def close(stream):
            self.events.append("close")
            self.file.close()
            return self.close_code

        addresses = {
            "_wfopen_s": install(opened, S.LONG, [C.POINTER(S.PTR), C.c_wchar_p, C.c_wchar_p]),
            "fwrite": install(fwrite, S.SIZE, [S.PTR, S.SIZE, S.SIZE, S.PTR]),
            "fflush": install(flush, S.LONG, [S.PTR]),
            "fclose": install(close, S.LONG, [S.PTR])}
        for name, address in addresses.items():
            S.PTR.from_address(self.module + profile(build_sha256)['crt_iat'][name]).value = address

        def query(context, name, version):
            self.events.append("query")
            value = C.cast(context, C.POINTER(Variant)).contents
            if value.vt != 9 or value.value.pointer != 0x1234 or version != 0:
                raise AssertionError("The native context or version argument changed")
            C.memmove(name, b"LicomUKRJMParaSolid\0", 19)
            return self.query_code

        def write(context, stream, callback):
            self.events.append("write")
            buffer = C.create_string_buffer(self.data)
            writer = C.CFUNCTYPE(S.SIZE, S.PTR, S.SIZE, S.SIZE, S.PTR)(callback)
            written = writer(buffer, 1, len(self.data), stream)
            if written != len(self.data):
                raise AssertionError("fwrite did not return an item count")
            return self.write_code

        self.exports = {
            "ObjectQueryWrite2": install(query, S.LONG, [S.PTR, S.PTR, S.LONG]),
            "ObjectDoWrite2": install(write, S.LONG, [S.PTR, S.PTR, S.PTR])}

    def run(self, target, error_reader=None):
        context = Variant()
        context.vt, context.value.pointer = 9, 0x1234
        writer = S.NativeObjectWriter(self.module, self.exports.get, C.CFUNCTYPE, build_sha256=self.build_sha256)
        return writer.write(context, target, lambda *a, **kw: None, error_reader=error_reader)


class ObjectTests(unittest.TestCase):
    def test_optional_error_reader_observes_before_and_after_write_without_changing_result(self):
        with tempfile.TemporaryDirectory() as directory:
            native = FakeNative()
            def observe():
                return {'events_seen': len(native.events)}
            result = native.run(Path(directory) / 'observed.bin', error_reader=observe)
            self.assertTrue(result['ok'])
            self.assertEqual(result['write_return_code'], 0)
            self.assertEqual(result['kernel_error_before_write'], {'events_seen': 2})
            self.assertEqual(result['kernel_error_after_write'], {'events_seen': 3})
            self.assertEqual(native.events, ['query', 'open', 'write', 'flush', 'close'])

    def test_current_build_uses_its_crt_imports_for_the_entire_file_lifetime(self):
        with tempfile.TemporaryDirectory() as directory:
            native = FakeNative(build_sha256=CURRENT_SHA256)
            for rva in S.IAT_RVAS.values():
                self.assertIsNone(S.PTR.from_address(native.module + rva).value)
            target = Path(directory) / 'current.bin'
            result = native.run(target)
            self.assertTrue(result['ok'])
            self.assertEqual(target.read_bytes(), fragment()[0])
            self.assertEqual(native.events, ['query', 'open', 'write', 'flush', 'close'])
            self.assertFalse(native.errors)

    def test_parser_extracts_exact_geometry_and_preserves_ansi_source_bytes(self):
        data, geometry = fragment()
        parsed = S.parse_fragment(data)
        start = parsed["geometry_offset"]
        self.assertEqual(data[start:start + parsed["geometry_size_bytes"]], geometry)
        self.assertEqual(parsed["source_record_count"], 1)
        self.assertIn("b3", parsed["source_records"][0]["source_path"]["bytes_hex"])
        self.assertEqual(parsed["trailing_body_metadata_count"], 1)
        self.assertEqual(parsed["trailing_size_bytes"], 12)
        self.assertFalse(parsed["trailing_metadata_fully_decoded"])

    def test_truncated_prefix_geometry_and_tail_rejected(self):
        data, unused = fragment()
        for end in (0, 3, 4, 8, 12, 15, len(data) - 13, len(data) - 9):
            with self.subTest(end=end), self.assertRaises(ValueError):
                S.parse_fragment(data[:end])

    def test_unsupported_version_oversized_fields_and_invalid_geometry_rejected(self):
        data, unused = fragment()
        bad = [struct.pack("<I", 2) + data[4:],
               data[:4] + struct.pack("<I", 1025) + data[8:],
               data[:12] + struct.pack("<I", 32769) + data[16:],
               data.replace(b"FORMAT=binary", b"FORMAT=xxxxxx")]
        for value in bad:
            with self.assertRaises(ValueError):
                S.parse_fragment(value)

    def test_native_adapter_writes_file_with_matching_crt_and_closes_it(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "record.bin"
            native = FakeNative()
            result = native.run(target)
            self.assertTrue(result["ok"])
            self.assertEqual(result["object_name"], "LicomUKRJMParaSolid")
            self.assertEqual(target.read_bytes(), fragment()[0])
            self.assertEqual(native.events, ["query", "open", "write", "flush", "close"])
            self.assertTrue(native.file.closed)
            self.assertFalse(native.errors)
            self.assertTrue(result["query_buffer_guards_intact"])
            self.assertFalse(result["complete_ard"])

    def test_existing_output_is_preserved_before_any_native_call(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "record.bin"
            target.write_bytes(b"keep")
            native = FakeNative()
            with self.assertRaises(FileExistsError):
                native.run(target)
            self.assertEqual(target.read_bytes(), b"keep")
            self.assertEqual(native.events, [])

    def test_write_flush_or_close_error_is_not_reported_as_success(self):
        for parameters in ({"write_code": 1}, {"flush_code": -1}, {"close_code": -1}):
            with self.subTest(parameters=parameters), tempfile.TemporaryDirectory() as directory:
                native = FakeNative(**parameters)
                result = native.run(Path(directory) / "record.bin")
                self.assertFalse(result["ok"])
                self.assertTrue(native.file.closed)
                self.assertEqual(native.events[-1], "close")
                self.assertFalse(native.errors)

    def test_rejected_query_creates_no_output(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "record.bin"
            native = FakeNative(query_code=0)
            self.assertFalse(native.run(target)["ok"])
            self.assertFalse(target.exists())
            self.assertEqual(native.events, ["query"])
            self.assertFalse(native.errors)


if __name__ == "__main__":
    unittest.main(verbosity=2)
