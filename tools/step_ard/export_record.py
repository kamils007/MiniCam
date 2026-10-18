"""MiniCam's process boundary around the verified test-15 converter.

Only a newly generated record is published. The editor owns the final document
and combines it with its current 2D geometry, layers and thumbnail.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

# The GUI launches Python with -I, so imports come only from this runtime.
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from build_step_ard_15 import convert, validate_kernel
from build_step_ard_14 import validate_cls, validate_projection, validate_schema_path
from build_step_ard import require_native_success

REQUEST_SCHEMA = 'minicam-alphacam-request-v1'
RESPONSE_SCHEMA = 'minicam-alphacam-response-v1'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verified_record(directory, source_sha256):
    """Recheck completion and the actual record before crossing the GUI boundary."""
    directory = Path(directory)
    report = json.loads((directory / 'step_ard_result.json').read_text(encoding='utf-8-sig'))
    if (report.get('schema') != 'step-to-ard-integrated-15-v1'
            or report.get('source_step_sha256') != source_sha256
            or any(report.get(k) is not True for k in ('ard_created', 'step_repeated_in_this_test',
                'fresh_capture_verified', 'inputs_and_installation_unchanged', 'single_body_only'))
            or report.get('acam_observed') is not False
            or report.get('error') or report.get('final_check_error')):
        detail = report.get('error') or report.get('final_check_error')
        raise ValueError('Konwersja bieżącej bryły niepotwierdzona: ' + str(detail or 'incomplete report'))
    native = report['feature_stage']
    value = validate_cls(native['cls_ready'], native['cls_result'], native['cls_exit_code'],
                         require_argument_bridge=True)
    validate_kernel(native)
    proof = native['feature']
    stage = proof['stages'][0]
    validate_projection(stage, value)
    validate_schema_path(stage)
    capture = (directory / 'step_capture/captured.x_t')
    if not capture.is_file():
        capture = directory / 'step_capture/captured.x_b'
    capture_hash = digest(capture.read_bytes())
    if capture_hash != report['capture']['sha256']:
        raise ValueError('Fresh capture bytes changed')
    child = stage['result']['input_copy']
    if (child.get('sha256_before') != capture_hash or child.get('sha256_after') != capture_hash
            or child.get('unchanged') is not True):
        raise ValueError('Feature input does not match this conversion')
    data = (directory / 'feature/native/feature_object.bin').read_bytes()
    parsed = require_native_success(proof, data, capture_hash, record_only=True)
    if parsed != report['native_fragment'] or parsed['source_record_count'] != 1:
        raise ValueError('Native record does not match the verified report')
    return data


def run(request_file, response_file):
    request_file, response_file = Path(request_file).resolve(), Path(response_file).resolve()
    job = request_file.parent
    if response_file.parent != job or response_file.exists():
        raise ValueError('Response must be a new file in the request directory')
    response = {'schema': RESPONSE_SCHEMA, 'ok': False}
    try:
        request = json.loads(request_file.read_text(encoding='utf-8-sig'))
        if request.get('schema') != REQUEST_SCHEMA:
            raise ValueError('Unsupported MiniCam export request')
        if sys.platform != 'win32' or struct.calcsize('P') != 8:
            raise ValueError('Eksport bryły ALPHACAM wymaga Windows i Pythona x64.')
        model = Path(request['step']).resolve()
        before = digest(model.read_bytes())
        if before != request['step_sha256']:
            raise ValueError('STEP snapshot differs from the editor request')
        output = job / 'conversion'
        code = convert(model, Path(request['alphacam_root']), Path(request['solidlink_server']), output)
        if code != 0:
            report = json.loads((output / 'step_ard_result.json').read_text(encoding='utf-8-sig'))
            raise ValueError(str(report.get('error') or report.get('final_check_error') or 'Konwersja nieudana'))
        record = verified_record(output, before)
        if digest(model.read_bytes()) != before:
            raise ValueError('STEP snapshot changed during conversion')
        with (job / 'record.bin').open('xb') as stream:
            stream.write(record)
        response.update(ok=True, step_sha256=before, record_sha256=digest(record),
                        record_size_bytes=len(record), record_filename='record.bin', bodies=1)
    except Exception as error:
        response['error'] = str(error)
    archive = job / 'conversion.zip'
    if archive.is_file():
        response['diagnostic_archive'] = str(archive)
    # The GUI also waits for normal process exit; an incomplete write is rejected.
    with response_file.open('x', encoding='utf-8') as stream:
        json.dump(response, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    return 0 if response['ok'] else 1


def main():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8', errors='backslashreplace')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--request', type=Path, required=True)
    parser.add_argument('--response', type=Path, required=True)
    args = parser.parse_args()
    return run(args.request, args.response)


if __name__ == '__main__':
    raise SystemExit(main())
