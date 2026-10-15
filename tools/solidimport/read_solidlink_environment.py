#!/usr/bin/env python3
"""Read only SolidLink COM registration and referenced file metadata.

No DLL loading, COM activation, conversion, registry writes or license queries.
Python standard library only. Run on the Windows machine with ALPHACAM 2025.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys


EXPECTED_DLL_SHA256 = "b88bef3244cac2cc38bfe41df2089f0a1b6e7b62aea08ae443b95cd47dcd8860"
PROGID_PREFIX = "solidlink.document."
MAX_CLASS_NAMES = 500000
MAX_MATCHES = 200


def referenced_file(value):
    """Extract a file path from a COM server command or typelib resource path.

    Does not run the command or expand arbitrary shell expressions.
    An unquoted server command is ambiguous, so only its existing file is used.
    """
    if not isinstance(value, str):
        return None
    text = os.path.expandvars(value.strip())
    if text.startswith('"'):
        end = text.find('"', 1)
        return text[1:end] if end > 1 else None
    match = re.match(r"^(.+?\.(?:exe|dll|ocx|tlb|olb))(?:$|\s|[\\/]\d+$)", text, re.I)
    return match.group(1) if match else None


def file_metadata(value):
    path_text = referenced_file(value)
    result = {"reference": value, "path": path_text}
    if path_text is None:
        return result
    try:
        path = Path(path_text)
        result["exists"] = path.is_file()
        if not result["exists"]:
            return result
        result["size_bytes"] = path.stat().st_size
        if result["size_bytes"] > 256 * 1024 * 1024:
            result["hash_skipped"] = "File exceeds 256 MiB diagnostic limit"
            return result
        digest = hashlib.sha256()
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
        result["sha256"] = digest.hexdigest()
    except OSError as error:
        result["error"] = str(error)
    return result


class Registry:
    def __init__(self, api, view):
        self.api, self.view, self.errors = api, view, []

    def value(self, key, name=""):
        try:
            with self.api.OpenKey(self.api.HKEY_CLASSES_ROOT, key, 0,
                                  self.api.KEY_READ | self.view) as opened:
                value, kind = self.api.QueryValueEx(opened, name)
                return value if isinstance(value, str) else None
        except FileNotFoundError:
            return None
        except OSError as error:
            self.errors.append({"key": key, "error": str(error)})
            return None

    def subkeys(self, key):
        try:
            with self.api.OpenKey(self.api.HKEY_CLASSES_ROOT, key, 0,
                                  self.api.KEY_READ | self.view) as opened:
                count = self.api.QueryInfoKey(opened)[0]
                if count > MAX_CLASS_NAMES:
                    raise ValueError("Registry class count exceeds diagnostic limit")
                return [self.api.EnumKey(opened, i) for i in range(count)]
        except FileNotFoundError:
            return []
        except OSError as error:
            self.errors.append({"key": key, "error": str(error)})
            return []


def read_class(registry, progid):
    # All later keys are reached only from a selected SolidLink ProgID.
    current = registry.value(progid + r"\CurVer")
    clsid = registry.value(progid + r"\CLSID")
    if not clsid and current and current.lower().startswith(PROGID_PREFIX):
        clsid = registry.value(current + r"\CLSID")
    result = {"progid": progid, "curver": current, "clsid": clsid}
    if not clsid or not re.fullmatch(r"\{[0-9a-fA-F-]{36}\}", clsid):
        return result
    root = "CLSID\\" + clsid
    result["description"] = registry.value(root)
    result["servers"] = {}
    for name in ["LocalServer32", "InprocServer32"]:
        command = registry.value(root + "\\" + name)
        if command:
            result["servers"][name] = file_metadata(command)
    typelib = registry.value(root + r"\TypeLib")
    result["typelib_guid"] = typelib
    result["typelib_paths"] = []
    if typelib and re.fullmatch(r"\{[0-9a-fA-F-]{36}\}", typelib):
        libroot = "TypeLib\\" + typelib
        for version in registry.subkeys(libroot)[:32]:
            versionroot = libroot + "\\" + version
            for locale in registry.subkeys(versionroot)[:32]:
                if locale.lower() in {"flags", "helpdir"}:
                    continue
                for architecture in ["win64", "win32"]:
                    key = versionroot + "\\" + locale + "\\" + architecture
                    path = registry.value(key)
                    if path:
                        result["typelib_paths"].append({"key": key, **file_metadata(path)})
    return result


def collect(registry):
    names = sorted(name for name in registry.subkeys("")
                   if name.lower().startswith(PROGID_PREFIX))
    if len(names) > MAX_MATCHES:
        raise ValueError("Too many SolidLink class registrations")
    return {"classes": [read_class(registry, name) for name in names],
            "errors": registry.errors}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, help="New output directory; must not exist")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Run this diagnostic on Windows with ALPHACAM installed")
    import winreg
    out = args.out or Path(__file__).resolve().parent / (
        "solidlink_diag_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f"))
    out.mkdir(parents=True, exist_ok=False)
    report = {"schema": "solidlink-registration-readonly-v1",
              "captured_utc": datetime.now(timezone.utc).isoformat(),
              "python_pointer_size": struct.calcsize("P"),
              "native_code_loaded": False, "com_servers_activated": False,
              "conversion_attempted": False, "registry_modified": False,
              "analyzed_solidimport_sha256": EXPECTED_DLL_SHA256,
              "expected_default_progid": "SolidLink.Document.2025.3", "views": {}}
    try:
        for name, view in [("64bit", winreg.KEY_WOW64_64KEY),
                           ("32bit", winreg.KEY_WOW64_32KEY)]:
            report["views"][name] = collect(Registry(winreg, view))
    except Exception as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
    target = out / "solidlink_environment.json"
    with target.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print("Raport: " + str(target))
    count = sum(len(view["classes"]) for view in report["views"].values())
    print("Liczba rejestracji SolidLink (obie architektury): " + str(count))
    print("Przeslij solidlink_environment.json. Nie wykonano jeszcze konwersji STEP.")
    return 1 if "error" in report else 0


if __name__ == "__main__":
    raise SystemExit(main())
