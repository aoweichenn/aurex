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
import re
import subprocess
import sys
from dataclasses import asdict, dataclass

from perf_thresholds import (
    ProcessMetrics,
    format_optional_float,
    format_optional_int,
    load_compiler_profile,
    make_calibration,
    print_compiler_phase_report,
    run_timed_command,
    scaled_threshold,
    stress_build_options,
    stress_lto_enabled,
)


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(
    os.environ.get(
        "AUREX_STRESS_BUILD_DIR",
        os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build" / "perf")),
    )
).resolve()
AUREXC = BUILD / "bin" / "aurexc"
GENERATED_ROOT = BUILD / "generated" / "diagnostic-stress"
OUTPUT_JSON = BUILD / "diagnostic-stress.json"

DEFAULT_COUNTS = (100, 1000, 5000)
DEFAULT_SHAPE = "mixed"
DIAGNOSTIC_STRESS_SHAPES = {
    "mixed",
    "missing",
}


@dataclass(frozen=True)
class StressRow:
    requested_errors: int
    shape: str
    source_bytes: int
    elapsed_ms: float
    max_rss_mib: float | None
    printed_errors: int
    suppressed: int
    output_bytes: int
    process_metrics: dict[str, object]
    compiler_profile_path: str
    compiler_profile: dict[str, object] | None
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
        f"-DAUREX_ENABLE_LTO={'ON' if stress_lto_enabled() else 'OFF'}",
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


def make_missing_diagnostic_stress_source(error_count: int) -> str:
    source: list[str] = [
        "module perf.diagnostic_stress;\n\n",
        "fn main() -> i32 {\n",
        "    let anchor: i32 = 0;\n",
    ]
    for index in range(error_count):
        source.append(f"    let value_{index}: i32 = missing_value_{index};\n")
    source.append("    return anchor;\n}\n")
    return "".join(source)


def diagnostic_templates(index: int) -> list[str]:
    suffix = str(index)
    templates = [
        f"    let missing_{suffix}: i32 = missing_value_{suffix};\n",
        f"    let mismatch_{suffix}: i32 = true;\n",
        "    takes();\n",
        "    takes(true, pair);\n",
        f"    let bad_call_{suffix} = (1 + 2)();\n",
        f"    let bad_field_{suffix} = pair.missing_{suffix};\n",
        f"    let bad_index_value_{suffix} = anchor[0];\n",
        f"    let bad_index_type_{suffix} = (&x)[true];\n",
        f"    let bad_struct_{suffix} = Pair {{ left: 1, missing_{suffix}: 2 }};\n",
        f"    let bad_struct_field_type_{suffix} = Pair {{ left: true, right: 2 }};\n",
        f"    let bad_empty_payload_{suffix} = Payload.none(1);\n",
        f"    let bad_payload_field_{suffix} = Payload.item;\n",
        f"    let bad_payload_arity_{suffix} = Payload.item(1, 2);\n",
        f"    let bad_addr_{suffix} = ptraddr(1);\n",
        f"    let bad_from_{suffix} = ptrat<i32>(true);\n",
        f"    let bad_sizeof_void_{suffix}: usize = sizeof<void>();\n",
        f"    let bad_str_ptr_field_{suffix} = 1.ptr;\n",
        f"    let bad_str_len_field_{suffix} = 1.len;\n",
        f"    let bad_strfrom_{suffix} = strfromutf8(1);\n",
        f"    let bad_strraw_data_{suffix} = strraw(1, ((1) as usize));\n",
        f"    let bad_strraw_len_{suffix} = strraw(c\"bytes\", true);\n",
        f"    let bad_generic_apply_{suffix} = id<i32>;\n",
        f"    let bad_array_unknown_{suffix} = [missing_name_{suffix}];\n",
        f"    let bad_array_storage_{suffix} = [touch()];\n",
        f"    let bad_if_void_{suffix} = if true {{ touch() }} else {{ touch() }};\n",
        f"    let bad_logic_{suffix} = !1;\n",
        f"    let bad_neg_{suffix} = -pair;\n",
        f"    let bad_deref_{suffix} = *1;\n",
        f"    let bad_addr_lvalue_{suffix} = &(1 + 2);\n",
        f"    let bad_equal_{suffix} = pair == other;\n",
        f"    let bad_int_{suffix} = true & false;\n",
        f"    let bad_compare_{suffix} = pair < other;\n",
        f"    let bad_match_{suffix}: i32 = match Payload.none {{ .item(value) => value.left, .none => false }};\n",
    ]
    return templates


def make_mixed_diagnostic_stress_source(error_count: int) -> str:
    source: list[str] = [
        "module perf.diagnostic_stress;\n\n",
        "struct Pair {\n"
        "    left: i32;\n"
        "    right: i32;\n"
        "}\n\n",
        "enum Payload: u8 {\n"
        "    item(Pair) = 1,\n"
        "    none = 2,\n"
        "}\n\n",
        "fn touch() -> void {\n"
        "    return;\n"
        "}\n\n",
        "fn takes(value: i32, pair: Pair) -> i32 {\n"
        "    return value;\n"
        "}\n\n",
        "fn id<T>(value: T) -> T {\n"
        "    return value;\n"
        "}\n\n",
        "fn main() -> i32 {\n",
        "    let anchor: i32 = 0;\n",
        "    var x: i32 = 1;\n",
        "    let pair = Pair { left: 1, right: 2 };\n",
        "    let other = Pair { left: 3, right: 4 };\n",
    ]
    templates = diagnostic_templates(0)
    for index in range(error_count):
        source.append(diagnostic_templates(index)[index % len(templates)])
    source.append("    return anchor + x;\n}\n")
    return "".join(source)


