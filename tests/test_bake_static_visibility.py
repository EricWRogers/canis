#!/usr/bin/env python3
"""Exercise the conservative bake proof independently of the renderer."""
import importlib.util
from pathlib import Path
import numpy as np
spec=importlib.util.spec_from_file_location('baker',Path(__file__).resolve().parents[1]/'tools/bake-static-visibility.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
triangle=np.array([[-10.,-10.,-2.],[10.,-10.,-2.],[0.,10.,-2.]])
eye_min=np.array([-.1,-.1,-.1]);eye_max=-eye_min
assert b.blocked_by_triangle(eye_min,eye_max,np.array([[-.1,-.1,-4.1]]),np.array([[.1,.1,-3.9]]),triangle)[0]
assert not b.blocked_by_triangle(eye_min,eye_max,np.array([[-20.,-.1,-4.1]]),np.array([[20.,.1,-3.9]]),triangle)[0]
assert not b.blocked_by_triangle(np.array([-1.,-1.,-3.]),np.array([1.,1.,0.]),np.array([[-.1,-.1,-4.1]]),np.array([[.1,.1,-3.9]]),triangle)[0]
# Two solid door jambs leave the gap visible; neither triangle proves full occlusion.
for jamb in [np.array([[-10.,-10.,-2.],[-.3,-10.,-2.],[-.3,10.,-2.]]),np.array([[.3,-10.,-2.],[10.,-10.,-2.],[.3,10.,-2.]])]:
    assert not b.blocked_by_triangle(eye_min,eye_max,np.array([[-.1,-.1,-4.1]]),np.array([[.1,.1,-3.9]]),jamb)[0]
rng=np.random.default_rng(7104);verified=0
for _ in range(300):
    center=rng.uniform([-8,-8,-12],[8,8,-3]);extent=rng.uniform(.01,1.5,3)
    lo=center-extent;hi=center+extent
    if not b.blocked_by_triangle(eye_min,eye_max,lo[None],hi[None],triangle)[0]:continue
    eyes=rng.uniform(eye_min,eye_max,(128,3));points=rng.uniform(lo,hi,(128,3));d=points-eyes
    t=(-2-eyes[:,2])/d[:,2];hit=eyes+t[:,None]*d
    a,c,e=triangle;v0=c-a;v1=e-a;v2=hit-a
    d00=v0@v0;d01=v0@v1;d11=v1@v1;den=d00*d11-d01*d01
    u=(d11*(v2@v0)-d01*(v2@v1))/den;v=(d00*(v2@v1)-d01*(v2@v0))/den
    assert np.all((t>0)&(t<1)&(u>=0)&(v>=0)&(u+v<=1)), 'A proven-hidden box contained an unobstructed sample'
    verified+=len(t)
assert verified>1000
print(f'PASS: occluder proof, door gaps, cell boundaries and {verified} independently sampled rays')
# A tint-only material override still needs the embedded fallback texture.
# Exercise the converter across directories, including its output integrity CRC.
import json,tempfile,zlib
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory);source=root/'source';source.mkdir();output=root/'compiled/model.gltf'
    vertices=np.array([[0,0,-2],[1,0,-2],[0,1,-2]],dtype='<f4')
    indices=np.array([0,1,2],dtype='<u4');binary=vertices.tobytes()+indices.tobytes()
    (source/'mesh.bin').write_bytes(binary);(source/'albedo.png').write_bytes(b'texture-reference-fixture')
    g={'asset':{'version':'2.0'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'mesh':0}],
       'meshes':[{'primitives':[{'attributes':{'POSITION':0},'indices':1,'material':0}]}],
       'materials':[{'pbrMetallicRoughness':{'baseColorTexture':{'index':0}}}],
       'textures':[{'source':0}],'images':[{'uri':'albedo.png'}],
       'buffers':[{'uri':'mesh.bin','byteLength':len(binary)}],
       'bufferViews':[{'buffer':0,'byteLength':vertices.nbytes},{'buffer':0,'byteOffset':vertices.nbytes,'byteLength':indices.nbytes}],
       'accessors':[{'bufferView':0,'componentType':5126,'count':3,'type':'VEC3'},{'bufferView':1,'componentType':5125,'count':3,'type':'SCALAR'}]}
    path=source/'model.gltf';path.write_text(json.dumps(g))
    b.bake(path,output,32,np.array([0.,0.,0.]),[1,1,1],8,4)
    result=json.loads(output.read_text());table=json.loads(Path(str(output)+'.visibility').read_text())
    assert (output.parent/result['images'][0]['uri']).resolve()==(source/'albedo.png').resolve()
    assert result['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index']==0
    assert table['crc32']==zlib.crc32(output.with_suffix('.bin').read_bytes(),zlib.crc32(output.read_bytes()))
print('PASS: fallback albedo references and generated visibility integrity')
