@echo off
cd /d "%~dp0"
if not exist "artifacts\Bunker32-Controller.exe" powershell -NoProfile -ExecutionPolicy Bypass -File "pc\Build-Controller.ps1"
if not exist "artifacts\Bunker32-Controller.exe" (pause & exit /b 1)
start "" "artifacts\Bunker32-Controller.exe" --port COM3
