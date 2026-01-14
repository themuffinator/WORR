@echo off
setlocal

set "llvm_rc=%ProgramFiles%\LLVM\bin\llvm-rc.exe"
if exist "%llvm_rc%" (
  "%llvm_rc%" %*
  exit /b %ERRORLEVEL%
)

if defined ProgramFiles(x86) (
  set "llvm_rc=%ProgramFiles(x86)%\LLVM\bin\llvm-rc.exe"
  if exist "%llvm_rc%" (
    "%llvm_rc%" %*
    exit /b %ERRORLEVEL%
  )
)

where llvm-rc >nul 2>nul
if %ERRORLEVEL%==0 (
  llvm-rc %*
  exit /b %ERRORLEVEL%
)

echo llvm-rc.exe not found. Install LLVM or add llvm-rc.exe to PATH. 1>&2
exit /b 1
