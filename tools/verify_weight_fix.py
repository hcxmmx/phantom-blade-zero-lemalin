import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[1];snapshots=[]
def collect(path):
 bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False,use_scripts=False);m=bpy.data.objects['LeMaline v1.0_mesh'];groups={g.index:g.name for g in m.vertex_groups}
 rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
 snapshots.append({'vertices':[tuple(v.co) for v in m.data.vertices],'bones':[(b.name,b.parent.name if b.parent else None,[list(row) for row in b.matrix_local]) for b in rig.data.bones]})
 return [{groups[g.group]:g.weight for g in v.groups if g.weight>0} for v in m.data.vertices]
before=collect(R/'work/test01-conversion/LeMalin-game-cm.blend');after=collect(R/'work/LeMalin-game-cm.blend')
assert snapshots[0]==snapshots[1], 'Geometry or rig changed unexpectedly'
mapping=json.loads((R/'output/reports/bone-mapping.json').read_text(encoding='utf-8'));assert set(mapping['excluded_non_bone_groups'])=={'mmd_edge_scale','mmd_vertex_order'}
assert all(abs(sum(v.values())-1)<1e-6 for v in after)
assert not any('root' in v for v in after)
bpy.ops.wm.open_mainfile(filepath=str(R/'work/LeMalin-imported.blend'),load_ui=False,use_scripts=False);mesh=bpy.data.objects['LeMaline v1.0_mesh'];bones=set(next(o for o in bpy.data.objects if o.type=='ARMATURE').data.bones.keys())
assert len(after)==len(before)==len(mesh.data.vertices)==153033
root=[v.get('root',0) for v in before]
metadata={g.name:sum(next((e.weight for e in v.groups if e.group==g.index),0) for v in mesh.data.vertices) for g in mesh.vertex_groups if g.name not in bones}
report={'passed':True,'vertices':len(after),'excluded_non_bone_groups':list(metadata),'non_bone_weight_totals':metadata,'before_vertices_attached_to_root':sum(w>0 for w in root),'before_root_weight_range':[min(root),max(root)],'after_vertices_attached_to_root':0,'after_all_weights_normalized':True,'max_real_influences':max(len(v) for v in after),'rig_and_geometry_unchanged':True}
(R/'output/reports/weight-fix.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
