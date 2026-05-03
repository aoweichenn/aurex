#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
M0C="${BUILD_DIR}/m0c"
SELFHOST_IMPORT_FLAGS=(-I "${ROOT}/selfhost/src")
SEED="${ROOT}/selfhost/src/aurex/selfhost/bin/m0c_seed.ax"
LEXER_SMOKE="${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax"
LEXER_RANGES="${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax"
LEXER_DUMP="${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_dump.ax"
LEXER_FILE="${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax"
PARSER_SMOKE="${ROOT}/selfhost/src/aurex/selfhost/smoke/parser_smoke.ax"
STAGE1_LANG="${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_lang.ax"
STAGE1_CORE="${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_core.ax"
STAGE1="${ROOT}/selfhost/src/aurex/selfhost/bin/m0c_stage1.ax"
SEED_C="${BUILD_DIR}/m0c_seed.c"
SEED_BIN="${BUILD_DIR}/m0c_seed"
LEXER_SMOKE_C="${BUILD_DIR}/lexer_smoke.c"
LEXER_SMOKE_BIN="${BUILD_DIR}/lexer_smoke"
LEXER_RANGES_C="${BUILD_DIR}/lexer_ranges.c"
LEXER_RANGES_BIN="${BUILD_DIR}/lexer_ranges"
LEXER_DUMP_C="${BUILD_DIR}/lexer_dump.c"
LEXER_DUMP_BIN="${BUILD_DIR}/lexer_dump"
LEXER_FILE_C="${BUILD_DIR}/lexer_file.c"
LEXER_FILE_BIN="${BUILD_DIR}/lexer_file"
PARSER_SMOKE_C="${BUILD_DIR}/parser_smoke.c"
PARSER_SMOKE_BIN="${BUILD_DIR}/parser_smoke"
STAGE1_LANG_C="${BUILD_DIR}/stage1_lang.c"
STAGE1_LANG_BIN="${BUILD_DIR}/stage1_lang"
STAGE1_CORE_C="${BUILD_DIR}/stage1_core.c"
STAGE1_CORE_BIN="${BUILD_DIR}/stage1_core"
STAGE1_C="${BUILD_DIR}/m0c_stage1.c"
STAGE1_BIN="${BUILD_DIR}/m0c_stage1"
STAGE1_HELLO_C="${BUILD_DIR}/hello.stage1.c"
STAGE1_HELLO_BIN="${BUILD_DIR}/hello.stage1"
STAGE1_SEED_C="${BUILD_DIR}/m0c_seed.stage1.c"
STAGE1_SEED_BIN="${BUILD_DIR}/m0c_seed.stage1"
STAGE1_LEXER_SMOKE_C="${BUILD_DIR}/lexer_smoke.stage1.c"
STAGE1_LEXER_SMOKE_BIN="${BUILD_DIR}/lexer_smoke.stage1"
STAGE1_LEXER_RANGES_C="${BUILD_DIR}/lexer_ranges.stage1.c"
STAGE1_LEXER_RANGES_BIN="${BUILD_DIR}/lexer_ranges.stage1"
STAGE1_PARSER_SMOKE_C="${BUILD_DIR}/parser_smoke.stage1.c"
STAGE1_PARSER_SMOKE_BIN="${BUILD_DIR}/parser_smoke.stage1"
STAGE1_LEXER_FILE_C="${BUILD_DIR}/lexer_file.stage1.c"
STAGE1_LEXER_FILE_BIN="${BUILD_DIR}/lexer_file.stage1"
STAGE1_LANG_STAGE1_C="${BUILD_DIR}/stage1_lang.stage1.c"
STAGE1_LANG_STAGE1_BIN="${BUILD_DIR}/stage1_lang.stage1"
STAGE1_CORE_STAGE1_C="${BUILD_DIR}/stage1_core.stage1.c"
STAGE1_CORE_STAGE1_BIN="${BUILD_DIR}/stage1_core.stage1"
STAGE2_C="${BUILD_DIR}/m0c_stage2.smoke.c"
STAGE2_BIN="${BUILD_DIR}/m0c_stage2.smoke"
STAGE2_AUTO_C="${BUILD_DIR}/m0c_stage2.auto.c"
STAGE2_AUTO_BIN="${BUILD_DIR}/m0c_stage2.auto"
STAGE3_AUTO_C="${BUILD_DIR}/m0c_stage3.auto.c"
STAGE2_SEED_C="${BUILD_DIR}/m0c_seed.stage2.c"
STAGE2_SEED_BIN="${BUILD_DIR}/m0c_seed.stage2"
STAGE2_CORE_C="${BUILD_DIR}/stage1_core.stage2.c"
STAGE2_CORE_BIN="${BUILD_DIR}/stage1_core.stage2"
STAGE3_C="${BUILD_DIR}/m0c_stage3.smoke.c"
STAGE3_BIN="${BUILD_DIR}/m0c_stage3.smoke"
STAGE3_SEED_C="${BUILD_DIR}/m0c_seed.stage3.c"
STAGE3_SEED_BIN="${BUILD_DIR}/m0c_seed.stage3"
STAGE3_CORE_C="${BUILD_DIR}/stage1_core.stage3.c"
STAGE3_CORE_BIN="${BUILD_DIR}/stage1_core.stage3"
BOOT_C="${BUILD_DIR}/hello.bootstrap.c"
BOOT_BIN="${BUILD_DIR}/hello.bootstrap"

