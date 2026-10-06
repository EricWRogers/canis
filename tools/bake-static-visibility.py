#!/usr/bin/env python3
"""Partition a rigid opaque glTF and bake conservative, same-asset triangle occlusion.
No sampled-ray visibility guesses: each hidden AABB lies inside a triangle's shadow
volume for every corner of the camera cell. Borderline cases remain visible.
Input must contain only opaque immutable geometry (no doors, skinning or displacement).
"""
import argparse,collections,itertools,json,time,zlib,copy,os,re
from pathlib import Path
import numpy as np

CORNERS=np.array(list(itertools.product([0.,1.],repeat=3)))
def blocked_by_triangle(eye_min,eye_max,minimum,maximum,tri,epsilon=.002):
    """Conservative proof for every eye in the cell and every point in target AABB."""
    centers=(minimum+maximum)*.5;extents=(maximum-minimum)*.5
    eyes=eye_min+CORNERS*(eye_max-eye_min)
    n=np.cross(tri[1]-tri[0],tri[2]-tri[0]);length=np.linalg.norm(n)
    if length<1e-7:return np.zeros(len(minimum),bool)
    n/=length;dist=(eyes-tri[0])@n
    if np.all(dist < -epsilon):n=-n;dist=-dist
    if not np.all(dist>epsilon):return np.zeros(len(minimum),bool)
    # Targets must lie completely beyond the opaque face, never on its plane.
    blocked=((centers-tri[0])@n + extents@abs(n)) < -epsilon
    for eye in eyes:
        for a,b,c in [(0,1,2),(1,2,0),(2,0,1)]:
            side=np.cross(tri[a]-eye,tri[b]-eye);length=np.linalg.norm(side)
            if length<1e-7:return np.zeros(len(minimum),bool)
            side/=length
            if np.dot(side,tri[c]-eye)<0:side=-side
            blocked &= ((centers-eye)@side-extents@abs(side))>epsilon
        if not blocked.any():break
    return blocked

def triangle_rooms(node,primitive,count):
    """Room provenance partitions geometry; it never proves room-to-room occlusion."""
    ranges=primitive.get('extras',{}).get('canisRoomRanges')
    if ranges is not None:
        ids=np.empty(count,dtype=np.int32);names=[];first=0
        for part in ranges:
            assert part['first']==first and isinstance(part['count'],int) and part['count']>0, 'Room ranges must cover triangles exactly once in source order'
            end=first+part['count'];assert end<=count, 'Room range exceeds primitive'
            name=str(part['room']);assert name, 'Room name cannot be empty'
            if name not in names:names.append(name)
            ids[first:end]=names.index(name);first=end
        assert first==count, 'Room ranges leave triangles unassigned'
        return names,ids
    names=set(re.findall(r'(ss_room_\d+|ss_corridor_\d+[a-z]?|ss_tunnel_[^|+\n]+)\+',node.get('extras',{}).get('UnkName','')))
    # A source instance can list several original rooms. Keep it shared rather than
    # assigning everything to the first name and treating that as visibility proof.
    return [next(iter(names)) if len(names)==1 else 'shared'],np.zeros(count,dtype=np.int32)

