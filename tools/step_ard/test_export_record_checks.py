"""GUI/worker boundary tests. Native conversion is explicitly a test double."""
from copy import deepcopy
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

import export_record as worker
from object_serialization import parse_fragment
from parasolid_errors import KERNEL_SHA256
from test_step_ard_checks import native_report
from test_step_ard_14_checks import session


def fixture(directory, source_hash):
    directory.mkdir()
    (directory / 'step_capture').mkdir()
    (directory / 'feature/native').mkdir(parents=True)
    capture = b'**PARASOLID\nFORMAT=text;\n**END_OF_HEADER\nsynthetic fixture'
    (directory / 'step_capture/captured.x_t').write_bytes(capture)
    h = worker.digest(capture)
    path, fmt = b'private.x_t\0', b'Parasolid\0'
    geometry = b'**PARASOLID\nFORMAT=binary;\n**END_OF_HEADER\nsynthetic geometry'
    record = (struct.pack('<III', 3, 1, 1) + struct.pack('<I', len(path)) + path
              + struct.pack('<I', len(fmt)) + fmt + struct.pack('<IBI', 0, 1, 1)
              + struct.pack('<I', len(geometry)) + geometry + struct.pack('<III', 1, 1, 0))
    (directory / 'feature/native/feature_object.bin').write_bytes(record)
    proof = native_report(record)
    proof['input']['sha256'] = h
    stage = proof['stages'][0]
    stage['summary'].update(stop_reason='completed_native_record', save_status={'ok': True},
        host_selftest_ok=True, host_context_bound=True, text_table_initialized=True)
    files = [{'name': 'synthetic.s_t', 'sha256': 'fixture'}]
    stage['result'] = {'input_copy': {'sha256_before': h, 'sha256_after': h, 'unchanged': True},
        'model_probe': {'schema_path': {'ok': True, 'native_readback_matches': True,
            'original_schema_callbacks_retained': True, 'files_before': files}},
        'schema_path_cleanup': {'ok': True, 'previous_path_restored': True,
            'configured_value_unchanged': True, 'files_unchanged': True, 'files_after': files}}
    stage['checkpoints'] = [dict(checkpoint='host_method', interface='IAlphaCamApp',
        method='SpecialKey', host_phase='save', value=0), dict(checkpoint='live_save_projection',
        mask=16, value=0, source='current_CLS_GetAccessCode_348')]
    ready, final = session()
    ready['argument_bridge_selftest'] = final['argument_bridge_selftest'] = True
    kernel = {k: {'query_return_code': 0, 'was_error': False, 'output_guards_intact': True,
                  'error_clear_called': False} for k in ('before_save', 'after_save', 'before_record', 'after_record')}
    kernel.update(api={'module_sha256': KERNEL_SHA256,
                      'iat_export_identity_matches': True}, pskernel_file_unchanged=True)
    report = dict(schema='step-to-ard-integrated-15-v1', source_step_sha256=source_hash,
        ard_created=True, step_repeated_in_this_test=True, fresh_capture_verified=True,
        inputs_and_installation_unchanged=True, single_body_only=True, acam_observed=False,
        capture={'sha256': h}, native_fragment=parse_fragment(record),
        feature_stage={'cls_ready': ready, 'cls_result': final, 'cls_exit_code': 0,
                       'kernel_diagnostics': kernel, 'feature': proof})
    save_report(directory, report)
    return record, report


def save_report(directory, report):
    (directory / 'step_ard_result.json').write_text(json.dumps(report), encoding='utf-8')


class ExportWorkerChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.job = Path(self.temp.name)
        self.step = self.job / 'current-model.step'
        self.step.write_bytes(b'ISO-10303-21;\nsynthetic GUI snapshot')
        self.sha = worker.digest(self.step.read_bytes())
        self.request = self.job / 'request.json'
        self.response = self.job / 'response.json'
        self.data = {'schema': worker.REQUEST_SCHEMA, 'step': str(self.step), 'step_sha256': self.sha,
                     'alphacam_root': 'synthetic install', 'solidlink_server': 'synthetic server'}

    def run_request(self):
        self.request.write_text(json.dumps(self.data), encoding='utf-8')
        with patch.object(worker.sys, 'platform', 'win32'):
            code = worker.run(self.request, self.response)
        return code, json.loads(self.response.read_text())

    def test_success_publishes_only_current_verified_record(self):
        def convert(step, root, server, out):
            self.assertEqual(step, self.step)
            fixture(out, self.sha)
            return 0
        with patch.object(worker, 'convert', side_effect=convert):
            code, response = self.run_request()
        self.assertEqual(code, 0)
        self.assertTrue(response['ok'])
        record = (self.job / 'record.bin').read_bytes()
        self.assertEqual(response['record_sha256'], worker.digest(record))
        self.assertEqual(response['step_sha256'], self.sha)
        self.assertEqual(response['record_size_bytes'], len(record))

    def test_wrong_snapshot_stops_before_native_conversion(self):
        self.data['step_sha256'] = 'f' * 64
        with patch.object(worker, 'convert') as convert:
            code, response = self.run_request()
            convert.assert_not_called()
        self.assertEqual(code, 1)
        self.assertFalse(response['ok'])
        self.assertFalse((self.job / 'record.bin').exists())

    def test_failed_conversion_never_publishes_record(self):
        def convert(step, root, server, out):
            fixture(out, self.sha)
            save_report(out, {'error': {'message': 'synthetic native failure'}})
            return 1
        with patch.object(worker, 'convert', side_effect=convert):
            code, response = self.run_request()
        self.assertEqual(code, 1)
        self.assertIn('synthetic native failure', response['error'])
        self.assertFalse((self.job / 'record.bin').exists())

    def test_completed_report_cannot_hide_changed_capture_record_or_cleanup_failure(self):
        out = self.job / 'conversion'
        record, original = fixture(out, self.sha)
        self.assertEqual(worker.verified_record(out, self.sha), record)
        for key in ('ard_created', 'fresh_capture_verified', 'inputs_and_installation_unchanged'):
            report = deepcopy(original)
            report[key] = False
            save_report(out, report)
            with self.subTest(key=key), self.assertRaises(ValueError):
                worker.verified_record(out, self.sha)
        report = deepcopy(original)
        report['feature_stage']['cls_result']['close_completed'] = False
        save_report(out, report)
        with self.assertRaises(ValueError):
            worker.verified_record(out, self.sha)
        save_report(out, original)
        (out / 'feature/native/feature_object.bin').write_bytes(record + b'changed')
        with self.assertRaises(ValueError):
            worker.verified_record(out, self.sha)
        (out / 'feature/native/feature_object.bin').write_bytes(record)
        with (out / 'step_capture/captured.x_t').open('ab') as stream:
            stream.write(b'changed')
        with self.assertRaises(ValueError):
            worker.verified_record(out, self.sha)

    def test_wrong_schema_never_launches_converter_and_existing_response_is_not_reused(self):
        self.data['schema'] = 'unsupported'
        with patch.object(worker, 'convert') as convert:
            code, response = self.run_request()
            convert.assert_not_called()
        self.assertEqual(code, 1)
        with self.assertRaises(ValueError):
            worker.run(self.request, self.response)


if __name__ == '__main__':
    unittest.main()
