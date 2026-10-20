from copy import deepcopy
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
for folder in ('alphacam_reference', 'solidimport', 'step_ard'):
    sys.path.insert(0, str(ROOT / folder))
from read_ard_body import digest, extract_sample, validate_read
from build_step_ard_14 import validate_cls, validate_measurement, validate_schema_path
from test_step_ard_14_checks import session, model
from probe_feature_2025 import summarize_stage


def fixture():
    payload = b'**PARASOLID FORMAT=binary; synthetic test payload'
    fragment = struct.pack('<III', 3, 0, len(payload)) + payload + struct.pack('<III', 1, 0, 0)
    wrapper = b'\x22\x14\x00LicomUKRJMParaSolid\0' + struct.pack('<III', 1, len(fragment), 0) + fragment
    return b'Licom-APS File V1.94\r\n\x1a\x30' + struct.pack('<I', 0) + wrapper + b'\x2f\0\0\0\0\0', payload


def proof():
    ready, final = session()
    ready['argument_bridge_selftest'] = final['argument_bridge_selftest'] = True
    m = model()
    files = [{'name': 'fixture.s_t', 'sha256': 'test'}]
    m.update(measurement_only_completed=True, schema_path={
        'ok': True, 'native_readback_matches': True, 'original_schema_callbacks_retained': True, 'files_before': files},
        import_ignored=None)
    stage = {'process_exit_code': 0, 'checkpoints': [], 'result': {
        'model_probe': m, 'input_copy': {'unchanged': True, 'sha256_before': 'hash', 'sha256_after': 'hash'},
        'schema_path_cleanup': {'ok': True, 'previous_path_restored': True,
            'configured_value_unchanged': True, 'files_unchanged': True, 'files_after': files}},
        'summary': {'stop_reason': 'completed_measurement_only', 'bodies_after_count': 1,
            'host_selftest_ok': True, 'host_context_bound': True, 'text_table_initialized': True,
            'import_succeeded_with_bodies': True}}
    return {'cls_ready': ready, 'cls_result': final, 'cls_exit_code': 0,
        'feature': {'original_input_unchanged': True, 'no_acam_process_observed_at_checkpoints': True,
                    'native_exceptions': [], 'trace_read_errors': [], 'stages': [stage]}}


def validate(value):
    return validate_read(value, 'hash', validate_cls, validate_measurement, validate_schema_path)


class ReadArdTests(unittest.TestCase):
    def test_exact_payload_and_changed_source_rejection(self):
        data, expected = fixture()
        payload, report = extract_sample(data, expected_sha256=digest(data))
        self.assertEqual(payload, expected)
        self.assertEqual(report['parasolid_sha256'], digest(expected))
        with self.assertRaises(ValueError):
            extract_sample(data + b'changed', expected_sha256=digest(data))

    def test_truncated_or_ambiguous_envelopes_are_rejected(self):
        data, _ = fixture()
        for n in range(28, len(data)):
            value = data[:n]
            with self.subTest(n=n), self.assertRaises((ValueError, struct.error)):
                extract_sample(value, expected_sha256=digest(value))
        value = data[:-6] + data[28:-6] + data[-6:]
        with self.assertRaises(ValueError):
            extract_sample(value, expected_sha256=digest(value))

    def test_read_has_measurements_but_no_geometry_or_volume_claim(self):
        result = validate(proof())
        self.assertEqual(result['topology_counts']['Faces'], 10)
        self.assertFalse(result['geometry_verified'])
        self.assertFalse(result['dimensions_match_reference'])

    def test_failed_native_stage_cleanup_stale_input_or_save_cannot_pass(self):
        for mutation in ('exit', 'bodies', 'completed', 'save', 'record', 'hash', 'cls', 'faces', 'alignment', 'cleanup'):
            value = deepcopy(proof())
            stage = value['feature']['stages'][0]
            m = stage['result']['model_probe']
            if mutation == 'exit': stage['process_exit_code'] = 80
            if mutation == 'bodies': stage['summary']['bodies_after_count'] = 0
            if mutation == 'completed': m['measurement_only_completed'] = False
            if mutation == 'save': m['save'] = {'ok': True}
            if mutation == 'record': m['object_serialization'] = {'ok': True}
            if mutation == 'hash': stage['result']['input_copy']['sha256_after'] = 'stale'
            if mutation == 'cls': value['cls_result']['close_completed'] = False
            if mutation == 'faces': m['body_measurement']['topology_counts']['Faces'] = 0
            if mutation == 'alignment': m['alignment_configuration'][0]['enabled'] = True
            if mutation == 'cleanup': stage['result']['schema_path_cleanup']['previous_path_restored'] = False
            with self.subTest(mutation=mutation), self.assertRaises(ValueError): validate(value)

    def test_stage_summary_distinguishes_read_from_save(self):
        stage = {'process_exit_code': 0, 'result': {'model_probe': {
            'import': {'ok': True}, 'bodies_after': {'count': 1}, 'measurement_only_completed': True}}}
        self.assertEqual(summarize_stage(stage)['stop_reason'], 'completed_measurement_only')
        del stage['result']['model_probe']['measurement_only_completed']
        self.assertEqual(summarize_stage(stage)['stop_reason'], 'save_not_successful')


if __name__ == '__main__': unittest.main()
