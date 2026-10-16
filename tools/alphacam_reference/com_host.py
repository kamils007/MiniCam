"""Small diagnostic COM host for common ALPHACAM interfaces, 2025 probe.

Implements application/drawing/frame access, two UI state properties and a
stateful subset of layer APIs.
Other API methods terminate the disposable probe after logging their exact name.
Callbacks and object memory stay pinned until the DLL has been unloaded.
"""
import ctypes as C
import json
import os
from pathlib import Path
import uuid

PTR = C.c_void_p
HRESULT = C.c_int32
ULONG = C.c_uint32
SHORT = C.c_int16
LONG = C.c_int32
S_OK = 0
E_POINTER = C.c_int32(0x80004003).value
E_NOINTERFACE = C.c_int32(0x80004002).value
E_NOTIMPL = C.c_int32(0x80004001).value
DISP_E_UNKNOWNNAME = C.c_int32(0x80020006).value
DISP_E_BADINDEX = C.c_int32(0x8002000B).value
DISP_E_BADPARAMCOUNT = C.c_int32(0x8002000E).value
DISP_E_TYPEMISMATCH = C.c_int32(0x80020005).value
DISP_E_MEMBERNOTFOUND = C.c_int32(0x80020003).value
DISP_E_UNKNOWNINTERFACE = C.c_int32(0x80020001).value
IID_UNKNOWN = uuid.UUID("00000000-0000-0000-c000-000000000046").bytes_le
IID_DISPATCH = uuid.UUID("00020400-0000-0000-c000-000000000046").bytes_le
HOST_STOP_EXIT = 80
CALLBACK_ERROR_EXIT = 81


class VariantValue(C.Union):
    _fields_ = [("pointer", PTR), ("integer", LONG), ("short", SHORT),
                ("number", C.c_double), ("record", PTR * 2)]


class Variant(C.Structure):
    _fields_ = [("vt", C.c_uint16), ("reserved", C.c_uint16 * 3),
                ("value", VariantValue)]


class DispParams(C.Structure):
    _fields_ = [("args", PTR), ("named", PTR), ("count", ULONG), ("named_count", ULONG)]


class ComInstance(C.Structure):
    _fields_ = [("vtable", C.POINTER(PTR))]


def argument_ctype(desc):
    """Native parameter locations for this x64 TLB; opaque inputs are not read."""
    vt = desc["vt"]
    primitives = {2: SHORT, 3: LONG, 4: C.c_float, 5: C.c_double,
                  6: C.c_int64, 7: C.c_double, 10: LONG, 11: SHORT,
                  16: C.c_int8, 17: C.c_uint8, 18: C.c_uint16, 19: ULONG,
                  20: C.c_int64, 21: C.c_uint64, 22: LONG, 23: ULONG, 25: HRESULT}
    if vt in primitives:
        return primitives[vt]
    if vt == 29:
        # Every local, directly passed USERDEFINED parameter in these interfaces
        # was verified to be an enum. The imported reference is an interface.
        return PTR if desc["name"].startswith("imported_href_") else LONG
    if vt in (8, 9, 12, 13, 26, 27, 28, 30, 31):
        # VARIANT is 24 bytes: Windows x64 passes this aggregate indirectly.
        return PTR
    raise ValueError("Unsupported ABI descriptor: " + repr(desc))


