"""Read Windows COM type information; fixed-width x64 OAIDL layouts."""
import ctypes as C
import uuid

PTR = C.c_void_p
LONG = C.c_int32
ULONG = C.c_uint32
WORD = C.c_uint16
SHORT = C.c_int16


class TYPEVALUE(C.Union):
    _fields_ = [("pointer", PTR), ("href", ULONG)]


class TYPEDESC(C.Structure):
    _fields_ = [("value", TYPEVALUE), ("vt", WORD)]


class PARAMDESC(C.Structure):
    _fields_ = [("extra", PTR), ("flags", WORD)]


class ELEMDESC(C.Structure):
    _fields_ = [("type", TYPEDESC), ("param", PARAMDESC)]


class TYPEATTR(C.Structure):
    _fields_ = [("guid", C.c_ubyte * 16), ("lcid", ULONG), ("reserved", ULONG),
                ("constructor", LONG), ("destructor", LONG), ("schema", PTR),
                ("instance_size", ULONG), ("kind", LONG), ("functions", WORD),
                ("variables", WORD), ("implemented", WORD), ("vtable_size", WORD),
                ("alignment", WORD), ("flags", WORD), ("major", WORD), ("minor", WORD),
                ("alias", TYPEDESC), ("idl", PARAMDESC)]


class FUNCDESC(C.Structure):
    _fields_ = [("memid", LONG), ("scodes", PTR), ("params", C.POINTER(ELEMDESC)),
                ("kind", LONG), ("invkind", LONG), ("callconv", LONG),
                ("param_count", SHORT), ("optional_count", SHORT),
                ("vtable_offset", SHORT), ("scode_count", SHORT),
                ("result", ELEMDESC), ("flags", WORD)]


class VARDESC(C.Structure):
    _fields_ = [("memid", LONG), ("schema", PTR), ("value", PTR),
                ("element", ELEMDESC), ("flags", WORD), ("kind", LONG)]


def check_layouts():
    assert C.sizeof(PTR) == 8
    assert C.sizeof(TYPEDESC) == 16
    assert C.sizeof(ELEMDESC) == 32
    assert C.sizeof(TYPEATTR) == 96
    assert C.sizeof(FUNCDESC) == 88
    assert C.sizeof(VARDESC) == 64
    assert FUNCDESC.result.offset == 48
    assert TYPEATTR.functions.offset == 48


def numeric_variant(value):
    if value.vt == 2:
        return value.value.short
    if value.vt in (3, 22):
        return value.value.integer
    if value.vt in (19, 23):
        return value.value.unsigned
    return None


