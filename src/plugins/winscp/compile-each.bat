@echo off
rem Compile every source of one module separately (does NOT stop at first failing file).
rem Usage: compile-each.bat core|putty|forms|windows|salamander
rem Output: compile-<module>.log  (full)   and  compile-<module>-errors.log (error lines only)
setlocal EnableDelayedExpansion
cd /d "%~dp0"
set MOD=%~1
if "%MOD%"=="" set MOD=core
call "%ProgramFiles(x86)%\Embarcadero\Studio\37.0\bin\rsvars.bat"

if /i "%MOD%"=="scpcore-rel" (
  call build-x64.bat ScpCore.cbproj Release
  copy /y build.log build-scpcore-rel.log >nul
  exit /b 0
)
if /i "%MOD%"=="puttylib-rel" (
  call build-x64.bat Putty.cbproj Release
  copy /y build.log build-putty-rel.log >nul
  exit /b 0
)
if /i "%MOD%"=="plugin-rel" (
  if exist Win64x\Release\WinSCP.spl del Win64x\Release\WinSCP.spl
  call build-x64.bat WinSCP.cbproj Release
  copy /y build.log build-plugin-rel.log >nul
  exit /b 0
)
if /i "%MOD%"=="puttylib" (
  call build-x64.bat Putty.cbproj
  copy /y build.log build-putty.log >nul
  exit /b 0
)
if /i "%MOD%"=="plugin" (
  rem static libs are not tracked as link inputs: force relink
  if exist Win64x\Debug\WinSCP.spl del Win64x\Debug\WinSCP.spl
  call build-x64.bat WinSCP.cbproj
  copy /y build.log build-plugin.log >nul
  exit /b 0
)
if /i "%MOD%"=="scpcore" (
  call build-x64.bat ScpCore.cbproj
  exit /b 0
)
if /i "%MOD%"=="layout" goto :layout
if /i "%MOD%"=="info" (
  if not exist _info mkdir _info
  copy /y "%BDS%\bin\CodeGear.Cpp.Targets" _info\ 
  copy /y "%BDS%\bin\CodeGear.Delphi.Targets" _info\ 
  copy /y "%BDS%\bin\Borland.Cpp.Targets" _info\ 
  dir /b "%BDS%\bin\*.Targets" > _info\targets.txt
  dir /b "%BDS%\bin64" > _info\bin64.txt
  exit /b 0
)
set INC=-I core -I windows -I forms -I resource -I putty -I putty\windows -I putty\charset -I salamander -I ..\shared -I packages\my -I lib\hpp
set DEF=-D _DEBUG -D _WINDOWS -D FRAMEWORK_VCL -D USEPACKAGES -D NO_USING_NAMESPACE_SYSTEM_SYNCOBJS
set LANG=-x c++ -std=c++23
set SRC=core\*.cpp
if /i "%MOD%"=="core"       set DEF=%DEF% -D SSPI_MECH -D NO_FILEZILLA
if /i "%MOD%"=="putty"      set DEF=%DEF% -D SECURITY_WIN32 -D NET_SETUP_DIAGNOSTICS -D MPEXT -D _RTLDLL
if /i "%MOD%"=="putty"      set LANG=-x c -std=gnu11
if /i "%MOD%"=="putty"      set INC=-I putty -I putty\windows -I putty\charset
if /i "%MOD%"=="putty"      set SRC=putty\*.c putty\windows\*.c putty\charset\*.c
if /i "%MOD%"=="forms"      set DEF=%DEF% -D LOCALINTERFACE -D NO_RESOURCES -D NO_COMPONENTS
if /i "%MOD%"=="forms"      set SRC=forms\*.cpp
if /i "%MOD%"=="windows"    set DEF=%DEF% -D LOCALINTERFACE -D NO_RESOURCES -D NO_COMPONENTS
if /i "%MOD%"=="windows"    set SRC=windows\*.cpp
if /i "%MOD%"=="salamander" set DEF=%DEF% -D LOCALINTERFACE -D NO_RESOURCES -D STRICT
if /i "%MOD%"=="salamander" set SRC=salamander\*.cpp
if /i "%MOD%"=="shared"     set DEF=%DEF% -D LOCALINTERFACE -D STRICT
if /i "%MOD%"=="shared"     set SRC=..\shared\dbg.cpp ..\shared\winliblt.cpp

set OUT=Win64x\each\%MOD%
if not exist "%OUT%" mkdir "%OUT%"
set LOG=compile-%MOD%.log
echo Compiling module %MOD% > %LOG%
set OK=0
set BAD=0
set BADLIST=
for %%F in (%SRC%) do if /i not "%%~nF"=="KeyGen" (
  echo ===== %%F >> %LOG%
  "%BDS%\bin64\bcc64x.exe" -cc1 %DEF% -output-dir %OUT% %INC% ^
   -isystem "%BDS%\include" -isystem "%BDS%\include\x86_64-w64-mingw32\c++\v1" -isystem "%BDS%\lib\clang\20\include" ^
   -isystem "%BDS%\include\x86_64-w64-mingw32" -isystem "%BDS%\include\windows\sdk" -isystem "%BDS%\include\windows\rtl" ^
   -isystem "%BDS%\include\windows\vcl" -isystem "%BDSCOMMONDIR%\hpp\Win64x" ^
   -fcxx-exceptions -ffp-contract=on -fno-rounding-math -mlong-double-64 -fgnuc-version=4.2.1 -mms-bitfields ^
   -fno-sized-deallocation -fno-use-init-array -fborland-extensions -fdiagnostics-format borland ^
   -nobuiltininc -nostdsysteminc -triple x86_64-w64-windows-gnu -emit-obj -pic-level 2 -target-cpu x86-64 ^
   -fexceptions -exception-model=seh -funwind-tables=2 -fno-spell-checking -fno-use-cxa-atexit ^
   -ferror-limit 0 -main-file-name %%~nxF %LANG% -O0 -tR -tM -o %OUT%\%%~nF.o %%F >> %LOG% 2>&1
  if errorlevel 1 (set /a BAD+=1 & set BADLIST=!BADLIST! %%~nxF) else (set /a OK+=1)
)
echo. >> %LOG%
echo ==== RESULT: !OK! ok, !BAD! failed:!BADLIST! >> %LOG%
findstr /r /c:"error [EF][0-9]*:" /c:"fatal error" %LOG% > compile-%MOD%-errors.log
echo ==== RESULT: !OK! ok, !BAD! failed:!BADLIST!
exit /b 0

:layout
call layout-check.bat
exit /b 0
