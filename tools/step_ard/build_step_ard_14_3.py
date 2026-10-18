"""Test 14.3: verify the original ARD object writer independently of roundtrip.x_b."""
from build_step_ard_14 import main
from feature_builds import CURRENT_SHA256

if __name__ == '__main__':
    raise SystemExit(main(feature_sha256=CURRENT_SHA256, test_id='14_3', direct_record=True))
