"""Check real-value preservation, rejection paths and COM cleanup locally."""
import copy
import ctypes as C
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

import host_environment as E


def snapshot():
    return {"schema": E.SCHEMA, "source": "running_ALPHACAM_COM",
            "feature_dll_sha256": E.EXPECTED_FEATURE_SHA256,
            "application": {"ProgramLetter": 82, "SpecialKey": 3,
                            "Name": "ALPHACAM", "Path": "test-install"}}


class EnvironmentTests(unittest.TestCase):
    def test_snapshot_roundtrip_preserves_all_classification_bits(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "settings.json"
            for special in (0, 3, 16, 0x12345678, -(2 ** 31)):
                data = snapshot()
                data["application"]["SpecialKey"] = special
                target.write_text(json.dumps(data), encoding="utf-8-sig")
                self.assertEqual(E.load_environment(target)["application"]["SpecialKey"], special)

    def test_missing_wrong_build_or_wrong_module_is_rejected(self):
        variants = [[], {}, {**snapshot(), "source": "guessed"},
                    {**snapshot(), "feature_dll_sha256": "wrong"}]
        for field, value in (("SpecialKey", None), ("SpecialKey", True),
                             ("SpecialKey", 2 ** 31), ("ProgramLetter", 77),
                             ("Path", "")):
            data = copy.deepcopy(snapshot())
            data["application"][field] = value
            variants.append(data)
        for data in variants:
            with self.subTest(data=data), self.assertRaises(ValueError):
                E.validate_environment(data)

    def test_capture_reads_only_named_properties_and_hashes_matching_dll(self):
        reads, hashes = [], []
        values = snapshot()["application"]
        app = SimpleNamespace(source={"clsid": "test"},
                              get=lambda name: (reads.append(name), values[name])[1])
        def digest(path):
            hashes.append(path)
            return E.EXPECTED_FEATURE_SHA256
        result = E.capture_environment(app, [{"pid": 42}], digest)
        self.assertEqual(reads, ["Name", "Path", "ProgramLetter", "SpecialKey"])
        self.assertEqual(hashes, [Path("test-install") / "Add-Ins" / "Feature" / "Feature.dll"])
        self.assertEqual(result["feature_dll_path"], str(hashes[0]))
        self.assertEqual(result["application"]["SpecialKey"], 3)
        self.assertEqual(result["source"], "running_ALPHACAM_COM")
        with self.assertRaises(ValueError):
            E.capture_environment(app, [{"pid": 42}], lambda path: "other-build")

    def test_zero_or_multiple_processes_rejected_before_any_property_read(self):
        reads = []
        app = SimpleNamespace(get=lambda name: reads.append(name))
        for processes in ([], [{"pid": 1}, {"pid": 2}]):
            with self.assertRaises(ValueError):
                E.capture_environment(app, processes)
        self.assertEqual(reads, [])

    def test_attach_uses_get_active_object_and_balances_references(self):
        events = []
        reader = E.RunningApp.__new__(E.RunningApp)
        reader.app, reader.initialized, reader.source = E.PTR(), False, None
        reader.ole = SimpleNamespace(CoInitializeEx=lambda *a: events.append("init") or 0,
                                    CoUninitialize=lambda: events.append("uninit"))
        def active(clsid, reserved, output):
            events.append("get_active")
            C.cast(output, C.POINTER(E.PTR))[0] = 100
            return 0
        reader.auto = SimpleNamespace(GetActiveObject=active)
        reader.query = lambda obj, iid: events.append(("QI", obj.value, iid)) or E.PTR(200)
        reader.release = lambda obj: events.append(("release", obj.value)) if obj else None
        with reader as app:
            self.assertEqual(app.app.value, 200)
        self.assertEqual(events, ["init", "get_active", ("QI", 100, E.APP_IID),
                                  ("release", 100), ("release", 200), "uninit"])

    def test_no_running_com_object_cleans_up_and_does_not_fabricate_app(self):
        events = []
        reader = E.RunningApp.__new__(E.RunningApp)
        reader.app, reader.initialized, reader.source = E.PTR(), False, None
        reader.ole = SimpleNamespace(CoInitializeEx=lambda *a: 0,
                                    CoUninitialize=lambda: events.append("uninit"))
        reader.auto = SimpleNamespace(GetActiveObject=lambda *a: C.c_int32(0x800401E3).value)
        reader.release = lambda obj: self.assertFalse(obj)
        with self.assertRaisesRegex(RuntimeError, "Nie znaleziono aktywnego API"):
            reader.__enter__()
        self.assertEqual(events, ["uninit"])
        self.assertFalse(reader.app)


if __name__ == "__main__":
    unittest.main(verbosity=2)
