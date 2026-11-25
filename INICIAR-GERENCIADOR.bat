@echo off
setlocal

set "PROJECT_ROOT=%~dp0"
set "LAUNCHER=%PROJECT_ROOT%save-manager\scripts\run-save-manager.ps1"

if not exist "%LAUNCHER%" (
    echo Nao foi possivel encontrar:
    echo %LAUNCHER%
    echo.
    pause
    exit /b 1
)

where python.exe >nul 2>nul
if errorlevel 1 (
    echo Python 3 nao foi encontrado no PATH do Windows.
    echo Instale o Python 3 e tente novamente.
    echo.
    pause
    exit /b 1
)

start "" powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%LAUNCHER%"
exit /b 0
