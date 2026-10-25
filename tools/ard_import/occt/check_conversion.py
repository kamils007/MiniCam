"""Native OCCT acceptance checks using a synthetic box, no user/vendor files.

Usage: python3 check_conversion.py /path/to/ard_snapshot_to_occt
"""
from copy import deepcopy
import json
import hashlib
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXECUTABLE = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else None


def box():
    points = [(0, 0, 0), (2, 0, 0), (2, 3, 0), (0, 3, 0),
              (0, 0, 4), (2, 0, 4), (2, 3, 4), (0, 3, 4)]
    polygons = [([1, 2, 3, 4], [0, 0, -1], 6),
                ([5, 6, 7, 8], [0, 0, 1], 6),
                ([1, 2, 6, 5], [0, -1, 0], 8),
                ([2, 3, 7, 6], [1, 0, 0], 12),
                ([3, 4, 8, 7], [0, 1, 0], 8),
                ([4, 1, 5, 8], [-1, 0, 0], 12)]
    edges, faces, lookup = [], [], {}
    for i, (vertices, normal, area) in enumerate(polygons):
        refs = []
        for a, b in zip(vertices, vertices[1:] + vertices[:1]):
            key = tuple(sorted((a, b)))
            if key not in lookup:
                number = len(edges) + 20
                lookup[key] = number
                length = sum((x-y)**2 for x, y in zip(points[a-1], points[b-1]))**0.5
                edges.append({'id': number, 'vertices': [a, b], 'length': length,
                              'type': {'names': ['FeatureLinearEdge']}})
            refs.append(lookup[key])
        faces.append({'id': 100+i, 'type': {'names': ['FeaturePlanarFace']},
                      'plane': {'point': points[vertices[0]-1], 'vector': normal},
                      'edges': refs, 'area': area,
                      'loops': [{'type': {'names': ['FeatureOuterLoop']}, 'edges': refs}]})
    return {'schema': 'minicam-feature-geometry-v1', 'units': 'mm',
            'topology_read_complete': True,
            'vertices': [{'id': i+1, 'point': p} for i, p in enumerate(points)],
            'edges': edges, 'faces': faces}