cmake -S "${ROOT}" -B "${BUILD_DIR}" >/dev/null
cmake --build "${BUILD_DIR}" -j >/dev/null

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${SEED}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${SEED}" -o "${SEED_C}"
cc "${SEED_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${SEED_BIN}"
SEED_OUT="$("${SEED_BIN}")"
test "${SEED_OUT}" = "Aurex M0 selfhost seed"

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${LEXER_SMOKE}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_SMOKE}" -o "${LEXER_SMOKE_C}"
cc "${LEXER_SMOKE_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${LEXER_SMOKE_BIN}"
LEXER_SMOKE_OUT="$("${LEXER_SMOKE_BIN}")"
test "${LEXER_SMOKE_OUT}" = "selfhost lexer sequence ok"

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${LEXER_RANGES}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_RANGES}" -o "${LEXER_RANGES_C}"
cc "${LEXER_RANGES_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${LEXER_RANGES_BIN}"
LEXER_RANGES_OUT="$("${LEXER_RANGES_BIN}")"
test "${LEXER_RANGES_OUT}" = "selfhost lexer ranges ok"

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${PARSER_SMOKE}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${PARSER_SMOKE}" >"${BUILD_DIR}/parser_smoke.modules"
grep -q 'aurex.selfhost.parser.seed' "${BUILD_DIR}/parser_smoke.modules"
grep -q 'aurex.selfhost.lexer.core' "${BUILD_DIR}/parser_smoke.modules"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${PARSER_SMOKE}" -o "${PARSER_SMOKE_C}"
cc "${PARSER_SMOKE_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${PARSER_SMOKE_BIN}"
PARSER_SMOKE_OUT="$("${PARSER_SMOKE_BIN}")"
test "${PARSER_SMOKE_OUT}" = "selfhost parser seed ok"

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${STAGE1_LANG}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${STAGE1_LANG}" -o "${STAGE1_LANG_C}"
cc "${STAGE1_LANG_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE1_LANG_BIN}"
STAGE1_LANG_OUT="$("${STAGE1_LANG_BIN}")"
test "${STAGE1_LANG_OUT}" = "selfhost stage1 lang ok"

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${STAGE1_CORE}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${STAGE1_CORE}" -o "${STAGE1_CORE_C}"
cc "${STAGE1_CORE_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE1_CORE_BIN}"
STAGE1_CORE_OUT="$("${STAGE1_CORE_BIN}")"
test "${STAGE1_CORE_OUT}" = "selfhost stage1 core ok"

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${STAGE1}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${STAGE1}" >"${BUILD_DIR}/m0c_stage1.modules"
grep -q 'aurex.selfhost.compiler.driver' "${BUILD_DIR}/m0c_stage1.modules"
grep -q 'aurex.selfhost.compiler.subset' "${BUILD_DIR}/m0c_stage1.modules"
grep -q 'aurex.selfhost.compiler.emit_c' "${BUILD_DIR}/m0c_stage1.modules"
grep -q 'aurex.selfhost.compiler.imports' "${BUILD_DIR}/m0c_stage1.modules"
grep -q 'aurex.selfhost.compiler.emit.bundle' "${BUILD_DIR}/m0c_stage1.modules"
grep -q 'aurex.selfhost.compiler.emit.symbols' "${BUILD_DIR}/m0c_stage1.modules"
grep -q 'aurex.selfhost.compiler.emit.item' "${BUILD_DIR}/m0c_stage1.modules"
grep -q 'aurex.selfhost.compiler.emit.stmt' "${BUILD_DIR}/m0c_stage1.modules"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${STAGE1}" -o "${STAGE1_C}"
cc "${STAGE1_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE1_BIN}"
"${STAGE1_BIN}" "${ROOT}/examples/hello.ax" "${STAGE1_HELLO_C}"
cc "${STAGE1_HELLO_C}" -o "${STAGE1_HELLO_BIN}"
STAGE1_HELLO_OUT="$("${STAGE1_HELLO_BIN}")"
test "${STAGE1_HELLO_OUT}" = "hello from Aurex M0"
"${STAGE1_BIN}" "${SEED}" "${STAGE1_SEED_C}"
cc "${STAGE1_SEED_C}" -o "${STAGE1_SEED_BIN}"
STAGE1_SEED_OUT="$("${STAGE1_SEED_BIN}")"
test "${STAGE1_SEED_OUT}" = "Aurex M0 selfhost seed"
"${STAGE1_BIN}" "${ROOT}/selfhost/src/aurex/selfhost/lexer/core.ax" "${LEXER_SMOKE}" "${STAGE1_LEXER_SMOKE_C}"
grep -q '#define validate_sequence m0_aurex_selfhost_smoke_lexer_smoke_validate_sequence' "${STAGE1_LEXER_SMOKE_C}"
grep -q 'scan_next' "${STAGE1_LEXER_SMOKE_C}"
cc "${STAGE1_LEXER_SMOKE_C}" -o "${STAGE1_LEXER_SMOKE_BIN}"
STAGE1_LEXER_SMOKE_OUT="$("${STAGE1_LEXER_SMOKE_BIN}")"
test "${STAGE1_LEXER_SMOKE_OUT}" = "selfhost lexer sequence ok"
"${STAGE1_BIN}" "${ROOT}/selfhost/src/aurex/selfhost/lexer/core.ax" "${LEXER_RANGES}" "${STAGE1_LEXER_RANGES_C}"
grep -q '#define validate_ranges m0_aurex_selfhost_smoke_lexer_ranges_validate_ranges' "${STAGE1_LEXER_RANGES_C}"
grep -q 'scan_next' "${STAGE1_LEXER_RANGES_C}"
cc "${STAGE1_LEXER_RANGES_C}" -o "${STAGE1_LEXER_RANGES_BIN}"
STAGE1_LEXER_RANGES_OUT="$("${STAGE1_LEXER_RANGES_BIN}")"
test "${STAGE1_LEXER_RANGES_OUT}" = "selfhost lexer ranges ok"
"${STAGE1_BIN}" "${ROOT}/selfhost/src/aurex/selfhost/lexer/core.ax" "${ROOT}/selfhost/src/aurex/selfhost/parser/seed.ax" "${PARSER_SMOKE}" "${STAGE1_PARSER_SMOKE_C}"
grep -q '#define parse_seed_module m0_aurex_selfhost_parser_seed_parse_seed_module' "${STAGE1_PARSER_SMOKE_C}"
grep -q 'scan_token' "${STAGE1_PARSER_SMOKE_C}"
cc "${STAGE1_PARSER_SMOKE_C}" -o "${STAGE1_PARSER_SMOKE_BIN}"
STAGE1_PARSER_SMOKE_OUT="$("${STAGE1_PARSER_SMOKE_BIN}")"
test "${STAGE1_PARSER_SMOKE_OUT}" = "selfhost parser seed ok"
"${STAGE1_BIN}" "${ROOT}/selfhost/src/aurex/selfhost/lexer/core.ax" "${ROOT}/selfhost/src/aurex/selfhost/lexer/dump.ax" "${LEXER_FILE}" "${STAGE1_LEXER_FILE_C}"
grep -q '#define read_file m0_aurex_selfhost_tool_lexer_file_read_file' "${STAGE1_LEXER_FILE_C}"
grep -q 'm0_runtime_read_file' "${STAGE1_LEXER_FILE_C}"
cc "${STAGE1_LEXER_FILE_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE1_LEXER_FILE_BIN}"
"${STAGE1_LEXER_FILE_BIN}" "${ROOT}/examples/hello.ax" >"${BUILD_DIR}/lexer_file.stage1.tokens"
diff -u "${ROOT}/tests/golden/selfhost_lexer_file_hello.tokens" "${BUILD_DIR}/lexer_file.stage1.tokens"
"${STAGE1_BIN}" "${STAGE1_LANG}" "${STAGE1_LANG_STAGE1_C}"
grep -q 'aurex_m0_str' "${STAGE1_LANG_STAGE1_C}"
grep -q 'continue;' "${STAGE1_LANG_STAGE1_C}"
grep -q 'break;' "${STAGE1_LANG_STAGE1_C}"
grep -q 'else .*if' "${STAGE1_LANG_STAGE1_C}"
cc "${STAGE1_LANG_STAGE1_C}" -o "${STAGE1_LANG_STAGE1_BIN}"
STAGE1_LANG_STAGE1_OUT="$("${STAGE1_LANG_STAGE1_BIN}")"
test "${STAGE1_LANG_STAGE1_OUT}" = "selfhost stage1 lang ok"
"${STAGE1_BIN}" "${STAGE1_CORE}" "${STAGE1_CORE_STAGE1_C}"
grep -q 'typedef struct NativeFile NativeFile;' "${STAGE1_CORE_STAGE1_C}"
grep -q 'typedef struct TopOpaque TopOpaque;' "${STAGE1_CORE_STAGE1_C}"
grep -q 'Stage1Tag_one' "${STAGE1_CORE_STAGE1_C}"
grep -q 'const Cell \*const_cell' "${STAGE1_CORE_STAGE1_C}"
grep -q 'ptr->value' "${STAGE1_CORE_STAGE1_C}"
grep -q 'pair_ptr->value' "${STAGE1_CORE_STAGE1_C}"
grep -q 'pair_ptr->value  = value' "${STAGE1_CORE_STAGE1_C}"
grep -q '#define exported_value m0_stage1_exported_value' "${STAGE1_CORE_STAGE1_C}"
grep -q 'int32_t m0_stage1_exported_value();' "${STAGE1_CORE_STAGE1_C}"
grep -q 'int32_t m0_stage1_exported_value()' "${STAGE1_CORE_STAGE1_C}"
grep -q 'WrappedCell' "${STAGE1_CORE_STAGE1_C}"
grep -q 'uint8_t (\*buf)\[4\]' "${STAGE1_CORE_STAGE1_C}"
grep -q 'alignof' "${STAGE1_CORE_STAGE1_C}"
grep -q 'uintptr_t' "${STAGE1_CORE_STAGE1_C}"
cc "${STAGE1_CORE_STAGE1_C}" -o "${STAGE1_CORE_STAGE1_BIN}"
STAGE1_CORE_STAGE1_OUT="$("${STAGE1_CORE_STAGE1_BIN}")"
test "${STAGE1_CORE_STAGE1_OUT}" = "selfhost stage1 core ok"

