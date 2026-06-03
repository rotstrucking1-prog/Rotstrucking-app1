@echo off
title AoC Deploy Bridge — Installation
color 0E
echo.
echo  ============================================
echo    AoC Deploy Bridge v2.0 — INSTALLER
echo    One-time setup
echo  ============================================
echo.

REM Create install directory
echo Creating C:\AoC-Tools...
mkdir "C:\AoC-Tools" 2>nul

REM Copy files
echo Copying bridge files...
copy /Y "%~dp0aoc_deploy_bridge.py" "C:\AoC-Tools\" >nul
copy /Y "%~dp0START_BRIDGE.bat" "C:\AoC-Tools\" >nul
copy /Y "%~dp0START_NGROK.bat" "C:\AoC-Tools\" >nul
copy /Y "%~dp0START_ALL.bat" "C:\AoC-Tools\" >nul

REM Create desktop shortcut
echo Creating desktop shortcut...
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut([Environment]::GetFolderPath('Desktop') + '\AoC Deploy Bridge.lnk'); $s.TargetPath = 'C:\AoC-Tools\START_ALL.bat'; $s.WorkingDirectory = 'C:\AoC-Tools'; $s.Description = 'Start AoC Deploy Bridge + ngrok'; $s.Save(); Write-Host '  Desktop shortcut created!' -ForegroundColor Green"

REM Create deploy directories
echo Creating deploy directories...
mkdir "C:\Users\Bradh\Documents\Unreal Projects\AOC\DeployLogs" 2>nul
mkdir "C:\Users\Bradh\Documents\Unreal Projects\AOC\DeployBackups" 2>nul

REM Verify Python
echo.
echo Checking Python...
"C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe" --version
if errorlevel 1 (
    echo WARNING: Python 3.14 not found at expected path!
    echo Please update the path in aoc_deploy_bridge.py
) else (
    echo   Python OK!
)

REM Verify ngrok
echo.
echo Checking ngrok...
ngrok version >nul 2>&1
if errorlevel 1 (
    echo WARNING: ngrok not found in PATH!
    echo Download from: https://ngrok.com/download
    echo Or: winget install ngrok.ngrok
) else (
    echo   ngrok OK!
)

echo.
echo ============================================
echo   INSTALLATION COMPLETE!
echo.
echo   Files installed to: C:\AoC-Tools\
echo   Desktop shortcut: AoC Deploy Bridge
echo.
echo   TO START: Double-click the desktop shortcut
echo   or run C:\AoC-Tools\START_ALL.bat
echo.
echo   Then send the ngrok URL to Tasklet!
echo ============================================
echo.
pause
