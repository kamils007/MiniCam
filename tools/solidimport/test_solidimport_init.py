#!/usr/bin/env python3
"""Test 3.2: MFC thread, ANSI STEP argument; stop before drawing import.

Windows x64 / Python stdlib only. Actual ALPHACAM must already be running.
No drawing interface or frame mutation is forwarded to the running application.
"""
import argparse
import ctypes as C
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

from test_step_conversion import (CLSID, PROGID, SERVER_SHA256, Registry,
                                  digest, inspect_output, referenced_file)

DLL_SHA256 = 'b88bef3244cac2cc38bfe41df2089f0a1b6e7b62aea08ae443b95cd47dcd8860'
CAPTURE_EXIT = 83
HERE = Path(__file__).resolve().parent
NATIVE_PINS = []  # Retain callback memory through native DLL process detach.
sys.path.insert(0, str(HERE.parent / 'alphacam_reference'))


def save_json(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write('\n')


def encode_step_filename(path, encoding='mbcs'):
    """StepInputFile's second argument is char*, not wchar_t* or BSTR."""
    text = os.fspath(path)
    if not isinstance(text, str) or not text or '\0' in text:
        raise ValueError('Expected a nonempty filename without NUL characters')
    raw = text.encode(encoding, errors='strict')
    if b'\0' in raw or raw.decode(encoding, errors='strict') != text:
        raise ValueError('STEP filename does not round-trip through the Windows code page')
    return raw


def verify_native_step_filename(module, raw, expected):
    """Use the DLL's exact CString conversion imports before passing the path.

    These are original CString operations, not a translator call. Layout: one
    pointer-sized CString object, retained until its imported destructor runs.
    The caller has already checked the SolidImport build hash and MFC thread.
    """
    ptr = C.c_void_p

    def imported(rva, result, *types):
        address = ptr.from_address(module + rva).value
        if not address:
            raise RuntimeError('Unresolved CString import: ' + hex(rva))
        return C.WINFUNCTYPE(result, *types)(address)

    construct = imported(0x183C8, ptr, ptr, C.c_char_p)  # MFC ordinal 290: ANSI -> CStringW
    length = imported(0x18380, C.c_int32, ptr)           # ordinal 5674: GetLength
    destroy = imported(0x18A68, None, ptr)              # ordinal 1033: destructor
    value = ptr()
    construct(C.byref(value), raw)
    try:
        count = length(C.byref(value))
        if not value.value or not 0 < count <= 32767:
            raise RuntimeError('Invalid native CString filename length')
        actual = C.wstring_at(value.value, count)
        expected = Path(expected).resolve()
        if actual != str(expected) or not Path(actual).is_file():
            raise RuntimeError('Native CString conversion did not produce the full input filename')
        return {'argument_type': 'const char*', 'encoding': 'Windows ACP (mbcs)',
                'byte_count': len(raw), 'native_unicode_path_matches': True,
                'native_path_exists': True, 'native_input_sha256': digest(actual)}
    finally:
        destroy(C.byref(value))


def inspect_candidates(directory):
    candidates = []
    # Only the disposable worker's dedicated temp directory, never user's TEMP.
    for path in sorted(Path(directory).rglob('*')):
        if path.is_file() and path.suffix.lower() in {'.x_t', '.x_b'}:
            candidates.append({'path': str(path), **inspect_output(path)})
            if len(candidates) >= 32:
                break
    return candidates


def capture_at_boundary(temp, out):
    candidates = [x for x in inspect_candidates(temp) if x['recognized_parasolid']]
    if len(candidates) != 1:
        raise RuntimeError('Expected one complete Parasolid at import boundary, got %d' % len(candidates))
    source = Path(candidates[0]['path'])
    target = out / ('converted' + source.suffix.lower())
    with source.open('rb') as src, target.open('xb') as dst:
        shutil.copyfileobj(src, dst)
    info = inspect_output(target)
    if not info['recognized_parasolid'] or info['sha256'] != candidates[0]['sha256']:
        raise RuntimeError('Intermediate file changed during capture')
    return {'filename': target.name, **info}


def captured_result(report):
    marker = report.get('capture', {})
    thread = report.get('mfc_thread', {})
    filename = report.get('step_argument', {})
    return (not report.get('error') and not report.get('timeout')
            and report.get('worker_exit_code') == CAPTURE_EXIT
            and type(report.get('init_return_code')) is int
            and report['init_return_code'] == 0
            and thread.get('before_init') is True
            and thread.get('before_step') is True
            and thread.get('entry_import_matches_AfxGetThread') is True
            and type(thread.get('init_thread_id')) is int
            and thread.get('init_thread_id', 0) > 0
            and thread.get('init_thread_id') == thread.get('step_thread_id')
            and filename.get('argument_type') == 'const char*'
            and filename.get('native_unicode_path_matches') is True
            and filename.get('native_path_exists') is True
            and bool(report.get('input_sha256'))
            and filename.get('native_input_sha256') == report['input_sha256']
            and marker.get('boundary') == 'IAlphaCamApp.ActiveDrawing'
            and marker.get('phase') == 'step'
            and marker.get('output', {}).get('sha256') == report.get('output', {}).get('sha256')
            and report.get('output', {}).get('recognized_parasolid') is True
            and all(report.get(k) is True for k in
                    ('source_unchanged', 'input_copy_unchanged', 'server_unchanged', 'dll_unchanged')))


def installed_dll(live, args):
    root = Path(live.get('Path'))
    if live.get('ProgramLetter') != ord('R'):
        raise RuntimeError('Uruchom ALPHACAM Router 2025')
    dll = args.dll.resolve() if args.dll else None
    if dll is None:
        matches = sorted((root / 'Add-Ins').rglob('SolidImport.dll'))
        if len(matches) != 1:
            raise RuntimeError('Nie znaleziono jednej SolidImport.dll; uzyj --dll PELNA_SCIEZKA')
        dll = matches[0]
    if digest(dll) != DLL_SHA256:
        raise RuntimeError('SolidImport.dll ma inna wersje niz przeanalizowana')
    return root, dll


def worker(args, mfc, trace):
    from com_host import MinimalHost, PTR, HRESULT, ULONG, LONG, SHORT, Variant, argument_ctype
    from host_environment import RunningApp, checked
    from layer_host import BSTRs, output

    out = args.out.resolve()
    log = (out / 'host_calls.jsonl').open('x', encoding='utf-8', buffering=1)

    def event(name, **details):
        log.write(json.dumps({'event': name, **details}, ensure_ascii=False) + '\n')
        log.flush()

    try:
        if struct.calcsize('P') != 8:
            raise RuntimeError('Wymagany Python x64')
        live = RunningApp()
        NATIVE_PINS.append(live)
        live.__enter__()  # Kept alive until disposable process termination.
        root, dll = installed_dll(live, args)
        save_json(out / 'native_setup.json', {'dll': str(dll), 'sha256': DLL_SHA256,
                  'application_path': str(root), 'actual_app_attached': True})
        frame = PTR()
        NATIVE_PINS.append(frame)
        checked(live.method(live.app, 0x70 // 8, HRESULT, PTR)(live.app, C.byref(frame)),
                'Actual Frame getter')
        if not frame.value:
            raise RuntimeError('Actual Frame is null')
        bstrs = BSTRs()
        host = MinimalHost(HERE.parent / 'alphacam_reference' / 'host_contract_common.json', event,
                           bstrs=bstrs)
        host.phase = 'init'
        NATIVE_PINS.extend([host, bstrs])
        registrations = []
        phase = ['init']
        originals = {name: obj.handle for name, obj in host.objects.items()}

        def dispatch(obj, spec, values):
            name = spec['name']
            if obj.name == 'IAlphaCamApp':
                if name == 'ActiveDrawing' and phase[0] == 'step':
                    # 0x9070 first requests ActiveDrawing, only after translation.
                    # Terminate BEFORE returning any drawing pointer, even if no file exists.
                    captured = capture_at_boundary(out / 'temp', out)
                    save_json(out / 'capture.json', {'boundary': 'IAlphaCamApp.ActiveDrawing',
                              'phase': phase[0], 'output': captured})
                    event('capture_complete', output=captured, controlled_exit=CAPTURE_EXIT)
                    os._exit(CAPTURE_EXIT)
                if name in {'ProgramLevel', 'SpecialKey', 'License', 'Path', 'ProgramLetter'} and spec['invkind'] == 2:
                    # Exact original getters, including real ILicense reference ownership.
                    fn = live.method(live.app, spec['vtable_offset_x64'] // 8, HRESULT, PTR)
                    hr = fn(live.app, *values)
                    event('actual_app_getter', method=name, hresult=hr)
                    return hr
            if obj.name == 'IFrame':
                if name in {'RegisterImportFunction', 'RegisterExportFunction', 'RegisterImportFunction2'}:
                    item = {'method': name, 'arguments': [bstrs.read(x) for x in values]}
                    registrations.append(item)
                    event('private_registration', **item)
                    return 0
                if name in {'PathOfThisAddin', 'PathOfThisAddin2'} and spec['invkind'] == 2:
                    text = str(dll.parent) if name == 'PathOfThisAddin' else str(dll)
                    return output(values[0], PTR, bstrs.allocate(text))
                if name == 'ReadTextFile' and spec['invkind'] == 1:
                    # Read-only original localization API; never forward menu/drawing operations.
                    types = [argument_ctype(p['type']) for p in spec['parameters_native']]
                    hr = live.method(frame, spec['vtable_offset_x64'] // 8, HRESULT, *types)(frame, *values)
                    event('actual_frame_read_text', hresult=hr)
                    return hr
            return originals[obj.name](spec, values)

        for obj in host.objects.values():
            obj.handle = lambda spec, values, obj=obj: dispatch(obj, spec, values)
        kernel = C.WinDLL('kernel32', use_last_error=True)
        kernel.SetErrorMode.argtypes = [ULONG]
        kernel.SetErrorMode.restype = ULONG
        kernel.SetErrorMode(0x8003)
        kernel.LoadLibraryExW.argtypes = [C.c_wchar_p, PTR, ULONG]
        kernel.LoadLibraryExW.restype = PTR
        kernel.GetProcAddress.argtypes = [PTR, C.c_char_p]
        kernel.GetProcAddress.restype = PTR
        directories = [os.add_dll_directory(str(p)) for p in {dll.parent, root}]
        NATIVE_PINS.extend([kernel, directories])
        module = kernel.LoadLibraryExW(str(dll), None, 0x1100)
        if not module:
            raise C.WinError(C.get_last_error())
        event('dll_loaded', sha256=DLL_SHA256)

        def exported(name, result, *types):
            address = kernel.GetProcAddress(module, name.encode('ascii'))
            if not address:
                raise RuntimeError('Missing export: ' + name)
            return C.WINFUNCTYPE(result, *types)(address)

        version = exported('GetUIVersion', LONG)()
        if version != 1:
            raise RuntimeError('Unexpected native UI version: ' + str(version))
        context = Variant()
        NATIVE_PINS.append(context)
        context.vt = 9
        context.value.pointer = host.app_pointer
        host.objects['IAlphaCamApp'].refs += 1
        thread_proof = {'before_init': bool(mfc.get_thread()),
                        'init_thread_id': mfc.k.GetCurrentThreadId(),
                        'entry_import_matches_AfxGetThread': mfc.entry_import_matches_AfxGetThread}
        if not thread_proof['before_init']:
            raise RuntimeError('MFC thread missing before native initialization')
        trace.phase = 'InitAlphacamAddIn'
        event('before_InitAlphacamAddIn')
        code = exported('InitAlphacamAddIn', LONG, C.POINTER(Variant))(C.byref(context))
        save_json(out / 'init_result.json', {'return_code': code, 'registrations': registrations})
        event('after_InitAlphacamAddIn', return_code=code)
        if code != 0:
            raise RuntimeError('InitAlphacamAddIn returned ' + str(code))
        phase[0] = host.phase = 'step'
        thread_proof.update(before_step=bool(mfc.get_thread()),
                            step_thread_id=mfc.k.GetCurrentThreadId())
        save_json(out / 'mfc_thread_ready.json', thread_proof)
        if not thread_proof['before_step']:
            raise RuntimeError('MFC thread missing before STEP entry')
        trace.phase = 'STEP filename preflight'
        raw_filename = encode_step_filename(args.model.resolve())
        filename_proof = verify_native_step_filename(module, raw_filename, args.model)
        save_json(out / 'step_argument.json', filename_proof)
        event('step_filename_preflight', **filename_proof)
        trace.phase = 'StepInputFile'
        event('before_StepInputFile')
        exported('StepInputFile', None, C.POINTER(Variant), C.c_char_p)(
            C.byref(context), raw_filename)
        # The export does not preserve the translator result in its epilogue.
        event('after_StepInputFile')
        save_json(out / 'worker_result.json', {'step_returned': True})
        return 1
    except Exception as error:
        event('worker_error', type=type(error).__name__, message=str(error))
        save_json(out / 'worker_result.json', {'error': {'type': type(error).__name__, 'message': str(error)}})
        return 1
    # Native objects, DLL, COM apartment and callbacks remain pinned through
    # ExitProcess. This diagnostic deliberately does not validate normal unload.


def mfc_worker_process(args):
    from host_environment import RunningApp
    from mfc_thread import MFCWorker
    from native_trace import NativeExceptionTrace

    out = args.out.resolve()
    log = (out / 'mfc_calls.jsonl').open('x', encoding='utf-8', buffering=1)

    def event(name, **details):
        log.write(json.dumps({'event': name, **details}, ensure_ascii=False) + '\n')
        log.flush()

    try:
        trace = NativeExceptionTrace(out / 'native_exceptions.jsonl')
        NATIVE_PINS.append(trace)
        trace.phase = 'mfc_bootstrap'
        trace.install()
        # Resolve paths only in this STA. Close it before starting the MFC
        # thread; obtain all live COM interfaces again in that thread's own STA.
        with RunningApp() as live:
            root, dll = installed_dll(live, args)
        kernel = C.WinDLL('kernel32', use_last_error=True)
        kernel.LoadLibraryExW.argtypes = [C.c_wchar_p, C.c_void_p, C.c_uint32]
        kernel.LoadLibraryExW.restype = C.c_void_p
        kernel.SetErrorMode.argtypes = [C.c_uint32]
        kernel.SetErrorMode.restype = C.c_uint32
        kernel.SetErrorMode(0x8003)
        directories = [os.add_dll_directory(str(p)) for p in {root, dll.parent}]
        NATIVE_PINS.extend([kernel, directories])
        module = kernel.LoadLibraryExW(str(dll), None, 0x1100)
        if not module:
            raise C.WinError(C.get_last_error())
        mfc = MFCWorker(module, event)
        NATIVE_PINS.append(mfc)

        def run():
            code = worker(args, mfc, trace)
            sys.stdout.flush()
            sys.stderr.flush()
            os._exit(code)

        mfc.run_terminal(run, args.timeout)
    except BaseException as error:
        event('mfc_bootstrap_error', type=type(error).__name__, message=str(error))
        save_json(out / 'mfc_bootstrap_result.json',
                  {'error': {'type': type(error).__name__, 'message': str(error)}})
        return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, default=HERE / 'input_model.stp')
    parser.add_argument('--dll', type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--worker', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        parser.error('Wymagany Windows i Python x64')
    if args.worker:
        # No Python/COM finalizers can outlive the callback objects on shutdown.
        code = mfc_worker_process(args)
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(code)
    if not 5 <= args.timeout <= 300:
        parser.error('Timeout: 5..300 s')
    model = args.model.resolve()
    if not model.is_file() or model.suffix.lower() not in {'.stp', '.step'}:
        parser.error('Brak pliku STEP: ' + str(model))
    out = (args.out or HERE / ('solidimport_test_3_2_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out / 'temp').mkdir()
    before = digest(model)
    report = {'schema': 'solidimport-init-test-3-v3', 'test_version': '3.2', 'parasolid_captured': False,
              'input_sha256': before,
              'complete_ard': False, 'geometry_validated': False,
              'normal_unload_validated': False, 'output_directory': str(out)}
    server = dll = None
    input_copy = out / 'input.stp'
    try:
        import winreg
        registry = Registry(winreg, winreg.KEY_WOW64_64KEY)
        registered = registry.value(PROGID + r'\CLSID')
        if not registered or registered.casefold() != CLSID.casefold():
            raise RuntimeError('Unexpected SolidLink registration')
        path = referenced_file(registry.value('CLSID\\' + registered + r'\LocalServer32'))
        if not path:
            raise RuntimeError('Missing SolidLink server path')
        server = Path(path)
        if digest(server) != SERVER_SHA256:
            raise RuntimeError('Unexpected SolidLink build')
        with model.open('rb') as src, input_copy.open('xb') as dst:
            shutil.copyfileobj(src, dst)
        if digest(input_copy) != before:
            raise RuntimeError('Input changed while copying')
        command = [sys.executable, str(Path(__file__).resolve()), '--worker', '--model', str(input_copy),
                   '--out', str(out), '--timeout', str(args.timeout)]
        if args.dll:
            command += ['--dll', str(args.dll.resolve())]
        env = dict(os.environ, TEMP=str(out / 'temp'), TMP=str(out / 'temp'))
        print('TEST 3.2: sciezka ANSI + watek MFC + ALPHACAM Router 2025', flush=True)
        print('Limit: %d s. Wyniki: %s' % (args.timeout, out), flush=True)
        with (out / 'worker_stdout.txt').open('xb') as stdout, (out / 'worker_stderr.txt').open('xb') as stderr:
            try:
                result = subprocess.run(command, cwd=out, env=env, stdout=stdout, stderr=stderr,
                                        timeout=args.timeout, check=False)
                report['worker_exit_code'] = result.returncode
            except subprocess.TimeoutExpired:
                report['timeout'] = True
        for filename, key in [('native_setup.json', 'setup'), ('init_result.json', 'init'),
                              ('capture.json', 'capture'), ('worker_result.json', 'worker'),
                              ('mfc_bootstrap_result.json', 'mfc_bootstrap'),
                              ('mfc_thread_ready.json', 'mfc_thread'),
                              ('step_argument.json', 'step_argument')]:
            path = out / filename
            if path.is_file():
                report[key] = json.loads(path.read_text(encoding='utf-8'))
        report['init_return_code'] = report.get('init', {}).get('return_code')
        if report.get('setup'):
            dll = Path(report['setup']['dll'])
        marker = report.get('capture', {}).get('output', {})
        if marker.get('filename') in {'converted.x_t', 'converted.x_b'}:
            report['output'] = inspect_output(out / marker['filename'])
        report['temporary_candidates'] = inspect_candidates(out / 'temp')
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
    finally:
        for key, path, expected in [('source_unchanged', model, before),
                                    ('input_copy_unchanged', input_copy, before),
                                    ('server_unchanged', server, SERVER_SHA256),
                                    ('dll_unchanged', dll, DLL_SHA256)]:
            try:
                report[key] = bool(path and digest(path) == expected)
            except OSError:
                report[key] = False
        report['parasolid_captured'] = captured_result(report)
        save_json(out / 'solidimport_step_result.json', report)
        archive = shutil.make_archive(str(out), 'zip', root_dir=out)
    print('Parasolid: ' + ('przechwycony; geometria wymaga kontroli' if report['parasolid_captured'] else 'niepotwierdzony'))
    print('Przeslij plik: ' + archive)
    return 0 if report['parasolid_captured'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
