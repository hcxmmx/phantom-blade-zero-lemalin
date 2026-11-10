"""Build an allowlisted IoStore test package; omit game shader and physics stubs."""
from pathlib import Path
import hashlib,json,subprocess,argparse,os
parser=argparse.ArgumentParser();parser.add_argument('--test',choices=['01','02','03','04','05','06','07','08','09','10'],default='01');parser.add_argument('--version');args=parser.parse_args();version=args.test
if args.version:
 import re
 assert re.fullmatch(r'\d+\.\d+\.\d+(?:-dev\.\d+)?',args.version)
label=args.version or 'test'+version
R=Path(__file__).resolve().parents[1];P=R/'project/ProjectPBZ';C=P/'Saved/Cooked/Windows';O=R/('output/game-'+label);O.mkdir(parents=True,exist_ok=True)
name='zzz_HcxmmxLeMalin_'+(args.version.replace('.','_').replace('-','_') if args.version else 'Test'+version)+'_P';paktool=Path(os.environ.get('UNREAL_ROOT', 'D:/Epic Games/UE_5.7'))/'Engine/Binaries/Win64/UnrealPak.exe'
allow=json.loads((R/'metadata/cook-allowlist.json').read_text());files=[]
for relative in allow:
 file=C/'ProjectPBZ'/relative;assert file.is_file(),file;files.append(file)
 files += [p for ext in ['.uexp','.ubulk','.uptnl'] if (p:=file.with_suffix(ext)).is_file()]
assert all('PA_Hun.' not in str(p) and not any('/'+v+'/' in p.as_posix() for v in ['Hcx2B','Garfield','PBZContent','HcxEditorStubs','Preview']) for p in files)
response=O/'response.txt';response.write_text('\n'.join(f'"{p.as_posix()}" "../../../{p.relative_to(C).as_posix()}"' for p in files),encoding='utf-8')
commands=O/'commands.txt';commands.write_text(f'-Output="{(O/(name+".utoc")).as_posix()}" -ContainerName={name} -ResponseFile="{response.as_posix()}"\n',encoding='utf-8')
metadata=C/'ProjectPBZ/Metadata'
with (O/'package.log').open('w') as log:
 subprocess.run([str(paktool),str(P/'ProjectPBZ.uproject'),'-CreateGlobalContainer='+str(O/'global.utoc'),'-CookedDirectory='+str(C),'-Commands='+str(commands),'-PackageStoreManifest='+str(metadata/'packagestore.manifest'),'-ScriptObjects='+str(metadata/'scriptobjects.bin'),'-TargetPlatform=Windows','-unattended'],stdout=log,stderr=subprocess.STDOUT,check=True)
marker=O/'marker.txt';marker.write_text('Hcxmmx LeMalin PBZ '+label+'\n',encoding='utf-8');mr=O/'marker-response.txt';mr.write_text(f'"{marker.as_posix()}" "../../../ProjectPBZ/Content/LeMalin/marker.txt"\n',encoding='utf-8')
marker_pak=O/(name+'.pak')
if marker_pak.exists():marker_pak.unlink()
with (O/'marker.log').open('w') as log:subprocess.run([str(paktool),str(marker_pak),'-Create='+str(mr)],stdout=log,stderr=subprocess.STDOUT,check=True)
manifest={'version':'LeMalin-'+label,'assets':allow,'files':[{'name':p.name,'size':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in [O/(name+ext) for ext in ['.pak','.utoc','.ucas']]],'validation':'pending user in-game test'}
(O/'manifest.json').write_text(json.dumps(manifest,indent=2));print(json.dumps(manifest['files'],indent=2))
