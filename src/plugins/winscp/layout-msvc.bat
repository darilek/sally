@echo off
rem Called in a separate cmd by layout-check.bat: builds/runs the layout test with MSVC.
setlocal DisableDelayedExpansion
set "VSCMD_START_DIR=%~dp0"
set "PATH=%SystemRoot%\system32;%SystemRoot%;%SystemRoot%\System32\Wbem"
call "%~1\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0"
echo vcvars done, cl: >> layout-build.log
cl /nologo /EHsc /std:c++17 /DUNICODE /D_UNICODE /D_WINDOWS /I..\shared tools_layout_check.cpp /FoWin64x\ /Fe:Win64x\layout_msvc.exe >> layout-build.log 2>&1
if exist Win64x\layout_msvc.exe Win64x\layout_msvc.exe > layout-msvc.txt 2>&1
exit /b 0
