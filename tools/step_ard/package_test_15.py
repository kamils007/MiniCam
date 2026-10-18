"""Package the integrated runner, original STEP and reviewed private template."""
import argparse
import hashlib
from pathlib import Path
import zipfile

from package_test_14 import FILES
from build_step_ard_14 import STEP_SHA256, BASE_SHA256, PO_SHA256

HERE = Path(__file__).resolve().parent
CAPTURE_SOURCES = ('probe_solidimport_step_capture.py', 'step_capture_support.py',
    'probe_solidimport_text_catalog.py', 'probe_solidimport_special_key.py',
    'probe_cls_connection.py', 'text_catalog.py', 'solidimport_step_capture_worker.ps1',
    'solidimport_step_capture_native.cs', 'solidimport_step_capture_host.cs',
    'cls_license_adapter.cs', 'solidimport_step_capture_reference.json')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step', type=Path, required=True)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--po', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    data = {}
    for folder, names in FILES.items():
        for name in names:
            if name.startswith(('URUCHOM_', 'README_')):
                continue
            data[folder + '/' + name] = (HERE.parent / folder / name).read_bytes()
    for name in CAPTURE_SOURCES:
        data['solidimport/' + name] = (HERE.parent / 'solidimport' / name).read_bytes()
    for name in ('build_step_ard_15.py', 'test_step_ard_15_checks.py',
                 'URUCHOM_TEST_15_STEP_ARD.cmd', 'README_TEST_15_PL.md'):
        data['step_ard/' + name] = (HERE / name).read_bytes()
    for path, name, expected in ((args.step, 'step_ard/data/input_model.stp', STEP_SHA256),
        (args.base, 'step_ard/data/1_test.ard', BASE_SHA256),
        (args.po, 'alphacam_reference/Feature.po', PO_SHA256)):
        value = path.read_bytes()
        if hashlib.sha256(value).hexdigest() != expected:
            raise ValueError('Unexpected supplied source: ' + str(path))
        data[name] = value
    data['CZYTAJ_NAJPIERW.txt'] = (
        'TEST 15: STEP -> Parasolid -> ARD w jednym uruchomieniu.\r\n'
        'Zamknij ALPHACAM. W folderze step_ard uruchom URUCHOM_TEST_15_STEP_ARD.cmd.\r\n'
        'Dwuklik przetwarza oryginalny STEP z paczki. Mozesz tez przeciagnac swoj STEP na ten plik CMD.\r\n'
        'Przeslij wynikowy ZIP wskazany w konsoli. Szczegoly: step_ard/README_TEST_15_PL.md.\r\n'
    ).encode('utf-8')
    for name in tuple(data):
        if name.endswith('.cmd'):
            data[name] = data[name].replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if any(name.lower().endswith(('.dll', '.exe', '.x_t', '.x_b')) for name in data):
        raise ValueError('Package must not contain vendor modules or a previous Parasolid')
    data['SHA256SUMS.txt'] = ''.join(hashlib.sha256(value).hexdigest() + '  ' + name + '\n'
        for name, value in sorted(data.items())).encode('utf-8')
    with zipfile.ZipFile(args.out, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, value in sorted(data.items()):
            archive.writestr(name, value)
    print(args.out.resolve())


if __name__ == '__main__':
    main()
