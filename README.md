# Aurex M0

Current baseline: **M0V0.1.8**.

Aurex M0 is a small bootstrap compiler project written in modern C++20. The
lexer and parser are handwritten. The AST uses compact IDs and vector storage.
Stage0 now compiles through Aurex IR -> LLVM IR -> clang by default.

## Quick Start

```sh
cmake -S . -B build
cmake --build build -j
build/m0c examples/hello.ax -o build/hello
build/hello
```

Expected output:

```text
hello from Aurex M0
```

Stage0 now resolves imports:

```sh
build/m0c -I tests/imports tests/positive/import_path.ax -o build/import_path
```

Stage0 can also produce assembly and object files through clang:

```sh
build/m0c --emit=asm examples/hello.ax -o build/hello.s
build/m0c --emit=obj examples/hello.ax -o build/hello.o
build/m0c --emit=exe examples/hello.ax -o build/hello
```

`--emit=exe` is the default. `--clang <path>` selects a clang binary, and
repeated `--clang-arg <arg>` options pass raw arguments such as `-O2` or `-g`.

The IR path is visible with:

```sh
build/m0c --emit=ir examples/hello.ax
build/m0c --emit=llvm-ir examples/hello.ax
```

`--emit=ir` prints Aurex's typed CFG/SSA IR. `--emit=llvm-ir` lowers that IR to
LLVM IR and prints the result. Native output runs Aurex IR -> LLVM IR -> clang.
The old Stage0 C backend has been removed from the production build; the
selfhost Stage1 path now emits Aurex IR snapshots instead of C.

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

Self-hosting is explicit in `selfhost/`, but M0V0.1.8 is not fully self-hosted
yet. The current track contains reusable M0 lexer/parser pieces, an ID-backed
AST seed, and a Stage1 compiler entry that emits `aurex_ir v0` snapshots.

The old selfhost C emitter has been removed from the active tree. Stage1 now
uses `aurex/selfhost/compiler/ir/`, split into writer, name, type, expression,
and emission modules. For syntax not yet covered by the parser seed, Stage1
records deterministic `selfhost_module ... lowering(ast_pending)` markers so
the compiler bundle remains measurable without claiming full lowering.

```sh
tools/bootstrap_chain.sh
make -C selfhost check
```

Manual Stage1 IR run:

```sh
build/m0c -I selfhost/src selfhost/src/aurex/selfhost/bin/m0c_stage1.ax --runtime-c selfhost/runtime/runtime.c -o build/m0c_stage1
build/m0c_stage1 examples/hello.ax build/hello.stage1.air
```

## Documentation

See:

- `docs/USAGE.en.md`
- `docs/USAGE.zh.md`
- `docs/ARCHITECTURE.zh.md`
- `docs/SELFHOST.md`
- `docs/DESIGN.en.md`
- `docs/DESIGN.zh.md`
- `docs/SEMANTICS.md`
- `docs/M0V0.1.8.md`
- `docs/M0V0.1.7.md`
- `docs/M0V0.1.6.md`
- `docs/M0V0.1.5.md`
- `docs/M0V0.1.4.md`
- `docs/M0V0.1.3.md`
- `docs/M0V0.1.2.md`
- `docs/M0V0.1.1.md` for the previous release baseline.