def curved_model(mode):
    """Independent analytic solids: full/half cylinder, oblique quarter cylinder."""
    r, h, slope = 2., 4., .5
    root = {'schema': 'minicam-feature-geometry-v1', 'units': 'mm',
            'topology_read_complete': True, 'vertices': [], 'edges': [], 'faces': []}
    def vertex(n, p):
        root['vertices'].append({'id': n, 'point': p})
    def edge(n, a, b, length, kind, faces, z=0):
        value = {'id': n, 'vertices': [a, b], 'length': length,
                 'type': {'names': [kind]}, 'faces': faces}
        if kind == 'FeatureCircularEdge':
            value.update(radius=r, center=[0, 0, z],
                         plane={'point': [0, 0, z], 'vector': [0, 0, 1]})
        root['edges'].append(value)
    def plane(n, p, normal, area, edges):
        root['faces'].append({'id': n, 'type': {'names': ['FeaturePlanarFace']},
            'plane': {'point': p, 'vector': normal}, 'area': area, 'edges': edges,
            'loops': [{'type': {'names': ['FeatureOuterLoop']}, 'edges': edges}]})
    def cylinder(area, edges, span):
        root['faces'].append({'id': 102, 'type': {'names': ['FeatureCylindricalFace']},
            'conic': {'axis': {'point': [0, 0, 0], 'vector': [0, 0, 1]},
                      'radius': r, 'startangle': 0, 'endangle': span, 'xaxis': [1, 0, 0]},
            'area': area, 'edges': edges,
            'loops': [{'type': {'names': ['FeatureOuterLoop']}, 'edges': edges}]})
    if mode == 'full':
        vertex(1, [r, 0, 0]); vertex(2, [r, 0, h])
        edge(20, 1, 1, 2*math.pi*r, 'FeatureCircularEdge', [100, 102])
        edge(21, 2, 2, 2*math.pi*r, 'FeatureCircularEdge', [101, 102], h)
        edge(22, 1, 2, h, 'FeatureLinearEdge', [102])
        plane(100, [0, 0, 0], [0, 0, -1], math.pi*r*r, [20])
        plane(101, [0, 0, h], [0, 0, 1], math.pi*r*r, [21])
        cylinder(2*math.pi*r*h, [20, 22, 21], 2*math.pi)
        return root, math.pi*r*r*h, (3, 3, 2)
    if mode == 'half':
        for n, p in enumerate([[-r, 0, 0], [r, 0, 0], [-r, 0, h], [r, 0, h]], 1): vertex(n, p)
        edge(20, 1, 2, math.pi*r, 'FeatureCircularEdge', [100, 102])
        edge(21, 2, 1, 2*r, 'FeatureLinearEdge', [100, 103])
        edge(22, 3, 4, math.pi*r, 'FeatureCircularEdge', [101, 102], h)
        edge(23, 4, 3, 2*r, 'FeatureLinearEdge', [101, 103])
        edge(24, 1, 3, h, 'FeatureLinearEdge', [102, 103])
        edge(25, 2, 4, h, 'FeatureLinearEdge', [102, 103])
        plane(100, [0, 0, 0], [0, 0, -1], math.pi*r*r/2, [20, 21])
        plane(101, [0, 0, h], [0, 0, 1], math.pi*r*r/2, [22, 23])
        plane(103, [0, 0, 0], [0, -1, 0], 2*r*h, [21, 24, 23, 25])
        cylinder(math.pi*r*h, [20, 25, 22, 24], math.pi)
        return root, math.pi*r*r*h/2, (4, 6, 4)
    for n, p in enumerate([[r, 0, 0], [0, r, 0], [0, 0, 0],
                           [r, 0, h+slope*r], [0, r, h], [0, 0, h]], 1): vertex(n, p)
    # Simpson integration is independent of OCCT's elliptic curve length routine.
    count = 4096
    step = math.pi/2/count
    samples = [r*math.sqrt(1+slope*slope*math.sin(i*step)**2) for i in range(count+1)]
    ellipse_length = step/3*(samples[0]+samples[-1]+4*sum(samples[1:-1:2])+2*sum(samples[2:-1:2]))
    edge(20, 1, 2, math.pi*r/2, 'FeatureCircularEdge', [100, 102])
    edge(21, 2, 3, r, 'FeatureLinearEdge', [100, 103])
    edge(22, 3, 1, r, 'FeatureLinearEdge', [100, 104])
    edge(23, 4, 5, ellipse_length, 'FeatureSplineEdge', [101, 102])
    edge(24, 5, 6, r, 'FeatureLinearEdge', [101, 103])
    edge(25, 6, 4, r*math.sqrt(1+slope*slope), 'FeatureLinearEdge', [101, 104])
    edge(26, 1, 4, h+slope*r, 'FeatureLinearEdge', [102, 104])
    edge(27, 2, 5, h, 'FeatureLinearEdge', [102, 103])
    edge(28, 3, 6, h, 'FeatureLinearEdge', [103, 104])
    plane(100, [0, 0, 0], [0, 0, -1], math.pi*r*r/4, [20, 21, 22])
    plane(101, [0, 0, h], [-slope, 0, 1], math.pi*r*r/4*math.sqrt(1+slope*slope), [23, 24, 25])
    plane(103, [0, 0, 0], [-1, 0, 0], r*h, [21, 28, 24, 27])
    plane(104, [0, 0, 0], [0, -1, 0], r*h+slope*r*r/2, [22, 26, 25, 28])
    cylinder(r*h*math.pi/2+slope*r*r, [20, 27, 23, 26], math.pi/2)
    return root, math.pi*r*r*h/4+slope*r**3/3, (5, 9, 6)


