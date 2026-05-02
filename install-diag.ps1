# Peterbilt 379 Diagnostic Suite v1.0 - Automated Installer
# Downloads from GitHub (no Google Drive issues)

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "  Peterbilt 379 Diagnostic Suite v1.0" -ForegroundColor Cyan
Write-Host "  Automated Installer" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Find Python
Write-Host "[1/5] Looking for Python..." -ForegroundColor Yellow
$pythonCmd = $null

# Check common commands
foreach ($cmd in @('python', 'python3', 'py')) {
    try {
        $ver = & $cmd --version 2>&1
        if ($ver -match 'Python 3') {
            $pythonCmd = $cmd
            Write-Host "Found: $ver at $cmd" -ForegroundColor Green
            break
        }
    } catch { }
}

# Check common install paths if not found
if (-not $pythonCmd) {
    $searchPaths = @(
        "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python*\python.exe",
        "C:\Python3*\python.exe",
        "C:\Python*\python.exe",
        "$env:APPDATA\Python\Python*\python.exe",
        "$env:USERPROFILE\AppData\Local\Microsoft\WindowsApps\python*.exe"
    )
    foreach ($pattern in $searchPaths) {
        $found = Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            $pythonCmd = $found.FullName
            $ver = & $pythonCmd --version 2>&1
            Write-Host "Found: $ver at $pythonCmd" -ForegroundColor Green
            break
        }
    }
}

if (-not $pythonCmd) {
    Write-Host "ERROR: Python 3 not found!" -ForegroundColor Red
    Write-Host "Download from: https://www.python.org/downloads/" -ForegroundColor Yellow
    Write-Host "IMPORTANT: Check 'Add Python to PATH' during install!" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# Step 2: Set up install directory
Write-Host "[2/5] Setting up install directory..." -ForegroundColor Yellow
$installDir = "$env:USERPROFILE\Desktop\Peterbilt_Diagnostic"
if (Test-Path $installDir) {
    Remove-Item -Recurse -Force $installDir
}
New-Item -ItemType Directory -Path $installDir -Force | Out-Null
Write-Host "Created: $installDir" -ForegroundColor Green

# Step 3: Download zip from GitHub
Write-Host "[3/5] Downloading diagnostic suite..." -ForegroundColor Yellow
$zipUrl = "https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/Peterbilt379_Diagnostic_v1.0.zip"
$zipPath = "$installDir\diag.zip"

try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing
    $fileSize = (Get-Item $zipPath).Length
    Write-Host "Downloaded: $([math]::Round($fileSize/1024, 1)) KB" -ForegroundColor Green
    
    if ($fileSize -lt 1000) {
        Write-Host "ERROR: Download too small - may be corrupted" -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }
} catch {
    Write-Host "ERROR: Download failed - $($_.Exception.Message)" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

# Step 4: Extract
Write-Host "[4/5] Extracting and installing dependencies..." -ForegroundColor Yellow
try {
    Expand-Archive -Path $zipPath -DestinationPath $installDir -Force
    Remove-Item $zipPath -Force
    Write-Host "Extracted successfully!" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Extraction failed - $($_.Exception.Message)" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

# Find the actual directory (may be nested)
$mainPy = Get-ChildItem -Path $installDir -Recurse -Filter "main.py" | Select-Object -First 1
if ($mainPy) {
    $appDir = $mainPy.DirectoryName
} else {
    $appDir = $installDir
}

# Step 5: Install Python dependencies
Write-Host "[5/5] Installing PyQt5 and pyserial..." -ForegroundColor Yellow
& $pythonCmd -m pip install --upgrade pip 2>&1 | Out-Null
& $pythonCmd -m pip install PyQt5 pyserial 2>&1

Write-Host ""
Write-Host "======================================" -ForegroundColor Green
Write-Host "  INSTALLATION COMPLETE!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""
Write-Host "Launching Peterbilt 379 Diagnostic Suite..." -ForegroundColor Cyan
Write-Host "(Close this window to stop the program)" -ForegroundColor Gray
Write-Host ""

Set-Location $appDir
& $pythonCmd main.py
