"""View exact UE retarget output on the independent LeMalin Blender mesh."""
import bpy,json,math
from pathlib import Path
import numpy as np
from mathutils import Matrix,Vector,Quaternion
R=Path(__file__).resolve().parents[1];O=R/'output/motion'
bpy.ops.wm.open_mainfile(filepath=str(R/'work/LeMalin-game-cm.blend'),load_ui=False,use_scripts=False)
rig=bpy.data.objects['LeMalin'];mesh=bpy.data.objects['LeMaline v1.0_mesh']
clips=[json.loads(p.read_text()) for p in sorted(O.glob('Hun*.json'))];assert len(clips)==3
def matrix(v):return Matrix.LocRotScale(Vector(v[:3]),Quaternion((v[6],*v[3:6])),Vector(v[7:10]))
data=clips[0]['target'];lookup={b.name.lower():b for b in rig.data.bones};matches=[(i,lookup[n.lower()]) for i,n in enumerate(data['bones']) if n.lower() in lookup]
assert len(matches)==len(rig.data.bones),(len(matches),len(rig.data.bones))
U=np.array([data['reference_global'][i][:3] for i,b in matches]);B=np.array([tuple(b.matrix_local.translation) for i,b in matches]);uc,bc=U.mean(0),B.mean(0);us,bs=U-uc,B-bc
l,sv,r=np.linalg.svd(us.T@bs);rot=r.T@l.T;scale=sv.sum()/(us*us).sum();trans=bc-scale*rot@uc
C=Matrix.Identity(4)
for y in range(3):
 for x in range(3):C[y][x]=scale*rot[y,x]
 C[y][3]=trans[y]
error=float(np.max(np.linalg.norm((scale*(rot@U.T)).T+trans-B,axis=1)));assert error<.02,error
corrections={i:matrix(data['reference_global'][i]).inverted()@C.inverted()@b.matrix_local for i,b in matches}
segments=[];start=1
for clip in clips:
 rig.animation_data_create();rig.animation_data.action=bpy.data.actions.new(clip['name']);frames=clip['target']['frames']
 for f,pose in enumerate(frames):
  absolute={b.name:C@matrix(pose[i])@corrections[i] for i,b in matches}
  for i,b in matches:
   pb=rig.pose.bones[b.name];pb.rotation_mode='QUATERNION';basis=b.matrix_local.inverted()@absolute[b.name]
   if b.parent:basis=b.matrix_local.inverted()@b.parent.matrix_local@absolute[b.parent.name].inverted()@absolute[b.name]
   pb.matrix_basis=basis
   for channel in ['location','rotation_quaternion','scale']:pb.keyframe_insert(channel,frame=start+f,group=b.name)
 action=rig.animation_data.action;rig.animation_data.action=None;track=rig.animation_data.nla_tracks.new();strip=track.strips.new(clip['name'],start,action);strip.action_frame_start=start;strip.action_frame_end=start+len(frames)-1;strip.extrapolation='NOTHING'
 segments.append({'clip':clip['name'],'start':start,'end':start+len(frames)-1});bpy.context.scene.timeline_markers.new(clip['name'],frame=start);start+=len(frames)+15
scene=bpy.context.scene;scene.frame_start=1;scene.frame_end=segments[-1]['end'];scene.render.fps=30
scene.render.engine='CYCLES';scene.cycles.samples=20;scene.cycles.use_denoising=True
scene.world=bpy.data.worlds.new('Motion Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.15,.17,.21,1)
scene.view_settings.view_transform='AgX'
center=Vector((0,0,85))
for pos,power,size in [((200,-400,500),3500000,400),((-300,-200,200),1800000,300),((100,300,400),4500000,300)]:
 d=bpy.data.lights.new('Area','AREA');d.energy=power;d.size=size;o=bpy.data.objects.new('Area',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=20000,location=(0,0,-.3));floor=bpy.context.object;floor.name='MotionFloor';mat=bpy.data.materials.new('Floor');mat.diffuse_color=(.08,.10,.13,1);floor.data.materials.append(mat)
d=bpy.data.cameras.new('MotionCamera');cam=bpy.data.objects.new('MotionCamera',d);scene.collection.objects.link(cam);scene.camera=cam;d.type='ORTHO';d.ortho_scale=220;d.clip_end=100000
cam.location=(230,-600,180);cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
scene.render.resolution_x=850;scene.render.resolution_y=950;scene.render.resolution_percentage=100
for screen in bpy.data.screens:
 for area in screen.areas:
  if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.clip_end=100000
scene.frame_set(1);bpy.ops.wm.save_as_mainfile(filepath=str(O/'LeMalin-idle-walk-attack.blend'))
report={'reference_fit_error_cm':error,'segments':segments,'checks':[]}
for seg in segments:
 name='idle' if 'Idle' in seg['clip'] else 'walk' if 'Walk' in seg['clip'] else 'attack'
 for index,offset in enumerate([0,(seg['end']-seg['start'])//3,2*(seg['end']-seg['start'])//3]):
  frame=seg['start']+offset;scene.frame_set(frame);bpy.context.view_layer.update();ev=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());pts=[ev.matrix_world@v.co for v in ev.data.vertices];assert all(math.isfinite(c) for p in pts for c in p)
  report['checks'].append({'clip':name,'frame':frame,'floor_min_cm':min(p.z for p in pts),'bounds_min':[min(p[i] for p in pts) for i in range(3)],'bounds_max':[max(p[i] for p in pts) for i in range(3)]})
  scene.render.filepath=str(O/f'{name}-{index}.png');bpy.ops.render.render(write_still=True)
(O/'preview-report.json').write_text(json.dumps(report,indent=2))
print('LEMALIN_MOTION_PREVIEW_READY',flush=True)
