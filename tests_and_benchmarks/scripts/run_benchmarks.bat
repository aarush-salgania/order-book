@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "BASE_DIR=%SCRIPT_DIR%.."
set "BUILD_DIR=%BASE_DIR%\build_bench"

echo === 1. Building Benchmarks (Release Mode) ===
cmake -S "%BASE_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake --build "%BUILD_DIR%" --target ob_bench ob_latency --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

set "BENCH_EXE=%BUILD_DIR%\ob_bench.exe"
if not exist "%BENCH_EXE%" set "BENCH_EXE=%BUILD_DIR%\Release\ob_bench.exe"

set "LATENCY_EXE=%BUILD_DIR%\ob_latency.exe"
if not exist "%LATENCY_EXE%" set "LATENCY_EXE=%BUILD_DIR%\Release\ob_latency.exe"

for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set dt=%%I
set "DATETIME=%dt:~0,8%_%dt:~8,6%"

for /f %%I in ('git rev-parse --short HEAD 2^>nul') do set "COMMIT=%%I"
if "%COMMIT%"=="" set "COMMIT=unknown"

set "OUT_DIR=%BASE_DIR%\results\%DATETIME%_%COMMIT%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo.
echo === 2. Capturing Environment Metadata ===
python "%SCRIPT_DIR%summarize.py" meta --build-dir "%BUILD_DIR%" --out "%OUT_DIR%\meta.json"

echo.
echo === 3. Running Throughput Benchmarks ===
"%BENCH_EXE%" --benchmark_out="%OUT_DIR%\gbench.json" --benchmark_out_format=json %*

echo.
echo === 4. Running Per-Operation Tail-Latency Harness ===
"%LATENCY_EXE%" --json="%OUT_DIR%\latency.json"

echo.
echo === 5. Generating Benchmark Report ===
python "%SCRIPT_DIR%summarize.py" report "%OUT_DIR%"

echo.
echo ========================================================
echo  Benchmark run complete!
echo  Report generated: %OUT_DIR%\REPORT.md
echo  (Submit this REPORT.md file)
echo ========================================================

