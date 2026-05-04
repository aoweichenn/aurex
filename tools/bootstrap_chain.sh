#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
AUREXC="${BUILD_DIR}/bin/aurexc"
SELFHOST_BUILD_DIR="${BUILD_DIR}/selfhost"
TMP_DIR="${BUILD_DIR}/tmp"
SELFHOST_IMPORT_FLAGS=(-I "${ROOT}/selfhost/src")

SEED="${ROOT}/selfhost/src/aurex/selfhost/bin/aurexc_seed.ax"
LEXER_SMOKE="${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax"
LEXER_RANGES="${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax"
LEXER_DUMP="${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_dump.ax"
LEXER_FILE="${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax"
PARSER_SMOKE="${ROOT}/selfhost/src/aurex/selfhost/smoke/parser_smoke.ax"
STAGE1_LANG="${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_lang.ax"
STAGE1_CORE="${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_core.ax"
STAGE1_IR="${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_ir.ax"
STAGE1_FLOW="${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_flow.ax"
STAGE1_EXPR="${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_expr.ax"
STAGE1="${ROOT}/selfhost/src/aurex/selfhost/bin/aurexc_stage1.ax"

SEED_BIN="${SELFHOST_BUILD_DIR}/aurexc_seed"
LEXER_SMOKE_BIN="${SELFHOST_BUILD_DIR}/lexer_smoke"
LEXER_RANGES_BIN="${SELFHOST_BUILD_DIR}/lexer_ranges"
LEXER_DUMP_BIN="${SELFHOST_BUILD_DIR}/lexer_dump"
LEXER_FILE_BIN="${SELFHOST_BUILD_DIR}/lexer_file"
PARSER_SMOKE_BIN="${SELFHOST_BUILD_DIR}/parser_smoke"
STAGE1_LANG_BIN="${SELFHOST_BUILD_DIR}/stage1_lang"
STAGE1_CORE_BIN="${SELFHOST_BUILD_DIR}/stage1_core"
STAGE1_IR_BIN="${SELFHOST_BUILD_DIR}/stage1_ir"
STAGE1_BIN="${SELFHOST_BUILD_DIR}/aurexc_stage1"

STAGE1_HELLO_TAC="${SELFHOST_BUILD_DIR}/hello.stage1.tac"
STAGE1_SEED_TAC="${SELFHOST_BUILD_DIR}/aurexc_seed.stage1.tac"
STAGE1_PARSER_TAC="${SELFHOST_BUILD_DIR}/parser_smoke.stage1.tac"
STAGE1_TAC_OUT="${SELFHOST_BUILD_DIR}/stage1_ir.stage1.tac"
STAGE1_FLOW_TAC="${SELFHOST_BUILD_DIR}/stage1_flow.stage1.tac"
STAGE1_EXPR_TAC="${SELFHOST_BUILD_DIR}/stage1_expr.stage1.tac"
STAGE1_COMPILER_TAC="${SELFHOST_BUILD_DIR}/aurexc_stage1.bundle.tac"

