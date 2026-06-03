#!/usr/bin/env python3
"""Performance smoke benchmark for Aurex M2.

Compiler phase timings here are coarse smoke checks. Frontend hot-path
measurements are delegated to Google Benchmark so the microbenchmark runner,
warmup, timing loop, and report format are not handwritten.
"""

from __future__ import annotations

import os
import pathlib
import subprocess
import tempfile
import time


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build" / "perf"))).resolve()
AUREXC = BUILD / "bin" / "aurexc"
FRONTEND_BENCH = BUILD / "bin" / "aurex_frontend_bench"
BENCHMARK_MIN_TIME_SECONDS = "0.01s"


def run(cmd: list[str]) -> float:
    start = time.perf_counter()
    completed = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if completed.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(cmd)}\n{completed.stderr}")
    return time.perf_counter() - start


def run_capture(cmd: list[str]) -> str:
    completed = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if completed.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(cmd)}\n{completed.stderr}")
    return completed.stdout.strip()


def configure() -> None:
    cmd = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(BUILD),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DAUREX_BUILD_BENCHMARKS=ON",
    ]
    cc = os.environ.get("CC")
    cxx = os.environ.get("CXX")
    if cc:
        cmd.append(f"-DCMAKE_C_COMPILER={cc}")
    if cxx:
        cmd.append(f"-DCMAKE_CXX_COMPILER={cxx}")
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)


def make_source(function_count: int) -> str:
    functions = [
        "module bench;\n",
        "extern c { @name(\"puts\") fn puts(s: *const u8) -> i32; }\n",
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
        "fn main() -> i32 {\n"
        "    puts(c\"bench\");\n"
        "    return bench_fn_0(0);\n"
        "}\n"
    )
    return "".join(functions)


def main() -> None:
    configure()
    subprocess.run(["cmake", "--build", str(BUILD), "-j"], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(
        [
            "cmake",
            "--build",
            str(BUILD),
            "--target",
            "aurex_frontend_bench",
            "-j",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = pathlib.Path(tmp)
        source = tmpdir / "bench.ax"
        output = tmpdir / "bench"
        source.write_text(make_source(80), encoding="utf-8")
        token_time = run([str(AUREXC), "--dump-tokens", str(source)])
        ast_time = run([str(AUREXC), "--dump-ast", str(source)])
        ir_time = run([str(AUREXC), "--emit=ir", str(source)])
        llvm_ir_time = run([str(AUREXC), "--emit=llvm-ir", str(source)])
        native_time = run([str(AUREXC), str(source), "-o", str(output)])
        print(f"tokens: {token_time:.4f}s")
        print(f"ast:    {ast_time:.4f}s")
        print(f"ir:     {ir_time:.4f}s")
        print(f"llvm:   {llvm_ir_time:.4f}s")
        print(f"native: {native_time:.4f}s")
        print()
        print("[google benchmark frontend]")
        print(run_capture([
            str(FRONTEND_BENCH),
            f"--benchmark_min_time={BENCHMARK_MIN_TIME_SECONDS}",
            "--benchmark_color=false",
        ]))


if __name__ == "__main__":
    main()
