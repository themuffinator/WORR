@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "WINDRES=%SCRIPT_DIR%rc.cmd"
meson %*
exit /b %ERRORLEVEL%
