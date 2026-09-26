@echo off
setlocal EnableExtensions EnableDelayedExpansion

where cl >nul 2>nul
if not errorlevel 1 goto run_tests

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" goto missing_tools

for /f "usebackq tokens=*" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL goto missing_tools

call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto setup_failed

:run_tests
pushd "%~dp0"
cl /nologo /std:c++17 /EHsc /W4 timer_test.cpp /Fe:timer_test.exe
if errorlevel 1 (
  popd
  exit /b 2
)

timer_test.exe
set "TEST_EXIT=%ERRORLEVEL%"
popd
exit /b %TEST_EXIT%

:missing_tools
echo Visual Studio C++ Build Tools were not found.
exit /b 2

:setup_failed
echo Visual Studio C++ Build Tools initialization failed.
exit /b 2
