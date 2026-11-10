"""Bake leg position and surface direction into a thin-stocking comparison."""
from pathlib import Path
import importlib.util
from PIL import Image, ImageFilter
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'work/textures'
parser = Path.home() / 'AppData/Roaming/Blender Foundation/Blender/5.2/extensions/blender_org/mmd_tools/core/pmx/__init__.py'
spec = importlib.util.spec_from_file_location('pmx_sheer', parser)
pmx = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pmx)
model = pmx.load(str(ROOT / 'source/LeMaline v1.0.pmx'))
size = 2048
coverage = np.zeros((size, size), dtype=np.float32)
height = np.zeros_like(coverage)
front = np.zeros_like(coverage)
side = np.zeros_like(coverage)
start = 0
faces = []
for material in model.materials:
    material_faces = model.faces[start:start + material.vertex_count // 3]
    start += material.vertex_count // 3
    if material.name != 'Cloth':
        continue
    # Four mesh components together form the continuous lower-body hosiery.
    # The UV/mesh split at the upper thighs is not a garment boundary.
    parents = {i: i for face in material_faces for i in face}
    def root(i):
        while parents[i] != i:
            parents[i] = parents[parents[i]]
            i = parents[i]
        return i
    for a, b, c in material_faces:
        parents[root(a)] = root(b)
        parents[root(c)] = root(b)
    groups = {}
    for i in parents:
        groups.setdefault(root(i), []).append(i)
    stockings = {key for key, indices in groups.items()
                 if min(model.vertices[i].co[1] for i in indices) < 9.1
                 and max(model.vertices[i].co[1] for i in indices) < 11.3
                 and max(abs(model.vertices[i].co[0]) for i in indices) < 2}
    assert len(stockings) == 4, 'Expected legs and both upper-thigh/pelvis panels'
    faces = [face for face in material_faces if root(face[0]) in stockings]
assert faces, 'Stocking geometry missing'
knee = next(b.location[1] for b in model.bones if b.name == '左ひざ')
for face in faces:
    vertices = [model.vertices[i] for i in face]
    uv = np.array([v.uv for v in vertices]) * (size - 1)
    low = np.maximum(np.floor(uv.min(axis=0)).astype(int), 0)
    high = np.minimum(np.ceil(uv.max(axis=0)).astype(int), size - 1)
    x, y = np.meshgrid(np.arange(low[0], high[0] + 1) + .5,
                       np.arange(low[1], high[1] + 1) + .5)
    a, b, c = uv
    denominator = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
    if abs(denominator) < 1e-8:
        continue
    wa = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / denominator
    wb = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / denominator
    wc = 1 - wa - wb
    inside = (wa >= -1e-5) & (wb >= -1e-5) & (wc >= -1e-5)
    region = np.s_[low[1]:high[1] + 1, low[0]:high[0] + 1]
    coverage[region][inside] = 1
    heights = [v.co[1] for v in vertices]
    facing = [np.clip(-v.normal[2], 0, 1) for v in vertices]
    lateral = [abs(v.normal[0]) for v in vertices]
    height[region][inside] = (wa * heights[0] + wb * heights[1] + wc * heights[2])[inside]
    front[region][inside] = (wa * facing[0] + wb * facing[1] + wc * facing[2])[inside]
    side[region][inside] = (wa * lateral[0] + wb * lateral[1] + wc * lateral[2])[inside]

def fill_and_soften(values, maximum):
    image = Image.fromarray(np.rint(values / maximum * 255).clip(0, 255).astype('uint8'))
    return np.asarray(image.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(1)), dtype=np.float32) / 255 * maximum

