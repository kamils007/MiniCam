#!/usr/bin/env python3
"""Isolated Windows x64 Feature.dll 2025 probe with a minimal COM host.

The original CLI defaults to the reviewed 2025.3 build. Test 14.1 explicitly
selects the separately reviewed 2025.4 profile for every native adapter.

Supplies application, drawing and frame objects using the existing ObjectNumberOf
context argument. Logs every host callback and stops at an unimplemented method.
The full add-in initializer is not called. A separate adapter initializes only
the in-process UI text table with native MFC CString objects from Feature.po.
The DLL file and executable code stay unchanged. The model stage uses a private
input copy and may create roundtrip.x_b. No ARD is written.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
import shutil
from pathlib import Path
import struct
import subprocess
import sys
from datetime import datetime
from feature_builds import profile


EXPECTED_SHA256 = "4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec"
EXPECTED_EXPORT_RVAS = {"GetUIVersion": 0x38A10, "GetSolidFeatures": 0x389C0,
                        "InitAlphacamAddIn": 0x38B50, "ObjectDoWrite2": 0x40050,
                        "ObjectQueryWrite2": 0x41370,
                        "ObjectNumberOf": 0x40CE0}


def inspect_pe(path, expected_sha256=EXPECTED_SHA256):
    expected_exports = profile(expected_sha256)['exports']
    data = path.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("This is not a PE file.")
    u16 = lambda p: struct.unpack_from("<H", data, p)[0]
    u32 = lambda p: struct.unpack_from("<I", data, p)[0]
    pe = u32(0x3C)
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid PE signature.")
    coff = pe + 4
    optional = coff + 20
    if u16(coff) != 0x8664 or u16(optional) != 0x20B:
        raise ValueError("An x64 PE32+ DLL is required.")
    sections = []
    for i in range(u16(coff + 2)):
        p = optional + u16(coff + 16) + 40 * i
        sections.append((u32(p + 12), max(u32(p + 8), u32(p + 16)), u32(p + 20)))

    def offset(rva):
        for start, size, raw in sections:
            if start <= rva < start + size:
                return raw + rva - start
        raise ValueError("RVA outside sections: " + hex(rva))

    directory = offset(u32(optional + 112))
    function_table = offset(u32(directory + 28))
    name_table = offset(u32(directory + 32))
    ordinal_table = offset(u32(directory + 36))
    exports = {}
    for i in range(u32(directory + 24)):
        p = offset(u32(name_table + 4 * i))
        name = data[p:data.index(b"\0", p)].decode("ascii")
        ordinal_index = u16(ordinal_table + 2 * i)
        exports[name] = u32(function_table + 4 * ordinal_index)
    result = {"filename": path.name, "size_bytes": len(data),
              "sha256": hashlib.sha256(data).hexdigest(),
              "machine": "AMD64", "export_count": len(exports),
              "selected_exports": {k: hex(exports[k]) for k in expected_exports if k in exports}}
    result["matches_analyzed_build"] = (
        result["sha256"] == expected_sha256
        and all(exports.get(k) == v for k, v in expected_exports.items())
    )
    return result


def acam_processes():
    """List only Acam.exe processes, without exposing unrelated inventory."""
    kernel = C.WinDLL("kernel32", use_last_error=True)

    class PROCESSENTRY32W(C.Structure):
        _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD), ("th32ProcessID", W.DWORD),
                    ("th32DefaultHeapID", C.c_size_t), ("th32ModuleID", W.DWORD),
                    ("cntThreads", W.DWORD), ("th32ParentProcessID", W.DWORD),
                    ("pcPriClassBase", W.LONG), ("dwFlags", W.DWORD),
                    ("szExeFile", W.WCHAR * 260)]

    kernel.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = W.HANDLE
    kernel.Process32FirstW.argtypes = [W.HANDLE, C.POINTER(PROCESSENTRY32W)]
    kernel.Process32FirstW.restype = W.BOOL
    kernel.Process32NextW.argtypes = kernel.Process32FirstW.argtypes
    kernel.Process32NextW.restype = W.BOOL
    kernel.CloseHandle.argtypes = [W.HANDLE]
    kernel.CloseHandle.restype = W.BOOL
    handle = kernel.CreateToolhelp32Snapshot(2, 0)
    if handle == C.c_void_p(-1).value:
        raise C.WinError(C.get_last_error())
    found = []
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = C.sizeof(entry)
        more = kernel.Process32FirstW(handle, C.byref(entry))
        while more:
            if entry.szExeFile.lower() == "acam.exe":
                found.append({"pid": entry.th32ProcessID, "name": entry.szExeFile})
            more = kernel.Process32NextW(handle, C.byref(entry))
    finally:
        kernel.CloseHandle(handle)
    return found


def worker(path, stage, dependency_dirs, model_path=None, output_dir=None, host_environment=None, batch_profile=None,
           build_sha256=EXPECTED_SHA256):
    """All native interaction is contained here, in a child process."""
    import uuid
    build = profile(build_sha256)
    if not inspect_pe(path, build_sha256)['matches_analyzed_build']:
        raise ValueError('Feature.dll does not match the selected reviewed layout')
    HRESULT = C.c_int32
    ULONG = C.c_uint32
    UINT = C.c_uint32
    LONG = C.c_int32
    WORD = C.c_uint16
    PTR = C.c_void_p

    class GUID(C.Structure):
        _fields_ = [("Data1", C.c_uint32), ("Data2", C.c_uint16),
                    ("Data3", C.c_uint16), ("Data4", C.c_ubyte * 8)]

        @classmethod
        def from_text(cls, value):
            return cls.from_buffer_copy(uuid.UUID(value).bytes_le)

    class RECORD(C.Structure):
        _fields_ = [("record", PTR), ("info", PTR)]

    class VALUE(C.Union):
        _fields_ = [("pointer", PTR), ("integer", LONG), ("short", C.c_int16),
                    ("unsigned", ULONG), ("int64", C.c_int64),
                    ("number", C.c_double), ("record", RECORD)]

    class VARIANT(C.Structure):
        _fields_ = [("vt", WORD), ("r1", WORD), ("r2", WORD), ("r3", WORD), ("value", VALUE)]

    class DISPPARAMS(C.Structure):
        _fields_ = [("args", PTR), ("named", PTR), ("count", UINT), ("named_count", UINT)]

    class EXCEPINFO(C.Structure):
        _fields_ = [("code", WORD), ("reserved", WORD), ("source", PTR),
                    ("description", PTR), ("helpfile", PTR), ("helpcontext", ULONG),
                    ("reserved_ptr", PTR), ("deferred", PTR), ("scode", LONG)]

    assert C.sizeof(VARIANT) == 24
    assert C.sizeof(EXCEPINFO) == 64
    assert EXCEPINFO.deferred.offset == 48 and EXCEPINFO.scode.offset == 56
    null_iid = GUID()
    dispatch_iid = GUID.from_text("00020400-0000-0000-c000-000000000046")
    ole = C.WinDLL("ole32")
    ole.CoInitializeEx.argtypes = [PTR, ULONG]
    ole.CoInitializeEx.restype = HRESULT
    ole.CoUninitialize.argtypes = []
    ole.CoUninitialize.restype = None
    automation = C.WinDLL("oleaut32")
    automation.VariantClear.argtypes = [C.POINTER(VARIANT)]
    automation.VariantClear.restype = HRESULT
    automation.SysFreeString.argtypes = [PTR]
    automation.SysFreeString.restype = None
    automation.SysAllocString.argtypes = [W.LPCWSTR]
    automation.SysAllocString.restype = PTR
    automation.GetErrorInfo.argtypes = [ULONG, C.POINTER(PTR)]
    automation.GetErrorInfo.restype = HRESULT
    automation.SetErrorInfo.argtypes = [ULONG, PTR]
    automation.SetErrorInfo.restype = HRESULT
    kernel = C.WinDLL("kernel32", use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [W.LPCWSTR, W.HANDLE, W.DWORD]
    kernel.LoadLibraryExW.restype = W.HMODULE
    kernel.GetProcAddress.argtypes = [W.HMODULE, C.c_char_p]
    kernel.GetProcAddress.restype = PTR
    kernel.GetModuleHandleW.argtypes = [W.LPCWSTR]
    kernel.GetModuleHandleW.restype = W.HMODULE
    kernel.FreeLibrary.argtypes = [W.HMODULE]
    kernel.FreeLibrary.restype = W.BOOL
    kernel.SetErrorMode.argtypes = [W.UINT]
    kernel.SetErrorMode.restype = W.UINT
    kernel.SetDefaultDllDirectories.argtypes = [W.DWORD]
    kernel.SetDefaultDllDirectories.restype = W.BOOL
    # Suppress critical-error/crash dialogs in this disposable child only.
    kernel.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    # Also use explicit directories for later delay-load dependency requests.
    if not kernel.SetDefaultDllDirectories(0x00001000):
        raise C.WinError(C.get_last_error())

    def checkpoint(name, **details):
        print("FEATURE_PROBE_EVENT " + json.dumps({"checkpoint": name, **details},
                                                ensure_ascii=True), flush=True)

    def hresult(value):
        return "0x%08X" % (value & 0xFFFFFFFF)

    def method(obj, index, result, *args):
        table = C.cast(obj, C.POINTER(C.POINTER(PTR))).contents
        address = table[index]
        if not address:
            raise RuntimeError("Null COM vtable entry")
        return C.WINFUNCTYPE(result, PTR, *args)(address)

    def release(obj):
        if obj:
            method(obj, 2, ULONG)(obj)

    def read_error_info():
        pointer = PTR()
        hr = automation.GetErrorInfo(0, C.byref(pointer))
        details = {"get_error_info_hresult": hresult(hr), "present": bool(pointer.value)}
        if pointer.value:
            try:
                for index, name in [(4, "source"), (5, "description"), (6, "helpfile")]:
                    string = PTR()
                    try:
                        code = method(pointer, index, HRESULT, C.POINTER(PTR))(
                            pointer, C.byref(string))
                        details[name + "_hresult"] = hresult(code)
                        if string.value:
                            details[name] = C.wstring_at(string)
                    finally:
                        if string.value:
                            automation.SysFreeString(string)
            finally:
                release(pointer)
        return details

    def exception_details(exception, hr):
        details = {"code": exception.code, "scode": hresult(exception.scode),
                   "deferred_fill_present": bool(exception.deferred)}
        # Consume the thread's error object before another COM call can clear it.
        if hr < 0:
            try:
                details["error_info"] = read_error_info()
            except Exception as error:
                details["error_info_read_error"] = str(error)
        if exception.deferred:
            try:
                code = C.WINFUNCTYPE(HRESULT, C.POINTER(EXCEPINFO))(
                    exception.deferred)(C.byref(exception))
                details["deferred_fill_hresult"] = hresult(code)
                details["code"] = exception.code
                details["scode"] = hresult(exception.scode)
            except Exception as error:
                details["deferred_fill_error"] = str(error)
        for field in ["source", "description", "helpfile"]:
            pointer = getattr(exception, field)
            if pointer:
                try:
                    details[field] = C.wstring_at(pointer)
                finally:
                    automation.SysFreeString(pointer)
                    setattr(exception, field, None)
        details["helpcontext"] = exception.helpcontext
        return details

    def native_state():
        # Read-only snapshots at verified RVAs in the SHA-256 matched DLL.
        # Values are reported as presence flags, never called as functions.
        def present(rva):
            return bool(PTR.from_address(int(module) + rva).value)
        offsets = build['state']
        app = PTR.from_address(int(module) + offsets['application']).value
        return {"context_manager_present": present(offsets['context_manager']),
                "fallback_application_pointer_present": bool(app),
                "fallback_application_matches_host": bool(host and app == host.app_pointer),
                "context_stack_depth": C.c_uint32.from_address(int(module) + offsets['depth']).value,
                "internal_backend_present": present(offsets['backend']),
                "progress_dialog_object_present": present(offsets['progress']),
                "progress_parent_window_present": present(offsets['progress_parent']),
                "text_table_adapter_installed": bool(text_table and text_table.installed),
                "pskernel_loaded": bool(kernel.GetModuleHandleW("PSKERNEL.dll"))}

    def setup_host():
        from com_host import MinimalHost
        nonlocal host, host_log
        host_log = (output_dir / "host_calls.jsonl").open("x", encoding="utf-8", buffering=1)

        def host_event(event, **details):
            host_log.write(json.dumps({"event": event, **details}, ensure_ascii=True) + "\n")
            host_log.flush()
            checkpoint(event, **details)

        host = MinimalHost(Path(__file__).with_name("host_contract_common.json"), host_event,
                           special_key=None if batch_profile else host_environment["application"]["SpecialKey"])
        checkpoint("host_environment_applied", source=host_environment["source"],
                   captured_utc=host_environment.get("captured_utc"),
                   special_key=host.app_values.get("SpecialKey"),
                   special_key_scope="Save mask 0x10 only" if batch_profile else "historical snapshot")
        result["host_selftest"] = host.self_test()
        checkpoint("host_selftest_complete", result=result["host_selftest"])
        host.phase = "bind"
        address = kernel.GetProcAddress(module, b"ObjectNumberOf")
        if not address:
            raise RuntimeError("ObjectNumberOf export is missing")
        context = VARIANT()
        context.vt = 9
        context.value.pointer = host.app_pointer
        host.objects["IAlphaCamApp"].add_ref(host.app_pointer)
        checkpoint("before_host_bind", native_state=native_state())
        try:
            # Verified in the exact analyzed x64 export: RCX points at a
            # 24-byte VARIANT, EDX is an integer flag, EAX returns the count.
            # Its context helper retains a fallback app reference after popping
            # the call-local context. This is an experimental host binding,
            # not the full InitAlphacamAddIn initialization sequence.
            count = C.WINFUNCTYPE(LONG, C.POINTER(VARIANT), LONG)(address)(
                C.byref(context), 0)
        finally:
            automation.VariantClear(C.byref(context))
        state = native_state()
        result["host_binding"] = {"method": "ObjectNumberOf", "flags": 0,
                                  "returned_count": count, "native_state": state,
                                  "ok": state["fallback_application_matches_host"]}
        checkpoint("after_host_bind", result=result["host_binding"], native_state=state)
        if not result["host_binding"]["ok"]:
            raise RuntimeError("The DLL did not retain the supplied application context")
        if not state["internal_backend_present"]:
            raise RuntimeError("Internal importer backend is absent; model import was not attempted")
        host.phase = "import"

    def property_get(obj, name):
        dispid = LONG()
        names = (W.LPWSTR * 1)(name)
        hr = method(obj, 5, HRESULT, C.POINTER(GUID), C.POINTER(W.LPWSTR), UINT, ULONG,
                    C.POINTER(LONG))(obj, C.byref(null_iid), names, 1, 0, C.byref(dispid))
        if hr < 0:
            return {"ok": False, "operation": "GetIDsOfNames", "hresult": hresult(hr)}, None
        params = DISPPARAMS()
        value = VARIANT()
        exception = EXCEPINFO()
        argument_error = UINT()
        automation.SetErrorInfo(0, None)
        hr = method(obj, 6, HRESULT, LONG, C.POINTER(GUID), ULONG, WORD,
                    C.POINTER(DISPPARAMS), C.POINTER(VARIANT), C.POINTER(EXCEPINFO),
                    C.POINTER(UINT))(obj, dispid.value, C.byref(null_iid), 0, 2,
                    C.byref(params), C.byref(value), C.byref(exception), C.byref(argument_error))
        exception_result = exception_details(exception, hr)
        result = {"ok": hr >= 0, "operation": "Invoke PROPERTYGET", "hresult": hresult(hr),
                  "variant_type": value.vt}
        if hr < 0:
            result["exception"] = exception_result
        if hr < 0:
            automation.VariantClear(C.byref(value))
            return result, None
        return result, value

    def invoke_method(obj, name, arguments):
        dispid = LONG()
        names = (W.LPWSTR * 1)(name)
        hr = method(obj, 5, HRESULT, C.POINTER(GUID), C.POINTER(W.LPWSTR), UINT, ULONG,
                    C.POINTER(LONG))(obj, C.byref(null_iid), names, 1, 0, C.byref(dispid))
        if hr < 0:
            return {"ok": False, "operation": "GetIDsOfNames", "hresult": hresult(hr)}, None
        values = (VARIANT * len(arguments))()
        result_value = VARIANT()
        try:
            # IDispatch takes positional arguments in reverse order.
            for i, argument in enumerate(reversed(arguments)):
                if isinstance(argument, str):
                    pointer = automation.SysAllocString(argument)
                    if not pointer:
                        raise MemoryError("SysAllocString failed")
                    values[i].vt = 8
                    values[i].value.pointer = pointer
                elif isinstance(argument, int):
                    values[i].vt = 3
                    values[i].value.integer = argument
                else:
                    raise TypeError("Unsupported argument type")
            params = DISPPARAMS(C.cast(values, PTR), None, len(arguments), 0)
            exception = EXCEPINFO()
            argument_error = UINT(0xFFFFFFFF)
            automation.SetErrorInfo(0, None)
            hr = method(obj, 6, HRESULT, LONG, C.POINTER(GUID), ULONG, WORD,
                        C.POINTER(DISPPARAMS), C.POINTER(VARIANT), C.POINTER(EXCEPINFO),
                        C.POINTER(UINT))(obj, dispid.value, C.byref(null_iid), 0, 1,
                        C.byref(params), C.byref(result_value), C.byref(exception),
                        C.byref(argument_error))
            exception_result = exception_details(exception, hr)
            result = {"ok": hr >= 0, "operation": "Invoke METHOD", "method": name,
                      "hresult": hresult(hr), "variant_type": result_value.vt}
            if hr < 0:
                result["exception"] = exception_result
            if argument_error.value != 0xFFFFFFFF:
                result["reverse_argument_index"] = argument_error.value
            from com_typeinfo import numeric_variant
            number = numeric_variant(result_value)
            if number is not None:
                result["numeric_result"] = number
            return result, number
        finally:
            automation.VariantClear(C.byref(result_value))
            for value in values:
                automation.VariantClear(C.byref(value))

    def property_set_bool(obj, name, enabled):
        dispid=LONG();names=(W.LPWSTR*1)(name)
        hr=method(obj,5,HRESULT,C.POINTER(GUID),C.POINTER(W.LPWSTR),UINT,ULONG,C.POINTER(LONG))(
            obj,C.byref(null_iid),names,1,0,C.byref(dispid))
        if hr<0:raise RuntimeError("Unknown alignment property: "+name)
        value=VARIANT();value.vt=11;value.value.short=-1 if enabled else 0
        named=LONG(-3)
        params=DISPPARAMS(C.cast(C.pointer(value),PTR),C.cast(C.pointer(named),PTR),1,1)
        answer=VARIANT();exception=EXCEPINFO();argument_error=UINT()
        hr=method(obj,6,HRESULT,LONG,C.POINTER(GUID),ULONG,WORD,C.POINTER(DISPPARAMS),
                  C.POINTER(VARIANT),C.POINTER(EXCEPINFO),C.POINTER(UINT))(
            obj,dispid.value,C.byref(null_iid),0,4,C.byref(params),C.byref(answer),C.byref(exception),C.byref(argument_error))
        exception_details(exception,hr);automation.VariantClear(C.byref(answer))
        if hr<0:raise RuntimeError("Cannot configure alignment: "+name)
        status,actual=property_get(obj,name)
        try:
            if not status.get("ok") or actual is None or actual.vt!=11 or bool(actual.value.short)!=enabled:
                raise RuntimeError("Alignment property did not retain requested value: "+name)
        finally:
            if actual is not None:automation.VariantClear(C.byref(actual))
        return {"property":name,"enabled":enabled,"readback_matches":True}

    def geometry_item(obj, index):
        dispid=LONG(); names=(W.LPWSTR*1)("Item")
        hr=method(obj,5,HRESULT,C.POINTER(GUID),C.POINTER(W.LPWSTR),UINT,ULONG,C.POINTER(LONG))(
            obj,C.byref(null_iid),names,1,0,C.byref(dispid))
        if hr<0: raise RuntimeError("Collection.Item unavailable")
        arg=VARIANT(); arg.vt=3; arg.value.integer=index
        params=DISPPARAMS(C.cast(C.pointer(arg),PTR),None,1,0)
        answer=VARIANT(); exception=EXCEPINFO(); argument_error=UINT()
        automation.SetErrorInfo(0,None)
        hr=method(obj,6,HRESULT,LONG,C.POINTER(GUID),ULONG,WORD,C.POINTER(DISPPARAMS),
                  C.POINTER(VARIANT),C.POINTER(EXCEPINFO),C.POINTER(UINT))(
            obj,dispid.value,C.byref(null_iid),0,1,C.byref(params),C.byref(answer),
            C.byref(exception),C.byref(argument_error))
        details=exception_details(exception,hr)
        return {"ok":hr>=0,"hresult":hresult(hr),"details":details},answer

    def measure_single_body(bodies, inspector):
        dispid=LONG();names=(W.LPWSTR*1)("Item")
        hr=method(bodies,5,HRESULT,C.POINTER(GUID),C.POINTER(W.LPWSTR),UINT,ULONG,C.POINTER(LONG))(
            bodies,C.byref(null_iid),names,1,0,C.byref(dispid))
        if hr<0:raise RuntimeError("Bodies.Item unavailable")
        argument=VARIANT();argument.vt=3;argument.value.integer=1
        params=DISPPARAMS(C.cast(C.pointer(argument),PTR),None,1,0)
        body=VARIANT();exception=EXCEPINFO();argument_error=UINT()
        hr=method(bodies,6,HRESULT,LONG,C.POINTER(GUID),ULONG,WORD,C.POINTER(DISPPARAMS),
                  C.POINTER(VARIANT),C.POINTER(EXCEPINFO),C.POINTER(UINT))(
            bodies,dispid.value,C.byref(null_iid),0,1,C.byref(params),C.byref(body),C.byref(exception),C.byref(argument_error))
        exception_details(exception,hr)
        try:
            if hr<0 or body.vt!=9 or not body.value.pointer:raise RuntimeError("Bodies.Item did not return a body")
            bounds={};counts={}
            for name in ("MinX","MaxX","MinY","MaxY","MinZ","MaxZ"):
                status,value=property_get(body.value.pointer,name)
                try:
                    if not status.get("ok") or value is None or value.vt!=5:raise RuntimeError("Cannot measure "+name)
                    bounds[name]=value.value.number
                finally:
                    if value is not None:automation.VariantClear(C.byref(value))
            for name in ("Faces","Edges","Vertices"):
                status,collection=property_get(body.value.pointer,name)
                try:
                    if not status.get("ok") or collection is None or collection.vt!=9:raise RuntimeError("Cannot inspect "+name)
                    status,count=property_get(collection.value.pointer,"Count")
                    try:
                        from com_typeinfo import numeric_variant
                        number=numeric_variant(count) if count is not None else None
                        if not status.get("ok") or type(number) is not int or number<1:raise RuntimeError("Invalid "+name+" count")
                        counts[name]=number
                    finally:
                        if count is not None:automation.VariantClear(C.byref(count))
                finally:
                    if collection is not None:automation.VariantClear(C.byref(collection))
            dimensions=[bounds["Max"+axis]-bounds["Min"+axis] for axis in ("X","Y","Z")]
            measured={"bounds":bounds,"dimensions_api_units":dimensions,"topology_counts":counts,
                      "alignment_disabled":True,"full_topology_validity_verified":False,"volume_verified":False}
            if batch_profile and batch_profile.get('geometry_snapshot'):
                from feature_geometry import extract_geometry
                from feature_dispatch import FeatureDispatch
                adapter=FeatureDispatch(body.value.pointer,property_get,geometry_item,
                    lambda value:automation.VariantClear(C.byref(value)),inspector.describe)
                with adapter:
                    geometry=extract_geometry(adapter)
                if any(len(geometry[key])!=counts[collection] for key,collection in
                       [('faces','Faces'),('edges','Edges'),('vertices','Vertices')]):
                    raise RuntimeError('Geometry snapshot counts disagree with the measured body')
                data=json.dumps(geometry,ensure_ascii=False,allow_nan=False,indent=2).encode('utf-8')
                target=output_dir/'feature_geometry.json'
                with target.open('xb') as stream:stream.write(data)
                measured['geometry_snapshot']={"filename":target.name,"sha256":hashlib.sha256(data).hexdigest(),
                    "counts":counts,"topology_read_complete":True,
                    "property_error_count":len(geometry['property_errors']),"converted_to_occt":False}
                checkpoint('geometry_snapshot_written',result=measured['geometry_snapshot'])
            return measured
        finally:automation.VariantClear(C.byref(body))

    def count_bodies(obj):
        from com_typeinfo import numeric_variant
        status, bodies = property_get(obj, "Bodies")
        result = {"bodies_property": status}
        if bodies is not None:
            try:
                if bodies.vt == 9 and bodies.value.pointer:
                    status, count = property_get(bodies.value.pointer, "Count")
                    result["count_property"] = status
                    if count is not None:
                        try:
                            result["count"] = numeric_variant(count)
                        finally:
                            automation.VariantClear(C.byref(count))
            finally:
                automation.VariantClear(C.byref(bodies))
        return result

    def object_record_probe():
        from object_serialization import NativeObjectWriter
        context = VARIANT()
        context.vt = 9
        context.value.pointer = host.app_pointer
        host.objects["IAlphaCamApp"].add_ref(host.app_pointer)
        host.phase = "object_write"
        exception_trace.phase = "object_write"
        checkpoint("before_object_serialization", native_state=native_state())
        try:
            writer = NativeObjectWriter(module,
                lambda name: kernel.GetProcAddress(module, name.encode("ascii")), build_sha256=build_sha256)
            return writer.write(context, output_dir / "feature_object.bin", checkpoint,
                                error_reader=kernel_errors.read if kernel_errors else None)
        finally:
            automation.VariantClear(C.byref(context))

    def model_probe(obj):
        nonlocal exception_trace, text_table, kernel_errors, schema_path
        from com_typeinfo import Inspector, resolve_import_contract, valid_save_contract
        inspector = Inspector(method, release, automation, VARIANT)
        result = {}
        checkpoint("before_typeinfo_inspection")
        result["interface"] = inspector.describe(obj)
        checkpoint("interface_description", interface=result["interface"])
        contract = resolve_import_contract(result["interface"])
        result["resolved_import_contract"] = contract
        if contract is None:
            result["skipped"] = "Import signature or enum was not recognized; no import was attempted."
            return result
        if batch_profile:
            result["alignment_configuration"]=[property_set_bool(obj,name,False) for name in
                ("AlignOnImport","AlignTopWithOrigin","AlignLongestEdgeWithXAxis","UseLatheAlign","UsePanelAlignment","UseMinBoundingBoxAlignment")]
            checkpoint("batch_alignment_configured",settings=result["alignment_configuration"])
        result["bodies_before"] = count_bodies(obj)
        result["native_state_before_import"] = native_state()
        from native_trace import NativeExceptionTrace

        def host_objects():
            return {name: {"pointer": hex(obj.pointer),
                           "vtable": hex(C.addressof(obj.table)), "references": obj.refs}
                    for name, obj in host.objects.items()}

        exception_trace = NativeExceptionTrace(output_dir / "native_exceptions.jsonl", host_objects)
        exception_trace.install()
        checkpoint("native_trace_ready", captures="x64 RIP, registers and stack frames")
        from feature_texts import Catalog, NativeTextTable
        exception_trace.phase = "text_init"
        host.phase = "text_init"
        checkpoint("before_text_table_init")
        text_table = NativeTextTable(int(module), Catalog(Path(__file__).with_name("Feature.po"), build_sha256),
                                    build_sha256)
        result["text_table"] = text_table.install()
        checkpoint("text_table_ready", result=result["text_table"])
        if batch_profile and batch_profile.get('kernel_errors'):
            from parasolid_errors import ParasolidErrors
            kernel_errors = ParasolidErrors(module, build_sha256)
            result['parasolid_error_api'] = kernel_errors.info
            checkpoint('parasolid_error_api_ready', identity=kernel_errors.info)
        if batch_profile and batch_profile.get('schema_path'):
            from feature_schema_path import NativeSchemaPath
            schema_path = NativeSchemaPath(int(module), path.parent, build_sha256)
            result['schema_path'] = schema_path.install()
            checkpoint('schema_path_ready', result=result['schema_path'])
        result["native_state_before_import"] = native_state()
        exception_trace.phase = "import"
        host.phase = "import"
        checkpoint("before_model_import", method=contract["method"],
                   import_mode=contract["mode_name"], import_mode_value=contract["mode_value"],
                   file_type=contract["file_type_name"], file_type_value=contract["file_type_value"],
                   model_copy=str(model_path), native_state=result["native_state_before_import"])
        try:
            result["import"], imported = invoke_method(
                obj, contract["method"],
                [contract["file_type_value"], str(model_path), contract["mode_value"]])
        except Exception as error:
            result["import_error"] = {"type": type(error).__name__, "message": str(error)}
            result["native_state_after_import"] = native_state()
            checkpoint("native_import_error", error=result["import_error"],
                       native_exceptions_captured=len(exception_trace.records),
                       native_state=result["native_state_after_import"])
            return result
        result["native_state_after_import"] = native_state()
        checkpoint("after_model_import", result=result["import"],
                   native_state=result["native_state_after_import"])
        result["bodies_after"] = count_bodies(obj)
        checkpoint("after_import_body_count", result=result["bodies_after"])
        count = result["bodies_after"].get("count")
        if not result["import"].get("ok") or not isinstance(count, int) or count <= 0:
            result["save_skipped"] = "No successful import with a nonempty Bodies collection."
            return result
        status, bodies = property_get(obj, "Bodies")
        try:
            if bodies is None or bodies.vt != 9 or not bodies.value.pointer:
                result["save_skipped"] = "Bodies interface is unavailable."
                return result
            result["bodies_interface"] = inspector.describe(bodies.value.pointer)
            if not valid_save_contract(result["bodies_interface"]):
                result["save_skipped"] = "Save signature was not recognized."
                return result
            if batch_profile:
                result["body_measurement"]=measure_single_body(bodies.value.pointer, inspector)
                checkpoint("batch_body_measured",measurement=result["body_measurement"])
            if batch_profile and batch_profile.get('measure_only'):
                result['measurement_only_completed'] = True
                result['save_skipped'] = 'Read-only ARD diagnostic: no Save or object serialization requested.'
                checkpoint('measurement_only_completed')
                return result
            target = output_dir / "roundtrip.x_b"
            if target.exists():
                raise FileExistsError("Refusing to overwrite " + str(target))
            checkpoint("before_parasolid_save", output=str(target))
            if kernel_errors:
                result['kernel_error_before_save'] = kernel_errors.read()
                checkpoint('kernel_error_before_save', result=result['kernel_error_before_save'])
            host.phase = "save"
            exception_trace.phase = "save"
            if batch_profile:
                host.app_values["SpecialKey"]=batch_profile["special_key_projection"]
                checkpoint("live_save_projection",mask=16,value=host.app_values["SpecialKey"],source="current_CLS_GetAccessCode_348")
            try:
                result["save"], saved = invoke_method(bodies.value.pointer, "Save", [str(target)])
            finally:
                if batch_profile:host.app_values.pop("SpecialKey",None)
            if kernel_errors:
                result['kernel_error_after_save'] = kernel_errors.read()
                checkpoint('kernel_error_after_save', result=result['kernel_error_after_save'])
            if target.is_file():
                data = target.read_bytes()
                result["output"] = {"filename": target.name, "size_bytes": len(data),
                                    "sha256": hashlib.sha256(data).hexdigest(),
                                    "has_parasolid_header": b"FORMAT=binary" in data[:4096]}
            checkpoint("after_parasolid_save", result=result.get("save"),
                       saved_body_count=saved, output=result.get("output"))
            direct_record = bool(batch_profile and batch_profile.get('direct_record'))
            if (result["save"].get("ok") and isinstance(saved, int) and saved > 0
                    and (result.get("output", {}).get("has_parasolid_header") or direct_record)):
                if direct_record:
                    checkpoint('direct_object_record_requested', standalone_save_file_confirmed=bool(result.get('output')))
                try:
                    result["object_serialization"] = object_record_probe()
                except Exception as error:
                    result["object_serialization"] = {"ok": False, "error_type": type(error).__name__,
                                                       "error": str(error), "complete_ard": False}
                    checkpoint("object_serialization_error", result=result["object_serialization"])
            return result
        finally:
            if bodies is not None:
                automation.VariantClear(C.byref(bodies))

    handles = []
    module = None
    host = None
    host_log = None
    exception_trace = None
    text_table = None
    kernel_errors = None
    schema_path = None
    native_object = PTR()
    dispatch = PTR()
    typeinfo = PTR()
    body_variant = None
    init_hr = ole.CoInitializeEx(None, 2)
    if init_hr < 0:
        raise RuntimeError("CoInitializeEx failed: " + hresult(init_hr))
    result = {"stage": stage, "python_bits": C.sizeof(PTR) * 8,
              "acam_processes_before": acam_processes()}
    checkpoint("before_load", acam_processes=result["acam_processes_before"])
    try:
        for directory in [path.parent] + dependency_dirs:
            handles.append(os.add_dll_directory(str(directory)))
        # Search the selected DLL directory, explicit directories and Windows defaults.
        module = kernel.LoadLibraryExW(str(path), None, 0x00000100 | 0x00001000)
        if not module:
            error = C.get_last_error()
            raise OSError(error, C.FormatError(error))
        result["loaded"] = True
        result["pskernel_loaded_after_load"] = bool(kernel.GetModuleHandleW("PSKERNEL.dll"))
        version_address = kernel.GetProcAddress(module, b"GetUIVersion")
        if not version_address:
            raise RuntimeError("GetUIVersion export is missing")
        result["ui_interface_version"] = C.WINFUNCTYPE(C.c_int)(version_address)()
        checkpoint("loaded", ui_interface_version=result["ui_interface_version"])
        if stage == "load":
            return result
        factory_address = kernel.GetProcAddress(module, b"GetSolidFeatures")
        if not factory_address:
            raise RuntimeError("GetSolidFeatures export is missing")
        # Observed x64 contract: first argument is an output interface-pointer
        # address. Return value deliberately ignored; it is not reliable on all paths.
        checkpoint("before_GetSolidFeatures")
        C.WINFUNCTYPE(None, C.POINTER(PTR))(factory_address)(C.byref(native_object))
        result["factory_returned_object"] = bool(native_object.value)
        checkpoint("after_GetSolidFeatures", returned_object=bool(native_object.value))
        if not native_object.value:
            return result
        hr = method(native_object, 0, HRESULT, C.POINTER(GUID), C.POINTER(PTR))(
            native_object, C.byref(dispatch_iid), C.byref(dispatch))
        result["query_idispatch_hresult"] = hresult(hr)
        if hr < 0 or not dispatch.value:
            return result
        if stage == "model":
            try:
                setup_host()
                result["model_probe"] = model_probe(dispatch)
            except Exception as error:
                result["model_probe_error"] = {"type": type(error).__name__, "message": str(error)}
                checkpoint("model_probe_error", error=result["model_probe_error"])
            result["host_layers"] = host.layers.snapshot() if host else []
            result["host_aux_state"] = dict(host.aux_state) if host else {}
            result["host_display_state"] = dict(host.states) if host else {}
            if host:
                host.layers.changed()
            result["pskernel_loaded_after_test"] = bool(kernel.GetModuleHandleW("PSKERNEL.dll"))
            return result
        count = UINT()
        hr = method(dispatch, 3, HRESULT, C.POINTER(UINT))(dispatch, C.byref(count))
        result["typeinfo_count_hresult"] = hresult(hr)
        result["typeinfo_count"] = count.value
        if stage == "factory" and hr >= 0 and count.value:
            hr = method(dispatch, 4, HRESULT, UINT, ULONG, C.POINTER(PTR))(
                dispatch, 0, 0, C.byref(typeinfo))
            result["get_typeinfo_hresult"] = hresult(hr)
            if hr >= 0 and typeinfo.value:
                name = PTR()
                doc = PTR()
                helpfile = PTR()
                helpcontext = ULONG()
                hr = method(typeinfo, 12, HRESULT, LONG, C.POINTER(PTR), C.POINTER(PTR),
                            C.POINTER(ULONG), C.POINTER(PTR))(
                                typeinfo, -1, C.byref(name), C.byref(doc),
                                C.byref(helpcontext), C.byref(helpfile))
                result["typeinfo_documentation_hresult"] = hresult(hr)
                if hr >= 0 and name.value:
                    result["interface_name"] = C.wstring_at(name)
                for pointer in [name, doc, helpfile]:
                    if pointer.value:
                        automation.SysFreeString(pointer)
        if stage == "bodies":
            checkpoint("before_Bodies_property")
            result["bodies_property"], body_variant = property_get(dispatch, "Bodies")
            if body_variant is not None and body_variant.vt == 9 and body_variant.value.pointer:
                count_status, count_variant = property_get(body_variant.value.pointer, "Count")
                result["bodies_count_property"] = count_status
                if count_variant is not None:
                    try:
                        if count_variant.vt == 2:
                            result["bodies_count"] = count_variant.value.short
                        elif count_variant.vt in (3, 22):
                            result["bodies_count"] = count_variant.value.integer
                        elif count_variant.vt in (19, 23):
                            result["bodies_count"] = count_variant.value.unsigned
                    finally:
                        automation.VariantClear(C.byref(count_variant))
        result["pskernel_loaded_after_test"] = bool(kernel.GetModuleHandleW("PSKERNEL.dll"))
        return result
    finally:
        # A failure in a native call can still terminate this child. The parent
        # captures that exit code and proceeds to write its diagnostic report.
        result["acam_processes_after"] = acam_processes()
        if exception_trace is not None:
            exception_trace.phase = "cleanup"
        checkpoint("before_cleanup", acam_processes=result["acam_processes_after"],
                   pskernel_loaded=bool(kernel.GetModuleHandleW("PSKERNEL.dll")))
        if body_variant is not None:
            automation.VariantClear(C.byref(body_variant))
        release(typeinfo)
        release(dispatch)
        release(native_object)
        if schema_path is not None:
            try:
                result['schema_path_cleanup'] = schema_path.close()
                checkpoint('schema_path_closed', result=result['schema_path_cleanup'])
            except Exception as error:
                result['schema_path_cleanup_error'] = {'type': type(error).__name__, 'message': str(error)}
                checkpoint('schema_path_cleanup_error', error=result['schema_path_cleanup_error'])
        if text_table is not None:
            try:
                text_table.close()
                result["text_table_cleanup_ok"] = True
                checkpoint("text_table_closed", original_pointer_restored=True)
            except Exception as error:
                result["text_table_cleanup_error"] = {"type": type(error).__name__,
                                                       "message": str(error)}
                checkpoint("text_table_cleanup_error", error=result["text_table_cleanup_error"])
        if kernel_errors:
            try:
                result['pskernel_file_unchanged'] = kernel_errors.unchanged()
            except OSError as error:
                result['pskernel_file_unchanged'] = False
                result['pskernel_file_check_error'] = str(error)
            checkpoint('pskernel_file_check', unchanged=result['pskernel_file_unchanged'])
        if module:
            kernel.FreeLibrary(module)
        for handle in reversed(handles):
            handle.close()
        ole.CoUninitialize()
        if exception_trace is not None:
            exception_trace.close()
        # Keep callback objects pinned through FreeLibrary and COM cleanup.
        if host_log is not None:
            host_log.close()
        checkpoint("cleanup_complete")


def choose_dll():
    try:
        import tkinter as tk
        from tkinter import filedialog
        window = tk.Tk()
        window.withdraw()
        window.attributes("-topmost", True)
        name = filedialog.askopenfilename(title="Wybierz Feature.dll z instalacji ALPHACAM 2025",
                                         parent=window,
                                         initialdir=r"C:\Program Files\Hexagon\ALPHACAM 2025\Add-Ins\Feature",
                                         initialfile="Feature.dll",
                                         filetypes=[("Feature DLL", "*.dll"), ("All files", "*.*")])
        window.destroy()
        return Path(name) if name else None
    except Exception:
        name = input("Pelna sciezka do Feature.dll z ALPHACAM 2025: ").strip().strip('"')
        return Path(name) if name else None


def choose_model():
    try:
        import tkinter as tk
        from tkinter import filedialog
        window = tk.Tk()
        window.withdraw()
        window.attributes("-topmost", True)
        try:
            name = filedialog.askopenfilename(
                title="Wybierz Parasolid .x_b / .x_t zgodny z ALPHACAM 2025",
                parent=window,
                filetypes=[("Parasolid", "*.x_b *.x_t"), ("All files", "*.*")])
        finally:
            window.destroy()
        return Path(name) if name else None
    except Exception:
        name = input("Pelna sciezka do modelu Parasolid .x_b / .x_t: ").strip().strip('"')
        return Path(name) if name else None


def parse_worker_output(stdout, stderr=""):
    result = {"checkpoints": []}
    other = []
    for line in stdout.splitlines():
        try:
            if line.startswith("FEATURE_PROBE_RESULT "):
                result["result"] = json.loads(line[len("FEATURE_PROBE_RESULT "):])
            elif line.startswith("FEATURE_PROBE_EVENT "):
                result["checkpoints"].append(json.loads(line[len("FEATURE_PROBE_EVENT "):]))
            else:
                other.append(line)
        except json.JSONDecodeError:
            other.append(line)
    if other:
        result["stdout"] = "\n".join(other)[-6000:]
    if stderr:
        result["stderr"] = stderr[-6000:]
    return result


def summarize_stage(stage):
    checkpoints = stage.get("checkpoints", [])
    callbacks = [c for c in checkpoints if c.get("checkpoint") == "host_method"
                 and c.get("host_phase") != "selftest"]
    missing = next((c for c in checkpoints if c.get("checkpoint") == "host_unimplemented"), None)
    callback_error = next((c for c in checkpoints if c.get("checkpoint") == "host_callback_error"), None)
    result = stage.get("result", {})
    model = result.get("model_probe", {})
    def latest(name, key, default=None):
        return next((c[key] for c in reversed(checkpoints)
                     if c.get("checkpoint") == name and key in c), default)
    imported = model.get("import") or latest("after_model_import", "result", {})
    bodies = model.get("bodies_after") or latest("after_import_body_count", "result", {})
    saved = model.get("save") or latest("after_parasolid_save", "result", {})
    output = model.get("output") or latest("after_parasolid_save", "output")
    saved_count = saved.get("numeric_result")
    save_ok = bool(saved.get("ok") and type(saved_count) is int and saved_count > 0
                   and output and output.get("size_bytes", 0) > 0
                   and output.get("has_parasolid_header"))
    object_record = (model.get("object_serialization") or latest("after_object_do_write", "result")
                     or latest("object_serialization_error", "result")
                     or latest("after_object_query_write", "result"))
    object_ok = bool(object_record and object_record.get("ok")
                     and object_record.get("fragment", {}).get("recognized_prefix"))
    native_error = (model.get("import_error") or latest("native_import_error", "error")
                    or result.get("model_probe_error") or latest("model_probe_error", "error")
                    or result.get("text_table_cleanup_error")
                    or latest("text_table_cleanup_error", "error")
                    or result.get("error") or callback_error)
    summary = {"host_selftest_ok": any(c.get("checkpoint") == "host_selftest_complete"
                                       and c.get("result", {}).get("ok") for c in checkpoints),
               "host_context_bound": any(c.get("checkpoint") == "after_host_bind"
                                         and c.get("result", {}).get("ok") for c in checkpoints),
               "text_table_initialized": any(c.get("checkpoint") == "text_table_ready"
                                              and c.get("result", {}).get("ok") for c in checkpoints),
               "implemented_host_calls_from_dll": len(callbacks),
               "called_host_properties": list(dict.fromkeys(c["interface"] + "." + c["method"]
                                                            for c in callbacks)),
               "first_unimplemented_host_method": missing,
               "callback_error": callback_error,
               "unsupported_interface_queries": list(dict.fromkeys(c["requested_iid"] for c in checkpoints
                   if c.get("checkpoint") == "host_query_interface" and not c.get("supported")
                   and c.get("host_phase") != "selftest")),
               "import_status": imported,
               "bodies_after_count": bodies.get("count"),
               "import_succeeded_with_bodies": bool(imported.get("ok") and (bodies.get("count") or 0) > 0),
               "save_status": saved,
               "saved_body_count": saved_count,
               "save_succeeded_with_file": save_ok,
               "object_serialization": object_record,
               "object_record_written_and_prefix_recognized": object_ok,
               "output": output,
               "native_error": native_error}
    summary["host_layers"] = result.get("host_layers", latest("host_layer_state", "layers", []))
    summary["host_aux_state"] = result.get("host_aux_state", latest("host_aux_state", "state", {}))
    summary["host_display_state"] = result.get("host_display_state", latest("host_display_state", "state", {}))
    if missing:
        summary["stop_reason"] = "unimplemented_host_method"
    elif "timeout_seconds" in stage:
        summary["stop_reason"] = "child_timeout"
    elif callback_error:
        summary["stop_reason"] = "host_callback_error"
    elif summary["native_error"]:
        summary["stop_reason"] = "native_or_probe_error"
    elif stage.get("process_exit_code") != 0:
        summary["stop_reason"] = "child_process_exit"
    elif not summary["import_succeeded_with_bodies"]:
        summary["stop_reason"] = "import_not_successful"
    elif model.get('measurement_only_completed') is True:
        summary['stop_reason'] = 'completed_measurement_only'
    elif stage.get('direct_record'):
        summary['stop_reason'] = ('completed_native_record' if object_ok and saved.get('ok') and saved_count == 1
                                  else 'native_record_not_successful')
    elif not save_ok:
        summary["stop_reason"] = "save_not_successful"
    elif object_record is not None and not object_ok:
        summary["stop_reason"] = "object_serialization_not_successful"
    else:
        summary["stop_reason"] = "completed"
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path, nargs="?")
    parser.add_argument("--model", type=Path, help="Input Parasolid model; a private copy is used")
    parser.add_argument("--dependency-dir", type=Path, action="append", default=[],
                        help="Additional installed DLL dependency directory; may be repeated")
    parser.add_argument("--out", type=Path, help="New diagnostic JSON file")
    parser.add_argument("--timeout", type=float, default=60.0, help="Seconds per child stage")
    parser.add_argument("--worker", choices=["model"], help=argparse.SUPPRESS)
    parser.add_argument("--work-dir", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--host-environment", type=Path,
                        help="Read-only host settings snapshot from host_environment.py")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("This runtime probe requires Windows x64. It cannot execute this DLL on Linux.")
    if C.sizeof(C.c_void_p) != 8:
        parser.error("Use 64-bit Python.")
    if args.timeout <= 0:
        parser.error("Timeout must be positive.")
    print("TEST 4.9 - zapis rekordu obiektu do ARD, Feature.dll 2025", flush=True)
    if not args.dll:
        print("Wybierz Feature.dll z instalacji ALPHACAM 2025.", flush=True)
    path = args.dll or choose_dll()
    if path is None:
        return 0
    path = path.resolve(strict=True)
    metadata = inspect_pe(path)
    if not metadata["matches_analyzed_build"]:
        parser.error("DLL differs from the analyzed 2025.3.0.200 sample. No native function was called. SHA256: " + metadata["sha256"])
    from host_environment import load_environment
    environment_path = args.host_environment or Path(__file__).with_name("host_environment_2025.json")
    try:
        environment = load_environment(environment_path, metadata["sha256"])
    except (OSError, ValueError, TypeError) as error:
        parser.error("Najpierw otworz ALPHACAM 2025 Router i uruchom "
                     "ODCZYTAJ_USTAWIENIA_2025.cmd. Potem zamknij ALPHACAM i powtorz test. " + str(error))
    from feature_texts import Catalog
    catalog = Catalog(Path(__file__).with_name("Feature.po"))
    dirs = [p.resolve(strict=True) for p in args.dependency_dir]
    if args.worker:
        if args.model is None or args.work_dir is None:
            parser.error("Worker requires a model copy and an output directory")
        try:
            result = worker(path, args.worker, dirs, args.model.resolve(strict=True),
                            args.work_dir.resolve(strict=True), environment)
        except Exception as error:
            result = {"stage": args.worker, "error_type": type(error).__name__, "error": str(error)}
        print("FEATURE_PROBE_RESULT " + json.dumps(result, ensure_ascii=True), flush=True)
        return 0
    if acam_processes():
        parser.error("Zamknij Alphacama na czas testu i uruchom skrypt ponownie.")
    if not args.model:
        print("Wybierz plik Parasolid .x_b/.x_t zapisany przez ALPHACAM 2025.", flush=True)
    model = args.model or choose_model()
    if model is None:
        return 0
    model = model.resolve(strict=True)
    if not model.is_file() or model.suffix.lower() not in (".x_b", ".x_t"):
        parser.error("Ten test wymaga pliku Parasolid (.x_b/.x_t) zgodnego z ALPHACAM 2025.")
    output_dir = Path.cwd() / ("feature_test_4_9_2025_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f"))
    output = args.out or output_dir / "feature_probe_2025_host.json"
    if output.exists():
        parser.error("Output already exists; choose a new path with --out")
    output_dir.mkdir(exist_ok=False)
    environment_copy = output_dir / "host_environment_2025.json"
    environment_copy.write_text(json.dumps(environment, ensure_ascii=False, indent=2), encoding="utf-8")
    model_copy = output_dir / ("kopia_wejsciowa" + model.suffix.lower())
    source_hash = hashlib.sha256(model.read_bytes()).hexdigest()
    shutil.copyfile(model, model_copy)
    if hashlib.sha256(model_copy.read_bytes()).hexdigest() != source_hash:
        raise RuntimeError("Input copy verification failed")
    report = {"test": "Feature.dll minimal COM host binding and Parasolid import probe",
              "probe_revision": "4.9-object-record-2025", "dll": metadata,
              "host_configuration": {"program_letter": "R", "program_letter_I2": 82,
                                     "program_kind": "Router", "in_api_call": True,
                                     "special_key": environment["application"]["SpecialKey"]},
              "host_environment": environment,
              "text_catalog": catalog.info,
              "input": {"filename": model.name, "size_bytes": model.stat().st_size, "sha256": source_hash},
              "acam_processes_before": acam_processes(), "stages": [],
              "limits": ["The full add-in initializer is not called.",
                         "Three base host objects and on-demand layer objects are supplied using the ObjectNumberOf context argument.",
                         "Host supports application/frame access, UI updating properties and an in-memory subset of drawing/layer APIs.",
                         "The layer store starts empty. Geometry collections, undo and attributes remain unimplemented and stop the child when called.",
                         "SetUndoCommandName stores only the command label; it does not implement undo. View refresh requests affect zero owned views and render nothing.",
                         "ThreeDViews stores the requested display mode. ZoomAll records a fit request for zero owned views; neither creates a window.",
                         "SpecialKey preserves a prior read-only API snapshot. It does not validate a license or supply a license authorization.",
                         "After a successful Parasolid save, ObjectQueryWrite2 and ObjectDoWrite2 attempt a separate feature_object.bin. It is an add-in record, not a complete ARD.",
                         "Only the in-process UI text table is initialized separately; the original pointer is restored before DLL unloading.",
                         "Three missing Feature.po entries and seven external-context strings are valid empty CString objects, listed in text_catalog.",
                         "An unimplemented host method logs its identity and terminates the disposable child with code 80.",
                         "The DLL file and executable code are unchanged. No COM server registration is changed.",
                         "Import operates on a private input copy. No ARD is written.",
                         "A newer Parasolid input may be incompatible with the 2025 kernel.",
                         "A successful Parasolid save does not prove ARD save capability."]}
    report["limits"].append("Native first-chance access violations are recorded without changing their CONTEXT or handling disposition.")
    print("Feature.dll: 2025.3.0.200, matched SHA-256")
    print("Host: IAlphaCamApp + IDrawing + IFrame")
    print("Tryb hosta: Router (R), wywolania przez API")
    print("SpecialKey z odczytu API:", environment["application"]["SpecialKey"])
    print("Teksty: Feature.po pl-PL, 26 tablic, 583 obiekty CString")
    print("Model:", model.name)
    print("Kopia pliku wejsciowego:", model_copy)
    print("Nowy zapis DLL, jesli sie uda, otrzyma nazwe roundtrip.x_b.")
    print("Dodatkowa proba zapisu rekordu obiektu otrzyma nazwe feature_object.bin.")
    print("Test moze potrwac do", args.timeout, "sekund. Wynik zostanie zapisany w JSON.", flush=True)
    for stage in ["model"]:
        print("Test:", stage, flush=True)
        command = [sys.executable, str(Path(__file__).resolve()), str(path), "--worker", stage,
                   "--model", str(model_copy), "--work-dir", str(output_dir),
                   "--host-environment", str(environment_copy)]
        for directory in dirs:
            command += ["--dependency-dir", str(directory)]
        try:
            process = subprocess.run(command, capture_output=True, text=True,
                                     errors="replace", timeout=args.timeout)
            stage_result = {"stage": stage, "process_exit_code": process.returncode,
                            "process_exit_code_hex": "0x%08X" % (process.returncode & 0xFFFFFFFF)}
            stage_result.update(parse_worker_output(process.stdout, process.stderr))
        except subprocess.TimeoutExpired as error:
            stage_result = {"stage": stage, "timeout_seconds": args.timeout}
            stdout = error.stdout or b""
            stderr = error.stderr or b""
            if isinstance(stdout, bytes):
                stdout = stdout.decode("utf-8", errors="replace")
            if isinstance(stderr, bytes):
                stderr = stderr.decode("utf-8", errors="replace")
            stage_result.update(parse_worker_output(stdout, stderr))
        report["stages"].append(stage_result)
        summary = summarize_stage(stage_result)
        stage_result["summary"] = summary
        print("Test wlasnego hosta:", summary["host_selftest_ok"])
        print("Kontekst przyjety przez DLL:", summary["host_context_bound"])
        print("Tabela tekstow przygotowana:", summary["text_table_initialized"])
        print("Wywolane wlasciwosci hosta:", ", ".join(summary["called_host_properties"]) or "brak")
        missing = summary["first_unimplemented_host_method"]
        if missing:
            print("BRAKUJACA METODA:", missing["interface"] + "." + missing["method"],
                  "invkind=" + str(missing["invkind"]))
        print("Kod zakonczenia procesu:", stage_result.get("process_exit_code_hex", "timeout"))
        result = stage_result.get("result", {})
        probe = result.get("model_probe", {})
        print("Parasolid zaladowany:", result.get("pskernel_loaded_after_test", "brak wyniku"))
        print("Import:", json.dumps(summary["import_status"] or probe.get("skipped", "nie ukonczono"), ensure_ascii=True))
        print("Liczba bryl po imporcie:", summary["bodies_after_count"])
        print("Warstwy utworzone w hoscie:", len(summary["host_layers"]))
        print("Zapis:", json.dumps(probe.get("save", probe.get("save_skipped", "brak wyniku")), ensure_ascii=True))
        print("Liczba zapisanych bryl:", summary["saved_body_count"])
        print("Zapis utworzyl plik Parasolid:", summary["save_succeeded_with_file"])
        print("Rekord obiektu zapisany i rozpoznany:", summary["object_record_written_and_prefix_recognized"])
        if summary["object_serialization"]:
            print("Rekord obiektu:", json.dumps(summary["object_serialization"], ensure_ascii=True))
        if summary["native_error"]:
            print("Blad:", summary["native_error"])
        print("Powod zakonczenia:", summary["stop_reason"])
        if stage_result.get("checkpoints"):
            print("Ostatni punkt:", stage_result["checkpoints"][-1].get("checkpoint"))
    report["acam_processes_after"] = acam_processes()
    observed = list(report["acam_processes_before"]) + list(report["acam_processes_after"])
    for stage_result in report["stages"]:
        for event in stage_result.get("checkpoints", []):
            observed.extend(event.get("acam_processes", []))
    report["no_acam_process_observed_at_checkpoints"] = not observed
    report["original_input_sha256_after"] = hashlib.sha256(model.read_bytes()).hexdigest()
    report["original_input_unchanged"] = report["original_input_sha256_after"] == source_hash
    report["native_exceptions"] = []
    trace_path = output_dir / "native_exceptions.jsonl"
    if trace_path.is_file():
        for line in trace_path.read_text(encoding="utf-8").splitlines():
            try:
                report["native_exceptions"].append(json.loads(line))
            except json.JSONDecodeError:
                report.setdefault("trace_read_errors", []).append("Incomplete native trace record")
    native_fault = next((e for e in report["native_exceptions"]
                         if e.get("event") == "native_access_violation" and e.get("phase") == "import"), None)
    if native_fault:
        place = native_fault.get("location", {})
        print("Miejsce wyjatku:", place.get("module"), "+", place.get("rva"))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print("\nRAPORT:", output.resolve())
    if (output_dir / "host_calls.jsonl").is_file():
        print("LOG WYWOLAN:", (output_dir / "host_calls.jsonl").resolve())
    return 0


if __name__ == "__main__":
    sys.exit(main())
