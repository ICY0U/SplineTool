# Builds AutoGrind against an Unreal Engine install with RunUAT BuildPlugin and zips it for a release:
# dist/AutoGrind-<version>-UE<engine>-Win64.zip, holding an AutoGrind folder to drop into a project's Plugins.
#
#   powershell -ExecutionPolicy Bypass -File scripts/package.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -Engine "D:/UE_5.4/Engine" -StrictIncludes
#
# -Engine defaults to $env:UE_ROOT, then the Epic Games Launcher's UE 5.4. -StrictIncludes builds without
# precompiled headers or unity files, which shows any source file that does not include what it uses.
param(
	[string]$Engine = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:/Program Files/Epic Games/UE_5.4/Engine' }),
	[string]$Output = (Join-Path $PSScriptRoot '../dist'),
	[switch]$StrictIncludes
)
$ErrorActionPreference = 'Stop'

$plugin = (Resolve-Path (Join-Path $PSScriptRoot '../AutoGrind/AutoGrind.uplugin')).Path
$runUat = Join-Path $Engine 'Build/BatchFiles/RunUAT.bat'
if (-not (Test-Path $runUat)) { throw "No RunUAT at $runUat. Pass -Engine <UE install>/Engine or set UE_ROOT." }

$descriptor = Get-Content $plugin -Raw | ConvertFrom-Json
$engineVersion = ($descriptor.EngineVersion -split '\.')[0..1] -join '.'
$name = "AutoGrind-$($descriptor.VersionName)-UE$engineVersion-Win64"

New-Item -ItemType Directory -Force $Output | Out-Null
$Output = (Resolve-Path $Output).Path
$stage = Join-Path $Output 'AutoGrind'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }

$arguments = @('BuildPlugin', "-Plugin=$plugin", "-Package=$stage", '-TargetPlatforms=Win64')
if ($StrictIncludes) { $arguments += '-StrictIncludes' }
& $runUat @arguments | Out-Host
if ($LASTEXITCODE -ne 0) { throw "BuildPlugin failed ($LASTEXITCODE)" }

# The build's intermediate files are not needed to use the plugin.
Remove-Item -Recurse -Force (Join-Path $stage 'Intermediate') -ErrorAction SilentlyContinue
$zip = Join-Path $Output "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "Packaged $zip"