cmake -S "${ROOT}" -B "${BUILD_DIR}" >/dev/null
cmake --build "${BUILD_DIR}" -j >/dev/null
mkdir -p "${SELFHOST_BUILD_DIR}" "${TMP_DIR}"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${SEED}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${SEED}" -o "${SEED_BIN}"
test "$("${SEED_BIN}")" = "Aurex M0 selfhost seed"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${LEXER_SMOKE}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_SMOKE}" -o "${LEXER_SMOKE_BIN}"
test "$("${LEXER_SMOKE_BIN}")" = "selfhost lexer sequence ok"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${LEXER_RANGES}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_RANGES}" -o "${LEXER_RANGES_BIN}"
test "$("${LEXER_RANGES_BIN}")" = "selfhost lexer ranges ok"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${PARSER_SMOKE}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${PARSER_SMOKE}" >"${SELFHOST_BUILD_DIR}/parser_smoke.modules"
grep -q 'aurex.selfhost.parser.seed' "${SELFHOST_BUILD_DIR}/parser_smoke.modules"
grep -q 'aurex.selfhost.parser.cursor' "${SELFHOST_BUILD_DIR}/parser_smoke.modules"
grep -q 'aurex.selfhost.parser.expr' "${SELFHOST_BUILD_DIR}/parser_smoke.modules"
grep -q 'aurex.selfhost.parser.types' "${SELFHOST_BUILD_DIR}/parser_smoke.modules"
grep -q 'aurex.selfhost.lexer.core' "${SELFHOST_BUILD_DIR}/parser_smoke.modules"
grep -q 'aurex.selfhost.syntax.ast' "${SELFHOST_BUILD_DIR}/parser_smoke.modules"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${PARSER_SMOKE}" -o "${PARSER_SMOKE_BIN}"
test "$("${PARSER_SMOKE_BIN}")" = "selfhost parser seed ok"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${STAGE1_LANG}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${STAGE1_LANG}" -o "${STAGE1_LANG_BIN}"
test "$("${STAGE1_LANG_BIN}")" = "selfhost stage1 lang ok"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${STAGE1_CORE}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${STAGE1_CORE}" -o "${STAGE1_CORE_BIN}"
test "$("${STAGE1_CORE_BIN}")" = "selfhost stage1 core ok"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${STAGE1_IR}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${STAGE1_IR}" -o "${STAGE1_IR_BIN}"
"${STAGE1_IR_BIN}" >/dev/null

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${STAGE1}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${STAGE1}" >"${SELFHOST_BUILD_DIR}/aurexc_stage1.modules"
grep -q 'aurex.selfhost.compiler.driver' "${SELFHOST_BUILD_DIR}/aurexc_stage1.modules"
grep -q 'aurex.selfhost.compiler.ir.emit' "${SELFHOST_BUILD_DIR}/aurexc_stage1.modules"
grep -q 'aurex.selfhost.compiler.ir.expr' "${SELFHOST_BUILD_DIR}/aurexc_stage1.modules"
grep -q 'aurex.selfhost.compiler.ir.types' "${SELFHOST_BUILD_DIR}/aurexc_stage1.modules"
grep -q 'aurex.selfhost.compiler.ir.names' "${SELFHOST_BUILD_DIR}/aurexc_stage1.modules"
grep -q 'aurex.selfhost.compiler.ir.writer' "${SELFHOST_BUILD_DIR}/aurexc_stage1.modules"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${STAGE1}" -o "${STAGE1_BIN}"

"${STAGE1_BIN}" "${ROOT}/examples/hello.ax" "${STAGE1_HELLO_TAC}"
grep -q 'aurex_tac v0' "${STAGE1_HELLO_TAC}"
grep -q 'fn puts(s: \*const u8) @puts linkage(extern_c) abi(c) -> i32' "${STAGE1_HELLO_TAC}"
grep -q 'fn main(argc: i32, argv: \*mut \*mut u8)' "${STAGE1_HELLO_TAC}"
grep -q 'c_string c"hello from Aurex M0"' "${STAGE1_HELLO_TAC}"
grep -q 'ret %t' "${STAGE1_HELLO_TAC}"

"${STAGE1_BIN}" "${SEED}" "${STAGE1_SEED_TAC}"
grep -q 'c_string c"Aurex M0 selfhost seed"' "${STAGE1_SEED_TAC}"

"${STAGE1_BIN}" "${STAGE1_FLOW}" "${STAGE1_FLOW_TAC}"
grep -q 'fn helper(limit: i32)' "${STAGE1_FLOW_TAC}"
grep -q 'linkage(internal)' "${STAGE1_FLOW_TAC}"
grep -q 'let one:' "${STAGE1_FLOW_TAC}"
grep -q 'var acc:' "${STAGE1_FLOW_TAC}"
grep -q 'assign %t' "${STAGE1_FLOW_TAC}"
grep -q 'while %t' "${STAGE1_FLOW_TAC}"
grep -q 'if %t' "${STAGE1_FLOW_TAC}"
grep -q 'block \^block' "${STAGE1_FLOW_TAC}"

