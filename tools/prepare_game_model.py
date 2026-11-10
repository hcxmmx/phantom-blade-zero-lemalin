"""Build a separate deform rig. Original PMX, controls and metadata remain in imported.blend."""
import bpy,json
from pathlib import Path
from mathutils import Matrix
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(R/'work/LeMalin-material-preview.blend'),load_ui=False,use_scripts=False)
old=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
mesh=bpy.data.objects['LeMaline v1.0_mesh']
mapping={'全ての親':'root','下半身':'pelvis','上半身':'spine_01','上半身2':'spine_02','首':'neck','頭':'head'}
parents={'root':None,'pelvis':'root','spine_01':'pelvis','spine_02':'spine_01','neck':'spine_02','head':'neck'}
for jp,s in [('左','l'),('右','r')]:
 for j,e,p in [('肩','clavicle','spine_02'),('腕','upperarm','clavicle_'+s),('ひじ','lowerarm','upperarm_'+s),('手首','hand','lowerarm_'+s),('足','thigh','pelvis'),('ひざ','calf','thigh_'+s),('足首','foot','calf_'+s),('つま先','ball','foot_'+s)]:
  mapping[jp+j]=e+'_'+s;parents[e+'_'+s]=p
 for j,e,nums in [('親指','thumb','０１２'),('人指','index','１２３'),('中指','middle','１２３'),('薬指','ring','１２３'),('小指','pinky','１２３')]:
  for i,n in enumerate(nums,1):mapping[jp+j+n]=f'{e}_{i:02d}_{s}';parents[f'{e}_{i:02d}_{s}']='hand_'+s if i==1 else f'{e}_{i-1:02d}_{s}'
aliases={}
for jp,s in [('左','l'),('右','r')]:
 for j,t in [('足D','thigh'),('ひざD','calf'),('足首D','foot'),('足先EX','ball')]:aliases[jp+j]=t+'_'+s
 for j,t in [('腕捩','upperarm'),('手捩','lowerarm')]:
  for suffix in ['', '1','2','3']:aliases[jp+j+suffix]=t+'_'+s
 aliases['upperLeg_twist_'+s.upper()]='thigh_'+s
 for j in ['Ex_subKnee_'+s.upper(),'Ex_subKneeEnd_'+s.upper()]:aliases[j]='calf_'+s
 for j in ['Ex_subButtocks_'+s.upper(),'Ex_subButtocksEnd_'+s.upper()]:aliases[j]='thigh_'+s
def resolve(n):
 if n in mapping:return mapping[n]
 if n in aliases:return aliases[n]
 b=old.data.bones.get(n)
 return resolve(b.parent.name) if b and b.parent else 'root'
# Retain weighted secondary hair, ears, chest, ribbon bones and their extra ancestors.
source_bone_names=set(old.data.bones.keys())
non_bone_groups=[g.name for g in mesh.vertex_groups if g.name not in source_bone_names]
weighted={mesh.vertex_groups[g.group].name for v in mesh.data.vertices for g in v.groups if g.weight>0 and mesh.vertex_groups[g.group].name in source_bone_names}
extras=set()
for n in weighted:
 b=old.data.bones.get(n)
 while b and (b.name.startswith('Ex_') or b.name in {'胸亲','左胸上','左胸上2','左胸先','左胸下','右胸上','右胸上2','右胸先','右胸下','頭先','右手首先','左手首先'}):
  extras.add(b.name);b=b.parent
for i,n in enumerate(sorted(extras)):mapping[n]='aux_'+str(i).zfill(3)
for n in extras:parents[mapping[n]]=resolve(old.data.bones[n].parent.name) if old.data.bones[n].parent else 'head'
data=bpy.data.armatures.new('LeMalin_Deform');rig=bpy.data.objects.new('LeMalin',data);bpy.context.scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig;rig.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
for n,target in mapping.items():
 b=old.data.bones[n];e=data.edit_bones.new(target);e.head=b.head_local;e.tail=b.tail_local
 if (e.tail-e.head).length<1e-5:e.tail.z+=.01
 e.matrix=b.matrix_local;e.length=max(b.length,.01)
for n,parent in parents.items():
 if parent:data.edit_bones[n].parent=data.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')
groupmap={g.index:resolve(g.name) for g in mesh.vertex_groups if g.name in source_bone_names};weights=[]
for v in mesh.data.vertices:
 row={}
 for g in v.groups:
  if g.group not in groupmap:continue
  n=groupmap[g.group];row[n]=row.get(n,0)+g.weight
 assert sum(row.values())>0,('No deform weight',v.index)
 weights.append(row)
mesh.vertex_groups.clear()
groups={n:mesh.vertex_groups.new(name=n) for n in set(n for row in weights for n in row)}
for i,row in enumerate(weights):
 total=sum(row.values())
 for n,w in row.items():
  if w>0:groups[n].add([i],w/total,'REPLACE')
mesh.parent=None;mesh.matrix_world=Matrix.Identity(4)
mesh.modifiers.clear();mod=mesh.modifiers.new('LeMalin skin','ARMATURE');mod.object=rig
keep={mesh,rig}
for o in list(bpy.data.objects):
 if o not in keep:bpy.data.objects.remove(o,do_unlink=True)
floor=min(v.co.z for v in mesh.data.vertices)
for o in [mesh,rig]:o.matrix_world=Matrix.Translation((0,0,-100*floor))@Matrix.Scale(100,4)
bpy.ops.object.select_all(action='DESELECT')
for o in [mesh,rig]:o.select_set(True)
bpy.context.view_layer.objects.active=rig;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
rig.show_in_front=True;rig.data.display_type='STICK'
(R/'output/reports/bone-mapping.json').write_text(json.dumps({'mapping':mapping,'weight_aliases':aliases,'parents':parents,'weighted_original_groups':sorted(weighted),'excluded_non_bone_groups':non_bone_groups,'deform_bones':len(data.bones),'secondary_bones':len(extras),'floor_lift_cm':-100*floor,'note':'Only actual source armature bones contribute skin weights. MMD edge/order metadata excluded. Secondary bones retained with static parent following; dynamics not converted.'},ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(R/'work/LeMalin-game-cm.blend'))
bpy.ops.export_scene.fbx(filepath=str(R/'work/LeMalin-game-cm.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',path_mode='STRIP',use_mesh_modifiers=False)
print('LEMALIN_GAME_MODEL_READY',flush=True)
