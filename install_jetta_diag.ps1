Write-Host "`n====================================" -ForegroundColor Cyan
Write-Host "  ROTS Trucking - Jetta TDI Scanner" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan
$dir = "$env:USERPROFILE\Desktop\JettaDiag"
if (!(Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
Write-Host "`nDownloading scanner..." -ForegroundColor Yellow
$url = "https://raw.githubusercontent.com/rotstrucking1-prog/Rotstrucking-app1/main/jetta_diag_test.py"
Invoke-WebRequest -Uri $url -OutFile "$dir\jetta_diag_test.py" -UseBasicParsing
Write-Host "Installing pyserial..." -ForegroundColor Yellow
& pip install pyserial 2>$null
& pip3 install pyserial 2>$null
$pyPaths = @(
    "python", "python3", "py",
    "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python310\python.exe",
    "$env:APPDATA\Python\Python312\python.exe",
    "C:\Python312\python.exe",
    "C:\Python311\python.exe",
    "C:\Python310\python.exe"
)
$pyExe = $null
foreach ($p in $pyPaths) {
    try { $v = & $p --version 2>&1; if ($v -match "Python") { $pyExe = $p; break } } catch {}
}
if ($pyExe) {
    Write-Host "`nFound Python: $pyExe" -ForegroundColor Green
    Write-Host "Running scanner...`n" -ForegroundColor Green
    & $pyExe "$dir\jetta_diag_test.py"
} else {
    Write-Host "`nPython not found! Install from python.org first." -ForegroundColor Red
    pause
}
