@echo off
title AoC Deploy Bridge v2.0
color 0A
echo.
echo  ============================================
echo    AoC Deploy Bridge v2.0
echo    Starting deployment server...
echo  ============================================
echo.

REM Check Python
"C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe" --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found at expected path!
    echo Expected: C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe
    pause
    exit /b 1
)

REM Start the bridge
echo Starting AoC Deploy Bridge on port 8080...
echo.
"C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0aoc_deploy_bridge.py"

echo.
echo Bridge stopped. Press any key to exit.
pause
