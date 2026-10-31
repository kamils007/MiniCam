"""Portable checks for server paths and scoped COM registration discovery."""
import unittest

from read_solidlink_environment import collect, referenced_file


class RegistryDouble:
    errors = []

    def __init__(self, values):
        self.values = values
        self.reads = []

    def subkeys(self, key):
        if key == "":
            return ["Unrelated.Application", "SolidLink.Document.2025.3"]
        return []

    def value(self, key):
        self.reads.append(key)
        return self.values.get(key)


class EnvironmentTests(unittest.TestCase):
    def test_unicode_server_command_and_typelib_resource(self):
        self.assertEqual(referenced_file('"C:\\Program Files\\Bryła\\server.exe" /Automation'),
                         'C:\\Program Files\\Bryła\\server.exe')
        self.assertEqual(referenced_file(r'C:\Program Files\SolidLink\server.exe /Embedding'),
                         r'C:\Program Files\SolidLink\server.exe')
        self.assertEqual(referenced_file(r'C:\Program Files\SolidLink\server.dll\1'),
                         r'C:\Program Files\SolidLink\server.dll')
        self.assertIsNone(referenced_file('"unterminated'))
        self.assertIsNone(referenced_file("unrecognized-command"))

    def test_unrelated_registration_is_never_followed(self):
        registry = RegistryDouble({})
        report = collect(registry)
        self.assertEqual(len(report["classes"]), 1)
        self.assertIsNone(report["classes"][0]["clsid"])
        self.assertFalse(any("Unrelated" in key for key in registry.reads))

    def test_curver_resolves_without_activating_a_server(self):
        clsid = "{00000000-0000-0000-0000-000000000001}"
        registry = RegistryDouble({
            r"SolidLink.Document.2025.3\CurVer": "SolidLink.Document.2025.3.1",
            r"SolidLink.Document.2025.3.1\CLSID": clsid,
            "CLSID\\" + clsid + r"\LocalServer32": r'"Z:\missing\server.exe" /Automation'})
        item = collect(registry)["classes"][0]
        self.assertEqual(item["clsid"], clsid)
        self.assertEqual(item["servers"]["LocalServer32"]["path"], r"Z:\missing\server.exe")
        self.assertFalse(item["servers"]["LocalServer32"]["exists"])

    def test_invalid_class_identifier_cannot_escape_registry_scope(self):
        registry = RegistryDouble({r"SolidLink.Document.2025.3\CLSID": r"..\Other"})
        collect(registry)
        self.assertFalse(any(key.startswith("CLSID\\") for key in registry.reads))


if __name__ == "__main__":
    unittest.main()
