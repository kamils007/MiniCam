"""Fresh input linkage and failure propagation; all native calls are test doubles."""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import build_step_ard_15 as pipeline
import build_step_ard as wrapper_module
from test_step_ard_14_checks import model, session
from test_step_ard_checks import native_report

SHA = lambda data: hashlib.sha256(data).hexdigest()
STEP = b'ISO-10303-21;\nsynthetic STEP test fixture\n'
CAPTURE = b'**PARASOLID\nFORMAT=text;\n**END_OF_HEADER\nsynthetic payload\n'
GEOMETRY = b'**PARASOLID\nFORMAT=binary;\n**END_OF_HEADER\nsynthetic payload\n'


def write_json(path, value):
    Path(path).write_text(json.dumps(value), encoding='utf-8')


def capture_fixture(directory, step_sha=SHA(STEP)):
    directory = Path(directory)
    directory.mkdir()
    (directory / 'input').mkdir()
    (directory / 'input/model.stp').write_bytes(STEP)
    (directory / 'captured.x_t').write_bytes(CAPTURE)
    actual = pipeline.inspect_output(directory / 'captured.x_t')
    report = {'input_sha256': step_sha, 'parasolid_capture_verified': True,
              'initializer_succeeded': True, 'step_conversion_attempted': True,
              'worker_exit_code': 83, 'acam_monitor_samples': 10,
              'worker': {'capture': {'filename': 'captured.x_t'}},
              'catalog': {}, 'captured_file': actual}
    write_json(directory / 'solidimport_step_capture_result.json', report)
    write_json(directory / 'worker_result.json', report['worker'])
    write_json(directory / 'catalog_report.json', report['catalog'])
    return report


class PipelineChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_fresh_capture_requires_recomputed_native_proof_and_matching_actual_bytes(self):
        folder = self.root / 'capture'
        report = capture_fixture(folder)
        with patch.object(pipeline, 'confirmed_capture', return_value=True):
            self.assertEqual(pipeline.validate_fresh_capture(folder, SHA(STEP), 0)[2]['sha256'], SHA(CAPTURE))
            with self.assertRaises(ValueError):
                pipeline.validate_fresh_capture(folder, 'f'*64, 0)
            with self.assertRaises(ValueError):
                pipeline.validate_fresh_capture(folder, SHA(STEP), 1)
            (folder / 'captured.x_t').write_bytes(CAPTURE + b'changed')
            with self.assertRaises(ValueError):
                pipeline.validate_fresh_capture(folder, SHA(STEP), 0)
        (folder / 'captured.x_t').write_bytes(CAPTURE)
        with patch.object(pipeline, 'confirmed_capture', return_value=False), self.assertRaises(ValueError):
            pipeline.validate_fresh_capture(folder, SHA(STEP), 0)
        write_json(folder / 'worker_result.json', {'different': True})
        with patch.object(pipeline, 'confirmed_capture', return_value=True), self.assertRaises(ValueError):
            pipeline.validate_fresh_capture(folder, SHA(STEP), 0)

    def test_capture_metadata_cannot_escape_expected_output_filename(self):
        folder = self.root / 'capture'
        report = capture_fixture(folder)
        report['worker']['capture']['filename'] = '../old.x_t'
        write_json(folder / 'solidimport_step_capture_result.json', report)
        write_json(folder / 'worker_result.json', report['worker'])
        with patch.object(pipeline, 'confirmed_capture', return_value=True), self.assertRaises(ValueError):
            pipeline.validate_fresh_capture(folder, SHA(STEP), 0)

    def test_feature_child_must_read_exact_fresh_copy_and_leave_it_unchanged(self):
        source = self.root / 'fresh.x_t'
        source.write_bytes(CAPTURE)
        proof = {'result': {'input_copy': {'unchanged': True, 'sha256_before': SHA(CAPTURE),
                  'sha256_after': SHA(CAPTURE), 'path': str(source.resolve())}}}
        pipeline.validate_feature_input(proof, source, SHA(CAPTURE))
        for key, value in [('sha256_before', 'f'*64), ('sha256_after', 'f'*64),
                           ('unchanged', False), ('path', 'old capture')]:
            bad = deepcopy(proof)
            bad['result']['input_copy'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                pipeline.validate_feature_input(bad, source, SHA(CAPTURE))

    def test_kernel_error_must_not_be_hidden_by_successful_writer(self):
        good = {'kernel_diagnostics': {'api': {'module_sha256': pipeline.KERNEL_SHA256,
                    'iat_export_identity_matches': True}, 'pskernel_file_unchanged': True}}
        for key in ('before_save', 'after_save', 'before_record', 'after_record'):
            good['kernel_diagnostics'][key] = {'query_return_code': 0, 'was_error': False,
                                              'output_guards_intact': True, 'error_clear_called': False}
        pipeline.validate_kernel(good)
        for key, value in [('query_return_code', 1), ('was_error', True),
                           ('output_guards_intact', False), ('error_clear_called', True)]:
            bad = deepcopy(good)
            bad['kernel_diagnostics']['after_save'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                pipeline.validate_kernel(bad)

    def test_other_step_dimensions_are_measured_without_claiming_reference_agreement(self):
        result = pipeline.validate_measurement(model(), None)
        self.assertFalse(result['dimensions_match_reference'])
        self.assertIsNone(result['reference_dimensions_mm'])
        self.assertFalse(result['geometry_verified'])
        with self.assertRaises(ValueError):
            pipeline.validate_measurement(model(), (1, 2, 3))

    def test_zip_excludes_vendor_modules_private_temp_and_unconfirmed_ard(self):
        out = self.root / 'result'
        out.mkdir()
        for name in ('step_ard_result.json', 'STEP_TEST_15.ard', 'vendor.dll',
                     'feature/private_temp/cache.bin', 'step_capture/temp/catalog_map.json',
                     'step_capture/captured.x_t', 'feature/native/feature_object.bin'):
            path = out / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture')
        archive = pipeline.archive_result(out, False)
        with zipfile.ZipFile(archive) as z:
            self.assertEqual(set(z.namelist()), {'step_ard_result.json',
                'step_capture/captured.x_t', 'feature/native/feature_object.bin'})

    def test_preflight_failure_never_calls_capture_or_writer_and_produces_diagnostics(self):
        source = self.root / 'model.stp'
        source.write_bytes(STEP)
        out = self.root / 'result'
        with patch.object(pipeline, 'process_names', return_value=[]), \
             patch.object(pipeline.capture_stage, 'main') as capture, \
             patch.object(pipeline, 'run_native') as native:
            self.assertEqual(pipeline.convert(source, self.root / 'missing', self.root / 'SolidLink.exe', out), 1)
            capture.assert_not_called()
            native.assert_not_called()
        result = json.loads((out / 'step_ard_result.json').read_text())
        self.assertFalse(result['ard_created'])
        self.assertFalse((out / 'STEP_TEST_15.ard').exists())
        self.assertTrue(out.with_suffix('.zip').is_file())

    def test_full_orchestration_uses_fresh_capture_and_revokes_ard_on_late_acam(self):
        # Test doubles simulate both native stages. Real guards, copying,
        # measurement checks, object parsing, wrapper and archiving still run.
        for late_acam in (False, True):
            with self.subTest(late_acam=late_acam), tempfile.TemporaryDirectory() as temporary:
                folder = Path(temporary)
                source, out = folder / 'source.stp', folder / 'result'
                source.write_bytes(STEP)
                bundle = folder / 'bundle/step_ard'
                (bundle / 'data').mkdir(parents=True)
                (bundle.parent / 'alphacam_reference').mkdir()
                prefix = b'fixture drawing' + b'\x2e\x04\x00id\0'
                base = prefix + b'\x2f' + struct.pack('<I', prefix.index(b'\x2e')+1) + b'\0'
                (bundle / 'data/1_test.ard').write_bytes(base)
                (bundle.parent / 'alphacam_reference/Feature.po').write_bytes(b'fixture catalog')
                root, server = folder / 'install', folder / 'SolidLink.exe'
                fake_modules = {root / 'Add-Ins/Feature/Feature.dll': pipeline.CURRENT_SHA256,
                                root / 'Add-Ins/Feature/PSKERNEL.dll': pipeline.KERNEL_SHA256,
                                root / 'Add-Ins/SolidImport/SolidImport.dll': pipeline.capture_stage.IMPORTER_SHA256,
                                server: pipeline.SERVER_SHA256}
                for path in fake_modules:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b'native module test double; never loaded')
                native_proof = {}

                def fingerprint(path):
                    return fake_modules.get(Path(path)) or SHA(Path(path).read_bytes())

                def capture(args):
                    directory = Path(args[args.index('--out')+1])
                    capture_fixture(directory)
                    return 0

                def native(root, private, directory, report, observe, feature_sha256):
                    self.assertEqual(private.read_bytes(), CAPTURE)
                    self.assertEqual(report['model_expected_sha256'], SHA(CAPTURE))
                    self.assertTrue((out / 'step_capture/worker_result.json').is_file())
                    (directory / 'native').mkdir()
                    path = str(private).encode() + b'\0'
                    fmt = b'Parasolid\0'
                    record = (struct.pack('<III', 3, 1, 1) + struct.pack('<I', len(path)) + path
                        + struct.pack('<I', len(fmt)) + fmt + struct.pack('<IBI', 0, 1, 1)
                        + struct.pack('<I', len(GEOMETRY)) + GEOMETRY + struct.pack('<III', 1, 1, 0))
                    (directory / 'native/feature_object.bin').write_bytes(record)
                    (directory / 'native/roundtrip.x_b').write_bytes(GEOMETRY)
                    ready, final = session()
                    ready['argument_bridge_selftest'] = final['argument_bridge_selftest'] = True
                    report.update(cls_ready=ready, cls_result=final, cls_exit_code=0, feature_exit_code=0)
                    checks = {name: {'query_return_code': 0, 'was_error': False,
                        'output_guards_intact': True, 'error_clear_called': False}
                        for name in ('before_save', 'after_save', 'before_record', 'after_record')}
                    checks.update(api={'module_sha256': pipeline.KERNEL_SHA256,
                        'iat_export_identity_matches': True}, pskernel_file_unchanged=True)
                    report['kernel_diagnostics'] = checks
                    native_proof.update(native_report(record))
                    native_proof['input']['sha256'] = SHA(CAPTURE)
                    stage = native_proof['stages'][0]
                    stage['summary'].update(stop_reason='completed_native_record', save_status={'ok': True},
                        host_selftest_ok=True, host_context_bound=True, text_table_initialized=True,
                        output={'size_bytes': len(GEOMETRY), 'sha256': SHA(GEOMETRY)})
                    stage['checkpoints'] = [dict(checkpoint='host_method', interface='IAlphaCamApp',
                        method='SpecialKey', host_phase='save', value=0), dict(checkpoint='live_save_projection',
                        mask=16, value=0, source='current_CLS_GetAccessCode_348')]
                    catalog = [{'name': 'fixture.s_t', 'sha256': 'fixture'}]
                    stage['result'] = {'input_copy': {'sha256_before': SHA(CAPTURE),
                        'sha256_after': SHA(CAPTURE), 'unchanged': True, 'path': str(private.resolve())},
                        'model_probe': dict(model(), schema_path={'ok': True, 'native_readback_matches': True,
                            'original_schema_callbacks_retained': True, 'files_before': catalog}),
                        'schema_path_cleanup': {'ok': True, 'previous_path_restored': True,
                            'configured_value_unchanged': True, 'files_unchanged': True, 'files_after': catalog}}

                def processes():
                    return [{'name': 'Acam.exe', 'pid': 123}] if late_acam and (out / 'STEP_TEST_15.ard').exists() else []

                with patch.object(pipeline, 'HERE', bundle), patch.object(pipeline, 'INSTALLATION', {}), \
                     patch.object(pipeline, 'BASE_SHA256', SHA(base)), patch.object(wrapper_module, 'BASE_SHA256', SHA(base)), \
                     patch.object(pipeline, 'PO_SHA256', SHA(b'fixture catalog')), \
                     patch.object(pipeline, 'digest', side_effect=fingerprint), \
                     patch.object(pipeline, 'process_names', side_effect=processes), \
                     patch.object(pipeline.capture_stage, 'main', side_effect=capture), \
                     patch.object(pipeline, 'confirmed_capture', return_value=True), \
                     patch.object(pipeline, 'sources_unchanged', return_value=True), \
                     patch.object(pipeline, 'run_native', side_effect=native) as native_call, \
                     patch.object(pipeline, 'collect_feature', return_value=native_proof):
                    code = pipeline.convert(source, root, server, out)
                self.assertEqual(code, 1 if late_acam else 0)
                native_call.assert_called_once()
                result = json.loads((out / 'step_ard_result.json').read_text())
                self.assertEqual(result['ard_created'], not late_acam)
                self.assertEqual((out / 'STEP_TEST_15.ard').exists(), not late_acam)
                with zipfile.ZipFile(out.with_suffix('.zip')) as archive:
                    self.assertEqual('STEP_TEST_15.ard' in archive.namelist(), not late_acam)


if __name__ == '__main__':
    unittest.main()
