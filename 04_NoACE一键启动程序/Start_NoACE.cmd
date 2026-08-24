@echo off
setlocal
cd /d "%~dp0"
set "GAME_EXE=C:\Program Files (x86)\preternatural\preternatural.exe"
if defined PRETERNATURAL_GAME_EXE set "GAME_EXE=%PRETERNATURAL_GAME_EXE%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start_Game_NoACE.ps1" -GameExe "%GAME_EXE%"
if errorlevel 1 (
  echo.
  echo Launcher failed. See logs\launcher_status.log
  pause
)
endlocal
