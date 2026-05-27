@echo off
title AoC - Restart UE5
color 0E
echo.
echo  ======================================
echo   AoC UE5 Restart
echo   Closing UE5 and reopening AocWorld
echo  ======================================
echo.

:: Kill UE5
echo [1/3] Closing UE5 Editor...
taskkill /F /IM UnrealEditor.exe >nul 2>&1
timeout /t 5 /nobreak >nul

:: Verify it's closed
tasklist /FI "IMAGENAME eq UnrealEditor.exe" /NH 2>nul | find /i "UnrealEditor.exe" >nul
if %errorlevel% equ 0 (
    echo      Still running, waiting...
    timeout /t 10 /nobreak >nul
    taskkill /F /IM UnrealEditor.exe >nul 2>&1
    timeout /t 5 /nobreak >nul
)

echo [2/3] UE5 closed!
echo.
echo [3/3] Launching UE5 with AocWorld...

:: Launch UE5 with the project and map
start "" "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\Bradh\Documents\Unreal Projects\AOC\AOC.uproject" /Game/AocWorld

echo.
echo  Done! UE5 is launching with AocWorld.
echo  This window will close in 10 seconds.
timeout /t 10
