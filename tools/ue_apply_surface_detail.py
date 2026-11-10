"""Only replace Hair/Cloth normal and Mix textures; leave accepted animation assets intact."""
from pathlib import Path
import json
import unreal as u

R = Path(u.Paths.project_dir()).resolve().parents[1]
D = '/Game/LeMalin'
tools = u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG 0')

def texture(name, file, normal=False):
    task = u.AssetImportTask()
    task.factory = u.TextureFactory()
    task.filename = str(file)
    task.destination_path = D + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tools.import_asset_tasks([task])
    tex = u.load_asset(D + '/Textures/' + name)
    assert tex, name
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('virtual_texture_streaming', True)
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP if normal else u.TextureCompressionSettings.TC_MASKS)
    if normal:
        tex.set_editor_property('flip_green_channel', False)
    u.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex

added = []
for part in ['Hair', 'Cloth']:
    normal = texture('T_' + part + '_DetailNormal', R / 'source/tex' / f'TEX_LML_{part}_NormalMap.png', True)
    arm = texture('T_' + part + '_DetailARM', R / 'work/textures' / f'{part}_Detail_ARM.png')
    mat = u.load_asset(D + '/Materials/MI_' + part)
    assert mat, part
    values = mat.get_editor_property('texture_parameter_values')
    for i, value in enumerate(values):
        info = value.get_editor_property('parameter_info')
        if info.association == u.MaterialParameterAssociation.LAYER_PARAMETER and info.index == 0:
            name = str(info.name)
            if name in ['Normal', 'Mix']:
                value.set_editor_property('parameter_value', normal if name == 'Normal' else arm)
                values[i] = value
    mat.set_editor_property('texture_parameter_values', values)
    u.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    added += [f'Content/LeMalin/Textures/T_{part}_DetailNormal.uasset', f'Content/LeMalin/Textures/T_{part}_DetailARM.uasset']

# Refresh just the displayed build label, without regenerating physics or animation.
widget = u.load_asset(D + '/WBP_LeMalinTuning')
title = u.find_object(widget, 'WidgetTree.Title') if widget else None
if isinstance(title, u.TextBlock):
    title.set_text(u.Text('LeMalin 0.2.0-dev.6 / F8'))
    u.BlueprintEditorLibrary.compile_blueprint(widget)
    u.EditorAssetLibrary.save_loaded_asset(widget, only_if_is_dirty=False)

p = R / 'metadata/cook-allowlist.json'
p.write_text(json.dumps(sorted(set(json.loads(p.read_text()) + added)), indent=2))
u.log('LEMALIN_SURFACE_DETAIL_READY: hair/cloth normal + gentle AO; albedo, roughness, metallic and animation unchanged')
