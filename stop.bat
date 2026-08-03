@echo off
taskkill /f /im arcore.exe 2>nul
taskkill /f /im auth.exe 2>nul
taskkill /f /im cdn.exe 2>nul
taskkill /f /im home.exe 2>nul
taskkill /f /im database.exe 2>nul
taskkill /f /im static.exe 2>nul
taskkill /f /im node.exe 2>nul
taskkill /f /im python.exe 2>nul

:: Kill any orphaned processes on arcore ports
for %%p in (9500 9501 9502 9503 9504) do (
    for /f "tokens=5" %%a in ('netstat -ano ^| findstr ":%%p "') do (
        taskkill /f /pid %%a 2>nul
    )
)

echo arcore and all backend services stopped.
