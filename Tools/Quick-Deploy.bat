@echo off
title AoC - Quick Deploy
color 0D
echo.
echo  ======================================
echo   AoC Quick Deploy
echo   Deploying files from Downloads...
echo  ======================================
echo.

"C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0aoc_server.py" --deploy

echo.
pause