"${STAGE1_BIN}" \
    "${ROOT}/selfhost/src/aurex/selfhost/lexer/core.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/io.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/cursor.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/writer.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/symbols.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/types.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/expr.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/assign.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/stmt.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/item.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/bundle.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit_subset.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/subset.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit_c.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/imports.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/driver.ax" \
    "${STAGE1}" \
    "${STAGE2_C}"
cc "${STAGE2_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE2_BIN}"
"${STAGE1_BIN}" "${STAGE1}" "${STAGE2_AUTO_C}"
cc "${STAGE2_AUTO_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE2_AUTO_BIN}"
"${STAGE2_AUTO_BIN}" "${STAGE1}" "${STAGE3_AUTO_C}"
cmp -s "${STAGE2_AUTO_C}" "${STAGE3_AUTO_C}"
"${STAGE2_AUTO_BIN}" "${SEED}" "${BUILD_DIR}/m0c_seed.stage2.auto.c"
cc "${BUILD_DIR}/m0c_seed.stage2.auto.c" -o "${BUILD_DIR}/m0c_seed.stage2.auto"
STAGE2_AUTO_SEED_OUT="$("${BUILD_DIR}/m0c_seed.stage2.auto")"
test "${STAGE2_AUTO_SEED_OUT}" = "Aurex M0 selfhost seed"
"${STAGE2_BIN}" "${SEED}" "${STAGE2_SEED_C}"
cc "${STAGE2_SEED_C}" -o "${STAGE2_SEED_BIN}"
STAGE2_SEED_OUT="$("${STAGE2_SEED_BIN}")"
test "${STAGE2_SEED_OUT}" = "Aurex M0 selfhost seed"
"${STAGE2_BIN}" "${STAGE1_CORE}" "${STAGE2_CORE_C}"
grep -q 'ptr->value' "${STAGE2_CORE_C}"
grep -q 'pair_ptr->value' "${STAGE2_CORE_C}"
grep -q 'pair_ptr->value  = value' "${STAGE2_CORE_C}"
cc "${STAGE2_CORE_C}" -o "${STAGE2_CORE_BIN}"
STAGE2_CORE_OUT="$("${STAGE2_CORE_BIN}")"
test "${STAGE2_CORE_OUT}" = "selfhost stage1 core ok"

