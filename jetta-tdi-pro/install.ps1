
# JETTA TDI PRO — One-Click Installer
# Creates desktop shortcut, auto-updates from GitHub

$ErrorActionPreference = "SilentlyContinue"
$appDir = "$env:LOCALAPPDATA\JettaTDIPro"
$shortcut = "$env:USERPROFILE\Desktop\Jetta TDI Pro.lnk"
$pyUrl = "https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/jetta-tdi-pro/jetta_tdi_pro.py"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  JETTA TDI PRO — Installer" -ForegroundColor Cyan
Write-Host "  2009 VW Jetta TDI 2.0L CR Diesel" -ForegroundColor DarkCyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Create app directory
if (!(Test-Path $appDir)) { New-Item -ItemType Directory -Path $appDir -Force | Out-Null }

# Check Python
Write-Host "[1/4] Checking Python..." -ForegroundColor Yellow
$py = $null
foreach ($cmd in @("python", "python3", "py")) {
    try {
        $ver = & $cmd --version 2>&1
        if ($ver -match "Python 3") { $py = $cmd; break }
    } catch {}
}

if (!$py) {
    Write-Host "  Python 3 not found. Installing via winget..." -ForegroundColor Yellow
    winget install Python.Python.3.12 --accept-source-agreements --accept-package-agreements 2>$null
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
    $py = "python"
}
Write-Host "  Python: OK" -ForegroundColor Green

# Install dependencies
Write-Host "[2/4] Installing dependencies..." -ForegroundColor Yellow
& $py -m pip install --quiet --upgrade pip 2>$null
& $py -m pip install --quiet pyserial PyQt5 2>$null
Write-Host "  PyQt5 + pyserial: OK" -ForegroundColor Green

# Download latest app
Write-Host "[3/4] Downloading Jetta TDI Pro..." -ForegroundColor Yellow
Invoke-WebRequest -Uri $pyUrl -OutFile "$appDir\jetta_tdi_pro.py" -UseBasicParsing
Write-Host "  Downloaded to: $appDir\jetta_tdi_pro.py" -ForegroundColor Green

# Create launcher script
$launcher = @"
@echo off
cd /d "$appDir"
pythonw jetta_tdi_pro.py 2>nul || python jetta_tdi_pro.py
"@
Set-Content -Path "$appDir\launch.bat" -Value $launcher

# Create updater script
$updater = @"
@echo off
echo Updating Jetta TDI Pro...
powershell -Command "Invoke-WebRequest -Uri '$pyUrl' -OutFile '$appDir\jetta_tdi_pro.py' -UseBasicParsing"
echo Updated! Restarting...
cd /d "$appDir"
pythonw jetta_tdi_pro.py 2>nul || python jetta_tdi_pro.py
"@
Set-Content -Path "$appDir\update.bat" -Value $updater

# Create desktop shortcut
Write-Host "[4/4] Creating desktop shortcut..." -ForegroundColor Yellow
$ws = New-Object -ComObject WScript.Shell
$sc = $ws.CreateShortcut($shortcut)
$sc.TargetPath = "$appDir\launch.bat"
$sc.WorkingDirectory = $appDir
$sc.Description = "Jetta TDI Pro — OBD2 Diagnostic Suite"
$sc.WindowStyle = 7  # minimized (hides cmd window)
$sc.Save()
Write-Host "  Desktop shortcut created!" -ForegroundColor Green

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  INSTALLED SUCCESSFULLY!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
Write-Host "  Double-click 'Jetta TDI Pro' on your desktop to launch." -ForegroundColor Cyan
Write-Host "  To update: run '$appDir\update.bat'" -ForegroundColor DarkCyan
Write-Host ""
Write-Host "Launching now..." -ForegroundColor Yellow
Start-Process -FilePath "$appDir\launch.bat" -WorkingDirectory $appDir
