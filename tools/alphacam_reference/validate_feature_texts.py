"""Text catalog, ABI layout and ownership checks without loading Feature.dll.

Native CString calls are replaced by a fault-injecting test double. Only the
Windows probe can verify the actual MFC calls and the importer integration.
"""
import ctypes as C
from pathlib import Path
import tempfile
import unittest

from feature_texts import (Catalog, COUNTS, DLL_SHA256, NativeTextTable,
                           TABLE_POINTER_RVA, parse_po)

HERE = Path(__file__).resolve().parent


class StringDouble:
    def __init__(self):
        self.values = {}
        self.destroyed = []
        self.fail_at = None

    def construct(self, address):
        if address in self.values:
            raise RuntimeError("double construction")
        self.values[address] = ""

    def assign(self, address, value):
        if len(self.values) == self.fail_at:
            raise RuntimeError("injected allocation failure")
        self.values[address] = value

    def read(self, address):
        return self.values[address]

    def destroy(self, address):
        del self.values[address]
        self.destroyed.append(address)


class TextTests(unittest.TestCase):
    def setUp(self):
        self.memory = C.create_string_buffer(TABLE_POINTER_RVA + 8)
        self.base = C.addressof(self.memory)
        self.cell = C.c_void_p.from_address(self.base + TABLE_POINTER_RVA)
        self.cell.value = 0x12345678
        self.strings = StringDouble()
        self.catalog = Catalog(HERE / "Feature.po")
        self.adapter = NativeTextTable(self.base, self.catalog, DLL_SHA256, self.strings)

    def test_actual_catalog_and_faulting_pointer_chain(self):
        info = self.adapter.install()
        # Replay the exact address computation immediately before the captured
        # Feature.dll+0x4882D call: [global+0x38]+0x28 -> Feature.po 80-6.
        group = C.c_void_p.from_address(self.cell.value + 0x38).value
        text = self.strings.read(group + 0x28)
        self.assertEqual(text, "Zaimportowany. Przetwarzanie...")
        self.assertEqual(info["cstring_count"], 583)
        self.assertEqual(info["missing_feature_ids"], ["90-83", "120-28", "120-29"])
        self.assertEqual(info["external_context_strings_empty"], 7)
        for index, count in enumerate(COUNTS):
            pointer = C.c_void_p.from_address(self.cell.value + index * 8).value
            self.assertEqual(C.c_uint64.from_address(pointer - 8).value, count)
        self.adapter.close()
        self.assertEqual(self.cell.value, 0x12345678)
        self.assertFalse(self.strings.values)
        self.assertEqual(len(self.strings.destroyed), 583)
        self.adapter.close()  # Repeated cleanup must not double-destroy strings.
        self.assertEqual(len(self.strings.destroyed), 583)

    def test_partial_failure_never_publishes_and_releases_storage(self):
        self.strings.fail_at = 17
        with self.assertRaisesRegex(RuntimeError, "injected allocation failure"):
            self.adapter.install()
        self.assertEqual(self.cell.value, 0x12345678)
        self.assertFalse(self.strings.values)
        self.assertEqual(len(self.strings.destroyed), 17)
        self.assertTrue(self.adapter.closed)

    def test_changed_foreign_table_is_never_overwritten_or_freed(self):
        self.adapter.install()
        self.cell.value = 0x87654321
        with self.assertRaisesRegex(RuntimeError, "storage remains pinned"):
            self.adapter.close()
        self.assertEqual(self.cell.value, 0x87654321)
        self.assertEqual(len(self.strings.values), 583)
        self.cell.value = C.addressof(self.adapter.pointers)
        self.adapter.close()

    def test_rejects_different_dll_build_before_touching_native_state(self):
        with self.assertRaisesRegex(ValueError, "verified x64 DLL"):
            NativeTextTable(0, self.catalog, "wrong hash", self.strings)

    def test_catalog_is_guarded_and_parsing_preserves_unicode(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Feature.po"
            path.write_text('msgid "80-6"\nmsgstr "wrong"', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "differs"):
                Catalog(path)
        result = parse_po('\ufeffmsgctxt "feature|"\nmsgid "10-1"\n'
                          'msgstr "Zażółć\\n"\n"gęślą \\\"jaźń\\\""\n')
        self.assertEqual(result[("feature|", "10-1")], 'Zażółć\ngęślą "jaźń"')
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            parse_po('msgid "a"\nmsgstr "x"\n\nmsgid "a"\nmsgstr "y"')
        with self.assertRaisesRegex(ValueError, "Invalid gettext"):
            parse_po('msgid "a"\nmsgstr "a\\0b"')


if __name__ == "__main__":
    unittest.main(verbosity=2)
