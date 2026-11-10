param(
    [string]$UnrealRoot = $env:UNREAL_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $UnrealRoot) { throw 'Set UNREAL_ROOT or pass -UnrealRoot.' }
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'project/ProjectPBZ/ProjectPBZ.uproject'
$build = Join-Path $UnrealRoot 'Engine/Build/BatchFiles/Build.bat'
& $build ProjectPBZEditor Win64 Development $project -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "Editor build failed with exit code $LASTEXITCODE" }
