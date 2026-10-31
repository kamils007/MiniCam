"""Native callback layer checks, with a UTF-16 BSTR test allocator on Linux."""
import ctypes as C
from pathlib import Path
import unittest
import com_host as H
from layer_host import E_FAIL, E_INVALIDARG, BADINDEX

class Strings:
    def __init__(self): self.buffers = {}
    def allocate(self, text):
        data=text.encode('utf-16-le'); b=C.create_string_buffer(len(data)+6)
        C.c_uint32.from_buffer(b).value=len(data); p=C.addressof(b)+4
        C.memmove(p,data,len(data));self.buffers[p]=b;return p
    def read(self,p):return C.string_at(p,C.c_uint32.from_address(p-4).value).decode('utf-16-le') if p else ''
    def free(self,p):del self.buffers[p]

class LayerTests(unittest.TestCase):
    def setUp(self):
        self.strings=Strings();self.stops=[];self.events=[]
        self.host=H.MinimalHost(Path(__file__).with_name('host_contract_common.json'),
            lambda event,**kw:self.events.append({'event':event,**kw}),
            callback_factory=getattr(C,'WINFUNCTYPE',C.CFUNCTYPE),abort=self.stops.append,bstrs=self.strings)
    def tearDown(self):self.assertFalse(self.strings.buffers,'BSTR ownership leak')
    def call(self,key,offset,types,*args):return self.host.call(key,offset,H.HRESULT,types,*args)
    def release(self,key):self.host.call(key,16,H.ULONG,[])
    def key(self,p):return next(k for k,o in self.host.objects.items() if o.pointer==p)
    def create(self,name):
        p=self.strings.allocate(name);out=H.PTR()
        try:self.assertEqual(self.call('IDrawing',0x180,[H.PTR,H.PTR],p,C.byref(out)),0)
        finally:self.strings.free(p)
        key=self.key(out.value);self.release(key);return key
    def name(self,key):
        p=H.PTR();self.assertEqual(self.call(key,0x38,[H.PTR],C.byref(p)),0)
        try:return self.strings.read(p.value)
        finally:self.strings.free(p.value)
    def test_empty_multiple_unicode_layers_and_iteration(self):
        out=H.PTR(123);self.assertEqual(self.call('IDrawing',0x160,[H.PTR],C.byref(out)),0);self.assertIsNone(out.value)
        a,b=self.create('Bryła_żółta😀'),self.create('Warstwa 2')
        self.assertEqual(self.name(a),'Bryła_żółta😀');self.assertEqual(self.create('Warstwa 2'),b)
        n=H.LONG();self.assertEqual(self.call('IDrawing',0x168,[H.PTR],C.byref(n)),0);self.assertEqual(n.value,2)
        self.assertEqual(self.call(a,0x90,[H.PTR],C.byref(out)),0);self.assertEqual(out.value,self.host.objects[b].pointer);self.release(b)
        self.assertEqual(self.call(b,0x90,[H.PTR],C.byref(out)),0);self.assertIsNone(out.value)
        self.assertEqual(self.call('IDrawing',0x178,[H.PTR],self.host.objects[b].pointer),0)
        self.assertIs(self.host.layers.active,self.host.objects[b]);self.assertTrue(all(o.refs==1 for o in self.host.objects.values()))
    def test_duplicate_names_and_deleted_objects(self):
        a,b=self.create('a'),self.create('b');p=self.strings.allocate('a')
        try:self.assertEqual(self.call(b,0x40,[H.PTR],p),E_INVALIDARG)
        finally:self.strings.free(p)
        self.assertEqual(self.name(b),'b');self.assertEqual(self.call(a,0xa0,[]),0);self.assertIn(a,self.host.objects)
        self.assertEqual(len(self.host.layers.order),1);v=H.SHORT()
        self.assertEqual(self.call(a,0x48,[H.PTR],C.byref(v)),E_FAIL)
        self.assertEqual(self.call('IDrawing',0x178,[H.PTR],123),E_INVALIDARG)
    def test_native_width_guards_and_float_state(self):
        k=self.create('layer');self.assertEqual(self.call(k,0x108,[H.ULONG],0x80abcdef),0)
        words=(H.ULONG*3)(0xDEADBEEF,0,0xCAFEBABE)
        self.assertEqual(self.call(k,0x100,[H.PTR],C.byref(words,4)),0)
        self.assertEqual(list(words),[0xDEADBEEF,0x80abcdef,0xCAFEBABE])
        self.assertEqual(self.call(k,0x150,[C.c_double],0.375),0);v=C.c_double()
        self.assertEqual(self.call(k,0x148,[H.PTR],C.byref(v)),0);self.assertEqual(v.value,0.375)
        self.assertEqual(self.call(k,0x38,[H.PTR],None),H.E_POINTER);self.assertFalse(self.stops)
    def test_collection_lookup_and_reference_ownership(self):
        a,b=self.create('a'),self.create('b');out=H.PTR()
        self.assertEqual(self.call('IDrawing',0x330,[H.PTR],C.byref(out)),0);self.release('ILayers')
        index=H.Variant();index.vt=3;index.value.integer=2
        self.assertEqual(self.call('ILayers',0x40,[H.PTR,H.PTR],C.byref(index),C.byref(out)),0)
        self.assertEqual(out.value,self.host.objects[b].pointer);self.release(b)
        index.value.integer=99
        self.assertEqual(self.call('ILayers',0x40,[H.PTR,H.PTR],C.byref(index),C.byref(out)),BADINDEX);self.assertIsNone(out.value)
        index.vt=8;index.value.pointer=self.strings.allocate('a')
        try:self.assertEqual(self.call('ILayers',0x40,[H.PTR,H.PTR],C.byref(index),C.byref(out)),0)
        finally:self.strings.free(index.value.pointer)
        self.assertEqual(out.value,self.host.objects[a].pointer);self.release(a)
        self.assertTrue(all(o.refs==1 for o in self.host.objects.values()))
    def invoke(self,key,dispid,flags,params,result=None):
        iid=(C.c_ubyte*16)()
        return self.call(key,48,[H.LONG,H.PTR,H.ULONG,C.c_uint16,H.PTR,H.PTR,H.PTR,H.PTR],
            dispid,C.byref(iid),0,flags,C.byref(params),C.byref(result) if result is not None else None,None,None)
    def test_dispatch_strings_interfaces_and_named_put(self):
        value=H.Variant();value.vt=8;value.value.pointer=self.strings.allocate('Dispatch layer')
        params=H.DispParams(C.cast(C.byref(value),H.PTR),None,1,0);result=H.Variant()
        try:self.assertEqual(self.invoke('IDrawing',37,1,params,result),0)
        finally:self.strings.free(value.value.pointer)
        self.assertEqual(result.vt,9);key=self.key(result.value.pointer);self.release(key)
        self.assertEqual(self.invoke(key,1,2,H.DispParams(),result),0);self.assertEqual(result.vt,8)
        self.assertEqual(self.strings.read(result.value.pointer),'Dispatch layer');self.strings.free(result.value.pointer)
        self.assertEqual(self.invoke(key,1,2,H.DispParams()),0)
        value.vt=11;value.value.short=0;named=H.LONG(-3)
        params=H.DispParams(C.cast(C.byref(value),H.PTR),C.cast(C.byref(named),H.PTR),1,1)
        self.assertEqual(self.invoke(key,2,4,params),0);self.assertFalse(self.host.objects[key].layer_data['Visible'])
        named.value=42;self.assertEqual(self.invoke(key,2,4,params),H.DISP_E_BADPARAMCOUNT);self.assertFalse(self.stops)
    def test_geometry_and_undo_still_failfast(self):
        key=self.create('a');out=H.PTR()
        self.assertEqual(self.call(key,0x98,[H.PTR],C.byref(out)),H.E_NOTIMPL);self.assertEqual(self.stops[-1]['method'],'Geometries')
        self.assertEqual(self.call(key,0x160,[]),H.E_NOTIMPL);self.assertEqual(self.stops[-1]['method'],'SaveForUndo')

    def test_undo_label_copies_bstr_and_dispatch_checks_its_argument(self):
        text = self.strings.allocate('Import bryły żółtej😀')
        self.assertEqual(self.call('IAlphaCamApp',0x218,[H.PTR],text),H.S_OK)
        self.strings.free(text)
        self.assertEqual(self.host.aux_state['undo_command_name'],'Import bryły żółtej😀')
        value=H.Variant();value.vt=8;value.value.pointer=self.strings.allocate('Nowa nazwa')
        params=H.DispParams(C.cast(C.byref(value),H.PTR),None,1,0)
        try:self.assertEqual(self.invoke('IAlphaCamApp',58,1,params),H.S_OK)
        finally:self.strings.free(value.value.pointer)
        self.assertEqual(self.host.aux_state['undo_command_name'],'Nowa nazwa')
        value.vt=3;value.value.integer=123
        self.assertEqual(self.invoke('IAlphaCamApp',58,1,params),H.DISP_E_TYPEMISMATCH)
        self.assertEqual(self.host.aux_state['undo_command_name'],'Nowa nazwa')
        self.assertEqual(self.call('IAlphaCamApp',0x218,[H.PTR],None),H.S_OK)
        self.assertEqual(self.host.aux_state['undo_command_name'],'')

    def test_refresh_empty_view_set_and_refuse_to_claim_rendering_views(self):
        self.assertEqual(self.call('IDrawing',0x730,[]),H.S_OK)
        self.assertEqual(self.invoke('IDrawing',38,1,H.DispParams()),H.S_OK)
        self.assertEqual(self.host.aux_state['shaded_redraw_requests'],1)
        self.assertEqual(self.host.aux_state['redraw_requests'],1)
        self.assertEqual(self.call('IDrawing',0xa0,[]),H.S_OK)
        self.assertEqual(self.invoke('IDrawing',9,1,H.DispParams()),H.S_OK)
        self.assertEqual(self.host.aux_state['zoom_all_requests'],2)
        self.assertFalse(self.stops)
        self.host.aux_state['view_count']=1
        self.assertEqual(self.call('IDrawing',0x730,[]),H.E_NOTIMPL)
        self.assertEqual(self.host.aux_state['shaded_redraw_requests'],1)
        self.assertEqual(self.call('IDrawing',0xa0,[]),H.E_NOTIMPL)
        self.assertEqual(self.host.aux_state['zoom_all_requests'],2)

if __name__=='__main__':unittest.main(verbosity=2)
