"""No successful capture without actual native progress, stable files and cleanup."""
import copy
from pathlib import Path
import tempfile
import unittest

from step_capture_support import SERVER_SHA256,SERVER_CLSID,check_registration,confirmed_capture
from test_text_catalog_checks import sample as base_sample
from test_step_conversion import inspect_output


def sample():
    r=base_sample();w=r['worker'];digest='c'*64
    w.update(schema='solidimport-step-capture-worker-13-v1',phase='parasolid_captured',
        init_returned=True,init_return_code=0,init_succeeded=True,
        step_conversion_attempted=True,step_returned=False,step_mfc_thread_ready=True,
        no_parasolid_before_step=True,capture_boundary_instruction_checked=True,
        parasolid_capture_written=True,worker_input_copy_unchanged=True,
        active_drawing_supplied=False,geometry_verified=False,step_thread_id=42,capture_thread_id=42,
        capture_boundary='IAlphaCamApp.ActiveDrawing',recognized_candidate_count=1,
        step_argument={'argument_type':'const char*','native_unicode_path_matches':True,'native_input_sha256':digest},
        capture={'filename':'captured.x_t','sha256':digest,'size_bytes':200,
                 'recognized_parasolid_header':True,'source_copy_match':True})
    r.update(worker_exit_code=83,source_unchanged=True,input_copy_unchanged=True,server_unchanged=True,
        input_sha256=digest,solidlink_processes_before=[],registration={'verified':True,'sha256':SERVER_SHA256},
        observed_servers=[{'pid':123,'sha256':SERVER_SHA256,'path_matches_registration':True}],
        captured_file={'exists':True,'recognized_parasolid':True,'size_bytes':200,'sha256':digest,'format':'text'})
    return r


class StepCaptureChecks(unittest.TestCase):
    def test_capture_is_separate_from_drawing_geometry_and_step_return(self):
        r=sample();self.assertTrue(confirmed_capture(r))
        self.assertFalse(r['worker']['step_returned'])
        self.assertFalse(r['worker']['active_drawing_supplied'])
        self.assertFalse(r['worker']['geometry_verified'])

    def test_return_without_capture_or_wrong_phase_is_never_success(self):
        for patch in [{'step_returned':True},{'phase':'completed'}, {'phase':'failed'},
                      {'init_return_code':False},{'init_return_code':-1},
                      {'capture_boundary':'IAlphaCamApp.Other'},{'recognized_candidate_count':0},
                      {'recognized_candidate_count':2},{'active_drawing_supplied':True},
                      {'step_thread_id':43},{'capture_thread_id':43},
                      {'no_parasolid_before_step':False},{'worker_input_copy_unchanged':False},
                      {'cls_close_completed':False},{'geometry_verified':True}]:
            r=sample();r['worker'].update(patch)
            self.assertFalse(confirmed_capture(r),patch)

    def test_original_copy_server_and_observed_runtime_must_match(self):
        for patch in [{'source_unchanged':False},{'input_copy_unchanged':False},
                      {'server_unchanged':False},{'solidlink_processes_before':[{'pid':1}]},
                      {'observed_servers':[]},{'timeout':True},{'worker_exit_code':0},
                      {'acam_observed_during_worker':True},{'source_files_unchanged':False}]:
            r=sample();r.update(patch)
            self.assertFalse(confirmed_capture(r),patch)
        for key,value in [('sha256','d'*64),('pid',0),('path_matches_registration',False)]:
            r=sample();r['observed_servers'][0][key]=value
            self.assertFalse(confirmed_capture(r),(key,value))
        r=sample();r['worker']['step_argument']['native_unicode_path_matches']=False
        self.assertFalse(confirmed_capture(r))

    def test_output_hash_header_payload_and_catalog_evidence_are_required(self):
        for patch in [{'sha256':'e'*64},{'recognized_parasolid':False},{'size_bytes':0},
                      {'size_bytes':201},{'exists':False},{'format':None}]:
            r=sample();r['captured_file'].update(patch)
            self.assertFalse(confirmed_capture(r),patch)
        r=sample();r['worker']['text_catalog_reads'][0]['value_utf8_sha256']='e'*64
        self.assertFalse(confirmed_capture(r))
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'capture.x_t'
            p.write_bytes(b'**\nFORMAT=text;\n**END_OF_HEADER\n')
            self.assertFalse(inspect_output(p)['recognized_parasolid'])
            p.write_bytes(p.read_bytes()+b'payload\n')
            self.assertTrue(inspect_output(p)['recognized_parasolid'])

    def test_registration_conflicts_fail_before_activation(self):
        class Registry:
            errors=[]
            def value(self,key):return '{00000000-0000-0000-0000-000000000001}'
        self.assertRaises(ValueError,check_registration,Registry(),Path('SolidLink.exe'),{})


if __name__=='__main__':
    unittest.main()
