#!/usr/bin/env python3
"""Test 5: read-only discovery of the missing standalone SolidImport host.

No COM activation, DLL loading, license checkout, conversion or registry writes.
The License coclass and ILicense IID come from the supplied 2025 type library.
This report is dependency discovery, never evidence of successful conversion.
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
import sys

from mfc_thread import read_pe_exports
from read_solidlink_environment import Registry, referenced_file

LICENSE_CLSID = '{28AA001D-94E1-44AB-9571-E5E4E3BDB103}'
LICENSE_IID = '{214DE706-D211-4A06-84C3-6CB3044A4685}'
ROUTER_CLSID = '{4FBD3D30-9E88-11D2-9906-00104B4AF281}'
SOLIDLINK_CLSID = '{80C33292-C478-4543-9DC1-5381A5156F05}'
MAX_FILE = 128 * 1024 * 1024
MAX_TOTAL = 320 * 1024 * 1024
MAX_ENTRIES = 12000
MAX_MODULES = 48
HERE = Path(__file__).resolve().parent


def process_names(names=('acam.exe',)):
    """Use fixed-width Win32 types, also when inspected by portable tests."""
    import ctypes as C
    u32, i32, ptr = C.c_uint32, C.c_int32, C.c_void_p

    class Entry(C.Structure):
        _fields_ = [('size', u32), ('usage', u32), ('pid', u32),
                    ('heap', C.c_size_t), ('module', u32), ('threads', u32),
                    ('parent', u32), ('priority', i32), ('flags', u32),
                    ('name', C.c_wchar * 260)]

    k = C.WinDLL('kernel32', use_last_error=True)
    k.CreateToolhelp32Snapshot.argtypes, k.CreateToolhelp32Snapshot.restype = [u32, u32], ptr
    k.Process32FirstW.argtypes = [ptr, C.POINTER(Entry)]
    k.Process32FirstW.restype = i32
    k.Process32NextW.argtypes, k.Process32NextW.restype = [ptr, C.POINTER(Entry)], i32
    k.CloseHandle.argtypes, k.CloseHandle.restype = [ptr], i32
    snapshot = k.CreateToolhelp32Snapshot(2, 0)
    if snapshot == ptr(-1).value:
        raise C.WinError(C.get_last_error())
    wanted = {name.casefold() for name in names}
    found = []
    try:
        entry = Entry()
        entry.size = C.sizeof(entry)
        more = k.Process32FirstW(snapshot, C.byref(entry))
        while more:
            if entry.name.casefold() in wanted:
                found.append({'pid': entry.pid, 'name': entry.name})
            more = k.Process32NextW(snapshot, C.byref(entry))
    finally:
        k.CloseHandle(snapshot)
    return found


def pe_imports(data):
    """Read DLL names only; never import or execute their code."""
    def number(fmt, at):
        if at < 0 or at + struct.calcsize(fmt) > len(data):
            raise ValueError('Truncated PE')
        return struct.unpack_from(fmt, data, at)[0]

    u16 = lambda at: number('<H', at)
    u32 = lambda at: number('<I', at)
    if data[:2] != b'MZ':
        raise ValueError('Missing MZ')
    pe = u32(60)
    if data[pe:pe + 4] != b'PE\0\0' or u16(pe + 4) != 0x8664:
        raise ValueError('Expected AMD64 PE')
    opt, count = pe + 24, u16(pe + 6)
    if u16(opt) != 0x20B or not 1 <= count <= 96 or u16(pe + 20) < 128:
        raise ValueError('Unexpected PE headers')
    sections = []
    for i in range(count):
        at = opt + u16(pe + 20) + i * 40
        sections.append((u32(at + 12), u32(at + 16), u32(at + 20)))

    def offset(rva, length):
        for base, size, raw in sections:
            if base <= rva and rva + length <= base + size:
                at = raw + rva - base
                if at + length <= len(data):
                    return at
        raise ValueError('PE RVA outside file-backed sections')

    rva, size = u32(opt + 120), u32(opt + 124)
    if not rva or not size:
        return []
    if size < 20:
        raise ValueError('Truncated import directory')
    names = []
    for i in range(min(size // 20, 4096)):
        at = offset(rva + 20 * i, 20)
        descriptor = struct.unpack_from('<IIIII', data, at)
        if not any(descriptor):
            return names
        name_rva = descriptor[3]
        start = offset(name_rva, 1)
        end = data.find(b'\0', start, min(start + 512, len(data)))
        if end < 0:
            raise ValueError('Unterminated DLL name')
        offset(name_rva, end - start + 1)
        names.append(data[start:end].decode('ascii'))
    raise ValueError('Unterminated import directory')


def module_metadata(path):
    path = Path(path)
    result = {'path': str(path), 'exists': path.is_file()}
    if not result['exists']:
        return result
    result['size_bytes'] = path.stat().st_size
    if result['size_bytes'] > MAX_FILE:
        result['inspection_skipped'] = 'Per-file size limit'
        return result
    data = path.read_bytes()
    result['sha256'] = hashlib.sha256(data).hexdigest()
    try:
        result['imports'] = pe_imports(data)
        result['machine'] = 'AMD64'
    except (ValueError, UnicodeError, struct.error) as error:
        result['imports_error'] = str(error)
    try:
        exports = read_pe_exports(data)
        names = sorted(exports['names'])
        selected = [x for x in names if 'guid_initializer' not in x and re.search(
            r'licen[sc]|tpif|checkout|initiali[sz]|dllgetclassobject|dllregisterserver', x, re.I)]
        result['export_count'] = len(exports['ordinals'])
        result['named_export_count'] = len(names)
        result['selected_exports'] = selected[:300]
        result['selected_exports_truncated'] = len(selected) > 300
        result['com_factory_exported'] = 'DllGetClassObject' in names
    except (ValueError, UnicodeError, struct.error) as error:
        result['exports_error'] = str(error)
    result['unchanged_after_read'] = hashlib.sha256(path.read_bytes()).hexdigest() == result['sha256']
    return result


def class_registration(registry, clsid):
    root = 'CLSID\\' + clsid
    result = {'clsid': clsid, 'description': registry.value(root), 'servers': {}}
    for kind in ('InprocServer32', 'LocalServer32'):
        command = registry.value(root + '\\' + kind)
        if command:
            result['servers'][kind] = {'reference': command, 'path': referenced_file(command)}
    for subkey in ('ProgID', 'VersionIndependentProgID', 'TypeLib', 'TreatAs'):
        result[subkey] = registry.value(root + '\\' + subkey)
    result['AppID'] = registry.value(root, 'AppID')
    return result


def license_route(registration):
    servers = registration.get('servers', {})
    inproc = servers.get('InprocServer32', {}).get('path')
    if inproc and Path(inproc).suffix.casefold() == '.dll':
        return 'inproc_registration_found_not_activated_or_validated'
    if servers.get('LocalServer32'):
        return 'out_of_process_registration_only_not_activated'
    return 'no_standalone_server_registration_found'


def candidates(roots, extra_paths):
    """Bounded scan of vendor folders, with no traversal of symbolic links."""
    found, seen, visited, truncated = [], set(), 0, False

    def add(path):
        key = str(path.resolve()).casefold()
        if key not in seen and path.is_file():
            seen.add(key)
            found.append(path)

    for path in extra_paths:
        add(Path(path))
    for root in roots:
        pending = [(Path(root), 0)]
        while pending and visited < MAX_ENTRIES and len(found) < MAX_MODULES:
            folder, depth = pending.pop()
            if folder.is_symlink() or not folder.is_dir():
                continue
            try:
                with os.scandir(folder) as entries:
                    for entry in entries:
                        visited += 1
                        if visited > MAX_ENTRIES or len(found) >= MAX_MODULES:
                            truncated = True
                            break
                        if entry.is_symlink():
                            continue
                        if entry.is_dir(follow_symlinks=False) and depth < 3:
                            pending.append((Path(entry.path), depth + 1))
                        elif entry.is_file(follow_symlinks=False) and entry.name.lower().endswith('.dll'):
                            if re.search(r'licen[sc]|tpif|^cls|rlm|sentinel|acamfile', entry.name, re.I):
                                add(Path(entry.path))
            except OSError:
                continue
        truncated = truncated or bool(pending)
    return found[:MAX_MODULES], {'entries_visited': visited, 'scan_truncated': truncated}


def collect(root, api):
    views = {}
    extra = [root / 'Hexagon.TPIF.dll', root / 'AcamFile75.dll',
             root / 'Add-Ins' / 'SolidImport' / 'SolidImport.dll']
    for name, flag in [('64bit', api.KEY_WOW64_64KEY), ('32bit', api.KEY_WOW64_32KEY)]:
        reg = Registry(api, flag)
        classes = {label: class_registration(reg, clsid) for label, clsid in
                   [('License', LICENSE_CLSID), ('Router', ROUTER_CLSID),
                    ('SolidLink', SOLIDLINK_CLSID)]}
        views[name] = {'classes': classes, 'registry_errors': reg.errors}
        if name == '64bit':
            for cls in classes.values():
                for server in cls['servers'].values():
                    if server['path']:
                        extra.append(Path(server['path']))
            solidlink = classes['SolidLink']['servers'].get('LocalServer32', {}).get('path')
            if solidlink:
                extra.append(Path(solidlink).parent / 'SLInterop.dll')
    common = Path(os.environ.get('CommonProgramFiles', r'C:\Program Files\Common Files')) / 'Hexagon'
    paths, scan = candidates([root, common], extra)
    modules, total = [], 0
    for path in paths:
        size = path.stat().st_size
        if total + size > MAX_TOTAL:
            modules.append({'path': str(path), 'inspection_skipped': 'Total size limit'})
            continue
        total += size
        try:
            modules.append(module_metadata(path))
        except OSError as error:
            modules.append({'path': str(path), 'read_error': str(error)})
    return {'registration': views, 'modules': modules, 'scan': scan,
            'license_route': license_route(views['64bit']['classes']['License']),
            'license_iid_from_2025_typelib': LICENSE_IID}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    if sys.platform != 'win32' or struct.calcsize('P') != 8:
        parser.error('Wymagany Windows i Python x64')
    out = (args.out or HERE / ('standalone_test_5_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    out.mkdir(parents=True, exist_ok=False)
    report = {'schema': 'solidimport-standalone-discovery-v1', 'test_version': '5',
              'captured_utc': datetime.now(timezone.utc).isoformat(),
              'discovery_completed': False, 'conversion_attempted': False,
              'parasolid_created': False, 'ard_created': False,
              'com_activation_attempted': False, 'vendor_native_code_loaded': False,
              'license_state_modified': False, 'registry_modified': False}
    print('TEST 5: zaleznosci STEP bez uruchomionego ALPHACAM-a.', flush=True)
    print('To odczyt konfiguracji; ten test jeszcze nie konwertuje STEP.', flush=True)
    try:
        before = process_names()
        report['acam_processes_before'] = before
        if before:
            raise RuntimeError('Zamknij wszystkie wersje ALPHACAM i uruchom test ponownie.')
        root = args.root
        if root is None:
            snapshot = json.loads((HERE / 'host_environment_2025.json').read_text(encoding='utf-8-sig'))
            root = Path(snapshot['application']['Path'])
        root = root.resolve()
        if not root.is_dir() or not (root / 'Acam.exe').is_file():
            raise RuntimeError('Brak instalacji pod wskazana sciezka: ' + str(root))
        report['installation_root'] = str(root)
        import winreg
        report.update(collect(root, winreg))
        report['discovery_completed'] = True
    except Exception as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
    finally:
        try:
            report['acam_processes_after'] = process_names()
            report['no_acam_at_checkpoints'] = (report.get('acam_processes_before') == []
                                               and report['acam_processes_after'] == [])
        except OSError as error:
            report['process_check_error'] = str(error)
        target = out / 'standalone_dependencies.json'
        with target.open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
        archive = shutil.make_archive(str(out), 'zip', root_dir=out)
    print('Odczyt: ' + ('zakonczony' if report['discovery_completed'] else 'blad'))
    if report.get('error'):
        print('Blad: ' + report['error']['message'])
    print('STEP -> Parasolid: NIE TESTOWANO')
    print('PRZESLIJ ZIP: ' + archive)
    return 0 if report['discovery_completed'] and report.get('no_acam_at_checkpoints') else 1


if __name__ == '__main__':
    raise SystemExit(main())
