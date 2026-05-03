#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
M0C="${BUILD_DIR}/m0c"
SELFHOST_IMPORT_FLAGS=(-I "${ROOT}/selfhost/src")

cmake -S "${ROOT}" -B "${BUILD_DIR}" >/dev/null
cmake --build "${BUILD_DIR}" -j >/dev/null

"${M0C}" --version | grep -q 'M0V0.1.8'
"${M0C}" --help | grep -q -- '--check'
"${M0C}" --help | grep -q -- '--emit=ast'
"${M0C}" --help | grep -q -- '--dump-modules'
"${M0C}" --check "${ROOT}/examples/hello.ax"
"${M0C}" --emit=check "${ROOT}/examples/hello.ax"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/bin/m0c_seed.ax"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_dump.ax"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/parser_smoke.ax"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_lang.ax"
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --check "${ROOT}/selfhost/src/aurex/selfhost/smoke/stage1_core.ax"
"${M0C}" --dump-tokens "${ROOT}/examples/hello.ax" >/tmp/aurex_tokens.txt
"${M0C}" --emit=ast "${ROOT}/examples/hello.ax" >/tmp/aurex_ast.txt
"${M0C}" --emit=checked "${ROOT}/examples/hello.ax" >/tmp/aurex_checked.txt
"${M0C}" --dump-modules "${ROOT}/tests/positive/module_math.ax" >/tmp/aurex_modules.txt
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${ROOT}/selfhost/src/aurex/selfhost/tool/lexer_file.ax" >/tmp/aurex_selfhost_modules.txt
"${M0C}" "${SELFHOST_IMPORT_FLAGS[@]}" --dump-modules "${ROOT}/selfhost/src/aurex/selfhost/smoke/parser_smoke.ax" >/tmp/aurex_selfhost_parser_modules.txt
grep -q 'c_string_literal' /tmp/aurex_tokens.txt
grep -q 'extern_block' /tmp/aurex_ast.txt
grep -q 'checked_module' /tmp/aurex_checked.txt
grep -q 'lib.math' /tmp/aurex_modules.txt
grep -q 'module_math' /tmp/aurex_modules.txt
grep -q 'aurex.selfhost.lexer.dump' /tmp/aurex_selfhost_modules.txt
grep -q 'aurex.selfhost.lexer.core' /tmp/aurex_selfhost_modules.txt
grep -q 'aurex.selfhost.parser.seed' /tmp/aurex_selfhost_parser_modules.txt
grep -q 'aurex.selfhost.parser.cursor' /tmp/aurex_selfhost_parser_modules.txt
grep -q 'aurex.selfhost.parser.expr' /tmp/aurex_selfhost_parser_modules.txt
grep -q 'aurex.selfhost.parser.types' /tmp/aurex_selfhost_parser_modules.txt
grep -q 'aurex.selfhost.lexer.core' /tmp/aurex_selfhost_parser_modules.txt
diff -u "${ROOT}/tests/golden/hello.tokens" /tmp/aurex_tokens.txt

"${M0C}" "${ROOT}/examples/hello.ax" -o "${BUILD_DIR}/hello.c"
cc "${BUILD_DIR}/hello.c" -o "${BUILD_DIR}/hello"
HELLO_OUT="$("${BUILD_DIR}/hello")"
test "${HELLO_OUT}" = "hello from Aurex M0"

