"""Variant ownership adapter for the geometry snapshot reader (no native ABI)."""
import ctypes as C


class FeatureDispatch:
    def __init__(self, pointer, get_property, item, clear, describe, variant=None):
        self.pointer = pointer
        self._get = get_property
        self._item = item
        self._clear = clear
        self._describe = describe
        self._variant = variant
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
                                      self._describe, variant=value)
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
        return self._decode(*self._get(self.pointer, name), name)

    def call(self, name, index):
        # Read-only collection indexing is the only method this adapter calls.
        if self.closed or name != 'Item' or type(index) is not int or index < 1:
            raise ValueError('Only read-only Item indexing is supported')
        return self._decode(*self._item(self.pointer, index), name)

    def describe(self, resolve_methods):
        return self._describe(self.pointer, resolve_methods=resolve_methods)
