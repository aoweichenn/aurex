#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
AUREXC="${BUILD_DIR}/bin/aurexc"
TEST_DIR="${BUILD_DIR}/tests"
TMP_DIR="${BUILD_DIR}/tmp"
BOOT_DIR="${BUILD_DIR}/bootstrap"
SELFHOST_IMPORT_FLAGS=(-I "${ROOT}/selfhost/src")

cmake -S "${ROOT}" -B "${BUILD_DIR}" >/dev/null
cmake --build "${BUILD_DIR}" -j >/dev/null
mkdir -p "${TEST_DIR}" "${TMP_DIR}" "${BOOT_DIR}"

test -f "${ROOT}/docs/README.md"
test -f "${ROOT}/docs/zh/README.md"
test -f "${ROOT}/docs/zh/architecture.md"
test -f "${ROOT}/docs/zh/requirements.md"
test -f "${ROOT}/docs/zh/runtime-flow.md"
test -f "${ROOT}/docs/zh/api.md"
test -f "${ROOT}/docs/zh/implementation.md"
test -f "${ROOT}/docs/zh/usage.md"
test -f "${ROOT}/docs/zh/introduction.md"
test -f "${ROOT}/docs/zh/version.md"
test -f "${ROOT}/docs/zh/next-steps.md"
test -f "${ROOT}/docs/en/README.md"
test -f "${ROOT}/docs/en/architecture.md"
test -f "${ROOT}/docs/en/requirements.md"
test -f "${ROOT}/docs/en/runtime-flow.md"
test -f "${ROOT}/docs/en/api.md"
test -f "${ROOT}/docs/en/implementation.md"
test -f "${ROOT}/docs/en/usage.md"
test -f "${ROOT}/docs/en/introduction.md"
test -f "${ROOT}/docs/en/version.md"
test -f "${ROOT}/docs/en/next-steps.md"
obsolete_doc_paths=(
    "${ROOT}/docs/ARCHITECTURE.zh.md"
    "${ROOT}/docs/DESIGN.en.md"
    "${ROOT}/docs/DESIGN.zh.md"
    "${ROOT}/docs/SELFHOST.md"
    "${ROOT}/docs/SEMANTICS.md"
    "${ROOT}/docs/USAGE.en.md"
    "${ROOT}/docs/USAGE.zh.md"
)
for obsolete_doc_path in "${obsolete_doc_paths[@]}"; do
    test ! -e "${obsolete_doc_path}"
