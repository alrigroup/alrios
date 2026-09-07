@echo off
rem ==============================================================================
rem ALRIOS — Sovereign Operating System & Microkernel Ecosystem
rem Official Automated Installer Bootstrap for Windows (x64)
rem ==============================================================================
setlocal enabledelayedexpansion

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set OUT=%ROOT%\arcore

title ALRIOS Installer Setup

echo ========================================================================
echo           ALRIOS — Instalador Automatizado para Windows
echo ========================================================================
echo.

echo -> [1/3] Verificando binarios do ecossistema em %OUT%...
if not exist "%OUT%\alrios.exe" (
    echo [ERRO] Binarios do ALRIOS nao encontrados na pasta arcore.
    pause
    exit /b 1
)

echo -> [2/3] Registrando ALRIOS no PATH do Windows...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$userPath = [Environment]::GetEnvironmentVariable('Path', 'User'); if ($userPath -notlike '*%OUT%*') { [Environment]::SetEnvironmentVariable('Path', $userPath + ';%OUT%', 'User'); Write-Host '   ✓ PATH do usuario atualizado com sucesso.' } else { Write-Host '   ✓ ALRIOS ja esta presente no PATH.' }"

echo -> [3/3] Finalizando instalacao...
echo.
echo ========================================================================
echo  ✓ ALRIOS instalado com sucesso no Windows!
echo    Abra um novo Prompt de Comando ou PowerShell e execute:
echo    alrios power on
echo    arpm list
echo ========================================================================
echo.
pause