mask_image = Image.fromarray((coverage * 255).astype('uint8')).filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(.7))
mask = np.asarray(mask_image, dtype=np.float32) / 255
height = fill_and_soften(height, 12)
front = fill_and_soften(front, 1)
side = fill_and_soften(side, 1)
knee_stretch = np.exp(-((height - knee) / .65) ** 2) * front
# Continuous density gradients follow the leg shape across the separate panels.
# Darker sides and gentler skin tint give the opaque approximation more depth.
thigh_stretch = np.exp(-((height - knee - 2.4) / 1.65) ** 2)
shin_stretch = np.exp(-((height - knee + 2.2) / 1.45) ** 2)
transmission = (.018 + .052 * thigh_stretch + .030 * knee_stretch + .034 * shin_stretch)
transmission *= .30 + .70 * (1 - np.clip(side, 0, 1) ** 2)
# Fine staggered knit loops share the existing UV density of all four panels.
# Their normal/roughness contribution filters away naturally in distant mips.
y, x = np.mgrid[:size, :size].astype(np.float32)
course = 2 * np.pi * y / 8
wale = 2 * np.pi * x / 6 + .45 * np.sin(course)
knit = .65 * np.cos(wale) * np.cos(course) + .35 * np.sin(2 * course)
skin = np.array([202, 173, 160], dtype=np.float32) / 255
skin = np.where(skin <= .04045, skin / 12.92, ((skin + .055) / 1.055) ** 2.4)
for level, ratio in [('Original', 1.), ('Half', .9), ('Quarter', .8), ('Eighth', .65), ('Sixteenth', .5)]:
    original = Image.open(OUTPUT / f'StyleBlack_{level}_Cloth.png').convert('RGBA')
    rgba = np.array(original)
    srgb = rgba[..., :3].astype(np.float32) / 255
    base = np.where(srgb <= .04045, srgb / 12.92, ((srgb + .055) / 1.055) ** 2.4)
    thin = base * .72 * (1 - transmission[..., None]) + skin * ratio * transmission[..., None]
    thin *= (1 + .012 * knit)[..., None]
    result = base * (1 - mask[..., None]) + thin * mask[..., None]
    encoded = np.where(result <= .0031308, result * 12.92, 1.055 * result ** (1 / 2.4) - .055)
    rgba[..., :3] = np.rint(encoded * 255).clip(0, 255).astype('uint8')
    Image.fromarray(rgba).save(OUTPUT / f'StyleSheer_{level}_Cloth.png')
arm = np.array(Image.open(OUTPUT / 'StyleBlack_Cloth_ARM.png').convert('RGB'))
roughness = np.full_like(mask, .96)
arm[..., 1] = np.rint(arm[..., 1] * (1 - mask) + roughness * 255 * mask).clip(0, 255).astype('uint8')
Image.fromarray(arm).save(OUTPUT / 'StyleSheer_Cloth_ARM.png')
original_normal = np.array(Image.open(OUTPUT / 'StyleBlack_Cloth_Normal.png').convert('RGB'))
normal = original_normal.astype(np.float32) / 255 * 2 - 1
slope_y, slope_x = np.gradient(knit)
cloth_xy = normal[..., :2] * .7 - .10 * np.stack([slope_x, slope_y], axis=-1)
normal[..., :2] = normal[..., :2] * (1 - mask[..., None]) + cloth_xy * mask[..., None]
normal[..., 2] = np.sqrt(np.maximum(1 - np.sum(normal[..., :2] ** 2, axis=-1), .001))
encoded_normal = np.rint((normal * .5 + .5) * 255).clip(0, 255).astype('uint8')
encoded_normal[mask == 0] = original_normal[mask == 0]
Image.fromarray(encoded_normal).save(OUTPUT / 'StyleSheer_Cloth_Normal.png')
mask_image.save(OUTPUT / 'StyleSheer_Coverage.png')
for name, value in [('Smooth', .04), ('Matte', .96)]:
    probe = arm.copy()
    probe[..., 1] = np.rint(arm[..., 1] * (1 - mask) + value * 255 * mask).astype('uint8')
    Image.fromarray(probe).save(OUTPUT / f'StockingProbe_{name}_ARM.png')

for name in ['Flat', 'Ridges']:
    probe_normal = np.zeros((size, size, 3), dtype=np.float32)
    if name == 'Ridges':
        # Deliberately coarse waves remain visible at the usual gameplay distance.
        probe_normal[..., 0] = .65 * np.sin(2 * np.pi * x / 56)
        probe_normal[..., 1] = .40 * np.sin(2 * np.pi * y / 80)
    probe_normal[..., 2] = np.sqrt(1 - np.sum(probe_normal[..., :2] ** 2, axis=-1))
    probe_normal = (probe_normal * .5 + .5) * 255
    probe = np.rint(encoded_normal * (1 - mask[..., None]) + probe_normal * mask[..., None]).astype('uint8')
    Image.fromarray(probe).save(OUTPUT / f'StockingProbe_{name}_Normal.png')
print('Matte hosiery with continuous leg-shaped density and skin tint packed')
