"""Portable PE/ABI guards for the actual NONAME MFC export layout."""
import json
from pathlib import Path
import struct
import unittest

from mfc_thread import (BEGIN_WORKER_NAME, BEGIN_WORKER_ORDINAL, GET_THREAD_NAME,
                        GET_THREAD_ORDINAL, read_pe_exports, worker_api_names)


def fixture(named=False):
    data = bytearray(0x1000)
    def put(fmt, pos, *values):
        struct.pack_into(fmt, data, pos, *values)
    data[:2] = b'MZ'
    put('<I', 0x3C, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    put('<HH', 0x84, 0x8664, 2)
    put('<H', 0x94, 240)
    optional = 0x98
    put('<H', optional, 0x20B)
    put('<II', optional + 112, 0x2000, 0x600)
    sections = optional + 240
    for pos, rva, raw, flags in [(sections, 0x1000, 0x400, 0x60000020),
                                  (sections + 40, 0x2000, 0x600, 0x40000040)]:
        put('<III', pos + 8, 0x600 if rva == 0x2000 else 0x200, rva,
            0x600 if rva == 0x2000 else 0x200)
        put('<I', pos + 20, raw)
        put('<I', pos + 36, flags)
    base = BEGIN_WORKER_ORDINAL
    count = GET_THREAD_ORDINAL - base + 1
    put('<IIIIII', 0x600 + 16, base, count, 1 if named else 0, 0x2040, 0x2200, 0x2210)
    put('<I', 0x640, 0x1000)
    put('<I', 0x640 + (GET_THREAD_ORDINAL - base) * 4, 0x1010)
    if named:
        put('<I', 0x800, 0x2220)
        put('<H', 0x810, GET_THREAD_ORDINAL - base)
        name = GET_THREAD_NAME.encode('ascii') + b'\0'
        data[0x820:0x820 + len(name)] = name
    return data


class MFCThreadChecks(unittest.TestCase):
    def test_noname_exports_are_supported(self):
        exports = read_pe_exports(fixture())
        self.assertEqual(exports['names'], {})
        self.assertEqual(worker_api_names(exports), (GET_THREAD_ORDINAL, BEGIN_WORKER_ORDINAL))

    def test_matching_named_export_is_supported(self):
        exports = read_pe_exports(fixture(named=True))
        self.assertEqual(exports['names'][GET_THREAD_NAME], GET_THREAD_ORDINAL)
        self.assertEqual(worker_api_names(exports), (GET_THREAD_ORDINAL, BEGIN_WORKER_ORDINAL))

    def test_absent_forwarded_data_or_mismatched_exports_are_rejected(self):
        for rva in [0, 0x2000, 0x5000]:
            data = fixture()
            struct.pack_into('<I', data, 0x640, rva)
            with self.assertRaises(RuntimeError):
                worker_api_names(read_pe_exports(data))
        exports = read_pe_exports(fixture(named=True))
        exports['names'][GET_THREAD_NAME] = BEGIN_WORKER_ORDINAL
        with self.assertRaises(RuntimeError):
            worker_api_names(exports)

    def test_wrong_architecture_truncation_and_invalid_sections_are_rejected(self):
        for data in [b'', fixture()[:0x90], fixture()[:0x700]]:
            with self.assertRaises(ValueError):
                read_pe_exports(data)
        data = fixture()
        struct.pack_into('<H', data, 0x84, 0x14C)
        with self.assertRaises(ValueError):
            read_pe_exports(data)

    def test_selected_abi_matches_microsoft_public_symbol_reference(self):
        reference = json.loads(Path(__file__).with_name('mfc_thread_api_reference.json').read_text())
        names = {item['name']: item['ordinals'] for item in reference['symbols']}
        self.assertEqual(names[GET_THREAD_NAME], [GET_THREAD_ORDINAL])
        self.assertEqual(names[BEGIN_WORKER_NAME], [BEGIN_WORKER_ORDINAL])
        self.assertIn('msdl.microsoft.com/download/symbols/', reference['source']['url'])


if __name__ == '__main__':
    unittest.main()
