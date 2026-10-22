# Build RetroMail and create a Windows installer (NSIS .exe) plus a portable .zip.
#
# Requirements: Qt 6.9+ for MSVC, Visual Studio 2022 (C++ workload), CMake, NSIS.
# Run from a "Developer PowerShell for VS 2022":
#
#   .\packaging\windows\make-installer.ps1 -QtDir C:\Qt\6.9.0\msvc2022_64
#
# Output: dist\RetroMail-<version>-Windows.exe and .zip
param(
    [string]$QtDir = $env:QT_DIR,
    [string]$BuildDir = "build-windows"
)
$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $Root

if (-not $QtDir) {
    Write-Error "Pass -QtDir (e.g. C:\Qt\6.9.0\msvc2022_64) or set QT_DIR."
}
if (-not (Get-Command makensis -ErrorAction SilentlyContinue) -and
    -not (Test-Path "${env:ProgramFiles(x86)}\NSIS\makensis.exe")) {
    Write-Warning "NSIS not found; only the .zip will be produced. Install it with: winget install NSIS.NSIS"
    $Generators = "ZIP"
} else {
    $Generators = "NSIS;ZIP"
}

Write-Host "==> Configuring with Qt from $QtDir"
cmake -S . -B $BuildDir `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DRETROMAIL_FETCH_QTKEYCHAIN=ON `
    -DRETROMAIL_BUILD_TESTS=OFF
if ($LASTEXITCODE) { exit $LASTEXITCODE }

Write-Host "==> Building"
cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE) { exit $LASTEXITCODE }

Write-Host "==> Packaging ($Generators)"
New-Item -ItemType Directory -Force -Path dist | Out-Null
Push-Location $BuildDir
cpack -G $Generators -C Release -B (Join-Path $Root "dist")
$code = $LASTEXITCODE
Pop-Location
Remove-Item -Recurse -Force (Join-Path $Root "dist\_CPack_Packages") -ErrorAction SilentlyContinue
if ($code) { exit $code }

Write-Host ""
Write-Host "Done. Files in dist\:"
Get-ChildItem dist | ForEach-Object { Write-Host "  $($_.Name)" }
