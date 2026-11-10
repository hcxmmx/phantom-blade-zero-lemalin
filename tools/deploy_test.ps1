param([ValidateSet('01','02','03','04','05','06','07','08','09','10')][string]$Test='01',[ValidatePattern('^\d+\.\d+\.\d+(-dev\.\d+)?$|^$')][string]$Version='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$backup=Get-Content -LiteralPath (Join-Path $repo 'output/preinstall-backup.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$packageDirectory=Join-Path $repo $(if($Version){"output/game-$Version"}else{"output/game-test$Test"})
$manifest=Get-Content -LiteralPath (Join-Path $packageDirectory 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$gameRoot='D:\youxi\steamapps\common\PBZ'
$running=Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase) }
if($running){throw 'Game is running. Close it before deploying.'}
$mods=[IO.Path]::GetFullPath($backup.directory)
if($mods -ne ($gameRoot+'\ProjectPBZ\Content\Paks\~mods')){throw 'Unexpected install target'}
$actual=@(Get-ChildItem -LiteralPath $mods -File)
if($actual.Count -ne @($backup.files).Count){throw 'Installed files changed since backup; inspect before deployment'}
foreach($f in $backup.files){
 foreach($path in @((Join-Path $mods $f.name),(Join-Path $backup.backup $f.name))){if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $f.sha256){throw "Backup or installed hash mismatch: $path"}}
}
foreach($f in $manifest.files){$path=Join-Path $packageDirectory $f.name;if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $f.sha256){throw "Package hash mismatch: $path"}}
try{
 foreach($f in $backup.files){Remove-Item -LiteralPath (Join-Path $mods $f.name)}
 foreach($f in $manifest.files){Copy-Item -LiteralPath (Join-Path $packageDirectory $f.name) -Destination (Join-Path $mods $f.name);if((Get-FileHash -LiteralPath (Join-Path $mods $f.name) -Algorithm SHA256).Hash -ne $f.sha256){throw 'Installed hash mismatch'}}
}catch{
 foreach($f in $manifest.files){$path=Join-Path $mods $f.name;if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}}
 foreach($f in $backup.files){Copy-Item -LiteralPath (Join-Path $backup.backup $f.name) -Destination (Join-Path $mods $f.name)}
 throw
}
@{version=$manifest.version;installed_directory=$mods;restore_backup=$backup.backup;files=$manifest.files;game_validation='pending user manual test'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $repo 'output/current-installed.json') -Encoding UTF8
Write-Output "$($manifest.version)_INSTALLED_AND_HASH_VERIFIED"
