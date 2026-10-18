"""Test 14.5: configure the original Feature schema resolver, then save."""
from build_step_ard_14 import main
from feature_builds import CURRENT_SHA256

if __name__ == '__main__':
    raise SystemExit(main(feature_sha256=CURRENT_SHA256, test_id='14_5',
                          direct_record=True, kernel_errors=True, schema_path=True))