@unittest.skipUnless(EXECUTABLE, 'Supply the native converter executable')
class ConversionChecks(unittest.TestCase):
    def run_model(self, data, existing=False, report_case=None):
        with tempfile.TemporaryDirectory() as tmp:
            source, target = Path(tmp)/'input.json', Path(tmp)/'output.step'
            source.write_text(json.dumps(data), encoding='utf-8')
            args = [EXECUTABLE, str(source), str(target)]
            report_path = Path(tmp)/'read_report.json'
            if report_case:
                report = {
                    'schema': 'minicam-ard-feature-read-v1', 'read_confirmed': True,
                    'geometry_snapshot_available': True, 'installed_runtime_unchanged': True,
                    'source_ard_unchanged': True, 'save_requested': False,
                    'input': {'sha256': 'a'*64}, 'extraction': {'parasolid_sha256': 'b'*64},
                    'geometry_snapshot': {'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                                          'topology_read_complete': True},
                    'measurement': {'topology_counts': {'Faces': 6, 'Edges': 12, 'Vertices': 8},
                                    'bounds': {'MinX': 0, 'MinY': 0, 'MinZ': 0,
                                               'MaxX': 2, 'MaxY': 3, 'MaxZ': 4}}}
                if report_case == 'source': report['input']['sha256'] = 'c'*64
                if report_case == 'payload': report['extraction']['parasolid_sha256'] = 'c'*64
                if report_case == 'hash': report['geometry_snapshot']['sha256'] = 'c'*64
                if report_case == 'count': report['measurement']['topology_counts']['Faces'] = 5
                if report_case == 'bounds': report['measurement']['bounds']['MaxZ'] = 5
                if report_case == 'failed': report['read_confirmed'] = False
                if report_case == 'changed': report['installed_runtime_unchanged'] = False
                if report_case == 'save': report['save_requested'] = True
                report_path.write_text(json.dumps(report), encoding='utf-8')
                args += ['--report', str(report_path), '--source-sha256', 'a'*64,
                         '--payload-sha256', 'b'*64]
            if existing:
                target.write_text('preserve this file', encoding='utf-8')
            result = subprocess.run(args,
                                    capture_output=True, text=True, timeout=30)
            outputs = {p.name: p.read_bytes() for p in Path(tmp).iterdir() if p not in (source, report_path)}
            return result, outputs

    def test_box_valid_solid_volume_topology_and_step_readback(self):
        result, outputs = self.run_model(box())
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout[result.stdout.rfind('\n{')+1:])
        self.assertTrue(report['occt_solid_valid'])
        self.assertTrue(report['step_readback_valid'])
        self.assertAlmostEqual(report['volume_mm3'], 24.)
        self.assertAlmostEqual(report['area_mm2'], 52.)
        self.assertEqual((report['faces'], report['edges'], report['vertices']), (6, 12, 8))
        self.assertIn('output.step', outputs)
        self.assertIn('output.step.brep', outputs)

    def test_inconsistent_lengths_areas_and_unsupported_surfaces_rejected(self):
        for case in ('length', 'area', 'surface', 'open_loop'):
            data = deepcopy(box())
            if case == 'length':
                data['edges'][0]['length'] += 1
            elif case == 'area':
                data['faces'][0]['area'] += 1
            elif case == 'surface':
                data['faces'][0]['type']['names'] = ['UnsupportedSpline']
            else:
                data['faces'][0]['loops'][0]['edges'].pop()
            with self.subTest(case=case):
                result, outputs = self.run_model(data)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(outputs, {})

    def test_complete_circles_semicircles_and_exact_ellipse_intersection(self):
        for mode in ('full', 'half', 'ellipse'):
            with self.subTest(mode=mode):
                data, volume, counts = curved_model(mode)
                result, outputs = self.run_model(data)
                self.assertEqual(result.returncode, 0, result.stderr)
                report = json.loads(result.stdout[result.stdout.rfind('\n{')+1:])
                self.assertTrue(report['occt_solid_valid'] and report['step_readback_valid'])
                self.assertAlmostEqual(report['volume_mm3'], volume, places=6)
                self.assertEqual((report['faces'], report['edges'], report['vertices']), counts)

    def test_inconsistent_closed_circle_and_ellipse_data_rejected(self):
        for mode in ('full', 'ellipse'):
            data, _, _ = curved_model(mode)
            target = next(e for e in data['edges'] if e['id'] == (20 if mode == 'full' else 23))
            target['length'] += .1
            with self.subTest(mode=mode):
                result, outputs = self.run_model(data)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(outputs, {})

    def test_existing_output_preserved(self):
        result, outputs = self.run_model(box(), existing=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(outputs, {'output.step': b'preserve this file'})

    def test_gui_report_contract_requires_matching_source_geometry_counts_and_bounds(self):
        result, outputs = self.run_model(box(), report_case='valid')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('output.step', outputs)
        for case in ('source', 'payload', 'hash', 'count', 'bounds', 'failed', 'changed', 'save'):
            with self.subTest(case=case):
                result, outputs = self.run_model(box(), report_case=case)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(outputs, {})


if __name__ == '__main__':
    unittest.main()
