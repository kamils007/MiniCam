"""Test 14.4: read the actual Parasolid error after both native save paths."""
from build_step_ard_14 import main
from feature_builds import CURRENT_SHA256

if __name__ == '__main__':
    raise SystemExit(main(feature_sha256=CURRENT_SHA256, test_id='14_4', direct_record=True, kernel_errors=True))
