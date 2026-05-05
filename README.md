# Aurex M0

Documentation baseline: **0.1.2**.

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
build/bin/aurexc -I tests/imports tests/positive/import_path.ax -o build/tests/import_path
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

The bundled std root is found through `--stdlib`, `AUREX_STDLIB`, build-tree
defaults, and relocatable install layouts such as `share/aurex/std` next to the
installed `bin/aurexc`. Executable output links the selected std support backend
(`--std-backend host-c` by default), whose stable host-facing symbols are
versioned as `aurex_std_v0_*`. Temporary C FFI bindings live under
`std/ffi/c/`; language-level std modules import that boundary instead of
declaring host C directly.

## Quality Gates

```sh
tools/run_tests.sh
tools/bench.py
```

The test script covers lexer/AST dumps, hello end-to-end codegen, positive
language samples, negative semantic samples, M1 language features, std FFI
checks, LLVM lowering, native execution, and install-tree std lookup.

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
