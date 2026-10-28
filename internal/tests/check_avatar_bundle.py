"""Static asset validation; cannot establish Android GPU/animation behavior."""
from pathlib import Path
import argparse,json
import numpy as np
import UnityPy
p=argparse.ArgumentParser();p.add_argument('bundle',type=Path);p.add_argument('--report',type=Path);a=p.parse_args()
e=UnityPy.load(str(a.bundle));objs={o.path_id:o for o in e.objects};trees={i:o.read_typetree() for i,o in objs.items()};sf=next(iter(objs.values())).assets_file
assert not sf.externals and not sf.script_types and not sf.ref_types and all(t.class_id!=114 for t in sf.types) and int(sf.target_platform)==13 # Android
trans={i:t for i,t in trees.items() if objs[i].type.name=='Transform'}
def qm(q):
 x,y,z,w=[q[k] for k in 'xyzw'];return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
world={}
def wm(i):
 if i not in world:
  t=trans[i];m=np.eye(4);m[:3,:3]=qm(t['m_LocalRotation'])@np.diag([t['m_LocalScale'][k] for k in 'xyz']);m[:3,3]=[t['m_LocalPosition'][k] for k in 'xyz'];parent=t['m_Father']['m_PathID'];world[i]=(wm(parent) if parent else np.eye(4))@m
 return world[i]
errors=[];count=0
for i,o in objs.items():
 if o.type.name!='SkinnedMeshRenderer':continue
 r=trees[i];mesh=trees[r['m_Mesh']['m_PathID']];v=mesh['m_VertexData'];data=bytes(v['m_DataSize']);n=v['m_VertexCount'];assert len(data)==n*64
 pos=np.ndarray((n,3),dtype='<f4',buffer=data,offset=0,strides=(64,4));weights=np.ndarray((n,4),dtype='<f4',buffer=data,offset=32,strides=(64,4));joints=np.ndarray((n,4),dtype='<u4',buffer=data,offset=48,strides=(64,4))
 assert np.isfinite(pos).all() and np.isfinite(weights).all() and np.max(np.abs(weights.sum(axis=1)-1))<1e-5
 assert len(r['m_Bones'])==len(mesh['m_BindPose'])==len(mesh['m_BonesAABB']) and joints.max()<len(r['m_Bones'])
 matrices=[]
 for ref,b in zip(r['m_Bones'],mesh['m_BindPose']):matrices.append(wm(ref['m_PathID'])@np.array([[b[f'e{x}{y}'] for y in range(4)] for x in range(4)]))
 matrices=np.stack(matrices);vertices=np.column_stack([pos,np.ones(n)]);skinned=np.zeros((n,4))
 for channel in range(4):skinned+=np.einsum('nij,nj->ni',matrices[joints[:,channel]],vertices)*weights[:,channel,None]
 go=r['m_GameObject']['m_PathID'];tr=next(k for k,t in trans.items() if t['m_GameObject']['m_PathID']==go);expected=(wm(tr)@vertices.T).T
 error=float(np.max(np.abs(skinned-expected)));assert error<2e-4,(i,error);errors.append(error);count+=n
 indices=np.frombuffer(bytes(mesh['m_IndexBuffer']),dtype='<u2' if mesh['m_IndexFormat']==0 else '<u4');assert indices.max()<n and len(indices)%3==0
 assert mesh['m_SubMeshes'][0]['indexCount']==len(indices) and r['m_RenderingLayerMask']==1
 assert r['m_Enabled'] and not r['m_CastShadows'] and not r['m_UpdateWhenOffscreen']
for i,o in objs.items():
 if o.type.name=='Shader':
  t=trees[i];assert t['m_ParsedForm']['m_Name'].startswith('CFESP/Avatar')
  for sub in t['m_ParsedForm']['m_SubShaders']:
   for p in sub['m_Passes']:assert p['m_State']['zTest']['val']==4
 if o.type.name=='Texture2D':
  t=trees[i];assert t['m_TextureFormat']==47 and t['m_Width']<=1024 and t['m_Height']<=1024 and not t['m_IsReadable']
  im=o.read().image;assert im.size==(t['m_Width'],t['m_Height'])
assert count==json.loads(a.bundle.with_suffix('.json').read_text())['vertices']
container=next(trees[i] for i,o in objs.items() if o.type.name=='AssetBundle');prefab=trees[container['m_Container'][0][1]['asset']['m_PathID']];assert not prefab['m_IsActive']
root=prefab['m_Component'][0]['component']['m_PathID'];meta=json.loads(a.bundle.with_suffix('.json').read_text())
for b in meta['bindings']:
 indexed=root
 for index in b.get('steps',[]):indexed=trans[indexed]['m_Children'][index]['m_PathID']
 current=root
 for name in b['path'].split('/'):
  candidates=[v['m_PathID'] for v in trans[current]['m_Children'] if trees[trans[v['m_PathID']]['m_GameObject']['m_PathID']]['m_Name']==name]
  assert len(candidates)==1,b
  current=candidates[0]
 if 'steps' in b:assert current==indexed
report={'android_platform':int(sf.target_platform),'external_dependencies':0,'vertices_checked':count,'max_rest_skin_error':max(errors),'renderer_count':len(errors),'texture_format':'ETC2_RGBA8','textures_decode':True,'max_texture_size':1024,'depth_test':'LEqual','humanoid_paths_verified':len(meta['bindings']),'device_tested':False}
print(json.dumps(report,indent=2))
if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
