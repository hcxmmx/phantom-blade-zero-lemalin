"""Render the new UMG panel once, using the runtime widget and its bindings."""
from pathlib import Path
import os,time,unreal as u
R=Path(u.Paths.project_dir()).resolve().parents[1];D='/Game/LeMalin'
u.EditorPythonScripting.set_keep_python_script_alive(True)
start=time.monotonic();done=False
def tick(delta):
 global start,done
 if time.monotonic()-start<8:return
 if not done:
  done=True
  try:
   source=u.load_asset('/Game/Hcx2B/SK_SourceHun')
   target=u.load_asset('/HuaShira/Characters/Hero/Hero_Hun/SkinParts/Hun_Hair_Band')
   seq=u.load_asset(D+'/DiagnosticsRuntime/A_Hun_TangBlade_Walk_Fwd_Loop_Soul')
   bp=u.load_asset(D+'/ABP_LeMalin_Lively')
   if os.environ.get('LEMALIN_PROBE_PLAYER_PANEL') == '1':
    result=u.HcxRetargetTools.probe_le_malin_material_controls(source,target,seq,bp,str(R/'output/reports/player-panel-hookup.json'))
   else:
    result=u.HcxRetargetTools.verify_le_malin_tuning_panel(source,target,seq,bp,str(R/'output/reports/lemalin-panel-preview.json'))
   u.log('LEMALIN_PANEL_PREVIEW '+result)
   (R/'output/reports/panel-preview-result.txt').write_text(result,encoding='utf-8')
  finally:start=time.monotonic()
 else:
  u.unregister_slate_post_tick_callback(handle)
  u.SystemLibrary.quit_editor()
handle=u.register_slate_post_tick_callback(tick)
