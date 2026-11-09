@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"

python "%ROOT%samples\suite\update_hmi_assets.py" %*
if errorlevel 1 (
  echo [ERROR] HMI asset update failed.
  exit /b 1
)
exit /b 0
