@echo off
rem Leave this running in a cmd window. Claude writes "build.request" containing
rem the module name(s) (core putty forms windows salamander), this script runs
rem compile-each.bat for each and writes "build.done" when finished.
cd /d "%~dp0"
echo Watching for build.request in %CD% ... (Ctrl+C to stop)
:loop
if exist build.request (
  set /p MODS=<build.request
  del build.request
  if exist build.done del build.done
  call :run
  echo done> build.done
  echo [%TIME%] finished: %MODS%
)
timeout /t 3 /nobreak >nul
goto loop

:run
for %%M in (%MODS%) do if /i "%%M"=="pas" (call build-x64.bat MyComponents.cbproj) else (call compile-each.bat %%M)
exit /b 0
