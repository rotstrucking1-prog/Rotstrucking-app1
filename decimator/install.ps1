$ErrorActionPreference = "Continue"
$botDir = "C:\RoatBot"
$pyExe = "C:\Python314\python.exe"
$botUrl = "https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/decimator/bot.py"

Write-Host ""
Write-Host "============================================" -ForegroundColor Red
Write-Host "  DECIMATOR v4.2 INSTALLER" -ForegroundColor Red
Write-Host "============================================" -ForegroundColor Red
Write-Host ""

if (-not (Test-Path $pyExe)) {
    Write-Host "[ERROR] Python not found at $pyExe" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}
Write-Host "[OK] Python found" -ForegroundColor Green

if (-not (Test-Path $botDir)) {
    New-Item -ItemType Directory -Path $botDir -Force | Out-Null
}
Write-Host "[OK] Bot folder: $botDir" -ForegroundColor Green

Write-Host "[...] Downloading bot.py..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $botUrl -OutFile "$botDir\bot.py" -UseBasicParsing
Write-Host "[OK] bot.py downloaded" -ForegroundColor Green

Write-Host "[...] Installing Python packages..." -ForegroundColor Cyan
& $pyExe -m pip install pyautogui pillow pynput 2>&1 | Out-Null
Write-Host "[OK] Packages installed" -ForegroundColor Green

$desktop = [Environment]::GetFolderPath("Desktop")

Set-Content -Path "$desktop\Decimator RUN.bat" -Value "@echo off`r`ncd /d C:\RoatBot`r`nC:\Python314\python.exe bot.py`r`npause"
Set-Content -Path "$desktop\Decimator CALIBRATE.bat" -Value "@echo off`r`ncd /d C:\RoatBot`r`nC:\Python314\python.exe bot.py --calibrate`r`npause"
Set-Content -Path "$desktop\Decimator DEBUG.bat" -Value "@echo off`r`ncd /d C:\RoatBot`r`nC:\Python314\python.exe bot.py --debug`r`npause"
Set-Content -Path "$desktop\Decimator UPDATE.bat" -Value "@echo off`r`necho Updating Decimator...`r`npowershell -Command ""Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/decimator/bot.py' -OutFile 'C:\RoatBot\bot.py' -UseBasicParsing""`r`necho Done!`r`npause"
Set-Content -Path "$botDir\update.bat" -Value "@echo off`r`necho Updating Decimator...`r`npowershell -Command ""Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/decimator/bot.py' -OutFile 'C:\RoatBot\bot.py' -UseBasicParsing""`r`necho Done!`r`npause"

Write-Host "[OK] Desktop shortcuts created" -ForegroundColor Green
Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  INSTALL COMPLETE!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
Write-Host "Desktop shortcuts:" -ForegroundColor Yellow
Write-Host "  Decimator RUN        - runs the bot" -ForegroundColor White
Write-Host "  Decimator CALIBRATE  - set up inventory grid" -ForegroundColor White
Write-Host "  Decimator DEBUG      - verify grid overlay" -ForegroundColor White
Write-Host "  Decimator UPDATE     - pull latest version" -ForegroundColor White
Write-Host ""
Write-Host "FIRST: Run CALIBRATE, then DEBUG, then RUN" -ForegroundColor Yellow
Write-Host ""
Read-Host "Press Enter to exit"
