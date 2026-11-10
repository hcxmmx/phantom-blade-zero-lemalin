"""Add Blink + bounded native secondary physics while keeping the Test02 retargeter/materials."""
from pathlib import Path
import json,os,unreal as u
R=Path(u.Paths.project_dir()).resolve().parents[1];D='/Game/LeMalin';B='/HuaShira/Characters/Hero/Hero_Hun';tools=u.AssetToolsHelpers.get_asset_tools()
if os.environ.get('LEMALIN_PLAYER_RELEASE') == '1':
 prepare = R/'tools/ue_prepare_native_ui.py'
 exec(compile(prepare.read_text(encoding='utf-8'), str(prepare), 'exec'), {})
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
base=u.load_asset(D+'/SK_LeMalin');assert base
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/HuaShira',D],True)
for path in [B+'/SkinParts/Hun_Hair_Band',D+'/ABP_LeMalin_Lively',D+'/ABPT_LeMalin_Lively',D+'/WBP_LeMalinTuning',D+'/SG_LeMalinSettings',D+'/PA_LeMalin_Ears_Upright',D+'/PA_LeMalin_Ears']+[D+'/PA_LeMalin_Braids'+s+'_NoCollision' for s in ['', '_Light','_Firm','_Heavy']]+[D+'/PA_LeMalin_Braids_Light',D+'/PA_LeMalin_Braids_Firm',D+'/PA_LeMalin_Braids_Heavy',D+'/PA_LeMalin_Braids',D+'/PM_LeMalinHairContact']:
 if u.EditorAssetLibrary.does_asset_exist(path):assert u.EditorAssetLibrary.delete_asset(path)
u.SystemLibrary.collect_garbage()
options=u.FbxImportUI();options.import_as_skeletal=True;options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH;options.automated_import_should_detect_type=False;options.import_materials=False;options.import_textures=False;options.import_animations=False;options.create_physics_asset=False;options.skeleton=base.skeleton
options.skeletal_mesh_import_data.set_editor_property('import_morph_targets',True);options.skeletal_mesh_import_data.normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
job=u.AssetImportTask();job.filename=str(R/'work/LeMalin-lively-cm.fbx');job.destination_path=D;job.destination_name='SK_LeMalin_Lively';job.automated=True;job.replace_existing=True;job.replace_existing_settings=True;job.save=True;job.options=options
if os.environ.get('LEMALIN_REUSE_LIVELY_MESH')!='1':tools.import_asset_tasks([job])
mesh=u.load_asset(D+'/SK_LeMalin_Lively');assert mesh and mesh.skeleton==base.skeleton
names=[m.get_name() for m in mesh.get_editor_property('morph_targets')]
assert set(names)=={'Blink','FaceSleepyEyes','FaceSleepyFace','FaceSmileEyes','FaceSmileFace','FaceFocusEyes','FaceFocusFace','GazeLeft','GazeRight','GazeUp','GazeDown'},names
materials={str(s.material_slot_name):s.material_interface for s in base.get_editor_property('materials')};slots=mesh.get_editor_property('materials')
for i,s in enumerate(slots):s.material_interface=materials[str(s.material_slot_name)];slots[i]=s
mesh.set_editor_property('materials',slots);u.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
template=u.load_asset(D+'/ABPT_LeMalin_Lively')
if not template:
 f=u.AnimBlueprintFactory();f.set_editor_property('template',True);f.set_editor_property('parent_class',u.AnimInstance);template=tools.create_asset('ABPT_LeMalin_Lively',D,u.AnimBlueprint,f)
 assert u.HcxRetargetTools.build_retarget_graph(template,u.load_asset(D+'/RTG_Hun_LeMalin'))
result=u.HcxRetargetTools.build_le_malin_liveliness(template,mesh);assert result.startswith(('OK','already')),result
panel=u.HcxRetargetTools.build_le_malin_tuning_panel(template);assert panel.startswith('OK'),panel
if os.environ.get('LEMALIN_PLAYER_RELEASE') == '1':
 assert u.EditorAssetLibrary.save_directory('/Game/PBZContent/UI/Fonts', only_if_is_dirty=False, recursive=True)
