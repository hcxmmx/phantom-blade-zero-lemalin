"""Lightweight skeleton proxy for motion baking on a memory-constrained workstation."""
import bpy
from pathlib import Path
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(R/'work/LeMalin-game-cm.blend'),load_ui=False,use_scripts=False)
rig=bpy.data.objects['LeMalin'];verts=[];faces=[]
for b in rig.data.bones:
 p=b.head_local;idx=len(verts);verts.extend([tuple(p),tuple(p+__import__('mathutils').Vector((.15,0,0))),tuple(p+__import__('mathutils').Vector((0,.15,.15)))]);faces.append((idx,idx+1,idx+2))
data=bpy.data.meshes.new('RigProxy');data.from_pydata(verts,[],faces);mesh=bpy.data.objects.new('RigProxy',data);bpy.context.scene.collection.objects.link(mesh)
for i,b in enumerate(rig.data.bones):mesh.vertex_groups.new(name=b.name).add([i*3,i*3+1,i*3+2],1,'REPLACE')
mod=mesh.modifiers.new('Skin','ARMATURE');mod.object=rig
bpy.ops.object.select_all(action='DESELECT');mesh.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.export_scene.fbx(filepath=str(R/'work/LeMalin-rig-proxy-cm.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',path_mode='STRIP')
