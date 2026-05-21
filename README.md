# Aurex M2

Documentation baseline: **M2 language-core-no-std**.

Aurex is a small systems-language compiler project written in modern C++20. The
lexer and parser are handwritten. The AST uses compact IDs and vector storage.
Stage0 now compiles through Aurex IR -> LLVM IR -> clang by default.

M2 is a deliberate reset after the M1 design track. The M1 standard-library,
selfhost, and system/build-tool experiments are no longer active in this tree.
M2 focuses on stabilizing the language core first: syntax, type checking,
generics, pattern matching, `unsafe` boundaries, Aurex IR, and LLVM/native
output.

## Quick Start

Minimal frontend configure/build, which does not require LLVM or GoogleTest:

```sh
cmake --preset frontend-minimal
cmake --build --preset frontend-minimal -j
```

To build and run the full native compiler path, use the LLVM preset:

```sh
cmake --preset full-llvm
cmake --build build/full-llvm -j
build/full-llvm/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Expected output:

```text
hello from Aurex M2
```

Stage0 now resolves imports:

```sh
build/full-llvm/bin/aurexc -I tests/samples/imports tests/samples/positive/modules/import_path.ax -o build/tests/import_path
```

Reusable import examples live under `examples/libs`:

```sh
build/full-llvm/bin/aurexc -I examples/libs --emit=checked tests/samples/positive/modules/import_path.ax
```

Stage0 can also produce assembly and object files through clang:

```sh
build/full-llvm/bin/aurexc -S examples/hello.ax -o build/tests/hello.s
build/full-llvm/bin/aurexc -c examples/hello.ax -o build/tests/hello.o
build/full-llvm/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/full-llvm/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/full-llvm/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` is the default. `-fsyntax-only` is accepted as an alias for
`--check`; `-S` and `-c` follow the usual compiler-driver spelling for assembly
and object output. CLI help is grouped into primary action options and secondary
modifier options, while the command syntax remains clang-style flat flags.
`--clang <path>` or `--clang=<path>` selects a clang binary for native output,
`--opt-level O1`, `--opt-level=O1`, or `-O1` enables Aurex IR passes, and
repeated `--clang-arg <arg>` / `--clang-arg=<arg>` options pass raw arguments
such as `-O2` or `-g` to clang. Native backend options are rejected for
non-native emit modes such as `--emit=ir`.

`--incremental-cache <path>` enables the driver cache path. Successful semantic
analysis writes source content fingerprints, import paths, loaded modules, and
checked stable/incremental definition keys to the cache file; later `--check`
invocations reuse it only when the root file, import-path list, and every
recorded dependency fingerprint still match.

The IR path is visible with:

```sh
build/full-llvm/bin/aurexc --emit=ir examples/hello.ax
build/full-llvm/bin/aurexc --emit=llvm-ir examples/hello.ax
```

`--emit=ir` prints Aurex's typed CFG/SSA IR. `--emit=llvm-ir` lowers that IR to
LLVM IR and prints the result. Native output runs Aurex IR -> LLVM IR -> clang.
The old Stage0 C backend has been removed from the production build; the
historical standalone C translator and the old bootstrap tree have also been
removed from the active tree.

This branch intentionally freezes and removes the Aurex standard library.
There is no bundled `std/` tree, no implicit standard-library import path, no
`--stdlib` / `--no-stdlib` / `--std-backend` CLI surface, and no automatic
host-C support linking. Native output contains only the user's Aurex module
lowered through LLVM IR and clang. Language-core experiments that need C
functions declare a narrow `extern c` boundary in the sample under test.

## Quality Gates

```sh
tools/run_tests.sh
tools/bench.py
make perf
make perf-stress
make perf-stress-threshold
make perf-release-threshold
make perf-ast-stress
```

The test script covers lexer/AST dumps, hello end-to-end codegen, positive
language samples, negative semantic samples, current language features, LLVM
lowering, native execution, import paths, and install-tree compiler execution.
`tools/bench.py` uses a Release `build/perf` tree and Google Benchmark for
frontend hot-path measurements. `make perf` prints the lightweight JSON-derived
Aurex frontend baseline and the Google Benchmark process-level comparison
against available modern frontend drivers (`clang++`, `g++`, and `rustc`);
`make perf-stress-threshold` enforces the local CI stress gate, while
`make perf-release-threshold` now builds and runs the release gate as
Release+LTO from `build/perf-lto` by default. `make perf-release-lto-threshold`
and `make perf-release-all-threshold` are compatibility aliases for that same
release gate, not a second non-LTO lane. These threshold gates share
`AUREX_PERF_THRESHOLD_PROFILE` and `AUREX_PERF_THRESHOLD_SCALE`; the profile is
recorded in each stress JSON file, and the positive scale factor multiplies the
elapsed-time and peak-RSS thresholds for calibrated cross-machine runs without
changing the baseline command lines. The stress tools now also pass
`--profile-output` to `aurexc` and persist `aurex-profile-v1` phase JSON for
module read/lex/parse/append, sema, cache, IR, LLVM, and native backend phases;
the console report includes process wall/user/sys time, peak RSS, page faults,
and per-phase elapsed time plus cumulative RSS-after / RSS-delta values.
`make perf-compare` runs only the cross-frontend comparison lane.
`make perf-stress` runs generated mixed-feature
generic, AST bulk, and diagnostic baselines and records both elapsed time and
peak RSS. `make perf-ast-stress` runs only the AST bulk lane. The three stress
generators default to `--shape=mixed`: generic stress covers generic
struct/enum/type-alias constraints, impl methods, pointer aliases, tuple/pair
helpers, slices, and pattern matching; AST stress covers extern declarations,
type aliases, structs/enums, impl/methods, generic constraints, tuples,
arrays/slices, match/or-patterns, `let else`, `if is`, `try`, `defer`, loops,
compound assignment, unsafe pointer/string builtins, and sampled large-module
bulk expressions; diagnostic stress cycles unknown-name, type mismatch,
call-arity/type, field/index, struct literal, enum payload, builtin, generic
apply, array/void, operator, and match-arm failures. The `--check` path does
not retain generic instance side tables; `--emit=typed` keeps typed generic
bodies without lowering so retained-side-table memory can be stressed separately
from IR/codegen. `tools/generic_stress.py --shape=templates` covers the
many-distinct-generic-template 2000/5000+ case as a narrow comparison shape, and
`--shape=instances` keeps the old many-instantiations shape. The default sema
path no longer copies or
retains a full normalized AST snapshot. The syntax AST now stores `TypeNode`, `ExprNode`, and `PatternNode`
as compact 32-byte headers plus per-kind payload arenas; `StmtNode` and
`ItemNode` now use the same compact header + per-kind payload arena layout.
Sema, IR lowering, and AST dump hot paths read `ExprNode` payloads through
compact views instead of materializing fat nodes. The remaining address-based
item owner lookup has been replaced with explicit `ItemId` ownership. Identifier
storage now uses a reusable global bump allocator through the syntax-layer
`IdentifierInterner`; AST identifier-bearing nodes carry native `IdentId`
payload fields, and sema typed lookup keys use the AST module interner instead
of a second private interner. Function, type, value, generic, enum-case,
method/member, and local-scope lookup now use `IdentId` typed indexes. Sema
source-name fields borrow the AST identifier interner instead of copying those
strings into checked storage; only generated ABI/display/dump names are owned by
the checked C-name interner. Compact AST header/payload vectors and identifier
interner vectors/hash nodes are now bump-backed; parser construction, module
loading, and postfix materialization write compact expression headers plus
per-kind payloads directly. Parser startup estimates AST storage from token
shape and reserves hot payload arenas up front so bump-backed vectors do not
retain repeated growth buffers on large modules. Benchmark and stress tooling
records machine-specific Release/LTO profiles, phase timings, peak RSS, and
threshold metadata in generated benchmark outputs; README intentionally keeps
only the commands and gate shape instead of treating local timing/RSS samples as
portable performance promises. Generic side-table lifetime is now closed
on the main path: retained instances use function-local NodeSpan side tables,
share module-level sparse NodeSpan layouts only when a template needs
non-contiguous node-id mappings, release sema-only expected-type and
pattern-case caches after analysis, and use 1 KiB per-instance side-table
blocks to keep 2000/5000+ mixed-template stress from paying a 64 KiB floor per
instance. The remaining work is adding more calibrated machine profiles as data
comes in, not a known whole-module side-table retention path or missing perf
gate mechanism.
Expression typing now keeps intrinsic and contextual final types separate:
`expr_intrinsic_types` records context-free expression types, `expr_types`
records final checked types, `expr_expected_types` keys final-cache reuse, and
`CoercionRecord` captures contextual literal/null/slice adjustments. This closes
the old expected-type cache-pollution path without changing IR lowering, which
continues to consume final `expr_types`.
Match exhaustiveness now uses a pattern matrix / usefulness witness search for
bool, enum payload, tuple, struct, fixed-array, dynamic-slice, and open integer
patterns instead of enumerating structural cartesian products or relying on a
4096-combination cap. Dynamic slices are checked through symbolic representative
lengths plus element matrices, so `[]` + `[_, ..]` and bool head partitions are
accepted without requiring a catch-all wildcard.

## Stage Status

The previous M1/selfhost/bootstrap experiment has been removed from the active
tree. Active work is on the C++ Stage0 compiler, Aurex IR, and the LLVM backend.
A new bootstrap track should wait until the M2 language core has stable module
isolation, visibility, generics, sum types, pattern matching, `unsafe`,
non-resource capability/trait/where, and IR contracts.

## Documentation

See:

- `docs/README.md`
- `docs/zh/README.md`
- `docs/en/README.md`

The documentation is now organized by topic and language instead of one file
per small 0.1.x increment.
