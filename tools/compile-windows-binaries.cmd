@echo off
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
  echo Usage: %~nx0 [Debug^|Release]
  exit /b 2
)
where cmake >nul 2>nul
if errorlevel 1 (
  echo cmake was not found on PATH.
  exit /b 1
)
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "BUILD=%ROOT%\build\native-windows"
cmake -S "%ROOT%\tools\portable-smoke" -B "%BUILD%"
if errorlevel 1 exit /b %errorlevel%
cmake --build "%BUILD%" --config %CONFIG% --target bacnet-discover bacnet-client
if errorlevel 1 exit /b %errorlevel%
echo %BUILD%\native\%CONFIG%\bacnet-discover.exe
echo %BUILD%\native\%CONFIG%\bacnet-client.exe
endlocal
