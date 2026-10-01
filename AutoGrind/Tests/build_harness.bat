@echo off
rem Builds the standalone scorer harness from the plugin's own detector source.
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) set "VSDIR=%%I"
if not defined VSDIR (
	echo No Visual Studio with the C++ tools found.
	exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /D_CRT_SECURE_NO_WARNINGS /std:c++17 /O2 /W4 /WX /EHsc /Fobuild\ /Febuild\harness.exe harness.cpp ..\Source\AutoGrind\Private\Core\*.cpp
