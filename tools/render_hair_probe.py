"""Isolated 20-degree bend on an investigation copy, never a production asset."""
import bpy,math
from pathlib import Path
from mathutils import Vector,Quaternion
R=Path(__file__).resolve().parents[1];O=R/'output/hair-investigation'
bpy.ops.wm.open_mainfile(filepath=str(O/'Hair-weight-inspection.blend'),load_ui=False,use_scripts=False)
rig=bpy.data.objects['LeMalin'];b=rig.data.bones['aux_006'];p=rig.pose.bones[b.name]
axis=b.matrix_local.to_quaternion().inverted()@Vector((0,1,0));p.rotation_mode='QUATERNION';p.rotation_quaternion=Quaternion(axis,math.radians(20))
for o in bpy.data.objects:
 if o.type=='FONT':o.hide_render=True
bpy.context.view_layer.update();bpy.context.scene.render.filepath=str(O/'isolated-middle-bend.png');bpy.ops.render.render(write_still=True)
