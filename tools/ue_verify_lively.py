from pathlib import Path
import json,unreal as u
R=Path(u.Paths.project_dir()).resolve().parents[1];D='/Game/LeMalin'
source=u.load_asset('/Game/Hcx2B/SK_SourceHun');target=u.load_asset('/HuaShira/Characters/Hero/Hero_Hun/SkinParts/Hun_Hair_Band');seq=u.load_asset(D+'/DiagnosticsRuntime/A_Hun_TangBlade_Walk_Fwd_Loop_Soul');bp=u.load_asset(D+'/ABP_LeMalin_Lively');assert source and target and seq and bp
result=u.HcxRetargetTools.verify_le_malin_liveliness(source,target,seq,bp,str(R/'output/lively/postprocess.json'))
report=json.loads((R/'output/reports/lively-setup.json').read_text());report['verification']=result;report['stress_test']='walk, acceleration to 500 cm/s, stop, 120 degree turn and return'
(R/'output/reports/lively-setup.json').write_text(json.dumps(report,indent=2));u.log('LEMALIN_STRESS_VERIFY '+result);assert result=='OK',result
