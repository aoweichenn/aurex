#!/usr/bin/env python3
"""Query pruning provider-skip gate for Aurex incremental cache.

This gate builds a compact module, writes an incremental cache, then rewrites
the cache under `--experimental-query-pruning` in three scenarios:

1. all-reuse: the source is unchanged, so every query provider should be seeded
   from cache and zero providers should run.
2. body-recompute: only the main function body changes, so the body syntax and
   type-check body providers should run while signature/module queries are
   seeded.
3. generic-recompute: the cached generic instance signature result changes, so
   exactly one generic provider should run while the remaining queries are
   seeded.

The gate checks the query diff, plan, pruning, and provider-eval profile
phases so it validates both sides of the pruning boundary instead of relying on
an indirect heuristic.
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
QUERY_DIFF_PHASE = "incremental_cache.query_diff"
QUERY_PLAN_PHASE = "incremental_cache.query_plan"
QUERY_PRUNING_PHASE = "incremental_cache.query_pruning"
QUERY_PROVIDER_EVAL_PHASE = "incremental_cache.query_provider_eval"
QUERY_CACHE_FIELD_COUNT = 12
QUERY_CACHE_KIND_FIELD = 0
QUERY_CACHE_QUERY_KIND_FIELD = 1
QUERY_CACHE_RESULT_GLOBAL_FIELD = 7
QUERY_CACHE_GENERIC_INSTANCE_KIND = "generic_instance_signature"
QUERY_EDGE_CACHE_FIELD_COUNT = 13
QUERY_EDGE_CACHE_HEADER = "query_edges"
QUERY_EDGE_CACHE_KIND = "query_edge"
QUERY_EDGE_DEPENDENT_KIND_FIELD = 1
QUERY_EDGE_DEPENDENCY_KIND_FIELD = 7
QUERY_DEPENDENCY_SCHEDULE = {
    "file_content": 0,
    "lex_file": 1,
    "parse_file": 2,
    "module_graph": 3,
    "item_list": 4,
    "module_exports": 5,
    "item_signature": 6,
    "generic_template_signature": 7,
    "generic_instance_signature": 8,
    "function_body_syntax": 9,
    "type_check_body": 10,
    "generic_instance_body": 11,
    "lower_function_ir": 12,
    "diagnostics": 13,
}
EXPECTED_QUERY_DEPENDENCY_KINDS = {
    "lex_file": {"file_content"},
    "parse_file": {"lex_file"},
    "item_list": {"module_graph"},
    "module_exports": {"item_list"},
    "item_signature": {"module_exports"},
    "generic_template_signature": {"item_list"},
    "generic_instance_signature": {"generic_template_signature"},
    "type_check_body": {"function_body_syntax", "item_signature"},
    "generic_instance_body": {"generic_instance_signature"},
    "lower_function_ir": {"type_check_body", "generic_instance_body"},
}

ALL_REUSE_SCENARIO = "all_reuse"
BODY_RECOMPUTE_SCENARIO = "body_recompute"
GENERIC_RECOMPUTE_SCENARIO = "generic_recompute"


@dataclass(frozen=True)
class ScenarioExpectation:
    name: str
    expected_diff_total: int
    expected_diff_missing: int
    expected_diff_unchanged: int
    expected_diff_changed: int
    expected_plan_reusable: int
    expected_plan_recompute_roots: int
    expected_plan_propagated_recompute: int
    expected_plan_recompute: int
    expected_pruning_reused: int
    expected_pruning_recomputed: int
    expected_pruning_reused_file_contents: int
    expected_pruning_reused_lex_files: int
    expected_pruning_reused_parse_files: int
    expected_pruning_reused_module_graphs: int
    expected_pruning_reused_module_exports: int
    expected_pruning_reused_item_lists: int
    expected_pruning_reused_item_signatures: int
    expected_pruning_reused_function_body_syntaxes: int
    expected_pruning_reused_type_check_bodies: int
    expected_pruning_reused_generic_template_signatures: int
    expected_pruning_reused_generic_instance_signatures: int
    expected_pruning_reused_generic_instance_bodies: int
    expected_pruning_reused_lower_function_irs: int
    expected_pruning_reused_diagnostics: int
    expected_pruning_recomputed_file_contents: int
    expected_pruning_recomputed_lex_files: int
    expected_pruning_recomputed_parse_files: int
    expected_pruning_recomputed_module_graphs: int
    expected_pruning_recomputed_module_exports: int
    expected_pruning_recomputed_item_lists: int
    expected_pruning_recomputed_item_signatures: int
    expected_pruning_recomputed_function_body_syntaxes: int
    expected_pruning_recomputed_type_check_bodies: int
    expected_pruning_recomputed_generic_template_signatures: int
    expected_pruning_recomputed_generic_instance_signatures: int
    expected_pruning_recomputed_generic_instance_bodies: int
    expected_pruning_recomputed_lower_function_irs: int
    expected_pruning_recomputed_diagnostics: int
    expected_provider_seeded_file_contents: int
    expected_provider_seeded_lex_files: int
    expected_provider_seeded_parse_files: int
    expected_provider_seeded_module_graphs: int
    expected_provider_seeded_module_exports: int
    expected_provider_seeded_item_lists: int
    expected_provider_seeded_item_signatures: int
    expected_provider_seeded_function_body_syntaxes: int
    expected_provider_seeded_type_check_bodies: int
    expected_provider_seeded_generic_template_signatures: int
    expected_provider_seeded_generic_instance_signatures: int
    expected_provider_seeded_generic_instance_bodies: int
    expected_provider_seeded_lower_function_irs: int
    expected_provider_seeded_diagnostics: int
    expected_provider_evaluated_file_contents: int
    expected_provider_evaluated_lex_files: int
    expected_provider_evaluated_parse_files: int
    expected_provider_evaluated_module_graphs: int
    expected_provider_evaluated_module_exports: int
    expected_provider_evaluated_item_lists: int
    expected_provider_evaluated_item_signatures: int
    expected_provider_evaluated_function_body_syntaxes: int
    expected_provider_evaluated_type_check_bodies: int
    expected_provider_evaluated_generic_template_signatures: int
    expected_provider_evaluated_generic_instance_signatures: int
    expected_provider_evaluated_generic_instance_bodies: int
    expected_provider_evaluated_lower_function_irs: int
    expected_provider_evaluated_diagnostics: int


@dataclass(frozen=True)
class ScenarioResult:
    name: str
    source_variant: int
    function_count: int
    source_bytes: int
    cache_path: str
    profile_path: str
    query_diff_detail: str
    query_diff_fields: dict[str, str]
    query_plan_detail: str
    query_plan_fields: dict[str, str]
    query_pruning_detail: str
    query_pruning_fields: dict[str, str]
    provider_eval_detail: str
    provider_eval_fields: dict[str, str]
    query_edge_count: int
    source: str


@dataclass(frozen=True)
class QueryPruningGateResult:
    function_count: int
    scenarios: list[ScenarioResult]


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
        "module perf.query_pruning_gate;\n",
    ]
    if body_variant == 2:
        source.append("// cache-bypass change outside query subjects\n")
    source.append("\n")
    source.append(
        "struct Box[T] {\n"
        "    value: T;\n"
        "}\n\n"
    )
    for index in range(function_count):
        source.append(f"fn helper_{index}() -> i32 {{ return {index}; }}\n")
    source.append("fn main() -> i32 {\n")
    if function_count > 0:
        seed_value = "helper_0()"
    else:
        seed_value = "0"
    source.append(f"    let box: Box[i32] = Box[i32] {{ value: {seed_value} }};\n")
    if body_variant == 0 or body_variant == 2:
        source.append("    return box.value;\n")
    elif body_variant == 1:
        source.append("    let baseline: i32 = box.value;\n")
        source.append("    return baseline + 0;\n")
    else:
        raise ValueError(f"unsupported body variant: {body_variant}")
    source.append("}\n")
    return "".join(source)


def write_source(function_count: int, body_variant: int, scenario_name: str) -> pathlib.Path:
    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    source_path = GENERATED_ROOT / f"query_pruning_gate_{scenario_name}_{function_count}.ax"
    source_path.write_text(make_source(function_count, body_variant), encoding="utf-8")
    return source_path


def run_compiler(args: list[str]) -> None:
    completed = subprocess.run(args, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        raise RuntimeError(f"aurexc failed with exit code {completed.returncode}: {' '.join(args)}")


def mutate_cached_query_result(cache_path: pathlib.Path) -> None:
    lines = cache_path.read_text(encoding="utf-8").splitlines()
    for index, line in enumerate(lines):
        fields = line.split("\t")
        if len(fields) != QUERY_CACHE_FIELD_COUNT or fields[QUERY_CACHE_KIND_FIELD] != "query":
            continue
        if fields[QUERY_CACHE_QUERY_KIND_FIELD] != QUERY_CACHE_GENERIC_INSTANCE_KIND:
            continue
        result_global = int(fields[QUERY_CACHE_RESULT_GLOBAL_FIELD])
        if result_global == 0:
            raise RuntimeError(f"invalid generic instance query result in cache: {line!r}")
        fields[QUERY_CACHE_RESULT_GLOBAL_FIELD] = str(result_global + 1)
        lines[index] = "\t".join(fields)
        cache_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return
    raise RuntimeError(f"missing generic instance query record in cache: {cache_path}")


def query_edge_kind_is_expected(dependent: str, dependency: str) -> bool:
    if dependent == "diagnostics":
        return bool(dependency) and dependency != "diagnostics"
    return dependency in EXPECTED_QUERY_DEPENDENCY_KINDS.get(dependent, set())


def validate_cache_query_edges(cache_path: pathlib.Path) -> int:
    expected_count: int | None = None
    edge_count = 0
    for line_number, line in enumerate(cache_path.read_text(encoding="utf-8").splitlines(), start=1):
        fields = line.split("\t")
        if not fields:
            continue
        if fields[0] == QUERY_EDGE_CACHE_HEADER:
            if len(fields) != 2:
                raise RuntimeError(f"malformed query_edges header at {cache_path}:{line_number}: {line!r}")
            expected_count = int(fields[1])
            continue
        if fields[0] != QUERY_EDGE_CACHE_KIND:
            continue
        if len(fields) != QUERY_EDGE_CACHE_FIELD_COUNT:
            raise RuntimeError(f"malformed query_edge row at {cache_path}:{line_number}: {line!r}")
        dependent = fields[QUERY_EDGE_DEPENDENT_KIND_FIELD]
        dependency = fields[QUERY_EDGE_DEPENDENCY_KIND_FIELD]
        dependent_rank = QUERY_DEPENDENCY_SCHEDULE.get(dependent)
        dependency_rank = QUERY_DEPENDENCY_SCHEDULE.get(dependency)
        if dependent_rank is None or dependency_rank is None:
            raise RuntimeError(f"unknown query_edge kind at {cache_path}:{line_number}: {line!r}")
        if dependency_rank > dependent_rank:
            raise RuntimeError(f"backward query_edge schedule at {cache_path}:{line_number}: {line!r}")
        if not query_edge_kind_is_expected(dependent, dependency):
            raise RuntimeError(f"unexpected query_edge dependency kind at {cache_path}:{line_number}: {line!r}")
        edge_count += 1
    if expected_count is None:
        raise RuntimeError(f"missing query_edges header in cache: {cache_path}")
    if expected_count != edge_count:
        raise RuntimeError(f"query_edges header mismatch in {cache_path}: expected {expected_count}, got {edge_count}")
    if edge_count == 0:
        raise RuntimeError(f"cache has no query dependency edges: {cache_path}")
    return edge_count


def parse_detail_fields(detail: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for part in detail.split(","):
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        fields[key] = value
    return fields


def load_profile_phase(profile_path: pathlib.Path, phase_name: str) -> tuple[str, dict[str, str]]:
    with profile_path.open("r", encoding="utf-8") as profile_file:
        profile = json.load(profile_file)
    for phase in profile.get("phases", []):
        if phase.get("name") != phase_name:
            continue
        detail = str(phase.get("detail", ""))
        return detail, parse_detail_fields(detail)
    raise RuntimeError(f"missing profile phase: {phase_name}")


def require_exact_field(fields: dict[str, str], name: str, expected: str) -> None:
    value = fields.get(name)
    if value != expected:
        raise RuntimeError(f"expected {name}={expected}, got {value!r}")


def query_subject_counts(function_count: int) -> tuple[int, int, int, int, int, int, int, int, int, int, int, int, int, int, int]:
    file_contents = 1
    lex_files = 1
    parse_files = 1
    module_graphs = 1
    module_exports = 1
    item_lists = 1
    item_signatures = function_count + 2
    function_body_syntaxes = function_count + 1
    type_check_bodies = function_count + 1
    generic_template_signatures = 1
    generic_instance_signatures = 1
    generic_instance_bodies = 0
    lower_function_irs = type_check_bodies + generic_instance_bodies
    diagnostics = (
        file_contents
        + lex_files
        + parse_files
        + module_graphs
        + module_exports
        + item_lists
        + item_signatures
        + function_body_syntaxes
        + type_check_bodies
        + generic_template_signatures
        + generic_instance_signatures
        + generic_instance_bodies
        + lower_function_irs
    )
    total = (
        file_contents
        + lex_files
        + parse_files
        + module_graphs
        + module_exports
        + item_lists
        + item_signatures
        + function_body_syntaxes
        + type_check_bodies
        + generic_template_signatures
        + generic_instance_signatures
        + generic_instance_bodies
        + lower_function_irs
        + diagnostics
    )
    return (
        file_contents,
        lex_files,
        parse_files,
        module_graphs,
        module_exports,
        item_lists,
        item_signatures,
        function_body_syntaxes,
        type_check_bodies,
        generic_template_signatures,
        generic_instance_signatures,
        generic_instance_bodies,
        lower_function_irs,
        diagnostics,
        total,
    )


def make_expectation(function_count: int, scenario_name: str) -> ScenarioExpectation:
    (
        file_contents,
        lex_files,
        parse_files,
        module_graphs,
        module_exports,
        item_lists,
        item_signatures,
        function_body_syntaxes,
        type_check_bodies,
        generic_template_signatures,
        generic_instance_signatures,
        generic_instance_bodies,
        lower_function_irs,
        diagnostics,
        total,
    ) = query_subject_counts(function_count)
    source_stage_queries = file_contents + lex_files + parse_files
    if scenario_name == ALL_REUSE_SCENARIO:
        changed_roots = file_contents
        changed_diagnostics = 0
        propagated_recompute = 0
        recomputed = changed_roots + propagated_recompute
        return ScenarioExpectation(
            name=scenario_name,
            expected_diff_total=total,
            expected_diff_missing=0,
            expected_diff_unchanged=total - changed_roots,
            expected_diff_changed=changed_roots,
            expected_plan_reusable=total - recomputed,
            expected_plan_recompute_roots=changed_roots,
            expected_plan_propagated_recompute=propagated_recompute,
            expected_plan_recompute=recomputed,
            expected_pruning_reused=total - recomputed,
            expected_pruning_recomputed=recomputed,
            expected_pruning_reused_file_contents=0,
            expected_pruning_reused_lex_files=lex_files,
            expected_pruning_reused_parse_files=parse_files,
            expected_pruning_reused_module_graphs=module_graphs,
            expected_pruning_reused_module_exports=module_exports,
            expected_pruning_reused_item_lists=item_lists,
            expected_pruning_reused_item_signatures=item_signatures,
            expected_pruning_reused_function_body_syntaxes=function_body_syntaxes,
            expected_pruning_reused_type_check_bodies=type_check_bodies,
            expected_pruning_reused_generic_template_signatures=generic_template_signatures,
            expected_pruning_reused_generic_instance_signatures=generic_instance_signatures,
            expected_pruning_reused_generic_instance_bodies=generic_instance_bodies,
            expected_pruning_reused_lower_function_irs=lower_function_irs,
            expected_pruning_reused_diagnostics=diagnostics - changed_diagnostics,
            expected_pruning_recomputed_file_contents=file_contents,
            expected_pruning_recomputed_lex_files=0,
            expected_pruning_recomputed_parse_files=0,
            expected_pruning_recomputed_module_graphs=0,
            expected_pruning_recomputed_module_exports=0,
            expected_pruning_recomputed_item_lists=0,
            expected_pruning_recomputed_item_signatures=0,
            expected_pruning_recomputed_function_body_syntaxes=0,
            expected_pruning_recomputed_type_check_bodies=0,
            expected_pruning_recomputed_generic_template_signatures=0,
            expected_pruning_recomputed_generic_instance_signatures=0,
            expected_pruning_recomputed_generic_instance_bodies=0,
            expected_pruning_recomputed_lower_function_irs=0,
            expected_pruning_recomputed_diagnostics=changed_diagnostics,
            expected_provider_seeded_file_contents=0,
            expected_provider_seeded_lex_files=lex_files,
            expected_provider_seeded_parse_files=parse_files,
            expected_provider_seeded_module_graphs=module_graphs,
            expected_provider_seeded_module_exports=module_exports,
            expected_provider_seeded_item_lists=item_lists,
            expected_provider_seeded_item_signatures=item_signatures,
            expected_provider_seeded_function_body_syntaxes=function_body_syntaxes,
            expected_provider_seeded_type_check_bodies=type_check_bodies,
            expected_provider_seeded_generic_template_signatures=generic_template_signatures,
            expected_provider_seeded_generic_instance_signatures=generic_instance_signatures,
            expected_provider_seeded_generic_instance_bodies=generic_instance_bodies,
            expected_provider_seeded_lower_function_irs=lower_function_irs,
            expected_provider_seeded_diagnostics=diagnostics - changed_diagnostics,
            expected_provider_evaluated_file_contents=file_contents,
            expected_provider_evaluated_lex_files=0,
            expected_provider_evaluated_parse_files=0,
            expected_provider_evaluated_module_graphs=0,
            expected_provider_evaluated_module_exports=0,
            expected_provider_evaluated_item_lists=0,
            expected_provider_evaluated_item_signatures=0,
            expected_provider_evaluated_function_body_syntaxes=0,
            expected_provider_evaluated_type_check_bodies=0,
            expected_provider_evaluated_generic_template_signatures=0,
            expected_provider_evaluated_generic_instance_signatures=0,
            expected_provider_evaluated_generic_instance_bodies=0,
            expected_provider_evaluated_lower_function_irs=0,
            expected_provider_evaluated_diagnostics=changed_diagnostics,
        )
    if scenario_name == BODY_RECOMPUTE_SCENARIO:
        changed_lower_function_irs = 1
        changed_body_queries = 2 + changed_lower_function_irs
        changed_roots = source_stage_queries + changed_body_queries
        changed_diagnostics = 0
        return ScenarioExpectation(
            name=scenario_name,
            expected_diff_total=total,
            expected_diff_missing=0,
            expected_diff_unchanged=total - changed_roots,
            expected_diff_changed=changed_roots,
            expected_plan_reusable=total - changed_roots - changed_diagnostics,
            expected_plan_recompute_roots=changed_roots,
            expected_plan_propagated_recompute=changed_diagnostics,
            expected_plan_recompute=changed_roots + changed_diagnostics,
            expected_pruning_reused=total - changed_roots - changed_diagnostics,
            expected_pruning_recomputed=changed_roots + changed_diagnostics,
            expected_pruning_reused_file_contents=0,
            expected_pruning_reused_lex_files=0,
            expected_pruning_reused_parse_files=0,
            expected_pruning_reused_module_graphs=module_graphs,
            expected_pruning_reused_module_exports=module_exports,
            expected_pruning_reused_item_lists=item_lists,
            expected_pruning_reused_item_signatures=item_signatures,
            expected_pruning_reused_function_body_syntaxes=function_body_syntaxes - 1,
            expected_pruning_reused_type_check_bodies=type_check_bodies - 1,
            expected_pruning_reused_generic_template_signatures=generic_template_signatures,
            expected_pruning_reused_generic_instance_signatures=generic_instance_signatures,
            expected_pruning_reused_generic_instance_bodies=generic_instance_bodies,
            expected_pruning_reused_lower_function_irs=lower_function_irs - changed_lower_function_irs,
            expected_pruning_reused_diagnostics=diagnostics - changed_diagnostics,
            expected_pruning_recomputed_file_contents=file_contents,
            expected_pruning_recomputed_lex_files=lex_files,
            expected_pruning_recomputed_parse_files=parse_files,
            expected_pruning_recomputed_module_graphs=0,
            expected_pruning_recomputed_module_exports=0,
            expected_pruning_recomputed_item_lists=0,
            expected_pruning_recomputed_item_signatures=0,
            expected_pruning_recomputed_function_body_syntaxes=1,
            expected_pruning_recomputed_type_check_bodies=1,
            expected_pruning_recomputed_generic_template_signatures=0,
            expected_pruning_recomputed_generic_instance_signatures=0,
            expected_pruning_recomputed_generic_instance_bodies=0,
            expected_pruning_recomputed_lower_function_irs=changed_lower_function_irs,
            expected_pruning_recomputed_diagnostics=changed_diagnostics,
            expected_provider_seeded_file_contents=0,
            expected_provider_seeded_lex_files=0,
            expected_provider_seeded_parse_files=0,
            expected_provider_seeded_module_graphs=module_graphs,
            expected_provider_seeded_module_exports=module_exports,
            expected_provider_seeded_item_lists=item_lists,
            expected_provider_seeded_item_signatures=item_signatures,
            expected_provider_seeded_function_body_syntaxes=function_body_syntaxes - 1,
            expected_provider_seeded_type_check_bodies=type_check_bodies - 1,
            expected_provider_seeded_generic_template_signatures=generic_template_signatures,
            expected_provider_seeded_generic_instance_signatures=generic_instance_signatures,
            expected_provider_seeded_generic_instance_bodies=generic_instance_bodies,
            expected_provider_seeded_lower_function_irs=lower_function_irs - changed_lower_function_irs,
            expected_provider_seeded_diagnostics=diagnostics - changed_diagnostics,
            expected_provider_evaluated_file_contents=file_contents,
            expected_provider_evaluated_lex_files=lex_files,
            expected_provider_evaluated_parse_files=parse_files,
            expected_provider_evaluated_module_graphs=0,
            expected_provider_evaluated_module_exports=0,
            expected_provider_evaluated_item_lists=0,
            expected_provider_evaluated_item_signatures=0,
            expected_provider_evaluated_function_body_syntaxes=1,
            expected_provider_evaluated_type_check_bodies=1,
            expected_provider_evaluated_generic_template_signatures=0,
            expected_provider_evaluated_generic_instance_signatures=0,
            expected_provider_evaluated_generic_instance_bodies=0,
            expected_provider_evaluated_lower_function_irs=changed_lower_function_irs,
            expected_provider_evaluated_diagnostics=changed_diagnostics,
        )
    if scenario_name == GENERIC_RECOMPUTE_SCENARIO:
        changed_roots = file_contents + generic_instance_signatures
        changed_diagnostics = 0
        propagated_recompute = 0
        recomputed = changed_roots + propagated_recompute
        return ScenarioExpectation(
            name=scenario_name,
            expected_diff_total=total,
            expected_diff_missing=0,
            expected_diff_unchanged=total - changed_roots,
            expected_diff_changed=changed_roots,
            expected_plan_reusable=total - recomputed,
            expected_plan_recompute_roots=changed_roots,
            expected_plan_propagated_recompute=propagated_recompute,
            expected_plan_recompute=recomputed,
            expected_pruning_reused=total - recomputed,
            expected_pruning_recomputed=recomputed,
            expected_pruning_reused_file_contents=0,
            expected_pruning_reused_lex_files=lex_files,
            expected_pruning_reused_parse_files=parse_files,
            expected_pruning_reused_module_graphs=module_graphs,
            expected_pruning_reused_module_exports=module_exports,
            expected_pruning_reused_item_lists=item_lists,
            expected_pruning_reused_item_signatures=item_signatures,
            expected_pruning_reused_function_body_syntaxes=function_body_syntaxes,
            expected_pruning_reused_type_check_bodies=type_check_bodies,
            expected_pruning_reused_generic_template_signatures=generic_template_signatures,
            expected_pruning_reused_generic_instance_signatures=0,
            expected_pruning_reused_generic_instance_bodies=generic_instance_bodies,
            expected_pruning_reused_lower_function_irs=lower_function_irs,
            expected_pruning_reused_diagnostics=diagnostics - changed_diagnostics,
            expected_pruning_recomputed_file_contents=file_contents,
            expected_pruning_recomputed_lex_files=0,
            expected_pruning_recomputed_parse_files=0,
            expected_pruning_recomputed_module_graphs=0,
            expected_pruning_recomputed_module_exports=0,
            expected_pruning_recomputed_item_lists=0,
            expected_pruning_recomputed_item_signatures=0,
            expected_pruning_recomputed_function_body_syntaxes=0,
            expected_pruning_recomputed_type_check_bodies=0,
            expected_pruning_recomputed_generic_template_signatures=0,
            expected_pruning_recomputed_generic_instance_signatures=generic_instance_signatures,
            expected_pruning_recomputed_generic_instance_bodies=0,
            expected_pruning_recomputed_lower_function_irs=0,
            expected_pruning_recomputed_diagnostics=changed_diagnostics,
            expected_provider_seeded_file_contents=0,
            expected_provider_seeded_lex_files=lex_files,
            expected_provider_seeded_parse_files=parse_files,
            expected_provider_seeded_module_graphs=module_graphs,
            expected_provider_seeded_module_exports=module_exports,
            expected_provider_seeded_item_lists=item_lists,
            expected_provider_seeded_item_signatures=item_signatures,
            expected_provider_seeded_function_body_syntaxes=function_body_syntaxes,
            expected_provider_seeded_type_check_bodies=type_check_bodies,
            expected_provider_seeded_generic_template_signatures=generic_template_signatures,
            expected_provider_seeded_generic_instance_signatures=0,
            expected_provider_seeded_generic_instance_bodies=generic_instance_bodies,
            expected_provider_seeded_lower_function_irs=lower_function_irs,
            expected_provider_seeded_diagnostics=diagnostics - changed_diagnostics,
            expected_provider_evaluated_file_contents=file_contents,
            expected_provider_evaluated_lex_files=0,
            expected_provider_evaluated_parse_files=0,
            expected_provider_evaluated_module_graphs=0,
            expected_provider_evaluated_module_exports=0,
            expected_provider_evaluated_item_lists=0,
            expected_provider_evaluated_item_signatures=0,
            expected_provider_evaluated_function_body_syntaxes=0,
            expected_provider_evaluated_type_check_bodies=0,
            expected_provider_evaluated_generic_template_signatures=0,
            expected_provider_evaluated_generic_instance_signatures=1,
            expected_provider_evaluated_generic_instance_bodies=0,
            expected_provider_evaluated_lower_function_irs=0,
            expected_provider_evaluated_diagnostics=changed_diagnostics,
        )
    raise ValueError(f"unsupported scenario: {scenario_name}")


def verify_diff_profile(fields: dict[str, str], expectation: ScenarioExpectation) -> str:
    require_exact_field(fields, "total", str(expectation.expected_diff_total))
    require_exact_field(fields, "missing", str(expectation.expected_diff_missing))
    require_exact_field(fields, "unchanged", str(expectation.expected_diff_unchanged))
    require_exact_field(fields, "changed", str(expectation.expected_diff_changed))
    require_exact_field(fields, "malformed", "0")
    return (
        f"total={expectation.expected_diff_total},missing={expectation.expected_diff_missing},"
        f"unchanged={expectation.expected_diff_unchanged},changed={expectation.expected_diff_changed},malformed=0"
    )


def verify_plan_profile(fields: dict[str, str], expectation: ScenarioExpectation) -> str:
    require_exact_field(fields, "reusable", str(expectation.expected_plan_reusable))
    require_exact_field(fields, "recompute_roots", str(expectation.expected_plan_recompute_roots))
    require_exact_field(fields, "propagated_recompute", str(expectation.expected_plan_propagated_recompute))
    require_exact_field(fields, "recompute", str(expectation.expected_plan_recompute))
    return (
        f"reusable={expectation.expected_plan_reusable},recompute_roots={expectation.expected_plan_recompute_roots},"
        f"propagated_recompute={expectation.expected_plan_propagated_recompute},recompute={expectation.expected_plan_recompute}"
    )


def verify_pruning_profile(fields: dict[str, str], expectation: ScenarioExpectation) -> str:
    require_exact_field(fields, "enabled", "1")
    require_exact_field(fields, "applied", "1")
    require_exact_field(fields, "reused", str(expectation.expected_pruning_reused))
    require_exact_field(fields, "recomputed", str(expectation.expected_pruning_recomputed))
    require_exact_field(fields, "reused_file_contents", str(expectation.expected_pruning_reused_file_contents))
    require_exact_field(fields, "reused_lex_files", str(expectation.expected_pruning_reused_lex_files))
    require_exact_field(fields, "reused_parse_files", str(expectation.expected_pruning_reused_parse_files))
    require_exact_field(fields, "reused_module_graphs", str(expectation.expected_pruning_reused_module_graphs))
    require_exact_field(fields, "reused_module_exports", str(expectation.expected_pruning_reused_module_exports))
    require_exact_field(fields, "reused_item_lists", str(expectation.expected_pruning_reused_item_lists))
    require_exact_field(fields, "reused_item_signatures", str(expectation.expected_pruning_reused_item_signatures))
    require_exact_field(
        fields,
        "reused_function_body_syntaxes",
        str(expectation.expected_pruning_reused_function_body_syntaxes),
    )
    require_exact_field(
        fields,
        "reused_type_check_bodies",
        str(expectation.expected_pruning_reused_type_check_bodies),
    )
    require_exact_field(
        fields,
        "reused_generic_template_signatures",
        str(expectation.expected_pruning_reused_generic_template_signatures),
    )
    require_exact_field(
        fields,
        "reused_generic_instance_signatures",
        str(expectation.expected_pruning_reused_generic_instance_signatures),
    )
    require_exact_field(
        fields,
        "reused_generic_instance_bodies",
        str(expectation.expected_pruning_reused_generic_instance_bodies),
    )
    require_exact_field(fields, "reused_lower_function_irs", str(expectation.expected_pruning_reused_lower_function_irs))
    require_exact_field(fields, "reused_diagnostics", str(expectation.expected_pruning_reused_diagnostics))
    require_exact_field(fields, "recomputed_file_contents", str(expectation.expected_pruning_recomputed_file_contents))
    require_exact_field(fields, "recomputed_lex_files", str(expectation.expected_pruning_recomputed_lex_files))
    require_exact_field(fields, "recomputed_parse_files", str(expectation.expected_pruning_recomputed_parse_files))
    require_exact_field(fields, "recomputed_module_graphs", str(expectation.expected_pruning_recomputed_module_graphs))
    require_exact_field(fields, "recomputed_module_exports", str(expectation.expected_pruning_recomputed_module_exports))
    require_exact_field(fields, "recomputed_item_lists", str(expectation.expected_pruning_recomputed_item_lists))
    require_exact_field(fields, "recomputed_item_signatures", str(expectation.expected_pruning_recomputed_item_signatures))
    require_exact_field(
        fields,
        "recomputed_function_body_syntaxes",
        str(expectation.expected_pruning_recomputed_function_body_syntaxes),
    )
    require_exact_field(
        fields,
        "recomputed_type_check_bodies",
        str(expectation.expected_pruning_recomputed_type_check_bodies),
    )
    require_exact_field(
        fields,
        "recomputed_generic_template_signatures",
        str(expectation.expected_pruning_recomputed_generic_template_signatures),
    )
    require_exact_field(
        fields,
        "recomputed_generic_instance_signatures",
        str(expectation.expected_pruning_recomputed_generic_instance_signatures),
    )
    require_exact_field(
        fields,
        "recomputed_generic_instance_bodies",
        str(expectation.expected_pruning_recomputed_generic_instance_bodies),
    )
    require_exact_field(
        fields,
        "recomputed_lower_function_irs",
        str(expectation.expected_pruning_recomputed_lower_function_irs),
    )
    require_exact_field(fields, "recomputed_diagnostics", str(expectation.expected_pruning_recomputed_diagnostics))
    require_exact_field(fields, "fallback", "none")
    return (
        f"enabled=1,applied=1,reused={expectation.expected_pruning_reused},"
        f"recomputed={expectation.expected_pruning_recomputed},"
        f"reused_file_contents={expectation.expected_pruning_reused_file_contents},"
        f"reused_lex_files={expectation.expected_pruning_reused_lex_files},"
        f"reused_parse_files={expectation.expected_pruning_reused_parse_files},"
        f"reused_module_graphs={expectation.expected_pruning_reused_module_graphs},"
        f"reused_module_exports={expectation.expected_pruning_reused_module_exports},"
        f"reused_item_lists={expectation.expected_pruning_reused_item_lists},"
        f"reused_item_signatures={expectation.expected_pruning_reused_item_signatures},"
        f"reused_function_body_syntaxes={expectation.expected_pruning_reused_function_body_syntaxes},"
        f"reused_type_check_bodies={expectation.expected_pruning_reused_type_check_bodies},"
        f"reused_generic_template_signatures={expectation.expected_pruning_reused_generic_template_signatures},"
        f"reused_generic_instance_signatures={expectation.expected_pruning_reused_generic_instance_signatures},"
        f"reused_generic_instance_bodies={expectation.expected_pruning_reused_generic_instance_bodies},"
        f"reused_lower_function_irs={expectation.expected_pruning_reused_lower_function_irs},"
        f"reused_diagnostics={expectation.expected_pruning_reused_diagnostics},"
        f"recomputed_file_contents={expectation.expected_pruning_recomputed_file_contents},"
        f"recomputed_lex_files={expectation.expected_pruning_recomputed_lex_files},"
        f"recomputed_parse_files={expectation.expected_pruning_recomputed_parse_files},"
        f"recomputed_module_graphs={expectation.expected_pruning_recomputed_module_graphs},"
        f"recomputed_module_exports={expectation.expected_pruning_recomputed_module_exports},"
        f"recomputed_item_lists={expectation.expected_pruning_recomputed_item_lists},"
        f"recomputed_item_signatures={expectation.expected_pruning_recomputed_item_signatures},"
        f"recomputed_function_body_syntaxes={expectation.expected_pruning_recomputed_function_body_syntaxes},"
        f"recomputed_type_check_bodies={expectation.expected_pruning_recomputed_type_check_bodies},"
        f"recomputed_generic_template_signatures={expectation.expected_pruning_recomputed_generic_template_signatures},"
        f"recomputed_generic_instance_signatures={expectation.expected_pruning_recomputed_generic_instance_signatures},"
        f"recomputed_generic_instance_bodies={expectation.expected_pruning_recomputed_generic_instance_bodies},"
        f"recomputed_lower_function_irs={expectation.expected_pruning_recomputed_lower_function_irs},"
        f"recomputed_diagnostics={expectation.expected_pruning_recomputed_diagnostics},"
        "fallback=none"
    )


def verify_provider_eval_profile(fields: dict[str, str], expectation: ScenarioExpectation) -> str:
    require_exact_field(fields, "mode", "pruned")
    require_exact_field(fields, "seeded", str(expectation.expected_pruning_reused))
    require_exact_field(fields, "evaluated", str(expectation.expected_pruning_recomputed))
    require_exact_field(fields, "seeded_file_contents", str(expectation.expected_provider_seeded_file_contents))
    require_exact_field(fields, "seeded_lex_files", str(expectation.expected_provider_seeded_lex_files))
    require_exact_field(fields, "seeded_parse_files", str(expectation.expected_provider_seeded_parse_files))
    require_exact_field(fields, "seeded_module_graphs", str(expectation.expected_provider_seeded_module_graphs))
    require_exact_field(fields, "seeded_module_exports", str(expectation.expected_provider_seeded_module_exports))
    require_exact_field(fields, "seeded_item_lists", str(expectation.expected_provider_seeded_item_lists))
    require_exact_field(fields, "seeded_item_signatures", str(expectation.expected_provider_seeded_item_signatures))
    require_exact_field(
        fields,
        "seeded_function_body_syntaxes",
        str(expectation.expected_provider_seeded_function_body_syntaxes),
    )
    require_exact_field(
        fields,
        "seeded_type_check_bodies",
        str(expectation.expected_provider_seeded_type_check_bodies),
    )
    require_exact_field(
        fields,
        "seeded_generic_template_signatures",
        str(expectation.expected_provider_seeded_generic_template_signatures),
    )
    require_exact_field(
        fields,
        "seeded_generic_instance_signatures",
        str(expectation.expected_provider_seeded_generic_instance_signatures),
    )
    require_exact_field(
        fields,
        "seeded_generic_instance_bodies",
        str(expectation.expected_provider_seeded_generic_instance_bodies),
    )
    require_exact_field(fields, "seeded_lower_function_irs", str(expectation.expected_provider_seeded_lower_function_irs))
    require_exact_field(fields, "seeded_diagnostics", str(expectation.expected_provider_seeded_diagnostics))
    require_exact_field(fields, "evaluated_file_contents", str(expectation.expected_provider_evaluated_file_contents))
    require_exact_field(fields, "evaluated_lex_files", str(expectation.expected_provider_evaluated_lex_files))
    require_exact_field(fields, "evaluated_parse_files", str(expectation.expected_provider_evaluated_parse_files))
    require_exact_field(fields, "evaluated_module_graphs", str(expectation.expected_provider_evaluated_module_graphs))
    require_exact_field(fields, "evaluated_module_exports", str(expectation.expected_provider_evaluated_module_exports))
    require_exact_field(fields, "evaluated_item_lists", str(expectation.expected_provider_evaluated_item_lists))
    require_exact_field(fields, "evaluated_item_signatures", str(expectation.expected_provider_evaluated_item_signatures))
    require_exact_field(
        fields,
        "evaluated_function_body_syntaxes",
        str(expectation.expected_provider_evaluated_function_body_syntaxes),
    )
    require_exact_field(
        fields,
        "evaluated_type_check_bodies",
        str(expectation.expected_provider_evaluated_type_check_bodies),
    )
    require_exact_field(
        fields,
        "evaluated_generic_template_signatures",
        str(expectation.expected_provider_evaluated_generic_template_signatures),
    )
    require_exact_field(
        fields,
        "evaluated_generic_instance_signatures",
        str(expectation.expected_provider_evaluated_generic_instance_signatures),
    )
    require_exact_field(
        fields,
        "evaluated_generic_instance_bodies",
        str(expectation.expected_provider_evaluated_generic_instance_bodies),
    )
    require_exact_field(
        fields,
        "evaluated_lower_function_irs",
        str(expectation.expected_provider_evaluated_lower_function_irs),
    )
    require_exact_field(fields, "evaluated_diagnostics", str(expectation.expected_provider_evaluated_diagnostics))
    return (
        "mode=pruned,seeded="
        f"{expectation.expected_pruning_reused},evaluated={expectation.expected_pruning_recomputed},"
        f"seeded_file_contents={expectation.expected_provider_seeded_file_contents},"
        f"seeded_lex_files={expectation.expected_provider_seeded_lex_files},"
        f"seeded_parse_files={expectation.expected_provider_seeded_parse_files},"
        f"seeded_module_graphs={expectation.expected_provider_seeded_module_graphs},"
        f"seeded_module_exports={expectation.expected_provider_seeded_module_exports},"
        f"seeded_item_lists={expectation.expected_provider_seeded_item_lists},"
        f"seeded_item_signatures={expectation.expected_provider_seeded_item_signatures},"
        f"seeded_function_body_syntaxes={expectation.expected_provider_seeded_function_body_syntaxes},"
        f"seeded_type_check_bodies={expectation.expected_provider_seeded_type_check_bodies},"
        f"seeded_generic_template_signatures={expectation.expected_provider_seeded_generic_template_signatures},"
        f"seeded_generic_instance_signatures={expectation.expected_provider_seeded_generic_instance_signatures},"
        f"seeded_generic_instance_bodies={expectation.expected_provider_seeded_generic_instance_bodies},"
        f"seeded_lower_function_irs={expectation.expected_provider_seeded_lower_function_irs},"
        f"seeded_diagnostics={expectation.expected_provider_seeded_diagnostics},"
        f"evaluated_file_contents={expectation.expected_provider_evaluated_file_contents},"
        f"evaluated_lex_files={expectation.expected_provider_evaluated_lex_files},"
        f"evaluated_parse_files={expectation.expected_provider_evaluated_parse_files},"
        f"evaluated_module_graphs={expectation.expected_provider_evaluated_module_graphs},"
        f"evaluated_module_exports={expectation.expected_provider_evaluated_module_exports},"
        f"evaluated_item_lists={expectation.expected_provider_evaluated_item_lists},"
        f"evaluated_item_signatures={expectation.expected_provider_evaluated_item_signatures},"
        f"evaluated_function_body_syntaxes={expectation.expected_provider_evaluated_function_body_syntaxes},"
        f"evaluated_type_check_bodies={expectation.expected_provider_evaluated_type_check_bodies},"
        f"evaluated_generic_template_signatures={expectation.expected_provider_evaluated_generic_template_signatures},"
        f"evaluated_generic_instance_signatures={expectation.expected_provider_evaluated_generic_instance_signatures},"
        f"evaluated_generic_instance_bodies={expectation.expected_provider_evaluated_generic_instance_bodies},"
        f"evaluated_lower_function_irs={expectation.expected_provider_evaluated_lower_function_irs},"
        f"evaluated_diagnostics={expectation.expected_provider_evaluated_diagnostics}"
    )


def run_scenario(function_count: int, scenario_name: str, source_variant: int) -> ScenarioResult:
    expectation = make_expectation(function_count, scenario_name)
    source_path = write_source(function_count, 0, scenario_name)
    cache_path = GENERATED_ROOT / f"query_pruning_gate_{scenario_name}_{function_count}.axic"
    profile_path = GENERATED_ROOT / f"query_pruning_gate_{scenario_name}_{function_count}.profile.json"
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
    if scenario_name == GENERIC_RECOMPUTE_SCENARIO:
        mutate_cached_query_result(cache_path)
    source_path = write_source(function_count, source_variant, scenario_name)
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

    query_diff_detail, query_diff_fields = load_profile_phase(profile_path, QUERY_DIFF_PHASE)
    query_plan_detail, query_plan_fields = load_profile_phase(profile_path, QUERY_PLAN_PHASE)
    query_pruning_detail, query_pruning_fields = load_profile_phase(profile_path, QUERY_PRUNING_PHASE)
    provider_eval_detail, provider_eval_fields = load_profile_phase(profile_path, QUERY_PROVIDER_EVAL_PHASE)

    verified_query_diff_detail = verify_diff_profile(query_diff_fields, expectation)
    verified_query_plan_detail = verify_plan_profile(query_plan_fields, expectation)
    verified_query_pruning_detail = verify_pruning_profile(query_pruning_fields, expectation)
    verified_provider_eval_detail = verify_provider_eval_profile(provider_eval_fields, expectation)

    if query_diff_detail != verified_query_diff_detail:
        raise RuntimeError(
            f"unexpected query diff detail for {scenario_name}: {query_diff_detail!r} != {verified_query_diff_detail!r}"
        )
    if query_plan_detail != verified_query_plan_detail:
        raise RuntimeError(
            f"unexpected query plan detail for {scenario_name}: {query_plan_detail!r} != {verified_query_plan_detail!r}"
        )
    if query_pruning_detail != verified_query_pruning_detail:
        raise RuntimeError(
            "unexpected query pruning detail for "
            f"{scenario_name}: {query_pruning_detail!r} != {verified_query_pruning_detail!r}"
        )
    if provider_eval_detail != verified_provider_eval_detail:
        raise RuntimeError(
            "unexpected query provider eval detail for "
            f"{scenario_name}: {provider_eval_detail!r} != {verified_provider_eval_detail!r}"
        )

    query_edge_count = validate_cache_query_edges(cache_path)
    source = source_path.read_text(encoding="utf-8")
    return ScenarioResult(
        name=scenario_name,
        source_variant=source_variant,
        function_count=function_count,
        source_bytes=len(source.encode("utf-8")),
        cache_path=str(cache_path),
        profile_path=str(profile_path),
        query_diff_detail=query_diff_detail,
        query_diff_fields=query_diff_fields,
        query_plan_detail=query_plan_detail,
        query_plan_fields=query_plan_fields,
        query_pruning_detail=query_pruning_detail,
        query_pruning_fields=query_pruning_fields,
        provider_eval_detail=provider_eval_detail,
        provider_eval_fields=provider_eval_fields,
        query_edge_count=query_edge_count,
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
    for scenario in result.scenarios:
        print(
            f"{scenario.name}: diff={scenario.query_diff_detail}; plan={scenario.query_plan_detail}; "
            f"pruning={scenario.query_pruning_detail}; provider_eval={scenario.provider_eval_detail}; "
            f"query_edges={scenario.query_edge_count}"
        )
    print(f"json: {OUTPUT_JSON}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--functions",
        type=int,
        default=int(os.environ.get("AUREX_QUERY_PRUNING_GATE_FUNCTIONS", str(DEFAULT_FUNCTION_COUNT))),
        help=f"number of helper functions in the generated module; default: {DEFAULT_FUNCTION_COUNT}",
    )
    parser.add_argument("--skip-build", action="store_true", help="reuse the existing aurexc binary")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.functions < 0:
        raise ValueError("--functions must be non-negative")
    if not args.skip_build:
        build_compiler()
    scenarios = [
        run_scenario(args.functions, ALL_REUSE_SCENARIO, 2),
        run_scenario(args.functions, BODY_RECOMPUTE_SCENARIO, 1),
        run_scenario(args.functions, GENERIC_RECOMPUTE_SCENARIO, 2),
    ]
    result = QueryPruningGateResult(function_count=args.functions, scenarios=scenarios)
    write_json(result)
    print_report(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
