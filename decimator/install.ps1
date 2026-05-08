# Decimator v4.2 Installer
# Roat Pkz PVP Assist Bot

$ErrorActionPreference = "Continue"
$botDir = "C:\RoatBot"
$pyExe = "C:\Python314\python.exe"
$botUrl = "https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/decimator/bot.py"

Write-Host ""
Write-Host "============================================" -ForegroundColor Red
Write-Host "  DECIMATOR v4.2 — INSTALLER" -ForegroundColor Red
Write-Host "  Roat Pkz PVP Assist Bot" -ForegroundColor Yellow
Write-Host "============================================" -ForegroundColor Red
Write-Host ""

# Check Python
if (-not (Test-Path $pyExe)) {
    Write-Host "[ERROR] Python not found at $pyExe" -ForegroundColor Red
    Write-Host "Install Python 3.14 first." -ForegroundColor Yellow
    pause
    exit 1
}
Write-Host "[OK] Python found at $pyExe" -ForegroundColor Green

# Create bot directory
if (-not (Test-Path $botDir)) {
    New-Item -ItemType Directory -Path $botDir -Force | Out-Null
}
Write-Host "[OK] Bot folder: $botDir" -ForegroundColor Green

# Download bot.py
Write-Host "[...] Downloading bot.py..." -ForegroundColor Cyan
try {
    Invoke-WebRequest -Uri $botUrl -OutFile "$botDir\bot.py" -UseBasicParsing
    Write-Host "[OK] bot.py downloaded" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Failed to download bot.py: $_" -ForegroundColor Red
    pause
    exit 1
}

# Install pip packages
Write-Host "[...] Installing Python packages..." -ForegroundColor Cyan
& $pyExe -m pip install --quiet pyautogui pillow pynput 2>&1 | Out-Null
Write-Host "[OK] Packages installed (pyautogui, pillow, pynput)" -ForegroundColor Green

# Create update.bat
$updateBat = @"
@echo off
echo ========================================
echo   DECIMATOR — Updating bot.py...
echo ========================================
powershell -Command "Invoke-WebRequest -Uri '$botUrl' -OutFile '$botDir\bot.py' -UseBasicParsing"
echo.
echo [DONE] bot.py updated!
echo.
pause
"@
Set-Content -Path "$botDir\update.bat" -Value $updateBat
# Also put update.bat on Desktop
$desktop = [Environment]::GetFolderPath("Desktop")
Set-Content -Path "$desktop\Decimator UPDATE.bat" -Value $updateBat
Write-Host "[OK] update.bat created" -ForegroundColor Green

# Create desktop shortcuts using .bat wrappers (more reliable than .lnk)
$runBat = @"
@echo off
cd /d $botDir
$pyExe bot.py
pause
"@
Set-Content -Path "$desktop\Decimator RUN.bat" -Value $runBat

$calBat = @"
@echo off
cd /d $botDir
$pyExe bot.py --calibrate
pause
"@
Set-Content -Path "$desktop\Decimator CALIBRATE.bat" -Value $calBat

$dbgBat = @"
@echo off
cd /d $botDir
$pyExe bot.py --debug
pause
"@
Set-Content -Path "$desktop\Decimator DEBUG.bat" -Value $dbgBat

Write-Host "[OK] Desktop shortcuts created" -ForegroundColor Green

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  INSTALL COMPLETE!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
Write-Host "Desktop shortcuts:" -ForegroundColor Yellow
Write-Host "  - Decimator RUN        (runs the bot)" -ForegroundColor White
Write-Host "  - Decimator CALIBRATE  (set up inventory grid)" -ForegroundColor White
Write-Host "  - Decimator DEBUG      (verify grid overlay)" -ForegroundColor White
Write-Host "  - Decimator UPDATE     (pull latest version)" -ForegroundColor White
Write-Host ""
Write-Host "Bot folder: $botDir" -ForegroundColor Cyan
Write-Host ""
Write-Host "FIRST TIME: Run CALIBRATE first, then DEBUG to verify, then RUN!" -ForegroundColor Yellow
Write-Host ""
pause
