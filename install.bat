@echo off
rem ==============================================================================
rem ALRIOS — Sovereign Operating System & Microkernel Ecosystem
rem Unified One-Click Launcher for ALRIOS Master Installer (Windows)
rem ==============================================================================
set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%

if exist "%ROOT%\arcore\arinstall.exe" (
    start "" "%ROOT%\arcore\arinstall.exe" %*
) else (
    echo [ALRIOS] Executavel arinstall.exe nao encontrado em arcore.
    pause
)
