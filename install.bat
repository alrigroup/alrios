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

if exist "%OUT%\arinstall.exe" (
    echo -^> Iniciando assistente de instalacao arinstall.exe...
    start "" "%OUT%\arinstall.exe" %*
) else (
    echo -^> Verificando binarios do ecossistema...
    if exist "%OUT%\alrios.exe" (
        echo [OK] ALRIOS instalado com sucesso em %OUT%
        echo.
        echo Comandos rapidos:
        echo   %OUT%\alrios.exe power on   (Inicia os microservicos)
        echo   %OUT%\alrios.exe arpm list  (Lista aplicativos instalados)
    ) else (
        echo [ERRO] Binarios do ALRIOS nao encontrados na pasta arcore.
    )
    echo.
    pause
)
