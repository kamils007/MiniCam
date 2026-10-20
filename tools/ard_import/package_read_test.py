"""Package the ARD read test without private catalogs, models or vendor files."""
import argparse
import ast
import hashlib
from pathlib import Path
import zipfile
from read_ard_body import OVERLAYS

HERE = Path(__file__).resolve().parent


def package(output, geometry=False):
    files = {}
    launcher = ('TEST_GEOMETRII_BRYLY.cmd', 'README_GEOMETRIA.md') if geometry else ('TEST_ODCZYTU_BRYLY.cmd', 'README_ODCZYT.md')
    for name in ('read_ard_body.py', 'InvokeArdRead.ps1', *launcher):
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
    prefix = 'MiniCam_geometria_bryly/' if geometry else 'MiniCam_test_odczytu_bryly/'
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr(prefix + name, data)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError('ZIP verification failed')
        for name, data in files.items():
            if archive.read(prefix + name) != data:
                raise ValueError('ZIP contents differ')
    return files


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--geometry', action='store_true')
    args = parser.parse_args()
    files = package(args.out, geometry=args.geometry)
    print(str(args.out.resolve()) + ': ' + str(len(files)) + ' files; ZIP verified')
