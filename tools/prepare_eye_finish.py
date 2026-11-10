"""Create linear ARM inputs for the two eye gloss comparison materials."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'work/textures'
OUTPUT.mkdir(parents=True, exist_ok=True)

for name, roughness in [('Gloss', 0.18), ('Crystal', 0.06)]:
    image = Image.new('RGB', (128, 128), (255, round(roughness * 255), 0))
    image.save(OUTPUT / f'Eye_{name}_ARM.png')

print('Eye ARM textures ready, roughness 0.18 / 0.06, metallic 0')