"${STAGE2_BIN}" \
    "${ROOT}/selfhost/src/aurex/selfhost/lexer/core.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/io.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/cursor.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/writer.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/symbols.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/types.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/expr.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/assign.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/stmt.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/item.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit/bundle.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit_subset.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/subset.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/emit_c.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/imports.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/driver.ax" \
    "${STAGE1}" \
    "${STAGE3_C}"
cmp -s "${STAGE2_C}" "${STAGE3_C}"
cc "${STAGE3_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${STAGE3_BIN}"
"${STAGE3_BIN}" "${SEED}" "${STAGE3_SEED_C}"
cmp -s "${STAGE2_SEED_C}" "${STAGE3_SEED_C}"
cc "${STAGE3_SEED_C}" -o "${STAGE3_SEED_BIN}"
STAGE3_SEED_OUT="$("${STAGE3_SEED_BIN}")"
test "${STAGE3_SEED_OUT}" = "Aurex M0 selfhost seed"
"${STAGE3_BIN}" "${STAGE1_CORE}" "${STAGE3_CORE_C}"
cmp -s "${STAGE2_CORE_C}" "${STAGE3_CORE_C}"
grep -q 'ptr->value' "${STAGE3_CORE_C}"
grep -q 'pair_ptr->value' "${STAGE3_CORE_C}"
grep -q 'pair_ptr->value  = value' "${STAGE3_CORE_C}"
grep -q '#define exported_value m0_stage1_exported_value' "${STAGE3_CORE_C}"
grep -q 'int32_t m0_stage1_exported_value();' "${STAGE3_CORE_C}"
grep -q 'int32_t m0_stage1_exported_value()' "${STAGE3_CORE_C}"
grep -q 'WrappedCell' "${STAGE3_CORE_C}"
cc "${STAGE3_CORE_C}" -o "${STAGE3_CORE_BIN}"
STAGE3_CORE_OUT="$("${STAGE3_CORE_BIN}")"
test "${STAGE3_CORE_OUT}" = "selfhost stage1 core ok"

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${LEXER_DUMP}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_DUMP}" -o "${LEXER_DUMP_C}"
cc "${LEXER_DUMP_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${LEXER_DUMP_BIN}"
"${LEXER_DUMP_BIN}" >"${BUILD_DIR}/lexer_dump.tokens"
diff -u "${ROOT}/tests/golden/selfhost_lexer_dump.tokens" "${BUILD_DIR}/lexer_dump.tokens"

