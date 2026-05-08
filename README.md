# Aurex M0

Documentation baseline: **language-core-no-std**.

Aurex M0 is a small bootstrap compiler project written in modern C++20. The
lexer and parser are handwritten. The AST uses compact IDs and vector storage.
Stage0 now compiles through Aurex IR -> LLVM IR -> clang by default.

## Quick Start

```sh
cmake -S . -B build
cmake --build build -j
build/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Expected output:

```text
hello from Aurex M0
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
build/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` is the default. `--clang <path>` selects a clang binary,
`--opt-level O1` enables Aurex IR passes, and repeated `--clang-arg <arg>`
options pass raw arguments such as `-O2` or `-g` to clang.

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
```

The test script covers lexer/AST dumps, hello end-to-end codegen, positive
language samples, negative semantic samples, current language features, LLVM
lowering, native execution, import paths, and install-tree compiler execution.

## Bootstrap Status

The previous M0-written bootstrap experiment has been removed. Active work is
on the C++ Stage0 compiler, Aurex IR, and the LLVM backend. A new bootstrap
track is expected around M3, after the language has enough module isolation,
visibility, generics, sum types, pattern matching, inference, and AIR contracts
to make the rewrite representative instead of carrying old seed constraints.

## Documentation

See:

- `docs/README.md`
- `docs/zh/README.md`
- `docs/en/README.md`

The documentation is now organized by topic and language instead of one file
per small 0.1.x increment.
