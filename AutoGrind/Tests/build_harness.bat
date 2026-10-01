@echo off
rem Builds the standalone scorer harness from the plugin's own detector source.
setlocal
set VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /D_CRT_SECURE_NO_WARNINGS /std:c++17 /O2 /W4 /WX /EHsc /Fobuild\ /Febuild\harness.exe harness.cpp ..\Source\AutoGrind\Private\Core\AutoGrindCore.cpp
