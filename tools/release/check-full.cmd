@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%..\.."
set "SCRIPT=%SCRIPT_DIR%backcheck-global-example.ps1"

pushd "%REPO_ROOT%" >nul

echo ============================================================
echo Release Backcheck HIL Runner
echo ============================================================
echo Start: %DATE% %TIME%
echo.
echo Command:
echo pwsh -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Version "0.24.2" -Example "examples/hil-wago-client-acceptance" -Upload -Monitor -UploadPort "COM4" -MonitorBaud 115200
echo.

pwsh -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Version "0.24.2" -Example "examples/hil-wago-client-acceptance" -Upload -Monitor -UploadPort "COM4" -MonitorBaud 115200
set "EXITCODE=%ERRORLEVEL%"

popd >nul

echo.
echo ============================================================
if "%EXITCODE%"=="0" (
	echo STATUS: SUCCESS
) else (
	echo STATUS: FAILED (exit code %EXITCODE%^)
)
echo End:   %DATE% %TIME%
echo ============================================================
echo.
echo Note: If monitor was opened, stop it with Ctrl+C before this status screen appears.
echo.
pause
exit /b %EXITCODE%
