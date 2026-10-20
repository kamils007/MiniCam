"""Native OCCT acceptance checks using a synthetic box, no user/vendor files.

Usage: python3 check_conversion.py /path/to/ard_snapshot_to_occt
"""
from copy import deepcopy
import json
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


@unittest.skipUnless(EXECUTABLE, 'Supply the native converter executable')
class ConversionChecks(unittest.TestCase):
    def run_model(self, data, existing=False):
        with tempfile.TemporaryDirectory() as tmp:
            source, target = Path(tmp)/'input.json', Path(tmp)/'output.step'
            source.write_text(json.dumps(data), encoding='utf-8')
            if existing:
                target.write_text('preserve this file', encoding='utf-8')
            result = subprocess.run([EXECUTABLE, str(source), str(target)],
                                    capture_output=True, text=True, timeout=30)
            outputs = {p.name: p.read_bytes() for p in Path(tmp).iterdir() if p != source}
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

    def test_existing_output_preserved(self):
        result, outputs = self.run_model(box(), existing=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(outputs, {'output.step': b'preserve this file'})


if __name__ == '__main__':
    unittest.main()
