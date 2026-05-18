#!/usr/bin/env python3
"""Run clang-format on changed C++ lines without forcing a whole-tree reformat."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


CPP_SUFFIXES = {".cc", ".cpp", ".cxx", ".h", ".hpp"}
DEFAULT_CI_BASE = "origin/main"


def repo_root() -> Path:
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return Path(result.stdout.strip())


def changed_cpp_files(root: Path, base: str) -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACMRT", base, "--"],
        check=True,
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    files: list[str] = []
    for line in result.stdout.splitlines():
        path = root / line
        if path.suffix in CPP_SUFFIXES and path.exists():
            files.append(line)
    return files


def explicit_cpp_files(root: Path, values: list[str]) -> list[str]:
    files: list[str] = []
    for value in values:
        path = Path(value)
        if not path.is_absolute():
            path = root / path
        if path.suffix in CPP_SUFFIXES and path.exists():
            files.append(str(path.relative_to(root)))
    return files


def clang_format_diff(root: Path, base: str, files: list[str]) -> str:
    command = ["git", "clang-format", "--diff", base, "--", *files]
    result = subprocess.run(
        command,
        cwd=root,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    if result.returncode not in (0, 1):
        raise subprocess.CalledProcessError(result.returncode, command, result.stdout, result.stderr)
    if result.stdout.strip() == "clang-format did not modify any files":
        return ""
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description="Check clang-format on changed C++ lines.")
    parser.add_argument(
        "--base",
        default=DEFAULT_CI_BASE,
        help=f"Git revision to diff against; defaults to {DEFAULT_CI_BASE!r}.",
    )
    parser.add_argument("files", nargs="*", help="Optional explicit files to check.")
    args = parser.parse_args()

    root = repo_root()
    base = args.base
    files = explicit_cpp_files(root, args.files) if args.files else changed_cpp_files(root, base)
    if not files:
        print("format_check: no C++ files to check")
        return 0

    diff = clang_format_diff(root, base, files)
    if diff.strip():
        print(diff, end="")
        print("format_check: clang-format would change modified C++ lines", file=sys.stderr)
        return 1
    print(f"format_check: checked {len(files)} C++ file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
