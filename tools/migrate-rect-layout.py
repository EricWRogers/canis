#!/usr/bin/env python3
"""Migrate saved scenes/prefabs to Transform + RectLayout, retaining other text.

Usage: python3 canis/tools/migrate-rect-layout.py [--write] project/assets
Requires PyYAML. Without --write, reports files needing migration.
"""
import argparse
import math
from pathlib import Path
import yaml

class Dumper(yaml.SafeDumper):
    pass
Dumper.add_representer(list, lambda d, v: d.represent_sequence('tag:yaml.org,2002:seq', v, flow_style=True))

LAYOUT_FIELDS = ('active', 'size', 'anchorMin', 'anchorMax', 'pivot', 'originOffset', 'rotationOriginOffset')

def quat(value):
    if len(value) == 4:
        return value
    x, y, z = (v * .5 for v in value)
    cx, cy, cz, sx, sy, sz = math.cos(x), math.cos(y), math.cos(z), math.sin(x), math.sin(y), math.sin(z)
    return [sx*cy*cz-cx*sy*sz, cx*sy*cz+sx*cy*sz, cx*cy*sz-sx*sy*cz, cx*cy*cz+sx*sy*sz]

def multiply(a, b):
    x,y,z,w=a; X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y, w*Y-x*Z+y*W+z*X, w*Z+x*Y-y*X+z*W, w*W-x*X-y*Y-z*Z]

def rotate(q, v):
    return multiply(multiply(q, [*v, 0]), [-q[0], -q[1], -q[2], q[3]])[:3]

def components(entity):
    rect = entity['Canis::RectTransform']
    spatial = dict(entity.get('Canis::Transform') or {})
    xy = rect.get('position', [0, 0]); depth = rect.get('depth', .001)
    scale = rect.get('scale', [1, 1]); angle = rect.get('rotation', 0)
    if (entity.get('Canis::Canvas') or {}).get('renderMode') == 2:
        old_pos = spatial.get('position', [0, 0, 0]); old_scale = spatial.get('scale', [1, 1, 1])
        old_rot = quat(spatial.get('rotation', [0, 0, 0]))
        delta = rotate(old_rot, [old_scale[0]*xy[0], old_scale[1]*xy[1], old_scale[2]*depth])
        spatial.update(position=[a+b for a,b in zip(old_pos, delta)],
                       rotation=multiply(old_rot, quat([0, 0, angle])),
                       scale=[old_scale[0]*scale[0], old_scale[1]*scale[1], old_scale[2]])
    else:
        spatial.update(position=[*xy, depth], rotation=[0, 0, angle], scale=[*scale, 1])
    spatial.setdefault('active', True)
    spatial['parent'] = rect.get('parent') or spatial.get('parent', 0)
    spatial['children'] = list(dict.fromkeys([*(spatial.get('children') or []), *(rect.get('children') or [])]))
    return spatial, {key: rect[key] for key in LAYOUT_FIELDS if key in rect}

def migrate(text):
    if 'Canis::RectTransform:' not in text:
        return text, 0
    document = yaml.compose(text)
    lines = text.splitlines(keepends=True)
    edits = []; count = 0
    def walk(node):
        nonlocal count
        if isinstance(node, yaml.MappingNode):
            fields = {key.value: (key, value) for key, value in node.value}
            if 'Entity' in fields and 'Canis::RectTransform' in fields:
                value = yaml.safe_load(yaml.serialize(node))
                if 'Canis::RectLayout' in value:
                    raise ValueError('Both old and new layout components exist on one entity')
                spatial, layout = components(value)
                key, old = fields['Canis::RectTransform']
                replacements = {'Canis::RectLayout': layout}
                if 'Canis::Transform' not in fields:
                    replacements = {'Canis::Transform': spatial, **replacements}
                def replace(key, value, data):
                    indent = ' ' * key.start_mark.column
                    block = yaml.dump(data, Dumper=Dumper, sort_keys=False, width=120)
                    end_line = value.end_mark.line + (1 if value.end_mark.index == len(text) and value.end_mark.column else 0)
                    edits.append((key.start_mark.line, end_line, ''.join(indent+line+'\n' for line in block.splitlines())))
                replace(key, old, replacements)
                if 'Canis::Transform' in fields:
                    key, old = fields['Canis::Transform']; replace(key, old, {'Canis::Transform': spatial})
                count += 1
            else:
                for _, child in node.value: walk(child)
        elif isinstance(node, yaml.SequenceNode):
            for child in node.value: walk(child)
    walk(document)
    for start, end, replacement in sorted(edits, reverse=True):
        lines[start:end] = [replacement]
    updated = ''.join(lines)
    yaml.safe_load(updated)  # Never write a malformed scene.
    return updated, count

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true'); parser.add_argument('paths', nargs='+', type=Path)
    args=parser.parse_args(); total=0
    paths=set()
    for path in args.paths:
        paths.update(path.rglob('*.scene') if path.is_dir() else [path])
    for path in sorted(paths):
        original=path.read_text(); updated,count=migrate(original)
        if count:
            if args.write:path.write_text(updated)
            print(f'{path}: {count} entities'+(' migrated' if args.write else ' need migration'))
            total+=count
    print(f'{total} layouts'+(' migrated' if args.write else ' need migration'))
if __name__ == '__main__': main()
