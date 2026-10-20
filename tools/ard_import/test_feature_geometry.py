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


if __name__ == '__main__': unittest.main()
