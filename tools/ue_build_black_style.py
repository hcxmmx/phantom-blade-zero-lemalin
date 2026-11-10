import os
"""Create native game material variants for the selectable black outfit."""
from pathlib import Path
import json
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parents[1]
ASSETS = '/Game/LeMalin'
tools = u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG 0')
added = []
surface = {}
for part, kind in [('Cloth', 'ARM'), ('Obj', 'ARM'), ('Cloth', 'Normal')]:
    name = f'T_StyleBlack_{part}_{kind}'
    task = u.AssetImportTask()
    task.factory = u.TextureFactory()
    task.filename = str(ROOT / 'work/textures' / f'StyleBlack_{part}_{kind}.png')
    task.destination_path = ASSETS + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tools.import_asset_tasks([task])
    texture = u.load_asset(ASSETS + '/Textures/' + name)
    assert texture
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('virtual_texture_streaming', True)
    texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP if kind == 'Normal' else u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    surface[(part, kind)] = texture
    added.append(f'Content/LeMalin/Textures/{name}.uasset')
for part in ['Cloth', 'Obj']:
    for level in ['Original', 'Half', 'Quarter', 'Eighth', 'Sixteenth']:
        suffix = f'{level}_{part}'
        task = u.AssetImportTask()
        task.factory = u.TextureFactory()
        task.filename = str(ROOT / 'work/textures' / f'StyleBlack_{suffix}.png')
        task.destination_path = ASSETS + '/Textures'
        task.destination_name = 'T_StyleBlack_' + suffix
        task.automated = True
        task.replace_existing = True
        task.save = False
        tools.import_asset_tasks([task])
        texture = u.load_asset(ASSETS + '/Textures/' + task.destination_name)
        assert texture
        texture.set_editor_property('srgb', True)
        texture.set_editor_property('virtual_texture_streaming', True)
        u.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

        source_name = 'MI_' + part if level == 'Original' else f'MI_Sun{level}_{part}'
        source = u.load_asset(ASSETS + '/Materials/' + source_name)
        assert source
        name = 'MI_StyleBlack_' + suffix
        material = u.load_asset(ASSETS + '/Materials/' + name)
        if not material:
            material = tools.duplicate_asset(name, ASSETS + '/Materials', source)
        assert material
        values = []
        found_albedo = False
        for original in source.get_editor_property('texture_parameter_values'):
            info = original.get_editor_property('parameter_info')
            value = u.TextureParameterValue()
            value.set_editor_property('parameter_info', info)
            target = original.get_editor_property('parameter_value')
            if (str(info.name) == 'Albedo'
                    and info.association == u.MaterialParameterAssociation.LAYER_PARAMETER
                    and info.index == 0):
                target = texture
                found_albedo = True
            value.set_editor_property('parameter_value', target)
            values.append(value)
        assert found_albedo
        for value in values:
            info = value.get_editor_property('parameter_info')
            if info.association == u.MaterialParameterAssociation.LAYER_PARAMETER and info.index == 0:
                if str(info.name) == 'Mix':
                    value.set_editor_property('parameter_value', surface[(part, 'ARM')])
                elif str(info.name) == 'Normal' and part == 'Cloth':
                    value.set_editor_property('parameter_value', surface[(part, 'Normal')])
        material.set_editor_property('texture_parameter_values', values)
        bindings = {str(value.get_editor_property('parameter_info').name): value.get_editor_property('parameter_value') for value in material.get_editor_property('texture_parameter_values')}
        assert bindings['Mix'] == surface[(part, 'ARM')]
        if part == 'Cloth':
            assert bindings['Normal'] == surface[(part, 'Normal')]
        scalars = list(source.get_editor_property('scalar_parameter_values'))
        for parameter, number in [('Roughness Min', 0.), ('Roughness Max', 1.), ('Roughness Exponent', 1.), ('Specular', .5)]:
            info = u.MaterialParameterInfo(name=parameter, association=u.MaterialParameterAssociation.LAYER_PARAMETER, index=0)
            value = u.ScalarParameterValue()
            value.set_editor_property('parameter_info', info)
            value.set_editor_property('parameter_value', number)
            scalars = [old for old in scalars if old.get_editor_property('parameter_info') != info] + [value]
        material.set_editor_property('scalar_parameter_values', scalars)
        u.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
        added += [f'Content/LeMalin/Textures/{task.destination_name}.uasset',
                  f'Content/LeMalin/Materials/{name}.uasset']

allowlist = ROOT / 'metadata/cook-allowlist.json'
allowlist.write_text(json.dumps(sorted(set(json.loads(allowlist.read_text()) + added)), indent=2))
if os.environ.get('LEMALIN_MATERIALS_ONLY') != '1':
    script = ROOT / 'tools/ue_build_lively.py'
    exec(compile(script.read_text(encoding='utf-8'), str(script), 'exec'), {})
    script = ROOT / 'tools/ue_probe_material_controls.py'
    exec(compile(script.read_text(encoding='utf-8'), str(script), 'exec'), {})
u.log('LEMALIN_BLACK_STYLE_READY')
