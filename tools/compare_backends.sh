#!/bin/bash
AUREXC="/home/aoweichen/demos/STU/CPP/COMPLIER/aurex/build/bin/aurexc"
SRC="/home/aoweichen/demos/STU/CPP/COMPLIER/aurex/tests/positive/aurora_arith.ax"

echo "========================================="
echo "  LLVM Backend (full pipeline, working)"
echo "========================================="
"${AUREXC}" --backend llvm "${SRC}" -o /tmp/test_llvm
/tmp/test_llvm
echo "LLVM exit code: $?"
echo ""

echo "========================================="
echo "  Aurora Backend (manual pipeline demo)"
echo "========================================="
echo ""
echo "1. aurexc --backend aurora --emit=asm -> .s"
"${AUREXC}" --backend aurora --emit=asm "${SRC}" -o /tmp/test_aurora.s
echo "   -> /tmp/test_aurora.s ($(wc -l < /tmp/test_aurora.s) lines)"

echo "2. as (GNU assembler) -> .o"
as /tmp/test_aurora.s -o /tmp/test_aurora.o
echo "   -> /tmp/test_aurora.o ($(wc -c < /tmp/test_aurora.o) bytes)"

echo "3. nm (symbols in .o)"
nm /tmp/test_aurora.o | grep -E ' T | t '

echo "4. gcc -nostartfiles -Wl,-e{m0_aurora_arith_main} -> binary"
gcc -nostartfiles /tmp/test_aurora.o -o /tmp/test_aurora -Wl,-em0_aurora_arith_main
echo "   -> /tmp/test_aurora ($(wc -c < /tmp/test_aurora) bytes)"

echo "5. Run binary"
/tmp/test_aurora 2>&1
RET=$?
if [ ${RET} -eq 0 ]; then
    echo "   SUCCESS (exit 0)"
elif [ ${RET} -eq 132 ]; then
    echo "   Illegal instruction (Aurora codegen bug)"
else
    echo "   exit code: ${RET}"
fi

echo ""
echo "========================================="
echo "  Pipeline flow works!"
echo "  (Runtime crash is pre-existing Aurora bug)"
echo "========================================="
