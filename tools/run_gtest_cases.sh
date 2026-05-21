#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${AUREX_BUILD_DIR:-${ROOT}/build/full-llvm}"

cmake -S "${ROOT}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j
exec "${BUILD_DIR}/bin/aurex_tests" --gtest_color=auto --gtest_print_time=1 "$@"
