param(
    [Parameter(Mandatory)][string]$Script,
    [string]$UnrealRoot = $env:UNREAL_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $UnrealRoot) { throw 'Set UNREAL_ROOT or pass -UnrealRoot.' }
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'project/ProjectPBZ/ProjectPBZ.uproject'
$scriptPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot $Script)).Path
$editor = Join-Path $UnrealRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
& $editor $project "-ExecutePythonScript=$scriptPath" -unattended -nullrhi -nosplash -stdout -FullStdOutLogOutput
if ($LASTEXITCODE -ne 0) { throw "UE script failed with exit code $LASTEXITCODE" }
