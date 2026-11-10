"""Diagnostic copies only: original weight verification, chain colouring and isolated bone probes."""
import bpy,json,math,hashlib,numpy as np
from pathlib import Path
from collections import Counter
from mathutils import Vector,Matrix
R=Path(__file__).resolve().parents[1];O=R/'output/hair-investigation';O.mkdir(exist_ok=True)
game=R/'work/LeMalin-game-cm.blend';before=hashlib.sha256(game.read_bytes()).hexdigest()
audit=json.loads((O/'pmx-hair-audit.json').read_text(encoding='utf-8'));mapping=json.loads((R/'output/reports/bone-mapping.json').read_text(encoding='utf-8'));inverse={v:k for k,v in mapping['mapping'].items()}
# Read the PMX through the independent parser without loading/editing the source file in Blender.
import importlib.util
parser=Path.home()/'AppData/Roaming/Blender Foundation/Blender/5.2/extensions/blender_org/mmd_tools/core/pmx/__init__.py'
spec=importlib.util.spec_from_file_location('pmx_hair_visual',parser);pmx=importlib.util.module_from_spec(spec);spec.loader.exec_module(pmx);raw=pmx.load(str(R/'source/LeMaline v1.0.pmx'))
bpy.ops.wm.open_mainfile(filepath=str(R/'output/motion/LeMalin-idle-walk-attack.blend'),load_ui=False,use_scripts=False)
rig=bpy.data.objects['LeMalin'];mesh=bpy.data.objects['LeMaline v1.0_mesh'];rig.animation_data_clear();mesh.data.shape_keys.animation_data_clear()
for k in mesh.data.shape_keys.key_blocks:
 if k.name!='Basis':k.value=0
for b in rig.pose.bones:b.matrix_basis=Matrix.Identity(4)
bpy.context.view_layer.update()
material_ids={i for i,m in enumerate(mesh.data.materials) if m.name.removeprefix('Preview_') in {'Hair','HairFront'}}
hair={v for p in mesh.data.polygons if p.material_index in material_ids for v in p.vertices}
max_error=0.;fallback=set();raw_groups={}
for i in hair:
 rw=raw.vertices[i].weight;values=[1.] if rw.type==0 else [rw.weights[0],1-rw.weights[0]] if rw.type==1 else rw.weights
 expected={}
 for bone,w in zip(rw.bones,values):
  if bone<0 or w<=0:continue
  name=raw.bones[bone].name;n=mapping['mapping'].get(name)
  if not n:fallback.add(name);continue
  expected[n]=expected.get(n,0)+w
 total=sum(expected.values());expected={n:w/total for n,w in expected.items()}
 actual={mesh.vertex_groups[g.group].name:g.weight for g in mesh.data.vertices[i].groups if g.weight>0}
 max_error=max(max_error,max(abs(actual.get(n,0)-expected.get(n,0)) for n in set(actual)|set(expected)))
 raw_groups[i]=[(inverse.get(n,n),w) for n,w in actual.items()]
assert not fallback,fallback;assert max_error<1e-6,max_error
report={'hair_vertices':len(hair),'max_weight_error':max_error,'unmapped_source_bones':list(fallback),'source_weights_preserved':True,'bone_probes':[],'production_blend_sha256':before}
def get_coords():
 bpy.context.view_layer.update();ev=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());return np.array([tuple(v.co) for v in ev.data.vertices])
rest=get_coords();bands=[(45,70),(70,90),(90,110),(110,125),(125,145)]
for name in ['Ex_BackHairA01_L','Ex_BackHairA03_L','Ex_BackHairA05_L']:
 n=mapping['mapping'][name];pb=rig.pose.bones[n];pb.rotation_mode='XYZ';pb.rotation_euler.x=math.radians(20);posed=get_coords();distance=np.linalg.norm(posed-rest,axis=1)
 row={'bone':name,'game_bone':n,'test_rotation_degrees':20,'bands':[]}
 for lo,hi in bands:
  ids=[i for i in hair if rest[i][0]>0 and lo<=rest[i][2]<hi];ds=distance[ids]
  row['bands'].append({'height_cm':[lo,hi],'vertices':len(ids),'moved_over_1mm':int((ds>.1).sum()),'mean_cm':float(ds.mean()) if len(ds) else 0,'max_cm':float(ds.max()) if len(ds) else 0})
 report['bone_probes'].append(row);pb.matrix_basis=Matrix.Identity(4)
bpy.context.view_layer.update()
# Vertex colours identify A00..AEnd directly on visible hair and the attached ribbons.
palette=[(.8,.05,.05,1),(1,.32,.02,1),(.9,.8,.03,1),(.06,.65,.12,1),(.02,.65,.8,1),(.04,.15,.9,1),(.5,.05,.9,1),(.9,.08,.65,1)]
layer=mesh.data.color_attributes.new(name='HairBoneAudit',type='FLOAT_COLOR',domain='POINT')
for v in mesh.data.vertices:
 row=[(inverse.get(mesh.vertex_groups[g.group].name,''),g.weight) for g in v.groups if g.weight>0]
 a=[(n,w) for n,w in row if n.startswith('Ex_BackHairA')]
 if a:
  n=max(a,key=lambda t:t[1])[0];segment=7 if 'End' in n else int(n[len('Ex_BackHairA'):len('Ex_BackHairA')+2]);colour=palette[segment]
 else:colour=(.12,.13,.16,1)
 layer.data[v.index].color=colour
for i,s in enumerate(mesh.material_slots):
 old=s.material;mat=bpy.data.materials.new('AUDIT_'+old.name);mat.use_nodes=True;p=mat.node_tree.nodes.get('Principled BSDF');p.inputs['Roughness'].default_value=.85
 c=mat.node_tree.nodes.new('ShaderNodeVertexColor');c.layer_name=layer.name;mat.node_tree.links.new(c.outputs['Color'],p.inputs['Base Color'])
 # Keep alpha cutouts; diagnostic colours do not change source geometry or source materials.
 image=next((n.image for n in old.node_tree.nodes if n.type=='TEX_IMAGE' and n.image),None)
 if image:
  t=mat.node_tree.nodes.new('ShaderNodeTexImage');t.image=image;mat.node_tree.links.new(t.outputs['Alpha'],p.inputs['Alpha'])
 s.material=mat
scene=bpy.context.scene;scene.cycles.samples=16;cam=scene.camera;center=Vector((0,0,103));cam.location=(110,470,160);cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=172
for side in ['L','R']:
 for j in range(8):
  src='Ex_BackHairA'+('End' if j==7 else f'{j:02d}')+'_'+side;bone=rig.data.bones[mapping['mapping'][src]];position=rig.matrix_world@bone.head_local
  text=bpy.data.curves.new('A%d_%s'%(j,side),'FONT');text.body='A%d'%j;text.size=3.;text.align_x='CENTER';obj=bpy.data.objects.new(text.name,text);scene.collection.objects.link(obj);obj.location=position+Vector(((-9 if side=='R' else 9),14,0));obj.rotation_euler=cam.rotation_euler
scene.render.filepath=str(O/'braid-weight-bands.png');bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(O/'Hair-weight-inspection.blend'))
assert hashlib.sha256(game.read_bytes()).hexdigest()==before
report['production_blend_unchanged']=True
(O/'weight-preservation-and-probes.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report),flush=True)
