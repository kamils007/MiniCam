#!/usr/bin/env python3
"""Isolated STEP to Parasolid trial through the installed SolidLink COM server.

Uses Windows PowerShell's COM Automation support; no pip dependencies. The
original STEP is read only. Conversion runs on a copy in a new output directory.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

from read_solidlink_environment import Registry, referenced_file

PROGID = 'SolidLink.Document.2025.3'
CLSID = '{80C33292-C478-4543-9DC1-5381A5156F05}'
SERVER_SHA256 = '283a9ce685944a12c66c0be8568e48f23eddc9efe63456deafdf308faf24ee4d'


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def inspect_output(path):
    path = Path(path)
    if not path.is_file():
        return {'exists': False, 'recognized_parasolid': False}
    with path.open('rb') as stream:
        header = stream.read(4096)
    match = re.search(rb'FORMAT\s*=\s*(text|binary)\s*;', header, re.I)
    end = header.find(b'**END_OF_HEADER')
    newline = header.find(b'\n', end) if end >= 0 else -1
    size = path.stat().st_size
    return {'exists': True, 'size_bytes': size, 'sha256': digest(path),
            'recognized_parasolid': bool(header.startswith(b'**') and match
                                         and newline >= 0 and size > newline + 1),
            'format': match[1].decode('ascii').lower() if match else None}


def successful_conversion(report):
    worker = report.get('worker', {})
    return (report.get('powershell_exit_code') == 0
            and 'error' not in report and not report.get('timeout')
            and not worker.get('error') and not worker.get('cleanup_error')
            and worker.get('translation_attempted') is True
            and type(worker.get('translate_return_code')) is int
            and worker['translate_return_code'] == 0
            and report.get('output', {}).get('recognized_parasolid') is True
            and report.get('source_unchanged') is True
            and report.get('input_copy_unchanged') is True
            and report.get('server_unchanged') is True)


def powershell_path():
    system = Path(os.environ.get('SystemRoot', r'C:\Windows'))
    # Sysnative reaches x64 PowerShell when the launching Python is 32-bit.
    folder = 'Sysnative' if struct.calcsize('P') == 4 else 'System32'
    path = system / folder / 'WindowsPowerShell' / 'v1.0' / 'powershell.exe'
    if not path.is_file():
        raise FileNotFoundError('Windows PowerShell x64 not found: ' + str(path))
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path,
                        default=Path(__file__).with_name('input_model.stp'))
    parser.add_argument('--out', type=Path)
    parser.add_argument('--timeout', type=int, default=90)
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('This conversion test requires Windows with SolidLink 2025.3 installed')
    if not 5 <= args.timeout <= 300:
        parser.error('Timeout must be between 5 and 300 seconds')
    model = args.model.resolve()
    if not model.is_file() or model.suffix.lower() not in {'.stp', '.step'}:
        parser.error('A STEP input file is required: ' + str(model))
    out = (args.out or Path(__file__).resolve().parent / (
        'solidlink_test_2_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    source_hash = digest(model)
    report = {'schema': 'solidlink-step-conversion-test-2-v1',
              'captured_utc': datetime.now(timezone.utc).isoformat(),
              'input': {'filename': model.name, 'sha256': source_hash,
                        'size_bytes': model.stat().st_size},
              'progid': PROGID, 'license_configuration_modified': False,
              'conversion_succeeded': False, 'complete_ard': False,
              'output_directory': str(out)}
    server = None
    try:
        import winreg
        registry = Registry(winreg, winreg.KEY_WOW64_64KEY)
        registered_clsid = registry.value(PROGID + r'\CLSID')
        if not registered_clsid or registered_clsid.casefold() != CLSID.casefold():
            raise RuntimeError('The registered SolidLink 2025.3 class differs from the analyzed installation')
        command = registry.value('CLSID\\' + registered_clsid + r'\LocalServer32')
        server_text = referenced_file(command)
        if not server_text:
            raise RuntimeError('SolidLink server path could not be read')
        server = Path(server_text)
        if digest(server) != SERVER_SHA256:
            raise RuntimeError('The registered SolidLink executable differs from the analyzed build')
        report['server'] = {'path': str(server), 'sha256': SERVER_SHA256}
        report['dependencies_in_server_directory'] = {
            name: (server.parent / name).is_file()
            for name in ['SLInterop.dll', 'PMFrustum.dll', 'PSKERNEL.dll']}
        input_copy = out / 'input.stp'
        with model.open('rb') as source, input_copy.open('xb') as destination:
            shutil.copyfileobj(source, destination)
        if digest(input_copy) != source_hash:
            raise RuntimeError('Input changed while making the test copy')
        target = out / 'converted.x_t'
        worker_script = Path(__file__).with_name('solidlink_worker.ps1').resolve()
        command = [str(powershell_path()), '-NoProfile', '-NonInteractive', '-STA',
                   '-ExecutionPolicy', 'Bypass', '-File', str(worker_script),
                   '-ClassId', registered_clsid, '-InputModel', str(input_copy),
                   '-OutputModel', str(target), '-OutputDirectory', str(out)]
        print('TEST 2: STEP -> Parasolid, zainstalowany SolidLink 2025.3', flush=True)
        print('Proba na kopii modelu. Limit: ' + str(args.timeout) + ' s.', flush=True)
        with (out / 'worker_stdout.txt').open('xb') as stdout, \
                (out / 'worker_stderr.txt').open('xb') as stderr:
            try:
                process = subprocess.run(command, cwd=out, stdout=stdout, stderr=stderr,
                                         timeout=args.timeout, check=False)
                report['powershell_exit_code'] = process.returncode
            except subprocess.TimeoutExpired:
                report['timeout'] = True
                report['note'] = ('PowerShell worker terminated after timeout; '
                                  'the separate SolidLink server was not forcibly terminated.')
        worker_report = out / 'worker_result.json'
        if worker_report.is_file():
            report['worker'] = json.loads(worker_report.read_text(encoding='utf-8-sig'))
        elif not report.get('timeout'):
            report['error'] = {'message': 'Worker produced no final report; see worker_stderr.txt'}
        report['output'] = inspect_output(target)
        report['input_copy_unchanged'] = digest(input_copy) == source_hash
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
    finally:
        try:
            report['source_unchanged'] = digest(model) == source_hash
            report['server_unchanged'] = bool(server and digest(server) == SERVER_SHA256)
        except OSError as error:
            report['verification_error'] = str(error)
            report['source_unchanged'] = False
        report['conversion_succeeded'] = successful_conversion(report)
        with (out / 'solidlink_step_result.json').open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
    print('Wyniki: ' + str(out))
    print('Konwersja: ' + ('potwierdzona kodem 0 i naglowkiem Parasolid'
                          if report['conversion_succeeded'] else 'niepotwierdzona; sprawdzimy raport'))
    print('Przeslij solidlink_step_result.json i converted.x_t, jesli powstal.')
    print('Przy bledzie dolacz worker_stderr.txt oraz worker_calls.jsonl, jesli istnieja.')
    return 0 if report['conversion_succeeded'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
