@echo off
title AoC Tools - First Time Setup
color 0A
echo.
echo  ================================================
echo   AoC Development Tools - First Time Setup
echo  ================================================
echo.
echo  This will set up your AoC development tools.
echo  Press any key to continue or Ctrl+C to cancel.
pause >nul

:: Create tool directories
echo.
echo [1/5] Creating directories...
if not exist "C:\AoC-Tools" mkdir "C:\AoC-Tools"
if not exist "C:\AoC-Tools\backups" mkdir "C:\AoC-Tools\backups"
if not exist "C:\AoC-Tools\logs" mkdir "C:\AoC-Tools\logs"
if not exist "%USERPROFILE%\Downloads\AOC-Deploy" mkdir "%USERPROFILE%\Downloads\AOC-Deploy"
echo       Done!

:: Copy tools to C:\AoC-Tools
echo [2/5] Installing tools to C:\AoC-Tools...
copy /Y "%~dp0aoc_server.py" "C:\AoC-Tools\aoc_server.py" >nul
copy /Y "%~dp0config.json" "C:\AoC-Tools\config.json" >nul
copy /Y "%~dp0Start-AoC-Server.bat" "C:\AoC-Tools\Start-AoC-Server.bat" >nul
copy /Y "%~dp0Restart-UE5.bat" "C:\AoC-Tools\Restart-UE5.bat" >nul
copy /Y "%~dp0Deploy-Build-Restart.bat" "C:\AoC-Tools\Deploy-Build-Restart.bat" >nul
copy /Y "%~dp0Quick-Deploy.bat" "C:\AoC-Tools\Quick-Deploy.bat" >nul
echo       Done!

:: Update bat files to use C:\AoC-Tools path
echo [3/5] Updating paths...
:: The bat files reference %%~dp0 so they work from any location

:: Create desktop shortcuts
echo [4/5] Creating desktop shortcuts...
set DESKTOP=%USERPROFILE%\Desktop

:: Restart UE5 shortcut
echo @echo off > "%DESKTOP%\AoC Restart UE5.bat"
echo call "C:\AoC-Tools\Restart-UE5.bat" >> "%DESKTOP%\AoC Restart UE5.bat"

:: Deploy + Build + Restart shortcut
echo @echo off > "%DESKTOP%\AoC Deploy-Build-Restart.bat"
echo call "C:\AoC-Tools\Deploy-Build-Restart.bat" >> "%DESKTOP%\AoC Deploy-Build-Restart.bat"

:: Start Server shortcut
echo @echo off > "%DESKTOP%\AoC Start Server.bat"
echo call "C:\AoC-Tools\Start-AoC-Server.bat" >> "%DESKTOP%\AoC Start Server.bat"

echo       Done!

:: Add ngrok config instructions
echo [5/5] Setup complete!
echo.
echo  ================================================
echo   INSTALLATION COMPLETE!
echo  ================================================
echo.
echo  Desktop shortcuts created:
echo    - AoC Restart UE5.bat
echo    - AoC Deploy-Build-Restart.bat
echo    - AoC Start Server.bat
echo.
echo  HOW TO USE:
echo  ===========
echo  1. Double-click "AoC Start Server.bat" to start
echo     the deployment server (http://localhost:8080)
echo.
echo  2. Double-click "AoC Restart UE5.bat" to quickly
echo     close and reopen UE5 with AocWorld
echo.
echo  3. Double-click "AoC Deploy-Build-Restart.bat"
echo     to deploy code, rebuild, and restart UE5
echo.
echo  4. Drop .h/.cpp files in:
echo     %%USERPROFILE%%\Downloads\AOC-Deploy
echo     They will auto-deploy when the server is running!
echo.
echo  OPTIONAL - Add ngrok tunnel for remote access:
echo     ngrok http 8080
echo     (This lets the AI agent push code directly)
echo.
pause
