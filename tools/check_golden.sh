#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
M0C="${BUILD_DIR}/m0c"
SELFHOST_IMPORT_FLAGS=(-I "${ROOT}/selfhost/src")

cmake -S "${ROOT}" -B "${BUILD_DIR}" >/dev/null
cmake --build "${BUILD_DIR}" -j >/dev/null

TMP_TOKENS="$(mktemp)"
"${M0C}" --dump-tokens "${ROOT}/examples/hello.ax" >"${TMP_TOKENS}"
diff -u "${ROOT}/tests/golden/hello.tokens" "${TMP_TOKENS}"
rm -f "${TMP_TOKENS}"

SELFHOST_RANGES_BIN="${BUILD_DIR}/lexer_ranges"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax" -o "${BUILD_DIR}/lexer_ranges.c"
cc "${BUILD_DIR}/lexer_ranges.c" "${ROOT}/selfhost/runtime/runtime.c" -o "${SELFHOST_RANGES_BIN}"
test "$("${SELFHOST_RANGES_BIN}")" = "selfhost lexer ranges ok"

PARSER_SMOKE_BIN="${BUILD_DIR}/parser_smoke"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/smoke/parser_smoke.ax" -o "${BUILD_DIR}/parser_smoke.c"
cc "${BUILD_DIR}/parser_smoke.c" "${ROOT}/selfhost/runtime/runtime.c" -o "${PARSER_SMOKE_BIN}"
test "$("${PARSER_SMOKE_BIN}")" = "selfhost parser seed ok"

STAGE1_LANG_BIN="${BUILD_DIR}/stage1_lang"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_lang.ax" -o "${BUILD_DIR}/stage1_lang.c"
cc "${BUILD_DIR}/stage1_lang.c" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE1_LANG_BIN}"
test "$("${STAGE1_LANG_BIN}")" = "selfhost stage1 lang ok"

STAGE1_CORE_BIN="${BUILD_DIR}/stage1_core"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_core.ax" -o "${BUILD_DIR}/stage1_core.c"
cc "${BUILD_DIR}/stage1_core.c" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE1_CORE_BIN}"
test "$("${STAGE1_CORE_BIN}")" = "selfhost stage1 core ok"

SELFHOST_TOKENS="${BUILD_DIR}/lexer_dump.tokens"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_dump.ax" -o "${BUILD_DIR}/lexer_dump.c"
cc "${BUILD_DIR}/lexer_dump.c" "${ROOT}/selfhost/runtime/runtime.c" -o "${BUILD_DIR}/lexer_dump"
"${BUILD_DIR}/lexer_dump" >"${SELFHOST_TOKENS}"
diff -u "${ROOT}/tests/golden/selfhost_lexer_dump.tokens" "${SELFHOST_TOKENS}"

SELFHOST_FILE_TOKENS="${BUILD_DIR}/lexer_file_hello.tokens"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax" -o "${BUILD_DIR}/lexer_file.c"
cc "${BUILD_DIR}/lexer_file.c" "${ROOT}/selfhost/runtime/runtime.c" -o "${BUILD_DIR}/lexer_file"
"${BUILD_DIR}/lexer_file" "${ROOT}/examples/hello.ax" >"${SELFHOST_FILE_TOKENS}"
diff -u "${ROOT}/tests/golden/selfhost_lexer_file_hello.tokens" "${SELFHOST_FILE_TOKENS}"

echo "golden tests passed"
