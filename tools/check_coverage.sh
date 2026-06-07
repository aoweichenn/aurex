#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${AUREX_COVERAGE_BUILD_DIR:-${ROOT}/build/coverage}"
COVERAGE_DIR="${BUILD_DIR}/coverage"
PROFILE_DIR="${COVERAGE_DIR}/profiles"
PROFDATA="${COVERAGE_DIR}/aurex.profdata"
ENTRYPOINT_PROFILE_DIR="${COVERAGE_DIR}/entrypoint-profiles"
ENTRYPOINT_PROFDATA="${COVERAGE_DIR}/aurexc-entrypoint.profdata"
ENTRYPOINT_SUMMARY="${COVERAGE_DIR}/entrypoint-summary.json"
ENTRYPOINT_SOURCE="${ROOT}/src/application/cli/main.cpp"

DEFAULT_COVERAGE_THRESHOLD=90
LINE_THRESHOLD="${AUREX_COVERAGE_LINE_THRESHOLD:-${DEFAULT_COVERAGE_THRESHOLD}}"
FUNCTION_THRESHOLD="${AUREX_COVERAGE_FUNCTION_THRESHOLD:-${DEFAULT_COVERAGE_THRESHOLD}}"
REGION_THRESHOLD="${AUREX_COVERAGE_REGION_THRESHOLD:-${DEFAULT_COVERAGE_THRESHOLD}}"
ENTRYPOINT_LINE_THRESHOLD="${AUREX_COVERAGE_ENTRYPOINT_LINE_THRESHOLD:-${LINE_THRESHOLD}}"
ENTRYPOINT_FUNCTION_THRESHOLD="${AUREX_COVERAGE_ENTRYPOINT_FUNCTION_THRESHOLD:-${FUNCTION_THRESHOLD}}"
ENTRYPOINT_REGION_THRESHOLD="${AUREX_COVERAGE_ENTRYPOINT_REGION_THRESHOLD:-${REGION_THRESHOLD}}"
DEFAULT_FOCUSED_SOURCES="src/frontend/parse/grammar/parser_postfix.cpp:src/frontend/sema/facade/sema_expr.cpp:src/frontend/sema/facade/sema_types.cpp:src/frontend/sema/internal/expressions/sources/sema_builtin_expression_analyzer.cpp:src/frontend/sema/internal/expressions/sources/sema_control_expression_analyzer.cpp:src/frontend/sema/internal/expressions/sources/sema_expression_analyzer.cpp:src/frontend/sema/internal/declarations/sources/sema_generic_analyzer.cpp:src/frontend/sema/internal/lookup/sources/sema_lookup_indexer.cpp:src/frontend/sema/internal/lookup/sources/sema_lookup_resolver.cpp:src/frontend/sema/internal/expressions/sources/sema_operator_expression_analyzer.cpp:src/frontend/sema/internal/patterns/sources/sema_pattern_match_analyzer.cpp:src/frontend/sema/internal/expressions/sources/sema_projection_aggregate_expression_analyzer.cpp:src/frontend/sema/internal/services/sources/sema_type_services.cpp"
FOCUSED_SOURCES="${AUREX_COVERAGE_FOCUSED_SOURCES:-${DEFAULT_FOCUSED_SOURCES}}"
FOCUSED_LINE_THRESHOLD="${AUREX_COVERAGE_FOCUSED_LINE_THRESHOLD:-${LINE_THRESHOLD}}"
FOCUSED_FUNCTION_THRESHOLD="${AUREX_COVERAGE_FOCUSED_FUNCTION_THRESHOLD:-${FUNCTION_THRESHOLD}}"
FOCUSED_REGION_THRESHOLD="${AUREX_COVERAGE_FOCUSED_REGION_THRESHOLD:-${REGION_THRESHOLD}}"
COVERAGE_BUILD_JOBS="${AUREX_COVERAGE_BUILD_JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-2}}"

if ! [[ "${COVERAGE_BUILD_JOBS}" =~ ^[1-9][0-9]*$ ]]; then
    printf 'invalid coverage build job count: %s\n' "${COVERAGE_BUILD_JOBS}" >&2
    exit 1
fi

find_llvm_tool() {
    local name="$1"
    if command -v "${name}" >/dev/null 2>&1; then
        command -v "${name}"
        return
    fi
    if [[ -x "/opt/homebrew/opt/llvm/bin/${name}" ]]; then
        printf '%s\n' "/opt/homebrew/opt/llvm/bin/${name}"
        return
    fi
    if [[ -x "/usr/local/opt/llvm/bin/${name}" ]]; then
        printf '%s\n' "/usr/local/opt/llvm/bin/${name}"
        return
    fi
    printf 'missing required tool: %s\n' "${name}" >&2
    return 1
}

LLVM_COV="${LLVM_COV:-$(find_llvm_tool llvm-cov)}"
LLVM_PROFDATA="${LLVM_PROFDATA:-$(find_llvm_tool llvm-profdata)}"
LLVM_BIN_DIR="$(cd "$(dirname "${LLVM_COV}")" && pwd)"
CC="${CC:-${LLVM_BIN_DIR}/clang}"
CXX="${CXX:-${LLVM_BIN_DIR}/clang++}"

