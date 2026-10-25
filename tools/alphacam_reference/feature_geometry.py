"""Read Feature's analytic geometry and topology through owned Automation objects.

This is a neutral snapshot, not an OCCT shape or a tessellated substitute.
No mutation, conversion or guessed native ABI is used. Unsupported optional
surface properties are reported explicitly; their absence is never success.
"""
import math
import json

MAX_ITEMS = 100000


class GeometryOperations:
    """Bound work by measured topology and keep repeated identity logs small."""
    def __init__(self, stream, counts):
        sizes = [counts.get(name) for name in ('Faces', 'Edges', 'Vertices')]
        if any(type(n) is not int or not 1 <= n <= MAX_ITEMS for n in sizes):
            raise ValueError('Invalid measured topology counts')
        self.stream = stream
        # IsSame comparisons are quadratic in each interface collection. A
        # fixed 250k budget stopped the user's valid 210/609/406-element body.
        self.limit = min(20000000, max(250000, 8 * sum(sizes) ** 2))
        self.sequence = self.comparisons = self.logged = 0

    def before(self, operation, name, argument=None):
        self.sequence += 1
        if self.sequence > self.limit:
            raise RuntimeError('Geometry operation limit exceeded: ' + str(self.limit))
        if name == 'IsSame':
            self.comparisons += 1
            if self.sequence > 1000 and self.sequence % 1000:
                return
        record = {'sequence': self.sequence, 'operation': operation, 'member': name}
        if name == 'Item':
            record['index'] = argument
        self.stream.write(json.dumps(record) + '\n')
        self.stream.flush()  # retain the last getter if the native host exits
        self.logged += 1

    def stats(self):
        return {'operation_count': self.sequence, 'operation_limit': self.limit,
                'is_same_count': self.comparisons, 'logged_operation_count': self.logged,
                'is_same_log_sampled': True}

    def finish(self):
        self.stream.write(json.dumps({'operation': 'summary', **self.stats()}) + '\n')
        self.stream.flush()


def boolean(value):
    if type(value) is not bool:
        raise ValueError('Invalid boolean value')
    return value


def integer(value):
    if type(value) is not int:
        raise ValueError('Invalid integer value')
    return value


def number(value):
    if type(value) is bool:
        raise ValueError('Boolean is not a geometry measurement')
    if type(value) in (int, float):
        result = float(value)
    elif value is not None:
        with value:
            result = float(value.get('Value'))
    else:
        raise ValueError('Missing numeric value')
    if not math.isfinite(result) or abs(result) > 1e12:
        raise ValueError('Invalid numeric value')
    return result


def ident(obj):
    value = obj.get('ID')
    if type(value) is not int:
        raise ValueError('Non-integer object ID')
    return value


def count(collection):
    n = collection.get('Count')
    if type(n) is not int or not 0 <= n <= MAX_ITEMS:
        raise ValueError('Invalid collection size')
    return n


def point(obj):
    return [number(obj.get(k)) for k in ('x', 'y', 'z')]


def vector(obj):
    return [number(obj.get(k)) for k in ('dx', 'dy', 'dz')]


def pv(obj):
    return {'point': point(obj), 'vector': vector(obj)}


def describe_type(obj):
    value = obj.get('Type')
    if type(value) is not int:
        raise ValueError('Invalid type enum')
    info = obj.describe({'type'})
    matches = []
    for fn in info.get('functions', []):
        if fn.get('name') == 'Type' and fn.get('invkind') == 2:
            for name, candidate in fn.get('return', {}).get('enum_values', {}).items():
                if candidate == value:
                    matches.append(name)
    return {'value': value, 'names': matches}


def references(obj, name):
    result = []
    with obj.get(name) as collection:
        for i in range(1, count(collection) + 1):
            with collection.call('Item', i) as member:
                result.append(ident(member))
    return result


