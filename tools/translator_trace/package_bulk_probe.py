"""Package only our diagnostic sources; never include vendor code or a model."""
import hashlib
from pathlib import Path
import zipfile

HERE = Path(__file__).resolve().parent
FILES = {
    'BULK_TEST.cmd': HERE / 'BULK_TEST.cmd',
    'BulkTest.ps1': HERE / 'BulkTest.ps1',
    'bulk_conversion_probe.py': HERE / 'bulk_conversion_probe.py',
    'solidlink_bulk_worker.py': HERE / 'solidlink_bulk_worker.py',
    'feature_cls_session_14.ps1': HERE.parent / 'step_ard' / 'feature_cls_session_14.ps1',
    'feature_cls_invocation.cs': HERE.parent / 'step_ard' / 'feature_cls_invocation.cs',
    'ard_payload_reader.py': HERE.parent / 'ard_import' / 'read_ard_body.py',
    'README_PL.md': HERE / 'README_BULK_PL.md',
    'spatial_interop_reference.json': HERE.parent / 'solidimport' / 'spatial_interop_2025_4_reference.json',
}


def package(output):
    content = {name: path.read_bytes() for name, path in FILES.items()}
    # cmd.exe and Windows PowerShell 5.1-compatible ASCII, with Windows newlines.
    for name in ('BULK_TEST.cmd', 'BulkTest.ps1'):
        text = content[name].decode('ascii').replace('\r\n', '\n')
        content[name] = text.replace('\n', '\r\n').encode('ascii')
    content['SHA256SUMS.txt'] = ''.join(
        hashlib.sha256(data).hexdigest() + '  ' + name + '\n'
        for name, data in content.items()).encode('ascii')
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, data in content.items():
            info = zipfile.ZipInfo(name, (2026, 10, 25, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    return output


if __name__ == '__main__':
    print(package(HERE / 'MiniCam_konwersja_bryly_v4.zip'))
