@echo off
rem Compare Sally SDK struct layouts between bcc64x (plugin) and MSVC (Sally).
rem Output: layout-bcc.txt, layout-msvc.txt, layout-diff.txt, layout-build.log
setlocal
cd /d "%~dp0"
if not exist Win64x mkdir Win64x
call "%ProgramFiles(x86)%\Embarcadero\Studio\37.0\bin\rsvars.bat"
echo === bcc64x > layout-build.log
"%BDS%\bin64\bcc64x.exe" -I..\shared -D_WINDOWS -DSTRICT -DLOCALINTERFACE tools_layout_check.cpp -o Win64x\layout_bcc.exe >> layout-build.log 2>&1
if exist Win64x\layout_bcc.exe Win64x\layout_bcc.exe > layout-bcc.txt 2>&1

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%I"
echo === msvc at "%VSPATH%" >> layout-build.log
if "%VSPATH%"=="" goto :nomsvc
cmd /d /c ""%~dp0layout-msvc.bat" "%VSPATH%""
fc layout-msvc.txt layout-bcc.txt > layout-diff.txt
echo done >> layout-build.log
exit /b 0
:nomsvc
echo MSVC not found via vswhere >> layout-build.log
exit /b 0
