"""Export blink and three paired face/eyelid previews from the source morphs."""
import bpy,json,importlib.util,math
import numpy as np
from pathlib import Path
from mathutils import Matrix
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(R/'work/LeMalin-game-cm.blend'),load_ui=False,use_scripts=False)
mesh=bpy.data.objects['LeMaline v1.0_mesh'];rig=bpy.data.objects['LeMalin'];keys=mesh.data.shape_keys.key_blocks
blink=keys.get('まばたき');assert blink,'PMX blink morph missing'
count=sum((v.co-keys[0].data[i].co).length>1e-6 for i,v in enumerate(blink.data));assert count>100
presets = {
 'FaceSleepyEyes': {'eye_jitome_L': .55, 'eye_jitome_R': .55},
 'FaceSleepyFace': {'左眉下げ': .15, '右眉下げ': .15},
 'FaceSmileEyes': {'ウィンク': .18, 'ウィンク右': .18},
 'FaceSmileFace': {'にやり': .60, '左眉上げ': .08, '右眉上げ': .08},
 'FaceFocusEyes': {'eye_angry_L': .55, 'eye_angry_R': .55},
 'FaceFocusFace': {'brow_angry02': .45},
}
base = np.empty(len(mesh.data.vertices) * 3, dtype=np.float32)
keys[0].data.foreach_get('co', base)
for name, sources in presets.items():
 coordinates = base.copy()
 for source, weight in sources.items():
  original = keys.get(source)
  assert original, source
  values = np.empty_like(base)
  original.data.foreach_get('co', values)
  coordinates += (values - base) * weight
 shape = mesh.shape_key_add(name=name, from_mix=False)
 shape.data.foreach_set('co', coordinates)
 shape.value = 0

# Reproduce the PMX eye/return-bone skinning at four small angles. The baked
# deltas use the existing head skinning and the same morph path as expressions.
parser = Path.home()/'AppData/Roaming/Blender Foundation/Blender/5.2/extensions/blender_org/mmd_tools/core/pmx/__init__.py'
spec = importlib.util.spec_from_file_location('pmx_gaze', parser)
pmx = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pmx)
model = pmx.load(str(R/'source/LeMaline v1.0.pmx'))
assert len(model.vertices) == len(mesh.data.vertices)
positions = np.array([vertex.co for vertex in model.vertices], dtype=np.float64)
design = np.column_stack([positions, np.ones(len(positions))])
conversion, _, _, _ = np.linalg.lstsq(design, base.reshape(-1, 3), rcond=None)
fit_error = float(np.max(np.abs(design @ conversion - base.reshape(-1, 3))))
assert fit_error < .002, ('PMX vertex order/coordinate mismatch', fit_error)
eye_ids = {index for index, bone in enumerate(model.bones) if bone.name in {'左目','右目','左目戻','右目戻'}}
eye_weights = {index: np.zeros(len(positions)) for index in eye_ids}
for index, vertex in enumerate(model.vertices):
 weight = vertex.weight
 values = [1.] if len(weight.bones) == 1 else [weight.weights[0], 1-weight.weights[0]] if len(weight.bones) == 2 else weight.weights
 for bone, value in zip(weight.bones, values):
  if bone in eye_weights:eye_weights[bone][index] += value
gaze_presets = {'GazeLeft': ('Y', -4), 'GazeRight': ('Y', 4), 'GazeUp': ('X', 2), 'GazeDown': ('X', -2)}
gaze_report = {'coordinate_fit_error_cm': fit_error, 'presets': {}}
for name, (axis, degrees) in gaze_presets.items():
 rotation = np.array(Matrix.Rotation(math.radians(degrees), 3, axis))
 delta = np.zeros_like(positions)
 for index in eye_ids:
  bone = model.bones[index]
  pivot = np.array(bone.location)
  if bone.name.endswith('戻'):
   parent = model.bones[bone.parent]
   assert bone.additionalTransform[0] == bone.parent
   compensation = np.array(Matrix.Rotation(math.radians(degrees * bone.additionalTransform[1]), 3, axis))
   parent_pivot = np.array(parent.location)
   moved = ((positions-pivot) @ compensation.T + pivot-parent_pivot) @ rotation.T + parent_pivot
  else:
   moved = (positions-pivot) @ rotation.T + pivot
  delta += (moved-positions) * eye_weights[index][:, None]
 affected = np.linalg.norm(delta, axis=1) > 1e-8
 assert np.count_nonzero(affected) > 100
 direction = 0 if axis == 'Y' else 1
 expected_sign = 1 if name in {'GazeLeft','GazeUp'} else -1
 assert float(delta[affected, direction].mean()) * expected_sign > 0
 shape = mesh.shape_key_add(name=name, from_mix=False)
 shape.data.foreach_set('co', (base.reshape(-1, 3) + delta @ conversion[:3]).astype(np.float32).ravel())
 shape.value = 0
 gaze_report['presets'][name] = {'axis': axis, 'degrees': degrees, 'affected_vertices': int(np.count_nonzero(affected))}
for k in list(keys)[1:]:
 if k!=blink and k.name not in presets and k.name not in gaze_presets:mesh.shape_key_remove(k)
blink.name='Blink';blink.value=0
assert all(mesh.vertex_groups[g.group].name!='root' for v in mesh.data.vertices for g in v.groups if g.weight>0)
bpy.ops.object.select_all(action='DESELECT');mesh.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.wm.save_as_mainfile(filepath=str(R/'work/LeMalin-lively-cm.blend'))
bpy.ops.export_scene.fbx(filepath=str(R/'work/LeMalin-lively-cm.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',path_mode='STRIP',use_mesh_modifiers=False,mesh_smooth_type='FACE')
(R/'output/reports/blink-source.json').write_text(json.dumps({'morph':'Blink','source_morph':'まばたき','affected_vertices':count,'vertices':len(mesh.data.vertices),'triangles':len(mesh.data.polygons),'preserved_test02_weights':True},ensure_ascii=False,indent=2),encoding='utf-8')
(R/'output/reports/expression-source.json').write_text(json.dumps(presets,ensure_ascii=False,indent=2),encoding='utf-8')
(R/'output/reports/gaze-source.json').write_text(json.dumps(gaze_report,indent=2),encoding='utf-8')
