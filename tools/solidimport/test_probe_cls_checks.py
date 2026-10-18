"""Reject incomplete or misleading CLS session reports, without native calls."""
from pathlib import Path
import tempfile
import unittest

from probe_cls_connection import confirmed_session, verify_modules


def sample_report():
    return {'worker_exit_code': 0, 'acam_processes_before': [], 'acam_processes_after': [],
            'acam_observed_during_worker': False, 'acam_monitor_samples': 3,
            'source_modules_unchanged': True,
            'worker': {'schema': 'cls-managed-worker-v2', 'phase': 'completed',
                       'runtime_contract_checked': True, 'core_solids_verified': False,
                       'step_conversion_attempted': False, 'set_cls_path': True,
                       'observed_session_id': 0, 'requested_product': 2,
                       'release_version_verified': False, 'open_method': 'OpenLicenseConnection',
                       'request_profile': 'installed_cls_defaults', 'check_month': -1, 'check_year': -1,
                       'open_return_code': 1,
                       'open_return_name': 'Ok', 'validation_return_code': 101,
                       'validation_return_name': 'Ok', 'reported_product': 2,
                       'close_attempted': True, 'close_completed': True}}


class CLSChecks(unittest.TestCase):
    def test_incomplete_crashed_or_open_application_reports_are_rejected(self):
        self.assertTrue(confirmed_session(sample_report()))
        for change in [{'worker_exit_code': 1}, {'worker_exit_code': False}, {'timeout': True},
                       {'error': 'failure'}, {'worker': {}}, {'source_modules_unchanged': False},
                       {'acam_processes_before': [{'pid': 1}]}, {'acam_processes_after': [{'pid': 2}]},
                       {'acam_observed_during_worker': True}, {'acam_monitor_samples': 0}]:
            report = sample_report()
            report.update(change)
            self.assertFalse(confirmed_session(report), change)
        self.assertFalse(confirmed_session({}))

    def test_native_enum_results_are_not_treated_as_hresult(self):
        for key, values in [('open_return_code', [0, True, 10, 11, 101, 1011]),
                            ('validation_return_code', [0, 1, True, 1009]),
                            ('open_return_name', ['ExistingConnection', 'Failure']),
                            ('validation_return_name', ['LinkClosed'])]:
            for value in values:
                report = sample_report()
                report['worker'][key] = value
                self.assertFalse(confirmed_session(report), (key, value))

    def test_current_connection_cleanup_is_required(self):
        for change in [{'close_attempted': False}, {'close_completed': False},
                       {'cleanup_error': 'failed'}, {'phase': 'before_close'},
                       {'error': 'original API error'}]:
            report = sample_report()
            report['worker'].update(change)
            self.assertFalse(confirmed_session(report), change)

    def test_runtime_contract_product_and_request_profile_must_match(self):
        for change in [{'runtime_contract_checked': False}, {'set_cls_path': False},
                       {'observed_session_id': None}, {'requested_product': 1},
                       {'reported_product': 1}, {'check_month': 0}, {'check_year': 0}, {'check_month': True},
                       {'open_method': 'OpenVersionedLicenseConnection'},
                       {'request_profile': 'unverified'}, {'release_version_verified': True},
                       {'core_solids_verified': True}, {'step_conversion_attempted': True}]:
            report = sample_report()
            report['worker'].update(change)
            self.assertFalse(confirmed_session(report), change)

    def test_hash_guard_refuses_missing_or_wrong_installed_modules(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(FileNotFoundError):
                verify_modules(root)
            (root / 'LicManaged.dll').write_bytes(b'wrong build')
            with self.assertRaisesRegex(ValueError, 'LicManaged.dll'):
                verify_modules(root)


if __name__ == '__main__':
    unittest.main()