if [[ ! -x "${CC}" || ! -x "${CXX}" ]]; then
    printf 'missing clang/clang++ next to llvm-cov; set CC and CXX explicitly\n' >&2
    exit 1
fi

collect_profraw_files() {
    local directory="$1"
    local label="$2"
    PROFRAW_FILES=()
    while IFS= read -r profraw; do
        PROFRAW_FILES+=("${profraw}")
    done < <(find "${directory}" -name '*.profraw' -type f | sort)
    if [[ "${#PROFRAW_FILES[@]}" -eq 0 ]]; then
        printf 'coverage failed: no .profraw files were produced for %s\n' "${label}" >&2
        exit 1
    fi
}

cmake -S "${ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DAUREX_ENABLE_COVERAGE=ON
cmake --build "${BUILD_DIR}" --parallel "${COVERAGE_BUILD_JOBS}"

rm -rf "${PROFILE_DIR}" "${ENTRYPOINT_PROFILE_DIR}" "${COVERAGE_DIR}/html" "${COVERAGE_DIR}/entrypoint-html"
mkdir -p "${PROFILE_DIR}" "${ENTRYPOINT_PROFILE_DIR}"

LLVM_PROFILE_FILE="${PROFILE_DIR}/aurex-%m-%p.profraw" \
    ctest --test-dir "${BUILD_DIR}" --output-on-failure "$@"

collect_profraw_files "${PROFILE_DIR}" "test suite"
"${LLVM_PROFDATA}" merge -sparse "${PROFRAW_FILES[@]}" -o "${PROFDATA}"

COVERAGE_OBJECT="${BUILD_DIR}/bin/aurex_tests"
if [[ ! -x "${COVERAGE_OBJECT}" ]]; then
    printf 'coverage failed: missing coverage object: %s\n' "${COVERAGE_OBJECT}" >&2
    exit 1
fi
OBJECT_ARGS=(--object "${COVERAGE_OBJECT}")
IGNORE_REGEX='(/tests/|/build[^/]*/|/opt/|/usr/|gtest|gmock|googletest)'
SOURCE_FILES=()
while IFS= read -r source_file; do
    SOURCE_FILES+=("${source_file}")
done < <(find "${ROOT}/src" -type f -name '*.cpp' | sort)

if [[ "${#SOURCE_FILES[@]}" -eq 0 ]]; then
    printf 'coverage failed: no source files were found\n' >&2
    exit 1
fi
SOURCE_ARGS=()
for source_file in "${SOURCE_FILES[@]}"; do
    if [[ "${source_file}" == "${ENTRYPOINT_SOURCE}" ]]; then
        continue
    fi
    SOURCE_ARGS+=(--sources "${source_file}")
done

"${LLVM_COV}" report "${OBJECT_ARGS[@]}" \
    --instr-profile="${PROFDATA}" \
    --ignore-filename-regex="${IGNORE_REGEX}" \
    "${SOURCE_ARGS[@]}" \
    | tee "${COVERAGE_DIR}/report.txt"

"${LLVM_COV}" export "${OBJECT_ARGS[@]}" \
    --instr-profile="${PROFDATA}" \
    --ignore-filename-regex="${IGNORE_REGEX}" \
    --summary-only \
    --format=text \
    "${SOURCE_ARGS[@]}" \
    >"${COVERAGE_DIR}/summary.json"

"${LLVM_COV}" show "${OBJECT_ARGS[@]}" \
    --instr-profile="${PROFDATA}" \
    --ignore-filename-regex="${IGNORE_REGEX}" \
    --format=html \
    --output-dir="${COVERAGE_DIR}/html" \
    "${SOURCE_ARGS[@]}" \
    >/dev/null

ENTRYPOINT_OBJECT="${BUILD_DIR}/bin/aurexc"
if [[ ! -x "${ENTRYPOINT_OBJECT}" ]]; then
    printf 'coverage failed: missing entrypoint coverage object: %s\n' "${ENTRYPOINT_OBJECT}" >&2
    exit 1
fi
LLVM_PROFILE_FILE="${ENTRYPOINT_PROFILE_DIR}/aurexc-%m-%p.profraw" \
    "${ENTRYPOINT_OBJECT}" --help >/dev/null
collect_profraw_files "${ENTRYPOINT_PROFILE_DIR}" "aurexc entrypoint"
"${LLVM_PROFDATA}" merge -sparse "${PROFRAW_FILES[@]}" -o "${ENTRYPOINT_PROFDATA}"

ENTRYPOINT_OBJECT_ARGS=(--object "${ENTRYPOINT_OBJECT}")
ENTRYPOINT_SOURCE_ARGS=(--sources "${ENTRYPOINT_SOURCE}")
"${LLVM_COV}" report "${ENTRYPOINT_OBJECT_ARGS[@]}" \
    --instr-profile="${ENTRYPOINT_PROFDATA}" \
    --ignore-filename-regex="${IGNORE_REGEX}" \
    "${ENTRYPOINT_SOURCE_ARGS[@]}" \
    | tee "${COVERAGE_DIR}/entrypoint-report.txt"

