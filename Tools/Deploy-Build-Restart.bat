@echo off
title AoC - Full Deploy Cycle
color 0B
echo.
echo  ================================================
echo   AoC FULL CYCLE: Deploy + Build + Restart
echo   One click to rule them all!
echo  ================================================
echo.

"C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0aoc_server.py" --full-cycle

echo.
echo  ================================================
echo   Full cycle complete!
echo  ================================================
pause
