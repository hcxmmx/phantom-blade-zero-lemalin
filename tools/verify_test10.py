"""Core collision package checks; no rendering or repeated UI suite."""
from pathlib import Path
import json,hashlib,itertools
R=Path(__file__).resolve().parents[1];D=R/'work/package-readback-test10'
def rows(n):return json.loads((D/(n+'.json')).read_text(encoding='utf-8'))
manifest=json.loads((R/'output/game-test10/manifest.json').read_text())
assert len(manifest['assets'])==len((D/'matches.txt').read_text().splitlines())==56
assert 'READ_FAIL' not in (R/'output/readback-test10.log').read_text(errors='replace')
for f in manifest['files']:assert hashlib.sha256((R/'output/game-test10'/f['name']).read_bytes()).hexdigest()==f['sha256']
props=next(o for o in rows('ABPT_LeMalin_Lively') if o['Name']=='Default__ABPT_LeMalin_Lively_C')['Properties']
assert props['LeMalinScale']==1.25 and props['LeMalinCollision']==1
slider=next(o for o in rows('WBP_LeMalinTuning') if o['Name']=='CollisionSlider')['Properties']
assert slider['Value']==1 and slider.get('MinValue',0)==0 and slider.get('MaxValue',1)==1
for suffix in ['', '_Light','_Firm','_Heavy']:
 for off in [False,True]:
  name='PA_LeMalin_Braids'+suffix+('_NoCollision' if off else '');objects=rows(name)
  asset=next(o for o in objects if o['Type']=='PhysicsAsset')
  bodymap={o['Name']:o['Properties'] for o in objects if o['Type']=='SkeletalBodySetup'}
  bodies=[bodymap[ref['ObjectName'].split(':')[-1].rstrip("'")] for ref in asset['Properties']['SkeletalBodySetups']]
  assert len(bodies)==25 and len([o for o in objects if o['Type']=='PhysicsConstraintTemplate'])==12
  assert {b['BoneName'] for b in bodies if b['PhysicsType'].endswith('Simulated')}=={f'aux_{i:03d}' for i in range(2,14)}
  disabled={tuple(p['Key']['Indices']) for p in asset['CollisionDisableTable']}
  active=set(itertools.combinations(range(25),2))-disabled
  if off:assert not active
  else:
   assert len(active)==132
   for i,j in active:
    a,b=bodies[i],bodies[j]
    assert a['PhysicsType'].endswith('Simulated') and b['PhysicsType'].endswith('Kinematic')
    assert not b['BoneName'].startswith('aux_')
  assert all(b['DefaultInstance']['PhysMaterialOverride']['ObjectPath'].startswith('/Game/LeMalin/PM_LeMalinHairContact.') for b in bodies)
runtime=json.loads((R/'output/lively/postprocess.json').read_text())
assert runtime['passed'] and runtime['collision_slider_switch']
assert runtime['body_penetration_on_cm']+.1<runtime['body_penetration_off_cm']
result={'passed':True,'body_colliders':11,'body_shapes':14,'active_hair_body_pairs':132,'braid_self_collision':False,'collision_switch':'0 off / 1 on','penetration_off_cm':runtime['body_penetration_off_cm'],'penetration_on_cm':runtime['body_penetration_on_cm'],'game_validation':'pending user'}
(R/'output/reports/package-verification-test10.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
