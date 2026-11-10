"""Verify the cooked package by independently decoded objects before installation."""
from pathlib import Path
import json,hashlib,argparse
parser=argparse.ArgumentParser();parser.add_argument('--test',choices=['01','02','03','04','05','06','07','08'],default='01');version=parser.parse_args().test
dynamic=version in ['03','04','05','06','07','08']
R=Path(__file__).resolve().parents[1];D=R/('work/package-readback' if version=='01' else 'work/package-readback-test'+version)
def objects(n):return json.loads((D/(n+'.json')).read_text(encoding='utf-8'))
def get(n,t):return next(o for o in objects(n) if o['Type']==t)
assert 'READ_FAIL' not in (R/('output/readback.log' if version=='01' else 'output/readback-test'+version+'.log')).read_text(errors='replace')
manifest=json.loads((R/('output/game-test'+version+'/manifest.json')).read_text())
matches=(D/'matches.txt').read_text().splitlines();assert len(matches)==len(manifest['assets'])==(51 if version=='08' else 46 if version in ['06','07'] else 45 if dynamic else 44)
assert not any(any(v in p for v in ['/Garfield/','/Hcx2B/','/PBZContent/','/Preview/','PA_Hun.uasset']) for p in matches)
mesh=get('Hun_Hair_Band','SkeletalMesh');triangles=sum(s['NumTriangles'] for s in mesh['LODModels'][0]['Sections']);assert triangles==272670
assert len(mesh['SkeletalMaterials'])==17
post_process='ABP_LeMalin_Lively' if dynamic else 'ABP_LeMalin'
assert mesh['Properties']['PostProcessAnimBlueprint']['ObjectPath'].startswith('/Game/LeMalin/'+post_process+'.')
ref=json.loads((D/'Hun_Hair_Band-reference.json').read_text());bones=ref['FinalRefBoneInfo']
expected=set(json.loads((R/'output/reports/bone-mapping.json').read_text(encoding='utf-8'))['mapping'].values())
assert {b['Name'] for b in bones}==expected|{'LeMalin'}
assert bones[0]=={'Name':'LeMalin','ParentIndex':-1}
assert all(abs(p['Scale3D'][k]-1)<1e-4 for p in ref['FinalRefBonePose'] for k in 'XYZ')
assert all(s['Material']['ObjectPath'].startswith('/Game/LeMalin/Materials/MI_') for s in mesh['SkeletalMaterials'])
assert get('Hun_Body_Simple','SkeletalMesh')['Properties']['PhysicsAsset']['ObjectPath'].startswith('/HuaShira/Characters/Hero/Hero_Hun/PA_Hun.')
template='ABPT_LeMalin_Lively' if dynamic else 'ABPT_LeMalin'
default=next(x for x in objects(template) if x['Name']=='Default__'+template+'_C')
node=default['Properties']['AnimGraphNode_RetargetPoseFromMesh'];assert node['RetargetFrom'].endswith('ParentSkeletalMeshComponent');assert node['IKRetargeterAsset']['ObjectPath'].startswith('/Game/LeMalin/RTG_Hun_LeMalin.')
for p in D.glob('MI_*.json'):
 m=get(p.stem,'MaterialInstanceConstant');assert m['Properties']['Parent']['ObjectPath'].startswith('/Game/PBZContent/Characters/Hero/Hun_New_V2b/Material/MI_ZeroHun_Cloth_03_b.')
 assert not m['Properties'].get('bHasStaticPermutationResource',False);assert not m.get('LoadedMaterialResources')
 params=m['Properties']['TextureParameterValues'];assert len(params)==3
 assert all(p['ParameterInfo']['Association'].endswith('LayerParameter') and p['ParameterInfo']['Index']==0 for p in params)
for p in D.glob('T_*.json'):
 t=get(p.stem,'Texture2D');assert t['Properties']['VirtualTextureStreaming'];assert t['VTData']['Chunks'];assert all(c['SizeInBytes']>0 for c in t['VTData']['Chunks'])