"${STAGE1_BIN}" "${STAGE1_EXPR}" "${STAGE1_EXPR_TAC}"
grep -q 'size_of <u8>' "${STAGE1_EXPR_TAC}"
grep -q 'align_of <u8>' "${STAGE1_EXPR_TAC}"
grep -q 'ptr_addr %t' "${STAGE1_EXPR_TAC}"
grep -q 'ptr_from_addr <' "${STAGE1_EXPR_TAC}"
grep -q 'cast <u8> %t' "${STAGE1_EXPR_TAC}"
grep -q 'ptr_cast <' "${STAGE1_EXPR_TAC}"
grep -q 'bit_cast <u32> %t' "${STAGE1_EXPR_TAC}"
grep -q 'struct Pair' "${STAGE1_EXPR_TAC}"
grep -q '.value' "${STAGE1_EXPR_TAC}"
grep -q 'index ' "${STAGE1_EXPR_TAC}"
grep -q '4]u8' "${STAGE1_EXPR_TAC}"

"${STAGE1_BIN}" "${STAGE1_IR}" "${STAGE1_TAC_OUT}"
grep -q 'fn helper() @stage1_ir_helper linkage(export_c) abi(c) -> i32' "${STAGE1_TAC_OUT}"
grep -q 'literal 40' "${STAGE1_TAC_OUT}"
grep -q 'call %t' "${STAGE1_TAC_OUT}"
grep -q 'add %t' "${STAGE1_TAC_OUT}"
grep -q 'mul %t' "${STAGE1_TAC_OUT}"

"${STAGE1_BIN}" \
    "${ROOT}/selfhost/src/aurex/selfhost/lexer/core.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/syntax/ast.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/parser/cursor.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/parser/types.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/parser/expr.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/parser/seed.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/io.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/ir/writer.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/ir/names.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/ir/types.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/ir/expr.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/ir/emit.ax" \
    "${ROOT}/selfhost/src/aurex/selfhost/compiler/driver.ax" \
    "${STAGE1}" \
    "${STAGE1_COMPILER_TAC}"
grep -q 'aurex_tac v0' "${STAGE1_COMPILER_TAC}"
grep -q '; selfhost_module aurex.selfhost.compiler.driver lowering(ast_pending)' "${STAGE1_COMPILER_TAC}"
grep -q '; selfhost_module aurex.selfhost.bin.aurexc_stage1 lowering(ast_pending)' "${STAGE1_COMPILER_TAC}"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${LEXER_DUMP}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_DUMP}" -o "${LEXER_DUMP_BIN}"
"${LEXER_DUMP_BIN}" >"${SELFHOST_BUILD_DIR}/lexer_dump.tokens"
diff -u "${ROOT}/tests/golden/selfhost_lexer_dump.tokens" "${SELFHOST_BUILD_DIR}/lexer_dump.tokens"

"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${LEXER_FILE}"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${LEXER_FILE}" >"${SELFHOST_BUILD_DIR}/lexer_file.modules"
grep -q 'aurex.selfhost.lexer.dump' "${SELFHOST_BUILD_DIR}/lexer_file.modules"
grep -q 'aurex.selfhost.lexer.core' "${SELFHOST_BUILD_DIR}/lexer_file.modules"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" "${LEXER_FILE}" -o "${LEXER_FILE_BIN}"
"${LEXER_FILE_BIN}" "${ROOT}/examples/hello.ax" >"${SELFHOST_BUILD_DIR}/lexer_file_hello.tokens"
diff -u "${ROOT}/tests/golden/selfhost_lexer_file_hello.tokens" "${SELFHOST_BUILD_DIR}/lexer_file_hello.tokens"
"${ROOT}/tools/compare_selfhost_lexer.sh" >"${TMP_DIR}/aurex_selfhost_lexer_compare.txt"
grep -q 'selfhost lexer matches Stage0 lexer for local corpus' "${TMP_DIR}/aurex_selfhost_lexer_compare.txt"

echo "bootstrap chain passed: Stage0 aurexc + selfhost lexer/parser + Stage1 TAC snapshots"
