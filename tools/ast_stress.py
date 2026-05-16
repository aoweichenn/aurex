#!/usr/bin/env python3
"""Bulk AST RSS/time stress baseline and threshold gate for Aurex.

This lane targets the P0-Perf-4 fat-AST path from the review: generate a large
module with many expression and statement nodes, run `aurexc --check`, and
record elapsed time plus peak RSS. Optional thresholds make the same lane usable
as a local or CI quality gate.
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
GENERATED_ROOT = BUILD / "generated" / "ast-stress"
OUTPUT_JSON = BUILD / "ast-stress.json"

DEFAULT_COUNTS = (10_000, 50_000, 100_000)
AST_STRESS_TIME_PARSE_PATTERN = re.compile(
    r"(?:maximum resident set size:\s+(\d+)|(\d+)\s+maximum resident set size)",
    re.IGNORECASE,
)
AST_STRESS_GNU_TIME_PATTERN = re.compile(r"Maximum resident set size \(kbytes\):\s+(\d+)", re.IGNORECASE)


@dataclass(frozen=True)
class StressRow:
    statements: int
    source_bytes: int
    elapsed_ms: float
    max_rss_mib: float | None
    source: str


@dataclass(frozen=True)
class StressThresholds:
    max_elapsed_ms: float | None
    max_rss_mib: float | None


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


def make_ast_stress_source(statement_count: int) -> str:
    source: list[str] = [
        "module perf.ast_stress;\n\n",
        "fn main() -> i32 {\n",
        "    var total: i32 = 0;\n",
    ]
    for index in range(statement_count):
        source.append(f"    total = total + {index % 97};\n")
    source.append("    return total;\n}\n")
    return "".join(source)


def write_source(statement_count: int) -> pathlib.Path:
    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    path = GENERATED_ROOT / f"ast_stress_{statement_count}.ax"
    source = make_ast_stress_source(statement_count)
    path.write_text(source, encoding="utf-8")
    return path


def parse_peak_rss_mib(stderr: str) -> float | None:
    if platform.system() == "Darwin":
        match = AST_STRESS_TIME_PARSE_PATTERN.search(stderr)
        if match is None:
            return None
        rss_bytes = int(next(group for group in match.groups() if group is not None))
        return rss_bytes / (1024.0 * 1024.0)
    match = AST_STRESS_GNU_TIME_PATTERN.search(stderr)
    if match is None:
        return None
    rss_kib = int(match.group(1))
    return rss_kib / 1024.0


def timed_command(source: pathlib.Path) -> tuple[float, float | None]:
    time_tool = pathlib.Path("/usr/bin/time")
    base_cmd = [str(AUREXC), "--check", str(source)]
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


def parse_optional_float(text: str | None, name: str) -> float | None:
    if text is None or not text.strip():
        return None
    value = float(text)
    if value <= 0:
        raise ValueError(f"{name} must be positive")
    return value


def display_path(path: pathlib.Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def run_stress(counts: list[int]) -> list[StressRow]:
    rows: list[StressRow] = []
    for count in counts:
        source = write_source(count)
        elapsed_ms, max_rss_mib = timed_command(source)
        rows.append(
            StressRow(
                statements=count,
                source_bytes=source.stat().st_size,
                elapsed_ms=elapsed_ms,
                max_rss_mib=max_rss_mib,
                source=display_path(source),
            )
        )
    return rows


def threshold_violations(rows: list[StressRow], thresholds: StressThresholds) -> list[str]:
    violations: list[str] = []
    for row in rows:
        if thresholds.max_elapsed_ms is not None and row.elapsed_ms > thresholds.max_elapsed_ms:
            violations.append(
                f"{row.statements} statements elapsed {row.elapsed_ms:.3f} ms "
                f"exceeds {thresholds.max_elapsed_ms:.3f} ms"
            )
        if thresholds.max_rss_mib is None:
            continue
        if row.max_rss_mib is None:
            violations.append(
                f"{row.statements} statements peak RSS unavailable; "
                f"cannot enforce {thresholds.max_rss_mib:.1f} MiB"
            )
        elif row.max_rss_mib > thresholds.max_rss_mib:
            violations.append(
                f"{row.statements} statements peak RSS {row.max_rss_mib:.1f} MiB "
                f"exceeds {thresholds.max_rss_mib:.1f} MiB"
            )
    return violations


def write_json(rows: list[StressRow], thresholds: StressThresholds, violations: list[str]) -> None:
    payload = {
        "build": str(BUILD),
        "aurexc": str(AUREXC),
        "machine": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor(),
        },
        "thresholds": asdict(thresholds),
        "threshold_violations": violations,
        "rows": [asdict(row) for row in rows],
    }
    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def print_report(rows: list[StressRow], thresholds: StressThresholds, violations: list[str]) -> None:
    print("Aurex bulk AST stress")
    print(f"build: {BUILD}")
    print(f"raw_json: {OUTPUT_JSON}")
    print()
    threshold_parts: list[str] = []
    if thresholds.max_elapsed_ms is not None:
        threshold_parts.append(f"elapsed <= {thresholds.max_elapsed_ms:.3f} ms")
    if thresholds.max_rss_mib is not None:
        threshold_parts.append(f"RSS <= {thresholds.max_rss_mib:.1f} MiB")
    threshold_text = ", ".join(threshold_parts) if threshold_parts else "none"
    print(f"thresholds: {threshold_text}")
    print("RSS is peak process resident set size when /usr/bin/time exposes it.")
    print()
    print(f"{'statements':>10} {'source_KiB':>12} {'elapsed_ms':>12} {'peak_RSS_MiB':>14} {'source':<36}")
    print(f"{'-' * 10} {'-' * 12} {'-' * 12} {'-' * 14} {'-' * 36}")
    for row in rows:
        rss = "n/a" if row.max_rss_mib is None else f"{row.max_rss_mib:.1f}"
        print(
            f"{row.statements:>10} "
            f"{row.source_bytes / 1024.0:>12.1f} "
            f"{row.elapsed_ms:>12.3f} "
            f"{rss:>14} "
            f"{row.source:<36}"
        )
    if violations:
        print()
        print("Threshold violations:")
        for violation in violations:
            print(f"- {violation}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--counts",
        default=os.environ.get("AUREX_AST_STRESS_COUNTS"),
        help="comma-separated statement counts, default: 10000,50000,100000",
    )
    parser.add_argument("--skip-build", action="store_true", help="reuse the existing build-perf aurexc binary")
    parser.add_argument(
        "--max-elapsed-ms",
        default=os.environ.get("AUREX_AST_STRESS_MAX_ELAPSED_MS"),
        help="fail if any row exceeds this elapsed time in milliseconds",
    )
    parser.add_argument(
        "--max-rss-mib",
        default=os.environ.get("AUREX_AST_STRESS_MAX_RSS_MIB"),
        help="fail if any row exceeds this peak RSS in MiB",
    )
    args = parser.parse_args()

    counts = parse_counts(args.counts)
    thresholds = StressThresholds(
        max_elapsed_ms=parse_optional_float(args.max_elapsed_ms, "--max-elapsed-ms"),
        max_rss_mib=parse_optional_float(args.max_rss_mib, "--max-rss-mib"),
    )
    if not args.skip_build:
        build_compiler()
    if not AUREXC.exists():
        raise RuntimeError(f"aurexc not found: {AUREXC}")

    rows = run_stress(counts)
    violations = threshold_violations(rows, thresholds)
    write_json(rows, thresholds, violations)
    print_report(rows, thresholds, violations)
    return 1 if violations else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