for src in "${ROOT}"/tests/positive/*.ax; do
    case "$(basename "${src}" .ax)" in
        import_path|module_name_collision|runtime_text|runtime_mem|runtime_file)
            continue
            ;;
    esac
    out="${BUILD_DIR}/$(basename "${src}" .ax).c"
    bin="${BUILD_DIR}/$(basename "${src}" .ax)"
    "${M0C}" "${src}" -o "${out}"
    cc "${out}" -o "${bin}"
    case "$(basename "${src}" .ax)" in
        condition_regression|pointer_ops)
            "${bin}" >/dev/null
            ;;
        address_of_let|pointer_field_write|eval_order_call_stmt|eval_order_return|eval_order_assign|eval_order_condition|builtins)
            "${bin}" >/dev/null
            ;;
    esac
done

for src in "${ROOT}"/tests/positive/runtime_*.ax; do
    out="${BUILD_DIR}/$(basename "${src}" .ax).c"
    bin="${BUILD_DIR}/$(basename "${src}" .ax)"
    "${M0C}" -I "${ROOT}" "${src}" -o "${out}"
    cc "${out}" -o "${bin}"
    "${bin}" >/dev/null
done

"${M0C}" -I "${ROOT}/tests/imports" "${ROOT}/tests/positive/import_path.ax" -o "${BUILD_DIR}/import_path.c"
cc "${BUILD_DIR}/import_path.c" -o "${BUILD_DIR}/import_path"
"${BUILD_DIR}/import_path" >/dev/null

"${M0C}" -I "${ROOT}/tests/imports" "${ROOT}/tests/positive/module_name_collision.ax" -o "${BUILD_DIR}/module_name_collision.c"
grep -q 'm0_module_name_collision_helper' "${BUILD_DIR}/module_name_collision.c"
grep -q 'm0_collide_a_helper' "${BUILD_DIR}/module_name_collision.c"
cc "${BUILD_DIR}/module_name_collision.c" -o "${BUILD_DIR}/module_name_collision"
"${BUILD_DIR}/module_name_collision" >/dev/null

for src in "${ROOT}"/tests/negative/*.ax; do
    if [ "$(basename "${src}" .ax)" = "module_name_mismatch" ]; then
        if "${M0C}" -I "${ROOT}/tests/imports" "${src}" -o "${BUILD_DIR}/negative.c" >/tmp/aurex_negative.out 2>/tmp/aurex_negative.err; then
            echo "expected semantic failure: ${src}" >&2
            exit 1
        fi
        continue
    fi
    if [ "$(basename "${src}" .ax)" = "cyclic_import" ]; then
        if "${M0C}" -I "${ROOT}/tests/imports" "${src}" -o "${BUILD_DIR}/negative.c" >/tmp/aurex_negative.out 2>/tmp/aurex_negative.err; then
            echo "expected semantic failure: ${src}" >&2
            exit 1
        fi
        continue
    fi
    if [ "$(basename "${src}" .ax)" = "ambiguous_import_name" ]; then
        if "${M0C}" -I "${ROOT}/tests/imports" "${src}" -o "${BUILD_DIR}/negative.c" >/tmp/aurex_negative.out 2>/tmp/aurex_negative.err; then
            echo "expected semantic failure: ${src}" >&2
            exit 1
        fi
        grep -q 'ambiguous function name' /tmp/aurex_negative.err
        continue
    fi
    if "${M0C}" "${src}" -o "${BUILD_DIR}/negative.c" >/tmp/aurex_negative.out 2>/tmp/aurex_negative.err; then
        echo "expected semantic failure: ${src}" >&2
        exit 1
    fi
done

make -C "${ROOT}/bootstrap" >/dev/null
BOOT_C="${BUILD_DIR}/hello.bootstrap.c"
BOOT_BIN="${BUILD_DIR}/hello.bootstrap"
"${ROOT}/bootstrap/m0_bootstrap" "${ROOT}/examples/hello.ax" -o "${BOOT_C}"
cc "${BOOT_C}" -o "${BOOT_BIN}"
BOOT_OUT="$("${BOOT_BIN}")"
test "${BOOT_OUT}" = "hello from Aurex M0"

"${ROOT}/tools/bootstrap_chain.sh" >/tmp/aurex_bootstrap_chain.txt
grep -q 'bootstrap chain passed' /tmp/aurex_bootstrap_chain.txt
"${ROOT}/tools/check_golden.sh" >/tmp/aurex_golden.txt
grep -q 'golden tests passed' /tmp/aurex_golden.txt
"${ROOT}/tools/compare_selfhost_lexer.sh" >/tmp/aurex_selfhost_lexer_compare.txt
grep -q 'selfhost lexer matches Stage0 lexer for local corpus' /tmp/aurex_selfhost_lexer_compare.txt

echo "M0V0.1.8 tests passed"
