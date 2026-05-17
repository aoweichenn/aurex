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

from perf_thresholds import make_calibration, scaled_threshold, stress_build_options, stress_lto_enabled


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
DEFAULT_SHAPE = "mixed"
AST_STRESS_SHAPES = {
    "mixed",
    "simple",
}
AST_MIXED_VALUE_MODULUS = 97
AST_MIXED_DISPATCH_CASES = 8
AST_MIXED_FEATURE_INTERVAL = 4096
AST_MIXED_HEAVY_EXPRESSION_INTERVAL = 16
AST_MIXED_LIGHT_STATE_INTERVAL = 4
AST_STRESS_TIME_PARSE_PATTERN = re.compile(
    r"(?:maximum resident set size:\s+(\d+)|(\d+)\s+maximum resident set size)",
    re.IGNORECASE,
)
AST_STRESS_GNU_TIME_PATTERN = re.compile(r"Maximum resident set size \(kbytes\):\s+(\d+)", re.IGNORECASE)


@dataclass(frozen=True)
class StressRow:
    statements: int
    shape: str
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


def make_simple_ast_stress_source(statement_count: int) -> str:
    source: list[str] = [
        "module perf.ast_stress;\n\n",
        "fn main() -> i32 {\n",
        "    var total: i32 = 0;\n",
    ]
    for index in range(statement_count):
        source.append(f"    total = total + {index % 97};\n")
    source.append("    return total;\n}\n")
    return "".join(source)