"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${LEXER_FILE}"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${LEXER_FILE}" >"${BUILD_DIR}/lexer_file.modules"
grep -q 'aurex.selfhost.lexer.dump' "${BUILD_DIR}/lexer_file.modules"
grep -q 'aurex.selfhost.lexer.core' "${BUILD_DIR}/lexer_file.modules"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_FILE}" -o "${LEXER_FILE_C}"
cc "${LEXER_FILE_C}" "${ROOT}/selfhost/runtime/runtime.c" -o "${LEXER_FILE_BIN}"
"${LEXER_FILE_BIN}" "${ROOT}/examples/hello.ax" >"${BUILD_DIR}/lexer_file_hello.tokens"
diff -u "${ROOT}/tests/golden/selfhost_lexer_file_hello.tokens" "${BUILD_DIR}/lexer_file_hello.tokens"
"${ROOT}/tools/compare_selfhost_lexer.sh" >/tmp/aurex_selfhost_lexer_compare.txt
grep -q 'selfhost lexer matches Stage0 lexer for local corpus' /tmp/aurex_selfhost_lexer_compare.txt

make -C "${ROOT}/bootstrap" >/dev/null
"${ROOT}/bootstrap/m0_bootstrap" "${ROOT}/examples/hello.ax" -o "${BOOT_C}"
cc "${BOOT_C}" -o "${BOOT_BIN}"
BOOT_OUT="$("${BOOT_BIN}")"
test "${BOOT_OUT}" = "hello from Aurex M0"

echo "bootstrap chain passed: Stage0 m0c + selfhost lexer smoke/ranges/dump/file + parser seed + M0 stage1/stage2/stage3 fixed-point smoke + standalone bootstrap seed"
