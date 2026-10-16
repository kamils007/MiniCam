"""Read actual host properties from an already running ALPHACAM via COM.

GetActiveObject attaches only; no server is created or document modified.
The resulting snapshot describes the host, not a license authorization.
"""
import ctypes as C
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sys
import uuid

from com_host import PTR, HRESULT, ULONG, LONG, Variant, DispParams

SCHEMA = "feature-host-environment-2025-v1"
EXPECTED_FEATURE_SHA256 = "4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec"
APP_IID = "88a2ff74-4309-11d2-9861-00104b4af281"
# CLSIDs and the IV75Acam.App wrapper are present in the supplied Acam.exe.
RUNNING_CLASSES = (
    ("Acamcore / Router", "4fbd3d30-9e88-11d2-9906-00104b4af281"),
    ("V75AcamRouter", "73c32e9e-bc14-40d2-995b-f28f7ccd9f23"),
    ("V75AcamProutaps", "0f1d8a94-07b5-41c1-980b-df1fdf086070"),
)


class ExcepInfo(C.Structure):
    _fields_ = [("code", C.c_uint16), ("reserved", C.c_uint16),
                ("source", PTR), ("description", PTR), ("helpfile", PTR),
                ("helpcontext", ULONG), ("reserved_ptr", PTR),
                ("deferred", PTR), ("scode", LONG)]


def guid(value):
    return (C.c_ubyte * 16).from_buffer_copy(uuid.UUID(value).bytes_le)


def checked(code, action):
    if code < 0:
        raise RuntimeError(action + ": 0x%08X" % (code & 0xFFFFFFFF))


class RunningApp:
    """Own all acquired references and release them before CoUninitialize."""
    def __init__(self):
        self.ole = C.WinDLL("ole32")
        self.auto = C.WinDLL("oleaut32")
        self.ole.CoInitializeEx.argtypes = [PTR, ULONG]
        self.ole.CoInitializeEx.restype = HRESULT
        self.ole.CoUninitialize.argtypes = []
        self.ole.CoUninitialize.restype = None
        self.auto.GetActiveObject.argtypes = [PTR, PTR, C.POINTER(PTR)]
        self.auto.GetActiveObject.restype = HRESULT
        self.auto.VariantClear.argtypes = [C.POINTER(Variant)]
        self.auto.VariantClear.restype = HRESULT
        self.auto.SysStringLen.argtypes = [PTR]
        self.auto.SysStringLen.restype = ULONG
        self.auto.SysFreeString.argtypes = [PTR]
        self.auto.SysFreeString.restype = None
        self.app = PTR()
        self.initialized = False
        self.source = None

    @staticmethod
    def method(obj, slot, result, *types):
        table = C.cast(obj, C.POINTER(C.POINTER(PTR))).contents
        return C.WINFUNCTYPE(result, PTR, *types)(table[slot])

    def release(self, obj):
        if obj:
            self.method(obj, 2, ULONG)(obj)

    def query(self, obj, iid):
        out = PTR()
        checked(self.method(obj, 0, HRESULT, PTR, PTR)(obj, guid(iid), C.byref(out)),
                "QueryInterface " + iid)
        if not out.value:
            raise RuntimeError("QueryInterface returned a null pointer")
        return out

    def bstr(self, pointer):
        count = self.auto.SysStringLen(pointer) if pointer else 0
        if count > 32768:
            raise ValueError("Unexpectedly long property string")
        return C.string_at(pointer, count * 2).decode("utf-16-le") if count else ""

    def property_variant(self, obj, name):
        dispid = LONG()
        names = (C.c_wchar_p * 1)(name)
        null_iid = (C.c_ubyte * 16)()
        checked(self.method(obj, 5, HRESULT, PTR, PTR, ULONG, ULONG, PTR)(
            obj, null_iid, names, 1, 0, C.byref(dispid)), "GetIDsOfNames " + name)
        value, params, exception = Variant(), DispParams(), ExcepInfo()
        try:
            code = self.method(obj, 6, HRESULT, LONG, PTR, ULONG, C.c_uint16,
                               PTR, PTR, PTR, PTR)(obj, dispid, null_iid, 0, 2,
                    C.byref(params), C.byref(value), C.byref(exception), None)
            if code < 0:
                detail = self.bstr(exception.description)
                checked(code, "PROPERTYGET " + name + (": " + detail if detail else ""))
            return value
        except BaseException:
            self.auto.VariantClear(C.byref(value))
            raise
        finally:
            for field in ("source", "description", "helpfile"):
                pointer = getattr(exception, field)
                if pointer:
                    self.auto.SysFreeString(pointer)

    def __enter__(self):
        checked(self.ole.CoInitializeEx(None, 2), "CoInitializeEx")
        self.initialized = True
        attempts = []
        try:
            for name, clsid in RUNNING_CLASSES:
                unknown, dispatch = PTR(), PTR()
                try:
                    code = self.auto.GetActiveObject(guid(clsid), None, C.byref(unknown))
                    checked(code, "GetActiveObject " + name)
                    try:
                        self.app = self.query(unknown, APP_IID)
                    except RuntimeError:
                        dispatch = self.query(unknown, "00020400-0000-0000-c000-000000000046")
                        wrapped = self.property_variant(dispatch, "App")
                        try:
                            if wrapped.vt not in (9, 13) or not wrapped.value.pointer:
                                raise RuntimeError("App did not return an interface")
                            self.app = self.query(wrapped.value.pointer, APP_IID)
                        finally:
                            self.auto.VariantClear(C.byref(wrapped))
                    self.source = {"name": name, "clsid": clsid}
                    return self
                except RuntimeError as error:
                    attempts.append(str(error))
                finally:
                    self.release(dispatch)
                    self.release(unknown)
            raise RuntimeError("Nie znaleziono aktywnego API ALPHACAM. "
                "Otworz ALPHACAM 2025 Router, poczekaj na okno pracy i sprobuj ponownie. "
                "Uruchom oba programy z tego samego konta i z tymi samymi uprawnieniami. "
                + " | ".join(attempts))
        except BaseException:
            self.__exit__(None, None, None)
            raise

    def get(self, name):
        value = self.property_variant(self.app, name)
        try:
            if value.vt == 2:
                return value.value.short
            if value.vt == 3:
                return value.value.integer
            if value.vt == 8:
                return self.bstr(value.value.pointer)
            raise ValueError(name + " returned unsupported VARIANT type " + str(value.vt))
        finally:
            self.auto.VariantClear(C.byref(value))

    def __exit__(self, *unused):
        self.release(self.app)
        self.app = PTR()
        if self.initialized:
            self.ole.CoUninitialize()
            self.initialized = False


