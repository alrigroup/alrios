@echo off
rem Copyright (c) 2026 ALRIGROUP and its affiliates.
rem Engineered and maintained by ALRI Development.
rem
rem This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
rem found in the LICENSE file in the root directory of this source tree.

setlocal

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set ARCORE_DIR=%ROOT%\arcore

if not exist "%ARCORE_DIR%\arcore.exe" (
    echo [ERROR] arcore.exe not found. Run build.bat first.
    exit /b 1
)

echo === Stopping old processes ===
call "%ROOT%\stop.bat" >nul 2>&1

cd /d "%ARCORE_DIR%"

echo === Starting ALRIOS Supervisor (arcore) ===
echo Press Ctrl+C to stop.
echo.

arcore.exe %*
set EXITCODE=%ERRORLEVEL%

echo arcore exited (code %EXITCODE%).
exit /b %EXITCODE%
