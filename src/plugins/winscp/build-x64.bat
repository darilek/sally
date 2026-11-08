@echo off
rem Build WinSCP plugin projects for Win64x (C++Builder 13 Modern toolchain).
rem Usage: build-x64.bat [Project.cbproj] [Debug|Release]
rem Output goes to build.log in this folder.
setlocal
cd /d "%~dp0"
set PROJ=%~1
if "%PROJ%"=="" set PROJ=ScpCore.cbproj
set CFG=%~2
if "%CFG%"=="" set CFG=Debug

set RSVARS=
for /d %%D in ("%ProgramFiles(x86)%\Embarcadero\Studio\*") do if exist "%%D\bin\rsvars.bat" set RSVARS=%%D\bin\rsvars.bat
if exist "%ProgramFiles(x86)%\Embarcadero\Studio\37.0\bin\rsvars.bat" set RSVARS=%ProgramFiles(x86)%\Embarcadero\Studio\37.0\bin\rsvars.bat
if "%RSVARS%"=="" (
  echo rsvars.bat not found - edit RSVARS in build-x64.bat > build.log
  type build.log
  exit /b 1
)
call "%RSVARS%"

echo Building %PROJ% [%CFG%^|Win64x] using %RSVARS% > build.log
msbuild "%PROJ%" /t:Build /p:Config=%CFG% /p:Platform=Win64x /p:BCC_EnableBatchCompilation=false /p:ContinueOnError=true /nologo /v:normal /clp:NoSummary >> build.log 2>&1
set RC=%ERRORLEVEL%
echo. >> build.log
echo ==== errors / warnings summary ==== >> build.log
findstr /i /c:" error" /c:"fatal" build.log > build-errors.log
type build-errors.log >> build.log
echo Exit code %RC% >> build.log
echo Done (exit %RC%). Log: %~dp0build.log
exit /b %RC%
