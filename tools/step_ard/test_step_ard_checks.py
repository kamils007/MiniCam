"""Portable guards for assembling only a verified native Feature record."""
from copy import deepcopy
import hashlib
import struct
import unittest
from unittest.mock import patch

import build_step_ard as build


def fragment():
    geometry = b'**PARASOLID\nFORMAT=binary;\n**END_OF_HEADER\nBODY'
    return struct.pack('<III', 3, 0, len(geometry)) + geometry + struct.pack('<III', 1, 1, 0)


def native_report(record):
    return {'input': {'sha256': 'inputhash'}, 'original_input_unchanged': True,
            'no_acam_process_observed_at_checkpoints': True, 'native_exceptions': [],
            'stages': [{'process_exit_code': 0, 'summary': {
                'stop_reason': 'completed', 'native_error': None,
                'bodies_after_count': 1, 'saved_body_count': 1,
                'import_succeeded_with_bodies': True, 'save_succeeded_with_file': True,
                'object_record_written_and_prefix_recognized': True,
                'object_serialization': {'object_name': 'LicomUKRJMParaSolid',
                    'query_return_code': 1, 'write_return_code': 0,
                    'flush_return_code': 0, 'close_return_code': 0,
                    'output': {'sha256': hashlib.sha256(record).hexdigest(), 'size_bytes': len(record)}}}}]}


class StepArdChecks(unittest.TestCase):
    def test_missing_host_method_is_reported_before_record_access(self):
        report = {'stages': [{'process_exit_code': 80, 'summary': {
            'stop_reason': 'unimplemented_host_method',
            'first_unimplemented_host_method': {'interface': 'IAlphaCamApp',
                'method': 'DisableUndo', 'invkind': 2, 'vtable_offset_x64': '0x2b8'}}}]}
        with self.assertRaisesRegex(RuntimeError, 'IAlphaCamApp.DisableUndo'):
            build.check_feature_stage(report)

    def test_native_failures_do_not_produce_a_verified_record(self):
        record = fragment()
        self.assertTrue(build.require_native_success(native_report(record), record, 'inputhash'))
        for key in ['original_input_unchanged', 'no_acam_process_observed_at_checkpoints']:
            report = native_report(record)
            report[key] = False
            with self.assertRaises(ValueError):
                build.require_native_success(report, record, 'inputhash')
        for change in [{'saved_body_count': 0}, {'bodies_after_count': 2},
                       {'stop_reason': 'child_timeout'}, {'native_error': 'fault'},
                       {'object_record_written_and_prefix_recognized': False}]:
            report = native_report(record)
            report['stages'][0]['summary'].update(change)
            with self.assertRaises(ValueError):
                build.require_native_success(report, record, 'inputhash')
        for report in [{}, {'stages': []}]:
            with self.assertRaises(ValueError):
                build.require_native_success(report, record, 'inputhash')

    def test_record_changes_wrong_input_and_native_faults_are_rejected(self):
        record = fragment()
        for changed_record, expected in [(record + b'changed', 'inputhash'), (record, 'differentinput')]:
            with self.assertRaises(ValueError):
                build.require_native_success(native_report(record), changed_record, expected)
        report = native_report(record)
        report['native_exceptions'] = [{'event': 'native_access_violation'}]
        with self.assertRaises(ValueError):
            build.require_native_success(report, record, 'inputhash')

    def test_wrapper_preserves_existing_records_footer_and_entire_native_record(self):
        prefix = b'opaque drawing and preview records' + b'\x2e\x04\x00id\x00'
        pointer = prefix.index(b'\x2e') + 1
        base = prefix + b'\x2f' + struct.pack('<I', pointer) + b'\0'
        with patch.object(build, 'BASE_SHA256', hashlib.sha256(base).hexdigest()):
            data, info = build.wrap_ard(base, fragment())
            self.assertEqual(data[:len(prefix)], prefix)
            self.assertEqual(data[-6:], base[-6:])
            self.assertEqual(data[info['fragment_offset']:-6], fragment())
            self.assertEqual(len(data), len(base) + 35 + len(fragment()))
            with self.assertRaises(ValueError):
                build.wrap_ard(base[:-1] + b'x', fragment())
            with self.assertRaises(ValueError):
                build.wrap_ard(base, b'truncated native record')


if __name__ == '__main__':
    unittest.main()