class HostObject:
    def __init__(self, host, contract):
        self.host = host
        self.name = contract["name"]
        self.iid = uuid.UUID(contract["guid"]).bytes_le
        self.refs = 1  # Python owner keeps the object alive throughout the probe.
        self.methods = {f["vtable_offset_x64"]: f for f in contract["functions"]}
        self.names = {f["name"].casefold(): f["dispid"] for f in contract["functions"]}
        known_size = max(self.methods) // 8 + 1
        size = known_size + 64
        self.table = (PTR * size)()
        self.callbacks = []
        self.instance = ComInstance(C.cast(self.table, C.POINTER(PTR)))
        self.pointer = C.addressof(self.instance)
        standard = [
            (HRESULT, [PTR, PTR], self.query_interface),
            (ULONG, [], self.add_ref),
            (ULONG, [], self.release),
            (HRESULT, [PTR], self.get_typeinfo_count),
            (HRESULT, [ULONG, ULONG, PTR], self.get_typeinfo),
            (HRESULT, [PTR, PTR, ULONG, ULONG, PTR], self.get_ids),
            (HRESULT, [LONG, PTR, ULONG, C.c_uint16, PTR, PTR, PTR, PTR], self.invoke),
        ]
        for slot, (result, args, function) in enumerate(standard):
            self.install(slot, result, args, function)
        for offset, function in self.methods.items():
            if offset < 56 or offset % 8:
                raise ValueError("Unexpected x64 custom method offset")
            args = [argument_ctype(p["type"]) for p in function["parameters_native"]]

            def native(this, *values, spec=function):
                return self.handle(spec, values)

            self.install(offset // 8, HRESULT, args, native)
        # Reserve a diagnostic suffix for methods added after the available host
        # TLB. A Windows x64 callback using only 'this' ignores other argument
        # registers/stack slots and terminates before returning or reading them.
        # These guards provide no invented method name or signature.
        for slot in range(known_size, size):
            spec = {"name": "UNKNOWN_SLOT_" + hex(slot * 8), "dispid": None,
                    "invkind": 0, "vtable_offset_x64": slot * 8,
                    "metadata_available": False}

            def unknown(this, method=spec):
                return self.host.stop(self, method)

            self.install(slot, HRESULT, [], unknown)
        if any(not x for x in self.table):
            raise ValueError("A native COM vtable contains unassigned slots")

    def install(self, slot, result, arguments, function):
        def guarded(this, *values):
            try:
                if this != self.pointer:
                    raise RuntimeError("COM this pointer does not match its object")
                return function(this, *values)
            except BaseException as error:
                self.host.emit("host_callback_error", interface=self.name,
                               slot=slot, error_type=type(error).__name__, error=str(error))
                os._exit(CALLBACK_ERROR_EXIT)
        callback = self.host.callback_factory(result, PTR, *arguments)(guarded)
        self.callbacks.append(callback)
        self.table[slot] = C.cast(callback, PTR).value

    def query_interface(self, this, iid, output):
        if not iid or not output:
            return E_POINTER
        C.cast(output, C.POINTER(PTR))[0] = None
        value = C.string_at(iid, 16)
        accepted = value in (IID_UNKNOWN, IID_DISPATCH, self.iid)
        self.host.emit("host_query_interface", interface=self.name,
                       requested_iid=str(uuid.UUID(bytes_le=value)), supported=accepted)
        if not accepted:
            return E_NOINTERFACE
        C.cast(output, C.POINTER(PTR))[0] = self.pointer
        self.refs += 1
        return S_OK

    def add_ref(self, this):
        self.refs += 1
        self.host.emit("host_reference", interface=self.name, operation="AddRef", refs=self.refs)
        return self.refs

    def release(self, this):
        self.refs -= 1
        if self.refs < 1:
            raise RuntimeError("DLL released the host owner's reference")
        self.host.emit("host_reference", interface=self.name, operation="Release", refs=self.refs)
        return self.refs

    def get_typeinfo_count(self, this, output):
        if not output:
            return E_POINTER
        C.cast(output, C.POINTER(ULONG))[0] = 0
        self.host.emit("host_dispatch", interface=self.name, method="GetTypeInfoCount")
        return S_OK

    def get_typeinfo(self, this, index, locale, output):
        if not output:
            return E_POINTER
        C.cast(output, C.POINTER(PTR))[0] = None
        self.host.emit("host_dispatch", interface=self.name, method="GetTypeInfo", supported=False)
        return DISP_E_BADINDEX

    def get_ids(self, this, iid, names, count, locale, output):
        if not iid or not names or not output:
            return E_POINTER
        if C.string_at(iid, 16) != bytes(16):
            return DISP_E_UNKNOWNINTERFACE
        ids = C.cast(output, C.POINTER(LONG))
        for i in range(count):
            ids[i] = -1
        if count != 1:
            return DISP_E_UNKNOWNNAME
        name = C.cast(names, C.POINTER(C.c_wchar_p))[0]
        dispid = self.names.get((name or "").casefold())
        self.host.emit("host_dispatch", interface=self.name, method="GetIDsOfNames",
                       name=name, supported=dispid is not None)
        if dispid is None:
            return DISP_E_UNKNOWNNAME
        ids[0] = dispid
        return S_OK

    def invoke(self, this, dispid, iid, locale, flags, params, output, exception, arg_error):
        if not iid or not params:
            return E_POINTER
        if C.string_at(iid, 16) != bytes(16):
            return DISP_E_UNKNOWNINTERFACE
        spec = next((f for f in self.methods.values()
                     if f["dispid"] == dispid and flags & f["invkind"]), None)
        if spec is None:
            return DISP_E_MEMBERNOTFOUND
        if self.host.layers.handles(self, spec) or self.is_auxiliary(spec):
            from layer_host import invoke_layer
            return invoke_layer(self, spec, params, output, arg_error)
        arguments = C.cast(params, C.POINTER(DispParams)).contents
        is_get = spec["invkind"] == 2
        is_put = spec["invkind"] == 4
        expected = 0 if is_get else 1 if is_put else None
        if expected is None or not self.is_implemented(spec):
            return self.host.stop(self, spec)
        if arguments.count != expected:
            return DISP_E_BADPARAMCOUNT
        if is_get:
            if not output:
                return E_POINTER
            result = C.cast(output, C.POINTER(Variant)).contents
            C.memset(output, 0, C.sizeof(Variant))
            if self.name == "IAlphaCamApp" and spec["name"] in self.host.app_values:
                value = self.host.app_values[spec["name"]]
                if spec["name"] == "SpecialKey":
                    result.vt = 3  # LONG, not the I2 used by ProgramLetter.
                    result.value.integer = value
                else:
                    result.vt = 11 if isinstance(value, bool) else 2
                    result.value.short = (-1 if value else 0) if isinstance(value, bool) else value
            elif self.name == "IAlphaCamApp":
                target = self.host.objects["IDrawing" if spec["name"] == "ActiveDrawing" else "IFrame"]
                target.refs += 1
                result.vt = 9
                result.value.pointer = target.pointer
            else:
                result.vt = 11
                result.value.short = -1 if self.host.states[spec["name"]] else 0
            self.host.emit("host_method", interface=self.name, method=spec["name"],
                           operation="PROPERTYGET", route="IDispatch", implemented=True)
            return S_OK
        if not arguments.args:
            return E_POINTER
        argument = C.cast(arguments.args, C.POINTER(Variant)).contents
        if argument.vt != 11:
            if arg_error:
                C.cast(arg_error, C.POINTER(ULONG))[0] = 0
            return DISP_E_TYPEMISMATCH
        return self.handle(spec, (argument.value.short,))

    def is_auxiliary(self, spec):
        return spec["invkind"] == 1 and (
            (self.name == "IAlphaCamApp" and spec["name"] == "SetUndoCommandName") or
            (self.name == "IDrawing" and spec["name"] in ("Redraw", "RedrawShadedViews", "ZoomAll")))

    def auxiliary(self, spec, arguments):
        state = self.host.aux_state
        name = spec["name"]
        if name == "SetUndoCommandName":
            # Copy the BSTR value: the caller retains ownership of the input.
            state["undo_command_name"] = self.host.layers.bstrs.read(arguments[0])
        else:
            # This host owns no display views. Refreshing that empty set is a
            # defined operation; do not report a render when views do exist.
            if state["view_count"] != 0:
                return self.host.stop(self, spec)
            counter = {"Redraw": "redraw_requests", "RedrawShadedViews": "shaded_redraw_requests",
                       "ZoomAll": "zoom_all_requests"}[name]
            state[counter] += 1
        self.host.emit("host_aux_state", state=dict(state))
        self.host.emit("host_method", interface=self.name, method=name, operation="METHOD",
                       vtable_offset_x64=hex(spec["vtable_offset_x64"]), implemented=True,
                       **({"views_refreshed": 0} if name != "SetUndoCommandName" else {}))
        return S_OK

    def is_implemented(self, spec):
        if self.is_auxiliary(spec):
            return True
        if self.host.layers.handles(self, spec):
            return True
        if self.name == "IAlphaCamApp":
            if spec["name"] == "SpecialKey":
                return "SpecialKey" in self.host.app_values and spec["invkind"] == 2
            if spec["name"] == "DisableUndo":
                return spec["invkind"] in (2, 4)
            return spec["name"] in ("ActiveDrawing", "Frame", "ProgramLetter", "InAPICall") and spec["invkind"] == 2
        names = ("ScreenUpdating", "ThreeDViews") if self.name == "IDrawing" else ("ProjectBarUpdating",)
        return spec["name"] in names and spec["invkind"] in (2, 4)

    def handle(self, spec, arguments):
        if not self.is_implemented(spec):
            return self.host.stop(self, spec)
        if self.is_auxiliary(spec):
            return self.auxiliary(spec, arguments)
        if self.host.layers.handles(self, spec):
            return self.host.layers.handle(self, spec, arguments)
        details = {"interface": self.name, "method": spec["name"],
                   "operation": {2: "PROPERTYGET", 4: "PROPERTYPUT"}[spec["invkind"]],
                   "vtable_offset_x64": hex(spec["vtable_offset_x64"]), "implemented": True}
        if spec["invkind"] == 2:
            if not arguments[0]:
                return E_POINTER
            if self.name == "IAlphaCamApp" and spec["name"] in self.host.app_values:
                value = self.host.app_values[spec["name"]]
                native = (-1 if value else 0) if isinstance(value, bool) else value
                output_type = LONG if spec["name"] == "SpecialKey" else SHORT
                C.cast(arguments[0], C.POINTER(output_type))[0] = native
                details["value"] = value
            elif self.name == "IAlphaCamApp":
                target = self.host.objects["IDrawing" if spec["name"] == "ActiveDrawing" else "IFrame"]
                target.refs += 1
                C.cast(arguments[0], C.POINTER(PTR))[0] = target.pointer
                details["returns_interface"] = target.name
            else:
                value = self.host.states[spec["name"]]
                C.cast(arguments[0], C.POINTER(SHORT))[0] = -1 if value else 0
                details["value"] = value
        else:
            value = arguments[0] != 0
            if self.name == "IAlphaCamApp" and spec["name"] == "DisableUndo":
                # This disposable batch host has no undo history. Disabling an
                # already disabled service is valid; enabling it is unsupported.
                if not value:
                    self.host.emit("host_undo_enable_unsupported", requested_disable=False)
                    return self.host.stop(self, spec)
                details.update(value=True, policy="batch_host_has_no_undo_history")
                self.host.emit("host_method", **details)
                return S_OK
            self.host.states[spec["name"]] = value
            details["value"] = value
            self.host.emit("host_display_state", state=dict(self.host.states))
        self.host.emit("host_method", **details)
        return S_OK


class MinimalHost:
    def __init__(self, contract_path, logger, callback_factory=None, abort=None, bstrs=None,
                 special_key=None):
        if C.sizeof(PTR) != 8 or C.sizeof(Variant) != 24 or C.sizeof(DispParams) != 24:
            raise RuntimeError("The host requires x64 COM layouts")
        self.callback_factory = callback_factory or getattr(C, "WINFUNCTYPE", None)
        if self.callback_factory is None:
            raise RuntimeError("Windows x64 is required for the native probe")
        self.logger = logger
        self.abort = abort
        self.phase = "create"
        self.sequence = 0
        self.stop_event = None
        self.states = {"ScreenUpdating": True, "ProjectBarUpdating": True, "ThreeDViews": False}
        # The requested output is a Router drawing (.ard). This host performs
        # only API-driven operations, so InAPICall is true when the DLL asks.
        self.app_values = {"ProgramLetter": ord("R"), "InAPICall": True,
                           "DisableUndo": True}
        # Preserve the actual classification read from the user's running app.
        # Without a snapshot, leave the getter unimplemented; never guess 0.
        if special_key is not None:
            if type(special_key) is not int or not -(2 ** 31) <= special_key < 2 ** 31:
                raise ValueError("SpecialKey must be a signed 32-bit integer")
            self.app_values["SpecialKey"] = special_key
        self.aux_state = {"undo_command_name": "", "view_count": 0,
                          "redraw_requests": 0, "shaded_redraw_requests": 0, "zoom_all_requests": 0}
        contract = json.loads(Path(contract_path).read_text(encoding="utf-8"))
        if contract["pointer_size"] != 8:
            raise ValueError("Wrong COM contract pointer size")
        contracts = {t["name"]: t for t in contract["types"]}
        self.objects = {name: HostObject(self, contracts[name]) for name in
                        ("IAlphaCamApp", "IDrawing", "IFrame")}
        from layer_host import LayerHost
        self.layers = LayerHost(self, contracts, HostObject, bstrs)
        self.app_pointer = self.objects["IAlphaCamApp"].pointer

    def emit(self, event, **details):
        self.sequence += 1
        self.logger(event, sequence=self.sequence, host_phase=self.phase, **details)
        if self.sequence >= 4096:
            self.logger("host_event_limit", limit=4096)
            os._exit(82)

    def stop(self, obj, spec):
        self.stop_event = {"interface": obj.name, "method": spec["name"],
                           "invkind": spec["invkind"], "dispid": spec["dispid"],
                           "vtable_offset_x64": hex(spec["vtable_offset_x64"]),
                           "metadata_available": spec.get("metadata_available", True)}
        self.layers.changed()
        self.emit("host_unimplemented", **self.stop_event)
        # Do not return made-up success or allow an unsupported call to proceed.
        # The parent still writes a report from flushed stdout and JSONL events.
        if self.abort is None:
            os._exit(HOST_STOP_EXIT)
        self.abort(self.stop_event)
        return E_NOTIMPL

    def call(self, obj_name, offset, result, argument_types, *values):
        obj = self.objects[obj_name]
        address = obj.table[offset // 8]
        return self.callback_factory(result, PTR, *argument_types)(address)(obj.pointer, *values)

    def self_test(self):
        """Exercise actual callback pointers, object identities and state updates."""
        self.phase = "selftest"
        first = PTR(123)
        count = LONG(-1)
        assert self.call("IDrawing", 0x160, HRESULT, [PTR], C.byref(first)) == S_OK
        assert first.value is None
        assert self.call("IDrawing", 0x168, HRESULT, [PTR], C.byref(count)) == S_OK
        assert count.value == 0
        for offset, expected in [(0x110, ord("R")), (0x360, -1), (0x2B8, -1)]:
            value = SHORT(0)
            assert self.call("IAlphaCamApp", offset, HRESULT, [PTR], C.byref(value)) == S_OK
            assert value.value == expected
        assert self.call("IAlphaCamApp", 0x210, HRESULT, [SHORT], -1) == S_OK
        if "SpecialKey" in self.app_values:
            # Canaries check both the output width and the value on Windows.
            words = (ULONG * 3)(0x12345678, 0xDEADBEEF, 0x87654321)
            assert self.call("IAlphaCamApp", 0x1C8, HRESULT, [PTR], C.byref(words, 4)) == S_OK
            assert list(words) == [0x12345678, self.app_values["SpecialKey"] & 0xFFFFFFFF, 0x87654321]
        for name, offset, target in [("IAlphaCamApp", 0x48, "IDrawing"),
                                      ("IAlphaCamApp", 0x70, "IFrame")]:
            out = PTR()
            assert self.call(name, offset, HRESULT, [PTR], C.byref(out)) == S_OK
            assert out.value == self.objects[target].pointer
            self.call(target, 16, ULONG, [])
        for name, get, put, state in [("IDrawing", 0x408, 0x410, "ScreenUpdating"),
                                      ("IFrame", 0x158, 0x110, "ProjectBarUpdating"),
                                      ("IDrawing", 0x50, 0x58, "ThreeDViews")]:
            for value in ((False, True, False) if state == "ThreeDViews" else (False, True)):
                assert self.call(name, put, HRESULT, [SHORT], -1 if value else 0) == S_OK
                out = SHORT(123)
                assert self.call(name, get, HRESULT, [PTR], C.byref(out)) == S_OK
                assert out.value == (-1 if value else 0)
                assert self.states[state] is value
        for name, obj in self.objects.items():
            before = obj.refs
            iid = (C.c_ubyte * 16).from_buffer_copy(IID_UNKNOWN)
            out = PTR()
            assert self.call(name, 0, HRESULT, [PTR, PTR], C.byref(iid), C.byref(out)) == S_OK
            assert out.value == obj.pointer and obj.refs == before + 1
            self.call(name, 16, ULONG, [])
            assert obj.refs == before
        self.phase = "ready"
        return {"ok": True, "object_count": len(self.objects),
                "metadata_methods": sum(len(o.methods) for o in self.objects.values()),
                "native_property_callbacks_checked": True, "IUnknown_identity_checked": True,
                "native_empty_layer_callbacks_checked": True}
