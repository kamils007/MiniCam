"""Portable checks for reporting conversion failures and bounded file checks."""
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest

from test_step_conversion import inspect_output, successful_conversion


class ConversionChecks(unittest.TestCase):
    def successful_report(self):
        return {'powershell_exit_code': 0,
                'worker': {'translation_attempted': True, 'translate_return_code': 0},
                'output': {'recognized_parasolid': True}, 'source_unchanged': True,
                'input_copy_unchanged': True, 'server_unchanged': True}

    def test_success_requires_native_result_and_preserved_inputs(self):
        report = self.successful_report()
        self.assertTrue(successful_conversion(report))
        for key in ['source_unchanged', 'input_copy_unchanged', 'server_unchanged']:
            damaged = deepcopy(report)
            damaged[key] = False
            self.assertFalse(successful_conversion(damaged), key)
        for value in [None, False, True, 1, 2, 3, 4, 5]:
            damaged = deepcopy(report)
            damaged['worker']['translate_return_code'] = value
            self.assertFalse(successful_conversion(damaged), value)

    def test_worker_error_timeout_and_missing_output_are_not_success(self):
        for change in [{'timeout': True}, {'error': {'message': 'injected'}},
                       {'powershell_exit_code': 1}, {'output': {'exists': True}}]:
            report = self.successful_report()
            report.update(change)
            self.assertFalse(successful_conversion(report), change)
        for key in ['error', 'cleanup_error']:
            report = self.successful_report()
            report['worker'][key] = 'injected failure'
            self.assertFalse(successful_conversion(report), key)
        self.assertFalse(successful_conversion({}))

    def test_text_and_binary_headers_require_a_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'output.x_t'
            for kind in [b'text', b'binary']:
                target.write_bytes(b'**PARASOLID\nFORMAT=' + kind + b';\n**END_OF_HEADER\nBODY')
                self.assertTrue(inspect_output(target)['recognized_parasolid'])
            target.write_bytes(b'**PARASOLID\nFORMAT=text;\n**END_OF_HEADER\n')
            self.assertFalse(inspect_output(target)['recognized_parasolid'])

    def test_missing_invalid_and_truncated_outputs_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'output.x_t'
            self.assertFalse(inspect_output(target)['exists'])
            for data in [b'', b'ISO-10303-21;', b'**PARASOLID\nFORMAT=text;',
                         b'FORMAT=text;\n**END_OF_HEADER\nBODY']:
                target.write_bytes(data)
                self.assertFalse(inspect_output(target)['recognized_parasolid'])


if __name__ == '__main__':
    unittest.main()
