@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-local.ps1"
if errorlevel 1 (
    echo Nie udalo sie uruchomic DME Activity. Sprawdz komunikat powyzej.
    pause
    exit /b 1
)
if /I "%~1"=="--no-browser" exit /b 0
start "" "http://localhost/dme-activity/"
