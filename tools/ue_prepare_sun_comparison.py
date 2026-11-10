"""Duplicate accepted MIs, preserving parent, normal/ARM and static shader state."""
from pathlib import Path
import json,unreal as u
R=Path(u.Paths.project_dir()).resolve().parents[1];D='/Game/LeMalin';tools=u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.PNG 0')
added=[]
for part in ['Skin','Face','Hair','Cloth','Obj']:
 base=u.load_asset(D+'/Materials/MI_'+part);assert base
 for mode in ['Half','Quarter','Eighth','Sixteenth','Black']:
  name='MI_Sun'+mode+'_'+part
  mat=u.load_asset(D+'/Materials/'+name) or tools.duplicate_asset(name,D+'/Materials',base);assert mat
  mat.set_editor_property('texture_parameter_values',base.get_editor_property('texture_parameter_values'))
  mat.set_editor_property('scalar_parameter_values',base.get_editor_property('scalar_parameter_values'))
  if mode in ['NoSpec','Black']:
   values=list(mat.get_editor_property('scalar_parameter_values'))
   for param,layer in [('Specular',True),('Wetness Specular',False),('RainDropSpecular',False)]:
    info=u.MaterialParameterInfo(name=param,association=u.MaterialParameterAssociation.LAYER_PARAMETER if layer else u.MaterialParameterAssociation.GLOBAL_PARAMETER,index=0 if layer else -1)
    value=u.ScalarParameterValue();value.set_editor_property('parameter_info',info);value.set_editor_property('parameter_value',0.)
    values=[v for v in values if v.get_editor_property('parameter_info')!=info]+[value]
   mat.set_editor_property('scalar_parameter_values',values)
  if mode!='NoSpec':
   texname='T_Sun'+mode+'_'+part
   tex=u.load_asset(D+'/Textures/'+texname)
   if not tex:
    task=u.AssetImportTask();task.factory=u.TextureFactory();task.filename=str(R/'work/textures'/('Sun'+mode+'_'+part+'.png'));task.destination_path=D+'/Textures';task.destination_name=texname;task.automated=True;task.replace_existing=True;task.save=False
    tools.import_asset_tasks([task]);tex=u.load_asset(D+'/Textures/'+texname);assert tex
    tex.set_editor_property('srgb',True);tex.set_editor_property('virtual_texture_streaming',True);u.EditorAssetLibrary.save_loaded_asset(tex,only_if_is_dirty=False)
   values=mat.get_editor_property('texture_parameter_values')
   for i,v in enumerate(values):
    info=v.get_editor_property('parameter_info')
    if str(info.name)=='Albedo' and info.association==u.MaterialParameterAssociation.LAYER_PARAMETER and info.index==0:v.set_editor_property('parameter_value',tex);values[i]=v
   mat.set_editor_property('texture_parameter_values',values)
   added.append('Content/LeMalin/Textures/'+texname+'.uasset')
  u.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False);added.append('Content/LeMalin/Materials/'+name+'.uasset')
p=R/'metadata/cook-allowlist.json';p.write_text(json.dumps(sorted({a for a in json.loads(p.read_text())+added if '/Materials/MI_SunNoSpec_' not in a}),indent=2))
u.log('LEMALIN_SUN_COMPARISONS_READY')
