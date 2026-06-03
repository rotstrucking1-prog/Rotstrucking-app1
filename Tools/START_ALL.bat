@echo off
title AoC Deploy System
color 0A
echo.
echo  ============================================
echo    AoC Deploy System — Full Stack
echo    Starting Bridge + ngrok tunnel
echo  ============================================
echo.

REM Kill any existing bridge/ngrok
taskkill /F /IM ngrok.exe >nul 2>&1

REM Start the Python bridge in background
echo [1/2] Starting Deploy Bridge on port 8080...
start "AoC Deploy Bridge" /min "C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0aoc_deploy_bridge.py"

REM Wait for bridge to start
echo Waiting for bridge to initialize...
timeout /t 3 /nobreak >nul

REM Verify bridge is running
powershell -Command "try { $r = Invoke-WebRequest -Uri 'http://localhost:8080/health' -UseBasicParsing -TimeoutSec 5; Write-Host '  Bridge is RUNNING!' -ForegroundColor Green } catch { Write-Host '  WARNING: Bridge may not have started' -ForegroundColor Yellow }"

echo.
echo [2/2] Starting ngrok tunnel...
echo.
echo =============================================
echo   COPY THE FORWARDING URL BELOW AND SEND
echo   IT TO TASKLET SO IT CAN CONNECT!
echo =============================================
echo.

ngrok http 8080

echo.
echo System stopped. Press any key to exit.
pause
