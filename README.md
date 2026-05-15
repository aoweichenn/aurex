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

```sh
cmake -S . -B build
cmake --build build -j
build/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Expected output:

```text
hello from Aurex M2
```

Stage0 now resolves imports:

```sh
build/bin/aurexc -I tests/samples/imports tests/samples/positive/modules/import_path.ax -o build/tests/import_path
```

Reusable import examples live under `examples/libs`:

```sh
build/bin/aurexc -I examples/libs --emit=checked tests/samples/positive/modules/import_path.ax
```

Stage0 can also produce assembly and object files through clang:

```sh
build/bin/aurexc -S examples/hello.ax -o build/tests/hello.s
build/bin/aurexc -c examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
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

The IR path is visible with:

```sh
build/bin/aurexc --emit=ir examples/hello.ax
build/bin/aurexc --emit=llvm-ir examples/hello.ax
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
make perf-ast-stress
```

The test script covers lexer/AST dumps, hello end-to-end codegen, positive
language samples, negative semantic samples, current language features, LLVM
lowering, native execution, import paths, and install-tree compiler execution.
`tools/bench.py` uses a Release `build-perf` tree and Google Benchmark for
frontend hot-path measurements. `make perf` prints the lightweight JSON-derived
Aurex frontend baseline and the Google Benchmark process-level comparison
against available modern frontend drivers (`clang++`, `g++`, and `rustc`)
without enforcing thresholds yet. `make perf-compare` runs only the
cross-frontend comparison lane. `make perf-stress` runs the generated
200/500/1000/2000 generic-instantiation baseline plus the AST bulk elapsed-time
and peak-RSS baseline. `make perf-ast-stress` runs only the AST bulk lane. The
`--check` path does not retain codegen-only generic instance side tables, and
the default sema path no longer copies or retains a full normalized AST
snapshot.

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
