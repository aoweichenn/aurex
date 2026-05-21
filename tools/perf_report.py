#!/usr/bin/env python3
"""Google Benchmark performance report for Aurex frontend hot paths.

This reports the current baseline without enforcing thresholds. Keep threshold
policy out of this script until the cross-compiler comparison lane has a stable
baseline and machine profile.
"""

from __future__ import annotations

import json
import os
import pathlib
import subprocess
import sys
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build" / "perf"))).resolve()
FRONTEND_BENCH = BUILD / "bin" / "aurex_frontend_bench"

BENCHMARK_MIN_TIME_SECONDS = "0.01s"
BENCHMARK_FILTER = "BM_LexMixed/64$|BM_SemaLookup/96$|BM_SemaGenerics/64$|BM_SemaAstBulk/1024$"

TIME_UNIT_TO_NS = {
    "ns": 1.0,
    "us": 1_000.0,
    "ms": 1_000_000.0,
    "s": 1_000_000_000.0,
}


def run(cmd: list[str]) -> str:
    completed = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if completed.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(cmd)}\n{completed.stderr}")
    return completed.stdout


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


def build_benchmark() -> None:
    configure()
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


def run_benchmark_json() -> dict[str, Any]:
    output = run([
        str(FRONTEND_BENCH),
        "--benchmark_format=json",
        f"--benchmark_min_time={BENCHMARK_MIN_TIME_SECONDS}",
        f"--benchmark_filter={BENCHMARK_FILTER}",
    ])
    return json.loads(output)


def find_benchmark(data: dict[str, Any], name: str) -> dict[str, Any]:
    for benchmark in data.get("benchmarks", []):
        if benchmark.get("run_type", "iteration") == "iteration" and benchmark.get("name") == name:
            return benchmark
    available = ", ".join(str(entry.get("name", "<unnamed>")) for entry in data.get("benchmarks", []))
    raise RuntimeError(f"missing benchmark {name}; available: {available}")


def cpu_time_ns(benchmark: dict[str, Any]) -> float:
    unit = str(benchmark.get("time_unit", "ns"))
    scale = TIME_UNIT_TO_NS.get(unit)
    if scale is None:
        raise RuntimeError(f"unsupported benchmark time unit: {unit}")
    return float(benchmark["cpu_time"]) * scale


def ns_per_source_byte(benchmark: dict[str, Any]) -> float:
    source_bytes = float(benchmark.get("source_bytes", 0.0))
    if source_bytes > 0.0:
        return cpu_time_ns(benchmark) / source_bytes
    bytes_per_second = float(benchmark.get("bytes_per_second", 0.0))
    if bytes_per_second <= 0.0:
        raise RuntimeError(f"missing source byte counter for {benchmark.get('name', '<unnamed>')}")
    return 1_000_000_000.0 / bytes_per_second


def ns_per_item(benchmark: dict[str, Any], item_count: float) -> float:
    if item_count <= 0.0:
        raise RuntimeError("item count must be positive")
    return cpu_time_ns(benchmark) / item_count


def print_report(data: dict[str, Any]) -> None:
    lex_mixed = find_benchmark(data, "BM_LexMixed/64")
    sema_lookup = find_benchmark(data, "BM_SemaLookup/96")
    sema_generics = find_benchmark(data, "BM_SemaGenerics/64")
    sema_ast_bulk = find_benchmark(data, "BM_SemaAstBulk/1024")

    print("Aurex frontend Google Benchmark baseline")
    print(f"build: {BUILD}")
    print()
    print(f"{'case':<24} {'cpu_time_ns':>14} {'normalized':>18}")
    print(f"{'-' * 24} {'-' * 14} {'-' * 18}")
    print(
        f"{'lex_mixed/64':<24} "
        f"{cpu_time_ns(lex_mixed):>14.3f} "
        f"{ns_per_source_byte(lex_mixed):>14.3f} ns/B"
    )
    print(
        f"{'sema_lookup/96':<24} "
        f"{cpu_time_ns(sema_lookup):>14.3f} "
        f"{ns_per_item(sema_lookup, 96.0):>14.3f} ns/item"
    )
    print(
        f"{'sema_generics/64':<24} "
        f"{cpu_time_ns(sema_generics):>14.3f} "
        f"{ns_per_item(sema_generics, 64.0):>14.3f} ns/item"
    )
    print(
        f"{'sema_ast_bulk/1024':<24} "
        f"{cpu_time_ns(sema_ast_bulk):>14.3f} "
        f"{ns_per_item(sema_ast_bulk, float(sema_ast_bulk.get('ast_exprs', 1024.0))):>14.3f} ns/expr"
    )


def main() -> int:
    build_benchmark()
    print_report(run_benchmark_json())
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, json.JSONDecodeError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
