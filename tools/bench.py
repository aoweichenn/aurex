#!/usr/bin/env python3
"""Small performance smoke benchmark for Aurex M0V0.1.8.

This is not a statistically rigorous benchmark suite. It is a repeatable
engineering smoke test that catches obvious regressions in lexer/parser/IR/native
throughput while keeping the repository dependency-free.
"""

from __future__ import annotations

import pathlib
import subprocess
import tempfile
import time


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
M0C = BUILD / "m0c"


def run(cmd: list[str]) -> float:
    start = time.perf_counter()
    completed = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if completed.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(cmd)}\n{completed.stderr}")
    return time.perf_counter() - start


def make_source(function_count: int) -> str:
    functions = [
        "module bench;\n",
        "extern c { fn puts(s: *const u8) -> i32 @name(\"puts\"); }\n",
    ]
    for i in range(function_count):
        name = f"bench_fn_{i}"
        functions.append(
            f"fn {name}(x: i32) -> i32 {{\n"
            f"    var y: i32 = x;\n"
            f"    while y < {i + 2} {{\n"
            f"        y = y + 1;\n"
            f"    }}\n"
            f"    return y;\n"
            f"}}\n"
        )
    functions.append(
        "export c fn main(argc: i32, argv: *mut *mut u8) -> i32 {\n"
        "    puts(c\"bench\");\n"
        "    return bench_fn_0(0);\n"
        "}\n"
    )
    return "".join(functions)


def main() -> None:
    subprocess.run(["cmake", "-S", str(ROOT), "-B", str(BUILD)], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["cmake", "--build", str(BUILD), "-j"], check=True, stdout=subprocess.DEVNULL)
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = pathlib.Path(tmp)
        source = tmpdir / "bench.ax"
        output = tmpdir / "bench"
        source.write_text(make_source(80), encoding="utf-8")
        token_time = run([str(M0C), "--dump-tokens", str(source)])
        ast_time = run([str(M0C), "--dump-ast", str(source)])
        ir_time = run([str(M0C), "--emit=ir", str(source)])
        llvm_ir_time = run([str(M0C), "--emit=llvm-ir", str(source)])
        native_time = run([str(M0C), str(source), "-o", str(output)])
        print(f"tokens: {token_time:.4f}s")
        print(f"ast:    {ast_time:.4f}s")
        print(f"ir:     {ir_time:.4f}s")
        print(f"llvm:   {llvm_ir_time:.4f}s")
        print(f"native: {native_time:.4f}s")


if __name__ == "__main__":
    main()
