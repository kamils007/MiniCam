"""Callback, lifetime and reporting checks; does not load Feature.dll.

On Linux these check the Python implementation with local CFUNCTYPE callbacks,
not the Windows ABI or ALPHACAM integration. The Windows probe repeats its native
property/identity self-test using WINFUNCTYPE before binding the host to the DLL.
"""
import ctypes as C
import gc
import json
from pathlib import Path
import subprocess
import sys
import unittest
import uuid

import com_host as H
from probe_feature_2025 import summarize_stage

HERE = Path(__file__).resolve().parent
CALLBACK = getattr(C, "WINFUNCTYPE", C.CFUNCTYPE)


class HostTests(unittest.TestCase):
    def setUp(self):
        self.events = []
        self.stops = []
        self.host = H.MinimalHost(HERE / "host_contract_common.json",
                                  lambda event, **data: self.events.append({"event": event, **data}),
                                  callback_factory=CALLBACK, abort=self.stops.append)

    def test_callbacks_and_pinned_lifetime(self):
        gc.collect()
        self.assertTrue(self.host.self_test()["ok"])
        self.assertEqual(len(self.host.objects), 3)
        self.assertTrue(all(obj.refs == 1 for obj in self.host.objects.values()))
        self.assertEqual(sum(len(o.callbacks) for o in self.host.objects.values()), 606 + 3 * 64)
        self.assertFalse(self.stops)

    def test_query_interface_identity_and_failure_output(self):
        for name, obj in self.host.objects.items():
            for iid_bytes in [H.IID_UNKNOWN, H.IID_DISPATCH, obj.iid]:
                out = H.PTR()
                iid = (C.c_ubyte * 16).from_buffer_copy(iid_bytes)
                self.assertEqual(self.host.call(name, 0, H.HRESULT, [H.PTR, H.PTR],
                                                C.byref(iid), C.byref(out)), H.S_OK)
                self.assertEqual(out.value, obj.pointer)
                self.host.call(name, 16, H.ULONG, [])
            out = H.PTR(123)
            iid = (C.c_ubyte * 16).from_buffer_copy(uuid.UUID(int=123).bytes_le)
            self.assertEqual(self.host.call(name, 0, H.HRESULT, [H.PTR, H.PTR],
                                            C.byref(iid), C.byref(out)), H.E_NOINTERFACE)
            self.assertIsNone(out.value)
            self.assertEqual(obj.refs, 1)

    def test_program_identity_native_width_and_dispatch_types(self):
        for name, offset, dispid, expected, vartype in [
                ("ProgramLetter", 0x110, 27, ord("R"), 2),
                ("InAPICall", 0x360, 97, -1, 11)]:
            words = (C.c_uint16 * 3)(0xABCD, 0, 0xDCBA)
            code = self.host.call("IAlphaCamApp", offset, H.HRESULT, [H.PTR], C.byref(words, 2))
            self.assertEqual(code, H.S_OK)
            self.assertEqual(list(words), [0xABCD, expected & 0xFFFF, 0xDCBA])
            self.assertEqual(self.host.call("IAlphaCamApp", offset, H.HRESULT, [H.PTR], None), H.E_POINTER)
            result = H.Variant()
            self.assertEqual(self.invoke("IAlphaCamApp", dispid, 2, H.DispParams(), result), H.S_OK)
            self.assertEqual((result.vt, result.value.short), (vartype, expected))
            self.assertEqual(self.dispatch_id("IAlphaCamApp", name), (H.S_OK, dispid))
        self.assertFalse(self.stops)

    def dispatch_id(self, obj, name):
        iid = (C.c_ubyte * 16)()
        names = (C.c_wchar_p * 1)(name)
        out = H.LONG(-1)
        code = self.host.call(obj, 40, H.HRESULT,
                              [H.PTR, H.PTR, H.ULONG, H.ULONG, H.PTR],
                              C.byref(iid), names, 1, 0, C.byref(out))
        return code, out.value

    def invoke(self, obj, dispid, flags, params, result=None, argerror=None):
        iid = (C.c_ubyte * 16)()
        return self.host.call(obj, 48, H.HRESULT,
                              [H.LONG, H.PTR, H.ULONG, C.c_uint16, H.PTR, H.PTR, H.PTR, H.PTR],
                              dispid, C.byref(iid), 0, flags, C.byref(params),
                              C.byref(result) if result is not None else None, None,
                              C.byref(argerror) if argerror is not None else None)

    def test_dispatch_get_and_put_bool(self):
        code, dispid = self.dispatch_id("IDrawing", "sCrEeNuPdAtInG")
        self.assertEqual(code, H.S_OK)
        result = H.Variant()
        self.assertEqual(self.invoke("IDrawing", dispid, 2, H.DispParams(), result), H.S_OK)
        self.assertEqual((result.vt, result.value.short), (11, -1))
        value = H.Variant()
        value.vt = 11
        value.value.short = 0
        named = H.LONG(-3)
        params = H.DispParams(C.cast(C.byref(value), H.PTR), C.cast(C.byref(named), H.PTR), 1, 1)
        self.assertEqual(self.invoke("IDrawing", dispid, 4, params), H.S_OK)
        self.assertFalse(self.host.states["ScreenUpdating"])
        value.vt = 3
        value.value.integer = 17
        argerror = H.ULONG(999)
        self.assertEqual(self.invoke("IDrawing", dispid, 4, params, argerror=argerror),
                         H.DISP_E_TYPEMISMATCH)
        self.assertEqual(argerror.value, 0)
        self.assertFalse(self.host.states["ScreenUpdating"])
        self.assertFalse(self.stops)

    def test_three_d_views_native_dispatch_and_no_window_side_effect(self):
        self.assertFalse(self.host.states["ThreeDViews"])
        self.assertEqual(self.host.call("IDrawing",0x58,H.HRESULT,[H.SHORT],-1),H.S_OK)
        words=(C.c_uint16*3)(0xABCD,0,0xDCBA)
        self.assertEqual(self.host.call("IDrawing",0x50,H.HRESULT,[H.PTR],C.byref(words,2)),H.S_OK)
        self.assertEqual(list(words),[0xABCD,0xFFFF,0xDCBA])
        result=H.Variant()
        self.assertEqual(self.invoke("IDrawing",4,2,H.DispParams(),result),H.S_OK)
        self.assertEqual((result.vt,result.value.short),(11,-1))
        value=H.Variant();value.vt=11;value.value.short=0;named=H.LONG(-3)
        params=H.DispParams(C.cast(C.byref(value),H.PTR),C.cast(C.byref(named),H.PTR),1,1)
        self.assertEqual(self.invoke("IDrawing",4,4,params),H.S_OK)
        self.assertFalse(self.host.states["ThreeDViews"])
        self.assertEqual(self.host.aux_state["view_count"],0)
        self.assertEqual(self.host.call("IDrawing",0x50,H.HRESULT,[H.PTR],None),H.E_POINTER)
        self.assertFalse(self.stops)

    def test_dispatch_interface_reference_ownership(self):
        code, dispid = self.dispatch_id("IAlphaCamApp", "ActiveDrawing")
        self.assertEqual(code, H.S_OK)
        result = H.Variant()
        obj = self.host.objects["IDrawing"]
        self.assertEqual(self.invoke("IAlphaCamApp", dispid, 2, H.DispParams(), result), H.S_OK)
        self.assertEqual((result.vt, result.value.pointer), (9, obj.pointer))
        self.assertEqual(obj.refs, 2)
        self.host.call("IDrawing", 16, H.ULONG, [])
        self.assertEqual(obj.refs, 1)

    def test_special_key_preserves_long_width_and_dispatch_type(self):
        # Test values are synthetic ABI cases, never a runtime default.
        for value in (0, 3, 0x12345678, -(2 ** 31), 2 ** 31 - 1):
            host = H.MinimalHost(HERE / "host_contract_common.json",
                lambda *args, **kw: None, callback_factory=CALLBACK,
                abort=self.stops.append, special_key=value)
            self.host = host
            words = (C.c_uint32 * 3)(0x11223344, 0xDEADBEEF, 0x55667788)
            self.assertEqual(host.call("IAlphaCamApp", 0x1C8, H.HRESULT,
                                      [H.PTR], C.byref(words, 4)), H.S_OK)
            self.assertEqual(list(words), [0x11223344, value & 0xFFFFFFFF, 0x55667788])
            result = H.Variant()
            self.assertEqual(self.invoke("IAlphaCamApp", 49, 2, H.DispParams(), result), H.S_OK)
            self.assertEqual((result.vt, result.value.integer), (3, value))
            self.assertEqual(self.dispatch_id("IAlphaCamApp", "SpecialKey"), (H.S_OK, 49))
            self.assertEqual(host.call("IAlphaCamApp", 0x1C8, H.HRESULT, [H.PTR], None), H.E_POINTER)
            self.assertTrue(host.self_test()["ok"])
        self.assertFalse(self.stops)

    def test_special_key_has_no_guessed_default(self):
        result = H.LONG(123456)
        self.assertEqual(self.host.call("IAlphaCamApp", 0x1C8, H.HRESULT,
                                       [H.PTR], C.byref(result)), H.E_NOTIMPL)
        self.assertEqual(result.value, 123456)
        self.assertEqual(self.stops[-1]["method"], "SpecialKey")
        for value in (True, "0", 2 ** 31, -(2 ** 31) - 1):
            with self.assertRaises(ValueError):
                H.MinimalHost(HERE / "host_contract_common.json", lambda *a, **kw: None,
                              callback_factory=CALLBACK, special_key=value)

    def test_unknown_method_never_returns_success(self):
        code = self.host.call("IDrawing", 0x110, H.HRESULT, [H.PTR], None)
        self.assertEqual(code, H.E_NOTIMPL)
        self.assertEqual(self.stops[0]["method"], "SaveAs")
        self.assertEqual(self.events[-1]["event"], "host_unimplemented")

    def test_added_method_slot_is_logged_and_stops(self):
        obj = self.host.objects["IDrawing"]
        offset = max(obj.methods) + 8
        code = self.host.call("IDrawing", offset, H.HRESULT, [H.PTR, H.LONG], None, 123)
        self.assertEqual(code, H.E_NOTIMPL)
        self.assertFalse(self.stops[0]["metadata_available"])
        self.assertEqual(self.stops[0]["vtable_offset_x64"], hex(offset))

    def test_real_failfast_in_child(self):
        program = """
import ctypes as C,json
from pathlib import Path
import com_host as H
host=H.MinimalHost(Path('host_contract_common.json'),
    lambda event,**kw: print(json.dumps({'event':event,**kw}),flush=True),
    callback_factory=getattr(C,'WINFUNCTYPE',C.CFUNCTYPE))
host.call('IDrawing',0x110,H.HRESULT,[H.PTR],None)
raise RuntimeError('unimplemented call returned unexpectedly')
"""
        process = subprocess.run([sys.executable, "-c", program], cwd=HERE,
                                 capture_output=True, text=True, timeout=15)
        self.assertEqual(process.returncode, H.HOST_STOP_EXIT, process.stderr)
        event = json.loads(process.stdout.splitlines()[-1])
        self.assertEqual((event["event"], event["method"]), ("host_unimplemented", "SaveAs"))

    def test_parent_report_distinguishes_stop_and_success(self):
        failed = summarize_stage({"process_exit_code": 80, "checkpoints": [
            {"checkpoint": "host_unimplemented", "interface": "IDrawing", "method": "SaveAs"}]})
        self.assertEqual(failed["stop_reason"], "unimplemented_host_method")
        self.assertFalse(failed["import_succeeded_with_bodies"])
        empty = summarize_stage({"process_exit_code": 0, "result": {"model_probe": {
            "import": {"ok": True}, "bodies_after": {"count": 0}}}})
        self.assertFalse(empty["import_succeeded_with_bodies"])
        self.assertEqual(empty["stop_reason"], "import_not_successful")
        complete = summarize_stage({"process_exit_code": 0, "result": {"model_probe": {
            "import": {"ok": True}, "bodies_after": {"count": 1}}}})
        self.assertTrue(complete["import_succeeded_with_bodies"])
        # A failfast during a later operation must preserve an import already
        # confirmed in flushed checkpoints, even without a final worker result.
        partial = summarize_stage({"process_exit_code": 80, "checkpoints": [
            {"checkpoint": "after_model_import", "result": {"ok": True}},
            {"checkpoint": "after_import_body_count", "result": {"count": 1}},
            {"checkpoint": "host_unimplemented", "interface": "IDrawing", "method": "Name"}]})
        self.assertTrue(partial["import_succeeded_with_bodies"])
        self.assertEqual(partial["stop_reason"], "unimplemented_host_method")

    def test_parent_preserves_saved_file_when_cleanup_stops(self):
        stage = {"process_exit_code": 80, "checkpoints": [
            {"checkpoint": "after_model_import", "result": {"ok": True}},
            {"checkpoint": "after_import_body_count", "result": {"count": 1}},
            {"checkpoint": "after_parasolid_save", "result": {"ok": True, "numeric_result": 1},
             "output": {"size_bytes": 1000, "has_parasolid_header": True}},
            {"checkpoint": "host_unimplemented", "interface": "IDrawing", "method": "Name"}]}
        result = summarize_stage(stage)
        self.assertTrue(result["save_succeeded_with_file"])
        self.assertEqual(result["saved_body_count"], 1)
        self.assertEqual(result["stop_reason"], "unimplemented_host_method")

    def test_import_alone_is_not_a_successful_save(self):
        model = {"import": {"ok": True}, "bodies_after": {"count": 1},
                 "save": {"ok": True, "numeric_result": 1}}
        stage = {"process_exit_code": 0, "result": {"model_probe": model}}
        self.assertEqual(summarize_stage(stage)["stop_reason"], "save_not_successful")
        model["output"] = {"size_bytes": 0, "has_parasolid_header": True}
        self.assertFalse(summarize_stage(stage)["save_succeeded_with_file"])
        model["output"]["size_bytes"] = 100
        self.assertEqual(summarize_stage(stage)["stop_reason"], "completed")
        model["save"]["numeric_result"] = 0
        self.assertFalse(summarize_stage(stage)["save_succeeded_with_file"])
        model["save"]["numeric_result"] = 1
        model["save"]["ok"] = False
        self.assertFalse(summarize_stage(stage)["save_succeeded_with_file"])

    def test_object_record_status_preserves_independent_parasolid_success(self):
        model = {"import": {"ok": True}, "bodies_after": {"count": 1},
                 "save": {"ok": True, "numeric_result": 1},
                 "output": {"size_bytes": 100, "has_parasolid_header": True},
                 "object_serialization": {"ok": False, "write_return_code": 1}}
        stage = {"process_exit_code": 0, "result": {"model_probe": model}}
        result = summarize_stage(stage)
        self.assertTrue(result["save_succeeded_with_file"])
        self.assertFalse(result["object_record_written_and_prefix_recognized"])
        self.assertEqual(result["stop_reason"], "object_serialization_not_successful")
        model["object_serialization"] = {"ok": True, "fragment": {"recognized_prefix": True}}
        self.assertEqual(summarize_stage(stage)["stop_reason"], "completed")
        partial = {"process_exit_code": 80, "checkpoints": [
            {"checkpoint": "after_model_import", "result": model["import"]},
            {"checkpoint": "after_import_body_count", "result": model["bodies_after"]},
            {"checkpoint": "after_parasolid_save", "result": model["save"], "output": model["output"]},
            {"checkpoint": "after_object_do_write", "result": model["object_serialization"]},
            {"checkpoint": "host_unimplemented", "interface": "IDrawing", "method": "Name"}]}
        result = summarize_stage(partial)
        self.assertTrue(result["object_record_written_and_prefix_recognized"])
        self.assertTrue(result["save_succeeded_with_file"])
        self.assertEqual(result["stop_reason"], "unimplemented_host_method")


if __name__ == "__main__":
    unittest.main(verbosity=2)
