#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${AUREX_COVERAGE_BUILD_DIR:-${ROOT}/build-coverage}"
COVERAGE_DIR="${BUILD_DIR}/coverage"
PROFILE_DIR="${COVERAGE_DIR}/profiles"
PROFDATA="${COVERAGE_DIR}/aurex.profdata"

DEFAULT_COVERAGE_THRESHOLD=95
LINE_THRESHOLD="${AUREX_COVERAGE_LINE_THRESHOLD:-${DEFAULT_COVERAGE_THRESHOLD}}"
FUNCTION_THRESHOLD="${AUREX_COVERAGE_FUNCTION_THRESHOLD:-${DEFAULT_COVERAGE_THRESHOLD}}"
REGION_THRESHOLD="${AUREX_COVERAGE_REGION_THRESHOLD:-${DEFAULT_COVERAGE_THRESHOLD}}"

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

cmake -S "${ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DAUREX_ENABLE_COVERAGE=ON
cmake --build "${BUILD_DIR}" -j

rm -rf "${PROFILE_DIR}"
mkdir -p "${PROFILE_DIR}"

LLVM_PROFILE_FILE="${PROFILE_DIR}/aurex-%m-%p.profraw" \
    ctest --test-dir "${BUILD_DIR}" --output-on-failure "$@"

PROFRAW_FILES=()
while IFS= read -r profraw; do
    PROFRAW_FILES+=("${profraw}")
done < <(find "${PROFILE_DIR}" -name '*.profraw' -type f | sort)
if [[ "${#PROFRAW_FILES[@]}" -eq 0 ]]; then
    printf 'coverage failed: no .profraw files were produced\n' >&2
    exit 1
fi

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

python3 - "$COVERAGE_DIR/summary.json" "$LINE_THRESHOLD" "$FUNCTION_THRESHOLD" "$REGION_THRESHOLD" <<'PY'
import json
import sys

summary_path, line_threshold, function_threshold, region_threshold = sys.argv[1:5]
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
    print(f"{key} coverage: {percent:.2f}% (threshold {threshold:.2f}%)")
    if percent + 1e-9 < threshold:
        failed.append(f"{key} {percent:.2f}% < {threshold:.2f}%")

if failed:
    print("coverage threshold failed: " + ", ".join(failed), file=sys.stderr)
    sys.exit(1)
PY

printf 'coverage report: %s\n' "${COVERAGE_DIR}/report.txt"
printf 'coverage html: %s\n' "${COVERAGE_DIR}/html/index.html"
