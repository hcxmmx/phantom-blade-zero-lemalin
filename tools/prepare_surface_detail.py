"""Pack existing artist occlusion into ARM; retain the accepted roughness/metallic."""
from pathlib import Path
from PIL import Image

R = Path(__file__).resolve().parents[1]
O = R / 'work/textures'
O.mkdir(parents=True, exist_ok=True)
for part, source, strength in [
    ('Hair', 'TEX_LML_Hair_AO.png', .25),
    ('Cloth', 'TEX_LML_Cloth_AOMask.png', .5),
]:
    with Image.open(R / 'source/tex' / source) as image:
        ao = image.convert('L').point(lambda value: round(255 * (1 - strength) + value * strength))
    Image.merge('RGB', (ao, Image.new('L', ao.size, 180), Image.new('L', ao.size, 0))).save(O / f'{part}_Detail_ARM.png')
print('Packed hair/cloth detail ARM; roughness 180 and metallic 0 retained')
