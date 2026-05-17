#!/usr/bin/env python3
"""Diagnostic throughput RSS/time stress baseline and threshold gate for Aurex.

It generates modules with many independent semantic errors, runs `aurexc
--check`, records elapsed time plus peak RSS, and optionally enforces RSS/time
thresholds. The compiler is expected to fail for each generated source; the
gate verifies that it fails through diagnostics rather than crashing or timing
out.
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

from perf_thresholds import make_calibration, scaled_threshold


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(
    os.environ.get(
        "AUREX_STRESS_BUILD_DIR",
        os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build-perf")),
    )
).resolve()
AUREXC = BUILD / "bin" / "aurexc"
GENERATED_ROOT = BUILD / "generated" / "diagnostic-stress"
OUTPUT_JSON = BUILD / "diagnostic-stress.json"

DEFAULT_COUNTS = (100, 1000, 5000)
DIAGNOSTIC_STRESS_TIME_PARSE_PATTERN = re.compile(
    r"(?:maximum resident set size:\s+(\d+)|(\d+)\s+maximum resident set size)",
    re.IGNORECASE,
)
DIAGNOSTIC_STRESS_GNU_TIME_PATTERN = re.compile(
    r"Maximum resident set size \(kbytes\):\s+(\d+)",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class StressRow:
    requested_errors: int
    source_bytes: int
    elapsed_ms: float
    max_rss_mib: float | None
    printed_errors: int
    suppressed: int
    output_bytes: int
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


def make_diagnostic_stress_source(error_count: int) -> str:
    source: list[str] = [
        "module perf.diagnostic_stress;\n\n",
        "fn main() -> i32 {\n",
        "    let anchor: i32 = 0;\n",
    ]
    for index in range(error_count):
        source.append(f"    let value_{index}: i32 = missing_value_{index};\n")
    source.append("    return anchor;\n}\n")
    return "".join(source)


def write_source(error_count: int) -> pathlib.Path:
    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    path = GENERATED_ROOT / f"diagnostic_stress_{error_count}.ax"
    source = make_diagnostic_stress_source(error_count)
    path.write_text(source, encoding="utf-8")
    return path


def parse_peak_rss_mib(stderr: str) -> float | None:
    if platform.system() == "Darwin":
        match = DIAGNOSTIC_STRESS_TIME_PARSE_PATTERN.search(stderr)
        if match is None:
            return None
        rss_bytes = int(next(group for group in match.groups() if group is not None))
        return rss_bytes / (1024.0 * 1024.0)
    match = DIAGNOSTIC_STRESS_GNU_TIME_PATTERN.search(stderr)
    if match is None:
        return None
    rss_kib = int(match.group(1))
    return rss_kib / 1024.0


def parse_suppressed_diagnostics(output: str) -> int:
    match = re.search(r"suppressing\s+(\d+)\s+additional diagnostics", output)
    return 0 if match is None else int(match.group(1))


def timed_command(source: pathlib.Path) -> tuple[float, float | None, str]:
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
    output = completed.stdout + completed.stderr
    if completed.returncode == 0:
        raise RuntimeError(
            f"diagnostic stress unexpectedly succeeded: {' '.join(base_cmd)}\n"
            f"output:\n{output}"
        )
    if "error:" not in output and "\033[1;31merror\033[0m" not in output:
        raise RuntimeError(
            f"diagnostic stress failed without diagnostics: {' '.join(base_cmd)}\n"
            f"output:\n{output}"
        )
    return elapsed_ms, parse_peak_rss_mib(completed.stderr), output


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
        elapsed_ms, max_rss_mib, output = timed_command(source)
        rows.append(
            StressRow(
                requested_errors=count,
                source_bytes=source.stat().st_size,
                elapsed_ms=elapsed_ms,
                max_rss_mib=max_rss_mib,
                printed_errors=output.count("error:") + output.count("\033[1;31merror\033[0m"),
                suppressed=parse_suppressed_diagnostics(output),
                output_bytes=len(output.encode("utf-8")),
                source=display_path(source),
            )
        )
    return rows


def threshold_violations(rows: list[StressRow], thresholds: StressThresholds) -> list[str]:
    violations: list[str] = []
    for row in rows:
        if row.printed_errors == 0:
            violations.append(f"{row.requested_errors} requested errors produced no printed diagnostics")
        if thresholds.max_elapsed_ms is not None and row.elapsed_ms > thresholds.max_elapsed_ms:
            violations.append(
                f"{row.requested_errors} errors elapsed {row.elapsed_ms:.3f} ms "
                f"exceeds {thresholds.max_elapsed_ms:.3f} ms"
            )
        if thresholds.max_rss_mib is None:
            continue
        if row.max_rss_mib is None:
            violations.append(
                f"{row.requested_errors} errors peak RSS unavailable; "
                f"cannot enforce {thresholds.max_rss_mib:.1f} MiB"
            )
        elif row.max_rss_mib > thresholds.max_rss_mib:
            violations.append(
                f"{row.requested_errors} errors peak RSS {row.max_rss_mib:.1f} MiB "
                f"exceeds {thresholds.max_rss_mib:.1f} MiB"
            )
    return violations


def write_json(
    rows: list[StressRow],
    raw_thresholds: StressThresholds,
    thresholds: StressThresholds,
    calibration,
    violations: list[str],
) -> None:
    payload = {
        "build": str(BUILD),
        "aurexc": str(AUREXC),
        "machine": calibration.machine,
        "threshold_calibration": calibration.to_json(),
        "raw_thresholds": asdict(raw_thresholds),
        "thresholds": asdict(thresholds),
        "threshold_violations": violations,
        "rows": [asdict(row) for row in rows],
    }
    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def print_report(rows: list[StressRow], thresholds: StressThresholds, calibration, violations: list[str]) -> None:
    print("Aurex diagnostic stress")
    print(f"build: {BUILD}")
    print(f"raw_json: {OUTPUT_JSON}")
    print()
    print(f"threshold_profile: {calibration.profile}")
    print(f"threshold_scale: {calibration.scale:.3f}")
    threshold_parts: list[str] = []
    if thresholds.max_elapsed_ms is not None:
        threshold_parts.append(f"elapsed <= {thresholds.max_elapsed_ms:.3f} ms")
    if thresholds.max_rss_mib is not None:
        threshold_parts.append(f"RSS <= {thresholds.max_rss_mib:.1f} MiB")
    threshold_text = ", ".join(threshold_parts) if threshold_parts else "none"
    print(f"thresholds: {threshold_text}")
    print("RSS is peak process resident set size when /usr/bin/time exposes it.")
    print()
    print(
        f"{'errors':>8} {'source_KiB':>12} {'elapsed_ms':>12} "
        f"{'peak_RSS_MiB':>14} {'printed':>8} {'suppressed':>10} "
        f"{'output_KiB':>12} {'source':<36}"
    )
    print(
        f"{'-' * 8} {'-' * 12} {'-' * 12} {'-' * 14} "
        f"{'-' * 8} {'-' * 10} {'-' * 12} {'-' * 36}"
    )
    for row in rows:
        rss = "n/a" if row.max_rss_mib is None else f"{row.max_rss_mib:.1f}"
        print(
            f"{row.requested_errors:>8} "
            f"{row.source_bytes / 1024.0:>12.1f} "
            f"{row.elapsed_ms:>12.3f} "
            f"{rss:>14} "
            f"{row.printed_errors:>8} "
            f"{row.suppressed:>10} "
            f"{row.output_bytes / 1024.0:>12.1f} "
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
        default=os.environ.get("AUREX_DIAGNOSTIC_STRESS_COUNTS"),
        help="comma-separated diagnostic counts; default: 100,1000,5000",
    )
    parser.add_argument("--skip-build", action="store_true", help="reuse the existing build-perf aurexc binary")
    parser.add_argument(
        "--max-elapsed-ms",
        default=os.environ.get("AUREX_DIAGNOSTIC_STRESS_MAX_ELAPSED_MS"),
        help="fail if any row exceeds this elapsed time in milliseconds",
    )
    parser.add_argument(
        "--max-rss-mib",
        default=os.environ.get("AUREX_DIAGNOSTIC_STRESS_MAX_RSS_MIB"),
        help="fail if any row exceeds this peak RSS in MiB",
    )
    parser.add_argument(
        "--threshold-profile",
        default=os.environ.get("AUREX_PERF_THRESHOLD_PROFILE"),
        help="machine/profile label written to stress JSON; default: local",
    )
    parser.add_argument(
        "--threshold-scale",
        default=os.environ.get("AUREX_PERF_THRESHOLD_SCALE"),
        help="positive multiplier applied to elapsed and RSS thresholds; default: 1.0",
    )
    args = parser.parse_args()

    counts = parse_counts(args.counts)
    calibration = make_calibration(args.threshold_profile, args.threshold_scale)
    raw_thresholds = StressThresholds(
        max_elapsed_ms=parse_optional_float(args.max_elapsed_ms, "--max-elapsed-ms"),
        max_rss_mib=parse_optional_float(args.max_rss_mib, "--max-rss-mib"),
    )
    thresholds = StressThresholds(
        max_elapsed_ms=scaled_threshold(raw_thresholds.max_elapsed_ms, calibration),
        max_rss_mib=scaled_threshold(raw_thresholds.max_rss_mib, calibration),
    )
    if not args.skip_build:
        build_compiler()
    if not AUREXC.exists():
        raise RuntimeError(f"aurexc not found: {AUREXC}")

    rows = run_stress(counts)
    violations = threshold_violations(rows, thresholds)
    write_json(rows, raw_thresholds, thresholds, calibration, violations)
    print_report(rows, thresholds, calibration, violations)
    return 1 if violations else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
