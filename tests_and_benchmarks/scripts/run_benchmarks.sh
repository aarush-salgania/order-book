#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BASE_DIR}/build_bench"

echo "=== 1. Building Benchmarks (Release Mode) ==="
cmake -S "${BASE_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target ob_bench ob_latency

BENCH_EXE="${BUILD_DIR}/ob_bench"
if [ ! -f "${BENCH_EXE}" ]; then BENCH_EXE="${BUILD_DIR}/ob_bench.exe"; fi

LATENCY_EXE="${BUILD_DIR}/ob_latency"
if [ ! -f "${LATENCY_EXE}" ]; then LATENCY_EXE="${BUILD_DIR}/ob_latency.exe"; fi

DATETIME=$(date +"%Y%m%d_%H%M%S")
COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
OUT_DIR="${BASE_DIR}/results/${DATETIME}_${COMMIT}"
mkdir -p "${OUT_DIR}"

echo ""
echo "=== 2. Capturing Environment Metadata ==="
python3 "${SCRIPT_DIR}/summarize.py" meta --build-dir "${BUILD_DIR}" --out "${OUT_DIR}/meta.json"

echo ""
echo "=== 3. Running Throughput Benchmarks ==="
"${BENCH_EXE}" --benchmark_out="${OUT_DIR}/gbench.json" --benchmark_out_format=json "$@"

echo ""
echo "=== 4. Running Per-Operation Tail-Latency Harness ==="
"${LATENCY_EXE}" --json="${OUT_DIR}/latency.json"

echo ""
echo "=== 5. Generating Benchmark Report ==="
python3 "${SCRIPT_DIR}/summarize.py" report "${OUT_DIR}"

echo ""
echo "========================================================"
echo " Benchmark run complete!"
echo " Report generated: ${OUT_DIR}/REPORT.md"
echo " (Submit this REPORT.md file)"
echo "========================================================"

