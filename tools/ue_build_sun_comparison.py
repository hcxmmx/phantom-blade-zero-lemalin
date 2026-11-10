import unreal as u
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parents[1]
for name in ['ue_prepare_sun_comparison.py','ue_build_lively.py']:
    path=R/'tools'/name
    exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),{})
