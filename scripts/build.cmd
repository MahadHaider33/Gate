@echo off
setlocal
cd /d "%~dp0.."
if not defined VCToolsInstallDir (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -all -latest -products * -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" 10.0.26100.0
)
set "PATH=%CD%\.tools\cmake\data\bin;%CD%\.tools\bin;%PATH%"
cmake --preset release
if errorlevel 1 exit /b 1
cmake --build --preset release
if errorlevel 1 exit /b 1
ctest --preset release
exit /b %errorlevel%
