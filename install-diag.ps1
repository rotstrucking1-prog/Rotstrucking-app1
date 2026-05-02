# Peterbilt 379 Diagnostic Suite v1.0 - Auto Installer
Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Peterbilt 379 Diagnostic Suite v1.0" -ForegroundColor Cyan
Write-Host "  Automated Installer" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Find Python
Write-Host "[1/5] Looking for Python..." -ForegroundColor Yellow
$pythonCmd = $null

$searchPaths = @(
    "python",
    "python3",
    "py",
    "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python310\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python39\python.exe",
    "C:\Python312\python.exe",
    "C:\Python311\python.exe",
    "C:\Python310\python.exe",
    "$env:USERPROFILE\AppData\Local\Microsoft\WindowsApps\python.exe"
)

foreach ($p in $searchPaths) {
    try {
        $ver = & $p --version 2>&1
        if ($ver -match "Python \d") {
            $pythonCmd = $p
            Write-Host "  Found: $ver at $p" -ForegroundColor Green
            break
        }
    } catch { }
}

if (-not $pythonCmd) {
    Write-Host "  Python NOT found! Installing automatically..." -ForegroundColor Red
    $pyUrl = "https://www.python.org/ftp/python/3.12.4/python-3.12.4-amd64.exe"
    $pyInstaller = "$env:TEMP\python-installer.exe"
    Write-Host "  Downloading Python 3.12..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $pyUrl -OutFile $pyInstaller -UseBasicParsing
    Write-Host "  Installing (this may take a minute)..." -ForegroundColor Yellow
    Start-Process -FilePath $pyInstaller -ArgumentList "/quiet","InstallAllUsers=0","PrependPath=1","Include_pip=1" -Wait
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
    foreach ($p in $searchPaths) {
        try {
            $ver = & $p --version 2>&1
            if ($ver -match "Python \d") { $pythonCmd = $p; Write-Host "  Installed: $ver" -ForegroundColor Green; break }
        } catch { }
    }
    if (-not $pythonCmd) { Write-Host "  ERROR: Install failed. Get Python from python.org" -ForegroundColor Red; Read-Host "Press Enter"; exit 1 }
}

# Step 2: Setup directory
Write-Host "[2/5] Setting up install directory..." -ForegroundColor Yellow
$installDir = "$env:USERPROFILE\Desktop\Peterbilt_Diagnostic"
if (Test-Path $installDir) { Remove-Item -Recurse -Force $installDir }
New-Item -ItemType Directory -Path $installDir -Force | Out-Null
Write-Host "  Created: $installDir" -ForegroundColor Green

# Step 3: Download from Google Drive
Write-Host "[3/5] Downloading diagnostic suite..." -ForegroundColor Yellow
$fileId = "1zSVPwAUCmuLXbgZVh5MD0SjdW0OJek3T"
$zipPath = "$env:TEMP\peterbilt_diag.zip"
$downloadUrl = "https://drive.google.com/uc?export=download&id=$fileId"
try {
    Invoke-WebRequest -Uri $downloadUrl -OutFile $zipPath -UseBasicParsing
    Write-Host "  Downloaded successfully" -ForegroundColor Green
} catch {
    $confirmUrl = "https://drive.google.com/uc?export=download&confirm=t&id=$fileId"
    Invoke-WebRequest -Uri $confirmUrl -OutFile $zipPath -UseBasicParsing
    Write-Host "  Downloaded (alternate method)" -ForegroundColor Green
}

# Step 4: Extract and install deps
Write-Host "[4/5] Extracting and installing dependencies..." -ForegroundColor Yellow
Expand-Archive -Path $zipPath -DestinationPath $installDir -Force
$mainPy = Get-ChildItem -Path $installDir -Recurse -Filter "main.py" | Select-Object -First 1
if ($mainPy) { $appDir = $mainPy.DirectoryName } else { $appDir = $installDir }
Write-Host "  Installing PyQt5 and pyserial..." -ForegroundColor Yellow
& $pythonCmd -m pip install PyQt5 pyserial 2>&1 | Out-Null
Write-Host "  Dependencies installed" -ForegroundColor Green

# Step 5: Create shortcut and launch
Write-Host "[5/5] Creating desktop shortcut..." -ForegroundColor Yellow
$bat = "@echo off`r`ncd /d `"$appDir`"`r`n`"$pythonCmd`" main.py`r`npause"
Set-Content -Path "$env:USERPROFILE\Desktop\Run_Peterbilt_Diag.bat" -Value $bat
Write-Host "  Shortcut: Run_Peterbilt_Diag.bat on Desktop" -ForegroundColor Green

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Installation Complete! Launching..." -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
Set-Location $appDir
& $pythonCmd main.py
