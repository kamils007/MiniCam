"""Guard the scope and success criteria for the native adapter diagnostic."""
import copy
import unittest

from probe_cls_connection import confirmed_license_adapter, confirmed_solids_access_request
from test_cls_solids_checks import sample_feature_report


def adapter_report():
    r = sample_feature_report()
    r['worker']['feature'].update(release_completed=False, release_not_optional=True,
                                  release_return_code=202, release_return_name='NotOptional')
    r['worker']['license_adapter'] = {
        'native_roundtrip_verified': True, 'interface_identity_matches': True,
        'owned_com_references_released': True, 'checkout_attempted': True,
        'checkout_granted': True, 'query_interface_hresult': 0,
        'core_hresult': 0, 'core_variant_bool': -1, 'access_code': 390,
        'request_count': 1, 'validation_before': 101, 'validation_after': 101,
        'unsupported_hresult': -2147467263, 'unsupported_variant_bool': 0,
        'null_name_hresult': -2147024809, 'null_name_variant_bool': 0,
        'core_output_width_verified': True, 'unsupported_output_width_verified': True,
        'null_name_output_width_verified': True, 'unsupported_did_not_request_cls': True,
    }
    return r


class AdapterChecks(unittest.TestCase):
    def test_nonoptional_is_distinct_from_individual_release(self):
        r = adapter_report()
        self.assertTrue(confirmed_license_adapter(r))
        self.assertFalse(confirmed_solids_access_request(r))
        self.assertIs(r['worker']['feature']['release_completed'], False)
        self.assertIs(r['worker']['core_solids_verified'], False)
        self.assertIs(r['worker']['step_conversion_attempted'], False)

    def test_actual_release_is_also_allowed(self):
        for code, name in [(201, 'Ok'), (207, 'CapacityReleasedOk')]:
            r = adapter_report()
            r['worker']['feature'].update(release_completed=True, release_not_optional=False,
                                          release_return_code=code, release_return_name=name)
            self.assertTrue(confirmed_license_adapter(r))

    def test_native_abi_denial_and_upstream_failures_are_not_confirmed(self):
        for key, value in [('native_roundtrip_verified', False), ('query_interface_hresult', 1),
                           ('core_hresult', -1), ('core_variant_bool', 1),
                           ('checkout_granted', False), ('checkout_granted', 1),
                           ('request_count', 2), ('access_code', 391),
                           ('validation_before', 0), ('validation_after', 0),
                           ('core_output_width_verified', False),
                           ('unsupported_hresult', 0), ('unsupported_variant_bool', -1),
                           ('null_name_hresult', 0), ('null_name_output_width_verified', False),
                           ('owned_com_references_released', False),
                           ('upstream_error_type', 'System.Exception')]:
            r = adapter_report()
            r['worker']['license_adapter'][key] = value
            self.assertFalse(confirmed_license_adapter(r), (key, value))

    def test_session_and_individual_cleanup_failures_are_not_hidden(self):
        for patch in [{'release_return_code': 203}, {'release_return_name': 'Other'},
                      {'release_attempted': False}, {'release_not_optional': False},
                      {'release_error': 'failure'}]:
            r = adapter_report()
            r['worker']['feature'].update(patch)
            self.assertFalse(confirmed_license_adapter(r), patch)
        r = adapter_report()
        r['worker']['close_completed'] = False
        self.assertFalse(confirmed_license_adapter(r))

    def test_no_claim_after_crash_timeout_or_observed_acam(self):
        for patch in [{'worker_exit_code': 1}, {'timeout': True},
                      {'acam_observed_during_worker': True}, {'source_acam_unchanged': False},
                      {'source_modules_unchanged': False}]:
            r = copy.deepcopy(adapter_report())
            r.update(patch)
            self.assertFalse(confirmed_license_adapter(r), patch)


if __name__ == '__main__':
    unittest.main()
