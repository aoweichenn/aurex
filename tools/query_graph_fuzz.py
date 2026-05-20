#!/usr/bin/env python3
"""Deterministic query graph fuzz/stress gate for Aurex incremental caches.

The harness generates valid query-heavy Aurex inputs, validates the emitted
query DAG, then replays a checked mutation corpus against the cache. Each
mutation must be rejected through the expected pruning fallback instead of
crashing, silently reusing a bad graph, or taking an unexpected fallback path.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
from dataclasses import asdict, dataclass

import query_pruning_gate as gate


ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(
    os.environ.get(
        "AUREX_QUERY_GRAPH_FUZZ_BUILD_DIR",
        os.environ.get(
            "AUREX_STRESS_BUILD_DIR",
            os.environ.get("AUREX_BENCH_BUILD_DIR", str(ROOT / "build-perf")),
        ),
    )
).resolve()
AUREXC = BUILD / "bin" / "aurexc"
GENERATED_ROOT = BUILD / "generated" / "query-graph-fuzz"
OUTPUT_JSON = BUILD / "query-graph-fuzz.json"
DEFAULT_CORPUS = ROOT / "tests" / "corpus" / "query_graph_mutations.json"
DEFAULT_ROUNDS = 4
DEFAULT_FUNCTION_COUNT = 8
DEFAULT_FUNCTION_STEP = 8
BASELINE_SOURCE_VARIANT = 2
BASELINE_BODY_VARIANT = 0
CACHE_SUFFIX = ".axic"
PROFILE_SUFFIX = ".profile.json"
QUERY_HEADER = "queries"
QUERY_ROW = "query"
EDGE_HEADER = "query_edges"
EDGE_ROW = "query_edge"
QUERY_ROW_KIND_FIELD = 1
EDGE_DEPENDENT_KIND_FIELD = 1
EDGE_DEPENDENCY_KIND_FIELD = 7
QUERY_KEY_FIELD_COUNT = 6
EDGE_DEPENDENCY_FIELD_START = 7
EDGE_DEPENDENCY_FIELD_END = 13
STABLE_KEY_CORRUPTION_SUFFIX = "ff"


@dataclass(frozen=True)
class MutationSpec:
    name: str
    kind: str
    expected_fallback: str


@dataclass(frozen=True)
class MutationResult:
    name: str
    kind: str
    expected_fallback: str
    observed_fallback: str
    validator_rejected: bool
    profile_path: str
    cache_path: str


@dataclass(frozen=True)
class RoundResult:
    round_index: int
    function_count: int
    query_records: int
    query_edges: int
    baseline_profile: str
    mutations: list[MutationResult]


@dataclass(frozen=True)
class QueryGraphFuzzResult:
    rounds: list[RoundResult]
    corpus: str


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


def run_compiler(args: list[str]) -> None:
    completed = subprocess.run(args, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        raise RuntimeError(f"aurexc failed with exit code {completed.returncode}: {' '.join(args)}")


def load_mutation_specs(path: pathlib.Path) -> list[MutationSpec]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("version") != 1:
        raise RuntimeError(f"unsupported query graph mutation corpus version in {path}")
    mutations = data.get("mutations")
    if not isinstance(mutations, list) or not mutations:
        raise RuntimeError(f"empty query graph mutation corpus: {path}")
    return [
        MutationSpec(
            name=str(entry["name"]),
            kind=str(entry["kind"]),
            expected_fallback=str(entry["expected_fallback"]),
        )
        for entry in mutations
    ]


def write_source(function_count: int, source_variant: int, path: pathlib.Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(gate.make_source(function_count, source_variant), encoding="utf-8")


def compile_initial_cache(source_path: pathlib.Path, cache_path: pathlib.Path) -> None:
    if cache_path.exists():
        cache_path.unlink()
    run_compiler(
        [
            str(AUREXC),
            "--check",
            "--incremental-cache",
            str(cache_path),
            str(source_path),
        ]
    )


def compile_with_pruning(source_path: pathlib.Path, cache_path: pathlib.Path, profile_path: pathlib.Path) -> None:
    if profile_path.exists():
        profile_path.unlink()
    run_compiler(
        [
            str(AUREXC),
            "--emit=checked",
            "--incremental-cache",
            str(cache_path),
            "--experimental-query-pruning",
            "--profile-output",
            str(profile_path),
            str(source_path),
        ]
    )


def split_fields(line: str) -> list[str]:
    return line.split("\t")


def is_query_row(fields: list[str]) -> bool:
    return len(fields) == gate.QUERY_CACHE_FIELD_COUNT and fields[0] == QUERY_ROW


def is_edge_row(fields: list[str]) -> bool:
    return len(fields) == gate.QUERY_EDGE_CACHE_FIELD_COUNT and fields[0] == EDGE_ROW


def row_indices(lines: list[str], row_kind: str) -> list[int]:
    return [index for index, line in enumerate(lines) if split_fields(line)[0] == row_kind]


def header_index(lines: list[str], header: str) -> int:
    for index, line in enumerate(lines):
        fields = split_fields(line)
        if len(fields) == 2 and fields[0] == header:
            return index
    raise RuntimeError(f"missing cache header: {header}")


def header_count(lines: list[str], header: str) -> int:
    fields = split_fields(lines[header_index(lines, header)])
    return int(fields[1])


def set_header_count(lines: list[str], header: str, count: int) -> None:
    lines[header_index(lines, header)] = f"{header}\t{count}"


def query_fields(fields: list[str]) -> list[str]:
    return fields[1 : 1 + QUERY_KEY_FIELD_COUNT]


def edge_dependent_fields(fields: list[str]) -> list[str]:
    return fields[1 : 1 + QUERY_KEY_FIELD_COUNT]


def edge_dependency_fields(fields: list[str]) -> list[str]:
    return fields[EDGE_DEPENDENCY_FIELD_START:EDGE_DEPENDENCY_FIELD_END]


def find_query_index(lines: list[str], kind: str, excluded: set[tuple[str, ...]] | None = None) -> int:
    excluded = excluded or set()
    for index in row_indices(lines, QUERY_ROW):
        fields = split_fields(lines[index])
        if is_query_row(fields) and fields[QUERY_ROW_KIND_FIELD] == kind and tuple(query_fields(fields)) not in excluded:
            return index
    raise RuntimeError(f"missing query row kind={kind}")


def find_edge_index(lines: list[str], dependent_kind: str, dependency_kind: str) -> int:
    for index in row_indices(lines, EDGE_ROW):
        fields = split_fields(lines[index])
        if (
            is_edge_row(fields)
            and fields[EDGE_DEPENDENT_KIND_FIELD] == dependent_kind
            and fields[EDGE_DEPENDENCY_KIND_FIELD] == dependency_kind
        ):
            return index
    raise RuntimeError(f"missing query edge {dependent_kind}->{dependency_kind}")


def replace_edge_dependency(lines: list[str], edge_index: int, dependency_key_fields: list[str]) -> None:
    fields = split_fields(lines[edge_index])
    fields[EDGE_DEPENDENCY_FIELD_START:EDGE_DEPENDENCY_FIELD_END] = dependency_key_fields
    lines[edge_index] = "\t".join(fields)


def drop_query_row_keep_count(lines: list[str]) -> None:
    del lines[find_query_index(lines, "item_signature")]


def drop_query_row_fix_count(lines: list[str]) -> None:
    edge_index = find_edge_index(lines, "type_check_body", "item_signature")
    dependency = tuple(edge_dependency_fields(split_fields(lines[edge_index])))
    for index in row_indices(lines, QUERY_ROW):
        fields = split_fields(lines[index])
        if tuple(query_fields(fields)) == dependency:
            del lines[index]
            set_header_count(lines, QUERY_HEADER, header_count(lines, QUERY_HEADER) - 1)
            return
    raise RuntimeError("missing edge dependency query row to remove")


def duplicate_query_row_fix_count(lines: list[str]) -> None:
    index = find_query_index(lines, "item_signature")
    lines.insert(index + 1, lines[index])
    set_header_count(lines, QUERY_HEADER, header_count(lines, QUERY_HEADER) + 1)


def duplicate_edge_row_fix_count(lines: list[str]) -> None:
    index = find_edge_index(lines, "type_check_body", "item_signature")
    lines.insert(index + 1, lines[index])
    set_header_count(lines, EDGE_HEADER, header_count(lines, EDGE_HEADER) + 1)


def drop_edge_row_keep_count(lines: list[str]) -> None:
    del lines[find_edge_index(lines, "type_check_body", "item_signature")]


def replace_edge_dependency_wrong_kind(lines: list[str]) -> None:
    edge_index = find_edge_index(lines, "type_check_body", "item_signature")
    replacement_index = find_query_index(lines, "module_exports")
    replace_edge_dependency(lines, edge_index, query_fields(split_fields(lines[replacement_index])))


def replace_edge_dependency_wrong_identity(lines: list[str]) -> None:
    edge_index = find_edge_index(lines, "type_check_body", "item_signature")
    original_dependency = tuple(edge_dependency_fields(split_fields(lines[edge_index])))
    replacement_index = find_query_index(lines, "item_signature", {original_dependency})
    replace_edge_dependency(lines, edge_index, query_fields(split_fields(lines[replacement_index])))


def corrupt_query_stable_key(lines: list[str]) -> None:
    index = find_query_index(lines, "item_signature")
    fields = split_fields(lines[index])
    fields[gate.QUERY_CACHE_STABLE_KEY_FIELD] += STABLE_KEY_CORRUPTION_SUFFIX
    lines[index] = "\t".join(fields)


MUTATION_HANDLERS = {
    "drop_query_row_keep_count": drop_query_row_keep_count,
    "drop_query_row_fix_count": drop_query_row_fix_count,
    "duplicate_query_row_fix_count": duplicate_query_row_fix_count,
    "duplicate_edge_row_fix_count": duplicate_edge_row_fix_count,
    "drop_edge_row_keep_count": drop_edge_row_keep_count,
    "replace_edge_dependency_wrong_kind": replace_edge_dependency_wrong_kind,
    "replace_edge_dependency_wrong_identity": replace_edge_dependency_wrong_identity,
    "corrupt_query_stable_key": corrupt_query_stable_key,
}


def mutate_cache(source_cache: pathlib.Path, mutated_cache: pathlib.Path, spec: MutationSpec) -> bool:
    lines = source_cache.read_text(encoding="utf-8").splitlines()
    handler = MUTATION_HANDLERS.get(spec.kind)
    if handler is None:
        raise RuntimeError(f"unknown query graph mutation: {spec.kind}")
    handler(lines)
    mutated_cache.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return cache_invariants_reject(mutated_cache)


def has_duplicate(values: list[tuple[str, ...]]) -> bool:
    return len(values) != len(set(values))


def cache_invariants_reject(cache_path: pathlib.Path) -> bool:
    lines = cache_path.read_text(encoding="utf-8").splitlines()
    query_keys: list[tuple[str, ...]] = []
    edge_keys: list[tuple[str, ...]] = []
    for line in lines:
        fields = split_fields(line)
        if is_query_row(fields):
            query_keys.append(tuple(query_fields(fields)))
            continue
        if is_edge_row(fields):
            edge_keys.append(tuple(fields[1:EDGE_DEPENDENCY_FIELD_END]))
    if header_count(lines, QUERY_HEADER) != len(query_keys):
        return True
    if header_count(lines, EDGE_HEADER) != len(edge_keys):
        return True
    if has_duplicate(query_keys) or has_duplicate(edge_keys):
        return True
    try:
        gate.validate_cache_query_graph(cache_path)
    except RuntimeError:
        return True
    return False


def require_pruning_fallback(profile_path: pathlib.Path, expected_fallback: str) -> str:
    _, fields = gate.load_profile_phase(profile_path, gate.QUERY_PRUNING_PHASE)
    observed = fields.get("fallback")
    if observed != expected_fallback:
        raise RuntimeError(f"{profile_path}: expected fallback={expected_fallback}, got {observed!r}")
    if expected_fallback == "none":
        if fields.get("applied") != "1":
            raise RuntimeError(f"{profile_path}: expected applied=1 for clean query graph")
    elif fields.get("applied") != "0":
        raise RuntimeError(f"{profile_path}: expected applied=0 for rejected query graph")
    return observed


def run_round(round_index: int, function_count: int, mutations: list[MutationSpec]) -> RoundResult:
    round_root = GENERATED_ROOT / f"round_{round_index}_functions_{function_count}"
    round_root.mkdir(parents=True, exist_ok=True)
    baseline_source = round_root / "baseline.ax"
    pruning_source = round_root / "pruning.ax"
    clean_cache = round_root / ("clean" + CACHE_SUFFIX)
    clean_profile = round_root / ("clean" + PROFILE_SUFFIX)
    write_source(function_count, BASELINE_BODY_VARIANT, baseline_source)
    compile_initial_cache(baseline_source, clean_cache)
    query_records, query_edges = gate.validate_cache_query_graph(clean_cache)
    write_source(function_count, BASELINE_SOURCE_VARIANT, pruning_source)
    compile_with_pruning(pruning_source, clean_cache, clean_profile)
    require_pruning_fallback(clean_profile, "none")

    mutation_results: list[MutationResult] = []
    for mutation in mutations:
        mutation_cache = round_root / f"{mutation.name}{CACHE_SUFFIX}"
        mutation_profile = round_root / f"{mutation.name}{PROFILE_SUFFIX}"
        shutil.copyfile(clean_cache, mutation_cache)
        validator_rejected = mutate_cache(clean_cache, mutation_cache, mutation)
        if not validator_rejected:
            raise RuntimeError(f"mutation did not violate Python query graph validator: {mutation.name}")
        compile_with_pruning(pruning_source, mutation_cache, mutation_profile)
        observed_fallback = require_pruning_fallback(mutation_profile, mutation.expected_fallback)
        mutation_results.append(
            MutationResult(
                name=mutation.name,
                kind=mutation.kind,
                expected_fallback=mutation.expected_fallback,
                observed_fallback=observed_fallback,
                validator_rejected=validator_rejected,
                profile_path=str(mutation_profile),
                cache_path=str(mutation_cache),
            )
        )

    return RoundResult(
        round_index=round_index,
        function_count=function_count,
        query_records=query_records,
        query_edges=query_edges,
        baseline_profile=str(clean_profile),
        mutations=mutation_results,
    )


def write_json(result: QueryGraphFuzzResult) -> None:
    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT_JSON.open("w", encoding="utf-8") as output:
        json.dump(asdict(result), output, indent=2, sort_keys=True)
        output.write("\n")


def print_report(result: QueryGraphFuzzResult) -> None:
    print("Aurex query graph fuzz gate")
    print(f"corpus: {result.corpus}")
    for round_result in result.rounds:
        print(
            f"round={round_result.round_index},functions={round_result.function_count},"
            f"query_records={round_result.query_records},query_edges={round_result.query_edges},"
            f"mutations={len(round_result.mutations)}"
        )
        for mutation in round_result.mutations:
            print(
                f"  {mutation.name}: fallback={mutation.observed_fallback},"
                f"validator_rejected={int(mutation.validator_rejected)}"
            )
    print(f"json: {OUTPUT_JSON}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true", help="reuse the existing aurexc binary")
    parser.add_argument("--rounds", type=int, default=DEFAULT_ROUNDS, help=f"default: {DEFAULT_ROUNDS}")
    parser.add_argument("--functions", type=int, default=DEFAULT_FUNCTION_COUNT, help=f"default: {DEFAULT_FUNCTION_COUNT}")
    parser.add_argument(
        "--function-step",
        type=int,
        default=DEFAULT_FUNCTION_STEP,
        help=f"default: {DEFAULT_FUNCTION_STEP}",
    )
    parser.add_argument("--corpus", type=pathlib.Path, default=DEFAULT_CORPUS, help="mutation corpus JSON")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.rounds <= 0:
        raise ValueError("--rounds must be positive")
    if args.functions < 2:
        raise ValueError("--functions must be at least 2 so identity mutations have alternate rows")
    if args.function_step < 0:
        raise ValueError("--function-step must be non-negative")
    if not args.skip_build:
        build_compiler()
    mutations = load_mutation_specs(args.corpus)
    rounds = [
        run_round(index, args.functions + (index * args.function_step), mutations)
        for index in range(args.rounds)
    ]
    result = QueryGraphFuzzResult(rounds=rounds, corpus=str(args.corpus))
    write_json(result)
    print_report(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
