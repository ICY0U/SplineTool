# Builds the editor with the AutoGrind plugin and runs its in-engine checks (AutoGrindTestCommandlet).
# Run from <Project>/Plugins/AutoGrind/Tests. Close the editor first: the build cannot replace a DLL it has loaded.
$ErrorActionPreference = 'Stop'
# Set UE_ROOT to your UE 5.4 install's Engine folder, e.g. C:/Program Files/Epic Games/UE_5.4/Engine
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:/Program Files/Epic Games/UE_5.4/Engine' }
$project = (Get-ChildItem (Join-Path $PSScriptRoot '../../..') -Filter *.uproject | Select-Object -First 1).FullName
$log = Join-Path $PSScriptRoot 'build/unreal-test.log'
New-Item -ItemType Directory -Force (Join-Path $PSScriptRoot 'build') | Out-Null

$target = [IO.Path]::GetFileNameWithoutExtension($project) + 'Editor'
& "$engine/Build/BatchFiles/Build.bat" $target Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Editor build failed ($LASTEXITCODE)" }

& "$engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project -run=AutoGrindTest -unattended -nullrhi -nosplash "-abslog=$log" | Out-Null
Select-String -Path $log -Pattern 'LogAutoGrindTest|Assertion failed|Error: (?!\[Callstack\])' | ForEach-Object { $_.Line }
if (-not (Select-String -Path $log -Pattern 'AUTOGRIND_TEST_PASS' -Quiet)) { throw "AutoGrind in-engine checks failed: see $log" }
