"""Read the attachment capture from the mod's settings save without modifying it."""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
SAVES = Path('D:/youxi/steamapps/common/PBZ/ProjectPBZ/Saved/SaveGames/Steam')
files = list(SAVES.glob('*/Hcxmmx_LeMalin_Settings_v1.sav'))
if not files:
    raise SystemExit('Mod settings save not found')

save = max(files, key=lambda path: path.stat().st_mtime)
data = save.read_bytes()
capture = None
for encoding in ['utf-8', 'utf-16-le']:
    start = data.find('SOURCE '.encode(encoding))
    if start < 4:
        continue
    count = struct.unpack_from('<i', data, start - 4)[0]
    if encoding == 'utf-8' and count > 0:
        end = start + count - 1
    elif encoding == 'utf-16-le' and count < 0:
        end = start + (-count - 1) * 2
    else:
        continue
    if end <= len(data):
        capture = data[start:end].decode(encoding)
        break
if not capture:
    raise SystemExit('No attachment capture yet. Open F8, hide back mounts and save settings.')

output = ROOT / 'output/reports/game-back-attachments.txt'
output.write_text(capture, encoding='utf-8')
print(output)
for line in capture.splitlines():
    if any(word in line.lower() for word in ['source ', 'knife', 'scabbard']):
        print(line)
