#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BASE_DIR}/build_tests"

echo "=== Configuring and Building Test Suite ==="
cmake -S "${BASE_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${BUILD_DIR}" --target ob_tests

TEST_EXE="${BUILD_DIR}/ob_tests"
if [ ! -f "${TEST_EXE}" ]; then
  TEST_EXE="${BUILD_DIR}/ob_tests.exe"
fi

echo ""
echo "=== Running Tests ==="
"${TEST_EXE}" "$@"

