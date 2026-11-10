import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[1];O=R/'output/diagnosis/weight-fix';O.mkdir(exist_ok=True,parents=True)
bpy.ops.wm.open_mainfile(filepath=str(R/'output/motion/LeMalin-idle-walk-attack.blend'),load_ui=False,use_scripts=False)
rig=bpy.data.objects['LeMalin'];mesh=bpy.data.objects['LeMaline v1.0_mesh']
with bpy.data.libraries.load(str(R/'work/LeMalin-game-cm.blend')) as (src,dst):dst.objects=['LeMaline v1.0_mesh']
correct=dst.objects[0];weights=[[(correct.vertex_groups[g.group].name,g.weight) for g in v.groups if g.weight>0] for v in correct.data.vertices]
mesh.vertex_groups.clear();groups={n:mesh.vertex_groups.new(name=n) for n in {n for row in weights for n,w in row}}
for i,row in enumerate(weights):
 for n,w in row:groups[n].add([i],w,'REPLACE')
bpy.data.objects.remove(correct,do_unlink=True)
bpy.context.scene.frame_set(1);bpy.ops.wm.save_as_mainfile(filepath=str(O/'LeMalin-weight-fix.blend'))
for name,frame in [('idle',1),('attack',410),('walk',628)]:
 bpy.context.scene.frame_set(frame);bpy.context.scene.render.filepath=str(O/(name+'.png'));bpy.ops.render.render(write_still=True)
