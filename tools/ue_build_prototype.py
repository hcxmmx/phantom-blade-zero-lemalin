"""Independent LeMalin UE assets and actual Hun diagnostic clips. No deployment."""
import unreal as u,json,os
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parents[1];D='/Game/LeMalin';O=R/'output/motion';O.mkdir(exist_ok=True)
tools=u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
options=u.FbxImportUI();options.import_as_skeletal=True;options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type=False;options.import_materials=False;options.import_textures=False;options.import_animations=False;options.create_physics_asset=False
options.skeletal_mesh_import_data.convert_scene=True;options.skeletal_mesh_import_data.convert_scene_unit=True
full=os.environ.get('LEMALIN_FULL_IMPORT')=='1'
filename='LeMalin-game-cm.fbx' if full else 'LeMalin-rig-proxy-cm.fbx'
target_name='SK_LeMalin' if full else 'SK_LeMalin_RigProxy'
if full:options.skeletal_mesh_import_data.normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
job=u.AssetImportTask();job.filename=str(R/'work'/filename);job.destination_path=D;job.destination_name=target_name;job.automated=True;job.replace_existing=True;job.save=True;job.options=options
tools.import_asset_tasks([job]);target=u.load_asset(D+'/'+target_name);source=u.load_asset('/Game/Hcx2B/SK_SourceHun');assert source and target
def asset(n,c,f):return u.load_asset(D+'/'+n) or tools.create_asset(n,D,c,f())
chains=[('Spine','spine_01','spine_05','spine_01','spine_02'),('Neck','neck_01','neck_02','neck','neck'),('Head','head','head','head','head')]
for s in ['l','r']:
 for n,a,b,c,d in [('Clavicle','clavicle','clavicle','clavicle','clavicle'),('Arm','upperarm','hand','upperarm','hand'),('Leg','thigh','foot','thigh','foot'),('Toe','ball','ball','ball','ball')]:chains.append((n+'_'+s,a+'_'+s,b+'_'+s,c+'_'+s,d+'_'+s))
 for f in ['thumb','index','middle','ring','pinky']:chains.append((f+'_'+s,f+'_01_'+s,f+'_03_'+s,f+'_01_'+s,f+'_03_'+s))
rigs=[]
for name,mesh,cols in [('IK_Hun',source,(1,2)),('IK_LeMalin',target,(3,4))]:
 rig=asset(name,u.IKRigDefinition,u.IKRigDefinitionFactory);c=u.IKRigController.get_controller(rig);assert c.set_skeletal_mesh(mesh);assert c.set_retarget_root('pelvis')
 for old in list(c.get_retarget_chains()):c.remove_retarget_chain(old.chain_name)
 for row in chains:c.add_retarget_chain(row[0],row[cols[0]],row[cols[1]],'None')
 rigs.append(rig)
rtg=asset('RTG_Hun_LeMalin',u.IKRetargeter,u.IKRetargetFactory);c=u.IKRetargeterController.get_controller(rtg)
c.set_ik_rig(u.RetargetSourceOrTarget.SOURCE,rigs[0]);c.set_ik_rig(u.RetargetSourceOrTarget.TARGET,rigs[1]);c.remove_all_ops()
for name in ['IKRetargetPelvisMotionOp','IKRetargetFKChainsOp']:
 i=c.add_retarget_op('/Script/IKRig.'+name);assert i>=0;c.run_op_initial_setup(i)
for row in chains:assert c.set_source_chain(row[0],row[0])
c.auto_align_all_bones(u.RetargetSourceOrTarget.TARGET)
for s in ['l','r']:
 for b in ['foot_','ball_']:c.set_rotation_offset_for_retarget_pose_bone(b+s,u.Quat(0,0,0,1),u.RetargetSourceOrTarget.TARGET)
report={'mesh':target.get_path_name(),'chains':chains,'clips':[],'limitations':['FK baseline; foot contact, grip and secondary dynamics require refinement.']}
for p in sorted((R/'work/motion').glob('*-samples.json')):
 name=p.name.removesuffix('-samples.json')
 assert u.HcxRetargetTools.bake_diagnostic_animation(source,target,rtg,str(p),D+'/Preview',str(O/(name+'.json'))),name
 report['clips'].append(name)
u.EditorAssetLibrary.save_directory(D,only_if_is_dirty=False)
(R/'output/reports/ue-prototype.json').write_text(json.dumps(report,indent=2))
u.log('LEMALIN_UE_PROTOTYPE_READY')
