#!/usr/bin/env python3
"""Test 9: native ILicense COM round trip backed by genuine CLS.

Exercises the exact IID, vtable slot and VARIANT_BOOL output width. No Acam,
SolidImport, SolidLink or converter is started by this diagnostic.
"""
from probe_cls_connection import main

if __name__ == '__main__':
    raise SystemExit(main(adapter_probe=True))