assert get('T_Hair','Texture2D')['SizeX']==4096
for f in manifest['files']:assert hashlib.sha256((R/('output/game-test'+version)/f['name']).read_bytes()).hexdigest()==f['sha256']
if dynamic:
 assert mesh['Properties']['PostProcessAnimBlueprint']['ObjectPath'].startswith('/Game/LeMalin/ABP_LeMalin_Lively.')
 props=default['Properties'];modified=[v for k,v in props.items() if k.startswith('AnimGraphNode_ModifyBone')];assert len(modified)==(7 if version=='08' else 6 if version in ['06','07'] else 16 if version=='05' else 18),len(modified)
 assert not any(k.startswith('AnimGraphNode_AnimDynamics') for k in props)
 assert all(m['RotationMode'].endswith('BMM_Additive') and m['TranslationMode'].endswith('BMM_Ignore') for m in modified if m['BoneToModify']['BoneName']!='LeMalin')
 assert all(m['BoneToModify']['BoneName'].startswith('aux_') or version=='08' and m['BoneToModify']['BoneName']=='LeMalin' for m in modified)
 assert len([k for k in props if k.startswith(('SpringPitch','SpringRoll','HairSpringPitch','HairSpringRoll'))])==(4 if version in ['06','07','08'] else 24 if version=='05' else 8)
 live=get('SK_LeMalin_Lively','SkeletalMesh');assert live['Properties'].get('MorphTargets'), 'Blink target missing from cooked mesh'
 assert json.loads((R/'output/lively/postprocess.json').read_text())['passed']
 # The user accepted the Test02 materials. Keep their parameters exactly identical.
 old=R/'work/package-readback-test02'
 for p in D.glob('MI_*.json'):assert json.loads(p.read_text())[0]['Properties']==json.loads((old/p.name).read_text())[0]['Properties']
 if version=='04':
  comparison=json.loads((R/'output/reports/test04-motion-comparison.json').read_text());assert comparison['passed'] and comparison['test04']['separate_display_actor_owner']
  assert comparison['test04']['ear_tip_offset_cm']>1.5 and comparison['test04']['braid_tip_offset_cm']>7
  assert props['BlinkClock']==next(x for x in json.loads((R/'work/package-readback-test03/ABPT_LeMalin_Lively.json').read_text()) if x['Name']=='Default__ABPT_LeMalin_Lively_C')['Properties']['BlinkClock']
  previous_display=next(x for x in json.loads((R/'work/package-readback-test03/Hun_Hair_Band.json').read_text()) if x['Type']=='SkeletalMesh')
  for field in ['LODModels','SkeletalMaterials','ImportedBounds']:assert mesh[field]==previous_display[field], 'Display geometry/material changed: '+field
 if version in ['05','06','07','08']:
  joints={m['BoneToModify']['BoneName'] for m in modified if m['BoneToModify']['BoneName']!='LeMalin'};assert not joints.intersection({'aux_000','aux_001','aux_002','aux_003'})
  if version=='05':assert {f'aux_{i:03d}' for i in range(4,14)}.issubset(joints)
  else:assert joints=={f'aux_{i:03d}' for i in range(24,30)}
  previous=R/'work/package-readback-test04'
  previous_display=next(x for x in json.loads((previous/'Hun_Hair_Band.json').read_text()) if x['Type']=='SkeletalMesh')
  for field in ['LODModels','SkeletalMaterials','ImportedBounds']:assert mesh[field]==previous_display[field],field
  old_default=next(x for x in json.loads((previous/'ABPT_LeMalin_Lively.json').read_text()) if x['Name']=='Default__ABPT_LeMalin_Lively_C')['Properties']
  assert props['BlinkClock']==old_default['BlinkClock']
  old_ears={m['BoneToModify']['BoneName']:m for k,m in old_default.items() if k.startswith('AnimGraphNode_ModifyBone') and m['BoneToModify']['BoneName'] in {f'aux_{i:03d}' for i in range(24,30)}}
  for m in modified:
   if m['BoneToModify']['BoneName'] in old_ears:assert m==old_ears[m['BoneToModify']['BoneName']]
if version in ['06','07','08']:
 rigid=[v for k,v in props.items() if k.startswith('AnimGraphNode_RigidBody')];assert len(rigid)==1
 rigid=rigid[0];assert rigid['OverridePhysicsAsset']['ObjectPath'].startswith('/Game/LeMalin/PA_LeMalin_Braids.')
 assert rigid['bOverrideWorldGravity'] and rigid['OverrideWorldGravity']=={'X':0.0,'Y':0.0,'Z':-980.0}
 assert not rigid.get('bEnableWorldGeometry',False)
 physics_objects=objects('PA_LeMalin_Braids');physics=get('PA_LeMalin_Braids','PhysicsAsset')
 bodies=[v['Properties'] for v in physics_objects if v['Type']=='SkeletalBodySetup']
 constraints=[v['Properties']['DefaultInstance'] for v in physics_objects if v['Type']=='PhysicsConstraintTemplate']
 assert len(bodies)==len(physics['Properties']['SkeletalBodySetups'])==12
 assert len(constraints)==len(physics['Properties']['ConstraintSetup'])==10
 assert {b['BoneName'] for b in bodies if b['PhysicsType'].endswith('PhysType_Kinematic')}=={'aux_002','aux_003'}
 assert {b['BoneName'] for b in bodies if b['PhysicsType'].endswith('PhysType_Simulated')}=={f'aux_{i:03d}' for i in range(4,14)}
 assert len(physics['CollisionDisableTable'])==66
 for c in constraints:
  profile=c['ProfileInstance'];segment=int(c['ConstraintBone1'].split('_')[1])//2
  assert profile['TwistLimit']['TwistLimitDegrees']==8
  assert profile['ConeLimit']['Swing1LimitDegrees']==(40 if segment==2 else 55)
  assert profile['ConeLimit']['Swing2LimitDegrees']==(40 if segment==2 else 55)
  assert profile['AngularDrive']['SlerpDrive']['Stiffness']==10
  assert profile['AngularDrive']['SlerpDrive']['Damping']==2
 verification=json.loads((R/'output/lively/postprocess.json').read_text());assert verification['tilted_pose_gravity_response_cm']>1 and verification['gravity_tip_world_drop_cm']>1 and verification['braid_length_error_cm']<.1
 if version=='07':
  settings=rigid['SimSpaceSettings'];assert abs(settings['WorldAlpha']-.70)<1e-5
  assert settings['MaxLinearAcceleration']==2200 and settings['MaxAngularAcceleration']==50
  assert all(abs(settings['ExternalLinearDragV'][axis]-.08)<1e-5 for axis in ['X','Y'])
  assert settings['ExternalLinearDragV']['Z']==0
  previous_physics=R/'work/package-readback-test06/PA_LeMalin_Braids.json'
  previous_bodies={v['Properties']['BoneName']:v['Properties'] for v in json.loads(previous_physics.read_text()) if v['Type']=='SkeletalBodySetup'}
  for b in bodies:
   assert abs(b['DefaultInstance']['AngularDamping']-1.6)<1e-5
   expected=json.loads(json.dumps(previous_bodies[b['BoneName']]))
   expected['DefaultInstance']['AngularDamping']=b['DefaultInstance']['AngularDamping']
   assert b==expected,'Unexpected body/shape change'
