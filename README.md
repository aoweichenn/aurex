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
selfhost Stage1 path now emits Aurex IR snapshots instead of C.

The bundled std root is found through `--stdlib`, `AUREX_STDLIB`, build-tree
defaults, and relocatable install layouts such as `share/aurex/std` next to the
installed `bin/aurexc`. Executable output links the selected std support backend
(`--std-backend host-c` by default), whose stable host-facing symbols are
versioned as `aurex_std_v0_*`.

## Quality Gates

```sh
tools/run_tests.sh
tools/bench.py
```

The test script covers lexer/AST dumps, hello end-to-end codegen, positive
language samples, negative semantic samples, and the standalone bootstrap
compiler.

## Bootstrap

The `bootstrap/` directory contains a single-file Stage0-mini compiler and a
plain Makefile:

```sh
make -C bootstrap
bootstrap/m0_bootstrap examples/hello.ax -o bootstrap/hello.bootstrap.c
```

The standalone bootstrap compiler is intentionally small and heavily commented.
It demonstrates the minimal translator path; the modular compiler is the
authoritative implementation.

## Selfhost Track

Self-hosting is explicit in `selfhost/`, but 0.1.2 is not fully self-hosted
yet. The current track contains reusable M0 lexer/parser pieces, an ID-backed
AST seed, and a Stage1 compiler entry that emits `aurex_ir v0` snapshots.

The old selfhost C emitter has been removed from the active tree. Stage1 now
uses `aurex/selfhost/compiler/ir/`, split into writer, name, type, expression,
and emission modules. The seed parser now covers multiple `export c fn` items
in one module, and Stage1 IR snapshots emit expression values per function
block. For syntax not yet covered by the parser seed, Stage1 records
deterministic `selfhost_module ... lowering(ast_pending)` markers so the
compiler bundle remains measurable without claiming full lowering.

```sh
tools/bootstrap_chain.sh
make -C selfhost check
```

Manual Stage1 IR run:

```sh
build/bin/aurexc -I selfhost/src selfhost/src/aurex/selfhost/bin/aurexc_stage1.ax -o build/selfhost/aurexc_stage1
build/selfhost/aurexc_stage1 examples/hello.ax build/selfhost/hello.stage1.air
```

## Documentation

See:

- `docs/README.md`
- `docs/zh/README.md`
- `docs/en/README.md`

The documentation is now organized by topic and language instead of one file
per small 0.1.x increment.
