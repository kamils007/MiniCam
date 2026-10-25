from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'alphacam_reference'))
from feature_geometry import extract_geometry, validate_geometry, number
from feature_dispatch import FeatureDispatch


class FakeObject:
    def __init__(self, node, tracker):
        self.node, self.tracker, self.closed = node, tracker, False
        tracker['opened'] += 1

    def __enter__(self):
        if self.closed: raise RuntimeError('Already closed')
        return self

    def __exit__(self, *_):
        if not self.closed:
            self.closed = True
            self.tracker['closed'] += 1

    def get(self, name):
        if self.closed: raise RuntimeError('Read after close')
        if name == 'Color':
            raise SystemExit('Native Color getter must not be called without drawing options')
        value = len(self.node) if isinstance(self.node, list) and name == 'Count' else self.node[name]
        return FakeObject(value, self.tracker) if isinstance(value, (dict, list)) else value

    def call(self, name, index):
        if self.closed or name != 'Item': raise RuntimeError('Unexpected mutation')
        return FakeObject(self.node[index - 1], self.tracker)

    def describe(self, names):
        return {'functions': [{'name': 'Type', 'invkind': 2,
            'return': {'enum_values': {self.node['_type_name']: self.node['Type']}}}]}


def body():
    vertices = [{'ID': i + 1, 'Point': dict(zip(('x','y','z'), p))}
                for i,p in enumerate([(0.,0.,2.), (3.,0.,2.), (0.,4.,2.)])]
    face = {'ID': 7, 'Type': 9, '_type_name': 'SyntheticPlane', 'Vertices': vertices,
            'Area': {'Value': 6.}, 'IsPlanar': True, 'Color': 12,
            'Normal': {'dx':0., 'dy':0., 'dz':1.},
            'Plane': {'x':0., 'y':0., 'z':2., 'dx':0., 'dy':0., 'dz':1.}}
    edges = [{'ID': i + 20, 'Type': 3, '_type_name': 'SyntheticLine', 'Length': length,
              'Vertices': [vertices[a],vertices[b]], 'Faces': [face]}
             for i,(a,b,length) in enumerate([(0,1,3.),(1,2,5.),(2,0,4.)])]
    face['Edges'] = edges
    face['Loops'] = [{'ID': 50, 'Type': 6, '_type_name': 'SyntheticOuterLoop',
                      'Edges': edges, 'Vertices': vertices}]
    return {'ID': 100, 'Vertices': vertices, 'Edges': edges, 'Faces': [face]}


