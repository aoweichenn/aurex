#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
M0C="${BUILD_DIR}/m0c"
LEXER_FILE="${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax"
LEXER_BIN="${BUILD_DIR}/lexer_file"
COMPARE_DIR="${BUILD_DIR}/selfhost_lexer_compare"
SELFHOST_IMPORT_FLAGS=(-I "${ROOT}/selfhost/src")

cmake -S "${ROOT}" -B "${BUILD_DIR}" >/dev/null
cmake --build "${BUILD_DIR}" -j >/dev/null

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_FILE}" --runtime-c "${ROOT}/selfhost/runtime/runtime.c" -o "${LEXER_BIN}"

rm -rf "${COMPARE_DIR}"
mkdir -p "${COMPARE_DIR}"

compare_one() {
    local source="$1"
    local stem
    stem="$(echo "${source#${ROOT}/}" | tr '/.' '__')"
    local stage0="${COMPARE_DIR}/${stem}.stage0.kinds"
    local selfhost="${COMPARE_DIR}/${stem}.selfhost.kinds"

    "${M0C}" --dump-tokens "${source}" | awk '{ print $2 }' >"${stage0}"
    "${LEXER_BIN}" "${source}" >"${selfhost}"
    diff -u "${stage0}" "${selfhost}"
}

compare_one "${ROOT}/examples/hello.ax"
for source in "${ROOT}"/tests/positive/*.ax "${ROOT}"/tests/negative/*.ax; do
    compare_one "${source}"
done

echo "selfhost lexer matches Stage0 lexer for local corpus"
