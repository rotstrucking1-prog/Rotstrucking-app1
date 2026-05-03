Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  ROTS Trucking - Jetta TDI OBD2 Dashboard" -ForegroundColor Green
Write-Host "  v2 - Confirmed PIDs Only (May 2 2026)" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""

$installDir = "$env:USERPROFILE\JettaOBD2"
$dashFile = "$installDir\obd2_dashboard.py"
$shortcut = "$env:USERPROFILE\Desktop\Jetta OBD2 Dashboard.lnk"

# Create folder
if (!(Test-Path $installDir)) { New-Item -ItemType Directory -Path $installDir -Force | Out-Null }

# Download dashboard
Write-Host "Downloading dashboard (confirmed PIDs only)..." -ForegroundColor Yellow
$url = "https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/jetta-obd2/obd2_dashboard.py"
try {
    Invoke-WebRequest -Uri $url -OutFile $dashFile -UseBasicParsing
    Write-Host "  Downloaded!" -ForegroundColor Green
} catch {
    Write-Host "  Download failed: $_" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

# Find Python
$pythonCmd = $null
foreach ($cmd in @("python", "python3", "py")) {
    try {
        $ver = & $cmd --version 2>&1
        if ($ver -match "Python 3") { $pythonCmd = $cmd; break }
    } catch {}
}
if (!$pythonCmd) {
    $paths = @(
        "$env:LOCALAPPDATA\Programs\Python\Python*\python.exe",
        "$env:APPDATA\Python\Python*\python.exe",
        "C:\Python3*\python.exe",
        "$env:USERPROFILE\AppData\Local\Microsoft\WindowsApps\python.exe"
    )
    foreach ($pattern in $paths) {
        $found = Get-ChildItem $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $pythonCmd = $found.FullName; break }
    }
}
if (!$pythonCmd) {
    Write-Host "Python 3 not found! Install from python.org" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}
Write-Host "Python: $pythonCmd" -ForegroundColor Cyan

# Install dependencies
Write-Host "Installing pyserial + PyQt5..." -ForegroundColor Yellow
& $pythonCmd -m pip install pyserial PyQt5 --quiet 2>&1 | Out-Null
Write-Host "  Done!" -ForegroundColor Green

# Desktop shortcut
try {
    $ws = New-Object -ComObject WScript.Shell
    $sc = $ws.CreateShortcut($shortcut)
    $sc.TargetPath = $pythonCmd
    $sc.Arguments = "`"$dashFile`""
    $sc.WorkingDirectory = $installDir
    $sc.Description = "Jetta TDI OBD2 Dashboard"
    $sc.Save()
    Write-Host "Desktop shortcut created!" -ForegroundColor Green
} catch {
    Write-Host "  Shortcut failed (run manually)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  INSTALLED! Launching dashboard..." -ForegroundColor Green
Write-Host "  12 confirmed gauges - no more N/A" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Green
Write-Host ""

Start-Process $pythonCmd $dashFile
