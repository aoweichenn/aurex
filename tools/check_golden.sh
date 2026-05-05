#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
AUREXC="${BUILD_DIR}/bin/aurexc"
GOLDEN_DIR="${BUILD_DIR}/golden"

cmake -S "${ROOT}" -B "${BUILD_DIR}" >/dev/null
cmake --build "${BUILD_DIR}" -j >/dev/null
mkdir -p "${GOLDEN_DIR}"

TMP_TOKENS="${GOLDEN_DIR}/hello.tokens"
"${AUREXC}" --dump-tokens "${ROOT}/examples/hello.ax" >"${TMP_TOKENS}"
diff -u "${ROOT}/tests/samples/golden/hello.tokens" "${TMP_TOKENS}"

echo "golden tests passed"
