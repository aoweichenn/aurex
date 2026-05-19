#!/usr/bin/env python3
"""Query pruning provider-skip gate for Aurex incremental cache.

This gate builds a small stable module, writes an incremental cache once, then
changes only a function body and rewrites the cache with
`--experimental-query-pruning` enabled. The body-only edit avoids the coarse
source-fingerprint cache hit while keeping query signatures reusable. The
expected all-reuse behavior is observable in the profile: query rows may be
seeded from cache, but zero query providers should be evaluated.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import sys
from dataclasses import asdict, dataclass


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(
    os.environ.get(
        "AUREX_QUERY_PRUNING_BUILD_DIR",
        os.environ.get(
            "AUREX_STRESS_BUILD_DIR",
            os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build-perf")),
        ),
    )
).resolve()
AUREXC = BUILD / "bin" / "aurexc"
GENERATED_ROOT = BUILD / "generated" / "query-pruning-gate"
OUTPUT_JSON = BUILD / "query-pruning-gate.json"

DEFAULT_FUNCTION_COUNT = 32
QUERY_PROVIDER_EVAL_PHASE = "incremental_cache.query_provider_eval"


@dataclass(frozen=True)
class QueryPruningGateResult:
    function_count: int
    source_bytes: int
    cache_path: str
    profile_path: str
    provider_eval_detail: str
    provider_eval_fields: dict[str, str]
    source: str


def configure() -> None:
    cmd = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(BUILD),
        "-DCMAKE_BUILD_TYPE=Release",
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


def make_source(function_count: int, body_variant: int) -> str:
    source: list[str] = [
        "module perf.query_pruning_gate;\n\n",
    ]
    for index in range(function_count):
        source.append(f"fn helper_{index}() -> i32 {{ return {index}; }}\n")
    source.append("\nfn main() -> i32 {\n")
    if function_count > 0 and body_variant == 0:
        source.append("    return helper_0();\n")
    elif function_count > 0:
        source.append("    return helper_0() + 0;\n")
    elif body_variant == 0:
        source.append("    return 0;\n")
    else:
        source.append("    return 0 + 0;\n")
    source.append("}\n")
    return "".join(source)


def write_source(function_count: int, body_variant: int) -> pathlib.Path:
    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    source_path = GENERATED_ROOT / f"query_pruning_gate_{function_count}.ax"
    source_path.write_text(make_source(function_count, body_variant), encoding="utf-8")
    return source_path


def run_compiler(args: list[str]) -> None:
    completed = subprocess.run(args, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        raise RuntimeError(f"aurexc failed with exit code {completed.returncode}: {' '.join(args)}")


def parse_detail_fields(detail: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for part in detail.split(","):
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        fields[key] = value
    return fields


def load_provider_eval_phase(profile_path: pathlib.Path) -> tuple[str, dict[str, str]]:
    with profile_path.open("r", encoding="utf-8") as profile_file:
        profile = json.load(profile_file)
    for phase in profile.get("phases", []):
        if phase.get("name") != QUERY_PROVIDER_EVAL_PHASE:
            continue
        detail = str(phase.get("detail", ""))
        return detail, parse_detail_fields(detail)
    raise RuntimeError(f"missing profile phase: {QUERY_PROVIDER_EVAL_PHASE}")


def require_int_field(fields: dict[str, str], name: str) -> int:
    try:
        return int(fields[name])
    except KeyError as error:
        raise RuntimeError(f"missing query provider eval field: {name}") from error
    except ValueError as error:
        raise RuntimeError(f"non-integer query provider eval field {name}: {fields[name]}") from error


def verify_provider_skip(fields: dict[str, str], min_seeded: int) -> None:
    mode = fields.get("mode")
    if mode != "pruned":
        raise RuntimeError(f"expected query provider eval mode=pruned, got {mode!r}")

    seeded = require_int_field(fields, "seeded")
    evaluated = require_int_field(fields, "evaluated")
    if seeded < min_seeded:
        raise RuntimeError(f"expected at least {min_seeded} seeded queries, got {seeded}")
    if evaluated != 0:
        raise RuntimeError(f"expected zero evaluated providers in all-reuse pruning, got {evaluated}")

    evaluated_kinds = [
        "evaluated_module_exports",
        "evaluated_item_signatures",
        "evaluated_generic_instance_signatures",
    ]
    for field in evaluated_kinds:
        value = require_int_field(fields, field)
        if value != 0:
            raise RuntimeError(f"expected {field}=0 in all-reuse pruning, got {value}")


def run_gate(function_count: int, min_seeded: int) -> QueryPruningGateResult:
    source_path = write_source(function_count, body_variant=0)
    cache_path = GENERATED_ROOT / "query_pruning_gate.axic"
    profile_path = GENERATED_ROOT / "query_pruning_gate.profile.json"
    if cache_path.exists():
        cache_path.unlink()
    if profile_path.exists():
        profile_path.unlink()

    run_compiler(
        [
            str(AUREXC),
            "--check",
            "--incremental-cache",
            str(cache_path),
            str(source_path),
        ]
    )
    source_path = write_source(function_count, body_variant=1)
    run_compiler(
        [
            str(AUREXC),
            "--check",
            "--incremental-cache",
            str(cache_path),
            "--experimental-query-pruning",
            "--profile-output",
            str(profile_path),
            str(source_path),
        ]
    )

    detail, fields = load_provider_eval_phase(profile_path)
    verify_provider_skip(fields, min_seeded)
    source = source_path.read_text(encoding="utf-8")
    return QueryPruningGateResult(
        function_count=function_count,
        source_bytes=len(source.encode("utf-8")),
        cache_path=str(cache_path),
        profile_path=str(profile_path),
        provider_eval_detail=detail,
        provider_eval_fields=fields,
        source=str(source_path),
    )


def write_json(result: QueryPruningGateResult) -> None:
    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT_JSON.open("w", encoding="utf-8") as output:
        json.dump(asdict(result), output, indent=2, sort_keys=True)
        output.write("\n")


def print_report(result: QueryPruningGateResult) -> None:
    print("Aurex query pruning provider-skip gate")
    print(f"functions: {result.function_count}")
    print(f"source_bytes: {result.source_bytes}")
    print(f"profile: {result.profile_path}")
    print(f"provider_eval: {result.provider_eval_detail}")
    print(f"json: {OUTPUT_JSON}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--functions",
        type=int,
        default=int(os.environ.get("AUREX_QUERY_PRUNING_GATE_FUNCTIONS", str(DEFAULT_FUNCTION_COUNT))),
        help=f"number of helper functions in the generated module; default: {DEFAULT_FUNCTION_COUNT}",
    )
    parser.add_argument(
        "--min-seeded",
        type=int,
        default=int(os.environ.get("AUREX_QUERY_PRUNING_GATE_MIN_SEEDED", "1")),
        help="minimum seeded query count required in all-reuse pruning; default: 1",
    )
    parser.add_argument("--skip-build", action="store_true", help="reuse the existing aurexc binary")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.functions < 0:
        raise ValueError("--functions must be non-negative")
    if args.min_seeded < 0:
        raise ValueError("--min-seeded must be non-negative")
    if not args.skip_build:
        build_compiler()
    result = run_gate(args.functions, args.min_seeded)
    write_json(result)
    print_report(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
