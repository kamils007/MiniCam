"""Portable checks for bounded error interpretation; no native Parasolid calls."""
import struct
import unittest
import ctypes as C

from parasolid_errors import error_fields, ParasolidErrors


class ErrorFieldsTests(unittest.TestCase):
    def test_unknown_build_is_rejected_before_any_windows_memory_access(self):
        with self.assertRaises(ValueError):
            ParasolidErrors(0, 'unknown')

    def test_native_output_read_preserves_last_error_and_checks_canaries(self):
        class Kernel:
            restored = None
            def GetLastError(self): return 987
            def SetLastError(self, value): self.restored = value
        for corrupt in (False, True):
            reader = ParasolidErrors.__new__(ParasolidErrors)
            reader.kernel = Kernel()
            reader.text = lambda pointer: None
            def ask(flags, output):
                C.cast(flags, C.POINTER(C.c_ubyte))[0] = 1
                C.c_int32.from_address(output.value + 32).value = 321
                if corrupt:
                    C.c_ubyte.from_address(output.value - 1).value = 0
                return 0
            reader.ask = ask
            if corrupt:
                with self.assertRaisesRegex(RuntimeError, 'canary'):
                    reader.read()
            else:
                result = reader.read()
                self.assertTrue(result['was_error'])
                self.assertEqual(result['error_code_at_verified_offset_0x20'], 321)
            self.assertEqual(reader.kernel.restored, 987)

    def test_undefined_error_structure_is_not_read_without_an_error(self):
        def forbidden(pointer):
            raise AssertionError('An undefined pointer was inspected')
        self.assertEqual(error_fields(b'', False, 0, forbidden), {})
        self.assertEqual(error_fields(b'', True, 99, forbidden), {})

    def test_inline_function_and_token_match_verified_kernel_layout(self):
        data = bytearray(128)
        data[:18] = b'PK_PART_transmit_u'
        token = b'PK_ERROR_schema_access_error'
        data[36:36+len(token)] = token
        struct.pack_into('<i', data, 32, 970)
        result = error_fields(data, True, 0)
        self.assertEqual(result['error_code_at_verified_offset_0x20'], 970)
        self.assertEqual(result['function'], 'PK_PART_transmit_u')
        self.assertEqual(result['code_token'], token.decode())
        self.assertEqual(result['pk_tokens'], [{'structure_offset': 0, 'text': 'PK_PART_transmit_u'},
                                               {'structure_offset': 36, 'text': token.decode()}])
        self.assertFalse(result['remaining_structure_fields_decoded'])

    def test_nonterminated_or_invalid_inline_tokens_are_ignored_without_dereferencing(self):
        for token in (b'PK_bad name', b'PK_\xff', b'PK_' + b'A'*29):
            data = bytearray(128)
            data[:len(token)] = token
            def forbidden(pointer):
                raise AssertionError('Inline bytes treated as a pointer')
            self.assertFalse(error_fields(data, True, 0, forbidden)['pk_tokens'])


if __name__ == '__main__':
    unittest.main()
