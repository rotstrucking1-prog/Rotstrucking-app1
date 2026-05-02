# ROTS Trucking - OBD2 Dashboard Auto-Installer
# Paste this into PowerShell (Run as Administrator)

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  ROTS TRUCKING - OBD2 Dashboard Installer" -ForegroundColor Cyan  
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

$installDir = "$env:USERPROFILE\ROTS_OBD2"

# Create directory
if (!(Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

Write-Host "[1/4] Downloading OBD2 Dashboard..." -ForegroundColor Yellow
$url = "https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/obd2/obd2_dashboard.py"
Invoke-WebRequest -Uri $url -OutFile "$installDir\obd2_dashboard.py" -UseBasicParsing
Write-Host "  Downloaded!" -ForegroundColor Green

Write-Host "[2/4] Checking Python..." -ForegroundColor Yellow
$pythonPaths = @(
    "python",
    "python3",
    "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python310\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python39\python.exe",
    "C:\Python312\python.exe",
    "C:\Python311\python.exe",
    "C:\Python310\python.exe",
    "C:\Python39\python.exe"
)

$pythonExe = $null
foreach ($p in $pythonPaths) {
    try {
        $ver = & $p --version 2>&1
        if ($ver -match "Python 3") {
            $pythonExe = $p
            Write-Host "  Found: $ver at $p" -ForegroundColor Green
            break
        }
    } catch {}
}

if (-not $pythonExe) {
    Write-Host "  Python not found! Installing..." -ForegroundColor Yellow
    Write-Host "  Downloading Python 3.12..." -ForegroundColor Yellow
    $pyUrl = "https://www.python.org/ftp/python/3.12.4/python-3.12.4-amd64.exe"
    Invoke-WebRequest -Uri $pyUrl -OutFile "$env:TEMP\python_installer.exe" -UseBasicParsing
    Start-Process -FilePath "$env:TEMP\python_installer.exe" -ArgumentList "/quiet InstallAllUsers=0 PrependPath=1" -Wait
    $pythonExe = "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
    Write-Host "  Python installed!" -ForegroundColor Green
}

Write-Host "[3/4] Installing dependencies..." -ForegroundColor Yellow
& $pythonExe -m pip install --upgrade pip 2>&1 | Out-Null
& $pythonExe -m pip install PyQt5 pyserial 2>&1 | Out-Null
Write-Host "  PyQt5 + pyserial installed!" -ForegroundColor Green

Write-Host "[4/4] Creating desktop shortcut..." -ForegroundColor Yellow
$shortcutPath = "$env:USERPROFILE\Desktop\OBD2 Dashboard.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $pythonExe
$shortcut.Arguments = "`"$installDir\obd2_dashboard.py`""
$shortcut.WorkingDirectory = $installDir
$shortcut.Description = "ROTS Trucking OBD2 Dashboard"
$shortcut.Save()
Write-Host "  Desktop shortcut created!" -ForegroundColor Green

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  INSTALLATION COMPLETE!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
Write-Host "Double-click 'OBD2 Dashboard' on your desktop to launch!" -ForegroundColor Cyan
Write-Host ""
Write-Host "Starting dashboard now..." -ForegroundColor Yellow
Write-Host ""

Start-Process -FilePath $pythonExe -ArgumentList "`"$installDir\obd2_dashboard.py`""

Read-Host "Press Enter to close this window"
