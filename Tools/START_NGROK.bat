@echo off
title AoC ngrok Tunnel
color 0B
echo.
echo  ============================================
echo    Starting ngrok tunnel to Deploy Bridge
echo    Port 8080 -> Public URL
echo  ============================================
echo.
echo IMPORTANT: Copy the "Forwarding" URL and send it to Tasklet!
echo Example: https://xxxx-xxxx.ngrok-free.dev
echo.

ngrok http 8080

echo.
echo ngrok stopped. Press any key to exit.
pause
