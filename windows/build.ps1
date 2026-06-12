# build.ps1 — build all six pedals + the signed Windows installer.
# Run from the repo root on a Windows machine (e.g. Infinity):
#
#   powershell -ExecutionPolicy Bypass -File windows\build.ps1
#
# Prereqs:
#   - Visual Studio 2022 (Desktop development with C++)
#   - CMake >= 3.22 on PATH
#   - Node 18+ on PATH (plugin UIs)
#   - Inno Setup 6 (iscc on PATH or default install location)
#   - For signing: Azure Trusted Signing set up (see windows\README.md),
#     with these environment variables set:
#       $env:AZURE_CODESIGN_DLIB    -> path to Azure.CodeSigning.Dlib.dll
#       $env:AZURE_CODESIGN_META    -> path to your metadata.json
#     If unset, the installer is produced unsigned.

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$Plugins  = @("VROOM","DAYDREAM","FOFOPEDAL","DOUBLE","BACKPORCH","SWAY")
$BuildDir = "build-win"
$Version  = if ($env:VERSION) { $env:VERSION } else { "0.1.0" }

Write-Host "== 1/4 Building plugin UIs ==" -ForegroundColor Cyan
$uiDirs = @("ui","daydream/ui","fofopedal/ui","double/ui","backporch/ui","sway/ui")
foreach ($d in $uiDirs) {
    Push-Location $d
    if (-not (Test-Path "node_modules")) { npm install }
    npm run build
    Pop-Location
}

Write-Host "== 2/4 CMake build (Release x64) ==" -ForegroundColor Cyan
cmake -B $BuildDir -G "Visual Studio 17 2022" -A x64
foreach ($p in $Plugins) {
    cmake --build $BuildDir --target "${p}_VST3" --config Release
    if ($LASTEXITCODE -ne 0) { throw "build failed: $p" }
}

Write-Host "== 3/4 Staging + signing plugin binaries ==" -ForegroundColor Cyan
$Stage = Join-Path $RepoRoot "$BuildDir\installer-stage"
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Path $Stage | Out-Null

function Sign-File($path) {
    if ($env:AZURE_CODESIGN_DLIB -and $env:AZURE_CODESIGN_META) {
        & signtool sign /v /fd SHA256 /tr "http://timestamp.acs.microsoft.com" /td SHA256 `
            /dlib $env:AZURE_CODESIGN_DLIB /dmdf $env:AZURE_CODESIGN_META $path
        if ($LASTEXITCODE -ne 0) { throw "signing failed: $path" }
    }
}

foreach ($p in $Plugins) {
    $src = "$BuildDir\${p}_artefacts\Release\VST3\${p}.vst3"
    if (-not (Test-Path $src)) { throw "missing artefact: $src" }
    Copy-Item -Recurse $src $Stage
    # VST3 bundles are folders; the actual binary lives inside. Signing the
    # plugin DLL is optional but free once signing works — do it.
    Get-ChildItem -Recurse "$Stage\${p}.vst3" -Filter "*.vst3" -File | ForEach-Object { Sign-File $_.FullName }
}

Write-Host "== 4/4 Building installer ==" -ForegroundColor Cyan
$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) { $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
if (-not (Test-Path $iscc)) { throw "Inno Setup 6 not found — install from jrsoftware.org" }

& $iscc "/DAppVersion=$Version" "/DStageDir=$Stage" "$PSScriptRoot\installer.iss"
if ($LASTEXITCODE -ne 0) { throw "iscc failed" }

$Setup = Join-Path $RepoRoot "dist\SweetPapaPedals-Setup-$Version.exe"
Sign-File $Setup

Write-Host ""
Write-Host "done -> $Setup" -ForegroundColor Green
if (-not ($env:AZURE_CODESIGN_DLIB -and $env:AZURE_CODESIGN_META)) {
    Write-Host "note: UNSIGNED build (SmartScreen will warn users)." -ForegroundColor Yellow
    Write-Host "      Set AZURE_CODESIGN_DLIB + AZURE_CODESIGN_META and re-run. See windows\README.md."
}
