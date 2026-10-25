"""Search hints prioritize IsSame candidates; they never establish identity."""
import itertools
from pathlib import Path
import sys
from types import SimpleNamespace as Obj
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'alphacam_reference'))
from feature_dispatch import SnapshotIds, FeatureDispatch


class Alias:
    pointers = itertools.count(1)

    def __init__(self, entity, hint, pointer=None):
        self.entity, self.hint = entity, hint
        self.pointer = next(self.pointers) if pointer is None else pointer

    def describe(self): return {'name': 'ISolidVertex'}
    def identity_hint(self, group): return self.hint
    def call(self, method, other):
        if method != 'IsSame': raise AssertionError('Unexpected operation')
        return self.entity == other.entity
    def retain(self): return Alias(self.entity, self.hint, self.pointer)
    def close(self): pass


class IdentitySearchTests(unittest.TestCase):
    def test_identical_geometry_does_not_merge_distinct_native_objects(self):
        ids = SnapshotIds()
        try:
            first = ids.assign(Alias('a', (1., 2., 3.)))
            second = ids.assign(Alias('b', (1., 2., 3.)))
            self.assertNotEqual(first, second)
            self.assertEqual(ids.assign(Alias('b', (1., 2., 3.))), second)
        finally: ids.close()

    def test_changed_or_unavailable_hint_falls_back_to_all_native_objects(self):
        ids = SnapshotIds()
        try:
            first = ids.assign(Alias('a', (1., 2., 3.)))
            ids.assign(Alias('b', (4., 5., 6.)))
            self.assertEqual(ids.assign(Alias('a', (1.0000001, 2., 3.))), first)
            self.assertEqual(ids.assign(Alias('a', None)), first)
            before = ids.comparisons
            self.assertEqual(ids.assign(Alias('a', (1.0000001, 2., 3.))), first)
            self.assertEqual(ids.comparisons - before, 1)
        finally: ids.close()

    def test_repeated_fresh_com_wrappers_keep_ids_and_reduce_comparisons(self):
        def replay(enabled):
            ids = SnapshotIds()
            try:
                result = []
                for n in list(range(200)) + [i % 200 for i in range(2000)]:
                    result.append(ids.assign(Alias(n, (n, 0., 0.) if enabled else None)))
                return result, ids.comparisons
            finally: ids.close()
        original, baseline = replay(False)
        optimized, comparisons = replay(True)
        self.assertEqual(original, optimized)
        self.assertLess(comparisons, baseline // 2)

    def test_dispatch_hints_read_and_release_known_numeric_wrappers(self):
        cleared = []
        def double(value): return Obj(vt=5, value=Obj(number=value))
        def dispatch(pointer): return Obj(vt=9, value=Obj(pointer=pointer))
        def getter(pointer, name):
            if name == 'Point': return {'ok': True}, dispatch(42)
            if pointer == 42: return {'ok': True}, double({'x': 1.1, 'y': 2.2, 'z': 3.3}[name])
            if name == 'Type': return {'ok': True}, Obj(vt=3, value=Obj(integer=2))
            if name in ('Length', 'Area'): return {'ok': True}, dispatch(43)
            if pointer == 43 and name == 'Value': return {'ok': True}, double(1.75)
            raise KeyError(name)
        def clear(value):
            if value.vt == 9: cleared.append(value.value.pointer)
        with FeatureDispatch(1, getter, None, clear, None) as obj:
            self.assertEqual(obj.identity_hint('ISolidVertex'), (1.1, 2.2, 3.3))
            self.assertEqual(obj.identity_hint('ISolidEdge'), (2, 1.75))
            self.assertEqual(obj.identity_hint('ISolidFace'), (2, 1.75))
            self.assertIsNone(obj.identity_hint('UnknownInterface'))
        self.assertCountEqual(cleared, [42, 43, 43])


if __name__ == '__main__': unittest.main()
