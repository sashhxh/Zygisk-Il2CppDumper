#!/usr/bin/env python3
"""Convert the provided VRM to a script-free Android prefab with bounded textures.
Uses serialized type layouts and the existing compiled URP Unlit program from
OXIDE 1.16.11978, without copying game geometry, textures or script components.
"""
from pathlib import Path
import argparse,copy,hashlib,io,json,re,struct,zlib
import numpy as np
from PIL import Image
import UnityPy
from UnityPy.export.Texture2DConverter import image_to_texture2d
from UnityPy.enums import TextureFormat
ap=argparse.ArgumentParser();ap.add_argument('vrm',type=Path);ap.add_argument('character_bundle',type=Path);ap.add_argument('shader_bundle',type=Path);ap.add_argument('dump',type=Path);ap.add_argument('output',type=Path);ap.add_argument('--bindings',type=Path,required=True);a=ap.parse_args()
data=a.vrm.read_bytes();assert data[:4]==b'glTF';n=struct.unpack_from('<I',data,12)[0];g=json.loads(data[20:20+n]);binary=data[28+n:]
def accessor(i):
 v=g['accessors'][i];b=g['bufferViews'][v['bufferView']];dt={5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[v['componentType']];cnt={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[v['type']];size=np.dtype(dt).itemsize
 out=np.ndarray((v['count'],cnt),dtype=dt,buffer=binary,offset=b.get('byteOffset',0)+v.get('byteOffset',0),strides=(b.get('byteStride',size*cnt),size)).copy()
 if v.get('normalized'):out=out.astype(np.float32)/np.iinfo(np.dtype(dt)).max
 return out
F=np.diag([1.,1.,-1.,1.])
def quat_matrix(q):
 x,y,z,w=q;return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
def matrix_quat(m):
 # Stable orthogonal-matrix conversion, including rotations near 180 degrees.
 K=np.array([[m[0,0]-m[1,1]-m[2,2],m[1,0]+m[0,1],m[2,0]+m[0,2],m[2,1]-m[1,2]],[m[1,0]+m[0,1],m[1,1]-m[0,0]-m[2,2],m[2,1]+m[1,2],m[0,2]-m[2,0]],[m[2,0]+m[0,2],m[2,1]+m[1,2],m[2,2]-m[0,0]-m[1,1],m[1,0]-m[0,1]],[m[2,1]-m[1,2],m[0,2]-m[2,0],m[1,0]-m[0,1],m.trace()]])/3
 _,v=np.linalg.eigh(K);q=v[:,-1];return q if q[3]>=0 else -q
parents={c:i for i,node in enumerate(g['nodes']) for c in node.get('children',[])}
def node_matrix(i):
 d=g['nodes'][i]
 if 'matrix' in d:return F@np.array(d['matrix']).reshape(4,4,order='F')@F
 m=np.eye(4);m[:3,:3]=quat_matrix(d.get('rotation',[0,0,0,1]))@np.diag(d.get('scale',[1,1,1]));m[:3,3]=d.get('translation',[0,0,0]);return F@m@F
world={}
def wm(i):
 if i not in world:world[i]=(wm(parents[i]) if i in parents else np.eye(4))@node_matrix(i)
 return world[i]
def node_path(i):return (node_path(parents[i])+'/' if i in parents else '')+('n%03d'%i)
source=UnityPy.load(str(a.character_bundle));bundle=next(iter(source.files.values()));templates={}
for o in source.objects:templates.setdefault(o.type.name,o)
sf=templates['GameObject'].assets_file;original_objects=dict(sf.objects)
# Capture the selected game's complete local rest rig before replacing objects.
transforms={p:o.read_typetree() for p,o in original_objects.items() if o.type.name=='Transform'}
game_names={p:o.read_typetree()['m_Name'] for p,o in original_objects.items() if o.type.name=='GameObject'}
info=original_objects[6676440564908021410].read_typetree();animation=original_objects[info['characterAnimation']['m_PathID']].read_typetree();anim=original_objects[animation['animator']['m_PathID']].read_typetree();avatar=original_objects[anim['m_Avatar']['m_PathID']].read_typetree();root_tr=next(p for p,t in transforms.items() if t['m_GameObject']==info['m_GameObject'])
game_human={e['m_HumanName']:e['m_BoneName'] for e in avatar['m_HumanDescription']['m_Human']}
def game_path(i):
 out=[]
 for _ in range(64):
  if i==root_tr:return '/'.join(reversed(out))
  t=transforms.get(i)
  if not t:return None
  out.append(game_names[t['m_GameObject']['m_PathID']]);i=t['m_Father']['m_PathID']
 return None
game_bones={game_names[t['m_GameObject']['m_PathID']]:p for p,t in transforms.items() if game_path(p) is not None};game_world={root_tr:np.eye(4)}
def game_wm(p):
 if p not in game_world:
  t=transforms[p];q=[t['m_LocalRotation'][k] for k in 'xyzw'];m=np.eye(4);m[:3,:3]=quat_matrix(q);m[:3,3]=[t['m_LocalPosition'][k] for k in 'xyz'];game_world[p]=game_wm(t['m_Father']['m_PathID'])@m
 return game_world[p]
dump=a.dump.read_text();human_enum=dump[dump.index('public enum HumanBodyBones'):];human_enum=human_enum[:human_enum.index('\n}')];human_ids={k:int(v) for k,v in re.findall(r'HumanBodyBones (\w+) = (\d+)',human_enum)}
bindings=[]
for bone in g['extensions']['VRM']['humanoid']['humanBones']:
 name=bone['bone'][0].upper()+bone['bone'][1:]
 if name not in game_human:continue
 node=bone['node'];gr=game_wm(game_bones[game_human[name]])[:3,:3];sr=wm(node)[:3,:3];sr=sr/np.linalg.norm(sr,axis=0);correction=matrix_quat(gr.T@sr)
 assert np.max(np.abs(gr@quat_matrix(correction)-sr))<1e-4
 bindings.append({'human':human_ids[name],'path':node_path(node),'correction':correction.tolist(),'name':name})
bindings.sort(key=lambda b:(b['path'].count('/'),b['human']))
assert {0,10,17,18}.issubset({b['human'] for b in bindings})
objects={};next_id=1000
P=lambda i:{'m_FileID':0,'m_PathID':int(i)}
def add(kind,tree=None,path_id=None):
 global next_id
 template=templates[kind];o=copy.copy(template)
 if path_id is None:path_id=next_id;next_id+=1
 assert path_id not in objects
 o.path_id=path_id;o.assets_file=sf
 if tree is None:tree=copy.deepcopy(template.read_typetree())
 o.save_typetree(tree);objects[o.path_id]=o;return o

def vec(v):return dict(zip('xyz',map(float,v)))
def aabb(vertices):
 mi=vertices.min(axis=0);ma=vertices.max(axis=0);return {'m_Center':vec((mi+ma)/2),'m_Extent':vec(np.maximum((ma-mi)/2,.001))}
# Two compiled render-state variants: opaque depth-writing and alpha blended.
se=UnityPy.load(str(a.shader_bundle));st=next(o for o in se.objects if o.type.name=='Shader' and o.read_typetree()['m_ParsedForm']['m_Name']=='Universal Render Pipeline/Unlit');sf.types.append(copy.copy(st.serialized_type));templates['Shader']=copy.copy(st);templates['Shader'].type_id=len(sf.types)-1;templates['Shader'].serialized_type=sf.types[-1]
shaders=[]
for transparent in [False,True]:
 t=st.read_typetree();name='CFESP/AvatarTransparent' if transparent else 'CFESP/AvatarOpaque';t['m_Name']=t['m_ParsedForm']['m_Name']=name;t['m_ParsedForm']['m_CustomEditorName']='';t['m_ParsedForm']['m_CustomEditorForRenderPipelines']=[];t['m_Dependencies']=[];t['m_NonModifiableTextures']=[];t['m_ParsedForm']['m_Dependencies']=[]
 digest=hashlib.sha256(name.encode()).digest();t['m_AssetGUID']={f'data[{i}]':int.from_bytes(digest[i*4:i*4+4],'little') for i in range(4)}
 for sub in t['m_ParsedForm']['m_SubShaders']:
  sub['m_Tags']['tags']=[(k,('Transparent' if transparent else 'Opaque') if k=='RenderType' else v) for k,v in sub['m_Tags']['tags']]
  for p in sub['m_Passes']:
   state=p['m_State'];c=lambda v:{'val':float(v),'name':'<noninit>'}
   state['zTest']=c(4);state['zWrite']=c(0 if transparent else 1);state['culling']=c(0)
   state['rtBlend0'].update(srcBlend=c(5 if transparent else 1),destBlend=c(10 if transparent else 0),srcBlendAlpha=c(1),destBlendAlpha=c(10 if transparent else 0))
 shaders.append(add('Shader',t))
# Decode and compress once, on the build host. ETC2 RGBA is core in GLES 3.
texture_objects={};tex_report=[]
for i in sorted({m['pbrMetallicRoughness']['baseColorTexture']['index'] for m in g['materials']}):
 imdef=g['images'][g['textures'][i]['source']];v=g['bufferViews'][imdef['bufferView']];im=Image.open(io.BytesIO(binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']])).convert('RGBA');im.thumbnail((1024,1024),Image.Resampling.LANCZOS)
 width,height=im.size;levels=[]
 while True:
  encoded,fmt=image_to_texture2d(im,TextureFormat.ETC2_RGBA8,platform=sf.target_platform);levels.append(encoded)
  if im.width==1 and im.height==1:break
  im=im.resize((max(1,im.width//2),max(1,im.height//2)),Image.Resampling.LANCZOS)
 t=templates['Texture2D'].read_typetree();payload=b''.join(levels);t.update(m_Name='CFESP_Avatar_Texture_%d'%i,m_Width=width,m_Height=height,m_CompleteImageSize=len(payload),m_TextureFormat=int(fmt),m_MipCount=len(levels),m_IsReadable=False,m_StreamingMipmaps=False,m_IsPreProcessed=False,m_PlatformBlob=[],m_ColorSpace=1,m_StreamData={'offset':0,'size':0,'path':''});t['image data']=payload;t['m_TextureSettings']['m_Aniso']=1;texture_objects[i]=add('Texture2D',t);tex_report.append({'index':i,'width':width,'height':height,'gpu_bytes':len(payload)})
materials=[]
for i,m in enumerate(g['materials']):
 transparent=m.get('alphaMode')=='BLEND';pbr=m['pbrMetallicRoughness'];texture=texture_objects[pbr['baseColorTexture']['index']]
 t=templates['Material'].read_typetree();t.update(m_Name='CFESP_Avatar_Material_%d'%i,m_Shader=P(shaders[transparent].path_id),m_ValidKeywords=['_SURFACE_TYPE_TRANSPARENT'] if transparent else [],m_InvalidKeywords=[],m_DoubleSidedGI=True,m_CustomRenderQueue=(3000 if transparent else 2000),stringTagMap=[('RenderType','Transparent' if transparent else 'Opaque')],disabledShaderPasses=['MOTIONVECTORS'],m_EnableInstancingVariants=False,m_BuildTextureStacks=[])
 t['m_SavedProperties']={'m_TexEnvs':[(key,{'m_Texture':P(texture.path_id),'m_Scale':{'x':1.,'y':1.},'m_Offset':{'x':0.,'y':0.}}) for key in ['_BaseMap','_MainTex']],'m_Ints':[],'m_Floats':[('_Surface',float(transparent)),('_ZWrite',float(not transparent)),('_Cull',0.),('_AlphaClip',0.),('_Cutoff',.5),('_SrcBlend',5. if transparent else 1.),('_DstBlend',10. if transparent else 0.)],'m_Colors':[(key,dict(zip('rgba',pbr.get('baseColorFactor',[1,1,1,1])))) for key in ['_BaseColor','_Color']]}
 materials.append(add('Material',t))
# Create a script-free prefab hierarchy. Numbered names give fixed ASCII paths.
root_go=add('GameObject');root_tf=add('Transform');node_go=[add('GameObject') for _ in g['nodes']];node_tf=[add('Transform') for _ in g['nodes']];components={i:[P(node_tf[i].path_id)] for i in range(len(node_go))};extra_children={i:[] for i in range(len(node_go))}
render_count=vertex_count=tri_count=0
for ni,node in enumerate(g['nodes']):
 if 'mesh' not in node:continue
 skin=g['skins'][node['skin']];ibm=accessor(skin['inverseBindMatrices']);bindpose=[]
 for v in ibm:
  m=F@v.reshape((4,4),order='F')@F;bindpose.append({f'e{r}{c}':float(m[r,c]) for r in range(4) for c in range(4)})
 for pi,primitive in enumerate(g['meshes'][node['mesh']]['primitives']):
  attr=primitive['attributes'];pos=accessor(attr['POSITION']).astype('<f4');pos[:,2]*=-1;norm=accessor(attr['NORMAL']).astype('<f4');norm[:,2]*=-1;uv=accessor(attr['TEXCOORD_0']).astype('<f4');uv[:,1]=1-uv[:,1]
  weights=accessor(attr['WEIGHTS_0']).astype('<f4');weights/=np.maximum(weights.sum(axis=1,keepdims=True),1e-8);joints=accessor(attr['JOINTS_0']).astype('<u4');assert joints.max()<len(skin['joints']);indices=accessor(primitive['indices']).reshape(-1,3).astype('<u4');indices[:,[1,2]]=indices[:,[2,1]]
  # Shared glTF accessors include vertices belonging to other material slots.
  # Drop unused vertices from each draw instead of skinning them repeatedly.
  used,remap=np.unique(indices.reshape(-1),return_inverse=True)
  pos=pos[used];norm=norm[used];uv=uv[used];weights=weights[used];joints=joints[used]
  index_format=0 if len(used)<=65535 else 1
  indices=remap.reshape(-1,3).astype('<u2' if index_format==0 else '<u4')
  inter=np.empty(len(pos),dtype=[('pos','<f4',3),('normal','<f4',3),('uv','<f4',2),('weights','<f4',4),('joints','<u4',4)]);inter['pos']=pos;inter['normal']=norm;inter['uv']=uv;inter['weights']=weights;inter['joints']=joints
  assert inter.dtype.itemsize==64 and np.isfinite(pos).all() and np.isfinite(weights).all()
  t=templates['Mesh'].read_typetree();bounds=aabb(pos);sub=copy.deepcopy(t['m_SubMeshes'][0]);sub.update(firstByte=0,indexCount=indices.size,topology=0,baseVertex=0,firstVertex=0,vertexCount=len(pos),localAABB=bounds)
  t.update(m_Name=f'CFESP_Avatar_Mesh_{ni}_{pi}',m_SubMeshes=[sub],m_Shapes={'vertices':[],'shapes':[],'channels':[],'fullWeights':[]},m_BindPose=bindpose,m_BoneNameHashes=[zlib.crc32(('n%03d'%j).encode()) for j in skin['joints']],m_RootBoneNameHash=0,m_BonesAABB=[],m_VariableBoneCountWeights={'m_Data':[]},m_MeshCompression=0,m_IsReadable=False,m_KeepVertices=False,m_KeepIndices=False,m_IndexFormat=index_format,m_IndexBuffer=indices.tobytes(),m_LocalAABB=bounds,m_BakedConvexCollisionMesh=[],m_BakedTriangleCollisionMesh=[],m_StreamData={'offset':0,'size':0,'path':''})
  channels=[{'stream':0,'offset':0,'format':0,'dimension':0} for _ in range(14)]
  for ci,off,fmt,dim in [(0,0,0,3),(1,12,0,3),(4,24,0,2),(12,32,0,4),(13,48,10,4)]:channels[ci]={'stream':0,'offset':off,'format':fmt,'dimension':dim}
  t['m_VertexData']={'m_VertexCount':len(pos),'m_Channels':channels,'m_DataSize':inter.tobytes()};t['m_MeshLodInfo']['m_SubMeshes']=[{'m_Levels':[{'m_IndexStart':0,'m_IndexCount':indices.size}]}]
  vertices4=np.column_stack([pos,np.ones(len(pos))]);bone_bounds=[]
  for bi,bp in enumerate(bindpose):
   mask=np.any((joints==bi)&(weights>0),axis=1)
   if np.any(mask):
    matrix=np.array([[bp[f'e{r}{c}'] for c in range(4)] for r in range(4)]);local=(matrix@vertices4[mask].T).T[:,:3];mn=local.min(axis=0);mx=local.max(axis=0)
   else:mn=mx=np.zeros(3)
   bone_bounds.append({'m_Min':vec(mn),'m_Max':vec(mx)})
  t['m_BonesAABB']=bone_bounds
  mesh=add('Mesh',t);go=add('GameObject');tf=add('Transform');rr=add('SkinnedMeshRenderer');extra_children[ni].append(P(tf.path_id))
  go.save_typetree({'m_Component':[{'component':P(tf.path_id)},{'component':P(rr.path_id)}],'m_Layer':0,'m_Name':f'mesh_{ni}_{pi}','m_Tag':0,'m_IsActive':True})
  tf.save_typetree({'m_GameObject':P(go.path_id),'m_LocalRotation':dict(zip('xyzw',[0.,0.,0.,1.])),'m_LocalPosition':vec([0,0,0]),'m_LocalScale':vec([1,1,1]),'m_Children':[],'m_Father':P(node_tf[ni].path_id)})
  rt=templates['SkinnedMeshRenderer'].read_typetree();rt.update(m_GameObject=P(go.path_id),m_Enabled=True,m_CastShadows=0,m_ReceiveShadows=0,m_DynamicOccludee=0,m_LightProbeUsage=0,m_ReflectionProbeUsage=0,m_RenderingLayerMask=1,m_Materials=[P(materials[primitive['material']].path_id)],m_StaticBatchRoot=P(0),m_ProbeAnchor=P(0),m_LightProbeVolumeOverride=P(0),m_SortingOrder=0,m_Quality=4,m_UpdateWhenOffscreen=False,m_SkinnedMotionVectors=False,m_Mesh=P(mesh.path_id),m_Bones=[P(node_tf[i].path_id) for i in skin['joints']],m_BlendShapeWeights=[],m_RootBone=P(node_tf[skin.get('skeleton',skin['joints'][0])].path_id),m_AABB={'m_Center':vec([0,.9,0]),'m_Extent':vec([2,2,2])},m_DirtyAABB=False);rr.save_typetree(rt)
  render_count+=1;vertex_count+=len(pos);tri_count+=len(indices)
for i,node in enumerate(g['nodes']):
 local=node_matrix(i);scale=np.linalg.norm(local[:3,:3],axis=0);q=matrix_quat(local[:3,:3]/scale)
 node_go[i].save_typetree({'m_Component':[{'component':v} for v in components[i]],'m_Layer':0,'m_Name':'n%03d'%i,'m_Tag':0,'m_IsActive':True})
 node_tf[i].save_typetree({'m_GameObject':P(node_go[i].path_id),'m_LocalRotation':dict(zip('xyzw',map(float,q))),'m_LocalPosition':vec(local[:3,3]),'m_LocalScale':vec(scale),'m_Children':[P(node_tf[c].path_id) for c in node.get('children',[])]+extra_children[i],'m_Father':P(node_tf[parents[i]].path_id if i in parents else root_tf.path_id)})
root_go.save_typetree({'m_Component':[{'component':P(root_tf.path_id)}],'m_Layer':0,'m_Name':'CFESP_CustomAvatar','m_Tag':0,'m_IsActive':False});root_tf.save_typetree({'m_GameObject':P(root_go.path_id),'m_LocalRotation':dict(zip('xyzw',[0.,0.,0.,1.])),'m_LocalPosition':vec([0,0,0]),'m_LocalScale':vec([1,1,1]),'m_Children':[P(node_tf[i].path_id) for i in range(len(node_tf)) if i not in parents],'m_Father':P(0)})
# Unity's bundle loader probes local IDs 1 and 2 for the AssetBundle header
# before it can use m_Container. An arbitrary ID round-trips in UnityPy but
# makes the player create an empty header (runtimeCompatibility=0), which
# produces the misleading "newer version of the Unity runtime" error.
container=add('AssetBundle',path_id=1);bt=templates['AssetBundle'].read_typetree();preload=[P(i) for i in objects if i!=container.path_id];asset={'preloadIndex':0,'preloadSize':len(preload),'asset':P(root_go.path_id)};bt.update(m_Name='cfesp_custom_avatar_r19',m_AssetBundleName='cfesp_custom_avatar_r19',m_PreloadTable=preload,m_Container=[('cfesp/avatar',asset)],m_MainAsset=asset,m_Dependencies=[],m_IsStreamedSceneAssetBundle=False);container.save_typetree(bt)
# Keep only native asset types. Old MonoBehaviour type/script lookup entries
# would still reference the source bundle's external MonoScripts.
used_types=[];type_indices={}
for obj in objects.values():
 old_type=obj.type_id
 if old_type not in type_indices:type_indices[old_type]=len(used_types);used_types.append(obj.serialized_type)
 obj.type_id=type_indices[old_type]
sf.types=used_types;sf.script_types=[];sf.ref_types=[]
sf.objects=objects;sf.externals=[];sf.name='CAB-'+hashlib.sha256(data+b'cfesp-avatar-r19').hexdigest()[:32];bundle.files={sf.name:sf};result=bundle.save(packer='lz4');a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(result)
a.output.with_name('avatar_bundle_info.h').write_text('#pragma once\n#define CFESP_AVATAR_BUNDLE_SIZE %d\n#define CFESP_AVATAR_BUNDLE_SHA256 "%s"\n'%(len(result),hashlib.sha256(result).hexdigest()))
# Verify every object reference resolves inside this bundle, not to game files.
check=UnityPy.load(result);ids={o.path_id for o in check.objects}
header=next(o for o in check.objects if o.path_id==1)
assert header.type.name=='AssetBundle' and header.read_typetree()['m_RuntimeCompatibility']==1
def pointers(t):
 if isinstance(t,dict):
  if 'm_FileID' in t and 'm_PathID' in t:assert t['m_FileID']==0 and (not t['m_PathID'] or t['m_PathID'] in ids),t
  for v in t.values():pointers(v)
 elif isinstance(t,(list,tuple)):
  for v in t:pointers(v)
for o in check.objects:pointers(o.read_typetree())
assert all(o.type.name not in ('MonoBehaviour','Animator','Rigidbody','Collider') for o in check.objects)
lines=['#pragma once','// Generated from the user-provided VRM and the selected game rest rig.','namespace mango_avatar {','struct Binding {int human;const char16_t* path;float correction[4];unsigned char depth;unsigned char steps[32];};','inline constexpr Binding bindings[]={']
parsed={o.path_id:o for o in check.objects}
def steps(path):
 current=root_tf.path_id;result=[]
 for part in path.split('/'):
  children=parsed[current].read_typetree()['m_Children']
  matches=[(i,c['m_PathID']) for i,c in enumerate(children) if parsed[parsed[c['m_PathID']].read_typetree()['m_GameObject']['m_PathID']].read_typetree()['m_Name']==part]
  assert len(matches)==1,path
  index,current=matches[0];assert index<256;result.append(index)
 assert len(result)<=32
 return result
for b in bindings:
 step=steps(b['path']);b['steps']=step
 lines.append('    {%d,u"%s",{%s},%d,{%s}}, // %s'%(b['human'],b['path'],','.join(f'{v:.9e}f' for v in b['correction']),len(step),','.join(map(str,step)),b['name']))
lines+=['};','struct MeshBinding {unsigned char depth;unsigned char steps[32];};','inline constexpr MeshBinding mesh_bindings[]={']
for i,obj in parsed.items():
 if obj.type.name!='SkinnedMeshRenderer':continue
 go=obj.read_typetree()['m_GameObject']['m_PathID'];tr=next(p for p,o in parsed.items() if o.type.name=='Transform' and o.read_typetree()['m_GameObject']['m_PathID']==go)
 path=[]
 while tr!=root_tf.path_id:
  d=parsed[tr].read_typetree();path.append(parsed[d['m_GameObject']['m_PathID']].read_typetree()['m_Name']);tr=d['m_Father']['m_PathID']
 step=steps('/'.join(reversed(path)));lines.append('    {%d,{%s}},'%(len(step),','.join(map(str,step))))
lines+=['};','}'];a.bindings.write_text('\n'.join(lines)+'\n')
report={'source_sha256':hashlib.sha256(data).hexdigest(),'bundle_sha256':hashlib.sha256(result).hexdigest(),'bundle_bytes':len(result),'assetbundle_header_path_id':header.path_id,'runtime_compatibility':1,'humanoid_bindings':len(bindings),'renderer_count':render_count,'vertices':vertex_count,'triangles':tri_count,'texture_gpu_bytes':sum(t['gpu_bytes'] for t in tex_report),'textures':tex_report,'objects':len(ids),'contains_game_scripts_or_geometry':False,'external_dependencies':0,'bindings':bindings}
a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k not in ['bindings','textures']},indent=2))