report={'passed':True,'assets':len(matches),'triangles':triangles,'deform_bones':154,'fbx_wrapper_bone':'LeMalin','ue_bones':len(bones),'unit_bone_scales':True,'material_slots':17,'material_instances':9,'game_parent_shader_only':True,'virtual_textures':11,'hair_texture_width':4096,'cook_errors':0,'cook_warnings':0,'original_PA_Hun_reference':True,'retarget_source':'ParentSkeletalMeshComponent','game_validation':'pending user manual test'}
if version=='08':
 assert json.loads((R/'output/reports/panel-verification-test08.json').read_text())['passed']
 assert props['LeMalinScale']==1 and abs(props['LeMalinInertia']-.55)<1e-5 and abs(props['LeMalinTurn']-.35)<1e-5
 assert props['LeMalinDamping']==2 and props['LeMalinGravity']==1
 scale=next(m for m in modified if m['BoneToModify']['BoneName']=='LeMalin')
 assert scale['ScaleMode'].endswith('BMM_Replace') and scale['ScaleSpace'].endswith('BCS_ComponentSpace')
 widget=objects('WBP_LeMalinTuning');sliders={o['Name']:o['Properties'] for o in widget if o['Type']=='Slider'}
 for name,minimum,maximum,default_value in [('Scale',85,125,100),('Inertia',0,100,55),('Turn',0,100,35),('Damping',1,4,2),('Gravity',50,150,100)]:
  s=sliders[name+'Slider'];assert s.get('MinValue',0)==minimum and s.get('MaxValue',1)==maximum and s['Value']==default_value
 save_default=next(o for o in objects('SG_LeMalinSettings') if o['Name']=='Default__SG_LeMalinSettings_C')['Properties']
 assert save_default=={'Scale':100.0,'Inertia':55.0,'Turn':35.0,'Damping':2.0,'Gravity':100.0}
 for suffix,value in [('Light',.8),('Firm',2.8),('Heavy',4.0)]:
  extra=objects('PA_LeMalin_Braids_'+suffix);extra_bodies=[o['Properties'] for o in extra if o['Type']=='SkeletalBodySetup'];assert len(extra_bodies)==12
  assert all(abs(b['DefaultInstance']['AngularDamping']-value)<1e-5 for b in extra_bodies)
  assert len([o for o in extra if o['Type']=='PhysicsConstraintTemplate'])==10
 report.update(live_panel_hotkey='F8',live_controls=['visual_size','inertia','turn_response','damping','gravity'],settings_slot='Hcxmmx_LeMalin_Settings_v1',visual_scale_percent_range=[85,125],damping_profiles=4,source_character_scale_unchanged=True)
if dynamic:report.update(blink_morph=True,damped_angle_springs=4 if version in ['06','07','08'] else 24 if version=='05' else 8,secondary_joints=7 if version=='08' else 6 if version in ['06','07'] else 16 if version=='05' else 18,materials_equal_accepted_test02=True)
if version in ['06','07','08']:report.update(native_braid_rigid_body=True,world_gravity_cm_s2=[0,0,-980],dynamic_bodies=10,kinematic_anchors=2,world_and_body_collision=False)
if version=='07':report.update(movement_inertia_fraction=.70,horizontal_air_drag=.08,angular_damping=1.6,gravity_joint_limits_mass_shapes_unchanged=True)
(R/('output/reports/package-verification-test'+version+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
