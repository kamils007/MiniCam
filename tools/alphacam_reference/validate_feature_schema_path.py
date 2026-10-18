"""Configuration lifecycle and catalog stability without Windows native calls."""
from pathlib import Path
import tempfile
import unittest

from feature_builds import CURRENT_SHA256
from feature_schema_path import NativeSchemaPath, PATH_RVA


class Strings:
    def __init__(self):
        self.value = ''
        self.calls = []

    def read(self, address):
        return self.value

    def assign(self, address, value):
        self.calls.append((address, value))
        self.value = value


class SchemaPathTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.schemas = self.directory / 'Schemas'
        self.schemas.mkdir()
        self.file = self.schemas / 'test_schema.s_t'
        self.file.write_bytes(b'synthetic fixture; not a vendor schema')
        self.strings = Strings()

    def create(self):
        return NativeSchemaPath(100, self.directory, CURRENT_SHA256, self.strings)

    def test_unknown_build_rejected_before_memory_or_files(self):
        with self.assertRaises(ValueError):
            NativeSchemaPath(0, '/missing/path', 'unknown')

    def test_native_assignment_and_restore_preserve_existing_catalog(self):
        adapter = self.create()
        info = adapter.install()
        self.assertTrue(info['native_readback_matches'])
        self.assertEqual(self.strings.calls[0][0], 100 + PATH_RVA)
        self.assertTrue(self.strings.value.endswith('\\'))
        result = adapter.close()
        self.assertTrue(result['ok'])
        self.assertEqual(self.strings.value, '')
        self.assertEqual(info['files_before'], result['files_after'])
        self.assertEqual(result, adapter.close())

    def test_existing_native_configuration_not_overwritten(self):
        self.strings.value = 'preconfigured'
        with self.assertRaisesRegex(ValueError, 'already configured'):
            self.create()
        self.assertFalse(self.strings.calls)

    def test_catalog_mutation_and_native_path_mutation_are_detected(self):
        adapter = self.create()
        adapter.install()
        self.file.write_bytes(b'changed')
        self.strings.value = 'unexpected'
        result = adapter.close()
        self.assertFalse(result['ok'])
        self.assertFalse(result['configured_value_unchanged'])
        self.assertFalse(result['files_unchanged'])
        self.assertTrue(result['previous_path_restored'])

    def test_missing_catalog_rejected_before_native_assignment(self):
        self.file.unlink()
        with self.assertRaisesRegex(ValueError, 'Missing'):
            self.create()
        self.assertFalse(self.strings.calls)

    def test_failed_install_readback_still_restores_original_value(self):
        adapter = self.create()
        original = self.strings.assign
        def assign(address, value):
            original(address, value + 'wrong' if value else value)
        self.strings.assign = assign
        with self.assertRaisesRegex(RuntimeError, 'readback'):
            adapter.install()
        self.assertFalse(adapter.close()['ok'])
        self.assertEqual(self.strings.value, '')


if __name__ == '__main__':
    unittest.main()
