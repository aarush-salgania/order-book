# run_tests.ps1: Build and execute the test suite
param(
    [switch]$KnownBugs,
    [string]$Filter = "",
    [string]$BuildType = "Debug"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$BaseDir = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $BaseDir "build_tests"

Write-Host "=== Configuring and Building Test Suite ($BuildType) ===" -ForegroundColor Cyan
cmake -S "$BaseDir" -B "$BuildDir" "-DCMAKE_BUILD_TYPE=$BuildType"
cmake --build "$BuildDir" --target ob_tests --config "$BuildType"

$TestExe = Join-Path $BuildDir "ob_tests.exe"
if (-not (Test-Path $TestExe)) {
    $TestExe = Join-Path $BuildDir "$BuildType\ob_tests.exe"
}
if (-not (Test-Path $TestExe)) {
    $TestExe = Join-Path $BuildDir "ob_tests"
}

$ArgsList = @()
if ($KnownBugs) {
    $ArgsList += "--known-bugs"
}
if ($Filter -ne "") {
    $ArgsList += "--gtest_filter=$Filter"
}

Write-Host "`n=== Running Tests ===" -ForegroundColor Cyan
& $TestExe $ArgsList
exit $LASTEXITCODE
