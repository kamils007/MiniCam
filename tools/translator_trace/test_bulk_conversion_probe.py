"""Portable boundary checks; the native library calls are test doubles here."""
import ctypes as C
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import struct
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('bulk_probe', HERE / 'bulk_conversion_probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
STEP = b'ISO-10303-21;\nHEADER;\nENDSEC;\nDATA;\n' + b' ' * 128 + b'ENDSEC;\nEND-ISO-10303-21;\n'


class FakeAPI:
    def __init__(self, target, partial=False, fail_ctor=False):
        self.target, self.is_partial, self.fail_ctor = target, partial, fail_ctor
        self.calls = []

    def initialize(self, flag):
        self.calls.append(('initialize', flag))

    def document(self, storage, path):
        self.calls.append(('document', path))
        return 0 if self.fail_ctor else storage

    def converter(self, storage):
        self.calls.append(('converter',))
        return storage

    def get_data(self, storage):
        return storage

    def set_type(self, handle, name):
        self.calls.append(('set_type', name))
        return 0

    def convert(self, converter, source, destination):
        self.calls.append(('convert', converter, source, destination))
        self.target.write_bytes(STEP)
        return 123 if self.is_partial else 0

    def success(self, value):
        return True  # A partial result may also satisfy the generic success predicate.

    def partial(self, value):
        return value == 123

    def exception(self):
        return False

    def message(self, value):
        return b'partial result'

    def document_destroy(self, address):
        self.calls.append(('document_destroy',))

    def converter_destroy(self, address):
        self.calls.append(('converter_destroy',))

    def shutdown(self):
        self.calls.append(('shutdown',))


class BoundaryTests(unittest.TestCase):
    def exercise(self, **options):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, target, report = root / 'source.x_b', root / 'converted.step', root / 'result.json'
            source.write_bytes(b'native test double input')
            api = FakeAPI(target, **options)
            status = probe.worker(root, source, target, report, api_factory=lambda directory: api)
            return status, json.loads(report.read_text()), api.calls

    def test_original_construction_precedes_convert_and_cleanup_releases_wrappers(self):
        status, report, calls = self.exercise()
        self.assertEqual(status, 0)
        self.assertTrue(report['native_conversion_succeeded'])
        self.assertFalse(report['geometry_verified'])
        kinds = [c[0] for c in calls]
        self.assertEqual(kinds[:4], ['initialize', 'document', 'document', 'converter'])
        self.assertEqual(kinds[-4:], ['converter_destroy', 'document_destroy', 'document_destroy', 'shutdown'])
        self.assertTrue(calls[0][1])
        self.assertEqual([c[1] for c in calls if c[0] == 'set_type'], [b'PARASOLID', b'STEP'])

    def test_partial_result_is_not_accepted_even_with_step_file(self):
        status, report, calls = self.exercise(partial=True)
        self.assertEqual(status, 1)
        self.assertFalse(report['native_conversion_succeeded'])
        self.assertIn('partial result', report['error'])
        self.assertEqual(calls[-1], ('shutdown',))

    def test_failed_constructor_is_not_used_or_destroyed(self):
        status, report, calls = self.exercise(fail_ctor=True)
        self.assertEqual(status, 1)
        kinds = [c[0] for c in calls]
        self.assertNotIn('convert', kinds)
        self.assertNotIn('document_destroy', kinds)
        self.assertEqual(kinds[-1], 'shutdown')

    def test_incomplete_step_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'a.step'
            path.write_bytes(STEP[:-25])
            self.assertFalse(probe.step_envelope(path))

    def test_storage_guard_detects_overwrite(self):
        wrapper = probe.OpaqueWrapper()
        wrapper.check()
        C.memset(wrapper.address + wrapper.SIZE, 0, 1)
        with self.assertRaisesRegex(RuntimeError, 'guard'):
            wrapper.check()

    def test_real_ard_payload_extraction_stays_correlated(self):
        # A minimal structural fixture, not a vendor model or native conversion.
        payload = b'**PARASOLID FORMAT=binary; synthetic test payload'
        fragment = struct.pack('<III', 3, 0, len(payload)) + payload + struct.pack('<III', 1, 0, 0)
        wrapper = b'\x22\x14\x00LicomUKRJMParaSolid\0' + struct.pack('<III', 1, len(fragment), 0) + fragment
        prefix = b'Licom-APS File V1.94\r\n\x1a\x30' + struct.pack('<I', 0)
        data = prefix + b'\x2e\x02\x00i\x00' + wrapper + b'\x2f' + struct.pack('<I', len(prefix) + 1) + b'\0'
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'sample.ard'
            path.write_bytes(data)
            actual, descriptor = probe.extract_ard(path)
            self.assertEqual(actual, payload)
            self.assertEqual(descriptor['source_ard_sha256'], hashlib.sha256(data).hexdigest())
            self.assertEqual(descriptor['parasolid_sha256'], hashlib.sha256(payload).hexdigest())


if __name__ == '__main__':
    unittest.main()
