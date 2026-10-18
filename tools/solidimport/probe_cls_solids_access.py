#!/usr/bin/env python3
"""Test 8: one genuine CLS request derived from Acam 2025.4.1.104.

Does not execute Acam.exe, initialize SolidImport, authorize SolidLink or convert.
"""
from probe_cls_connection import main

if __name__ == '__main__':
    raise SystemExit(main(feature_probe=True))