def validate_environment(data, expected_sha256=EXPECTED_FEATURE_SHA256):
    if not isinstance(data, dict) or data.get("schema") != SCHEMA or data.get("source") != "running_ALPHACAM_COM":
        raise ValueError("Wymagany jest odczyt z ODCZYTAJ_USTAWIENIA_2025.cmd")
    if data.get("feature_dll_sha256") != expected_sha256:
        raise ValueError("Odczyt ustawien dotyczy innej wersji Feature.dll")
    values = data.get("application", {})
    if not isinstance(values, dict):
        raise ValueError("Brak obiektu application w odczycie API")
    if type(values.get("ProgramLetter")) is not int or values["ProgramLetter"] != ord("R"):
        raise ValueError("Odczyt musi pochodzic z modulu Router (R)")
    special = values.get("SpecialKey")
    if type(special) is not int or not -(2 ** 31) <= special < 2 ** 31:
        raise ValueError("SpecialKey nie jest odczytana wartoscia LONG")
    if not isinstance(values.get("Path"), str) or not values["Path"]:
        raise ValueError("Brak sciezki aplikacji z API")
    return data


def load_environment(path, expected_sha256=EXPECTED_FEATURE_SHA256):
    return validate_environment(json.loads(Path(path).read_text(encoding="utf-8-sig")), expected_sha256)


def capture_environment(app, processes, hash_file=None):
    if len(processes) != 1:
        raise ValueError("Otworz dokladnie jedna instancje ALPHACAM 2025 Router")
    values = {name: app.get(name) for name in ("Name", "Path", "ProgramLetter", "SpecialKey")}
    if not isinstance(values["Path"], str) or not values["Path"]:
        raise ValueError("API nie zwrocilo sciezki instalacji")
    dll = Path(values["Path"]) / "Add-Ins" / "Feature" / "Feature.dll"
    digest = (hash_file or (lambda path: hashlib.sha256(path.read_bytes()).hexdigest()))(dll)
    return validate_environment({"schema": SCHEMA, "source": "running_ALPHACAM_COM",
        "captured_utc": datetime.now(timezone.utc).isoformat(),
        "source_class": app.source, "application": values,
        "feature_dll_path": str(dll), "feature_dll_sha256": digest,
        "acam_processes_during_capture": processes,
        "note": "Read-only host property snapshot; not a license authorization."})


def main():
    if sys.platform != "win32" or C.sizeof(PTR) != 8:
        raise RuntimeError("Wymagany Windows i Python 64-bitowy")
    from probe_feature_2025 import acam_processes
    target = Path(__file__).with_name("host_environment_2025.json")
    error_target = Path(__file__).with_name("host_environment_2025_error.json")
    print("Odczyt ustawien z OTWARTEGO ALPHACAM 2025 Router.", flush=True)
    try:
        processes = acam_processes()
        if len(processes) != 1:
            raise ValueError("Otworz dokladnie jedna instancje ALPHACAM 2025 Router")
        with RunningApp() as app:
            data = capture_environment(app, processes)
        target.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        print("ProgramLetter:", data["application"]["ProgramLetter"])
        print("SpecialKey:", data["application"]["SpecialKey"])
        print("ZAPISANO:", target)
        print("Teraz zamknij ALPHACAM i uruchom URUCHOM_TEST_4_2025.cmd.")
        return 0
    except Exception as error:
        error_target.write_text(json.dumps({"ok": False, "error_type": type(error).__name__,
            "error": str(error)}, ensure_ascii=False, indent=2), encoding="utf-8")
        print("BLAD:", error)
        print("Przeslij:", error_target)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
