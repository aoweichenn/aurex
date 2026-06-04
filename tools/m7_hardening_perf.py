#!/usr/bin/env python3
"""M7 hardening performance harness.

The script keeps the M7 closure measurements reproducible without hiding the
underlying tools. It builds the frontend Google Benchmark binary, runs the
full frontend benchmark matrix, runs a hyperfine query-test timing, and records
one /usr/bin/time -v focused semantic regression pass.
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_TEST_BUILD = ROOT / "build" / "full-llvm-fedora"
DEFAULT_PERF_BUILD = ROOT / "build" / "perf"
DEFAULT_OUTPUT_DIR = ROOT / "build" / "m7_hardening_perf"
DEFAULT_BENCHMARK_MIN_TIME = "0.2s"
DEFAULT_HYPERFINE_RUNS = 3

FRONTEND_BENCH_TARGET = "aurex_frontend_bench"
TEST_TARGET = "aurex_tests"
QUERY_UNIT_FILTER = "QueryUnit.*"
FOCUSED_SEMA_FILTER = (
    "CoreUnit.SemanticWhiteBoxStatementControlFlowQueries:"
    "CoreUnit.SemanticWhiteBoxParserOnlyModuleContractIsNormalized:"
    "CoreUnit.*BodyLoan*:"
    "CoreUnit.*LifetimeFacts*"
)


def run_capture(cmd: list[str], cwd: pathlib.Path = ROOT) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


def require_success(completed: subprocess.CompletedProcess[str], cmd: list[str]) -> str:
    if completed.returncode != 0:
        raise RuntimeError(
            "command failed: "
            + " ".join(cmd)
            + "\nstdout:\n"
            + completed.stdout
            + "\nstderr:\n"
            + completed.stderr
        )
    return completed.stdout + completed.stderr


def run_checked(cmd: list[str], cwd: pathlib.Path = ROOT) -> str:
    return require_success(run_capture(cmd, cwd), cmd)


def configure_perf_build(perf_build: pathlib.Path) -> None:
    run_checked([
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(perf_build),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DAUREX_BUILD_BENCHMARKS=ON",
    ])


def build_targets(test_build: pathlib.Path, perf_build: pathlib.Path) -> None:
    configure_perf_build(perf_build)
    run_checked(["cmake", "--build", str(perf_build), "--target", FRONTEND_BENCH_TARGET, "-j"])
    run_checked(["cmake", "--build", str(test_build), "--target", TEST_TARGET, "-j"])


def tool_version(name: str, args: list[str]) -> str:
    if shutil.which(name) is None:
        return f"{name}: not found"
    completed = run_capture([name, *args])
    text = (completed.stdout + completed.stderr).strip()
    return f"{name}: {text if text else 'available'}"


def perf_status() -> str:
    if shutil.which("perf") is None:
        return "perf: not found"
    completed = run_capture(["perf", "stat", "true"])
    if completed.returncode == 0:
        return "perf stat: available"
    first_line = (completed.stderr or completed.stdout).strip().splitlines()
    detail = first_line[0] if first_line else "failed without diagnostic"
    return f"perf stat: unavailable ({detail})"


def run_frontend_benchmark(perf_build: pathlib.Path, output_dir: pathlib.Path, min_time: str) -> str:
    bench = perf_build / "bin" / FRONTEND_BENCH_TARGET
    benchmark_json = output_dir / "frontend_bench.json"
    cmd = [
        str(bench),
        "--benchmark_color=false",
        f"--benchmark_min_time={min_time}",
        f"--benchmark_out={benchmark_json}",
        "--benchmark_out_format=json",
    ]
    return run_checked(cmd)


def run_hyperfine(test_build: pathlib.Path, output_dir: pathlib.Path, runs: int) -> str:
    if shutil.which("hyperfine") is None:
        return "hyperfine: skipped (not installed)\n"
    test_binary = test_build / "bin" / TEST_TARGET
    output_json = output_dir / "query_unit_hyperfine.json"
    command = f"{test_binary} --gtest_brief=1 --gtest_filter={QUERY_UNIT_FILTER}"
    return run_checked([
        "hyperfine",
        "--warmup",
        "1",
        "--runs",
        str(runs),
        "--export-json",
        str(output_json),
        command,
    ])


def run_time_verbose(test_build: pathlib.Path) -> str:
    time_tool = pathlib.Path("/usr/bin/time")
    if not time_tool.exists():
        return "/usr/bin/time: skipped (not installed)\n"
    test_binary = test_build / "bin" / TEST_TARGET
    return run_checked([
        str(time_tool),
        "-v",
        str(test_binary),
        "--gtest_brief=1",
        f"--gtest_filter={FOCUSED_SEMA_FILTER}",
    ])


def write_summary(output_dir: pathlib.Path, frontend: str, hyperfine: str, time_verbose: str) -> None:
    summary = output_dir / "summary.md"
    generated_at = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    summary.write_text(
        "\n".join([
            "# M7 Hardening Performance Run",
            "",
            f"- generated_at_utc: `{generated_at}`",
            f"- {tool_version('hyperfine', ['--version'])}",
            f"- {tool_version('perf', ['--version'])}",
            f"- {perf_status()}",
            "",
            "## Frontend Google Benchmark",
            "",
            "```text",
            frontend.strip(),
            "```",
            "",
            "## Hyperfine QueryUnit",
            "",
            "```text",
            hyperfine.strip(),
            "```",
            "",
            "## /usr/bin/time -v Focused Sema",
            "",
            "```text",
            time_verbose.strip(),
            "```",
            "",
        ]),
        encoding="utf-8",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-build", type=pathlib.Path, default=DEFAULT_TEST_BUILD)
    parser.add_argument("--perf-build", type=pathlib.Path, default=DEFAULT_PERF_BUILD)
    parser.add_argument("--output-dir", type=pathlib.Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--benchmark-min-time", default=DEFAULT_BENCHMARK_MIN_TIME)
    parser.add_argument("--hyperfine-runs", type=int, default=DEFAULT_HYPERFINE_RUNS)
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        build_targets(args.test_build.resolve(), args.perf_build.resolve())
    frontend = run_frontend_benchmark(args.perf_build.resolve(), output_dir, args.benchmark_min_time)
    hyperfine = run_hyperfine(args.test_build.resolve(), output_dir, args.hyperfine_runs)
    time_verbose = run_time_verbose(args.test_build.resolve())
    write_summary(output_dir, frontend, hyperfine, time_verbose)
    print(f"wrote {output_dir / 'summary.md'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