def append_ast_mixed_prelude(source: list[str]) -> None:
    source.append(
        "module perf.ast_stress;\n\n"
        "extern c {\n"
        "    fn observe(value: i32) -> void @name(\"observe\");\n"
        "}\n\n"
        "type BinaryOp = fn(i32, i32) -> i32;\n"
        "type ConstPtr[T] = *const T;\n"
        "type SizedPtr[T] where T: Sized = *const T;\n"
        "type UnsafeRead = unsafe fn(*const i32) -> i32;\n\n"
        "struct Pair {\n"
        "    left: i32;\n"
        "    right: i32;\n"
        "}\n\n"
        "struct Box[T] {\n"
        "    value: T;\n"
        "}\n\n"
        "struct Counter {\n"
        "    value: i32;\n"
        "}\n\n"
        "enum Packet: u8 {\n"
        "    int(i32) = 1,\n"
        "    other(i32) = 2,\n"
        "    none = 3,\n"
        "}\n\n"
        "enum Maybe[T] {\n"
        "    some(T),\n"
        "    none,\n"
        "}\n\n"
        "enum OptionI32: u8 {\n"
        "    some(i32) = 1,\n"
        "    none = 2,\n"
        "}\n\n"
        "enum ResultI32I32: u8 {\n"
        "    ok(i32) = 1,\n"
        "    err(i32) = 2,\n"
        "}\n\n"
        "enum Code: u16 {\n"
        "    hex = 0x2A,\n"
        "    bin = 0b1010,\n"
        "    dec = 1_000,\n"
        "}\n\n"
        "impl Counter {\n"
        "    pub fn new(value: i32) -> Counter {\n"
        "        return Counter { value: value };\n"
        "    }\n\n"
        "    pub fn add(self: &mut Counter, delta: i32) -> i32 {\n"
        "        self.value = self.value + delta;\n"
        "        return self.value;\n"
        "    }\n\n"
        "    pub fn read(self: &Counter) -> i32 {\n"
        "        return self.value;\n"
        "    }\n"
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
        "fn add(left: i32, right: i32) -> i32 {\n"
        "    return left + right;\n"
        "}\n\n"
        "fn apply(op: BinaryOp, left: i32, right: i32) -> i32 {\n"
        "    return op(left, right);\n"
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
        "fn make_pair(value: i32) -> (i32, bool) {\n"
        "    return (value, value > 0);\n"
        "}\n\n"
        "fn first[T, U](pair: (T, U)) -> T {\n"
        "    let (left, _) = pair;\n"
        "    return left;\n"
        "}\n\n"
        "fn second[T, U](pair: (T, U)) -> U {\n"
        "    let (_, right) = pair;\n"
        "    return right;\n"
        "}\n\n"
        "fn payload_score(packet: Packet) -> i32 {\n"
        "    return match packet {\n"
        "        .int(value) | .other(value) => value,\n"
        "        .none => 0,\n"
        "    };\n"
        "}\n\n"
        "fn unwrap_maybe[T](value: Maybe[T], fallback: T) -> T {\n"
        "    return match value {\n"
        "        .some(inner) => inner,\n"
        "        .none => fallback,\n"
        "    };\n"
        "}\n\n"
        "fn first_or_zero(values: []const i32) -> i32 {\n"
        "    if values is [head, ..] {\n"
        "        return head;\n"
        "    }\n"
        "    return 0;\n"
        "}\n\n"
        "fn edge_sum(values: []const i32) -> i32 {\n"
        "    return match values {\n"
        "        [head, .., tail] => head + tail,\n"
        "        [single] => single,\n"
        "        _ => 0,\n"
        "    };\n"
        "}\n\n"
        "fn array_edge_score() -> i32 {\n"
        "    let values: [3]i32 = [4, 5, 6];\n"
        "    return match values {\n"
        "        [4, .., tail] => tail,\n"
        "        _ => 0,\n"
        "    };\n"
        "}\n\n"
        "fn parse_digit(value: i32) -> ResultI32I32 {\n"
        "    if value < 0 {\n"
        "        return ResultI32I32.err(7);\n"
        "    }\n"
        "    return ResultI32I32.ok(value + 1);\n"
        "}\n\n"
        "fn add_one_result(value: i32) -> ResultI32I32 {\n"
        "    let parsed: i32 = parse_digit(value)?;\n"
        "    return ResultI32I32.ok(parsed + 1);\n"
        "}\n\n"
        "fn result_or(value: ResultI32I32, fallback: i32) -> i32 {\n"
        "    return match value {\n"
        "        .ok(inner) => inner,\n"
        "        .err(_) => fallback,\n"
        "    };\n"
        "}\n\n"
        "fn maybe_even(value: i32) -> OptionI32 {\n"
        "    if (value % 2) == 0 {\n"
        "        return OptionI32.some(value);\n"
        "    }\n"
        "    return OptionI32.none;\n"
        "}\n\n"
        "fn unwrap_even(value: i32) -> OptionI32 {\n"
        "    let even: i32 = maybe_even(value)?;\n"
        "    return OptionI32.some(even + 2);\n"
        "}\n\n"
        "fn option_or(value: OptionI32, fallback: i32) -> i32 {\n"
        "    return match value {\n"
        "        .some(inner) => inner,\n"
        "        .none => fallback,\n"
        "    };\n"
        "}\n\n"
        "fn mark(log: &mut i32) -> void {\n"
        "    *log = *log + 1;\n"
        "}\n\n"
        "unsafe fn from_raw(data: *const u8, len: usize) -> str {\n"
        "    return strraw(data, len);\n"
        "}\n\n"
        "unsafe fn read_raw(value: *const i32) -> i32 {\n"
        "    return *value;\n"
        "}\n\n"
    )


