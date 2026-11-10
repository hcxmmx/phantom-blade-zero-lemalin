param([string]$UnrealRoot='D:/Epic Games/UE_5.7',[ValidateSet('01','02','03','04','05','06','07','08','09','10')][string]$Test='01',[string]$Version='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$project=Join-Path $repo 'project/ProjectPBZ/ProjectPBZ.uproject'
$label=if($Version){$Version}else{"test$Test"}
& (Join-Path $UnrealRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $project -run=Cook -TargetPlatform=Windows "-CookDir=$repo/project/ProjectPBZ/Content/LeMalin" "-CookDir=$repo/project/ProjectPBZ/Plugins/HuaShira/Content" -NoDefaultMaps -NoGameAlwaysCook -unattended -nullrhi -nosplash -stdout -FullStdOutLogOutput "-abslog=$repo/output/cook-$label.log"
if($LASTEXITCODE -ne 0){throw "Cook failed $LASTEXITCODE"}
