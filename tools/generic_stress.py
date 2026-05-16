#!/usr/bin/env python3
"""Generic instantiation RSS/time stress baseline for Aurex.

This is a report-only perf lane. It generates large generic-heavy Aurex
modules, runs `aurexc`, and records elapsed time plus peak RSS. Keep threshold
policy out of this script until the baseline is stable across machines and
compiler configurations.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import platform
import re
import subprocess
import sys
import time
from dataclasses import asdict, dataclass


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(
    os.environ.get(
        "AUREX_STRESS_BUILD_DIR",
        os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build-perf")),
    )
).resolve()
AUREXC = BUILD / "bin" / "aurexc"
GENERATED_ROOT = BUILD / "generated" / "generic-stress"
OUTPUT_JSON = BUILD / "generic-stress.json"

DEFAULT_COUNTS = (500, 1000, 2000, 5000)
DEFAULT_EMIT_KIND = "check"
DEFAULT_SHAPE = "instances"
GENERIC_STRESS_EMIT_KINDS = {
    "check",
    "checked",
    "typed",
    "ir",
    "llvm-ir",
}
GENERIC_STRESS_SHAPES = {
    "instances",
    "templates",
}
GENERIC_STRESS_TIME_PARSE_PATTERN = re.compile(
    r"(?:maximum resident set size:\s+(\d+)|(\d+)\s+maximum resident set size)",
    re.IGNORECASE,
)
GENERIC_STRESS_GNU_TIME_PATTERN = re.compile(r"Maximum resident set size \(kbytes\):\s+(\d+)", re.IGNORECASE)


@dataclass(frozen=True)
class StressRow:
    instances: int
    emit_kind: str
    shape: str
    source_bytes: int
    elapsed_ms: float
    max_rss_mib: float | None
    source: str


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


def build_compiler() -> None:
    configure()
    subprocess.run(
        [
            "cmake",
            "--build",
            str(BUILD),
            "--target",
            "aurexc",
            "-j",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )


def append_payload_record(source: list[str], index: int) -> None:
    suffix = str(index)
    source.append(
        f"struct Payload{suffix} {{\n"
        "    value: i32;\n"
        "}\n\n"
    )


def append_payload_use_function(source: list[str], index: int) -> None:
    suffix = str(index)
    source.append(
        f"fn use_payload{suffix}(seed: i32) -> i32 {{\n"
        f"    let payload: Payload{suffix} = Payload{suffix} {{ value: seed }};\n"
        f"    let boxed: Box[Payload{suffix}] = make_box[Payload{suffix}](payload);\n"
        f"    let nested: Box[Box[Payload{suffix}]] = make_box[Box[Payload{suffix}]](boxed);\n"
        f"    let unwrapped: Payload{suffix} = unwrap_box[Payload{suffix}](unwrap_box[Box[Payload{suffix}]](nested));\n"
        f"    let maybe: Maybe[Payload{suffix}] = Maybe[Payload{suffix}].some(unwrapped);\n"
        "    return match maybe { .some(inner) => id[i32](inner.value), .none => 0 };\n"
        "}\n\n"
    )


def make_generic_stress_source(instance_count: int) -> str:
    source: list[str] = []
    source.append(
        "module perf.generic_stress;\n\n"
        "struct Box[T] {\n"
        "    value: T;\n"
        "}\n\n"
        "enum Maybe[T] {\n"
        "    some(T),\n"
        "    none\n"
        "}\n\n"
        "fn id[T](value: T) -> T {\n"
        "    return value;\n"
        "}\n\n"
        "fn make_box[T](value: T) -> Box[T] {\n"
        "    return Box[T] { value: value };\n"
        "}\n\n"
        "fn unwrap_box[T](box: Box[T]) -> T {\n"
        "    return box.value;\n"
        "}\n\n"
    )
    for index in range(instance_count):
        append_payload_record(source, index)
    for index in range(instance_count):
        append_payload_use_function(source, index)
    source.append("fn main() -> i32 {\n    var total: i32 = 0;\n")
    for index in range(instance_count):
        source.append(f"    total = total + use_payload{index}({index});\n")
    source.append("    return total;\n}\n")
    return "".join(source)


def make_distinct_generic_template_stress_source(template_count: int) -> str:
    source: list[str] = []
    source.append("module perf.generic_stress;\n\n")
    for index in range(template_count):
        suffix = str(index)
        source.append(
            f"fn id{suffix}[T](value: T) -> T {{\n"
            "    return value;\n"
            "}\n\n"
        )
    source.append("fn main() -> i32 {\n    var total: i32 = 0;\n")
    for index in range(template_count):
        source.append(f"    total = total + id{index}[i32]({index});\n")
    source.append("    return total;\n}\n")
    return "".join(source)


def make_stress_source(instance_count: int, shape: str) -> str:
    if shape == "templates":
        return make_distinct_generic_template_stress_source(instance_count)
    return make_generic_stress_source(instance_count)


def write_source(instance_count: int, shape: str) -> pathlib.Path:
    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    path = GENERATED_ROOT / f"generic_stress_{shape}_{instance_count}.ax"
    source = make_stress_source(instance_count, shape)
    path.write_text(source, encoding="utf-8")
    return path


def parse_peak_rss_mib(stderr: str) -> float | None:
    if platform.system() == "Darwin":
        match = GENERIC_STRESS_TIME_PARSE_PATTERN.search(stderr)
        if match is None:
            return None
        rss_bytes = int(next(group for group in match.groups() if group is not None))
        return rss_bytes / (1024.0 * 1024.0)
    match = GENERIC_STRESS_GNU_TIME_PATTERN.search(stderr)
    if match is None:
        return None
    rss_kib = int(match.group(1))
    return rss_kib / 1024.0


def timed_command(source: pathlib.Path, emit_kind: str) -> tuple[float, float | None]:
    time_tool = pathlib.Path("/usr/bin/time")
    base_cmd = [str(AUREXC), f"--emit={emit_kind}", str(source)]
    if time_tool.exists() and os.access(time_tool, os.X_OK):
        if platform.system() == "Darwin":
            cmd = [str(time_tool), "-l", *base_cmd]
        else:
            cmd = [str(time_tool), "-v", *base_cmd]
    else:
        cmd = base_cmd

    started = time.perf_counter()
    completed = subprocess.run(
        cmd,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed: {' '.join(base_cmd)}\n"
            f"stdout:\n{completed.stdout}\n"
            f"stderr:\n{completed.stderr}"
        )
    return elapsed_ms, parse_peak_rss_mib(completed.stderr)


def parse_counts(text: str | None) -> list[int]:
    if not text:
        return list(DEFAULT_COUNTS)
    counts: list[int] = []
    for part in text.split(","):
        stripped = part.strip()
        if not stripped:
            continue
        count = int(stripped)
        if count <= 0:
            raise ValueError("counts must be positive")
        counts.append(count)
    if not counts:
        raise ValueError("at least one count is required")
    return counts


def parse_emit_kind(text: str | None) -> str:
    emit_kind = DEFAULT_EMIT_KIND if text is None else text.strip()
    if emit_kind not in GENERIC_STRESS_EMIT_KINDS:
        valid = ", ".join(sorted(GENERIC_STRESS_EMIT_KINDS))
        raise ValueError(f"unsupported emit kind: {emit_kind}; expected one of: {valid}")
    return emit_kind


def parse_shape(text: str | None) -> str:
    shape = DEFAULT_SHAPE if text is None else text.strip()
    if shape not in GENERIC_STRESS_SHAPES:
        valid = ", ".join(sorted(GENERIC_STRESS_SHAPES))
        raise ValueError(f"unsupported stress shape: {shape}; expected one of: {valid}")
    return shape


def display_path(path: pathlib.Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def run_stress(counts: list[int], emit_kind: str, shape: str) -> list[StressRow]:
    rows: list[StressRow] = []
    for count in counts:
        source = write_source(count, shape)
        elapsed_ms, max_rss_mib = timed_command(source, emit_kind)
        rows.append(
            StressRow(
                instances=count,
                emit_kind=emit_kind,
                shape=shape,
                source_bytes=source.stat().st_size,
                elapsed_ms=elapsed_ms,
                max_rss_mib=max_rss_mib,
                source=display_path(source),
            )
        )
    return rows


def write_json(rows: list[StressRow]) -> None:
    payload = {
        "build": str(BUILD),
        "aurexc": str(AUREXC),
        "machine": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor(),
        },
        "rows": [asdict(row) for row in rows],
    }
    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def print_report(rows: list[StressRow]) -> None:
    print("Aurex generic instantiation stress baseline")
    print(f"build: {BUILD}")
    print(f"raw_json: {OUTPUT_JSON}")
    print()
    print("No thresholds are enforced. RSS is peak process resident set size when /usr/bin/time exposes it.")
    print()
    print(f"{'instances':>10} {'emit':>8} {'shape':>10} {'source_KiB':>12} {'elapsed_ms':>12} {'peak_RSS_MiB':>14} {'source':<36}")
    print(f"{'-' * 10} {'-' * 8} {'-' * 10} {'-' * 12} {'-' * 12} {'-' * 14} {'-' * 36}")
    for row in rows:
        rss = "n/a" if row.max_rss_mib is None else f"{row.max_rss_mib:.1f}"
        print(
            f"{row.instances:>10} "
            f"{row.emit_kind:>8} "
            f"{row.shape:>10} "
            f"{row.source_bytes / 1024.0:>12.1f} "
            f"{row.elapsed_ms:>12.3f} "
            f"{rss:>14} "
            f"{row.source:<36}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--counts",
        default=os.environ.get("AUREX_GENERIC_STRESS_COUNTS"),
        help="comma-separated instance counts; default: 500,1000,2000,5000",
    )
    parser.add_argument(
        "--emit",
        default=os.environ.get("AUREX_GENERIC_STRESS_EMIT", DEFAULT_EMIT_KIND),
        help="aurexc emit kind to stress: check, checked, typed, ir, llvm-ir; default: check",
    )
    parser.add_argument(
        "--shape",
        default=os.environ.get("AUREX_GENERIC_STRESS_SHAPE", DEFAULT_SHAPE),
        help="stress source shape: instances reuses a few templates over many types; templates creates many distinct generic functions; default: instances",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="reuse the existing build-perf aurexc binary",
    )
    args = parser.parse_args()

    counts = parse_counts(args.counts)
    emit_kind = parse_emit_kind(args.emit)
    shape = parse_shape(args.shape)
    if not args.skip_build:
        build_compiler()
    if not AUREXC.exists():
        raise RuntimeError(f"missing aurexc binary: {AUREXC}")
    rows = run_stress(counts, emit_kind, shape)
    write_json(rows)
    print_report(rows)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
