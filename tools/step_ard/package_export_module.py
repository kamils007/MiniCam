"""Create the optional MiniCam ALPHACAM runtime, without vendor DLLs or old solids."""
import argparse
import hashlib
from pathlib import Path
import zipfile

from package_test_14 import FILES
from package_test_15 import CAPTURE_SOURCES
from build_step_ard_14 import BASE_SHA256, PO_SHA256

HERE = Path(__file__).resolve().parent


def runtime_files(base, po):
    files = {}
    for folder, names in FILES.items():
        for name in names:
            if name.endswith('checks.py') or name.startswith(('README_', 'URUCHOM_')):
                continue
            files[folder + '/' + name] = (HERE.parent / folder / name).read_bytes()
    for name in CAPTURE_SOURCES:
        files['solidimport/' + name] = (HERE.parent / 'solidimport' / name).read_bytes()
    for name in ('build_step_ard_15.py', 'export_record.py'):
        files['step_ard/' + name] = (HERE / name).read_bytes()
    for path, target, expected in ((base, 'step_ard/data/1_test.ard', BASE_SHA256),
                                 (po, 'alphacam_reference/Feature.po', PO_SHA256)):
        data = Path(path).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError('Unexpected private runtime input: ' + str(path))
        files[target] = data
    files['README_PL.md'] = (HERE.parent.parent / 'modules/alphacam/README.md').read_bytes()
    if any(n.lower().endswith(('.dll', '.exe', '.x_t', '.x_b', '.step', '.stp')) for n in files):
        raise ValueError('Runtime must not contain vendor modules or a previous solid')
    files['SHA256SUMS.txt'] = ''.join(hashlib.sha256(data).hexdigest() + '  ' + name + '\n'
        for name, data in sorted(files.items())).encode('utf-8')
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--po', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    files = runtime_files(args.base, args.po)
    with zipfile.ZipFile(args.out, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr('alphacam-export/' + name, data)
    print(args.out.resolve())


if __name__ == '__main__':
    main()