class Inspector:
    def __init__(self, method, release, automation, variant_type):
        check_layouts()
        self.method = method
        self.release = release
        self.automation = automation
        self.variant_type = variant_type

    @staticmethod
    def checked(hr, operation):
        if hr < 0:
            raise RuntimeError(operation + " failed: 0x%08X" % (hr & 0xFFFFFFFF))

    def documentation(self, info, memid=-1):
        name, doc, helpfile = PTR(), PTR(), PTR()
        context = ULONG()
        try:
            hr = self.method(info, 12, LONG, LONG, C.POINTER(PTR), C.POINTER(PTR),
                             C.POINTER(ULONG), C.POINTER(PTR))(
                                 info, memid, C.byref(name), C.byref(doc),
                                 C.byref(context), C.byref(helpfile))
            self.checked(hr, "ITypeInfo.GetDocumentation")
            return {"name": C.wstring_at(name) if name.value else "",
                    "doc": C.wstring_at(doc) if doc.value else ""}
        finally:
            for pointer in [name, doc, helpfile]:
                if pointer.value:
                    self.automation.SysFreeString(pointer)

    def attributes(self, info):
        pointer = PTR()
        hr = self.method(info, 3, LONG, C.POINTER(PTR))(info, C.byref(pointer))
        self.checked(hr, "ITypeInfo.GetTypeAttr")
        if not pointer.value:
            raise RuntimeError("Null TYPEATTR")
        try:
            attr = C.cast(pointer, C.POINTER(TYPEATTR)).contents
            result = {"kind": attr.kind, "function_count": attr.functions,
                      "variable_count": attr.variables, "vtable_size": attr.vtable_size,
                      "guid": str(uuid.UUID(bytes_le=bytes(attr.guid)))}
            if attr.kind == 6:
                result["alias"] = self.type_desc(info, attr.alias, 1)
            return result
        finally:
            self.method(info, 19, None, PTR)(info, pointer)

    def enum_values(self, info, count):
        if not 0 <= count <= 256:
            raise RuntimeError("Unexpected enum size")
        values = {}
        for index in range(count):
            pointer = PTR()
            hr = self.method(info, 6, LONG, ULONG, C.POINTER(PTR))(
                info, index, C.byref(pointer))
            self.checked(hr, "ITypeInfo.GetVarDesc")
            try:
                desc = C.cast(pointer, C.POINTER(VARDESC)).contents
                if desc.kind == 2 and desc.value:
                    variant = C.cast(desc.value, C.POINTER(self.variant_type)).contents
                    number = numeric_variant(variant)
                    if number is not None:
                        values[self.documentation(info, desc.memid)["name"]] = number
            finally:
                self.method(info, 21, None, PTR)(info, pointer)
        return values

    def type_desc(self, info, desc, depth=0):
        result = {"vt": int(desc.vt)}
        if depth > 5:
            result["limit"] = True
        elif desc.vt in (26, 27) and desc.value.pointer:
            inner = C.cast(desc.value.pointer, C.POINTER(TYPEDESC)).contents
            result["element"] = self.type_desc(info, inner, depth + 1)
        elif desc.vt == 29:
            reference = PTR()
            hr = self.method(info, 14, LONG, ULONG, C.POINTER(PTR))(
                info, desc.value.href, C.byref(reference))
            self.checked(hr, "ITypeInfo.GetRefTypeInfo")
            try:
                result.update(self.documentation(reference))
                attributes = self.attributes(reference)
                result["kind"] = attributes["kind"]
                if attributes["kind"] == 0:
                    result["enum_values"] = self.enum_values(reference, attributes["variable_count"])
                elif attributes["kind"] == 6:
                    result["alias"] = attributes.get("alias")
            finally:
                self.release(reference)
        return result

    def names(self, info, memid, maximum):
        pointers = (PTR * maximum)()
        count = ULONG()
        try:
            hr = self.method(info, 7, LONG, LONG, C.POINTER(PTR), ULONG, C.POINTER(ULONG))(
                info, memid, pointers, maximum, C.byref(count))
            self.checked(hr, "ITypeInfo.GetNames")
            if count.value > maximum:
                raise RuntimeError("Invalid name count")
            return [C.wstring_at(pointers[i]) if pointers[i] else "" for i in range(count.value)]
        finally:
            for pointer in pointers:
                if pointer:
                    self.automation.SysFreeString(pointer)

    def describe(self, dispatch, resolve_methods=None):
        info = PTR()
        hr = self.method(dispatch, 4, LONG, ULONG, ULONG, C.POINTER(PTR))(
            dispatch, 0, 0, C.byref(info))
        self.checked(hr, "IDispatch.GetTypeInfo")
        try:
            result = self.documentation(info)
            result.update(self.attributes(info))
            if not 0 <= result["function_count"] <= 256:
                raise RuntimeError("Unexpected function count")
            functions = []
            for index in range(result["function_count"]):
                pointer = PTR()
                hr = self.method(info, 5, LONG, ULONG, C.POINTER(PTR))(
                    info, index, C.byref(pointer))
                self.checked(hr, "ITypeInfo.GetFuncDesc")
                try:
                    desc = C.cast(pointer, C.POINTER(FUNCDESC)).contents
                    if not 0 <= desc.param_count <= 32:
                        raise RuntimeError("Unexpected parameter count")
                    function = self.documentation(info, desc.memid)
                    function.update({"dispid": desc.memid, "invkind": desc.invkind,
                                     "vtable_offset": desc.vtable_offset,
                                     "parameter_count": desc.param_count, "parameters": []})
                    # Resolve references only for the operations used by this
                    # probe; unrelated methods may refer to other type libraries.
                    if function["name"].lower() in ({"import", "importas", "bodies", "save", "count"}
                                                   | set(resolve_methods or ())):
                        names = self.names(info, desc.memid, desc.param_count + 1)
                        function["return"] = self.type_desc(info, desc.result.type)
                        for i in range(desc.param_count):
                            param = desc.params[i]
                            function["parameters"].append({
                                "name": names[i + 1] if i + 1 < len(names) else "arg%d" % i,
                                "flags": param.param.flags, "type": self.type_desc(info, param.type)})
                    functions.append(function)
                finally:
                    self.method(info, 20, None, PTR)(info, pointer)
            result["functions"] = functions
            return result
        finally:
            self.release(info)


def get_input_parameters(function):
    return [p for p in function["parameters"] if not p["flags"] & 8]


def resolve_import_contract(interface):
    for function in interface["functions"]:
        if function["name"].lower() != "importas" or function["invkind"] != 1:
            continue
        parameters = get_input_parameters(function)
        if len(parameters) != 3 or any(p["flags"] & 2 for p in parameters):
            continue
        if parameters[1]["type"]["vt"] != 8:
            continue
        file_enum = parameters[0]["type"]
        if file_enum.get("name") != "FeatureFileTypeEnum":
            continue
        file_key = "FeatureParasolidFile"
        if file_key not in file_enum.get("enum_values", {}):
            continue
        enum = parameters[2]["type"]
        if enum.get("name") != "FeatureImportEnum":
            continue
        values = enum.get("enum_values", {})
        key = "FeatureImportVisibleSolidModel"
        if key in values:
            return {"method": function["name"], "mode_name": key,
                    "mode_value": values[key], "file_type_name": file_key,
                    "file_type_value": file_enum["enum_values"][file_key],
                    "descriptor": function}
    return None


def valid_save_contract(interface):
    for function in interface["functions"]:
        if function["name"].lower() == "save" and function["invkind"] == 1:
            parameters = get_input_parameters(function)
            if len(parameters) == 1 and not parameters[0]["flags"] & 2:
                if parameters[0]["type"]["vt"] == 8:
                    return True
    return False
