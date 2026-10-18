"""Portable checks for bounded, read-only standalone discovery."""
from pathlib import Path
import json
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from read_standalone_dependencies import (candidates, class_registration,
                                         license_route, main, module_metadata, pe_imports)
from test_mfc_thread_checks import fixture


def imports_fixture():
    data = fixture(named=True)
    struct.pack_into('<II', data, 0x98 + 120, 0x2400, 40)
    struct.pack_into('<IIIII', data, 0xA00, 0, 0, 0, 0x2440, 0)
    data[0xA40:0xA4D] = b'original.dll\0'
    return data


class StandaloneDependencyChecks(unittest.TestCase):
    def test_imports_are_read_from_file_not_executed(self):
        data = imports_fixture()
        self.assertEqual(pe_imports(data), ['original.dll'])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'sample.dll'
            path.write_bytes(data)
            metadata = module_metadata(path)
            self.assertEqual(metadata['imports'], ['original.dll'])
            self.assertTrue(metadata['unchanged_after_read'])
            self.assertEqual(path.read_bytes(), data)

    def test_bad_or_unterminated_imports_are_rejected(self):
        for change in ('wrong_machine', 'outside_section', 'missing_terminator', 'short_directory'):
            data = imports_fixture()
            if change == 'wrong_machine':
                struct.pack_into('<H', data, 0x84, 0x14C)
            elif change == 'outside_section':
                struct.pack_into('<I', data, 0xA0C, 0xFFFFF)
            elif change == 'missing_terminator':
                struct.pack_into('<I', data, 0xA14, 1)
                struct.pack_into('<I', data, 0xA20, 0x2440)
            else:
                struct.pack_into('<I', data, 0x98 + 124, 19)
            with self.assertRaises(ValueError, msg=change):
                pe_imports(data)
        with self.assertRaises(ValueError):
            pe_imports(b'MZ')

    def test_exe_registration_is_not_a_standalone_dll(self):
        for servers, expected in [
            ({}, 'no_standalone_server_registration_found'),
            ({'LocalServer32': {'path': 'Acam.exe'}}, 'out_of_process_registration_only_not_activated'),
            ({'InprocServer32': {'path': 'Acam.exe'}}, 'no_standalone_server_registration_found'),
            ({'InprocServer32': {'path': 'provider.dll'}}, 'inproc_registration_found_not_activated_or_validated'),
        ]:
            self.assertEqual(license_route({'servers': servers}), expected)

    def test_registry_discovery_preserves_command_arguments_without_running_them(self):
        class FakeRegistry:
            def value(self, key, name=''):
                return '"C:\\Program Files\\Acam.exe" /Automation' if key.endswith('LocalServer32') else None
        data = class_registration(FakeRegistry(), '{28AA001D-94E1-44AB-9571-E5E4E3BDB103}')
        self.assertEqual(data['servers']['LocalServer32']['path'], r'C:\Program Files\Acam.exe')
        self.assertEqual(license_route(data), 'out_of_process_registration_only_not_activated')

    def test_scan_does_not_follow_links_or_collect_license_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'installation'
            outside = Path(directory) / 'outside'
            root.mkdir()
            outside.mkdir()
            (root / 'Hexagon.TPIF.dll').write_bytes(b'MZ')
            (root / 'customer.lic').write_text('not part of the report')
            (outside / 'License.dll').write_bytes(b'outside')
            (root / 'linked').symlink_to(outside, target_is_directory=True)
            paths, limits = candidates([root], [])
            self.assertEqual(paths, [root / 'Hexagon.TPIF.dll'])
            self.assertFalse(limits['scan_truncated'])

    def test_finished_discovery_never_claims_conversion_or_bundles_modules(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'installation'
            root.mkdir()
            (root / 'Acam.exe').write_bytes(b'not executed')
            out = Path(directory) / 'result'
            argv = ['discovery', '--root', str(root), '--out', str(out)]
            with patch.object(sys, 'platform', 'win32'), patch.object(sys, 'argv', argv), \
                 patch('read_standalone_dependencies.process_names', return_value=[]), \
                 patch('read_standalone_dependencies.collect', return_value={}), \
                 patch.dict(sys.modules, {'winreg': object()}):
                self.assertEqual(main(), 0)
            report = json.loads((out / 'standalone_dependencies.json').read_text())
            self.assertTrue(report['discovery_completed'])
            for key in ['conversion_attempted', 'parasolid_created', 'ard_created',
                        'com_activation_attempted', 'vendor_native_code_loaded']:
                self.assertIs(report[key], False)
            with zipfile.ZipFile(str(out) + '.zip') as archive:
                self.assertEqual(archive.namelist(), ['standalone_dependencies.json'])


if __name__ == '__main__':
    unittest.main()
