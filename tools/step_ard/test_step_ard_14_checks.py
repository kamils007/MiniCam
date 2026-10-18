"""Reject wrong dimensions, stale CLS state and out-of-scope host projections."""
from copy import deepcopy
import unittest

from build_step_ard_14 import ALIGNMENT, validate_cls, validate_measurement, validate_projection, validate_schema_path


def session():
    ready = dict(schema='feature-cls-session-14-v1', ready=True, open_return_code=1,
                 validation_before=101, validation_after=101, access_code=348, request_count=1,
                 classification_flag=False, special_key_projection=0,
                 supported_special_key_mask=16, full_special_key_verified=False)
    final = dict(ready, close_completed=True, validation_after_feature=101)
    return ready, final


def model():
    return {'alignment_configuration': [dict(property=p, enabled=False, readback_matches=True) for p in ALIGNMENT],
            'body_measurement': {
                'bounds': dict(MinX=-20.0, MaxX=1180.0, MinY=3.0, MaxY=2303.0, MinZ=-5.0, MaxZ=5.0),
                'dimensions_api_units': [1200.0, 2300.0, 10.0],
                'topology_counts': dict(Faces=10, Edges=24, Vertices=16),
                'alignment_disabled': True, 'full_topology_validity_verified': False, 'volume_verified': False}}


class Test14Checks(unittest.TestCase):
    def test_schema_catalog_and_restore_required_before_ard_acceptance(self):
        files = [{'name': 'synthetic.s_t', 'size_bytes': 3, 'sha256': 'abc'}]
        good = {'result': {'model_probe': {'schema_path': {
            'ok': True, 'native_readback_matches': True, 'original_schema_callbacks_retained': True,
            'files_before': files}}, 'schema_path_cleanup': {
            'ok': True, 'previous_path_restored': True, 'configured_value_unchanged': True,
            'files_unchanged': True, 'files_after': files}}}
        self.assertIn('cleanup', validate_schema_path(good))
        for key in ('ok', 'previous_path_restored', 'configured_value_unchanged', 'files_unchanged'):
            bad = deepcopy(good)
            bad['result']['schema_path_cleanup'][key] = False
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_schema_path(bad)
        bad = deepcopy(good)
        bad['result']['schema_path_cleanup']['files_after'][0]['sha256'] = 'changed'
        # deepcopy preserves aliasing in the fixture, so use a distinct snapshot.
        bad['result']['model_probe']['schema_path']['files_before'] = files
        with self.assertRaises(ValueError):
            validate_schema_path(bad)

    def test_new_session_requires_the_argument_bridge_selftest_in_both_reports(self):
        ready, final = session()
        with self.assertRaises(ValueError):
            validate_cls(ready, final, 0, require_argument_bridge=True)
        ready['argument_bridge_selftest'] = True
        with self.assertRaises(ValueError):
            validate_cls(ready, final, 0, require_argument_bridge=True)
        final['argument_bridge_selftest'] = True
        self.assertEqual(validate_cls(ready, final, 0, require_argument_bridge=True), 0)

    def test_current_cls_query_and_cleanup_required(self):
        ready, final = session()
        self.assertEqual(validate_cls(ready, final, 0), 0)
        for target, patch in [('ready', {'request_count': 2}), ('ready', {'access_code': 390}),
                              ('ready', {'classification_flag': 0}), ('ready', {'special_key_projection': 1}),
                              ('ready', {'full_special_key_verified': True}),
                              ('final', {'classification_flag': True, 'special_key_projection': 16}),
                              ('final', {'close_completed': False}), ('final', {'validation_after_feature': 0}),
                              ('final', {'error_type': 'Error'}), ('final', {'lease_expired': True})]:
            a, b = session()
            (a if target == 'ready' else b).update(patch)
            with self.subTest(target=target, patch=patch), self.assertRaises(ValueError):
                validate_cls(a, b, 0)
        with self.assertRaises(ValueError):
            validate_cls(ready, final, 1)

    def test_real_true_classification_is_preserved_and_notoptional_is_not_individual_release(self):
        for code in (201, 202, 207):
            ready, final = session()
            for state in (ready, final):
                state.update(classification_flag=True, special_key_projection=16)
            final.update(release_return_code=code, individual_release_confirmed=code in (201, 207))
            self.assertEqual(validate_cls(ready, final, 0), 16)
            final['individual_release_confirmed'] = not final['individual_release_confirmed']
            with self.assertRaises(ValueError):
                validate_cls(ready, final, 0)

    def test_bounds_match_is_separate_from_geometry_validity(self):
        measurement = validate_measurement(model())
        self.assertTrue(measurement['dimensions_match_reference'])
        self.assertFalse(measurement['geometry_verified'])
        self.assertFalse(measurement['volume_verified'])

    def test_wrong_units_nan_reversed_bounds_and_missing_topology_are_rejected(self):
        changes = [dict(bounds=dict(MinX=0, MaxX=1.2, MinY=0, MaxY=2.3, MinZ=0, MaxZ=0.01),
                        dimensions_api_units=[1.2, 2.3, 0.01]),
                   dict(dimensions_api_units=[1200, 2300, 9]),
                   dict(topology_counts=dict(Faces=1, Edges=1, Vertices=0)),
                   dict(topology_counts=dict(Faces=True, Edges=1, Vertices=1)),
                   dict(volume_verified=True)]
        for patch in changes:
            value = model()
            value['body_measurement'].update(patch)
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                validate_measurement(value)
        for value in (float('nan'), float('inf'), -50.0):
            sample = model()
            sample['body_measurement']['bounds']['MaxX'] = value
            with self.assertRaises(ValueError):
                validate_measurement(sample)

    def test_all_alignment_options_must_be_disabled_and_read_back(self):
        for field, value in [('enabled', True), ('readback_matches', False)]:
            sample = model()
            sample['alignment_configuration'][0][field] = value
            with self.assertRaises(ValueError):
                validate_measurement(sample)
        sample = model()
        sample['alignment_configuration'].pop()
        with self.assertRaises(ValueError):
            validate_measurement(sample)

    def test_only_actual_save_consumer_may_read_the_current_mask(self):
        sample = {'checkpoints': [dict(checkpoint='live_save_projection', mask=16, value=0,
                                      source='current_CLS_GetAccessCode_348'),
                                  dict(checkpoint='host_method', interface='IAlphaCamApp', method='SpecialKey',
                                       host_phase='save', value=0)]}
        validate_projection(sample, 0)
        for patch in [dict(host_phase='import'), dict(host_phase='object_write'), dict(value=1)]:
            changed = deepcopy(sample)
            changed['checkpoints'][1].update(patch)
            with self.assertRaises(ValueError):
                validate_projection(changed, 0)
        with self.assertRaises(ValueError):
            validate_projection({'checkpoints': sample['checkpoints'][:1]}, 0)


if __name__ == '__main__':
    unittest.main()
