import bpy,json,numpy as np
from pathlib import Path
from mathutils import Matrix,Vector,Quaternion
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(R/'work/LeMalin-game-cm.blend'),load_ui=False,use_scripts=False)
rig=bpy.data.objects['LeMalin'];mesh=bpy.data.objects['LeMaline v1.0_mesh'];data=json.loads((R/'output/motion/Hun_TangBlade_Light_A01.json').read_text())['target'];lookup={n:i for i,n in enumerate(data['bones'])}
def mat(v):return Matrix.LocRotScale(Vector(v[:3]),Quaternion((v[6],*v[3:6])),Vector(v[7:10]))
ref={n:mat(data['reference_global'][i]).inverted() for n,i in lookup.items()}
regions={k:set() for k in ['Face','Hair','HairFront','Body','Cloth','Obj']}
for p in mesh.data.polygons:
 name=mesh.data.materials[p.material_index].name.removeprefix('Preview_')
 if name in regions:regions[name].update(p.vertices)
weightlist={}
for key,indices in regions.items():
 ids=sorted(indices)[::5]; rows=[]
 for i in ids:
  row=[(mesh.vertex_groups[g.group].name,g.weight) for g in mesh.data.vertices[i].groups if g.weight>1e-6];rows.append(row)
 weightlist[key]=rows
summary={key:dict() for key in regions}
for key,rows in weightlist.items():
 counts={}
 for row in rows:
  for n,w in row:counts[n]=counts.get(n,0)+w
 summary[key]['top_weights']=sorted(counts.items(),key=lambda x:-x[1])[:8];summary[key]['min_singular']=1.;summary[key]['collapsed_samples']=0
for frame in range(0,len(data['frames']),4):
 pose=data['frames'][frame];rots={n:np.array((mat(pose[i])@ref[n]).to_3x3()) for n,i in lookup.items()}
 for key,rows in weightlist.items():
  if not rows:continue
  Js=np.array([sum((w*rots[n] for n,w in row),np.zeros((3,3))) for row in rows]);sv=np.linalg.svd(Js,compute_uv=False);minimum=float(sv[:,-1].min())
  if minimum<summary[key]['min_singular']:summary[key]['min_singular']=minimum;summary[key]['worst_frame']=frame
  summary[key]['collapsed_samples']+=int((sv[:,-1]<.2).sum())
(R/'output/reports/skin-collapse.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary),flush=True)
