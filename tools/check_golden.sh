#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
AUREXC="${BUILD_DIR}/bin/aurexc"
GOLDEN_DIR="${BUILD_DIR}/golden"
SELFHOST_IMPORT_FLAGS=(-I "${ROOT}/selfhost/src")

cmake -S "${ROOT}" -B "${BUILD_DIR}" >/dev/null
cmake --build "${BUILD_DIR}" -j >/dev/null
mkdir -p "${GOLDEN_DIR}"

TMP_TOKENS="${GOLDEN_DIR}/hello.tokens"
"${AUREXC}" --dump-tokens "${ROOT}/examples/hello.ax" >"${TMP_TOKENS}"
diff -u "${ROOT}/tests/golden/hello.tokens" "${TMP_TOKENS}"

SELFHOST_RANGES_BIN="${GOLDEN_DIR}/lexer_ranges"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax" -o "${SELFHOST_RANGES_BIN}"
test "$("${SELFHOST_RANGES_BIN}")" = "selfhost lexer ranges ok"

PARSER_SMOKE_BIN="${GOLDEN_DIR}/parser_smoke"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/smoke/parser_smoke.ax" -o "${PARSER_SMOKE_BIN}"
test "$("${PARSER_SMOKE_BIN}")" = "selfhost parser seed ok"

STAGE1_LANG_BIN="${GOLDEN_DIR}/stage1_lang"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_lang.ax" -o "${STAGE1_LANG_BIN}"
test "$("${STAGE1_LANG_BIN}")" = "selfhost stage1 lang ok"

STAGE1_CORE_BIN="${GOLDEN_DIR}/stage1_core"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_core.ax" -o "${STAGE1_CORE_BIN}"
test "$("${STAGE1_CORE_BIN}")" = "selfhost stage1 core ok"

SELFHOST_TOKENS="${GOLDEN_DIR}/lexer_dump.tokens"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_dump.ax" -o "${GOLDEN_DIR}/lexer_dump"
"${GOLDEN_DIR}/lexer_dump" >"${SELFHOST_TOKENS}"
diff -u "${ROOT}/tests/golden/selfhost_lexer_dump.tokens" "${SELFHOST_TOKENS}"

SELFHOST_FILE_TOKENS="${GOLDEN_DIR}/lexer_file_hello.tokens"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax" -o "${GOLDEN_DIR}/lexer_file"
"${GOLDEN_DIR}/lexer_file" "${ROOT}/examples/hello.ax" >"${SELFHOST_FILE_TOKENS}"
diff -u "${ROOT}/tests/golden/selfhost_lexer_file_hello.tokens" "${SELFHOST_FILE_TOKENS}"

echo "golden tests passed"