def extract_geometry(body):
    result = {'schema': 'minicam-feature-geometry-v1', 'body_id': ident(body),
              'id_namespace': 'snapshot_local_identity_verified_by_IsSame',
              'units': 'mm', 'vertices': [], 'edges': [], 'faces': [],
              'property_errors': [], 'converted_to_occt': False}

    def optional(obj, name, convert, owner):
        try:
            value = obj.get(name)
            if value is None:
                raise ValueError('Null property')
            if hasattr(value, '__enter__'):
                with value:
                    return convert(value)
            return convert(value)
        except Exception as error:
            result['property_errors'].append({'owner': owner, 'property': name, 'error': str(error)})
            return None

    # The caller owns the body. Each returned interface is released before its
    # collection and before the parent Feature/CLS lifetime ends.
    with body.get('Vertices') as vertices:
        for i in range(1, count(vertices) + 1):
            with vertices.call('Item', i) as vertex:
                with vertex.get('Point') as location:
                    result['vertices'].append({'id': ident(vertex), 'point': point(location)})
    with body.get('Edges') as edges:
        for i in range(1, count(edges) + 1):
            with edges.call('Item', i) as edge:
                edge_id = ident(edge)
                entry = {'id': edge_id, 'type': describe_type(edge),
                         'vertices': references(edge, 'Vertices'),
                         'faces': references(edge, 'Faces')}
                for name, convert in [('Length', number), ('Radius', number), ('Center', point), ('Plane', pv)]:
                    entry[name.lower()] = optional(edge, name, convert, 'edge:' + str(edge_id))
                result['edges'].append(entry)
    with body.get('Faces') as faces:
        for i in range(1, count(faces) + 1):
            with faces.call('Item', i) as face:
                face_id = ident(face)
                entry = {'id': face_id, 'type': describe_type(face), 'loops': [],
                         'vertices': references(face, 'Vertices'), 'edges': references(face, 'Edges')}
                for name, convert in [('Area', number), ('Normal', vector), ('Plane', pv),
                                      ('IsPlanar', boolean)]:
                    entry[name.lower()] = optional(face, name, convert, 'face:' + str(face_id))
                # On the verified 2025 Feature build Color calls the host's
                # IDrawing.Options and terminates this isolated reader. Color
                # is display metadata; do not invent drawing options to get it.
                entry['color'] = None
                result['property_errors'].append({'owner': 'face:' + str(face_id),
                    'property': 'Color', 'error': 'Not requested: requires unsupported IDrawing.Options',
                    'reason': 'host_dependency_not_implemented'})

                def conic(obj):
                    data = {}
                    for name, convert in [('Axis', pv), ('Radius', number), ('Radius2', number),
                                          ('SemiAngle', number), ('StartAngle', number), ('EndAngle', number),
                                          ('XAxis', vector), ('Hollow', boolean)]:
                        data[name.lower()] = optional(obj, name, convert, 'face-conic:' + str(face_id))
                    return data

                # Do not assume that a conic accessor implements arbitrary
                # splines or tori. Preserve the actual enum for the consumer.
                if entry.get('isplanar') is not True:
                    entry['conic'] = optional(face, 'Conic', conic, 'face:' + str(face_id))
                with face.get('Loops') as loops:
                    for j in range(1, count(loops) + 1):
                        with loops.call('Item', j) as loop:
                            entry['loops'].append({'id': ident(loop), 'type': describe_type(loop),
                                                   'edges': references(loop, 'Edges'),
                                                   'vertices': references(loop, 'Vertices')})
                result['faces'].append(entry)
    validate_geometry(result)
    result['topology_read_complete'] = True
    return result


def validate_geometry(data):
    if data.get('schema') != 'minicam-feature-geometry-v1' or data.get('units') != 'mm':
        raise ValueError('Unsupported geometry schema or units')
    ids = {}
    for group in ('vertices', 'edges', 'faces'):
        values = data.get(group)
        if not isinstance(values, list) or not 1 <= len(values) <= MAX_ITEMS:
            raise ValueError('Invalid ' + group + ' list')
        ids[group] = set()
        for item in values:
            item_id = item.get('id')
            if type(item_id) is not int or item_id in ids[group]:
                raise ValueError('Duplicate or invalid ' + group + ' ID')
            ids[group].add(item_id)
            if group in ('edges', 'faces'):
                enum = item.get('type', {})
                if type(enum.get('value')) is not int or not isinstance(enum.get('names'), list):
                    raise ValueError('Invalid geometry type description')
    for vertex in data['vertices']:
        p = vertex.get('point')
        if not isinstance(p, list) or len(p) != 3:
            raise ValueError('Invalid vertex point')
        for coordinate in p:
            number(coordinate)
    for edge in data['edges']:
        for group in ('vertices', 'faces'):
            if any(type(k) is not int or k not in ids[group] for k in edge[group]):
                raise ValueError('Dangling edge reference')
    edges_by_id = {edge['id']: edge for edge in data['edges']}
    for face in data['faces']:
        for group in ('vertices', 'edges'):
            if any(type(k) is not int or k not in ids[group] for k in face[group]):
                raise ValueError('Dangling face reference')
        if not face.get('loops'):
            raise ValueError('Face has no decoded trimming loops')
        for loop in face['loops']:
            if not loop.get('edges') or any(k not in ids['edges'] or k not in face['edges'] for k in loop['edges']):
                raise ValueError('Dangling loop edge reference')
            if any(k not in ids['vertices'] or k not in face['vertices'] for k in loop.get('vertices', [])):
                raise ValueError('Dangling loop vertex reference')
        for edge in face['edges']:
            match = edges_by_id[edge]
            if face['id'] not in match['faces']:
                raise ValueError('Face-edge references are not reciprocal')
    return True
