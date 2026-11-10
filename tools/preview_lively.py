"""Replay the actual UE postprocess sample, including its Blink curve, on the accepted mesh."""
import bpy,json,sys,numpy as np
from pathlib import Path
from mathutils import Matrix,Vector,Quaternion
R=Path(__file__).resolve().parents[1];strong='--test04' in sys.argv;O=R/('output/lively-test04' if strong else 'output/lively');O.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(R/'output/motion/LeMalin-idle-walk-attack.blend'),load_ui=False,use_scripts=False)
rig=bpy.data.objects['LeMalin'];mesh=bpy.data.objects['LeMaline v1.0_mesh'];rig.animation_data_clear();mesh.data.shape_keys.animation_data_clear()
clip=json.loads((R/'output/lively/postprocess.json').read_text());data=clip['target'];lookup={b.name:b for b in rig.data.bones};pairs=[(i,lookup[n]) for i,n in enumerate(data['bones']) if n in lookup]
def mat(v):return Matrix.LocRotScale(Vector(v[:3]),Quaternion((v[6],*v[3:6])),Vector(v[7:10]))
U=np.array([data['reference_global'][i][:3] for i,b in pairs]);B=np.array([tuple(b.matrix_local.translation) for i,b in pairs]);uc,bc=U.mean(0),B.mean(0);us,bs=U-uc,B-bc;l,sv,r=np.linalg.svd(us.T@bs);rot=r.T@l.T;scale=sv.sum()/(us*us).sum();trans=bc-scale*rot@uc;C=Matrix.Identity(4)
for y in range(3):
 for x in range(3):C[y][x]=scale*rot[y,x]
 C[y][3]=trans[y]
err=float(np.max(np.linalg.norm((scale*(rot@U.T)).T+trans-B,axis=1)));assert err<.02
corr={i:mat(data['reference_global'][i]).inverted()@C.inverted()@b.matrix_local for i,b in pairs}
blink=mesh.data.shape_keys.key_blocks['まばたき']
for f,pose in enumerate(data['frames'],1):
 absolute={b.name:C@mat(pose[i])@corr[i] for i,b in pairs}
 for i,b in pairs:
  pb=rig.pose.bones[b.name];pb.rotation_mode='QUATERNION';basis=b.matrix_local.inverted()@absolute[b.name]
  if b.parent:basis=b.matrix_local.inverted()@b.parent.matrix_local@absolute[b.parent.name].inverted()@absolute[b.name]
  pb.matrix_basis=basis
  for channel in ['location','rotation_quaternion','scale']:pb.keyframe_insert(channel,frame=f,group=b.name)
 blink.value=clip['blink'][f-1];blink.keyframe_insert('value',frame=f)
scene=bpy.context.scene;scene.frame_start=1;scene.frame_end=len(data['frames']);scene.frame_set(1)
scene.timeline_markers.clear();bpy.ops.wm.save_as_mainfile(filepath=str(O/'LeMalin-lively-preview.blend'))
cam=scene.camera;cam.location=(210,470,190);center=Vector((0,0,100));cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
if strong:
 old=json.loads((R/'work/test03-stable/postprocess.json').read_text())['target'];frames={}
 for label,bone in [('braids','aux_012'),('ears','aux_028')]:
  i=data['bones'].index(bone);j=old['bones'].index(bone);diff=[np.linalg.norm(np.array(a[i][:3])-np.array(b[j][:3])) for a,b in zip(data['frames'],old['frames'])];frames[label]=int(np.argmax(diff))+1
 scene.frame_set(frames['braids']);scene.render.filepath=str(O/'braids-strong.png');bpy.ops.render.render(write_still=True)
 center=Vector((0,0,145));cam.location=(25,-250,152);cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=68
 scene.frame_set(frames['ears']);scene.render.filepath=str(O/'ears-strong.png');bpy.ops.render.render(write_still=True)
 print('STRONG_LIVELY_PREVIEW_READY',frames,flush=True);raise SystemExit
scene.frame_set(110);scene.render.filepath=str(O/'braids.png');bpy.ops.render.render(write_still=True)
center=Vector((0,0,142));cam.location=(20,-250,151);cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=58
for name,frame in [('eyes-open',1),('eyes-closed',int(np.argmax(clip['blink']))+1)]:
 scene.frame_set(frame);scene.render.filepath=str(O/(name+'.png'));bpy.ops.render.render(write_still=True)
print('LIVELY_PREVIEW_READY',flush=True)
