#!/usr/bin/env python3
"""Google Benchmark baseline against modern language frontend drivers.

The comparison is intentionally report-only. It measures end-to-end frontend
driver checks for analogous generated sources:

* Aurex: lexer + parser + sema via --check
* Clang/G++: C++20 parser + sema via -fsyntax-only
* rustc: stable check-like metadata emission via --emit=metadata

Do not turn these numbers into thresholds until the machine profile and
cross-compiler workload mix are stable.
"""

from __future__ import annotations

import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build-perf"))).resolve()
AUREXC = BUILD / "bin" / "aurexc"
COMPARE_BENCH = BUILD / "bin" / "aurex_frontend_compare_bench"
OUTPUT_JSON = BUILD / "frontend-compare.json"

BENCHMARK_MIN_TIME_SECONDS = os.environ.get("AUREX_COMPARE_MIN_TIME", "0.01s")
WORKLOADS = ("lookup", "generics")
WORKLOAD_ITEM_COUNT = 96.0

TIME_UNIT_TO_NS = {
    "ns": 1.0,
    "us": 1_000.0,
    "ms": 1_000_000.0,
    "s": 1_000_000_000.0,
}


@dataclass(frozen=True)
class CompilerSpec:
    key: str
    label: str
    env: str
    default: str | None
    mode: str


@dataclass(frozen=True)
class CompilerInfo:
    spec: CompilerSpec
    path: pathlib.Path | None
    version: str | None


COMPILERS = (
    CompilerSpec("aurex", "Aurex", "AUREX_COMPARE_AUREXC", None, "--check"),
    CompilerSpec("clangxx", "Clang++", "AUREX_COMPARE_CLANGXX", "clang++", "-std=c++20 -fsyntax-only -w"),
    CompilerSpec("gxx", "G++", "AUREX_COMPARE_GXX", "g++", "-std=c++20 -fsyntax-only -w"),
    CompilerSpec("rustc", "rustc", "AUREX_COMPARE_RUSTC", "rustc", "--edition=2021 --emit=metadata"),
)


def run(cmd: list[str], *, env: dict[str, str] | None = None) -> str:
    completed = subprocess.run(
        cmd,
        cwd=ROOT,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
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
            "aurexc",
            "aurex_frontend_compare_bench",
            "-j",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )


def resolve_compiler(spec: CompilerSpec) -> pathlib.Path | None:
    if spec.key == "aurex":
        return AUREXC if AUREXC.exists() and os.access(AUREXC, os.X_OK) else None

    requested = os.environ.get(spec.env)
    if requested:
        path = pathlib.Path(requested).expanduser()
        if path.is_absolute() or path.parent != pathlib.Path("."):
            return path if path.exists() and os.access(path, os.X_OK) else None
        found = shutil.which(requested)
        return pathlib.Path(found) if found else None

    if spec.default is None:
        return None
    found = shutil.which(spec.default)
    return pathlib.Path(found) if found else None


