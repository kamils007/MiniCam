"""Reject misleading feature-check results without calling Windows or CLS."""
import copy
import unittest

from probe_cls_connection import (ACAM_2025_4_SHA256, confirmed_solids_access_request,
                                  solids_access_outcome)
from test_probe_cls_checks import sample_report


def sample_feature_report():
    report = sample_report()
    report['source_acam_unchanged'] = True
    report['worker']['source_acam_sha256'] = ACAM_2025_4_SHA256
    report['worker']['feature'] = {
        'requested_code': 390, 'api_contract_checked': True,
        'checkout_attempted': True, 'checkout_granted': True,
        'release_attempted': True, 'release_completed': True,
        'release_return_code': 201, 'release_return_name': 'Ok',
        'validated_after_request': True, 'validation_after_return_code': 101,
    }
    return report


class SolidsAccessChecks(unittest.TestCase):
    def test_not_optional_does_not_hide_grant_or_claim_successful_release(self):
        report = sample_feature_report()
        report['worker_exit_code'] = 1
        report['worker']['phase'] = 'failed'
        report['worker']['feature'].update(release_completed=False,
                                          release_return_code=202,
                                          release_return_name='NotOptional')
        outcome = solids_access_outcome(report)
        self.assertIs(outcome['grant_reported'], True)
        self.assertIs(outcome['individual_release_not_optional'], True)
        self.assertIs(outcome['individual_release_reported'], False)
        self.assertIs(outcome['cls_close_call_completed'], True)
        self.assertIs(outcome['strict_probe_confirmed'], False)
        report['worker']['feature']['checkout_granted'] = False
        self.assertIs(solids_access_outcome(report)['grant_reported'], False)

    def test_completed_request_and_capacity_release(self):
        report = sample_feature_report()
        self.assertTrue(confirmed_solids_access_request(report))
        report['worker']['feature'].update(release_return_code=207,
                                          release_return_name='CapacityReleasedOk')
        self.assertTrue(confirmed_solids_access_request(report))
        self.assertIs(report['worker']['core_solids_verified'], False)

    def test_denial_wrong_code_and_missing_or_untyped_results(self):
        for key, value in [('checkout_granted', False), ('checkout_granted', 1),
                           ('requested_code', 391), ('requested_code', '390'),
                           ('api_contract_checked', False), ('checkout_attempted', False),
                           ('validated_after_request', False),
                           ('validation_after_return_code', 1)]:
            report = sample_feature_report()
            report['worker']['feature'][key] = value
            self.assertFalse(confirmed_solids_access_request(report), (key, value))
        report = sample_feature_report()
        del report['worker']['feature']
        self.assertFalse(confirmed_solids_access_request(report))

    def test_granted_without_release_or_close_is_not_confirmed(self):
        for patch in [{'release_attempted': False}, {'release_completed': False},
                      {'release_return_code': 206}, {'release_return_code': True},
                      {'release_return_name': 'NoLicensesReleased'},
                      {'release_error': 'failure'}]:
            report = sample_feature_report()
            report['worker']['feature'].update(patch)
            self.assertFalse(confirmed_solids_access_request(report), patch)
        report = sample_feature_report()
        report['worker']['close_completed'] = False
        self.assertFalse(confirmed_solids_access_request(report))

    def test_build_changes_crashes_timeout_or_observed_acam_are_not_confirmed(self):
        original = sample_feature_report()
        for patch in [{'source_acam_unchanged': False}, {'worker_exit_code': 1},
                      {'timeout': True}, {'acam_observed_during_worker': True},
                      {'source_modules_unchanged': False}]:
            report = copy.deepcopy(original)
            report.update(patch)
            self.assertFalse(confirmed_solids_access_request(report), patch)
        report = copy.deepcopy(original)
        report['worker']['source_acam_sha256'] = '0' * 64
        self.assertFalse(confirmed_solids_access_request(report))


if __name__ == '__main__':
    unittest.main()
