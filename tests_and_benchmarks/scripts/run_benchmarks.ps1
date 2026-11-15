# run_benchmarks.ps1: Build and execute throughput and tail-latency benchmarks
param(
    [string]$Baseline = "",
    [string]$Filter = "",
    [int]$Repetitions = 1,
    [int]$Cpu = -1
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$BaseDir = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $BaseDir "build_bench"

Write-Host "=== 1. Building Benchmarks (Release Mode) ===" -ForegroundColor Cyan
cmake -S "$BaseDir" -B "$BuildDir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BuildDir" --target ob_bench ob_latency --config Release

$BenchExe = Join-Path $BuildDir "ob_bench.exe"
if (-not (Test-Path $BenchExe)) { $BenchExe = Join-Path $BuildDir "Release\ob_bench.exe" }
if (-not (Test-Path $BenchExe)) { $BenchExe = Join-Path $BuildDir "ob_bench" }

$LatencyExe = Join-Path $BuildDir "ob_latency.exe"
if (-not (Test-Path $LatencyExe)) { $LatencyExe = Join-Path $BuildDir "Release\ob_latency.exe" }
if (-not (Test-Path $LatencyExe)) { $LatencyExe = Join-Path $BuildDir "ob_latency" }

$DateStr = Get-Date -Format "yyyyMMdd_HHmmss"
$Commit = "unknown"
try {
    $Commit = (git rev-parse --short HEAD).Trim()
} catch {}
$OutDir = Join-Path $BaseDir "results\${DateStr}_${Commit}"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Write-Host "`n=== 2. Capturing Environment Metadata ===" -ForegroundColor Cyan
python "$ScriptDir\summarize.py" meta --build-dir "$BuildDir" --out "$OutDir\meta.json"

Write-Host "`n=== 3. Running Throughput Benchmarks ===" -ForegroundColor Cyan
$BenchArgs = @("--benchmark_out=$OutDir\gbench.json", "--benchmark_out_format=json", "--benchmark_repetitions=$Repetitions")
if ($Filter -ne "") {
    $BenchArgs += "--benchmark_filter=$Filter"
}
& $BenchExe $BenchArgs

Write-Host "`n=== 4. Running Per-Operation Tail-Latency Harness ===" -ForegroundColor Cyan
$LatArgs = @("--json=$OutDir\latency.json")
if ($Cpu -ge 0) {
    $LatArgs += "--cpu=$Cpu"
}
& $LatencyExe $LatArgs

Write-Host "`n=== 5. Generating Benchmark Report ===" -ForegroundColor Cyan
$ReportArgs = @("report", "$OutDir")
if ($Baseline -ne "") {
    $ReportArgs += @("--baseline", "$Baseline")
}
python "$ScriptDir\summarize.py" @ReportArgs

Write-Host "`n========================================================" -ForegroundColor Green
Write-Host " Benchmark run complete!" -ForegroundColor Green
Write-Host " Report generated: $OutDir\REPORT.md" -ForegroundColor Green
Write-Host " (Submit this REPORT.md file)" -ForegroundColor Green
Write-Host "========================================================" -ForegroundColor Green