def append_ast_feature_block(source: list[str]) -> None:
    source.append(
        "    {\n"
        "        defer mark(&mut state);\n"
        "        let boxed: Box[i32] = Box[i32] { value: 39 };\n"
        "        let other_box: Box[i32] = Box[i32] { value: 39 };\n"
        "        let eq_box: Box[i32] = Box[i32] { value: 39 };\n"
        "        let nested_box: Box[Box[i32]] = make_box[Box[i32]](boxed);\n"
        "        let unboxed: i32 = unwrap_box[i32](unwrap_box[Box[i32]](nested_box));\n"
        "        let maybe: Maybe[i32] = Maybe[i32].some(unboxed);\n"
        "        if other_box.same_value(&eq_box) && same(1, 1) && lower(1, 2) {\n"
        "            total += unwrap_maybe(maybe, 0) + hashable(unboxed);\n"
        "        }\n\n"
        "        let pair: (i32, bool) = make_pair(4);\n"
        "        let (pair_value, pair_ok) = pair;\n"
        "        var (mutable_count, mutable_flag) = pair;\n"
        "        mutable_count += first(pair);\n"
        "        if pair_ok && mutable_flag && second(pair) {\n"
        "            total += pair_value + mutable_count;\n"
        "        }\n\n"
        "        var pointer_source: i32 = 9;\n"
        "        let pointer_value: *const i32 = const_ptr(&pointer_source);\n"
        "        let sized_ptr: SizedPtr[i32] = read_sized_ptr(pointer_value);\n"
        "        let reader: UnsafeRead = read_raw;\n"
        "        let read_value: i32 = unsafe { reader(sized_ptr) };\n"
        "        if ptraddr(sized_ptr) != 0usize {\n"
        "            total += read_value;\n"
        "        }\n\n"
        "        let values: [4]i32 = [1, 2, 3, 4];\n"
        "        let slice: []const i32 = values[:];\n"
        "        let [left, .., right] = values;\n"
        "        if slice is [head, ..] {\n"
        "            total += head + left + right + edge_sum(slice) + first_or_zero(slice) + array_edge_score();\n"
        "        }\n\n"
        "        let packet: Packet = Packet.other(7);\n"
        "        let .int(packet_value) | .other(packet_value) = packet else {\n"
        "            return 2;\n"
        "        };\n"
        "        total += packet_value + payload_score(Packet.none);\n\n"
        "        let ascii_bytes: [2]u8 = b\"ok\";\n"
        "        let ascii_view: []const u8 = ascii_bytes[:];\n"
        "        let ascii_text: str = strfromutf8(ascii_view);\n"
        "        let raw_text: str = unsafe { from_raw(strptr(\"ok\"), strblen(\"ok\")) };\n"
        "        if strvalid(ascii_view) && strblen(ascii_text) == strblen(raw_text) {\n"
        "            total += cast[i32](strblen(raw_text));\n"
        "        }\n\n"
        "        unsafe {\n"
        "            let zero_bits: f32 = bitcast[f32](cast[u32](0));\n"
        "            if sizeof[Packet] >= alignof[Packet] {\n"
        "                total += cast[i32](sizeof[Code]);\n"
        "            }\n"
        "        }\n\n"
        "        var loop_index: i32 = 0;\n"
        "        while loop_index < 3 {\n"
        "            loop_index += 1;\n"
        "            if loop_index == 1 {\n"
        "                continue;\n"
        "            }\n"
        "            if loop_index == 3 {\n"
        "                break;\n"
        "            }\n"
        "            total += loop_index;\n"
        "        }\n"
        "        for var j: i32 = 0; j < 3; j += 1 {\n"
        "            total += j;\n"
        "        }\n"
        "        for k in range(2, 5) {\n"
        "            total += k;\n"
        "        }\n"
        "        total += option_or(unwrap_even(4), 0) + result_or(add_one_result(4), 0);\n"
        "    }\n"
    )