done
mapfile -t obsolete_version_docs < <(find "${ROOT}/docs" -maxdepth 1 -type f -name 'M0V0.1.*.md' -print)
if ((${#obsolete_version_docs[@]} != 0)); then
    printf 'unexpected per-small-version documentation files:\n' >&2
    printf '%s\n' "${obsolete_version_docs[@]}" >&2
    exit 1
fi

"${AUREXC}" --version | grep -q '0.1.2'
"${AUREXC}" --help | grep -q -- '--check'
"${AUREXC}" --help | grep -q -- '--emit=ast'
"${AUREXC}" --help | grep -q -- '--emit=ir'
"${AUREXC}" --help | grep -q -- '--emit=llvm-ir'
"${AUREXC}" --help | grep -q -- '--emit=asm'
"${AUREXC}" --help | grep -q -- '--emit=obj'
"${AUREXC}" --help | grep -q -- '--emit=exe'
"${AUREXC}" --help | grep -q -- '--no-stdlib'
"${AUREXC}" --help | grep -q -- '--dump-modules'
"${AUREXC}" --help | grep -q -- '--opt-level'
"${AUREXC}" --help | grep -q -- '--stdlib'
"${AUREXC}" --help | grep -q -- '--std-backend'
"${AUREXC}" --check "${ROOT}/examples/hello.ax"
"${AUREXC}" --emit=check "${ROOT}/examples/hello.ax"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/bin/aurexc_seed.ax"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_dump.ax"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/parser_smoke.ax"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_lang.ax"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_core.ax"
"${AUREXC}" --dump-tokens "${ROOT}/examples/hello.ax" >"${TMP_DIR}/aurex_tokens.txt"
"${AUREXC}" --emit=ast "${ROOT}/examples/hello.ax" >"${TMP_DIR}/aurex_ast.txt"
"${AUREXC}" --emit=checked "${ROOT}/examples/hello.ax" >"${TMP_DIR}/aurex_checked.txt"
"${AUREXC}" --emit=ir "${ROOT}/examples/hello.ax" >"${TMP_DIR}/aurex_ir_hello.txt"
"${AUREXC}" --emit=llvm-ir "${ROOT}/examples/hello.ax" >"${TMP_DIR}/aurex_llvm_ir_hello.ll"
"${AUREXC}" --emit=ir --opt-level O1 "${ROOT}/tests/positive/eval_order_assign.ax" >"${TMP_DIR}/aurex_ir_eval_order_assign_o1.txt"
"${AUREXC}" --emit=ir "${ROOT}/tests/positive/std_text.ax" >"${TMP_DIR}/aurex_ir_std_text.txt"
"${AUREXC}" --emit=ir "${ROOT}/tests/positive/pointer_field_write.ax" >"${TMP_DIR}/aurex_ir_pointer_field.txt"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --emit=llvm-ir "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax" >"${TMP_DIR}/aurex_selfhost_lexer_file.ll"
"${AUREXC}" --dump-modules "${ROOT}/tests/positive/module_math.ax" >"${TMP_DIR}/aurex_modules.txt"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax" >"${TMP_DIR}/aurex_selfhost_modules.txt"
"${AUREXC}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${ROOT}/selfhost/src/aurex/selfhost/smoke/parser_smoke.ax" >"${TMP_DIR}/aurex_selfhost_parser_modules.txt"
grep -q 'c_string_literal' "${TMP_DIR}/aurex_tokens.txt"
grep -q 'extern_block' "${TMP_DIR}/aurex_ast.txt"
grep -q 'checked_module' "${TMP_DIR}/aurex_checked.txt"
grep -q 'aurex_ir v0' "${TMP_DIR}/aurex_ir_hello.txt"
grep -q 'define i32 @main' "${TMP_DIR}/aurex_llvm_ir_hello.ll"
grep -q 'fn puts(s: \*const u8) @puts linkage(extern_c) abi(c) -> i32' "${TMP_DIR}/aurex_ir_hello.txt"
grep -q 'call puts' "${TMP_DIR}/aurex_ir_hello.txt"
grep -q 'call m0_eval_order_assign_next(%' "${TMP_DIR}/aurex_ir_eval_order_assign_o1.txt"
grep -q 'phi \[' "${TMP_DIR}/aurex_ir_std_text.txt"
grep -q 'usize = cast' "${TMP_DIR}/aurex_ir_std_text.txt"
grep -q 'field_addr .*\.value' "${TMP_DIR}/aurex_ir_pointer_field.txt"
grep -q 'aurex_std_v0_read_file' "${TMP_DIR}/aurex_selfhost_lexer_file.ll"
grep -q 'lib.math' "${TMP_DIR}/aurex_modules.txt"
grep -q 'module_math' "${TMP_DIR}/aurex_modules.txt"
grep -q 'aurex.selfhost.lexer.dump' "${TMP_DIR}/aurex_selfhost_modules.txt"
grep -q 'aurex.selfhost.lexer.core' "${TMP_DIR}/aurex_selfhost_modules.txt"
grep -q 'aurex.selfhost.parser.seed' "${TMP_DIR}/aurex_selfhost_parser_modules.txt"
grep -q 'aurex.selfhost.parser.cursor' "${TMP_DIR}/aurex_selfhost_parser_modules.txt"
grep -q 'aurex.selfhost.parser.expr' "${TMP_DIR}/aurex_selfhost_parser_modules.txt"
grep -q 'aurex.selfhost.parser.types' "${TMP_DIR}/aurex_selfhost_parser_modules.txt"
grep -q 'aurex.selfhost.lexer.core' "${TMP_DIR}/aurex_selfhost_parser_modules.txt"
diff -u "${ROOT}/tests/golden/hello.tokens" "${TMP_DIR}/aurex_tokens.txt"

"${AUREXC}" "${ROOT}/examples/hello.ax" -o "${TEST_DIR}/hello"
HELLO_OUT="$("${TEST_DIR}/hello")"
test "${HELLO_OUT}" = "hello from Aurex M0"
"${AUREXC}" --emit=asm "${ROOT}/examples/hello.ax" -o "${TEST_DIR}/hello.s"
test -s "${TEST_DIR}/hello.s"
"${AUREXC}" --emit=obj "${ROOT}/examples/hello.ax" -o "${TEST_DIR}/hello.o"
test -s "${TEST_DIR}/hello.o"
"${AUREXC}" --emit=exe "${ROOT}/examples/hello.ax" -o "${TEST_DIR}/hello.direct"
HELLO_DIRECT_OUT="$("${TEST_DIR}/hello.direct")"
test "${HELLO_DIRECT_OUT}" = "hello from Aurex M0"
"${AUREXC}" --std-backend none "${ROOT}/examples/hello.ax" -o "${TEST_DIR}/hello.stdnone"
HELLO_STDNONE_OUT="$("${TEST_DIR}/hello.stdnone")"
test "${HELLO_STDNONE_OUT}" = "hello from Aurex M0"
"${AUREXC}" --emit=llvm-ir "${ROOT}/tests/positive/const_enum.ax" >"${TMP_DIR}/aurex_llvm_ir_const_enum.ll"
grep -q '@m0_const_enum_answer = internal unnamed_addr constant i32 42' "${TMP_DIR}/aurex_llvm_ir_const_enum.ll"
grep -q 'load i32, ptr @m0_const_enum_answer' "${TMP_DIR}/aurex_llvm_ir_const_enum.ll"

for src in "${ROOT}"/tests/positive/*.ax; do
    case "$(basename "${src}" .ax)" in
        import_path|module_name_collision|std_text|std_mem|std_file)
            continue
            ;;
    esac
    bin="${TEST_DIR}/$(basename "${src}" .ax)"
    "${AUREXC}" "${src}" -o "${bin}"
    case "$(basename "${src}" .ax)" in
        condition_regression|pointer_ops)
            "${bin}" >/dev/null
            ;;
        address_of_let|pointer_field_write|eval_order_call_stmt|eval_order_return|eval_order_assign|eval_order_condition|builtins)
            "${bin}" >/dev/null
            ;;
    esac
done

for src in "${ROOT}"/tests/positive/std_*.ax; do
    bin="${TEST_DIR}/$(basename "${src}" .ax)"
    direct="${TEST_DIR}/$(basename "${src}" .ax).direct"
    "${AUREXC}" "${src}" -o "${bin}"
    "${bin}" >/dev/null
    "${AUREXC}" --emit=exe "${src}" -o "${direct}"
    "${direct}" >/dev/null
done

cmake --install "${BUILD_DIR}" --prefix "${BUILD_DIR}/install" >/dev/null
"${BUILD_DIR}/install/bin/aurexc" "${ROOT}/tests/positive/std_text.ax" -o "${TEST_DIR}/std_text.installed"
"${TEST_DIR}/std_text.installed" >/dev/null

"${AUREXC}" -I "${ROOT}/tests/imports" "${ROOT}/tests/positive/import_path.ax" -o "${TEST_DIR}/import_path"
"${TEST_DIR}/import_path" >/dev/null

"${AUREXC}" -I "${ROOT}/tests/imports" --emit=llvm-ir "${ROOT}/tests/positive/module_name_collision.ax" >"${TEST_DIR}/module_name_collision.ll"
grep -q '@m0_module_name_collision_helper' "${TEST_DIR}/module_name_collision.ll"
grep -q '@m0_collide_a_helper' "${TEST_DIR}/module_name_collision.ll"
"${AUREXC}" -I "${ROOT}/tests/imports" "${ROOT}/tests/positive/module_name_collision.ax" -o "${TEST_DIR}/module_name_collision"
"${TEST_DIR}/module_name_collision" >/dev/null

for src in "${ROOT}"/tests/negative/*.ax; do
    if [ "$(basename "${src}" .ax)" = "module_name_mismatch" ]; then
        if "${AUREXC}" -I "${ROOT}/tests/imports" --check "${src}" >"${TMP_DIR}/aurex_negative.out" 2>"${TMP_DIR}/aurex_negative.err"; then
            echo "expected semantic failure: ${src}" >&2
            exit 1
        fi
        continue
    fi
    if [ "$(basename "${src}" .ax)" = "cyclic_import" ]; then
        if "${AUREXC}" -I "${ROOT}/tests/imports" --check "${src}" >"${TMP_DIR}/aurex_negative.out" 2>"${TMP_DIR}/aurex_negative.err"; then
            echo "expected semantic failure: ${src}" >&2
            exit 1
        fi
        continue
    fi
    if [ "$(basename "${src}" .ax)" = "ambiguous_import_name" ]; then
        if "${AUREXC}" -I "${ROOT}/tests/imports" --check "${src}" >"${TMP_DIR}/aurex_negative.out" 2>"${TMP_DIR}/aurex_negative.err"; then
            echo "expected semantic failure: ${src}" >&2
            exit 1
        fi
        grep -q 'ambiguous function name' "${TMP_DIR}/aurex_negative.err"
        continue
    fi
    if "${AUREXC}" --check "${src}" >"${TMP_DIR}/aurex_negative.out" 2>"${TMP_DIR}/aurex_negative.err"; then
        echo "expected semantic failure: ${src}" >&2
        exit 1
    fi
done

make -C "${ROOT}/bootstrap" >/dev/null
BOOT_C="${BOOT_DIR}/hello.bootstrap.c"
BOOT_BIN="${BOOT_DIR}/hello.bootstrap"
"${ROOT}/bootstrap/m0_bootstrap" "${ROOT}/examples/hello.ax" -o "${BOOT_C}"
cc "${BOOT_C}" -o "${BOOT_BIN}"
BOOT_OUT="$("${BOOT_BIN}")"
test "${BOOT_OUT}" = "hello from Aurex M0"

"${ROOT}/tools/bootstrap_chain.sh" >"${TMP_DIR}/aurex_bootstrap_chain.txt"
grep -q 'bootstrap chain passed' "${TMP_DIR}/aurex_bootstrap_chain.txt"
"${ROOT}/tools/check_golden.sh" >"${TMP_DIR}/aurex_golden.txt"
grep -q 'golden tests passed' "${TMP_DIR}/aurex_golden.txt"
"${ROOT}/tools/compare_selfhost_lexer.sh" >"${TMP_DIR}/aurex_selfhost_lexer_compare.txt"
grep -q 'selfhost lexer matches Stage0 lexer for local corpus' "${TMP_DIR}/aurex_selfhost_lexer_compare.txt"

echo "0.1.2 tests passed"
