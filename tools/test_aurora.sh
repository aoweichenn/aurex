#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
AUREXC="${BUILD_DIR}/bin/aurexc"
TMP_DIR="${BUILD_DIR}/tmp"

mkdir -p "${TMP_DIR}"

echo "=== Aurora Backend Tests ==="

echo -n "  CLI --backend help... "
"${AUREXC}" --help | grep -q -- '--backend'
echo "ok"

echo -n "  Aurora --check hello.ax... "
"${AUREXC}" --backend aurora --check "${ROOT}/examples/hello.ax"
echo "ok"

echo -n "  Aurora --emit=ir... "
"${AUREXC}" --backend aurora --emit=ir "${ROOT}/examples/hello.ax" >"${TMP_DIR}/aurora_ir.txt"
grep -q 'aurex_ir v0' "${TMP_DIR}/aurora_ir.txt"
echo "ok"

echo -n "  Aurora --emit=asm... "
"${AUREXC}" --backend aurora --emit=asm "${ROOT}/examples/hello.ax" -o "${TMP_DIR}/aurora_hello.s"
test -s "${TMP_DIR}/aurora_hello.s"
grep -q 'm0_hello_main' "${TMP_DIR}/aurora_hello.s"
grep -q 'call' "${TMP_DIR}/aurora_hello.s"
echo "ok"

echo -n "  Aurora --emit=obj... "
"${AUREXC}" --backend aurora --emit=obj "${ROOT}/examples/hello.ax" -o "${TMP_DIR}/aurora_hello.o"
test -s "${TMP_DIR}/aurora_hello.o"
echo "ok"

echo -n "  LLVM regression --emit=exe... "
"${AUREXC}" --backend llvm --emit=exe "${ROOT}/examples/hello.ax" -o "${TMP_DIR}/hello_llvm"
test "$("${TMP_DIR}/hello_llvm")" = "hello from Aurex M0"
echo "ok"

for name in aurora_arith aurora_control_flow aurora_calls; do
    src="${ROOT}/tests/positive/${name}.ax"
    echo -n "  Aurora positive/${name}.ax --check... "
    "${AUREXC}" --backend aurora --check "${src}"
    echo "ok"
    echo -n "  Aurora positive/${name}.ax --emit=asm... "
    "${AUREXC}" --backend aurora --emit=asm "${src}" -o "${TMP_DIR}/${name}_aurora.s"
    test -s "${TMP_DIR}/${name}_aurora.s"
    echo "ok"
    echo -n "  Aurora positive/${name}.ax --emit=obj... "
    "${AUREXC}" --backend aurora --emit=obj "${src}" -o "${TMP_DIR}/${name}_aurora.o"
    test -s "${TMP_DIR}/${name}_aurora.o"
    echo "ok"
    echo -n "  LLVM positive/${name}.ax --emit=exe... "
    "${AUREXC}" --backend llvm "${src}" -o "${TMP_DIR}/${name}_llvm"
    "${TMP_DIR}/${name}_llvm" >/dev/null
    echo "ok"
done

echo -n "  Aurora --dump-llvm-ir (expect failure)... "
if "${AUREXC}" --backend aurora --dump-llvm-ir "${ROOT}/examples/hello.ax" >/dev/null 2>"${TMP_DIR}/aurora_llvm_err.txt"; then
    echo "UNEXPECTED SUCCESS"
    exit 1
fi
grep -qE 'not available|IR text' "${TMP_DIR}/aurora_llvm_err.txt"
echo "ok"

echo -n "  --backend invalid (expect failure)... "
if "${AUREXC}" --backend invalid --check "${ROOT}/examples/hello.ax" 2>/dev/null; then
    echo "UNEXPECTED SUCCESS"
    exit 1
fi
echo "ok"

echo ""
echo "=== All Aurora tests passed ==="