def append_mixed_statement(source: list[str], index: int, remaining: int) -> int:
    value = index % AST_MIXED_VALUE_MODULUS
    periodic_feature = index > 0 and index % AST_MIXED_FEATURE_INTERVAL == 0
    if periodic_feature:
        feature_case = (index // AST_MIXED_FEATURE_INTERVAL) % 3
        if feature_case == 0 and remaining >= 4:
            suffix = str(index)
            source.append(
                "    {\n"
                f"        let local_values_{suffix}: [3]i32 = [{value}, {value + 1}, {value + 2}];\n"
                f"        let local_slice_{suffix}: []const i32 = local_values_{suffix}[:];\n"
                f"        let [first_{suffix}, .., last_{suffix}] = local_values_{suffix};\n"
                f"        total += first_{suffix} + last_{suffix} + edge_sum(local_slice_{suffix});\n"
                "    }\n"
            )
            return 4
        if feature_case == 1 and remaining >= 3:
            suffix = str(index)
            source.append(
                "    {\n"
                f"        let tuple_{suffix}: (i32, bool) = make_pair({value});\n"
                f"        let (tuple_value_{suffix}, tuple_ok_{suffix}) = tuple_{suffix};\n"
                f"        if tuple_ok_{suffix} {{ total += tuple_value_{suffix}; }} else {{ total -= 1; }}\n"
                "    }\n"
            )
            return 3
        if feature_case == 2 and remaining >= 3:
            suffix = str(index)
            source.append(
                "    {\n"
                f"        let box_{suffix}: Box[i32] = make_box[i32]({value});\n"
                f"        let maybe_{suffix}: Maybe[i32] = Maybe[i32].some(box_{suffix}.get());\n"
                f"        total += unwrap_maybe(maybe_{suffix}, 0);\n"
                "    }\n"
            )
            return 3

    if index % AST_MIXED_HEAVY_EXPRESSION_INTERVAL != 0:
        if index % AST_MIXED_LIGHT_STATE_INTERVAL == 0:
            source.append(f"    state = (state + {value}) % 997;\n")
        else:
            source.append(f"    total += {value};\n")
        return 1

    case = (index // AST_MIXED_HEAVY_EXPRESSION_INTERVAL) % AST_MIXED_DISPATCH_CASES
    if case == 0:
        source.append(f"    total += (state + {value}) & 255;\n")
        return 1
    if case == 1:
        source.append(f"    state = (state * 33 + {value}) % 997;\n")
        return 1
    if case == 2:
        source.append(f"    total += apply(op, state, {value});\n")
        return 1
    if case == 3:
        source.append(f"    total += payload_score(Packet.int({value}));\n")
        return 1
    if case == 4:
        source.append(f"    total += option_or(maybe_even({value}), 0);\n")
        return 1
    if case == 5:
        source.append(f"    total += result_or(add_one_result({value}), 0);\n")
        return 1
    if case == 6:
        source.append("    total += counter.add(1);\n")
        return 1
    if case == 7:
        source.append(f"    total += id[i32]({value});\n")
        return 1
    source.append("    if (state & 1) == 0 { total += 1; } else { total -= 1; }\n")
    return 1


def make_mixed_ast_stress_source(statement_count: int) -> str:
    source: list[str] = []
    append_ast_mixed_prelude(source)
    source.append(
        "fn main() -> i32 {\n"
        "    var total: i32 = 0;\n"
        "    var state: i32 = 1;\n"
        "    var counter = Counter.new(0);\n"
        "    let op: BinaryOp = add;\n"
    )
    append_ast_feature_block(source)
    emitted = 0
    unit = 0
    while emitted < statement_count:
        emitted += append_mixed_statement(source, unit, statement_count - emitted)
        unit += 1
    source.append("    return total + counter.read() + state;\n}\n")
    return "".join(source)


def parse_shape(text: str | None) -> str:
    shape = DEFAULT_SHAPE if text is None else text.strip()
    if shape not in AST_STRESS_SHAPES:
        valid = ", ".join(sorted(AST_STRESS_SHAPES))
        raise ValueError(f"unsupported stress shape: {shape}; expected one of: {valid}")
    return shape


def make_ast_stress_source(statement_count: int, shape: str) -> str:
    if shape == "simple":
        return make_simple_ast_stress_source(statement_count)
    return make_mixed_ast_stress_source(statement_count)


def write_source(statement_count: int, shape: str) -> pathlib.Path:
    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    path = GENERATED_ROOT / f"ast_stress_{shape}_{statement_count}.ax"
    source = make_ast_stress_source(statement_count, shape)
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


def run_stress(counts: list[int], shape: str) -> list[StressRow]:
    rows: list[StressRow] = []
    for count in counts:
        source = write_source(count, shape)
        elapsed_ms, max_rss_mib = timed_command(source)
        rows.append(
            StressRow(
                statements=count,
                shape=shape,
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
    print("Aurex bulk AST stress")
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
    print("RSS is peak process resident set size when /usr/bin/time exposes it.")
    print()
    print(f"{'statements':>10} {'shape':>8} {'source_KiB':>12} {'elapsed_ms':>12} {'peak_RSS_MiB':>14} {'source':<36}")
    print(f"{'-' * 10} {'-' * 8} {'-' * 12} {'-' * 12} {'-' * 14} {'-' * 36}")
    for row in rows:
        rss = "n/a" if row.max_rss_mib is None else f"{row.max_rss_mib:.1f}"
        print(
            f"{row.statements:>10} "
            f"{row.shape:>8} "
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
    parser.add_argument(
        "--shape",
        default=os.environ.get("AUREX_AST_STRESS_SHAPE", DEFAULT_SHAPE),
        help="stress source shape: mixed covers broad syntax; simple is the old assignment-only lane; default: mixed",
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
