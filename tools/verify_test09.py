"""Core checks only: actual cooked changes and one native runtime smoke result."""
from pathlib import Path
import json,hashlib
R=Path(__file__).resolve().parents[1];D=R/'work/package-readback-test09'
def objects(name):return json.loads((D/(name+'.json')).read_text(encoding='utf-8'))
manifest=json.loads((R/'output/game-test09/manifest.json').read_text())
assert len((D/'matches.txt').read_text().splitlines())==len(manifest['assets'])==51
assert 'READ_FAIL' not in (R/'output/readback-test09.log').read_text(errors='replace')
for f in manifest['files']:assert hashlib.sha256((R/'output/game-test09'/f['name']).read_bytes()).hexdigest()==f['sha256']
props=next(o for o in objects('ABPT_LeMalin_Lively') if o['Name']=='Default__ABPT_LeMalin_Lively_C')['Properties']
assert props['LeMalinScale']==1.25 and props.get('LeMalinTurn',0)==0
rigid=next(v for k,v in props.items() if k.startswith('AnimGraphNode_RigidBody'))
assert rigid['SimSpaceSettings'].get('MaxAngularVelocity',0)==0 and rigid['SimSpaceSettings'].get('MaxAngularAcceleration',0)==0
sliders={o['Name']:o['Properties'] for o in objects('WBP_LeMalinTuning') if o['Type']=='Slider'}
assert sliders['ScaleSlider']['MinValue']==125 and sliders['ScaleSlider']['MaxValue']==150 and sliders['ScaleSlider']['Value']==125
assert sliders['TurnSlider'].get('Value',0)==0 and sliders['TurnSlider'].get('bIsEnabled',True)==False
for suffix in ['', '_Light','_Firm','_Heavy']:
 rows=objects('PA_LeMalin_Braids'+suffix)
 bodies=[o['Properties'] for o in rows if o['Type']=='SkeletalBodySetup']
 joints=[o['Properties']['DefaultInstance'] for o in rows if o['Type']=='PhysicsConstraintTemplate']
 assert len(bodies)==14 and len(joints)==12
 assert {b['BoneName'] for b in bodies if b['PhysicsType'].endswith('Kinematic')}=={'aux_000','aux_001'}
 assert {b['BoneName'] for b in bodies if b['PhysicsType'].endswith('Simulated')}=={f'aux_{i:03d}' for i in range(2,14)}
runtime=json.loads((R/'output/lively/postprocess.json').read_text())
assert runtime['passed'] and runtime['a1_gravity_angle_degrees']>1 and runtime['default_scale_error_cm']<.02
assert runtime['default_visual_scale']==1.25 and runtime['turn_angular_velocity_limit']==runtime['turn_angular_acceleration_limit']==0
result={'passed':True,'size_percent':[125,150],'default_scale':1.25,'extra_turn_inertia':'disabled','gravity_bones':'A1-A6 L/R; A0 anchored','a1_gravity_angle_degrees':runtime['a1_gravity_angle_degrees'],'game_validation':'pending user'}
(R/'output/reports/package-verification-test09.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