physics=u.load_asset(D+'/PA_LeMalin_Braids');assert physics
u.EditorAssetLibrary.save_loaded_asset(physics,only_if_is_dirty=False)
panel_assets=['PA_LeMalin_Ears','PA_LeMalin_Ears_Upright','WBP_LeMalinTuning','SG_LeMalinSettings','PA_LeMalin_Braids_Light','PA_LeMalin_Braids_Firm','PA_LeMalin_Braids_Heavy']
panel_assets += ['PA_LeMalin_Braids'+s+'_NoCollision' for s in ['', '_Light','_Firm','_Heavy']]+['PM_LeMalinHairContact']
for name in panel_assets:assert u.EditorAssetLibrary.save_asset(D+'/'+name,only_if_is_dirty=False)
u.EditorAssetLibrary.save_loaded_asset(template,only_if_is_dirty=False)
bp=u.load_asset(D+'/ABP_LeMalin_Lively')
if not bp:
 f=u.AnimBlueprintFactory();f.set_editor_property('parent_class',u.EditorAssetLibrary.load_blueprint_class(D+'/ABPT_LeMalin_Lively'));f.set_editor_property('target_skeleton',mesh.skeleton);bp=tools.create_asset('ABP_LeMalin_Lively',D,u.AnimBlueprint,f)
u.BlueprintEditorLibrary.compile_blueprint(bp);u.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
path=B+'/SkinParts/Hun_Hair_Band'
visible=tools.duplicate_asset('Hun_Hair_Band',B+'/SkinParts',mesh);visible.set_editor_property('post_process_anim_blueprint',u.EditorAssetLibrary.load_blueprint_class(D+'/ABP_LeMalin_Lively'));u.EditorAssetLibrary.save_loaded_asset(visible,only_if_is_dirty=False)
allow=json.loads((R/'metadata/cook-allowlist.json').read_text());allow=[a for a in allow if a not in ['Content/LeMalin/ABPT_LeMalin.uasset','Content/LeMalin/ABP_LeMalin.uasset']]
allow += ['Content/LeMalin/'+n+'.uasset' for n in ['SK_LeMalin_Lively','ABPT_LeMalin_Lively','ABP_LeMalin_Lively','PA_LeMalin_Braids']]
allow += ['Content/LeMalin/'+n+'.uasset' for n in panel_assets]
(R/'metadata/cook-allowlist.json').write_text(json.dumps(sorted(set(allow)),indent=2))
source=u.load_asset('/Game/Hcx2B/SK_SourceHun');seq=u.load_asset(D+'/DiagnosticsRuntime/A_Hun_TangBlade_Walk_Fwd_Loop_Soul');assert seq
(R/'output/lively').mkdir(exist_ok=True)
back_only=os.environ.get('LEMALIN_BACK_ONLY')=='1'
skip_checks=os.environ.get('LEMALIN_SKIP_VERIFICATION')=='1'
verify='skipped for fast in-game iteration' if skip_checks else 'not repeated: accepted 0.1.0 dynamics' if back_only else u.HcxRetargetTools.verify_le_malin_liveliness(source,visible,seq,bp,str(R/'output/lively/postprocess.json'));u.log('LEMALIN_LIVELY_VERIFY '+verify)
panel_verify='skipped for fast in-game iteration' if skip_checks else u.HcxRetargetTools.verify_le_malin_tuning_panel(source,visible,seq,bp,str(R/'output/reports/back-mount-verification.json')) if back_only or os.environ.get('LEMALIN_CORE_ONLY','1')!='1' else 'not repeated: core-only validation';u.log('LEMALIN_PANEL_VERIFY '+panel_verify)
report={'generation':result,'panel':panel,'verification':verify,'morphs':names,'secondary_chains':4,'ear_rotation_joints':6,'damped_angle_springs':0,'ear_dynamic_bodies':6,'ear_kinematic_anchors':1,'braid_dynamic_bodies':12,'braid_kinematic_anchors':2,'world_gravity_cm_s2':[0,0,-980],'braid_physics_asset':physics.get_path_name(),'full_collision_simulation':False,'body_materials_retargeter':'unchanged from accepted Test02; visual scale adjustable','ears':'independent root-to-tip rigid-body chains, symmetric swing limits, upright recovery','blink':'unchanged','post_process':bp.get_path_name(),'game_validation':'pending user test'}
report['panel_verification']=panel_verify
report.update(body_collision=True,body_collider_bones=11,body_collider_shapes=14,collision_switch='0 off / 1 on',braid_self_collision=False,world_collision=False)
(R/'output/reports/lively-setup.json').write_text(json.dumps(report,indent=2));assert verify=='OK' or back_only or skip_checks,verify;assert panel_verify=='OK' or skip_checks or not back_only and os.environ.get('LEMALIN_CORE_ONLY','1')=='1',panel_verify
u.log('LEMALIN_LIVELY_READY')
if any(os.environ.get(key) == '1' for key in ['LEMALIN_PROBE_BACK_VISIBILITY', 'LEMALIN_PROBE_EXPRESSIONS', 'LEMALIN_PROBE_GAZE', 'LEMALIN_PROBE_PLAYER_PANEL']):
 probe = R / 'tools/ue_probe_material_controls.py'
 exec(compile(probe.read_text(encoding='utf-8'), str(probe), 'exec'), {})
