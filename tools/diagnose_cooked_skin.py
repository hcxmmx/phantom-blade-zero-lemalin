"""Compare pre-export skin against geometry/weights independently read from the installed container."""
import bpy,json,numpy as np
from pathlib import Path
from mathutils import Matrix,Vector,Quaternion,kdtree
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(R/'work/LeMalin-game-cm.blend'),load_ui=False,use_scripts=False)
source=bpy.data.objects['LeMalin'];sm=bpy.data.objects['LeMaline v1.0_mesh'];before=set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(next((R/'work/deformation-cooked/exported').rglob('*.glb'))))
new=set(bpy.data.objects)-before;target=next(o for o in new if o.type=='ARMATURE');meshes=[o for o in new if o.type=='MESH']
def fit(arm):
 data=json.loads((R/'output/motion/Hun_TangBlade_Light_A01.json').read_text())['target'];lookup={b.name.lower():b for b in arm.data.bones};pairs=[(i,lookup[n.lower()]) for i,n in enumerate(data['bones']) if n.lower() in lookup]
 U=np.array([data['reference_global'][i][:3] for i,b in pairs]);B=np.array([tuple(b.matrix_local.translation) for i,b in pairs]);uc,bc=U.mean(0),B.mean(0);us,bs=U-uc,B-bc;l,sv,r=np.linalg.svd(us.T@bs);rot=r.T@l.T;scale=sv.sum()/(us*us).sum();trans=bc-scale*rot@uc;C=Matrix.Identity(4)
 for y in range(3):
  for x in range(3):C[y][x]=scale*rot[y,x]
  C[y][3]=trans[y]
 residual=float(np.max(np.linalg.norm((scale*(rot@U.T)).T+trans-B,axis=1)));assert residual<.02,residual
 def mat(v):return Matrix.LocRotScale(Vector(v[:3]),Quaternion((v[6],*v[3:6])),Vector(v[7:10]))
 corrections={i:mat(data['reference_global'][i]).inverted()@C.inverted()@b.matrix_local for i,b in pairs}
 return pairs,C,corrections,residual,mat
a=fit(source);b=fit(target)
align=source.matrix_world@a[1]@b[1].inverted()@target.matrix_world.inverted()
def coords(objects,convert=Matrix.Identity(4)):
 pts=[]
 for o in objects:
  ev=o.evaluated_get(bpy.context.evaluated_depsgraph_get());pts.extend([tuple(convert@ev.matrix_world@v.co) for v in ev.data.vertices])
 return np.array(pts)
sr=coords([sm]);tr=coords(meshes,align);tree=kdtree.KDTree(len(sr))
for i,p in enumerate(sr):tree.insert(p,i)
tree.balance();nearest=[tree.find(p) for p in tr];indices=np.array([i for p,i,d in nearest]);rest_errors=np.array([d for p,i,d in nearest]);report={'source_vertices':len(sr),'cooked_vertices':len(tr),'rest_max_cm':float(rest_errors.max()),'rest_p99_cm':float(np.quantile(rest_errors,.99)),'source_fit_cm':a[3],'cooked_fit_units':b[3],'samples':[]}
def apply(arm,prepared,pose):
 pairs,C,corr,_,mat=prepared;absolute={bn.name:C@mat(pose[i])@corr[i] for i,bn in pairs}
 for i,bn in pairs:
  pb=arm.pose.bones[bn.name];basis=bn.matrix_local.inverted()@absolute[bn.name]
  if bn.parent:basis=bn.matrix_local.inverted()@bn.parent.matrix_local@absolute[bn.parent.name].inverted()@absolute[bn.name]
  pb.matrix_basis=basis
for name in ['Hun_TangBlade_Idle_Loop','Hun_TangBlade_Light_A01','Hun_TangBlade_Walk_Fwd_Loop']:
 data=json.loads((R/'output/motion'/(name+'.json')).read_text())['target']
 for frame in [0,len(data['frames'])//3,2*len(data['frames'])//3]:
  apply(source,a,data['frames'][frame]);apply(target,b,data['frames'][frame]);bpy.context.view_layer.update();sp=coords([sm]);tp=coords(meshes,align);errors=np.linalg.norm(tp-sp[indices],axis=1);valid=rest_errors<.001
  report['samples'].append({'clip':name,'frame':frame,'max_cm':float(errors[valid].max()),'p99_cm':float(np.quantile(errors[valid],.99)),'vertices_over_1cm':int(sum(errors[valid]>1))})
(R/'output/reports/cooked-skin-comparison.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
