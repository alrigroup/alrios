@echo off
setlocal enabledelayedexpansion

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set BUILD=%ROOT%\build
set OUT=%ROOT%\arcore
set APPS=%OUT%\apps

set PATH=C:\Program Files\Go\bin;%PATH%

set SNAP=%TEMP%\alrios-src.snapshot

echo === Mapping src before build (armake snapshot) ===
if exist "%OUT%\armake.exe" (
    "%OUT%\armake.exe" snapshot "%ROOT%\src" -o "%SNAP%"
) else (
    echo   armake.exe missing - skipping snapshot for this build
)

echo === Cleaning outputs ===
if exist "%OUT%\arcore.exe" del "%OUT%\arcore.exe"
if exist "%OUT%\armake.exe" del "%OUT%\armake.exe"
if exist "%OUT%\arkernel.dll" del "%OUT%\arkernel.dll"
if exist "%OUT%\tmp" rmdir /s /q "%OUT%\tmp"
if exist "%APPS%" rmdir /s /q "%APPS%"
if exist "%OUT%\db.sqlite" del "%OUT%\db.sqlite"
mkdir "%APPS%" 2>nul

echo === CMake Configure ===
cmake -S "%ROOT%" -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
  cmake -S %ROOT% -B %BUILD% -DCMAKE_BUILD_TYPE=Release
)
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Build All ===
cmake --build "%BUILD%" --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Removing node_modules from SPA sources ===
for /d %%d in ("%ROOT%\src\apps\*.web\web\node_modules") do if exist "%%d" rmdir /s /q "%%d"

echo === Cleaning build residue in src (armake cleanup) ===
if exist "%OUT%\armake.exe" (
    "%OUT%\armake.exe" cleanup "%ROOT%\src" -f "%SNAP%"
)
if exist "%SNAP%" del /q "%SNAP%" >nul 2>&1

echo === Done ===
echo -- EXEs/DLLs in %OUT%
dir /b "%OUT%\*.exe" "%OUT%\*.dll" 2>nul
echo -- Apps in %APPS%
if exist "%APPS%" dir /s /b "%APPS%\*.arapp" 2>nul