class GeometryTests(unittest.TestCase):
    def snapshot(self):
        tracker = {'opened':0, 'closed':0}
        with FakeObject(body(), tracker) as obj:
            result = extract_geometry(obj)
        self.assertEqual(tracker['opened'], tracker['closed'])
        return result

    def test_actual_points_loops_types_and_explicit_unsupported_properties(self):
        data = self.snapshot()
        self.assertEqual(data['vertices'][2]['point'], [0.,4.,2.])
        self.assertEqual(data['faces'][0]['loops'][0]['edges'], [20,21,22])
        self.assertEqual(data['faces'][0]['area'], 6.)
        self.assertIsNone(data['faces'][0]['color'])
        self.assertTrue(any(e.get('reason') == 'host_dependency_not_implemented'
                            and e['property'] == 'Color' for e in data['property_errors']))
        self.assertEqual(data['faces'][0]['plane']['point'], [0.,0.,2.])
        self.assertEqual(data['edges'][0]['type']['names'], ['SyntheticLine'])
        self.assertTrue(data['property_errors'])
        self.assertIsNone(data['edges'][0]['radius'])
        self.assertFalse(data['converted_to_occt'])
        self.assertTrue(data['topology_read_complete'])

    def test_dangling_references_duplicate_ids_nonfinite_points_and_lost_loops(self):
        for case in ('edge', 'loop', 'vertex', 'duplicate', 'nan', 'units', 'reciprocal'):
            data = deepcopy(self.snapshot())
            if case == 'edge': data['faces'][0]['edges'].append(999)
            if case == 'loop': data['faces'][0]['loops'] = []
            if case == 'vertex': data['edges'][0]['vertices'].append(999)
            if case == 'duplicate': data['vertices'][1]['id'] = data['vertices'][0]['id']
            if case == 'nan': data['vertices'][0]['point'][0] = float('nan')
            if case == 'units': data['units'] = 'unknown'
            if case == 'reciprocal': data['edges'][0]['faces'] = []
            with self.subTest(case=case), self.assertRaises(ValueError): validate_geometry(data)

    def test_numeric_wrappers_released_and_boolean_rejected(self):
        tracker = {'opened':0, 'closed':0}
        self.assertEqual(number(FakeObject({'Value':3.25},tracker)), 3.25)
        self.assertEqual(tracker['opened'], tracker['closed'])
        with self.assertRaises(ValueError): number(True)

    def test_dispatch_transfers_and_releases_ownership_once(self):
        from types import SimpleNamespace as Obj
        cleared = []
        scalar = Obj(vt=3, value=Obj(integer=17))
        variant = Obj(vt=9, value=Obj(pointer=123))
        def getter(pointer,name): return {'ok':True}, variant if name == 'child' else scalar
        def clear(v): cleared.append(v)
        with FeatureDispatch(1,getter,None,clear,None) as parent:
            self.assertEqual(parent.get('number'),17)
            with parent.get('child') as child:
                self.assertEqual(child.pointer,123)
            child.close()
        self.assertEqual(cleared,[scalar,variant])
        with self.assertRaises(ValueError): parent.call('Delete',1)

    def test_snapshot_ids_use_is_same_when_declared_native_id_is_unavailable(self):
        from types import SimpleNamespace as Obj
        retained, released, compared = [], [], []
        entities = {10:'vertex-a',11:'vertex-a',12:'vertex-b'}
        def getter(pointer,name):
            if name == 'ID': raise AssertionError('Native ID must never be called')
            return {'ok':True}, Obj(vt=9,value=Obj(pointer={'a':10,'alias':11,'b':12}[name]))
        def call(pointer,name,other):
            self.assertEqual(name,'IsSame')
            compared.append((pointer,other))
            return {'ok':True}, Obj(vt=11,value=Obj(short=-1 if entities[pointer]==entities[other] else 0))
        def keep(pointer):
            retained.append(pointer)
            return Obj(vt=9,value=Obj(pointer=pointer),retained=True)
        def clear(value):
            if getattr(value,'retained',False): released.append(value.value.pointer)
        def describe(pointer,**kwargs):
            return {'name':'ISolidBody' if pointer==1 else 'ISolidVertex'}
        with FeatureDispatch(1,getter,call,clear,describe,retain=keep) as body:
            self.assertEqual(body.get('ID'),1)
            with body.get('a') as first: first_id=first.get('ID')
            with body.get('alias') as alias: self.assertEqual(alias.get('ID'),first_id)
            verified_comparisons = len(compared)
            for _ in range(10):
                with body.get('alias') as alias: self.assertEqual(alias.get('ID'),first_id)
            self.assertEqual(len(compared),verified_comparisons)
            with body.get('b') as other: self.assertNotEqual(other.get('ID'),first_id)
        self.assertEqual(retained,[1,10,11,12])
        self.assertEqual(released,retained)
        self.assertIn((11,10),compared)

    def test_failed_identity_comparison_releases_pins_without_inventing_an_id(self):
        from types import SimpleNamespace as Obj
        released=[]
        def keep(pointer):return Obj(vt=9,value=Obj(pointer=pointer))
        def get(pointer,name):
            if name != 'other': return {'ok':False},None
            return {'ok':True},Obj(vt=9,value=Obj(pointer=10))
        def call(pointer,name,other):return {'ok':False,'hresult':'0x80020003'},None
        def clear(value):released.append(value.value.pointer)
        with self.assertRaises(RuntimeError):
            with FeatureDispatch(1,get,call,clear,lambda *a,**k:{'name':'ISolidVertex'},retain=keep) as root:
                root.get('ID')
                with root.get('other') as other:other.get('ID')
        self.assertCountEqual(released,[10,1])

    def test_topology_scaled_budget_and_sampled_comparisons_pass_report_failure_point(self):
        import io
        import json
        from feature_geometry import GeometryOperations
        trace = io.StringIO()
        operations = GeometryOperations(trace, {'Faces':210, 'Edges':609, 'Vertices':406})
        for _ in range(250001):
            operations.before('call', 'IsSame')
        operations.before('get', 'Vertices')
        operations.finish()
        records = [json.loads(line) for line in trace.getvalue().splitlines()]
        self.assertEqual(records[-2]['member'], 'Vertices')
        self.assertEqual(records[-1]['operation_count'], 250002)
        self.assertLess(len(records), 2000)
        self.assertGreater(operations.limit, 250002)
        self.assertLessEqual(operations.limit, 20000000)
        operations.sequence = operations.limit
        with self.assertRaisesRegex(RuntimeError, 'Geometry operation limit exceeded'):
            operations.before('get', 'Vertices')
        with self.assertRaises(ValueError):
            GeometryOperations(io.StringIO(), {'Faces':True, 'Edges':609, 'Vertices':406})


if __name__ == '__main__': unittest.main()
