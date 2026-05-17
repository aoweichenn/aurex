#!/usr/bin/env python3
"""Generate and run regex differential/conformance tests.

The generated Aurex program covers:
- a Python `re` differential corpus restricted to regular, linear-time syntax;
- Unicode 17 CaseFolding.txt full case-fold mappings;
- Unicode 17 GraphemeBreakTest.txt extended grapheme cluster boundaries.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
CASE_FOLDING = ROOT / "tests/data/unicode/17.0.0/CaseFolding.txt"
GRAPHEME_BREAK_TEST = ROOT / "tests/data/unicode/17.0.0/GraphemeBreakTest.txt"
GENERATED = ROOT / "build/regex_differential_generated.ax"
OUTPUT = ROOT / "build/tests/regex_differential"
GRAPHEME_CHUNK_SIZE = 80
CASE_FOLD_CHUNK_SIZE = 48


def aurex_string(value: str) -> str:
    out: list[str] = ['"']
    for ch in value:
        code = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif code == 0:
            out.append("\\0")
        elif 0x20 <= code <= 0x7E:
            out.append(ch)
        else:
            out.append(f"\\u{{{code:X}}}")
    out.append('"')
    return "".join(out)


def parse_case_folding(path: pathlib.Path, start: int, end: int) -> list[tuple[int, list[int]]]:
    cases: list[tuple[int, list[int]]] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        parts = [part.strip() for part in line.split(";")]
        if len(parts) < 3 or parts[1] != "F":
            continue
        source = int(parts[0], 16)
        mapping = [int(item, 16) for item in parts[2].split()]
        cases.append((source, mapping))
    return cases[start:end if end >= 0 else None]


def parse_grapheme_break_test(path: pathlib.Path, start: int, end: int) -> list[tuple[str, list[int]]]:
    cases: list[tuple[str, list[int]]] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        tokens = line.split()
        codepoints: list[int] = []
        boundary_positions: list[int] = []
        for token in tokens:
            if token == "÷":
                boundary_positions.append(len(codepoints))
            elif token == "×":
                continue
            else:
                codepoints.append(int(token, 16))
        if not codepoints:
            continue
        text = "".join(chr(value) for value in codepoints)
        boundaries: list[int] = []
        for position in boundary_positions:
            if 0 < position <= len(codepoints):
                prefix = "".join(chr(value) for value in codepoints[:position])
                boundaries.append(len(prefix.encode("utf-8")))
        if boundaries:
            cases.append((text, boundaries))
    return cases[start:end if end >= 0 else None]


def python_differential_cases() -> list[tuple[str, str, bool, int, int, bool]]:
    seeds = [
        (r"abc", "xxabczz"),
        (r"a+", "baaac"),
        (r"colou?r", "color colour"),
        (r"(cat|dog)s?", "xxdogs"),
        (r"[a-z]{2,4}", "12abcd34"),
        (r"\d{2,4}", "id=2026;"),
        (r"^warn$", "warn"),
        (r"^warn$", "info\nwarn"),
        (r"a.*?b", "za12b34b"),
        (r"[A-F0-9]+", "id BEEF done"),
        (r"(?:ab|a)b", "zab"),
        (r"[^0-9]+", "abc_"),
    ]
    cases: list[tuple[str, str, bool, int, int, bool]] = []
    for pattern, text in seeds:
        search = re.search(pattern, text)
        full = re.fullmatch(pattern, text)
        cases.append((
            pattern,
            text,
            search is not None,
            -1 if search is None else search.start(),
            -1 if search is None else search.end(),
            full is not None,
        ))
    return cases


def emit_python_differential(lines: list[str]) -> None:
    lines.append("fn require_python_differential() -> i32 {")
    for index, (pattern, text, matched, start, end, full) in enumerate(python_differential_cases()):
        code = 1000 + index * 10
        lines.append(f"    var re_{index}: regex.Regex = regex.compile({aurex_string(pattern)});")
        lines.append(f"    defer regex.destroy(&mut re_{index});")
        lines.append(f"    let found_{index}: regex.MatchResult = regex.search_compiled(&re_{index}, {aurex_string(text)});")
        if matched:
            lines.append(
                f"    if !found_{index}.ok() || found_{index}.start != {start}usize || found_{index}.end != {end}usize {{"
            )
            lines.append(f"        return {code} + regex.status_code(found_{index}.status);")
            lines.append("    }")
        else:
            lines.append(f"    if found_{index}.status != regex.RegexStatus.no_match {{")
            lines.append(f"        return {code} + regex.status_code(found_{index}.status);")
            lines.append("    }")
        lines.append(f"    let full_{index}: regex.MatchResult = regex.fullmatch_compiled(&re_{index}, {aurex_string(text)});")
        if full:
            lines.append(f"    if !full_{index}.ok() {{")
            lines.append(f"        return {code + 1} + regex.status_code(full_{index}.status);")
            lines.append("    }")
        else:
            lines.append(f"    if full_{index}.status != regex.RegexStatus.no_match {{")
            lines.append(f"        return {code + 1} + regex.status_code(full_{index}.status);")
            lines.append("    }")
    lines.append("    return 0;")
    lines.append("}")
    lines.append("")


def emit_case_folding(lines: list[str], cases: list[tuple[int, list[int]]]) -> None:
    chunk_count = (len(cases) + CASE_FOLD_CHUNK_SIZE - 1) // CASE_FOLD_CHUNK_SIZE
    for chunk in range(chunk_count):
        start = chunk * CASE_FOLD_CHUNK_SIZE
        end = min(len(cases), start + CASE_FOLD_CHUNK_SIZE)
        lines.append(f"fn require_case_folding_{chunk}() -> i32 {{")
        for index, (source, mapping) in enumerate(cases[start:end], start):
            source_text = chr(source)
            mapped_text = "".join(chr(value) for value in mapping)
            code = 2000 + index * 2
            lines.append(f"    var fold_{index}: regex.Regex = regex.compile({aurex_string('(?i)' + source_text)});")
            lines.append(f"    defer regex.destroy(&mut fold_{index});")
            lines.append(f"    let forward_{index}: regex.MatchResult = regex.fullmatch_compiled(&fold_{index}, {aurex_string(mapped_text)});")
            lines.append(f"    if !forward_{index}.ok() {{")
            lines.append(f"        return {code} + regex.status_code(forward_{index}.status);")
            lines.append("    }")
            if len(mapping) > 1:
                lines.append(f"    var reverse_{index}: regex.Regex = regex.compile({aurex_string('(?i)' + mapped_text)});")
                lines.append(f"    defer regex.destroy(&mut reverse_{index});")
                lines.append(f"    let reverse_match_{index}: regex.MatchResult = regex.fullmatch_compiled(&reverse_{index}, {aurex_string(source_text)});")
                lines.append(f"    if !reverse_match_{index}.ok() {{")
                lines.append(f"        return {code + 1} + regex.status_code(reverse_match_{index}.status);")
                lines.append("    }")
        lines.append("    return 0;")
        lines.append("}")
        lines.append("")
    lines.append("fn require_case_folding() -> i32 {")
    for chunk in range(chunk_count):
        lines.append(f"    let result_{chunk}: i32 = require_case_folding_{chunk}();")
        lines.append(f"    if result_{chunk} != 0 {{")
        lines.append(f"        return result_{chunk};")
        lines.append("    }")
    lines.append("    return 0;")
    lines.append("}")
    lines.append("")


def emit_grapheme(lines: list[str], cases: list[tuple[str, list[int]]]) -> None:
    chunk_count = (len(cases) + GRAPHEME_CHUNK_SIZE - 1) // GRAPHEME_CHUNK_SIZE
    for chunk in range(chunk_count):
        start = chunk * GRAPHEME_CHUNK_SIZE
        end = min(len(cases), start + GRAPHEME_CHUNK_SIZE)
        lines.append(f"fn require_grapheme_{chunk}(cluster: &regex.Regex) -> i32 {{")
        for index, (text, boundaries) in enumerate(cases[start:end], start):
            code = 5000 + index
            lines.append(f"    var cursor_{index}: usize = 0usize;")
            for boundary_index, boundary in enumerate(boundaries):
                lines.append(
                    f"    let g_{index}_{boundary_index}: regex.MatchResult = regex.search_compiled_from(cluster, {aurex_string(text)}, cursor_{index});"
                )
                lines.append(
                    f"    if !g_{index}_{boundary_index}.ok() || g_{index}_{boundary_index}.start != cursor_{index} || "
                    f"g_{index}_{boundary_index}.end != {boundary}usize {{"
                )
                lines.append(f"        return {code} + regex.status_code(g_{index}_{boundary_index}.status);")
                lines.append("    }")
                lines.append(f"    cursor_{index} = g_{index}_{boundary_index}.end;")
            lines.append(f"    if cursor_{index} != strblen({aurex_string(text)}) {{")
            lines.append(f"        return {code} + 900;")
            lines.append("    }")
        lines.append("    return 0;")
        lines.append("}")
        lines.append("")
    lines.append("fn require_grapheme_breaks() -> i32 {")
    lines.append('    var cluster: regex.Regex = regex.compile("\\\\X");')
    lines.append("    defer regex.destroy(&mut cluster);")
    lines.append("    if !cluster.valid() {")
    lines.append("        return 4900 + regex.status_code(cluster.status);")
    lines.append("    }")
    for chunk in range(chunk_count):
        lines.append(f"    let result_{chunk}: i32 = require_grapheme_{chunk}(&cluster);")
        lines.append(f"    if result_{chunk} != 0 {{")
        lines.append(f"        return result_{chunk};")
        lines.append("    }")
    lines.append("    return 0;")
    lines.append("}")
    lines.append("")


def generate_source(fold_start: int, fold_end: int, grapheme_start: int, grapheme_end: int) -> str:
    folds = parse_case_folding(CASE_FOLDING, fold_start, fold_end)
    graphemes = parse_grapheme_break_test(GRAPHEME_BREAK_TEST, grapheme_start, grapheme_end)
    lines: list[str] = [
        "module regex_differential_generated;",
        "",
        "import regex.api as regex;",
        "",
    ]
    emit_python_differential(lines)
    emit_case_folding(lines, folds)
    emit_grapheme(lines, graphemes)
    lines.extend(
        [
            "fn main() -> i32 {",
            "    let diff: i32 = require_python_differential();",
            "    if diff != 0 {",
            "        return diff;",
            "    }",
            "    let fold: i32 = require_case_folding();",
            "    if fold != 0 {",
            "        return fold;",
            "    }",
            "    return require_grapheme_breaks();",
            "}",
            "",
        ]
    )
    return "\n".join(lines)


def run_command(args: list[str]) -> None:
    subprocess.run(args, cwd=ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--generate-only", action="store_true")
    parser.add_argument("--fold-start", type=int, default=0)
    parser.add_argument("--fold-end", type=int, default=-1)
    parser.add_argument("--grapheme-start", type=int, default=0)
    parser.add_argument("--grapheme-end", type=int, default=-1)
    args = parser.parse_args()

    GENERATED.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    GENERATED.write_text(
        generate_source(args.fold_start, args.fold_end, args.grapheme_start, args.grapheme_end),
        encoding="utf-8",
    )
    print(f"generated {GENERATED}")
    if args.generate_only:
        return 0
    run_command(["build/bin/aurexc", "-I", "examples/libs", "--check", str(GENERATED)])
    run_command(["build/bin/aurexc", "-I", "examples/libs", str(GENERATED), "-o", str(OUTPUT)])
    run_command([str(OUTPUT)])
    return 0


if __name__ == "__main__":
    sys.exit(main())
