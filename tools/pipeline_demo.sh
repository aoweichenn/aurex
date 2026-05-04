#!/bin/bash
set -euo pipefail

AUREXC="/home/aoweichen/demos/STU/CPP/COMPLIER/aurex/build/bin/aurexc"
SRC="/home/aoweichen/demos/STU/CPP/COMPLIER/aurex/tests/positive/aurora_arith.ax"

echo "=== Step 1: Aurex (Aurora backend) -> AT&T assembly (.s) ==="
"${AUREXC}" --backend aurora --emit=asm "${SRC}" -o /tmp/test_ac.s
echo "Generated $(wc -l < /tmp/test_ac.s) lines of assembly"
echo ""
echo "--- asm snippet ---"
head -20 /tmp/test_ac.s
echo "..."

echo ""
echo "=== Step 2: GNU assembler (as) -> object file (.o) ==="
as /tmp/test_ac.s -o /tmp/test_ac.o
echo "Object file: $(wc -c < /tmp/test_ac.o) bytes"

echo ""
echo "=== Step 3: Symbols in .o ==="
nm /tmp/test_ac.o

echo ""
echo "=== Step 4: Link with custom entry point (no C runtime) ==="
gcc -nostartfiles -o /tmp/test_ac /tmp/test_ac.o -Wl,-em0_aurora_arith_main
echo "Binary: $(wc -c < /tmp/test_ac) bytes"

echo ""
echo "=== Step 5: Run binary ==="
/tmp/test_ac
RET=$?
echo "exit code = ${RET} (0 = success, all arithmetic checks passed)"

if [ "${RET}" -eq 0 ]; then
    echo ""
    echo "=== PIPELINE SUCCESS ==="
    echo ""
    echo "Command summary:"
    echo "  # Compile M0 -> x86 assembly"
    echo "  aurexc --backend aurora --emit=asm hello.ax -o hello.s"
    echo "  # Assemble"
    echo "  as hello.s -o hello.o"
    echo "  # Link"
    echo "  gcc -nostartfiles hello.o -o hello -Wl,-eSYMBOL"
    echo "  # Run"
    echo "  ./hello"
else
    echo "FAILED with exit ${RET}"
    exit 1
fi
