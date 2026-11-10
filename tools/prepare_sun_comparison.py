"""Pack real linear-light albedo levels, preserving original alpha and resolution."""
from pathlib import Path
from PIL import Image
import numpy as np
R=Path(__file__).resolve().parents[1]
O=R/'work/textures';O.mkdir(parents=True,exist_ok=True)
source={'Skin':'TEX_LML_Skin.png','Face':'TEX_LML_Face.png','Hair':'TEX_LML_Hair.png','Cloth':'TEX_LML_Cloth.png','Obj':'TEX_LML_Obj.png'}
s=np.arange(256,dtype=np.float64)/255
linear=np.where(s<=.04045,s/12.92,((s+.055)/1.055)**2.4)
for mode,ratio in [('Half',.5),('Quarter',.25),('Eighth',.125),('Sixteenth',.0625),('Black',0.)]:
 level=linear*ratio
 lut=np.rint(np.where(level<=.0031308,12.92*level,1.055*level**(1/2.4)-.055)*255).astype('uint8').tolist()
 for part,file in source.items():
  src=R/'source/tex'/file;out=O/f'Sun{mode}_{part}.png'
  if out.is_file() and out.stat().st_mtime>=src.stat().st_mtime:continue
  with Image.open(src) as image:
   red,green,blue,alpha=image.convert('RGBA').split()
   Image.merge('RGBA',(red.point(lut),green.point(lut),blue.point(lut),alpha)).save(out)
print('Prepared 50 / 25 / 12.5 / 6.25 percent linear albedo and black locator')