def parse_shape(text: str | None) -> str:
    shape = DEFAULT_SHAPE if text is None else text.strip()
    if shape not in DIAGNOSTIC_STRESS_SHAPES:
        valid = ", ".join(sorted(DIAGNOSTIC_STRESS_SHAPES))
        raise ValueError(f"unsupported diagnostic stress shape: {shape}; expected one of: {valid}")
    return shape


def make_diagnostic_stress_source(error_count: int, shape: str) -> str:
    if shape == "missing":
        return make_missing_diagnostic_stress_source(error_count)
    return make_mixed_diagnostic_stress_source(error_count)


def write_source(error_count: int, shape: str) -> pathlib.Path:
    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    path = GENERATED_ROOT / f"diagnostic_stress_{shape}_{error_count}.ax"
    source = make_diagnostic_stress_source(error_count, shape)
    path.write_text(source, encoding="utf-8")
    return path


def parse_suppressed_diagnostics(output: str) -> int:
    match = re.search(r"suppressing\s+(\d+)\s+additional diagnostics", output)
    return 0 if match is None else int(match.group(1))


def timed_command(
    source: pathlib.Path,
    profile: pathlib.Path,
) -> tuple[ProcessMetrics, dict[str, object] | None, str]:
    if profile.exists():
        profile.unlink()
    base_cmd = [str(AUREXC), "--profile-output", str(profile), "--check", str(source)]
    completed, metrics = run_timed_command(base_cmd, ROOT)
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
    compiler_profile = load_compiler_profile(profile)
    if compiler_profile is None:
        raise RuntimeError(f"compiler profile was not written: {profile}")
    return metrics, compiler_profile, output


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


def run_stress(counts: list[int], shape: str) -> list[StressRow]:
    rows: list[StressRow] = []
    for count in counts:
        source = write_source(count, shape)
        profile = GENERATED_ROOT / f"diagnostic_profile_{shape}_{count}.json"
        metrics, compiler_profile, output = timed_command(source, profile)
        rows.append(
            StressRow(
                requested_errors=count,
                shape=shape,
                source_bytes=source.stat().st_size,
                elapsed_ms=metrics.elapsed_ms,
                max_rss_mib=metrics.max_rss_mib,
                printed_errors=output.count("error:") + output.count("\033[1;31merror\033[0m"),
                suppressed=parse_suppressed_diagnostics(output),
                output_bytes=len(output.encode("utf-8")),
                process_metrics=metrics.to_json(),
                compiler_profile_path=display_path(profile),
                compiler_profile=compiler_profile,
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
        "build_options": stress_build_options(),
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
    print(f"build_options: Release, LTO={'on' if stress_lto_enabled() else 'off'}")
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
    print("Process RSS/page-fault metrics come from /usr/bin/time when available.")
    print("Compiler phase RSS is cumulative max RSS after each recorded phase, not isolated per-phase allocation.")
    print()
    print(
        f"{'errors':>8} {'shape':>8} {'source_KiB':>12} {'wall_ms':>12} "
        f"{'user_s':>9} {'sys_s':>9} {'peak_RSS_MiB':>14} {'minor_pf':>10} "
        f"{'major_pf':>10} {'printed':>8} {'suppressed':>10} {'output_KiB':>12} "
        f"{'source':<36}"
    )
    print(
        f"{'-' * 8} {'-' * 8} {'-' * 12} {'-' * 12} "
        f"{'-' * 9} {'-' * 9} {'-' * 14} {'-' * 10} "
        f"{'-' * 10} {'-' * 8} {'-' * 10} {'-' * 12} {'-' * 36}"
    )
    for row in rows:
        rss = "n/a" if row.max_rss_mib is None else f"{row.max_rss_mib:.1f}"
        metrics = row.process_metrics
        print(
            f"{row.requested_errors:>8} "
            f"{row.shape:>8} "
            f"{row.source_bytes / 1024.0:>12.1f} "
            f"{row.elapsed_ms:>12.3f} "
            f"{format_optional_float(metrics.get('user_time_s') if isinstance(metrics.get('user_time_s'), (float, int)) else None):>9} "
            f"{format_optional_float(metrics.get('system_time_s') if isinstance(metrics.get('system_time_s'), (float, int)) else None):>9} "
            f"{rss:>14} "
            f"{format_optional_int(metrics.get('minor_page_faults') if isinstance(metrics.get('minor_page_faults'), int) else None):>10} "
            f"{format_optional_int(metrics.get('major_page_faults') if isinstance(metrics.get('major_page_faults'), int) else None):>10} "
            f"{row.printed_errors:>8} "
            f"{row.suppressed:>10} "
            f"{row.output_bytes / 1024.0:>12.1f} "
            f"{row.source:<36}"
        )
    for row in rows:
        print_compiler_phase_report(
            f"{row.requested_errors} {row.shape} diagnostic errors",
            row.compiler_profile_path,
            row.compiler_profile,
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
    parser.add_argument(
        "--shape",
        default=os.environ.get("AUREX_DIAGNOSTIC_STRESS_SHAPE", DEFAULT_SHAPE),
        help="stress source shape: mixed cycles semantic diagnostic families; missing keeps the old missing-name lane; default: mixed",
    )
    parser.add_argument("--skip-build", action="store_true", help="reuse the existing build/perf aurexc binary")
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
    shape = parse_shape(args.shape)
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

    rows = run_stress(counts, shape)
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
