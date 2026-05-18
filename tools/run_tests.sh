#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
CTEST_JOBS="${AUREX_CTEST_JOBS:-}"
CC="${CC:-clang}"
CXX="${CXX:-clang++}"

if [[ -z "${CTEST_JOBS}" ]]; then
    if command -v nproc >/dev/null 2>&1; then
        CTEST_JOBS="$(nproc 2>/dev/null || true)"
    elif command -v getconf >/dev/null 2>&1; then
        CTEST_JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
    elif command -v sysctl >/dev/null 2>&1; then
        CTEST_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || true)"
    fi
fi

if [[ -z "${CTEST_JOBS}" || ! "${CTEST_JOBS}" =~ ^[0-9]+$ || "${CTEST_JOBS}" -lt 1 ]]; then
    CTEST_JOBS="2"
elif [[ -z "${AUREX_CTEST_JOBS:-}" && "${CTEST_JOBS}" -gt 32 ]]; then
    CTEST_JOBS="32"
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}"
cmake --build "${BUILD_DIR}" -j
ctest --test-dir "${BUILD_DIR}" --parallel "${CTEST_JOBS}" --output-on-failure "$@"
