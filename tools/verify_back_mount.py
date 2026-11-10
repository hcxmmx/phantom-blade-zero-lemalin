"""Core cooked checks for the live back-mount build."""
from pathlib import Path
import argparse,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--version',default='0.2.0-dev.1');v=p.parse_args().version
R=Path(__file__).resolve().parents[1];D=R/('work/package-readback-'+v)
def rows(n):return json.loads((D/(n+'.json')).read_text(encoding='utf-8'))
m=json.loads((R/('output/game-'+v)/'manifest.json').read_text());assert m['version']=='LeMalin-'+v
assert len(m['assets'])==len((D/'matches.txt').read_text().splitlines())==56
assert 'READ_FAIL' not in (R/('output/readback-'+v+'.log')).read_text(errors='replace')
for f in m['files']:assert hashlib.sha256((R/('output/game-'+v)/f['name']).read_bytes()).hexdigest()==f['sha256']
props=next(o for o in rows('ABPT_LeMalin_Lively') if o['Name']=='Default__ABPT_LeMalin_Lively_C')['Properties']
assert props['LeMalinBackDistance']==6 and props['LeMalinScale']==1.25 and props['LeMalinCollision']==1
sliders={o['Name']:o['Properties'] for o in rows('WBP_LeMalinTuning') if o['Type']=='Slider'}
s=sliders['BackDistanceSlider'];assert s.get('MinValue',0)==0 and s['MaxValue']==20 and s['Value']==6
saved=next(o for o in rows('SG_LeMalinSettings') if o['Name']=='Default__SG_LeMalinSettings_C')['Properties'];assert saved['BackDistance']==6
baseline=R/'work/package-readback-test10'
for name in ['PA_LeMalin_Braids','PA_LeMalin_Braids_Light','PA_LeMalin_Braids_Firm','PA_LeMalin_Braids_Heavy','PA_LeMalin_Braids_NoCollision','PA_LeMalin_Braids_Light_NoCollision','PA_LeMalin_Braids_Firm_NoCollision','PA_LeMalin_Braids_Heavy_NoCollision','PM_LeMalinHairContact']:
 old=json.loads((baseline/(name+'.json')).read_text(encoding='utf-8'))
 assert [(o['Name'],o['Properties']) for o in rows(name)]==[(o['Name'],o['Properties']) for o in old],name+' physics settings changed'
runtime=json.loads((R/'output/reports/back-mount-verification.json').read_text());assert runtime['passed'] and runtime['back_mount_error_cm']<.02 and runtime['back_distance_save_load']
result={'passed':True,'version':v,'back_distance_cm':[0,20],'default_cm':6,'back_mount_error_cm':runtime['back_mount_error_cm'],'save_load':True,'accepted_physics_unchanged':True,'game_validation':'pending user'}
(R/('output/reports/package-verification-'+v+'.json')).write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
