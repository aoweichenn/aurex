#!/usr/bin/env python3
"""Fast Google Benchmark redline checks for Aurex frontend hot paths.

Defaults are intentionally broad enough for developer machines. Tighten the
AUREX_PERF_* environment variables on stable CI hardware when the perf lane is
ready to become a release gate.
"""

from __future__ import annotations

import json
import os
import pathlib
import subprocess
import sys
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build-perf"))).resolve()
FRONTEND_BENCH = BUILD / "bin" / "aurex_frontend_bench"

BENCHMARK_MIN_TIME_SECONDS = "0.01s"
BENCHMARK_FILTER = "BM_LexMixed/64$|BM_SemaLookup/96$|BM_SemaGenerics/64$"

DEFAULT_LEX_MIXED_NS_PER_BYTE_MAX = 1_000.0
DEFAULT_SEMA_LOOKUP_NS_PER_ITEM_MAX = 500_000.0
DEFAULT_SEMA_GENERICS_NS_PER_ITEM_MAX = 1_000_000.0

TIME_UNIT_TO_NS = {
    "ns": 1.0,
    "us": 1_000.0,
    "ms": 1_000_000.0,
    "s": 1_000_000_000.0,
}


def env_float(name: str, fallback: float) -> float:
    text = os.environ.get(name)
    if text is None:
        return fallback
    try:
        return float(text)
    except ValueError:
        return fallback


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


def check(name: str, value: float, threshold: float) -> bool:
    print(f"{name}: {value:.3f} (max {threshold:.3f})")
    if value <= threshold:
        return True
    print(f"performance redline failed: {name} {value:.3f} > {threshold:.3f}", file=sys.stderr)
    return False


def main() -> int:
    thresholds = {
        "lex_mixed_ns_per_byte": env_float(
            "AUREX_PERF_LEX_MIXED_NS_PER_BYTE_MAX",
            DEFAULT_LEX_MIXED_NS_PER_BYTE_MAX,
        ),
        "sema_lookup_ns_per_item": env_float(
            "AUREX_PERF_SEMA_LOOKUP_NS_PER_ITEM_MAX",
            DEFAULT_SEMA_LOOKUP_NS_PER_ITEM_MAX,
        ),
        "sema_generics_ns_per_item": env_float(
            "AUREX_PERF_SEMA_GENERICS_NS_PER_ITEM_MAX",
            DEFAULT_SEMA_GENERICS_NS_PER_ITEM_MAX,
        ),
    }

    build_benchmark()
    data = run_benchmark_json()
    lex_mixed = find_benchmark(data, "BM_LexMixed/64")
    sema_lookup = find_benchmark(data, "BM_SemaLookup/96")
    sema_generics = find_benchmark(data, "BM_SemaGenerics/64")

    ok = True
    ok = check(
        "lex_mixed_ns_per_byte",
        ns_per_source_byte(lex_mixed),
        thresholds["lex_mixed_ns_per_byte"],
    ) and ok
    ok = check(
        "sema_lookup_ns_per_item",
        ns_per_item(sema_lookup, 96.0),
        thresholds["sema_lookup_ns_per_item"],
    ) and ok
    ok = check(
        "sema_generics_ns_per_item",
        ns_per_item(sema_generics, 64.0),
        thresholds["sema_generics_ns_per_item"],
    ) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, json.JSONDecodeError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
