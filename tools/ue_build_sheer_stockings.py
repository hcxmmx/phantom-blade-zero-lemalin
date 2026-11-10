"""Add the thin-stocking comparison using existing black-cloth shader instances."""
from pathlib import Path
import json
import os
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parents[1]
ASSETS = '/Game/LeMalin'
tools = u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG 0')
added = []

def texture(name, file, color, normal=False):
    task = u.AssetImportTask()
    task.factory = u.TextureFactory()
    task.filename = str(ROOT / 'work/textures' / file)
    task.destination_path = ASSETS + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tools.import_asset_tasks([task])
    asset = u.load_asset(ASSETS + '/Textures/' + name)
    assert asset
    asset.set_editor_property('srgb', color)
    asset.set_editor_property('virtual_texture_streaming', True)
    compression = u.TextureCompressionSettings.TC_DEFAULT if color else u.TextureCompressionSettings.TC_MASKS
    if normal:
        compression = u.TextureCompressionSettings.TC_NORMALMAP
    asset.set_editor_property('compression_settings', compression)
    u.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    added.append(f'Content/LeMalin/Textures/{name}.uasset')
    return asset

def material_variant(name, source, overrides):
    material = u.load_asset(ASSETS + '/Materials/' + name)
    if not material:
        material = tools.duplicate_asset(name, ASSETS + '/Materials', source)
    assert material
    values = []
    for original in source.get_editor_property('texture_parameter_values'):
        info = original.get_editor_property('parameter_info')
        value = u.TextureParameterValue()
        value.set_editor_property('parameter_info', info)
        target = original.get_editor_property('parameter_value')
        if info.association == u.MaterialParameterAssociation.LAYER_PARAMETER and info.index == 0:
            target = overrides.get(str(info.name), target)
        value.set_editor_property('parameter_value', target)
        values.append(value)
    material.set_editor_property('texture_parameter_values', values)
    bindings = {str(value.parameter_info.name): value.parameter_value
                for value in material.get_editor_property('texture_parameter_values')
                if value.parameter_info.association == u.MaterialParameterAssociation.LAYER_PARAMETER
                and value.parameter_info.index == 0}
    assert all(bindings[key] == value for key, value in overrides.items())
    u.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    added.append(f'Content/LeMalin/Materials/{name}.uasset')
    return material


arm = texture('T_StyleSheer_Cloth_ARM', 'StyleSheer_Cloth_ARM.png', False)
normal = texture('T_StyleSheer_Cloth_Normal', 'StyleSheer_Cloth_Normal.png', False, normal=True)
probes = {}
for mode, parameter, kind in [('Smooth', 'Mix', 'ARM'), ('Matte', 'Mix', 'ARM'),
                              ('Flat', 'Normal', 'Normal'), ('Ridges', 'Normal', 'Normal')]:
    name = f'StockingProbe_{mode}_{kind}'
    probes[mode] = {parameter: texture('T_' + name, name + '.png', False, normal=kind == 'Normal')}
for level in ['Original', 'Half', 'Quarter', 'Eighth', 'Sixteenth']:
    suffix = level + '_Cloth'
    albedo = texture('T_StyleSheer_' + suffix, 'StyleSheer_' + suffix + '.png', True)
    source = u.load_asset(ASSETS + '/Materials/MI_StyleBlack_' + suffix)
    assert source
    baseline = material_variant('MI_StyleSheer_' + suffix, source,
                                {'Albedo': albedo, 'Mix': arm, 'Normal': normal})
    for mode, overrides in probes.items():
        targets = dict(overrides)
        if mode == 'Matte' and os.environ.get('LEMALIN_PLAYER_RELEASE') != '1':
            targets['Albedo'] = texture('T_StockingProbe_Previous_' + suffix,
                                        '../stockings-dev22/StyleSheer_' + suffix + '.png', True)
        material_variant('MI_StockingProbe_' + mode + '_' + suffix, baseline, targets)

allowlist = ROOT / 'metadata/cook-allowlist.json'
allowlist.write_text(json.dumps(sorted(set(json.loads(allowlist.read_text()) + added)), indent=2))
if os.environ.get('LEMALIN_MATERIALS_ONLY') == '1':
    pass
elif os.environ.get('LEMALIN_TEXTURES_ONLY') == '1':
    widget = u.load_asset(ASSETS + '/WBP_LeMalinTuning')
    title = u.find_object(widget, 'WidgetTree.Title')
    assert isinstance(title, u.TextBlock)
    title.set_text(u.Text('LeMalin 0.2.0-dev.23 / F8'))
    caption = u.find_object(widget, 'WidgetTree.OutfitStyleCaption')
    hint = u.find_object(widget, 'WidgetTree.Hint')
    assert isinstance(caption, u.TextBlock) and isinstance(hint, u.TextBlock)
    caption.set_text(u.Text('服饰（0 白 / 1 黑 / 2 哑光薄袜 / 3 镜亮 / 4 上版全哑 / 5 平滑 / 6 波纹）'))
    hint.set_text(u.Text('底色建议 2 档。服饰 2 为新版哑光薄袜，4 为上版全哑配色，可实时对照。3/5/6 为材质诊断。'))
    u.BlueprintEditorLibrary.compile_blueprint(widget)
    u.EditorAssetLibrary.save_loaded_asset(widget, only_if_is_dirty=False)
else:
    for filename in ['ue_build_lively.py', 'ue_probe_material_controls.py']:
        script = ROOT / 'tools' / filename
        exec(compile(script.read_text(encoding='utf-8'), str(script), 'exec'), {})
u.log('LEMALIN_SHEER_STOCKINGS_READY')
