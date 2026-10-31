"""Stateful COM layer store; no geometry, undo or attribute success is invented.

An empty drawing has no layers. Returned interfaces are retained; removed objects
stay pinned until DLL unload and return E_FAIL on later layer operations. Unicode
output names use genuine OleAut32 BSTR allocations owned by the recipient.
"""
import ctypes as C

OK = 0
E_POINTER = C.c_int32(0x80004003).value
E_INVALIDARG = C.c_int32(0x80070057).value
E_FAIL = C.c_int32(0x80004005).value
BADINDEX = C.c_int32(0x8002000B).value
PTR, SHORT, LONG, ULONG = C.c_void_p, C.c_int16, C.c_int32, C.c_uint32


class BSTRs:
    def __init__(self):
        self.ole = C.WinDLL('oleaut32')
        self.ole.SysAllocStringLen.argtypes = [C.c_wchar_p, ULONG]
        self.ole.SysAllocStringLen.restype = PTR
        self.ole.SysStringLen.argtypes = [PTR]
        self.ole.SysStringLen.restype = ULONG
        self.ole.SysFreeString.argtypes = [PTR]
        self.ole.SysFreeString.restype = None

    def allocate(self, text):
        pointer = self.ole.SysAllocStringLen(text, len(text.encode('utf-16-le')) // 2)
        if not pointer:
            raise MemoryError('SysAllocStringLen failed')
        return pointer

    def read(self, pointer):
        if not pointer:
            return ''
        units = self.ole.SysStringLen(pointer)
        if units > 32768:
            raise ValueError('Layer name exceeds the probe limit')
        return C.string_at(pointer, units * 2).decode('utf-16-le')

    def free(self, pointer):
        self.ole.SysFreeString(pointer)


def output(pointer, ctype, value):
    if not pointer:
        return E_POINTER
    C.cast(pointer, C.POINTER(ctype))[0] = value
    return OK


class LayerHost:
    DRAWING = {('GetFirstLayer', 1), ('GetLayerCount', 1), ('GetLayer', 1),
               ('SetLayer', 1), ('CreateLayer', 1), ('Layers', 2)}
    TYPES = {'Visible': SHORT, 'Lock': SHORT, 'Color': LONG, 'LineType': LONG,
             'ColorRGB': ULONG, 'LineWidth': LONG, 'LineWidthCustom': C.c_double}

    def __init__(self, host, contracts, object_class, bstrs=None):
        self.host, self.contracts, self.object_class = host, contracts, object_class
        self._bstrs = bstrs
        self.order, self.active, self.next_id = [], None, 1

    @property
    def bstrs(self):
        if self._bstrs is None:
            self._bstrs = BSTRs()
        return self._bstrs

    def handles(self, obj, spec):
        name, kind = spec['name'], spec['invkind']
        if obj.name == 'IDrawing':
            return (name, kind) in self.DRAWING
        if obj.name == 'ILayers':
            return (name, kind) in {('Item', 1), ('Count', 2), ('Drawing', 2)}
        if obj.name == 'ILayer':
            return ((name in self.TYPES and kind in (2, 4)) or
                    (name in ('Name', 'Active') and kind in (2, 4)) or
                    (name in ('DisplayName', 'Special', 'UserLayer', 'Drawing') and kind == 2) or
                    (name in ('GetNext', 'Delete', 'GetUseDefaultColours', 'UseDefaultColours') and kind == 1))
        return False

    def snapshot(self):
        return [{'id': o.layer_id, 'name': o.layer_data['Name'], 'active': o is self.active,
                 **{k: o.layer_data[k] for k in self.TYPES}} for o in self.order]

    def changed(self):
        self.host.emit('host_layer_state', layers=self.snapshot())

    def collection(self):
        if 'ILayers' not in self.host.objects:
            self.host.objects['ILayers'] = self.object_class(self.host, self.contracts['ILayers'])
        return self.host.objects['ILayers']

    def create(self, name):
        existing = next((o for o in self.order if o.layer_data['Name'] == name), None)
        if existing:
            return existing
        obj = self.object_class(self.host, self.contracts['ILayer'])
        obj.layer_id = self.next_id
        self.next_id += 1
        obj.layer_data = {'Name': name, 'Visible': True, 'Lock': False, 'Color': 0,
                          'LineType': 0, 'ColorRGB': 0, 'LineWidth': 0,
                          'LineWidthCustom': 0.0, 'default_colours': True}
        self.host.objects['ILayer:' + str(obj.layer_id)] = obj
        self.order.append(obj)
        if self.active is None:
            self.active = obj
        self.changed()
        return obj

    def return_object(self, pointer, obj):
        if not pointer:
            return E_POINTER
        output(pointer, PTR, None)
        if obj is not None:
            obj.refs += 1
            output(pointer, PTR, obj.pointer)
        return OK

    def handle(self, obj, spec, args):
        name, kind = spec['name'], spec['invkind']
        if obj.name == 'IDrawing':
            code = self.drawing(name, args)
        elif obj.name == 'ILayers':
            code = self.items(name, args)
        else:
            code = self.layer(obj, name, kind, args)
        self.host.emit('host_method', interface=obj.name, method=name,
                       operation={1:'METHOD', 2:'PROPERTYGET', 4:'PROPERTYPUT'}[kind],
                       vtable_offset_x64=hex(spec['vtable_offset_x64']), implemented=True,
                       hresult='0x%08X' % (code & 0xFFFFFFFF),
                       **({'layer_id':obj.layer_id} if obj.name == 'ILayer' else {}))
        return code

    def drawing(self, name, args):
        if name == 'GetFirstLayer':
            return self.return_object(args[0], self.order[0] if self.order else None)
        if name == 'GetLayerCount':
            return output(args[0], LONG, len(self.order))
        if name == 'GetLayer':
            return self.return_object(args[0], self.active)
        if name == 'Layers':
            return self.return_object(args[0], self.collection()) if args[0] else E_POINTER
        if name == 'SetLayer':
            target = next((o for o in self.order if o.pointer == args[0]), None)
            if target is None:
                return E_INVALIDARG
            self.active = target
            self.changed()
            return OK
        if not args[1]:
            return E_POINTER
        output(args[1], PTR, None)
        name = self.bstrs.read(args[0])
        if not name or '\0' in name:
            return E_INVALIDARG
        return self.return_object(args[1], self.create(name))

    def items(self, name, args):
        if name == 'Drawing':
            return self.return_object(args[0], self.host.objects['IDrawing'])
        if name == 'Count':
            return output(args[0], LONG, len(self.order))
        from com_host import Variant
        if not args[0] or not args[1]:
            return E_POINTER
        output(args[1], PTR, None)
        value = C.cast(args[0], C.POINTER(Variant)).contents
        target = None
        if value.vt in (2, 3, 19, 22, 23):
            index = value.value.short if value.vt == 2 else value.value.integer
            if 1 <= index <= len(self.order):
                target = self.order[index - 1]
        elif value.vt == 8:
            text = self.bstrs.read(value.value.pointer)
            target = next((o for o in self.order if o.layer_data['Name'] == text), None)
        else:
            return E_INVALIDARG
        return self.return_object(args[1], target) if target else BADINDEX

    def layer(self, obj, name, kind, args):
        if obj not in self.order:
            return E_FAIL
        state = obj.layer_data
        if name == 'GetNext':
            index = self.order.index(obj) + 1
            return self.return_object(args[0], self.order[index] if index < len(self.order) else None)
        if name == 'Delete':
            self.order.remove(obj)
            if self.active is obj:
                self.active = self.order[0] if self.order else None
            self.changed()
            return OK
        if name == 'Drawing':
            return self.return_object(args[0], self.host.objects['IDrawing'])
        if name in ('Name', 'DisplayName') and kind == 2:
            if not args[0]:
                return E_POINTER
            output(args[0], PTR, None)
            return output(args[0], PTR, self.bstrs.allocate(state['Name']))
        if name == 'Name':
            value = self.bstrs.read(args[0])
            if not value or '\0' in value or any(o is not obj and o.layer_data['Name'] == value for o in self.order):
                return E_INVALIDARG
            state['Name'] = value
        elif name in ('Special', 'UserLayer'):
            return output(args[0], SHORT, 0 if name == 'Special' else -1)
        elif name == 'Active':
            if kind == 2:
                return output(args[0], SHORT, -1 if self.active is obj else 0)
            if args[0]:
                self.active = obj
            elif self.active is obj:
                self.active = None
        elif name == 'GetUseDefaultColours':
            return output(args[0], SHORT, -1 if state['default_colours'] else 0)
        elif name == 'UseDefaultColours':
            state['default_colours'] = True
        elif name in self.TYPES:
            ctype = self.TYPES[name]
            if kind == 2:
                value = (-1 if state[name] else 0) if ctype == SHORT else state[name]
                return output(args[0], ctype, value)
            state[name] = bool(args[0]) if ctype == SHORT else args[0]
            if name in ('Color', 'ColorRGB'):
                state['default_colours'] = False
        self.changed()
        return OK


def invoke_layer(obj, spec, params, result_pointer, arg_error):
    """Typed IDispatch subset; calls the same stateful native host handlers."""
    from com_host import Variant, DispParams, DISP_E_BADPARAMCOUNT, DISP_E_TYPEMISMATCH
    values = C.cast(params, C.POINTER(DispParams)).contents
    inputs = [p for p in spec['parameters_native'] if p['flags'] & 1]
    outputs = [p for p in spec['parameters_native'] if p['flags'] & 2]
    if values.count != len(inputs) or (values.count and not values.args):
        return DISP_E_BADPARAMCOUNT
    if spec['invkind'] == 4:
        if values.named_count != 1 or not values.named or C.cast(values.named, C.POINTER(LONG))[0] != -3:
            return DISP_E_BADPARAMCOUNT
    elif values.named_count:
        return DISP_E_BADPARAMCOUNT
    arguments = C.cast(values.args, C.POINTER(Variant)) if values.count else None
    native = []
    for index, parameter in enumerate(inputs):
        value = arguments[values.count - index - 1]
        vt = parameter['type']['vt']
        scalar = vt in (2, 3, 19, 22, 23, 29) and value.vt in (2, 3, 19, 22, 23)
        accepted = scalar or (vt in (5, 8, 11, 12, 26) and (vt == 12 or value.vt == {26:9}.get(vt, vt)))
        if not accepted:
            if arg_error:
                output(arg_error, ULONG, values.count - index - 1)
            return DISP_E_TYPEMISMATCH
        if vt == 12:
            native.append(C.addressof(value))
        elif vt in (8, 26):
            native.append(value.value.pointer)
        elif vt == 5:
            native.append(value.value.number)
        elif value.vt in (2, 11):
            native.append(value.value.short)
        else:
            native.append(value.value.integer & 0xFFFFFFFF if vt == 19 else value.value.integer)
    out, out_type = None, None
    if outputs:
        vt = outputs[0]['type']['element']['vt']
        out = {2:SHORT, 3:LONG, 19:ULONG, 5:C.c_double, 11:SHORT, 8:PTR, 26:PTR, 29:LONG}[vt]()
        native.append(C.byref(out))
        out_type = {26:9, 29:3}.get(vt, vt)
    code = obj.handle(spec, tuple(native))
    if code < 0 or out is None:
        return code
    if result_pointer:
        C.memset(result_pointer, 0, C.sizeof(Variant))
        result = C.cast(result_pointer, C.POINTER(Variant)).contents
        result.vt = out_type
        if out_type in (8, 9):
            result.value.pointer = out.value
        elif out_type in (2, 11):
            result.value.short = out.value
        elif out_type == 5:
            result.value.number = out.value
        else:
            result.value.integer = C.c_int32(out.value).value
    elif out_type == 8:
        obj.host.layers.bstrs.free(out.value)
    elif out_type == 9 and out.value:
        target = next(o for o in obj.host.objects.values() if o.pointer == out.value)
        target.release(target.pointer)
    return code