def bake(source,output,chunk_size,minimum,dimensions,cell_size,max_occluders,room_partition=False):
    start=time.monotonic();g=json.loads(source.read_text())
    assert not g.get('skins') and not g.get('animations'), 'Only rigid, immutable geometry can be baked'
    buffers=[(source.parent/b['uri']).read_bytes()for b in g['buffers']]
    source_crc=zlib.crc32(source.read_bytes())
    for buffer in buffers:source_crc=zlib.crc32(buffer,source_crc)
    def read(i):
        a=g['accessors'][i];v=g['bufferViews'][a['bufferView']];width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        dtype=np.dtype({5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]);stride=v.get('byteStride',dtype.itemsize*width)
        assert not a.get('normalized') and not a.get('sparse'), 'Expand normalized/sparse accessors before baking'
        return np.ndarray((a['count'],width),dtype=dtype,buffer=buffers[v['buffer']],offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(stride,dtype.itemsize)).copy()
    parents={child:i for i,n in enumerate(g['nodes'])for child in n.get('children',[])}
    def matrix(i):
        n=g['nodes'][i]
        assert not any(k in n for k in ['translation','rotation','scale']), 'Flatten TRS nodes to matrices first'
        m=np.array(n.get('matrix',np.eye(4).T.reshape(-1))).reshape(4,4).T
        return matrix(parents[i])@m if i in parents else m
    groups=collections.OrderedDict();group_names={};occluders=[];triangle_count=0;primitive_number=0
    active=set()
    def visit(i):
        if i in active:return
        active.add(i)
        for c in g['nodes'][i].get('children',[]):visit(c)
    for i in g['scenes'][g.get('scene',0)]['nodes']:visit(i)
    for i,n in enumerate(g['nodes']):
        if i not in active or 'mesh' not in n:continue
        m=matrix(i)
        for pr in g['meshes'][n['mesh']]['primitives']:
            assert pr.get('mode',4)==4 and not pr.get('targets')
            primitive_number+=1
            material=pr.get('material',-1)
            if material>=0:assert g['materials'][material].get('alphaMode','OPAQUE')=='OPAQUE'
            attrs={k:read(v) for k,v in pr['attributes'].items() if k in ('POSITION','NORMAL','TEXCOORD_0','COLOR_0')}
            attrs['POSITION']=(attrs['POSITION']@m[:3,:3].T+m[:3,3]).astype('<f4')
            if 'NORMAL' in attrs:
                normals=attrs['NORMAL']@np.linalg.inv(m[:3,:3]);normals/=np.maximum(np.linalg.norm(normals,axis=1)[:,None],1e-12);attrs['NORMAL']=normals.astype('<f4')
            ind=read(pr['indices']).reshape(-1,3).astype(np.uint32)
            if np.linalg.det(m[:3,:3])<0:ind=ind[:,[0,2,1]]
            triangles=attrs['POSITION'][ind];triangle_count+=len(ind)
            area=np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1)
            eligible=np.where(area>8)[0]
            if len(eligible):
                best=eligible[np.argsort(area[eligible])[-max_occluders:]]
                occluders.extend((float(area[k]),triangles[k].copy()) for k in best)
            cells=np.floor(triangles.mean(1)/chunk_size).astype(np.int32)
            # Coplanar glazing layers depend on authored draw order. Keep each
            # such source primitive whole; never merge across source primitives.
            if material>=0 and 'glass' in g['materials'][material].get('name','').lower():cells[:]=0
            names,room_ids=triangle_rooms(n,pr,len(ind)) if room_partition else (['shared'],np.zeros(len(ind),dtype=np.int32))
            if room_partition:
                # Anchor subdivisions to each room/material's geometry rather than
                # the world grid. A small room straddling a grid boundary stays one
                # draw; genuinely large rooms still get bounded subchunks.
                for room_id,room_name in enumerate(names):
                    if room_name=='shared':continue
                    mask=room_ids==room_id
                    if mask.any():
                        local=triangles[mask]
                        origin=local.min(axis=(0,1))
                        cells[mask]=np.floor((local.mean(1)-origin)/chunk_size).astype(np.int32)
            # Keep glazing in source order even when it spans room boundaries.
            if material>=0 and 'glass' in g['materials'][material].get('name','').lower():
                names=['shared'];room_ids[:]=0
            partition=np.column_stack((room_ids,cells)) if room_partition else cells
            unique,inverse=np.unique(partition,axis=0,return_inverse=True)
            order=np.argsort(inverse,kind='stable');offsets=np.r_[0,np.cumsum(np.bincount(inverse))]
            for c,cell in enumerate(unique):
                selected=ind[order[offsets[c]:offsets[c+1]]]
                used,reindex=np.unique(selected,return_inverse=True)
                key=(material,(primitive_number,*tuple(cell)))
                if room_partition:
                    room=names[cell[0]]
                    group_names[key]=room if room!='shared' else 'shared / '+','.join(map(str,cell[1:]))
                groups.setdefault(key,[]).append(({k:v[used] for k,v in attrs.items()},reindex.reshape(-1,3)))
    q={'asset':{'version':'2.0','generator':'Canis conservative static bake'},'scene':0,'scenes':[{'nodes':[]}],'nodes':[],'meshes':[],'materials':g.get('materials',[]),'accessors':[],'bufferViews':[]}
    # Slots may intentionally override only tint/normal maps while retaining the
    # embedded albedo. Preserve those fallback textures, rebasing external images.
    for key in ('textures','samplers','images'):
        if key in g:q[key]=copy.deepcopy(g[key])
    for img in q.get('images',[]):
        assert 'bufferView' not in img, 'Externalize embedded images before baking'
        if 'uri' in img and not img['uri'].startswith('data:'):
            img['uri']=os.path.relpath(source.parent/img['uri'],output.parent)
    binary=bytearray();bounds=[];room_groups=collections.OrderedDict()
    def add(array,typ):
        integer=typ=='SCALAR';array=np.asarray(array,dtype='<u4'if integer else'<f4');offset=len(binary);binary.extend(array.tobytes())
        q['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=array.nbytes))
        a=dict(bufferView=len(q['bufferViews'])-1,componentType=5125 if integer else 5126,count=len(array),type=typ)
        if typ=='VEC3':a.update(min=array.min(0).tolist(),max=array.max(0).tolist())
        q['accessors'].append(a);return len(q['accessors'])-1
    for (material,cell),parts in groups.items():
        # Retain uniform source attribute layouts in a batch; imported station has all four.
        keys=list(parts[0][0]);assert all(set(a)==set(keys) for a,ix in parts),'Normalize vertex layouts before baking'
        merged={k:np.concatenate([a[k] for a,ix in parts])for k in keys};indices=[];offset=0
        for attrs,ix in parts:indices.append(ix+offset);offset+=len(attrs['POSITION'])
        bounds.append([merged['POSITION'].min(0).tolist(),merged['POSITION'].max(0).tolist()])
        attributes={k:add(v,'VEC'+str(v.shape[1]))for k,v in merged.items()}
        primitive=dict(attributes=attributes,indices=add(np.concatenate(indices).reshape(-1,1),'SCALAR'))
        if material>=0:primitive['material']=material
        idx=len(q['meshes'])
        if room_partition:room_groups.setdefault(group_names[(material,cell)],[]).append(idx)
        q['meshes'].append({'primitives':[primitive]});q['nodes'].append({'name':f'Baked chunk {cell} / {material}','mesh':idx});q['scenes'][0]['nodes'].append(idx)
    output.parent.mkdir(parents=True,exist_ok=True);binpath=output.with_suffix('.bin');q['buffers']=[dict(uri=binpath.name,byteLength=len(binary))]
    text=json.dumps(q,separators=(',',':')).encode();output.write_bytes(text);binpath.write_bytes(binary)
    crc=zlib.crc32(binary,zlib.crc32(text))
    # Largest opaque faces are only occluders, never substitutes for the visual geometry.
    occluders=[t for area,t in sorted(occluders,key=lambda a:a[0],reverse=True)[:max_occluders]]
    b=np.array(bounds);hidden=[]
    for cell in itertools.product(*(range(d)for d in dimensions)):
        lo=minimum+np.array(cell)*cell_size;hi=lo+cell_size;mask=np.zeros(len(bounds),bool)
        for tri in occluders:mask |= blocked_by_triangle(lo,hi,b[:,0],b[:,1],tri)
        hidden.append(np.where(mask)[0].tolist())
    data=dict(version=1,sourceCrc32=source_crc,crc32=crc,minimum=minimum.tolist(),dimensions=list(dimensions),cellSize=cell_size,bounds=bounds,hidden=hidden)
    if room_partition:data['rooms']=[dict(name=name,primitives=ids) for name,ids in room_groups.items()]
    Path(str(output)+'.visibility').write_text(json.dumps(data,separators=(',',':')))
    report=dict(source=str(source),output=str(output),triangles=triangle_count,chunks=len(bounds),rooms=sum(not name.startswith('shared') for name in room_groups),spatialGroups=sum(name.startswith('shared') for name in room_groups),cells=len(hidden),occluders=len(occluders),hiddenPairs=sum(map(len,hidden)),seconds=round(time.monotonic()-start,2))
    print(json.dumps(report),flush=True)
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--chunk-size',type=float,default=32);p.add_argument('--cell-size',type=float,default=8)
    p.add_argument('--minimum',type=float,nargs=3,default=[160,8,96]);p.add_argument('--dimensions',type=int,nargs=3,default=[12,2,12]);p.add_argument('--occluders',type=int,default=96)
    p.add_argument('--rooms',action='store_true',help='Preserve semantic room partitions and emit hierarchical room bounds')
    a=p.parse_args();assert a.chunk_size>0 and a.cell_size>0 and all(d>0 for d in a.dimensions)
    bake(a.source,a.output,a.chunk_size,np.array(a.minimum),a.dimensions,a.cell_size,a.occluders,a.rooms)
