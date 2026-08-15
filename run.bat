@echo off
setlocal

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set ARWS=%ROOT%\arcore
set CFG=%ARWS%\storage\arws\arws.cfg
set TESTCFG=%ARWS%\storage\arws\arws.test.cfg
set BAKCFG=%ARWS%\storage\arws\arws.cfg.production

if not exist "%ARWS%\arcore.exe" (
    echo [ERROR] arcore.exe not found. Run build.bat first.
    exit /b 1
)

if not exist "%TESTCFG%" (
    echo [ERROR] arws.test.cfg not found.
    exit /b 1
)

echo === Stopping old processes ===
call "%ROOT%\stop.bat" >nul 2>&1

cd /d "%ARWS%"

echo === Switching arws.cfg to TEST mode (backup = arws.cfg.production) ===
if exist "%CFG%" (
    copy /y "%CFG%" "%BAKCFG%" >nul
)
copy /y "%TESTCFG%" "%CFG%" >nul

echo === Starting arcore (TEST mode: http://127.0.0.1:8080) ===
echo Press Ctrl+C to stop. arws.cfg will be restored on exit.
echo.

arcore.exe
set EXITCODE=%ERRORLEVEL%

echo.
echo === Restoring production arws.cfg ===
if exist "%BAKCFG%" (
    copy /y "%BAKCFG%" "%CFG%" >nul
)

echo arcore exited (code %EXITCODE%).
exit /b %EXITCODE%
