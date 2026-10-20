"""Package the ARD read test without private catalogs, models or vendor files."""
import argparse
import ast
import hashlib
from pathlib import Path
import zipfile

HERE = Path(__file__).resolve().parent
OVERLAYS = ('alphacam_reference/probe_feature_2025.py',
            'step_ard/feature_native_14.py', 'step_ard/build_step_ard_14.py')


def package(output):
    files = {}
    for name in ('read_ard_body.py', 'InvokeArdRead.ps1', 'TEST_ODCZYTU_BRYLY.cmd', 'README_ODCZYT.md'):
        data = (HERE / name).read_bytes()
        if name.endswith('.cmd'):
            data = data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        files[name] = data
    for name in OVERLAYS:
        files['overlay/' + name] = (HERE.parent / name).read_bytes()
    for name, data in files.items():
        if name.endswith('.py'):
            ast.parse(data, filename=name)
    files['SHA256SUMS.txt'] = ''.join(hashlib.sha256(v).hexdigest() + '  ' + k + '\n'
        for k, v in sorted(files.items())).encode()
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr('MiniCam_test_odczytu_bryly/' + name, data)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError('ZIP verification failed')
        for name, data in files.items():
            if archive.read('MiniCam_test_odczytu_bryly/' + name) != data:
                raise ValueError('ZIP contents differ')
    return files


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    files = package(args.out)
    print(str(args.out.resolve()) + ': ' + str(len(files)) + ' files; ZIP verified')
