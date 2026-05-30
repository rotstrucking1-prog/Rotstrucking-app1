# AoC v33 Deploy Script - Fixed error handling
$projectDir = 'C:\Users\Bradh\Documents\Unreal Projects\AOC'
$sourceDir = "$projectDir\Source\AOC"
$repoDir = 'C:\Users\Bradh\Downloads\Rotstrucking-app1'

Write-Host '=== AoC v33 Deploy ===' -ForegroundColor Cyan

Write-Host '[1/5] Killing UE5...'
Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue
Start-Sleep 2

Write-Host '[2/5] Deleting conflicting files...'
if (Test-Path "$sourceDir\Data\AoCSkillComponent.h") {
    Remove-Item "$sourceDir\Data\AoCSkillComponent.h" -Force
    Remove-Item "$sourceDir\Data\AoCSkillComponent.cpp" -Force
    Write-Host '  Old AoCSkillComponent deleted from Data'
}
$fixedH = "$sourceDir\Skills\AoCSkillComponent-fixed.h"
if (Test-Path $fixedH) {
    $newH = "$sourceDir\Skills\AoCSkillComponent.h"
    if (Test-Path $newH) { Remove-Item $newH -Force }
    Rename-Item $fixedH 'AoCSkillComponent.h' -Force
    Write-Host '  Renamed -fixed.h'
}
$fixedC = "$sourceDir\Skills\AoCSkillComponent-fixed.cpp"
if (Test-Path $fixedC) {
    $newC = "$sourceDir\Skills\AoCSkillComponent.cpp"
    if (Test-Path $newC) { Remove-Item $newC -Force }
    Rename-Item $fixedC 'AoCSkillComponent.cpp' -Force
    Write-Host '  Renamed -fixed.cpp'
}

# Clean duplicate HUD widget files (correct location is UI\Widgets\, not UI\)
$dupeHUD_h = "$sourceDir\UI\AoCHUDWidget.h"
$dupeHUD_cpp = "$sourceDir\UI\AoCHUDWidget.cpp"
if (Test-Path $dupeHUD_h) {
    Remove-Item $dupeHUD_h -Force
    Write-Host '  Deleted duplicate UI\AoCHUDWidget.h'
}
if (Test-Path $dupeHUD_cpp) {
    Remove-Item $dupeHUD_cpp -Force
    Write-Host '  Deleted duplicate UI\AoCHUDWidget.cpp'
}
# Clean duplicate ChatBubble from BOTH project AND repo clone (correct location is AI\, not NPC\)
$dupeCB_proj = "$sourceDir\NPC\AoCChatBubble.cpp"
$dupeCB_repo = "$repoDir\Source\AOC\NPC\AoCChatBubble.cpp"
if (Test-Path $dupeCB_proj) {
    Remove-Item $dupeCB_proj -Force
    Write-Host '  Deleted duplicate NPC\AoCChatBubble.cpp (project)'
}
if (Test-Path $dupeCB_repo) {
    Remove-Item $dupeCB_repo -Force
    Write-Host '  Deleted duplicate NPC\AoCChatBubble.cpp (repo clone)'
}
# Also delete repo clone UI duplicates to prevent re-copy
$dupeHUD_h_repo = "$repoDir\Source\AOC\UI\AoCHUDWidget.h"
$dupeHUD_cpp_repo = "$repoDir\Source\AOC\UI\AoCHUDWidget.cpp"
if (Test-Path $dupeHUD_h_repo) { Remove-Item $dupeHUD_h_repo -Force; Write-Host '  Deleted UI\AoCHUDWidget.h (repo clone)' }
if (Test-Path $dupeHUD_cpp_repo) { Remove-Item $dupeHUD_cpp_repo -Force; Write-Host '  Deleted UI\AoCHUDWidget.cpp (repo clone)' }

Write-Host '[3/5] Syncing source files...'
$repoSource = "$repoDir\Source\AOC"
Get-ChildItem $repoSource -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($repoSource.Length)
    $dest = "$sourceDir$rel"
    $dir = Split-Path $dest -Parent
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    Copy-Item $_.FullName $dest -Force
}
Write-Host '  Source files synced'

Write-Host '[4/5] Cleaning intermediates...'
$intDir = "$projectDir\Intermediate\Build\Win64\AOCEditor"
if (Test-Path $intDir) {
    Remove-Item $intDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host '  Cleaned'
}

Write-Host '[5/5] Building...'
$bat = 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat'
& $bat AOCEditor Win64 Development "$projectDir\AOC.uproject"

if ($LASTEXITCODE -eq 0) {
    Write-Host '=== BUILD SUCCEEDED ===' -ForegroundColor Green
    Start-Process 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList "`"$projectDir\AOC.uproject`""
} else {
    Write-Host '=== BUILD FAILED ===' -ForegroundColor Red
}
