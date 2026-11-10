"""Blender 5.2: lossless PMX work import and reproducible material inspection."""
import bpy, sys, json, math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(Path.home()/'AppData/Roaming/Blender Foundation/Blender/5.2/extensions/blender_org'))
import zipfile
addon=Path(sys.path[0])/'mmd_tools'
deps=R/'work/python-deps';deps.mkdir(exist_ok=True)
for wheel in (addon/'wheels').glob('*.whl'):
 with zipfile.ZipFile(wheel) as z:z.extractall(deps)
sys.path.insert(0,str(deps))
import mmd_tools
mmd_tools.register()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mmd_tools.import_model(filepath=str(R/'source/LeMaline v1.0.pmx'),scale=.08,types={'MESH','ARMATURE','MORPHS','PHYSICS','DISPLAY'},clean_model=False,remove_doubles=False,rename_bones=False)
for image in bpy.data.images:
 if image.name.lower().startswith('toon') and not image.has_data:
  toon=Path(mmd_tools.__file__).parent/'externals/MikuMikuDance'/Path(image.filepath).name
  if toon.exists():image.filepath=str(toon);image.reload()
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and getattr(o,'mmd_type','NONE')=='NONE']
report={'blender':bpy.app.version_string,'mmd_tools':mmd_tools.MMD_TOOLS_VERSION,'meshes':[], 'materials':[], 'bones':[]}
for o in meshes:
 o.data.calc_loop_triangles()
 report['meshes'].append({'name':o.name,'vertices':len(o.data.vertices),'triangles':len(o.data.loop_triangles),'shape_keys':[(k.name,k.value) for k in o.data.shape_keys.key_blocks] if o.data.shape_keys else [],'modifiers':[(m.name,m.type) for m in o.modifiers],'unweighted':sum(not any(g.weight>0 for g in v.groups) for v in o.data.vertices)})
for b in rig.data.bones:
 report['bones'].append({'name':b.name,'name_j':b.mmd_bone.name_j if hasattr(b,'mmd_bone') else '', 'parent':b.parent.name if b.parent else None,'head':list(b.head_local),'tail':list(b.tail_local)})
for m in bpy.data.materials:
 report['materials'].append({'name':m.name,'diffuse':list(m.diffuse_color),'nodes':[(n.name,n.bl_idname,getattr(getattr(n,'image',None),'filepath',None)) for n in m.node_tree.nodes] if m.use_nodes else []})
report['images']=[{'name':i.name,'path':i.filepath,'size':list(i.size),'loaded':i.has_data} for i in bpy.data.images if i.source=='FILE']
(R/'output/reports/import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
for i in bpy.data.images:
 if i.source=='FILE' and i.has_data:i.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(R/'work/LeMalin-imported.blend'))
# Keep original MMD shader in imported.blend; use explicit PBR preview copies here.
for o in meshes:
 for s in o.material_slots:
  old=s.material
  if not old:continue
  m=old.copy();m.name='Preview_'+old.name;s.material=m
  textures=[n.image for n in old.node_tree.nodes if n.type=='TEX_IMAGE' and n.image] if old.use_nodes else []
  main=next((n.image for n in old.node_tree.nodes if n.type=='TEX_IMAGE' and n.name=='mmd_base_tex'),None) if old.use_nodes else None
  main=main or (textures[0] if textures else None)
  m.use_nodes=True;m.node_tree.nodes.clear();p=m.node_tree.nodes.new('ShaderNodeBsdfPrincipled');out=m.node_tree.nodes.new('ShaderNodeOutputMaterial');m.node_tree.links.new(p.outputs['BSDF'],out.inputs['Surface'])
  p.inputs['Base Color'].default_value=old.diffuse_color;p.inputs['Roughness'].default_value=.45
  p.inputs['Alpha'].default_value=old.diffuse_color[3]
  if main:
   t=m.node_tree.nodes.new('ShaderNodeTexImage');t.image=main;m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color']);m.node_tree.links.new(t.outputs['Alpha'],p.inputs['Alpha'])
  m.surface_render_method='DITHERED';m.use_transparency_overlap=False
 for mod in o.modifiers:
  if mod.type=='SOLIDIFY':mod.show_render=False;mod.show_viewport=False
# Hide PMX helper geometry; leave all mesh topology and morph defaults intact.
for o in bpy.context.scene.objects:
 if o not in meshes and o.type not in {'ARMATURE','EMPTY'}:o.hide_render=True
rig.hide_render=True
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.19,.21,.26,1)
scene.view_settings.view_transform='AgX'
points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
lo=Vector(tuple(min(p[i] for p in points) for i in range(3)));hi=Vector(tuple(max(p[i] for p in points) for i in range(3)));center=(lo+hi)/2;h=hi.z-lo.z
report['bounds']={'min':list(lo),'max':list(hi),'height':h}
bpy.ops.mesh.primitive_plane_add(size=h*200,location=(0,0,lo.z-.005));floor=bpy.context.object;floor.name='StudioFloor'
mat=bpy.data.materials.new('StudioFloor');mat.diffuse_color=(.085,.10,.13,1);floor.data.materials.append(mat)
for pos,power,size in [((2,-4,5),500,4),((-3,-2,2),300,3),((1,3,4),600,3)]:
 d=bpy.data.lights.new('Studio','AREA');d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new('Studio',d);scene.collection.objects.link(o);o.location=center+Vector(pos);o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('PreviewCamera');cam=bpy.data.objects.new('PreviewCamera',d);scene.collection.objects.link(cam);scene.camera=cam;d.type='ORTHO';d.ortho_scale=h*1.17;d.clip_end=1000
scene.render.resolution_x=900;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
for screen in bpy.data.screens:
 for area in screen.areas:
  if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA'
def view(angle):
 cam.location=center+Vector((math.sin(angle)*h*3,-math.cos(angle)*h*3,h*.10));cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
view(.20)
(R/'output/reports/import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(R/'work/LeMalin-material-preview.blend'))
for name,angle in [('front',0),('three-quarter',.55),('back',math.pi)]:
 view(angle);scene.render.filepath=str(R/'output/preview'/f'{name}.png');bpy.ops.render.render(write_still=True)
print('LEMALIN_IMPORT_PREVIEW_READY',flush=True)
