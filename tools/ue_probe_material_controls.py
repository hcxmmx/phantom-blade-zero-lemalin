from pathlib import Path
import os,unreal as u
R=Path(u.Paths.project_dir()).resolve().parents[1]
if os.environ.get('LEMALIN_PROBE_PLAYER_PANEL') == '1':
    report = 'player-panel-hookup.json'
elif os.environ.get('LEMALIN_PROBE_GAZE') == '1':
    report = 'gaze-hookup.json'
elif os.environ.get('LEMALIN_PROBE_EXPRESSIONS') == '1':
    report = 'expression-preview-hookup.json'
elif os.environ.get('LEMALIN_PROBE_OUTFIT') == '1':
    report = 'outfit-style-hookup.json'
elif os.environ.get('LEMALIN_PROBE_BACK_VISIBILITY') == '1':
    report = 'back-visibility-hookup.json'
elif os.environ.get('LEMALIN_PROBE_EYES') == '1':
    report = 'eye-finish-hookup.json'
elif os.environ.get('LEMALIN_PROBE_HDR') == '1':
    report = 'hdr-hookup.json'
else:
    report = 'material-control-probe.json'
result=u.HcxRetargetTools.probe_le_malin_material_controls(
    u.load_asset('/Game/Hcx2B/SK_SourceHun'),
    u.load_asset('/HuaShira/Characters/Hero/Hero_Hun/SkinParts/Hun_Hair_Band'),
    u.load_asset('/Game/LeMalin/DiagnosticsRuntime/A_Hun_TangBlade_Walk_Fwd_Loop_Soul'),
    u.load_asset('/Game/LeMalin/ABP_LeMalin_Lively'),
    str(R/'output/reports'/report))
u.log('LEMALIN_MATERIAL_PROBE '+result)
assert result.startswith('OK'),result
