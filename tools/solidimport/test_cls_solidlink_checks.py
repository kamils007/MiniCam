"""False-success guards for CLS plus SolidLink, without native execution."""
from copy import deepcopy
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from test_cls_solidlink_conversion import (CLSID, PROGID, TARGET_PROGID, checked_registration,
                                         checked_current_registration,
                                         cls_session_confirmed, conversion_confirmed)
from test_probe_cls_checks import sample_report


def report_with_translation():
    r = sample_report()
    r.update({'solidlink_processes_before': [], 'source_unchanged': True,
              'input_copy_unchanged': True, 'server_unchanged': True,
              'output_absent_before': True, 'output': {'recognized_parasolid': True}})
    r['worker'].update({'schema': 'cls-solidlink-worker-v1',
                        'step_conversion_attempted': True,
                        'cls_validated_before_translation': True,
                        'cls_validated_after_translation': True,
                        'validation_after_return_code': 101,
                        'validation_after_return_name': 'Ok', 'reported_product_after': 2,
                        'solidlink_api_contract_verified': True,
                        'solidlink_file_major': 2025, 'solidlink_file_minor': 4,
                        'conversion': {'activated': True, 'translation_attempted': True,
                                       'application_argument': 'Alphacam',
                                       'translate_return_code': 0, 'interop_return_code': 0,
                                       'output_exists': True, 'com_release_completed': True}})
    return r


class CLSSolidLinkChecks(unittest.TestCase):
    def test_both_stages_have_separate_results(self):
        good = report_with_translation()
        self.assertTrue(cls_session_confirmed(good))
        self.assertTrue(conversion_confirmed(good))
        # Reproduce the earlier direct SolidLink result: native return 1,
        # no output, and GetInteropResult=0. An open CLS session is insufficient.
        failed = deepcopy(good)
        failed['worker']['conversion'].update({'translate_return_code': 1, 'output_exists': False})
        failed['output'] = {'exists': False, 'recognized_parasolid': False}
        self.assertTrue(cls_session_confirmed(failed))
        self.assertFalse(conversion_confirmed(failed))

    def test_session_must_validate_before_and_after_translation(self):
        for change in [{'open_return_code': 0}, {'open_return_code': True},
                       {'open_return_code': 10}, {'open_return_code': 11},
                       {'validation_return_code': 0}, {'validation_after_return_code': 1},
                       {'validation_after_return_code': True},
                       {'validation_after_return_name': 'LinkClosed'},
                       {'reported_product_after': 1}, {'cls_validated_before_translation': False},
                       {'cls_validated_after_translation': False}, {'close_completed': False},
                       {'error': {'message': 'CLS error'}}]:
            r = report_with_translation()
            r['worker'].update(change)
            self.assertFalse(cls_session_confirmed(r), change)
            self.assertFalse(conversion_confirmed(r), change)

    def test_timeout_crash_application_and_reused_server_reject_success(self):
        for change in [{'timeout': True}, {'worker_exit_code': 1}, {'worker_exit_code': False},
                       {'acam_processes_before': [{'pid': 1}]}, {'acam_processes_after': [{'pid': 1}]},
                       {'acam_observed_during_worker': True}, {'acam_monitor_samples': 0},
                       {'solidlink_processes_before': [{'pid': 2}]},
                       {'source_modules_unchanged': False}, {'verification_error': 'cannot read'},
                       {'worker_report_error': 'invalid JSON'}]:
            r = report_with_translation()
            r.update(change)
            self.assertFalse(cls_session_confirmed(r), change)
            self.assertFalse(conversion_confirmed(r), change)

    def test_translation_needs_real_zero_output_and_com_cleanup(self):
        for change in [{'translate_return_code': True}, {'translate_return_code': 1},
                       {'activated': False}, {'translation_attempted': False},
                       {'output_exists': False}, {'interop_return_code': None},
                       {'com_release_completed': False}, {'error': {'message': 'COM error'}},
                       {'cleanup_error': 'release failed'}]:
            r = report_with_translation()
            r['worker']['conversion'].update(change)
            self.assertTrue(cls_session_confirmed(r), change)
            self.assertFalse(conversion_confirmed(r), change)

    def test_file_integrity_and_new_output_are_required(self):
        for change in [{'output': {}}, {'output': {'recognized_parasolid': False}},
                       {'output_absent_before': False}, {'source_unchanged': False},
                       {'input_copy_unchanged': False}, {'server_unchanged': False}]:
            r = report_with_translation()
            r.update(change)
            self.assertFalse(conversion_confirmed(r), change)
        self.assertFalse(cls_session_confirmed({}))
        self.assertFalse(conversion_confirmed({}))

    def test_current_public_contract_is_required_for_translation(self):
        for change in [{'solidlink_api_contract_verified': False},
                       {'solidlink_file_major': 2023}, {'solidlink_file_minor': 3}]:
            r = report_with_translation()
            r['worker'].update(change)
            self.assertFalse(conversion_confirmed(r), change)


class FakeRegistry:
    def __init__(self, values, errors=()):
        self.values = dict(values)
        self.errors = list(errors)

    def value(self, key):
        return self.values.get(key)


class RegistrationChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.server = Path(self.temp.name) / 'SolidLink.exe'
        self.server.write_bytes(b'known fixture build')
        self.fingerprint = hashlib.sha256(self.server.read_bytes()).hexdigest()
        self.hash_patch = patch('test_cls_solidlink_conversion.SERVER_SHA256', self.fingerprint)
        self.hash_patch.start()
        self.addCleanup(self.hash_patch.stop)
        self.values = {PROGID + r'\CLSID': CLSID,
                       'CLSID\\' + CLSID + r'\LocalServer32': '"' + str(self.server) + '"'}

    def test_known_class_works_without_optional_progid_alias(self):
        del self.values[PROGID + r'\CLSID']
        registry = FakeRegistry(self.values)
        record = {}
        self.assertEqual(checked_registration(registry, record), self.server)
        self.assertEqual(record['activation_route'], 'known_clsid_without_progid_alias')
        self.assertIsNone(record['progid_clsid'])
        self.assertTrue(record['server_hash_matched'])
        self.assertEqual(registry.values, self.values)

    def test_guid_formatting_is_normalized_without_changing_registration(self):
        self.values[PROGID + r'\CLSID'] = '  ' + CLSID.strip('{}').lower() + '  '
        record = {}
        self.assertEqual(checked_registration(FakeRegistry(self.values), record), self.server)
        self.assertEqual(record['activation_route'], 'verified_progid_and_clsid')

    def test_conflicting_alias_is_reported_and_refused(self):
        self.values[PROGID + r'\CLSID'] = '{11111111-1111-1111-1111-111111111111}'
        record = {}
        with self.assertRaisesRegex(RuntimeError, 'inna lub niepoprawna'):
            checked_registration(FakeRegistry(self.values), record)
        self.assertEqual(record['progid_clsid'], self.values[PROGID + r'\CLSID'])
        self.assertNotIn('server_hash_matched', record)

    def test_absent_server_wrong_build_and_inproc_registration_are_refused(self):
        for change in ['missing_server', 'other_build', 'unexpected_inproc']:
            with self.subTest(change=change):
                values = dict(self.values)
                if change == 'missing_server':
                    del values['CLSID\\' + CLSID + r'\LocalServer32']
                elif change == 'other_build':
                    self.server.write_bytes(b'different build')
                else:
                    self.server.write_bytes(b'known fixture build')
                    values['CLSID\\' + CLSID + r'\InprocServer32'] = 'unexpected.dll'
                record = {}
                with self.assertRaises(RuntimeError):
                    checked_registration(FakeRegistry(values), record)
                self.assertIn('local_server_command', record)
                self.assertNotIn('server_hash_matched', record)

    def test_registry_access_error_is_retained_and_not_treated_as_absence(self):
        record = {}
        errors = [{'key': PROGID, 'error': 'access denied'}]
        with self.assertRaisesRegex(RuntimeError, 'Blad odczytu'):
            checked_registration(FakeRegistry(self.values, errors), record)
        self.assertEqual(record['registry_errors'], errors)


class CurrentInstallationChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.server = Path(self.temp.name) / 'SolidLink.exe'
        self.server.write_bytes(b'new 2025.4 public automation fixture')
        self.new_clsid = '{22222222-2222-2222-2222-222222222222}'
        self.values = {TARGET_PROGID + r'\CLSID': self.new_clsid,
                       'CLSID\\' + self.new_clsid + r'\LocalServer32': '"' + str(self.server) + '"'}

    def test_new_registered_clsid_and_user_selected_exe_are_fingerprinted(self):
        record = {}
        server, clsid, fingerprint = checked_current_registration(FakeRegistry(self.values), record, self.server)
        self.assertEqual(server, self.server)
        self.assertEqual(clsid, self.new_clsid)
        self.assertEqual(fingerprint, hashlib.sha256(self.server.read_bytes()).hexdigest())
        self.assertTrue(record['server_path_matched'])

    def test_registration_cannot_redirect_activation_to_another_exe(self):
        other = self.server.with_name('Other.exe')
        other.write_bytes(b'other program')
        self.values['CLSID\\' + self.new_clsid + r'\LocalServer32'] = '"' + str(other) + '"'
        record = {}
        with self.assertRaisesRegex(RuntimeError, 'nie wskazuje podanego pliku'):
            checked_current_registration(FakeRegistry(self.values), record, self.server)
        self.assertNotIn('baseline_sha256', record)

    def test_missing_or_invalid_current_alias_is_not_replaced_with_old_version(self):
        for value in [None, 'bad-guid', '{00000000-0000-0000-0000-000000000000}']:
            self.values[TARGET_PROGID + r'\CLSID'] = value
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                checked_current_registration(FakeRegistry(self.values), {}, self.server)

    def test_inproc_registration_and_access_errors_prevent_activation(self):
        self.values['CLSID\\' + self.new_clsid + r'\InprocServer32'] = 'unexpected.dll'
        with self.assertRaisesRegex(RuntimeError, 'InprocServer32'):
            checked_current_registration(FakeRegistry(self.values), {}, self.server)
        with self.assertRaisesRegex(RuntimeError, 'Blad odczytu'):
            checked_current_registration(FakeRegistry(self.values, [{'error': 'access denied'}]), {}, self.server)


if __name__ == '__main__':
    unittest.main()
