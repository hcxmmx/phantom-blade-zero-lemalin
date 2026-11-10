"""Read-only PMX hair skin/physics inventory. Writes investigation reports only."""
import importlib.util,json,math
from pathlib import Path
from collections import defaultdict,Counter
R=Path(__file__).resolve().parents[1];O=R/'output/hair-investigation';O.mkdir(parents=True,exist_ok=True)
parser=Path.home()/'AppData/Roaming/Blender Foundation/Blender/5.2/extensions/blender_org/mmd_tools/core/pmx/__init__.py'
spec=importlib.util.spec_from_file_location('pmx_hair_audit',parser);pmx=importlib.util.module_from_spec(spec);spec.loader.exec_module(pmx)
m=pmx.load(str(R/'source/LeMaline v1.0.pmx'));mapping=json.loads((R/'output/reports/bone-mapping.json').read_text(encoding='utf-8'))['mapping']
def weights(v):
 w=v.weight
 values=[1.] if w.type==0 else [w.weights[0],1-w.weights[0]] if w.type==1 else w.weights
 assert w.type in [0,1,2]
 return [(m.bones[i].name,float(a)) for i,a in zip(w.bones,values) if i>=0 and a>1e-7]
W=[weights(v) for v in m.vertices]
def pos(p):return [p[0]*8,-p[2]*8,p[1]*8]
V=[pos(v.co) for v in m.vertices];members={};start=0
for mat in m.materials:
 faces=m.faces[start:start+mat.vertex_count//3];members[mat.name]=set(i for face in faces for i in face);start+=mat.vertex_count//3
groups=defaultdict(list)
for i,row in enumerate(W):
 for n,w in row:
  if 'Hair' in n:groups[n].append((i,w))
bone_rows=[]
for i,b in enumerate(m.bones):
 if 'Hair' not in b.name:continue
 vertices=groups[b.name];s=sum(w for i,w in vertices)
 bone_rows.append({'name':b.name,'game_bone':mapping.get(b.name),'parent':m.bones[b.parent].name if b.parent>=0 else None,'position_cm':pos(b.location),'weight_sum':s,'weighted_vertices':len(vertices),'centroid_cm':[sum(V[i][k]*w for i,w in vertices)/s for k in range(3)] if s else None,'bounds_cm':[[min(V[i][k] for i,w in vertices) for k in range(3)],[max(V[i][k] for i,w in vertices) for k in range(3)]] if s else None})
bands=[]
hair=members['Hair']|members.get('HairFront',set())
for lo,hi in [(0,70),(70,90),(90,110),(110,125),(125,140),(140,180)]:
 ids=[i for i in hair if lo<=V[i][2]<hi];counts=Counter();dominants=Counter()
 for i in ids:
  for n,w in W[i]:counts[n]+=w
  dominants[max(W[i],key=lambda t:t[1])[0]]+=1
 bands.append({'height_cm':[lo,hi],'vertices':len(ids),'top_weights':counts.most_common(12),'dominant_bones':dominants.most_common(12)})
rigids=[]
for i,r in enumerate(m.rigids):
 if 'BackHair' in r.name or 'HairRibbon' in r.name:rigids.append({'index':i,**vars(r),'bound_bone':m.bones[r.bone].name if r.bone>=0 else None})
joints=[vars(j) for j in m.joints if 'BackHair' in j.name or 'HairRibbon' in j.name]
report={'source':'source/LeMaline v1.0.pmx','production_files_modified':False,'bones':bone_rows,'height_bands':bands,'rigids':rigids,'joints':joints,'materials':{n:len(v) for n,v in members.items()}}
(O/'pmx-hair-audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('HAIR_BANDS',json.dumps(bands));print('HAIR_BONE_WEIGHTS',json.dumps([b for b in bone_rows if b['name'].startswith(('Ex_BackHair','Ex_SideHair'))]));print('PHYSICS_COUNTS',len(rigids),len(joints))
