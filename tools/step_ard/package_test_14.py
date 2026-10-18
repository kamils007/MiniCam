"""Build the private runnable ZIP from repository sources and supplied data."""
import argparse
import hashlib
from pathlib import Path
import zipfile

from build_step_ard_14 import MODEL_SHA256, BASE_SHA256, PO_SHA256

HERE = Path(__file__).resolve().parent
FILES = {
    'step_ard': ('build_step_ard_14.py', 'build_step_ard.py', 'feature_native_14.py',
                 'feature_cls_session_14.ps1', 'feature_cls_invocation.cs', 'URUCHOM_TEST_14_PARASOLID_ARD.cmd',
                 'README_TEST_14_PL.md', 'test_step_ard_14_checks.py', 'test_step_ard_checks.py'),
    'alphacam_reference': ('probe_feature_2025.py', 'com_host.py', 'com_typeinfo.py',
                          'feature_texts.py', 'layer_host.py', 'native_trace.py',
                          'object_serialization.py', 'host_contract_common.json', 'feature_builds.py',
                          'parasolid_errors.py', 'feature_schema_path.py'),
    'solidimport': ('read_standalone_dependencies.py', 'read_solidlink_environment.py',
                    'mfc_thread.py', 'test_step_conversion.py'),
}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--base', type=Path, required=True)
    p.add_argument('--po', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--profile',choices=('14','14_1','14_2','14_3','14_4','14_5'),default='14')
    a = p.parse_args()
    data = {}
    for folder, names in FILES.items():
        for name in names:
            if a.profile != '14' and name in ('URUCHOM_TEST_14_PARASOLID_ARD.cmd', 'README_TEST_14_PL.md'):
                continue
            data[folder + '/' + name] = (HERE.parent / folder / name).read_bytes()
    if a.profile != '14':
        for name in ('build_step_ard_' + a.profile + '.py', 'URUCHOM_TEST_' + a.profile + '_PARASOLID_ARD.cmd', 'README_TEST_' + a.profile + '_PL.md'):
            data['step_ard/' + name] = (HERE / name).read_bytes()
    for path, target, expected in [(a.capture, 'step_ard/data/captured.x_t', MODEL_SHA256),
                                    (a.base, 'step_ard/data/1_test.ard', BASE_SHA256),
                                    (a.po, 'alphacam_reference/Feature.po', PO_SHA256)]:
        value = path.read_bytes()
        if hashlib.sha256(value).hexdigest() != expected:
            raise ValueError('Unexpected supplied data: ' + str(path))
        data[target] = value
    data['CZYTAJ_NAJPIERW.txt'] = ('TEST ' + a.profile.replace('_','.') + ': Parasolid z testu 13 -> ARD\r\n'
        'Zamknij ALPHACAM. Otworz folder step_ard i uruchom URUCHOM_TEST_' + a.profile + '_PARASOLID_ARD.cmd.\r\n'
        'Przeslij wynikowy ZIP. Szczegoly: step_ard/README_TEST_' + a.profile + '_PL.md.\r\n').encode('utf-8')
    for name in tuple(data):
        if name.endswith('.cmd'):
            data[name] = data[name].replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    manifest = ''.join(hashlib.sha256(value).hexdigest() + '  ' + name + '\n'
                       for name, value in sorted(data.items()))
    data['SHA256SUMS.txt'] = manifest.encode('utf-8')
    with zipfile.ZipFile(a.out, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, value in sorted(data.items()):
            archive.writestr(name, value)
    print(str(a.out.resolve()))


if __name__ == '__main__':
    main()
