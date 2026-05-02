$ErrorActionPreference = "SilentlyContinue"
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  OBD2 Dashboard - 2009 VW Jetta TDI" -ForegroundColor Cyan
Write-Host "  Installer for Vgate vLinker FS" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$installDir = "$env:USERPROFILE\OBD2_Dashboard"
if (!(Test-Path $installDir)) { New-Item -ItemType Directory -Path $installDir -Force | Out-Null }

Write-Host "[1/4] Downloading dashboard..." -ForegroundColor Yellow
$url = "https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/obd2_dashboard.py"
try {
    Invoke-WebRequest -Uri $url -OutFile "$installDir\obd2_dashboard.py" -UseBasicParsing
    Write-Host "  Downloaded!" -ForegroundColor Green
} catch {
    Write-Host "  Download failed: $_" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "[2/4] Checking Python..." -ForegroundColor Yellow
$python = $null
$tryPaths = @("python", "python3", "py",
    "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python310\python.exe",
    "C:\Python312\python.exe", "C:\Python311\python.exe", "C:\Python310\python.exe")
foreach ($p in $tryPaths) {
    try {
        $ver = & $p --version 2>&1
        if ($ver -match "Python 3") { $python = $p; break }
    } catch {}
}
if ($python) {
    Write-Host "  Found: $python" -ForegroundColor Green
} else {
    Write-Host "  Python 3 not found! Installing..." -ForegroundColor Yellow
    $pyUrl = "https://www.python.org/ftp/python/3.12.4/python-3.12.4-amd64.exe"
    Invoke-WebRequest -Uri $pyUrl -OutFile "$env:TEMP\python_installer.exe" -UseBasicParsing
    Start-Process -FilePath "$env:TEMP\python_installer.exe" -ArgumentList "/quiet InstallAllUsers=1 PrependPath=1" -Wait
    $python = "python"
}

Write-Host "[3/4] Installing dependencies..." -ForegroundColor Yellow
& $python -m pip install --upgrade pip 2>&1 | Out-Null
& $python -m pip install pyserial PyQt5 2>&1
Write-Host "  Dependencies installed!" -ForegroundColor Green

Write-Host "[4/4] Creating desktop shortcut..." -ForegroundColor Yellow
$shortcutPath = "$env:USERPROFILE\Desktop\OBD2 Dashboard.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $python
$shortcut.Arguments = "`"$installDir\obd2_dashboard.py`""
$shortcut.WorkingDirectory = $installDir
$shortcut.Description = "OBD2 Dashboard - 2009 VW Jetta TDI"
$shortcut.Save()
Write-Host "  Desktop shortcut created!" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  INSTALLATION COMPLETE!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Double-click 'OBD2 Dashboard' on your desktop to launch." -ForegroundColor Cyan
Write-Host "Make sure:" -ForegroundColor Yellow
Write-Host "  1. Vgate adapter is plugged into laptop USB" -ForegroundColor White
Write-Host "  2. Vgate is plugged into Jetta OBD2 port" -ForegroundColor White
Write-Host "  3. Ignition is ON (engine can be off or running)" -ForegroundColor White
Write-Host ""

Write-Host "Launch now? (Y/N): " -ForegroundColor Cyan -NoNewline
$answer = Read-Host
if ($answer -eq "Y" -or $answer -eq "y") {
    & $python "$installDir\obd2_dashboard.py"
}
