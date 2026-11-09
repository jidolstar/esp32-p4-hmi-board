@echo off
setlocal

pushd "%~dp0"
python tools\project.py run --host windows --profile main
set "HMI_EXIT_CODE=%ERRORLEVEL%"

if not "%HMI_EXIT_CODE%"=="0" (
  echo.
  echo Failed to run the Windows main profile. Exit code: %HMI_EXIT_CODE%
  pause
)

popd
exit /b %HMI_EXIT_CODE%
