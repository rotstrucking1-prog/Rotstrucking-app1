@echo off
title AoC Deploy Server
color 0A
echo.
echo  ======================================
echo   AoC Deploy Server v1.0
echo   Starting HTTP server on port 8080...
echo  ======================================
echo.
echo  Dashboard: http://localhost:8080
echo  Drop .h/.cpp files in: %USERPROFILE%\Downloads\AOC-Deploy\
echo.
echo  Press Ctrl+C to stop the server.
echo.

:: Create deploy folder if needed
if not exist "%USERPROFILE%\Downloads\AOC-Deploy" mkdir "%USERPROFILE%\Downloads\AOC-Deploy"

:: Run the server
"C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0aoc_server.py"

pause
