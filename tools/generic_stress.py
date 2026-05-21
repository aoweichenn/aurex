#!/usr/bin/env python3
"""Generic instantiation RSS/time stress baseline and threshold gate for Aurex.

It generates large generic-heavy Aurex modules, runs `aurexc`, records elapsed
time plus peak RSS, and optionally enforces RSS/time thresholds for local or CI
quality gates.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
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
GENERATED_ROOT = BUILD / "generated" / "generic-stress"
OUTPUT_JSON = BUILD / "generic-stress.json"

DEFAULT_COUNTS = (500, 1000, 2000, 5000)
DEFAULT_EMIT_KIND = "check"
DEFAULT_SHAPE = "mixed"
GENERIC_STRESS_EMIT_KINDS = {
    "check",
    "checked",
    "typed",
    "ir",
    "llvm-ir",
}
GENERIC_STRESS_SHAPES = {
    "instances",
    "mixed",
    "templates",
}


@dataclass(frozen=True)
class StressRow:
    instances: int
    emit_kind: str
    shape: str
    source_bytes: int
    elapsed_ms: float
    max_rss_mib: float | None
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


def append_mixed_payload_record(source: list[str], index: int) -> None:
    suffix = str(index)
    source.append(
        f"struct Payload{suffix} {{\n"
        "    value: i32;\n"
        "    extra: i32;\n"
        "}\n\n"
    )


def append_mixed_payload_use_function(source: list[str], index: int) -> None:
    suffix = str(index)
    extra = index % 17
    source.append(
        f"fn use_payload{suffix}(seed: i32) -> i32 {{\n"
        f"    let payload: Payload{suffix} = Payload{suffix} {{ value: seed, extra: seed + {extra} }};\n"
        f"    let boxed: Box[Payload{suffix}] = make_box[Payload{suffix}](payload);\n"
        f"    let nested: Box[Box[Payload{suffix}]] = make_box[Box[Payload{suffix}]](boxed);\n"
        f"    let unwrapped: Payload{suffix} = unwrap_box[Payload{suffix}](unwrap_box[Box[Payload{suffix}]](nested));\n"
        f"    let maybe: Maybe[Payload{suffix}] = Maybe[Payload{suffix}].some(unwrapped);\n"
        f"    let result: Outcome[Payload{suffix}, i32] = Outcome[Payload{suffix}, i32].ok(unwrapped);\n"
        f"    let pair: Pair[Payload{suffix}, i32] = make_pair[Payload{suffix}, i32](unwrapped, seed);\n"
        "    let maybe_score: i32 = match maybe {\n"
        "        .some(inner) => inner.value + inner.extra,\n"
        "        .none => 0,\n"
        "    };\n"
        "    let result_score: i32 = match result {\n"
        "        .ok(inner) => inner.value,\n"
        "        .err(code) => code,\n"
        "    };\n"
        "    let values: [3]i32 = [seed, maybe_score, result_score];\n"
        "    let view: []const i32 = values[:];\n"
        "    let ptr: ConstPtr[i32] = const_ptr(&seed);\n"
        "    let sized: SizedPtr[i32] = read_sized_ptr(ptr);\n"
        "    let left_box: Box[i32] = make_box[i32](second(pair));\n"
        "    let right_box: Box[i32] = make_box[i32](seed);\n"
        "    if ptraddr(sized) != 0usize && left_box.same_value(&right_box) && same(seed, seed) && lower(seed, seed + 1) {\n"
        "        return maybe_score + result_score + edge_sum(view) + hashable(seed);\n"
        "    }\n"
        "    return maybe_score + result_score + edge_sum(view);\n"
        "}\n\n"
    )


def make_mixed_generic_stress_source(instance_count: int) -> str:
    source: list[str] = []
    source.append(
        "module perf.generic_stress;\n\n"
        "type ConstPtr[T] = *const T;\n"
        "type SizedPtr[T] where T: Sized = *const T;\n\n"
        "struct Box[T] {\n"
        "    value: T;\n"
        "}\n\n"
        "struct Pair[T, U] {\n"
        "    left: T;\n"
        "    right: U;\n"
        "}\n\n"
        "enum Maybe[T] {\n"
        "    some(T),\n"
        "    none,\n"
        "}\n\n"
        "enum Outcome[T, E] {\n"
        "    ok(T),\n"
        "    err(E),\n"
        "}\n\n"
        "impl[T] Box[T] {\n"
        "    fn get(self: &Box[T]) -> T {\n"
        "        return self.value;\n"
        "    }\n"
        "}\n\n"
        "impl[T] Box[T] where T: Eq {\n"
        "    fn same_value(self: &Box[T], other: &Box[T]) -> bool {\n"
        "        return self.value == other.value;\n"
        "    }\n"
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
        "fn make_pair[T, U](left: T, right: U) -> Pair[T, U] {\n"
        "    return Pair[T, U] { left: left, right: right };\n"
        "}\n\n"
        "fn first[T, U](pair: Pair[T, U]) -> T {\n"
        "    return pair.left;\n"
        "}\n\n"
        "fn second[T, U](pair: Pair[T, U]) -> U {\n"
        "    return pair.right;\n"
        "}\n\n"
        "fn same[T](left: T, right: T) -> bool where T: Eq {\n"
        "    return left == right;\n"
        "}\n\n"
        "fn lower[T](left: T, right: T) -> bool where T: Ord {\n"
        "    return left < right;\n"
        "}\n\n"
        "fn hashable[T](value: T) -> i32 where T: Hash {\n"
        "    return 1;\n"
        "}\n\n"
        "fn const_ptr[T](value: &T) -> *const T {\n"
        "    return unsafe { ptrat[*const T](ptraddr(value)) };\n"
        "}\n\n"
        "fn read_sized_ptr[T](value: SizedPtr[T]) -> SizedPtr[T] where T: Sized {\n"
        "    return value;\n"
        "}\n\n"
        "fn edge_sum(values: []const i32) -> i32 {\n"
        "    return match values {\n"
        "        [head, .., tail] => head + tail,\n"
        "        [single] => single,\n"
        "        _ => 0,\n"
        "    };\n"
        "}\n\n"
    )
    for index in range(instance_count):
        append_mixed_payload_record(source, index)
    for index in range(instance_count):
        append_mixed_payload_use_function(source, index)
    source.append("fn main() -> i32 {\n    var total: i32 = 0;\n")
    for index in range(instance_count):
        source.append(f"    total += use_payload{index}({index});\n")
    source.append("    return total;\n}\n")
    return "".join(source)


def make_stress_source(instance_count: int, shape: str) -> str:
    if shape == "mixed":
        return make_mixed_generic_stress_source(instance_count)
    if shape == "templates":
        return make_distinct_generic_template_stress_source(instance_count)
    return make_generic_stress_source(instance_count)


def write_source(instance_count: int, shape: str) -> pathlib.Path:
    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    path = GENERATED_ROOT / f"generic_stress_{shape}_{instance_count}.ax"
    source = make_stress_source(instance_count, shape)
    path.write_text(source, encoding="utf-8")
    return path


def timed_command(
    source: pathlib.Path,
    emit_kind: str,
    profile: pathlib.Path,
) -> tuple[ProcessMetrics, dict[str, object] | None]:
    if profile.exists():
        profile.unlink()
    base_cmd = [str(AUREXC), "--profile-output", str(profile), f"--emit={emit_kind}", str(source)]
    completed, metrics = run_timed_command(base_cmd, ROOT)
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed: {' '.join(base_cmd)}\n"
            f"stdout:\n{completed.stdout}\n"
            f"stderr:\n{completed.stderr}"
        )
    compiler_profile = load_compiler_profile(profile)
    if compiler_profile is None:
        raise RuntimeError(f"compiler profile was not written: {profile}")
    return metrics, compiler_profile


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


def run_stress(counts: list[int], emit_kind: str, shape: str) -> list[StressRow]:
    rows: list[StressRow] = []
    for count in counts:
        source = write_source(count, shape)
        profile = GENERATED_ROOT / f"generic_profile_{shape}_{emit_kind}_{count}.json"
        metrics, compiler_profile = timed_command(source, emit_kind, profile)
        rows.append(
            StressRow(
                instances=count,
                emit_kind=emit_kind,
                shape=shape,
                source_bytes=source.stat().st_size,
                elapsed_ms=metrics.elapsed_ms,
                max_rss_mib=metrics.max_rss_mib,
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
        if thresholds.max_elapsed_ms is not None and row.elapsed_ms > thresholds.max_elapsed_ms:
            violations.append(
                f"{row.instances} {row.shape}/{row.emit_kind} elapsed {row.elapsed_ms:.3f} ms "
                f"exceeds {thresholds.max_elapsed_ms:.3f} ms"
            )
        if thresholds.max_rss_mib is None:
            continue
        if row.max_rss_mib is None:
            violations.append(
                f"{row.instances} {row.shape}/{row.emit_kind} peak RSS unavailable; "
                f"cannot enforce {thresholds.max_rss_mib:.1f} MiB"
            )
        elif row.max_rss_mib > thresholds.max_rss_mib:
            violations.append(
                f"{row.instances} {row.shape}/{row.emit_kind} peak RSS {row.max_rss_mib:.1f} MiB "
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
    print("Aurex generic instantiation stress")
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
        f"{'instances':>10} {'emit':>8} {'shape':>10} {'source_KiB':>12} "
        f"{'wall_ms':>12} {'user_s':>9} {'sys_s':>9} {'peak_RSS_MiB':>14} "
        f"{'minor_pf':>10} {'major_pf':>10} {'source':<36}"
    )
    print(
        f"{'-' * 10} {'-' * 8} {'-' * 10} {'-' * 12} "
        f"{'-' * 12} {'-' * 9} {'-' * 9} {'-' * 14} "
        f"{'-' * 10} {'-' * 10} {'-' * 36}"
    )
    for row in rows:
        rss = "n/a" if row.max_rss_mib is None else f"{row.max_rss_mib:.1f}"
        metrics = row.process_metrics
        print(
            f"{row.instances:>10} "
            f"{row.emit_kind:>8} "
            f"{row.shape:>10} "
            f"{row.source_bytes / 1024.0:>12.1f} "
            f"{row.elapsed_ms:>12.3f} "
            f"{format_optional_float(metrics.get('user_time_s') if isinstance(metrics.get('user_time_s'), (float, int)) else None):>9} "
            f"{format_optional_float(metrics.get('system_time_s') if isinstance(metrics.get('system_time_s'), (float, int)) else None):>9} "
            f"{rss:>14} "
            f"{format_optional_int(metrics.get('minor_page_faults') if isinstance(metrics.get('minor_page_faults'), int) else None):>10} "
            f"{format_optional_int(metrics.get('major_page_faults') if isinstance(metrics.get('major_page_faults'), int) else None):>10} "
            f"{row.source:<36}"
        )
    for row in rows:
        print_compiler_phase_report(
            f"{row.instances} {row.shape}/{row.emit_kind} generic instances",
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
        help="stress source shape: mixed covers generic constraints plus broad syntax; instances/templates keep narrow comparison shapes; default: mixed",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="reuse the existing build/perf aurexc binary",
    )
    parser.add_argument(
        "--max-elapsed-ms",
        default=os.environ.get("AUREX_GENERIC_STRESS_MAX_ELAPSED_MS"),
        help="fail if any row exceeds this elapsed time in milliseconds",
    )
    parser.add_argument(
        "--max-rss-mib",
        default=os.environ.get("AUREX_GENERIC_STRESS_MAX_RSS_MIB"),
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
    emit_kind = parse_emit_kind(args.emit)
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
        raise RuntimeError(f"missing aurexc binary: {AUREXC}")
    rows = run_stress(counts, emit_kind, shape)
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
