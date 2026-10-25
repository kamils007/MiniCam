"""Variant ownership adapter for the geometry snapshot reader (no native ABI)."""
import ctypes as C


class SnapshotIds:
    """Snapshot-local IDs, verified by native IsSame, independent of Feature.ID."""
    def __init__(self):
        self.groups = {}
        self.pointers = {}
        self.pins = []
        self.next_id = 1

    def remember(self, obj, group, number):
        # Pin every cached alias, not just the representative: otherwise COM
        # could reuse a released pointer for a different object later on.
        if len(self.pins) >= 300000:
            raise RuntimeError('Too many retained snapshot interfaces')
        retained = obj.retain()
        self.pins.append(retained)
        self.pointers[(group, obj.pointer)] = number
        return retained

    def assign(self, obj):
        group = obj.describe().get('name')
        if not group:
            raise RuntimeError('Cannot identify Feature interface type')
        cached = self.pointers.get((group, obj.pointer))
        if cached is not None:
            return cached
        entries = self.groups.setdefault(group, [])
        for number, retained in entries:
            if obj.pointer == retained.pointer:
                return number
            same = obj.call('IsSame', retained)
            if type(same) is not bool:
                raise RuntimeError('Feature.IsSame did not return a boolean')
            if same:
                self.remember(obj, group, number)
                return number
        if self.next_id > 300000:
            raise RuntimeError('Too many snapshot objects')
        retained = self.remember(obj, group, self.next_id)
        number = self.next_id
        self.next_id += 1
        entries.append((number, retained))
        return number

    def close(self):
        for retained in self.pins:
            retained.close()
        self.pins.clear()
        self.pointers.clear()
        self.groups.clear()


class FeatureDispatch:
    def __init__(self, pointer, get_property, item, clear, describe, variant=None,
                 retain=None, identities=None):
        self.pointer = pointer
        self._get = get_property
        self._item = item
        self._clear = clear
        self._describe = describe
        self._variant = variant
        self._retain = retain
        self._identities = identities if identities is not None else SnapshotIds()
        self._identity_owner = identities is None
        self._description = None
        self.closed = False

    def __enter__(self):
        if self.closed:
            raise RuntimeError('Feature interface already released')
        return self

    def __exit__(self, *_):
        self.close()

    def close(self):
        if not self.closed:
            self.closed = True
            if self._identity_owner:
                self._identities.close()
            if self._variant is not None:
                self._clear(self._variant)
                self._variant = None

    def _decode(self, status, value, name):
        transfer = False
        try:
            if not status.get('ok') or value is None:
                raise RuntimeError('Feature.' + name + ': ' + str(status))
            if value.vt == 9 and value.value.pointer:
                obj = FeatureDispatch(value.value.pointer, self._get, self._item, self._clear,
                                      self._describe, variant=value, retain=self._retain,
                                      identities=self._identities)
                transfer = True
                return obj
            if value.vt in (0, 1): return None
            if value.vt == 2: return int(value.value.short)
            if value.vt in (3, 22): return int(value.value.integer)
            if value.vt in (19, 23): return int(value.value.unsigned)
            if value.vt == 5: return float(value.value.number)
            if value.vt == 11: return bool(value.value.short)
            if value.vt == 8: return C.wstring_at(value.value.pointer) if value.value.pointer else ''
            raise RuntimeError('Unsupported Feature variant type ' + str(value.vt) + ' for ' + name)
        finally:
            if value is not None and not transfer:
                self._clear(value)

    def get(self, name):
        if self.closed: raise RuntimeError('Released Feature interface')
        if name == 'ID':
            return self._identities.assign(self)
        return self._decode(*self._get(self.pointer, name), name)

    def call(self, name, index):
        if self.closed:
            raise ValueError('Released Feature interface')
        if name == 'Item' and type(index) is int and index >= 1:
            argument = index
        elif name == 'IsSame' and isinstance(index, FeatureDispatch) and not index.closed:
            argument = index.pointer
        else:
            raise ValueError('Only read-only Item and IsSame are supported')
        return self._decode(*self._item(self.pointer, name, argument), name)

    def describe(self, resolve_methods=None):
        if resolve_methods:
            return self._describe(self.pointer, resolve_methods=resolve_methods)
        if self._description is None:
            self._description = self._describe(self.pointer)
        return self._description

    def retain(self):
        if self.closed or self._retain is None:
            raise RuntimeError('Cannot retain Feature object for identity comparison')
        value = self._retain(self.pointer)
        clone = FeatureDispatch(self.pointer,self._get,self._item,self._clear,self._describe,
                                variant=value,retain=self._retain,identities=self._identities)
        clone._description = self._description
        return clone
