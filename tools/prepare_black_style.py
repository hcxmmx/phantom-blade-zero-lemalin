"""Pack black-only colors and geometry-derived material-region maps."""
from pathlib import Path
from collections import defaultdict
import importlib.util
from PIL import Image, ImageDraw, ImageFilter
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'work/textures'
parser = Path.home() / 'AppData/Roaming/Blender Foundation/Blender/5.2/extensions/blender_org/mmd_tools/core/pmx/__init__.py'
spec = importlib.util.spec_from_file_location('pmx_black_regions', parser)
pmx = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pmx)
model = pmx.load(str(ROOT / 'source/LeMaline v1.0.pmx'))
size = 2048
masks = {name: Image.new('L', (size, size)) for name in ['Metal', 'Stockings', 'Gloves', 'Shoes']}
draw = {name: ImageDraw.Draw(mask) for name, mask in masks.items()}
start = 0
for material in model.materials:
    faces = model.faces[start:start + material.vertex_count // 3]
    start += material.vertex_count // 3
    if material.name not in ['Cloth', 'Obj', 'Belt', 'RabbitEar', 'Shoes']:
        continue
    if material.name == 'Cloth':
        for face in faces:
            vertices = [model.vertices[i] for i in face]
            center = np.mean([v.co for v in vertices], axis=0)
            region = 'Stockings' if center[1] < 10 else 'Gloves' if abs(center[0]) > 2 else None
            if region:
                draw[region].polygon([(v.uv[0] * size, v.uv[1] * size) for v in vertices], fill=255)
        continue
    if material.name == 'Shoes':
        for face in faces:
            draw['Shoes'].polygon([(model.vertices[i].uv[0] * size, model.vertices[i].uv[1] * size) for i in face], fill=255)
    # Select complete hardware islands. Wide cloth cuffs cross outside this block.
    parents = {i: i for face in faces for i in face}
    def root(i):
        while parents[i] != i:
            parents[i] = parents[parents[i]]
            i = parents[i]
        return i
    for a, b, c in faces:
        parents[root(a)] = root(b)
        parents[root(c)] = root(b)
    groups = defaultdict(list)
    for i in parents:
        groups[root(i)].append(i)
    metal_roots = {key for key, indices in groups.items()
                   if min(model.vertices[i].uv[0] for i in indices) >= .64
                   and min(model.vertices[i].uv[1] for i in indices) >= .61}
    for face in faces:
        if root(face[0]) in metal_roots:
            draw['Metal'].polygon([(model.vertices[i].uv[0] * size, model.vertices[i].uv[1] * size) for i in face], fill=255)

masks = {name: mask.filter(ImageFilter.MaxFilter(9)) for name, mask in masks.items()}
srgb = np.arange(256, dtype=np.float64) / 255
linear = np.where(srgb <= .04045, srgb / 12.92, ((srgb + .055) / 1.055) ** 2.4)
for part in ['Cloth', 'Obj']:
    original = Image.open(ROOT / 'source/tex' / f'TEX_LML_{part}.png').convert('RGBA')
    rgb = np.asarray(original)[..., :3]
    # Retain the white artwork's fold gradients with a neutral charcoal tint.
    color = linear[rgb] * (.045 if part == 'Cloth' else .035)
    if part == 'Obj':
        metal = np.asarray(masks['Metal']) / 255
        color = color * (1 - metal[..., None]) + linear[rgb] * metal[..., None]
    for name, ratio in [('Original', 1.), ('Half', .9), ('Quarter', .8), ('Eighth', .65), ('Sixteenth', .5)]:
        level = color * ratio
        encoded = np.rint(np.where(level <= .0031308, level * 12.92, 1.055 * level ** (1 / 2.4) - .055) * 255).clip(0, 255).astype('uint8')
        packed = Image.fromarray(encoded).convert('RGBA')
        packed.putalpha(original.getchannel('A'))
        packed.save(OUTPUT / f'StyleBlack_{name}_{part}.png')
    roughness = np.full((size, size), .40 if part == 'Obj' else .34)
    metallic = np.zeros((size, size))
    if part == 'Cloth':
        roughness[np.asarray(masks['Stockings']) > 127] = .47
        roughness[np.asarray(masks['Gloves']) > 127] = .43
        ao = np.asarray(Image.open(OUTPUT / 'Cloth_Detail_ARM.png').resize((size, size)))[..., 0]
    else:
        roughness[np.asarray(masks['Shoes']) > 127] = .23
        roughness[np.asarray(masks['Metal']) > 127] = .21
        metallic[np.asarray(masks['Metal']) > 127] = .65
        ao = np.full((size, size), 255)
    Image.fromarray(np.stack([ao, np.rint(roughness * 255), np.rint(metallic * 255)], axis=-1).astype('uint8')).save(OUTPUT / f'StyleBlack_{part}_ARM.png')

normal = np.asarray(Image.open(ROOT / 'source/tex/TEX_LML_Cloth_NormalMap.png').convert('RGB'), dtype=np.float32) / 255
xy = (normal[..., :2] * 2 - 1) * 1.6
z = np.sqrt(np.maximum(1 - np.sum(xy * xy, axis=-1), .001))
vectors = np.dstack([xy, z])
vectors /= np.linalg.norm(vectors, axis=-1, keepdims=True)
Image.fromarray(np.rint((vectors * .5 + .5) * 255).clip(0, 255).astype('uint8')).save(OUTPUT / 'StyleBlack_Cloth_Normal.png')
print('Packed black fabric, silver hardware islands and regional surface maps')
