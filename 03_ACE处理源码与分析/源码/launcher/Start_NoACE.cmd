@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start_Game_NoACE.ps1"
if errorlevel 1 (
  echo.
  echo Launcher failed. See logs\launcher_status.log
  pause
)
endlocal
