"""Create isolated runtime retarget graph and material instances of game-owned shaders."""
from pathlib import Path
import json,unreal as u
R=Path(u.Paths.project_dir()).resolve().parents[1];D='/Game/LeMalin';B='/HuaShira/Characters/Hero/Hero_Hun';P=R/'project/ProjectPBZ'
tools=u.AssetToolsHelpers.get_asset_tools();mesh=u.load_asset(D+'/SK_LeMalin');rtg=u.load_asset(D+'/RTG_Hun_LeMalin');assert mesh and rtg
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/HuaShira','/Game/PBZContent'],True)
template=u.load_asset(D+'/ABPT_LeMalin')
if not template:
 f=u.AnimBlueprintFactory();f.set_editor_property('template',True);f.set_editor_property('parent_class',u.AnimInstance)
 template=tools.create_asset('ABPT_LeMalin',D,u.AnimBlueprint,f);assert u.HcxRetargetTools.build_retarget_graph(template,rtg)
u.EditorAssetLibrary.save_loaded_asset(template,only_if_is_dirty=False)
bp=u.load_asset(D+'/ABP_LeMalin')
if not bp:
 f=u.AnimBlueprintFactory();f.set_editor_property('parent_class',u.EditorAssetLibrary.load_blueprint_class(D+'/ABPT_LeMalin'));f.set_editor_property('target_skeleton',mesh.skeleton)
 bp=tools.create_asset('ABP_LeMalin',D,u.AnimBlueprint,f)
u.BlueprintEditorLibrary.compile_blueprint(bp);u.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
cls=u.EditorAssetLibrary.load_blueprint_class(D+'/ABP_LeMalin');assert cls
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.PNG 0')
parent_path='/Game/PBZContent/Characters/Hero/Hun_New_V2b/Material/MI_ZeroHun_Cloth_03_b'
parent=u.load_asset(parent_path);assert parent
def texture(name,file,kind):
 task=u.AssetImportTask();task.factory=u.TextureFactory();task.filename=str(file);task.destination_path=D+'/Textures';task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True
 tools.import_asset_tasks([task]);tex=u.load_asset(D+'/Textures/'+name);assert tex
 tex.set_editor_property('srgb',kind=='color');tex.set_editor_property('virtual_texture_streaming',True)
 if kind=='normal':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP)
 u.EditorAssetLibrary.save_loaded_asset(tex,only_if_is_dirty=False);return tex
normal=texture('T_FlatNormal',R/'work/textures/flat_normal.png','normal');arm=texture('T_NeutralARM',R/'work/textures/neutral_ARM.png','arm')
files={'Hair':'TEX_LML_Hair.png','Skin':'TEX_LML_Skin.png','Cloth':'TEX_LML_Cloth.png','Face':'TEX_LML_Face.png','Eye':'TEX_LML_EyeA.png','Obj':'TEX_LML_Obj.png','Facial':'TEX_LML_Eff_Facial.png','Bickle':'TEX_LML_Eff_Bickle.png','Tear':'TEX_LML_Eff_Tear.png'}
materials={}
for key,file in files.items():
 tex=texture('T_'+key,R/'source/tex'/file,'color')
 mat=u.load_asset(D+'/Materials/MI_'+key) or tools.create_asset('MI_'+key,D+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
 u.MaterialEditingLibrary.set_material_instance_parent(mat,parent)
 values=[]
 for param,asset in [('Albedo',tex),('Normal',normal),('Mix',arm)]:
  info=u.MaterialParameterInfo(name=param,association=u.MaterialParameterAssociation.LAYER_PARAMETER,index=0)
  value=u.TextureParameterValue();value.set_editor_property('parameter_info',info);value.set_editor_property('parameter_value',asset);values.append(value)
 mat.set_editor_property('texture_parameter_values',values)
 u.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False);materials[key]=mat
mapping={'Hair':'Hair','HairFront':'Hair','Body':'Skin','Cloth':'Cloth','Face':'Face','Eye':'Eye','EyeLight':'Eye','Belt':'Obj','Obj':'Obj','RabbitEar':'Obj','Shoes':'Obj','Eff_Aozame':'Facial','Eff_Default':'Facial','Eff_Pink':'Facial','Mat_Eff_Angry':'Facial','Mat_Eff_Bickle':'Bickle','Mat_Eff_Sweat':'Tear'}
slots=mesh.get_editor_property('materials');report=[]
for i,slot in enumerate(slots):
 name=str(slot.material_slot_name).removeprefix('Preview_');assert name in mapping,name
 slot.material_interface=materials[mapping[name]];slots[i]=slot;report.append({'slot':name,'material':slot.material_interface.get_path_name()})
mesh.set_editor_property('materials',slots);u.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
path=B+'/SkinParts/Hun_Hair_Band'
if u.EditorAssetLibrary.does_asset_exist(path):assert u.EditorAssetLibrary.delete_asset(path)
visible=tools.duplicate_asset('Hun_Hair_Band',B+'/SkinParts',mesh);assert visible
visible.set_editor_property('post_process_anim_blueprint',cls);u.EditorAssetLibrary.save_loaded_asset(visible,only_if_is_dirty=False)
body=u.load_asset(B+'/SkinParts/Hun_Body_Simple');assert u.HcxRetargetTools.restore_physics_reference(body,B+'/PA_Hun')
u.EditorAssetLibrary.save_loaded_asset(body,only_if_is_dirty=False);u.EditorAssetLibrary.save_loaded_asset(u.load_asset(B+'/PA_Hun'),only_if_is_dirty=False)
allow=['Content/LeMalin/'+n+'.uasset' for n in ['ABPT_LeMalin','ABP_LeMalin','IK_Hun','IK_LeMalin','RTG_Hun_LeMalin','SK_LeMalin','SK_LeMalin_Skeleton']]
for folder in ['Materials','Textures']:
 allow += [p.relative_to(P).as_posix() for p in (P/'Content/LeMalin'/folder).glob('*.uasset')]
for folder in ['SkinParts','SkinParts/Cloth','Hats']:
 allow += [p.relative_to(P).as_posix() for p in (P/'Plugins/HuaShira/Content/Characters/Hero/Hero_Hun'/folder).glob('*.uasset')]
assert not any('PA_Hun.' in a or '/Garfield/' in a or '/Hcx2B/' in a or '/PBZContent/' in a for a in allow)
(R/'metadata').mkdir(exist_ok=True);(R/'metadata/cook-allowlist.json').write_text(json.dumps(sorted(allow),indent=2))
(R/'output/reports/game-setup.json').write_text(json.dumps({'display':path,'post_process':cls.get_path_name(),'physics_reference':body.get_editor_property('physics_asset').get_path_name(),'materials':report,'material_parent':parent_path,'texture_parameters':'LayerParameter index 0: Albedo, Normal, Mix','asset_count':len(allow),'game_validation':'pending'},indent=2))
u.log('LEMALIN_GAME_READY')
