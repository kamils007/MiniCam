"""Evidence checks for the consumed SpecialKey projection, including denials."""
import copy
import unittest

from probe_solidimport_special_key import confirmed_projection, confirmed_special_key_bridge
from test_solidimport_bridge_checks import sample as base_sample


def sample(flag16=False, first=False, second=False):
    r=base_sample()
    w=r['worker']
    answers=[(348,flag16),(347,first)] + ([] if first else [(349,second)])
    queries=[{'access_code':c,'granted':g,'thread_id':42,
              'validation_before':101,'validation_after':101} for c,g in answers]
    releases=[{'access_code':c,'return_code':202,'return_name':'NotOptional',
               'individual_release_confirmed':False} for c,g in answers if g]
    w.update(schema='solidimport-special-key-worker-11-v1',
        host_boundary='IFrame.ReadTextFile',post_special_key_host_request=True,
        special_key_consumer_checked=True,special_key_supplied=True,
        special_key_projection_ready=True,special_key_full_value_verified=False,
        special_key_supported_mask=18,special_key_consumer_mask=18,
        special_key_projection_value=(16 if flag16 else 0)|(2 if first or second else 0),
        special_key_cls_queries=queries,special_key_release_results=releases)
    return r


class SpecialKeyChecks(unittest.TestCase):
    def test_actual_denials_and_grants_both_prove_projection(self):
        for flag16 in (False,True):
            for first in (False,True):
                for second in (False,True):
                    r=sample(flag16,first,second)
                    self.assertTrue(confirmed_projection(r['worker']))
                    self.assertTrue(confirmed_special_key_bridge(r))
                    self.assertFalse(r['worker']['init_succeeded'])

    def test_wrong_or_extra_codes_and_short_circuit_are_rejected(self):
        for change in ('wrong_code','wrong_order','extra','missing_fallback','unneeded_fallback'):
            r=sample();w=r['worker'];q=w['special_key_cls_queries']
            if change=='wrong_code':q[0]['access_code']=354
            if change=='wrong_order':q.reverse()
            if change=='extra':q.append(copy.deepcopy(q[-1]))
            if change=='missing_fallback':q.pop()
            if change=='unneeded_fallback':q[1]['granted']=True
            self.assertFalse(confirmed_special_key_bridge(r),change)

    def test_unsupported_bits_or_invented_value_are_rejected(self):
        for key,value in [('special_key_projection_value',1),('special_key_projection_value',True),
                          ('special_key_supported_mask',26),('special_key_consumer_mask',19),
                          ('special_key_full_value_verified',True),('special_key_supplied',False),
                          ('special_key_consumer_checked',False)]:
            r=sample();r['worker'][key]=value
            self.assertFalse(confirmed_special_key_bridge(r),(key,value))
        for key,value in [('granted',1),('thread_id',43),('validation_before',1),
                          ('validation_after',0),('error_type','Failure')]:
            r=sample();r['worker']['special_key_cls_queries'][0][key]=value
            self.assertFalse(confirmed_special_key_bridge(r),(key,value))

    def test_owned_cleanup_cannot_be_missing_or_falsely_confirmed(self):
        for change in ('missing','wrong_code','false_success','error','unexpected'):
            r=sample(True,True);releases=r['worker']['special_key_release_results']
            if change=='missing':releases.pop()
            if change=='wrong_code':releases[0]['access_code']=390
            if change=='false_success':releases[0]['individual_release_confirmed']=True
            if change=='error':releases[0]['error_type']='Failure'
            if change=='unexpected':releases[0]['return_code']=203
            self.assertFalse(confirmed_special_key_bridge(r),change)
        r=sample(True,True)
        for release in r['worker']['special_key_release_results']:
            release.update(return_code=201,return_name='Ok',individual_release_confirmed=True)
        self.assertTrue(confirmed_special_key_bridge(r))

    def test_real_native_progress_and_process_evidence_are_required(self):
        for key,value in [('post_special_key_host_request',False),
                          ('host_boundary','IAlphaCamApp.SpecialKey'),('boundary_thread_id',43),
                          ('license_request_granted',False),('cls_close_completed',False)]:
            r=sample();r['worker'][key]=value
            self.assertFalse(confirmed_special_key_bridge(r),(key,value))
        for key,value in [('timeout',True),('worker_exit_code',1),
                          ('acam_observed_during_worker',True),('source_files_unchanged',False)]:
            r=sample();r[key]=value
            self.assertFalse(confirmed_special_key_bridge(r),(key,value))
        r=sample();r['worker_exit_code']=0
        r['worker'].update(phase='completed',init_returned=True,init_return_code=0,init_succeeded=True)
        self.assertTrue(confirmed_special_key_bridge(r))
        r['worker']['init_return_code']=False
        self.assertFalse(confirmed_special_key_bridge(r))


if __name__=='__main__':
    unittest.main()
