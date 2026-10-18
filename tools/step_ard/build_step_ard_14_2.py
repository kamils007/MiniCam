"""Test 14.2: normalize PowerShell reflection arguments before CLS calls."""
from build_step_ard_14 import main
from feature_builds import CURRENT_SHA256

if __name__ == '__main__':
    raise SystemExit(main(feature_sha256=CURRENT_SHA256, test_id='14_2'))
