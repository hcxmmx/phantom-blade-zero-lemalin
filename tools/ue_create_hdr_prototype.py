"""A single masked HDR pass, loaded by soft reference only when explicitly enabled."""
from pathlib import Path
import json,unreal as u
R=Path(u.Paths.project_dir()).resolve().parents[1];D='/Game/LeMalin/Materials';name='M_LeMalinHDR_Prototype'
tools=u.AssetToolsHelpers.get_asset_tools()
mat=u.load_asset(D+'/'+name)
if not mat:mat=tools.create_asset(name,D,u.Material,u.MaterialFactoryNew())
assert mat
u.MaterialEditingLibrary.delete_all_material_expressions(mat)
mat.set_editor_property('material_domain',u.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property('blendable_location',u.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM)
mat.set_editor_property('blendable_priority',1000)
# Raw pre-exposed input/output: the outside-mask path remains an exact pass-through.
mat.set_editor_property('disable_pre_exposure_scale',True)
nodes={}
for label,kind in [('Source',u.SceneTextureId.PPI_POST_PROCESS_INPUT0),('Stencil',u.SceneTextureId.PPI_CUSTOM_STENCIL),('SceneZ',u.SceneTextureId.PPI_SCENE_DEPTH),('CustomZ',u.SceneTextureId.PPI_CUSTOM_DEPTH)]:
    node=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionSceneTexture)
    node.set_editor_property('scene_texture_id',kind);nodes[label]=node
for label,default in [('Mode',0.),('Knee',1.),('RollOff',.25)]:
    node=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionScalarParameter)
    node.set_editor_property('parameter_name',label);node.set_editor_property('default_value',default);nodes[label]=node
custom=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionCustom)
custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('description','LeMalin visible pixels only; preserve low luminance; logarithmic HDR shoulder')
inputs=[]
for label in nodes:
    item=u.CustomInput();item.set_editor_property('input_name',label);inputs.append(item)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('code',r'''
float3 c = Source.rgb;
float match = 1.0 - step(0.5, abs(Stencil.r - 231.0));
float visible = step(CustomZ.r, SceneZ.r + 1.0);
if (Mode < 0.5 || match * visible < 0.5) return c;
if (Mode < 1.5) return float3(0.015, 0.35, 0.025);
float y = max(dot(c, float3(0.2126, 0.7152, 0.0722)), 0.00001);
float t = max(Knee, 0.001);
float r = max(RollOff, 0.001);
float outY = y <= t ? y : t + r * log(1.0 + (y - t) / r);
return c * (outY / y);
''')
for label,node in nodes.items():
    assert u.MaterialEditingLibrary.connect_material_expressions(node,'Color' if label in ['Source','Stencil','SceneZ','CustomZ'] else '',custom,label),label
assert u.MaterialEditingLibrary.connect_material_property(custom,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
u.MaterialEditingLibrary.recompile_material(mat)
u.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
p=R/'metadata/cook-allowlist.json';allow=json.loads(p.read_text());allow.append('Content/LeMalin/Materials/'+name+'.uasset');p.write_text(json.dumps(sorted(set(allow)),indent=2))
u.log('LEMALIN_HDR_MATERIAL_READY: stencil 231, depth visibility, raw HDR before bloom')
