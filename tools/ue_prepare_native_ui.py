"""Editor previews at game-owned paths. PBZContent remains excluded from the mod."""
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve().parents[1] / 'work/native-ui/assets/ProjectPBZ/Content'
tools = u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG 0')
for source in root.rglob('*.png'):
    relative = source.relative_to(root).with_suffix('')
    path = '/Game/' + relative.as_posix()
    if u.load_asset(path):
        continue
    task = u.AssetImportTask()
    task.filename = str(source)
    task.destination_path = '/Game/' + relative.parent.as_posix()
    task.destination_name = relative.name
    task.factory = u.TextureFactory()
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    texture = u.load_asset(path)
    assert texture, path
    texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
    u.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

# A lightweight editor preview occupies the original material path. At runtime,
# the game's own material and compiled shaders resolve through this reference.
parent_path = '/Game/PBZContent/UI/Menu/Art/M_UI_Base_BordersAndButtons'
parent = u.load_asset(parent_path)
if not parent:
    parent = tools.create_asset('M_UI_Base_BordersAndButtons', '/Game/PBZContent/UI/Menu/Art', u.Material, u.MaterialFactoryNew())
    parent.set_editor_property('material_domain', u.MaterialDomain.MD_UI)
    parent.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
    color = u.MaterialEditingLibrary.create_material_expression(parent, u.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant', u.LinearColor(0, 0, 0, 1))
    opacity = u.MaterialEditingLibrary.create_material_expression(parent, u.MaterialExpressionConstant)
    opacity.set_editor_property('r', .0625)
    u.MaterialEditingLibrary.connect_material_property(color, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    u.MaterialEditingLibrary.connect_material_property(opacity, '', u.MaterialProperty.MP_OPACITY)
    u.MaterialEditingLibrary.recompile_material(parent)
    u.EditorAssetLibrary.save_loaded_asset(parent, only_if_is_dirty=False)
path = '/Game/PBZContent/UI/Menu/Art/MI_UI_Settings_EntryBorder'
if not u.load_asset(path):
    material = tools.create_asset('MI_UI_Settings_EntryBorder', '/Game/PBZContent/UI/Menu/Art', u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    u.MaterialEditingLibrary.set_material_instance_parent(material, parent)
    u.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
u.log('LEMALIN_NATIVE_UI_PREVIEW_ASSETS_READY')
