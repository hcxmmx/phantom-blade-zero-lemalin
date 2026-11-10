$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$before=Get-Content -LiteralPath (Join-Path $repo 'output/preinstall-backup.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$installed=Get-Content -LiteralPath (Join-Path $repo 'output/current-installed.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$gameRoot='D:\youxi\steamapps\common\PBZ'
if(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase) }){throw 'Close the game before restoring'}
$mods=[IO.Path]::GetFullPath($before.directory)
if($mods -ne ($gameRoot+'\ProjectPBZ\Content\Paks\~mods')){throw 'Unexpected restore target'}
foreach($f in $before.files){if((Get-FileHash -LiteralPath (Join-Path $before.backup $f.name) -Algorithm SHA256).Hash -ne $f.sha256){throw 'Backup hash mismatch'}}
foreach($f in $installed.files){$path=Join-Path $mods $f.name;if(Test-Path -LiteralPath $path){if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $f.sha256){throw 'Installed file was modified; inspect before restoring'}}}
foreach($f in $installed.files){$path=Join-Path $mods $f.name;if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}}
foreach($f in $before.files){Copy-Item -LiteralPath (Join-Path $before.backup $f.name) -Destination (Join-Path $mods $f.name);if((Get-FileHash -LiteralPath (Join-Path $mods $f.name) -Algorithm SHA256).Hash -ne $f.sha256){throw 'Restore hash mismatch'}}
@{version=$before.version;installed_directory=$mods;files=$before.files;restored_previous=$true;restored_from=$installed.version;restore_backup=$before.backup} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $repo 'output/current-installed.json') -Encoding UTF8
Write-Output 'PREVIOUS_CHARACTER_RESTORED_AND_HASH_VERIFIED'
