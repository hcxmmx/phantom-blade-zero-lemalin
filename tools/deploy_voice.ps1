param([ValidatePattern('^\d+\.\d+\.\d+(-dev\.\d+)?$')][string]$Version='0.1.0-dev.1')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$package=Get-Content -LiteralPath (Join-Path $repo "output/voice-prototype/$Version/package.json") -Raw -Encoding UTF8 | ConvertFrom-Json
$gameRoot='D:\youxi\steamapps\common\PBZ'
$running=Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase) }
if($running){throw 'Game is running. Close it before deploying voice.'}
$target=Join-Path $gameRoot 'ProjectPBZ\Content\Paks\~mods\HcxmmxLeMalinVoice'
$receipt=Join-Path $repo 'output/current-voice-installed.json'
$previous=@()
if(Test-Path -LiteralPath $target){
 if(!(Test-Path -LiteralPath $receipt)){throw 'Untracked voice folder exists; inspect before replacing'}
 $installed=Get-Content -LiteralPath $receipt -Raw -Encoding UTF8 | ConvertFrom-Json
 if($installed.installed_directory -ne $target){throw 'Unexpected prior voice directory'}
 $previous=@($installed.files)
 if(@(Get-ChildItem -LiteralPath $target).Count -ne $previous.Count){throw 'Voice folder changed since prior deployment'}
 foreach($file in $previous){if((Get-FileHash -LiteralPath (Join-Path $target $file.name) -Algorithm SHA256).Hash -ne $file.sha256){throw 'Installed voice file changed'}}
}
foreach($file in $package.files){
 if([IO.Path]::GetFileName($file.name) -ne $file.name -or $file.name -notlike 'zzz_HcxmmxLeMalinVoice_*'){throw 'Unexpected voice filename'}
 if((Get-FileHash -LiteralPath (Join-Path $package.directory $file.name) -Algorithm SHA256).Hash -ne $file.sha256){throw 'Voice package hash mismatch'}
}
$backup=Join-Path $repo ('output/backups/voice-before-'+$Version+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach($file in $previous){Copy-Item -LiteralPath (Join-Path $target $file.name) -Destination (Join-Path $backup $file.name)}
if(Test-Path -LiteralPath $receipt){Copy-Item -LiteralPath $receipt -Destination (Join-Path $backup 'installed-before.json')}
@{directory=$target;files=$previous} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $backup 'before.json') -Encoding UTF8
New-Item -ItemType Directory -Force -Path $target | Out-Null
try{
 foreach($file in $previous){Remove-Item -LiteralPath (Join-Path $target $file.name)}
 foreach($file in $package.files){
  $destination=Join-Path $target $file.name
  Copy-Item -LiteralPath (Join-Path $package.directory $file.name) -Destination $destination
  if((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $file.sha256){throw 'Installed voice hash mismatch'}
 }
}catch{
 foreach($file in $package.files){$path=Join-Path $target $file.name;if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}}
 foreach($file in $previous){Copy-Item -LiteralPath (Join-Path $backup $file.name) -Destination (Join-Path $target $file.name)}
 throw
}
@{version=$package.version;installed_directory=$target;files=$package.files;restore_backup=$backup;language_setting='Chinese';game_validation='pending user manual test'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receipt -Encoding UTF8
Write-Output "LEMALIN_VOICE_$($package.version)_INSTALLED"
