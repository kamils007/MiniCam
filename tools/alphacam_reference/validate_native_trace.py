"""Portable capture/recording tests with synthetic exception/context memory.

These exercise the trace logic, not Windows handler registration or unwinding.
"""
import json
from pathlib import Path
import struct
import tempfile
import unittest

from native_trace import (NativeExceptionTrace, CONTEXT_SIZE, CONTEXT_REGISTERS,
                          ACCESS_VIOLATION, decode_registers)


class FakeAPI:
    def __init__(self, code=ACCESS_VIOLATION):
        self.error = 123
        self.memory = {0x1000: struct.pack("<QQ", 0x2000, 0x3000)}
        self.memory[0x2000] = struct.pack("<IIQQI4x15Q", code, 0, 0, 0x180012345,
                                         2, 0, 0xffffffffffffffff, *([0] * 13))
        context = bytearray(CONTEXT_SIZE)
        for i, (name, offset) in enumerate(CONTEXT_REGISTERS.items()):
            struct.pack_into("<Q", context, offset, i + 10)
        struct.pack_into("<Q", context, CONTEXT_REGISTERS["Rip"], 0x180012345)
        struct.pack_into("<Q", context, CONTEXT_REGISTERS["Rsp"], 0x5000)
        self.memory[0x3000] = bytes(context)
        self.memory[0x180012345] = bytes(range(24))
        self.memory[0x5000] = struct.pack("<64Q", *range(64))
        self.callback = None
        self.unregistered = False

    def read(self, address, size):
        self.error = 456
        data = self.memory[address]
        if len(data) < size:
            raise OSError("short read")
        return data[:size]

    def where(self, address):
        return {"address": hex(address), "module": "Feature.dll", "rva": hex(address - 0x180000000)}

    def stack_frames(self, raw_context):
        return {"method": "test-double", "frames": [self.where(decode_registers(raw_context)["Rip"])]}

    def last_error(self):
        return self.error

    def restore_error(self, value):
        self.error = value

    def register(self, function):
        self.callback = function
        return 7

    def unregister(self, handle):
        self.unregistered = handle == 7
        return self.unregistered


class TraceTests(unittest.TestCase):
    def test_register_layouts_and_complete_record(self):
        self.assertEqual(CONTEXT_REGISTERS["Rsp"], 152)
        self.assertEqual(CONTEXT_REGISTERS["Rip"], 248)
        api = FakeAPI()
        original = api.memory[0x3000]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            trace = NativeExceptionTrace(path, lambda: {"IDrawing": {"pointer": "0x1234"}}, api)
            trace.install()
            self.assertEqual(api.callback(0x1000), 0)
            self.assertEqual(api.error, 123)
            self.assertEqual(api.memory[0x3000], original)
            trace.close()
            self.assertTrue(api.unregistered)
            saved = json.loads(path.read_text())
            self.assertEqual(saved["location"]["rva"], "0x12345")
            self.assertEqual(saved["operation"], "read")
            self.assertEqual(saved["fault_address"], "0xffffffffffffffff")
            self.assertEqual(len(saved["raw_stack_qwords"]), 64)
            self.assertEqual(saved["host_objects"]["IDrawing"]["pointer"], "0x1234")

    def test_other_exception_is_not_consumed_or_recorded(self):
        with tempfile.TemporaryDirectory() as directory:
            api = FakeAPI(0xE06D7363)
            path = Path(directory) / "trace.jsonl"
            trace = NativeExceptionTrace(path, api=api)
            self.assertEqual(trace._handler(0x1000), 0)
            self.assertEqual(api.error, 123)
            trace.close()
            self.assertEqual(path.read_bytes(), b"")

    def test_capture_is_bounded_and_reentrant_calls_are_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            trace = NativeExceptionTrace(Path(directory) / "trace.jsonl", api=FakeAPI())
            trace.handling = True
            self.assertEqual(trace._handler(0x1000), 0)
            self.assertEqual(trace.records, [])
            trace.handling = False
            for _ in range(10):
                self.assertEqual(trace._handler(0x1000), 0)
            self.assertEqual(len(trace.records), 4)
            trace.close()

    def test_stack_read_failure_preserves_fault_location(self):
        with tempfile.TemporaryDirectory() as directory:
            api = FakeAPI()
            del api.memory[0x5000]
            trace = NativeExceptionTrace(Path(directory) / "trace.jsonl", api=api)
            self.assertEqual(trace._handler(0x1000), 0)
            self.assertEqual(trace.records[0]["location"]["rva"], "0x12345")
            self.assertIn("raw_stack_qwords_error", trace.records[0])
            trace.close()

    def test_bad_record_read_returns_continue_search_and_restores_error(self):
        with tempfile.TemporaryDirectory() as directory:
            api = FakeAPI()
            trace = NativeExceptionTrace(Path(directory) / "trace.jsonl", api=api)
            self.assertEqual(trace._handler(0x1111), 0)
            self.assertFalse(trace.handling)
            self.assertEqual(api.error, 123)
            trace.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
