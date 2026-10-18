"""Reject bridge claims without the original native licensing guard evidence."""
import copy
import unittest

from probe_solidimport_bridge import confirmed_importer_bridge


def sample():
    w = {k: True for k in ['source_fingerprints_checked', 'importer_loaded',
        'mfc_entry_import_matches_getter', 'mfc_thread_ready', 'cls_contract_checked',
        'init_started', 'license_getter_called', 'license_request_attempted',
        'license_request_granted', 'cls_close_attempted', 'cls_close_completed',
        'post_license_host_request', 'program_level_is_selected_host_profile']}
    w.update(schema='solidimport-cls-bridge-worker-10-v1', phase='host_boundary',
        init_returned=False, init_succeeded=False, host_boundary='IAlphaCamApp.SpecialKey',
        mfc_thread_id=42, init_thread_id=42, boundary_thread_id=42, sta_hresult=0,
        host_program_level=14, host_profile='acamLevelCORE', cls_open_return_code=1,
        cls_reported_product=2, cls_check_month=-1, cls_check_year=-1,
        cls_validation_before_init=101, cls_validation_after_init=101,
        license_validation_before=101, license_validation_after=101, license_request_count=1,
        special_key_supplied=False, step_conversion_attempted=False, full_host_verified=False,
        release_version_verified=False, individual_release_return_code=202,
        individual_release_return_name='NotOptional', individual_release_confirmed=False)
    return {'worker': w, 'worker_exit_code':80, 'acam_processes_before':[],
            'acam_processes_after':[], 'acam_observed_during_worker':False,
            'acam_monitor_samples':4, 'source_files_unchanged':True}


class ImporterBridgeChecks(unittest.TestCase):
    def test_controlled_boundary_proves_bridge_but_not_full_init(self):
        r=sample()
        self.assertTrue(confirmed_importer_bridge(r))
        self.assertIs(r['worker']['init_returned'], False)
        self.assertIs(r['worker']['init_succeeded'], False)
        self.assertIs(r['worker']['step_conversion_attempted'], False)

    def test_full_init_requires_actual_zero_return(self):
        r=sample()
        r['worker_exit_code']=0
        r['worker'].update(phase='completed',init_returned=True,init_return_code=0,init_succeeded=True)
        self.assertTrue(confirmed_importer_bridge(r))
        for value in [False,1,-1,None]:
            r['worker']['init_return_code']=value
            self.assertFalse(confirmed_importer_bridge(r))

    def test_no_grant_no_post_guard_or_wrong_thread_is_not_a_bridge(self):
        for key,value in [('license_request_granted',False),('license_getter_called',False),
                          ('license_request_count',0),('license_validation_before',0),
                          ('license_validation_after',0),('post_license_host_request',False),
                          ('host_boundary','event_limit'),('mfc_thread_ready',False),
                          ('mfc_entry_import_matches_getter',False),('init_thread_id',43),
                          ('boundary_thread_id',43),('sta_hresult',-2147417850),
                          ('special_key_supplied',True),('host_program_level',3),
                          ('full_host_verified',True),('step_conversion_attempted',True)]:
            r=sample();r['worker'][key]=value
            self.assertFalse(confirmed_importer_bridge(r),(key,value))

    def test_cleanup_and_not_optional_are_distinct(self):
        for patch in [{'cls_close_completed':False}, {'cls_validation_after_init':1},
                      {'cls_cleanup_error_type':'Failure'}, {'individual_release_return_code':203},
                      {'individual_release_confirmed':True}, {'individual_release_return_name':'Ok'}]:
            r=sample();r['worker'].update(patch)
            self.assertFalse(confirmed_importer_bridge(r),patch)
        r=sample();r['worker'].update(individual_release_return_code=201,
            individual_release_return_name='Ok',individual_release_confirmed=True)
        self.assertTrue(confirmed_importer_bridge(r))

    def test_source_changes_timeout_crash_and_open_acam_are_rejected(self):
        for patch in [{'source_files_unchanged':False}, {'timeout':True},
                      {'bootstrap_error':{'error':'compile'}}, {'worker_exit_code':1},
                      {'acam_processes_before':[{'pid':1}]}, {'acam_processes_after':[{'pid':2}]},
                      {'acam_observed_during_worker':True}, {'acam_monitor_samples':0}]:
            r=copy.deepcopy(sample());r.update(patch)
            self.assertFalse(confirmed_importer_bridge(r),patch)


if __name__ == '__main__':
    unittest.main()
