"""Check false success rejection and intermediate capture on portable Python."""
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest

from test_solidimport_init import CAPTURE_EXIT, capture_at_boundary, captured_result

HEADER = b'**PARASOLID\nFORMAT=text;\n**END_OF_HEADER\nBODY'


class InitCaptureChecks(unittest.TestCase):
    def report(self):
        return {'worker_exit_code': CAPTURE_EXIT, 'init_return_code': 0,
                'capture': {'boundary': 'IAlphaCamApp.ActiveDrawing', 'phase': 'step',
                            'output': {'sha256': 'abc'}},
                'output': {'recognized_parasolid': True, 'sha256': 'abc'},
                **{k: True for k in ['source_unchanged', 'input_copy_unchanged',
                                     'server_unchanged', 'dll_unchanged']}}

    def test_partial_native_runs_are_not_success(self):
        self.assertTrue(captured_result(self.report()))
        for change in [{'worker_exit_code': 0}, {'worker_exit_code': 80},
                       {'init_return_code': -1}, {'init_return_code': False},
                       {'timeout': True}, {'error': 'failure'}, {'capture': {}},
                       {'output': {'recognized_parasolid': False}}]:
            value = self.report()
            value.update(change)
            self.assertFalse(captured_result(value), change)
        for key in ['source_unchanged', 'input_copy_unchanged', 'server_unchanged', 'dll_unchanged']:
            value = self.report()
            value[key] = False
            self.assertFalse(captured_result(value), key)
        value = self.report()
        value['capture']['output']['sha256'] = 'different'
        self.assertFalse(captured_result(value))
        self.assertFalse(captured_result({}))

    def test_only_one_complete_intermediate_is_captured(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            temp = out / 'temp'
            temp.mkdir()
            source = temp / 'step.x_t'
            source.write_bytes(HEADER)
            info = capture_at_boundary(temp, out)
            self.assertTrue(info['recognized_parasolid'])
            self.assertEqual((out / info['filename']).read_bytes(), HEADER)
            self.assertEqual(source.read_bytes(), HEADER)
            with self.assertRaises(FileExistsError):
                capture_at_boundary(temp, out)

    def test_empty_truncated_and_ambiguous_files_do_not_get_captured(self):
        for payloads in [[], [b'**PARASOLID\nFORMAT=text;'], [HEADER, HEADER]]:
            with tempfile.TemporaryDirectory() as folder:
                out = Path(folder)
                temp = out / 'temp'
                temp.mkdir()
                for index, data in enumerate(payloads):
                    (temp / ('%d.x_t' % index)).write_bytes(data)
                with self.assertRaises(RuntimeError):
                    capture_at_boundary(temp, out)
                self.assertFalse((out / 'converted.x_t').exists())


if __name__ == '__main__':
    unittest.main()
