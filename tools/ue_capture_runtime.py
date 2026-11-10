import unreal as u
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parents[1];O=R/'output/diagnosis';O.mkdir(exist_ok=True)
source=u.load_asset('/Game/Hcx2B/SK_SourceHun');target=u.load_asset('/HuaShira/Characters/Hero/Hero_Hun/SkinParts/Hun_Hair_Band');bp=u.load_asset('/Game/LeMalin/ABP_LeMalin')
for word in ['Idle_Loop','Light_A01','Walk_Fwd_Loop']:
 name='Hun_TangBlade_'+word
 assert u.HcxRetargetTools.bake_diagnostic_animation(source,target,u.load_asset('/Game/LeMalin/RTG_Hun_LeMalin'),str(R/'work/motion'/(name+'-samples.json')),'/Game/LeMalin/DiagnosticsRuntime',str(O/(word+'-processor.json')))
 seq=u.load_object(None,'/Game/LeMalin/DiagnosticsRuntime/A_'+name+'_Soul');assert seq
 u.EditorAssetLibrary.save_loaded_asset(seq,only_if_is_dirty=False)
 assert u.HcxRetargetTools.capture_gait_cycle(source,target,seq,bp,bp,str(O/(word+'-runtime.json')))
u.log('LEMALIN_RUNTIME_CAPTURE_READY')