"${LLVM_COV}" export "${ENTRYPOINT_OBJECT_ARGS[@]}" \
    --instr-profile="${ENTRYPOINT_PROFDATA}" \
    --ignore-filename-regex="${IGNORE_REGEX}" \
    --summary-only \
    --format=text \
    "${ENTRYPOINT_SOURCE_ARGS[@]}" \
    >"${ENTRYPOINT_SUMMARY}"

"${LLVM_COV}" show "${ENTRYPOINT_OBJECT_ARGS[@]}" \
    --instr-profile="${ENTRYPOINT_PROFDATA}" \
    --ignore-filename-regex="${IGNORE_REGEX}" \
    --format=html \
    --output-dir="${COVERAGE_DIR}/entrypoint-html" \
    "${ENTRYPOINT_SOURCE_ARGS[@]}" \
    >/dev/null

python3 - "$COVERAGE_DIR/summary.json" "$LINE_THRESHOLD" "$FUNCTION_THRESHOLD" "$REGION_THRESHOLD" "source totals" <<'PY'
import json
import sys

summary_path, line_threshold, function_threshold, region_threshold, label = sys.argv[1:6]
thresholds = {
    "lines": float(line_threshold),
    "functions": float(function_threshold),
    "regions": float(region_threshold),
}

with open(summary_path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

totals = data["data"][0]["totals"]
failed = []
for key, threshold in thresholds.items():
    percent = float(totals[key]["percent"])
    print(f"{label} {key} coverage: {percent:.2f}% (threshold {threshold:.2f}%)")
    if percent + 1e-9 < threshold:
        failed.append(f"{key} {percent:.2f}% < {threshold:.2f}%")

if failed:
    print(f"{label} coverage threshold failed: " + ", ".join(failed), file=sys.stderr)
    sys.exit(1)
PY

python3 - "$ENTRYPOINT_SUMMARY" "$ENTRYPOINT_LINE_THRESHOLD" "$ENTRYPOINT_FUNCTION_THRESHOLD" "$ENTRYPOINT_REGION_THRESHOLD" "aurexc entrypoint" <<'PY'
import json
import sys

summary_path, line_threshold, function_threshold, region_threshold, label = sys.argv[1:6]
thresholds = {
    "lines": float(line_threshold),
    "functions": float(function_threshold),
    "regions": float(region_threshold),
}

with open(summary_path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

totals = data["data"][0]["totals"]
failed = []
for key, threshold in thresholds.items():
    percent = float(totals[key]["percent"])
    print(f"{label} {key} coverage: {percent:.2f}% (threshold {threshold:.2f}%)")
    if percent + 1e-9 < threshold:
        failed.append(f"{key} {percent:.2f}% < {threshold:.2f}%")

if failed:
    print(f"{label} coverage threshold failed: " + ", ".join(failed), file=sys.stderr)
    sys.exit(1)
PY

if [[ -n "${FOCUSED_SOURCES}" ]]; then
    python3 - "$COVERAGE_DIR/summary.json" "$FOCUSED_LINE_THRESHOLD" "$FOCUSED_FUNCTION_THRESHOLD" "$FOCUSED_REGION_THRESHOLD" "$FOCUSED_SOURCES" "$ROOT" <<'PY'
import json
import os
import sys

summary_path, line_threshold, function_threshold, region_threshold, source_list, root = sys.argv[1:7]
thresholds = {
    "lines": float(line_threshold),
    "functions": float(function_threshold),
    "regions": float(region_threshold),
}
root = os.path.realpath(root)
focused_sources = [source for source in source_list.split(":") if source]

with open(summary_path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

files = {
    os.path.relpath(os.path.realpath(entry["filename"]), root): entry
    for entry in data["data"][0].get("files", [])
}
failed = []
for source in focused_sources:
    entry = files.get(source)
    if entry is None:
        failed.append(f"{source} missing from coverage summary")
        continue
    summary = entry["summary"]
    for key, threshold in thresholds.items():
        percent = float(summary[key]["percent"])
        print(f"focused {source} {key} coverage: {percent:.2f}% (threshold {threshold:.2f}%)")
        if percent + 1e-9 < threshold:
            failed.append(f"{source} {key} {percent:.2f}% < {threshold:.2f}%")

if failed:
    print("focused source coverage threshold failed: " + ", ".join(failed), file=sys.stderr)
    sys.exit(1)
PY
fi

printf 'coverage report: %s\n' "${COVERAGE_DIR}/report.txt"
printf 'coverage html: %s\n' "${COVERAGE_DIR}/html/index.html"
printf 'entrypoint coverage report: %s\n' "${COVERAGE_DIR}/entrypoint-report.txt"
printf 'entrypoint coverage html: %s\n' "${COVERAGE_DIR}/entrypoint-html/index.html"
