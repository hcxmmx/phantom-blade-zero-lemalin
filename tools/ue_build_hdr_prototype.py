import unreal as u
from pathlib import Path
import os
R=Path(u.Paths.project_dir()).resolve().parents[1]
for name in ['ue_create_hdr_prototype.py','ue_build_lively.py']:
    path=R/'tools'/name
    exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),{})
if os.environ.get('LEMALIN_PROBE_HDR')=='1':
    path=R/'tools/ue_probe_material_controls.py'
    exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),{})
