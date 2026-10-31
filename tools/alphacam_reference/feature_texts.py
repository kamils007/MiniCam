"""UI text table adapter for the exact Feature.dll 2025.3.0.200 sample.

Reproduces only the CString table layout observed at InitAlphacamAddIn+0x903.
It does not execute an initializer fragment, change code, or supply other DLL
configuration. The original uninitialized table pointer is saved and restored.
CString storage is initialized and destroyed by the DLL's imported MFC routines.
The owner must keep this object alive until all Feature COM objects are released,
then close it before unloading Feature.dll. Full InitAlphacamAddIn must not run
while this adapter is installed (that initializer owns a different allocator).
"""
import ast
import ctypes as C
import hashlib
from pathlib import Path
import re

DLL_SHA256 = "4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec"
PO_SHA256 = "5ebfd4fedec46583fa72fd2e95862a0b3fbe7eb41b6a9e49ae12895176130776"
TABLE_POINTER_RVA = 0x186548
# InitAlphacamAddIn: 25 groups read from Feature.po, followed by seven
# additional strings from acam.po/acam.ctx. Array headers contain uint64 counts;
# each CString object is one pointer (eight bytes), not a wchar_t buffer.
COUNTS = (25, 6, 8, 31, 36, 34, 162, 19, 83, 10, 7, 29, 5,
          7, 17, 19, 9, 6, 7, 2, 11, 10, 25, 2, 6, 7)
IAT_RVAS = {"construct": 0x13D068, "assign": 0x13D8B0,
            "get": 0x13D060, "destroy": 0x13D070}


def parse_po(text):
    """Read singular gettext entries, preserving Unicode and escaped newlines."""
    entries, fields, current = {}, {}, None

    def finish():
        if not fields:
            return
        if "msgid" not in fields or "msgstr" not in fields:
            raise ValueError("Incomplete gettext entry")
        key = (fields.get("msgctxt"), fields["msgid"])
        if key in entries:
            raise ValueError("Duplicate gettext entry: " + repr(key))
        entries[key] = fields["msgstr"]

    for number, raw in enumerate(text.lstrip("\ufeff").splitlines() + [""], 1):
        line = raw.strip()
        if not line:
            finish()
            fields, current = {}, None
            continue
        if line.startswith("#"):
            continue
        match = re.fullmatch(r'(msgctxt|msgid|msgstr)\s+(".*")', line)
        if match:
            current, literal = match.groups()
            if current in fields:
                raise ValueError("Repeated gettext field on line " + str(number))
            fields[current] = ""
        elif line.startswith('"') and current:
            literal = line
        else:
            raise ValueError("Unsupported gettext syntax on line " + str(number))
        value = ast.literal_eval(literal)
        if not isinstance(value, str) or "\0" in value or len(value) > 32768:
            raise ValueError("Invalid gettext string on line " + str(number))
        fields[current] += value
    return entries


class Catalog:
    def __init__(self, path):
        path = Path(path)
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != PO_SHA256:
            raise ValueError("Feature.po differs from the analyzed, supplied catalog")
        self.entries = parse_po(data.decode("utf-8-sig"))
        self.groups = []
        self.missing_feature_ids = []
        # The last seven strings belong to other catalogs, which have not been
        # supplied. Keep them valid empty CString objects and report that limit.
        for group, count in enumerate(COUNTS):
            values = []
            for index in range(1, count + 1):
                ident = "%d-%d" % ((group + 1) * 10, index)
                value = self.entries.get(("feature|", ident), "")
                if group < 25 and ("feature|", ident) not in self.entries:
                    self.missing_feature_ids.append(ident)
                values.append(value)
            self.groups.append(values)
        if not self.groups[7][5]:
            raise ValueError("Required progress text Feature.po 80-6 is missing")
        self.info = {"filename": path.name, "sha256": PO_SHA256,
                     "language": "pl-PL", "table_count": len(COUNTS),
                     "cstring_count": sum(COUNTS),
                     "missing_feature_ids": self.missing_feature_ids,
                     "external_context_strings_empty": 7,
                     "progress_text_id": "80-6", "progress_text": self.groups[7][5]}


class MFCStrings:
    def __init__(self, module_base):
        factory = C.WINFUNCTYPE
        specifications = {"construct": (None, C.c_void_p),
                          "assign": (C.c_void_p, C.c_void_p, C.c_wchar_p),
                          "get": (C.c_void_p, C.c_void_p),
                          "destroy": (None, C.c_void_p)}
        for name, rva in IAT_RVAS.items():
            address = C.c_void_p.from_address(module_base + rva).value
            if not address:
                raise RuntimeError("Unresolved MFC string import: " + name)
            setattr(self, name, factory(*specifications[name])(address))

    def read(self, address):
        pointer = self.get(address)
        if not pointer:
            raise RuntimeError("MFC CString returned a null text pointer")
        return C.wstring_at(pointer)


class NativeTextTable:
    def __init__(self, module_base, catalog, verified_dll_sha256, functions=None):
        if verified_dll_sha256 != DLL_SHA256 or C.sizeof(C.c_void_p) != 8:
            raise ValueError("Text table adapter requires the verified x64 DLL build")
        self.catalog = catalog
        self.functions = functions if functions is not None else MFCStrings(module_base)
        self.cell = C.c_void_p.from_address(module_base + TABLE_POINTER_RVA)
        self.previous_pointer = self.cell.value
        self.pointers = (C.c_void_p * len(COUNTS))()
        self.buffers = []
        self.constructed = []
        self.installed = False
        self.closed = False

    def address(self, group, index):
        if not 0 <= group < len(COUNTS) or not 0 <= index < COUNTS[group]:
            raise IndexError("String index is outside the verified table")
        return int(self.pointers[group]) + index * 8

    def install(self):
        if self.installed or self.closed:
            raise RuntimeError("Text table adapter cannot be reused")
        try:
            for group, values in enumerate(self.catalog.groups):
                # Same count header as the DLL initializer, but Python owns the
                # array allocation. Never pass the array to native delete[].
                buffer = (C.c_uint64 * (len(values) + 1))()
                buffer[0] = len(values)
                self.buffers.append(buffer)
                self.pointers[group] = C.addressof(buffer) + 8
                for index, text in enumerate(values):
                    address = self.address(group, index)
                    self.functions.construct(address)
                    self.constructed.append(address)
                    self.functions.assign(address, text)
                    if self.functions.read(address) != text:
                        raise RuntimeError("Native CString verification failed at %d-%d" %
                                           ((group + 1) * 10, index + 1))
            # Publish only after all 583 objects and their contents are verified.
            if self.cell.value != self.previous_pointer:
                raise RuntimeError("Feature text table changed during construction")
            self.cell.value = C.addressof(self.pointers)
            self.installed = True
            return {**self.catalog.info, "ok": True,
                    "all_native_strings_verified": True,
                    "table_pointer_rva": hex(TABLE_POINTER_RVA),
                    "original_pointer_saved": True,
                    "full_addin_initializer_called": False}
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.closed:
            return
        if self.installed:
            if self.cell.value != C.addressof(self.pointers):
                raise RuntimeError("Feature replaced the adapter's text table; storage remains pinned")
            self.cell.value = self.previous_pointer
            self.installed = False
        # Called while MFC is still loaded, after all Feature COM references
        # have been released. Individual CString destructors own their data.
        while self.constructed:
            self.functions.destroy(self.constructed[-1])
            self.constructed.pop()
        self.closed = True
