@echo off
setlocal

pushd "%~dp0"
python tools\project.py build --host hmi --profile main
set "HMI_EXIT_CODE=%ERRORLEVEL%"

echo.
if "%HMI_EXIT_CODE%"=="0" (
  echo HMI main profile build completed. No firmware was uploaded.
) else (
  echo HMI main profile build failed. Exit code: %HMI_EXIT_CODE%
)
pause

popd
exit /b %HMI_EXIT_CODE%
