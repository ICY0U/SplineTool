@echo off
rem Builds the engine-free detector with Visual Studio's compiler, warnings as errors, and runs its checks:
rem regressions, drawing, the benchmark (also turned and moved off the origin) and the stress scenes.
rem Needs Visual Studio 2022 or later, or its Build Tools, with the C++ workload. No engine or game needed.
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo Visual Studio Installer not found: install Visual Studio or its Build Tools with the C++ workload.
	exit /b 1
)
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) set "VSDIR=%%I"
if not defined VSDIR (
	echo No Visual Studio with the C++ tools found.
	exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
set FLAGS=/nologo /std:c++17 /O2 /W4 /WX /permissive- /w14456 /w14457 /w14458 /w14459 /EHsc /D_CRT_SECURE_NO_WARNINGS
for %%T in (regression drawing benchmark stress) do (
	cl %FLAGS% /Fobuild\ /Febuild\%%T.exe %%T.cpp ..\Source\AutoGrind\Private\Core\*.cpp || exit /b 1
)
build\regression.exe || exit /b 1
build\drawing.exe || exit /b 1
build\benchmark.exe -turn || exit /b 1
build\stress.exe 60 || exit /b 1
echo All detector checks passed.
