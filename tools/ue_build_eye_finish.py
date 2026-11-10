"""Prepare eye-only instances of the game shader and rebuild the tuning controls."""
from pathlib import Path
import json
import os
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parents[1]
ASSETS = '/Game/LeMalin'
tools = u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG 0')
base = u.load_asset(ASSETS + '/Materials/MI_Eye')
assert base, 'Original eye material missing'
added = []

def import_texture(texture_name, file, color=False):
    task = u.AssetImportTask()
    task.factory = u.TextureFactory()
    task.filename = str(file)
    task.destination_path = ASSETS + '/Textures'
    task.destination_name = texture_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tools.import_asset_tasks([task])
    texture = u.load_asset(ASSETS + '/Textures/' + texture_name)
    assert texture, texture_name
    texture.set_editor_property('srgb', color)
    texture.set_editor_property('virtual_texture_streaming', True)
    texture.set_editor_property('compression_settings',
        u.TextureCompressionSettings.TC_DEFAULT if color else u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    added.append(f'Content/LeMalin/Textures/{texture_name}.uasset')
    return texture

for name in ['Gloss', 'Crystal', 'Gem']:
    albedo = None
    if name == 'Gem':
        texture = u.load_asset(ASSETS + '/Textures/T_Eye_Crystal_ARM')
        assert texture, 'Crystal Mix texture missing'
        albedo = import_texture('T_Eye_Gem_Albedo', ROOT / 'work/textures/Eye_Gem_Albedo.png', True)
    else:
        texture = import_texture(f'T_Eye_{name}_ARM', ROOT / 'work/textures' / f'Eye_{name}_ARM.png')

    material_name = f'MI_Eye_{name}'
    material = u.load_asset(ASSETS + '/Materials/' + material_name)
    if not material:
        material = tools.duplicate_asset(material_name, ASSETS + '/Materials', base)
    assert material, material_name
    values = []
    found_mix = False
    for original in base.get_editor_property('texture_parameter_values'):
        info = original.get_editor_property('parameter_info')
        value = u.TextureParameterValue()
        value.set_editor_property('parameter_info', info)
        parameter_texture = original.get_editor_property('parameter_value')
        if (str(info.name) == 'Mix'
                and info.association == u.MaterialParameterAssociation.LAYER_PARAMETER
                and info.index == 0):
            assert parameter_texture.get_name() == 'T_NeutralARM', 'Original eye Mix has changed'
            parameter_texture = texture
            found_mix = True
        if (albedo and str(info.name) == 'Albedo'
                and info.association == u.MaterialParameterAssociation.LAYER_PARAMETER
                and info.index == 0):
            parameter_texture = albedo
        value.set_editor_property('parameter_value', parameter_texture)
        values.append(value)
    assert found_mix, 'Original eye Mix binding missing'
    material.set_editor_property('texture_parameter_values', values)

    scalars = list(base.get_editor_property('scalar_parameter_values'))
    for parameter, number in [('Roughness Min', 0.), ('Roughness Max', 1.),
                              ('Roughness Exponent', 1.), ('Specular', .5)]:
        info = u.MaterialParameterInfo(name=parameter,
            association=u.MaterialParameterAssociation.LAYER_PARAMETER, index=0)
        value = u.ScalarParameterValue()
        value.set_editor_property('parameter_info', info)
        value.set_editor_property('parameter_value', number)
        scalars = [old for old in scalars if old.get_editor_property('parameter_info') != info]
        scalars.append(value)
    material.set_editor_property('scalar_parameter_values', scalars)
    u.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    added.append(f'Content/LeMalin/Materials/{material_name}.uasset')

allowlist = ROOT / 'metadata/cook-allowlist.json'
allowlist.write_text(json.dumps(sorted(set(json.loads(allowlist.read_text()) + added)), indent=2))
if os.environ.get('LEMALIN_MATERIALS_ONLY') != '1':
    script = ROOT / 'tools/ue_build_lively.py'
    exec(compile(script.read_text(encoding='utf-8'), str(script), 'exec'), {})
    if os.environ.get('LEMALIN_PROBE_EYES') == '1':
        script = ROOT / 'tools/ue_probe_material_controls.py'
        exec(compile(script.read_text(encoding='utf-8'), str(script), 'exec'), {})
u.log('LEMALIN_EYE_FINISH_READY: original / gloss / crystal / gem')
