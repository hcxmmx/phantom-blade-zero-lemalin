"""Read-only topology/morph audit of the preserved PMX import."""
import bpy,json,math
from pathlib import Path
from collections import Counter
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(R/'work/LeMalin-imported.blend'),load_ui=False,use_scripts=False)
o=bpy.data.objects['LeMaline v1.0_mesh'];m=o.data
faces=Counter(tuple(sorted(p.vertices)) for p in m.polygons)
positions=Counter(tuple(round(c,6) for c in v.co) for v in m.vertices)
normals=m.corner_normals
r={'vertices':len(m.vertices),'polygons':len(m.polygons),'duplicate_faces_sharing_vertex_indices':sum(n-1 for n in faces.values()),'coincident_positions_rounded_1e_6':sum(n-1 for n in positions.values()),'zero_area_faces':sum(p.area<1e-12 for p in m.polygons),'nonfinite_vertices':sum(not all(math.isfinite(c) for c in v.co) for v in m.vertices),'invalid_corner_normals':sum(not all(math.isfinite(c) for c in n.vector) or n.vector.length<.5 for n in normals),'custom_normals':m.has_custom_normals,'nonzero_default_morphs':[(k.name,k.value) for k in m.shape_keys.key_blocks if k.name!='Basis' and k.value!=0],'vertex_morph_count':len(m.shape_keys.key_blocks)-1,'note':'Coincident vertices may be intentional UV/material seams or layered geometry. No merge or deletion performed.'}
(R/'output/reports/topology.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(r),flush=True)
