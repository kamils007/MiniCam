"""Portable SLInterop/CLS boundary tests; no vendor library is executed here."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import solidlink_bulk_worker as native
import bulk_conversion_probe as parent

STEP = b'ISO-10303-21;\nHEADER;\nENDSEC;\nDATA;\n' + b' ' * 128 + b'ENDSEC;\nEND-ISO-10303-21;'


def ready():
    return {'schema': 'feature-cls-session-14-v1', 'ready': True, 'open_return_code': 1,
            'argument_bridge_selftest': True, 'validation_before': 101, 'validation_after': 101}


class FakeSolidLink:
    def __init__(self, raw=0, returned=True):
        self.raw, self.returned, self.calls = raw, returned, []
        self.mfc_reference = {'original_imports_verified': True}

    def prepare(self): self.calls.append('original_constructor')
    def close(self): self.calls.append('original_destructor')
    def load_format_libraries(self):
        self.calls.append('load_formats')
        return {'modules': {name: {'loaded': True} for name in ('SPAXParasolid.dll', 'SPAXStep.dll')}}

    def run_standard(self, source, destination, log):
        self.calls.append('RunInteropStandard')
        destination.write_bytes(STEP)
        return self.returned, self.raw

    def success(self, raw): return raw == 0
    def partial(self, raw): return False
    def message(self, raw): return b'Licence Not Available' if raw else b'Success'


class SolidLinkTests(unittest.TestCase):
    def exercise(self, api=None, connection=None):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, target, result, connection_file = (root / n for n in
                ('source.x_b', 'converted.step', 'result.json', 'cls_ready.json'))
            source.write_bytes(b'input test double')
            connection_file.write_text(json.dumps(ready() if connection is None else connection))
            api = api or FakeSolidLink()
            factory = Mock(return_value=api)
            code = native.worker(root, source, target, result, connection_file, api_factory=factory)
            return code, json.loads(result.read_text()), api, factory

    def test_whole_entry_and_original_lifetime_are_used(self):
        code, report, api, _ = self.exercise()
        self.assertEqual(code, 0)
        self.assertEqual(api.calls, ['original_constructor', 'load_formats', 'RunInteropStandard', 'original_destructor'])
        self.assertEqual(report['conversion_entry'], 'original_SLInterop_RunInteropStandard')
        self.assertTrue(report['whole_body_conversion'])
        self.assertFalse(report['acam_used'])
        self.assertFalse(report['geometry_verified'])

    def test_native_license_refusal_is_preserved_even_if_file_exists(self):
        code, report, api, _ = self.exercise(api=FakeSolidLink(raw=0x0100000c))
        self.assertEqual(code, 1)
        self.assertFalse(report['native_conversion_succeeded'])
        self.assertIn('Licence Not Available', report['error'])
        self.assertEqual(report['result']['raw'], 0x0100000c)
        self.assertEqual(api.calls[-1], 'original_destructor')

    def test_false_outer_return_is_not_overridden_by_successful_inner_code(self):
        code, report, _, _ = self.exercise(api=FakeSolidLink(returned=False))
        self.assertEqual(code, 1)
        self.assertFalse(report['native_conversion_succeeded'])

    def test_invalid_cls_stops_before_vendor_code(self):
        connection = ready()
        connection['validation_after'] = 0
        code, report, _, factory = self.exercise(connection=connection)
        self.assertEqual(code, 1)
        factory.assert_not_called()
        self.assertIn('CLS', report['error'])

    def test_current_session_and_normal_cleanup_are_both_required(self):
        final = dict(ready(), close_completed=True, validation_after_feature=101,
                     classification_flag=True, release_return_code=201)
        native.validate_finished(final, 0)
        for key, value in [('close_completed', False), ('validation_after_feature', 0),
                           ('release_return_code', 999), ('close_error', True)]:
            broken = dict(final, **{key: value})
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                native.validate_finished(broken, 0)

    def test_license_log_records_product_names_without_copying_raw_options(self):
        with tempfile.TemporaryDirectory() as temporary:
            log = Path(temporary) / 'translator.log'
            log.write_text('Error : Product "STEP_W" is not authorized.\nAuthorization: PRIVATE_VALUE')
            summary = parent.summarize_log(log)
            self.assertEqual(summary['unauthorized_products'], ['STEP_W'])
            self.assertNotIn('PRIVATE_VALUE', json.dumps(summary))

    def test_parent_holds_cls_until_worker_finishes_and_signals_normal_close(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary)
            observed = []
            final = dict(ready(), close_completed=True, validation_after_feature=101)
            class Process:
                returncode = None
                def __init__(self, helper): self.helper = helper
                def poll(self):
                    if self.helper: return self.returncode
                    self.returncode = 0
                    observed.append('child_ended')
                    return 0
                def wait(self, timeout):
                    if self.helper:
                        self_test.assertTrue((out / 'feature_done.signal').is_file())
                        observed.append('CLS_close')
                        (out / 'cls_result.json').write_text(json.dumps(final))
                    self.returncode = 0
            self_test = self
            def start(command, **kwargs):
                helper = '-File' in command
                if helper:
                    (out / 'cls_ready.json').write_text(json.dumps(ready()))
                return Process(helper)
            report = {}
            with patch.dict(parent.os.environ, {'SystemRoot': str(out)}), \
                    patch.object(parent.subprocess, 'Popen', side_effect=start):
                parent.run_with_cls(['python', 'worker'], out, out, report, 90)
            self.assertLess(observed.index('child_ended'), observed.index('CLS_close'))
            self.assertEqual(report['worker_exit_code'], 0)
            native.validate_finished(report['cls_result'], report['cls_exit_code'])


if __name__ == '__main__':
    unittest.main()