def first_version_line(path: pathlib.Path | None) -> str | None:
    if path is None:
        return None
    completed = subprocess.run(
        [str(path), "--version"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if completed.returncode != 0:
        return None
    for line in completed.stdout.splitlines():
        stripped = line.strip()
        if stripped:
            return stripped
    return None


def discover_compilers() -> list[CompilerInfo]:
    infos: list[CompilerInfo] = []
    for spec in COMPILERS:
        path = resolve_compiler(spec)
        infos.append(CompilerInfo(spec, path, first_version_line(path)))
    return infos


def available_filter(infos: list[CompilerInfo]) -> str:
    available = [re.escape(info.spec.key) for info in infos if info.path is not None]
    if not available:
        raise RuntimeError("no frontend compilers are available")
    return rf"BM_FrontendCheck/({'|'.join(available)})_({'|'.join(WORKLOADS)})_96(/real_time)?$"


def benchmark_env(infos: list[CompilerInfo]) -> dict[str, str]:
    env = os.environ.copy()
    for info in infos:
        if info.path is not None:
            env[info.spec.env] = str(info.path)
    return env


def run_benchmark_json(infos: list[CompilerInfo]) -> dict[str, Any]:
    output = run(
        [
            str(COMPARE_BENCH),
            "--benchmark_format=json",
            f"--benchmark_min_time={BENCHMARK_MIN_TIME_SECONDS}",
            f"--benchmark_filter={available_filter(infos)}",
        ],
        env=benchmark_env(infos),
    )
    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(output, encoding="utf-8")
    return json.loads(output)


def real_time_ns(benchmark: dict[str, Any]) -> float:
    unit = str(benchmark.get("time_unit", "ns"))
    scale = TIME_UNIT_TO_NS.get(unit)
    if scale is None:
        raise RuntimeError(f"unsupported benchmark time unit: {unit}")
    return float(benchmark["real_time"]) * scale


def benchmark_map(data: dict[str, Any]) -> dict[tuple[str, str], dict[str, Any]]:
    rows: dict[tuple[str, str], dict[str, Any]] = {}
    pattern = re.compile(r"^BM_FrontendCheck/([^_]+)_(lookup|generics)_96(?:/real_time)?$")
    for benchmark in data.get("benchmarks", []):
        if benchmark.get("run_type", "iteration") != "iteration":
            continue
        name = str(benchmark.get("name", ""))
        match = pattern.match(name)
        if match is None:
            continue
        compiler, workload = match.groups()
        rows[(compiler, workload)] = benchmark
    return rows


def ns_per_item(benchmark: dict[str, Any]) -> float:
    item_count = float(benchmark.get("workload_items", WORKLOAD_ITEM_COUNT))
    if item_count <= 0.0:
        item_count = WORKLOAD_ITEM_COUNT
    return real_time_ns(benchmark) / item_count


def print_compilers(infos: list[CompilerInfo]) -> None:
    print("compilers:")
    for info in infos:
        if info.path is None:
            print(f"  {info.spec.label:<8} missing ({info.spec.default or info.spec.env})")
            continue
        version = info.version or "version unavailable"
        print(f"  {info.spec.label:<8} {info.path} [{version}]")


def print_report(data: dict[str, Any], infos: list[CompilerInfo]) -> None:
    rows = benchmark_map(data)
    available_infos = [info for info in infos if info.path is not None]

    print("Modern frontend comparison baseline (Google Benchmark real_time)")
    print(f"build: {BUILD}")
    print(f"raw_json: {OUTPUT_JSON}")
    print()
    print_compilers(infos)
    print()
    print("modes:")
    for info in infos:
        print(f"  {info.spec.label:<8} {info.spec.mode}")
    print()
    print("No thresholds are enforced. Values include compiler process startup and frontend/check work.")
    print("rustc uses stable metadata emission because stable rustc has no direct -fsyntax-only equivalent.")
    print("vs_aurex is same-workload elapsed-time ratio; values above 1.00x are slower than Aurex.")
    print()
    print(f"{'workload':<12} {'compiler':<8} {'source_KiB':>11} {'real_time_ms':>14} {'us/item':>12} {'vs_aurex':>10}")
    print(f"{'-' * 12} {'-' * 8} {'-' * 11} {'-' * 14} {'-' * 12} {'-' * 10}")

    for workload in WORKLOADS:
        aurex_benchmark = rows.get(("aurex", workload))
        aurex_time = real_time_ns(aurex_benchmark) if aurex_benchmark is not None else None
        for info in available_infos:
            benchmark = rows.get((info.spec.key, workload))
            if benchmark is None:
                print(f"{workload + '/96':<12} {info.spec.label:<8} {'missing':>11} {'':>14} {'':>12} {'':>10}")
                continue
            if benchmark.get("error_occurred"):
                message = str(benchmark.get("error_message", "benchmark error"))
                print(f"{workload + '/96':<12} {info.spec.label:<8} {'error':>11} {'':>14} {'':>12} {message}")
                continue
            elapsed_ns = real_time_ns(benchmark)
            source_kib = float(benchmark.get("source_bytes", 0.0)) / 1024.0
            if aurex_time is None or aurex_time <= 0.0:
                ratio = "n/a"
            else:
                ratio = f"{elapsed_ns / aurex_time:.2f}x"
            print(
                f"{workload + '/96':<12} "
                f"{info.spec.label:<8} "
                f"{source_kib:>11.1f} "
                f"{elapsed_ns / 1_000_000.0:>14.3f} "
                f"{ns_per_item(benchmark) / 1_000.0:>12.3f} "
                f"{ratio:>10}"
            )


def main() -> int:
    build_benchmark()
    infos = discover_compilers()
    print_report(run_benchmark_json(infos), infos)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, json.JSONDecodeError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
