# AoC v33 Deploy Script
# Fixes file conflicts and runs full UBT rebuild

$ErrorActionPreference = 'Stop'
$projectDir = "$env:USERPROFILE\Documents\Unreal Projects\AOC"
$sourceDir = "$projectDir\Source\AOC"
$repoDir = "$env:USERPROFILE\Downloads\Rotstrucking-app1"

Write-Host "=== AoC v33 Deploy Script ===" -ForegroundColor Cyan

# Step 1: Kill UE5 if running
Write-Host "[1/6] Killing UE5..." -ForegroundColor Yellow
Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue
Start-Sleep 2

# Step 2: Git pull latest
Write-Host "[2/6] Git pull..." -ForegroundColor Yellow
Set-Location $repoDir
git pull origin aoc-source-v14 2>&1

# Step 3: Fix AoCSkillComponent conflict - delete old version from Data/
Write-Host "[3/6] Fixing AoCSkillComponent conflict..." -ForegroundColor Yellow
$oldH = "$sourceDir\Data\AoCSkillComponent.h"
$oldCpp = "$sourceDir\Data\AoCSkillComponent.cpp"
if (Test-Path $oldH) { Remove-Item $oldH -Force; Write-Host "  Deleted old Data\AoCSkillComponent.h" }
if (Test-Path $oldCpp) { Remove-Item $oldCpp -Force; Write-Host "  Deleted old Data\AoCSkillComponent.cpp" }

# Rename -fixed versions in Skills/
$fixedH = "$sourceDir\Skills\AoCSkillComponent-fixed.h"
$fixedCpp = "$sourceDir\Skills\AoCSkillComponent-fixed.cpp"
$newH = "$sourceDir\Skills\AoCSkillComponent.h"
$newCpp = "$sourceDir\Skills\AoCSkillComponent.cpp"
if (Test-Path $fixedH) { 
    if (Test-Path $newH) { Remove-Item $newH -Force }
    Rename-Item $fixedH -NewName "AoCSkillComponent.h" -Force
    Write-Host "  Renamed Skills\AoCSkillComponent-fixed.h -> AoCSkillComponent.h"
}
if (Test-Path $fixedCpp) {
    if (Test-Path $newCpp) { Remove-Item $newCpp -Force }
    Rename-Item $fixedCpp -NewName "AoCSkillComponent.cpp" -Force
    Write-Host "  Renamed Skills\AoCSkillComponent-fixed.cpp -> AoCSkillComponent.cpp"
}

# Step 4: Copy all source files from repo to UE5 project
Write-Host "[4/6] Copying source files..." -ForegroundColor Yellow
$repoSource = "$repoDir\Source\AOC"
if (Test-Path $repoSource) {
    # Copy all subdirectories
    Get-ChildItem $repoSource -Recurse | ForEach-Object {
        $relativePath = $_.FullName.Substring($repoSource.Length)
        $destPath = "$sourceDir$relativePath"
        if ($_.PSIsContainer) {
            if (-not (Test-Path $destPath)) { New-Item -ItemType Directory -Path $destPath -Force | Out-Null }
        } else {
            $destDir = Split-Path $destPath -Parent
            if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }
            Copy-Item $_.FullName $destPath -Force
        }
    }
    Write-Host "  Source files synced"
}

# Copy Build.cs
$buildCs = "$repoDir\Source\AOC\AOC.Build.cs" 
if (Test-Path $buildCs) {
    Copy-Item $buildCs "$sourceDir\AOC.Build.cs" -Force
    Write-Host "  Build.cs updated"
}

# Step 5: Delete intermediates to force clean rebuild
Write-Host "[5/6] Cleaning intermediates..." -ForegroundColor Yellow
$intermediateDir = "$projectDir\Intermediate\Build\Win64\AOCEditor"
if (Test-Path $intermediateDir) {
    Remove-Item $intermediateDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "  Intermediate build files cleaned"
}

# Step 6: Run full UBT rebuild
Write-Host "[6/6] Running full UBT rebuild..." -ForegroundColor Yellow
$buildBat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"
& $buildBat AOCEditor Win64 Development "$projectDir\AOC.uproject"

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=== BUILD SUCCEEDED ===" -ForegroundColor Green
    Write-Host "Starting UE5 Editor..." -ForegroundColor Cyan
    Start-Process "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"$projectDir\AOC.uproject`""
} else {
    Write-Host ""
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host "Check errors above" -ForegroundColor Red
}
