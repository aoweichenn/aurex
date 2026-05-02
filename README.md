# Aurex M0

Current baseline: **M0V0.1.8**.

Aurex M0 is a small bootstrap compiler project written in modern C++20. The
lexer and parser are handwritten. The AST uses compact IDs and vector storage.
The first backend emits C.

## Quick Start

```sh
cmake -S . -B build
cmake --build build -j
build/m0c examples/hello.ax -o build/hello.c
cc build/hello.c -o build/hello
build/hello
```

Expected output:

```text
hello from Aurex M0
```

Stage0 now resolves imports:

```sh
build/m0c -I tests/imports tests/positive/import_path.ax -o build/import_path.c
```

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

Self-hosting is now explicit in `selfhost/`.

```sh
tools/bootstrap_chain.sh
```

M0V0.1.8 is not fully self-hosted yet. The current tree contains a visible
bootstrap seed and a tested chain so future iterations can move compiler
components into M0 without hiding the status.

The selfhost track now has a reusable M0 lexer module in
`selfhost/src/aurex/selfhost/lexer/`. `core.ax` owns token constants,
`TokenSpan`, and the zero-allocation scanner; `dump.ax` owns token-kind
printing. The entry programs `lexer_smoke.ax`, `lexer_ranges.ax`,
`lexer_dump.ax`, and `lexer_file.ax` import those modules instead of carrying
copied lexer logic.

`lexer_ranges.ax` verifies that the M0 scanner returns parser-ready
`kind/begin/end` token metadata for a fixed source string.
`parser_smoke.ax` is the first M0 parser seed. It uses a tiny recursive-descent
cursor over `TokenSpan` and validates `module`, `import`, `extern c`, and
`export c fn` syntax for a fixed source string.
`lexer_dump.ax` scans an embedded M0 source and prints a stable token-kind
stream checked against `tests/golden/selfhost_lexer_dump.tokens`.
`lexer_file.ax` reads `examples/hello.ax` through explicit runtime file IO and
checks that token stream against `tests/golden/selfhost_lexer_file_hello.tokens`.
`tools/compare_selfhost_lexer.sh` compares the M0 lexer stream directly with
the production C++ Stage0 lexer stream over the local corpus.

```sh
make selfhost
```

## Documentation

See:

- `docs/USAGE.en.md`
- `docs/USAGE.zh.md`
- `docs/SELFHOST.md`
- `docs/DESIGN.en.md`
- `docs/DESIGN.zh.md`
- `docs/SEMANTICS.md`
- `docs/C_BACKEND_DESIGN.md`
- `docs/M0V0.1.8.md`
- `docs/M0V0.1.7.md`
- `docs/M0V0.1.6.md`
- `docs/M0V0.1.5.md`
- `docs/M0V0.1.4.md`
- `docs/M0V0.1.3.md`
- `docs/M0V0.1.2.md`
- `docs/M0V0.1.1.md` for the previous release baseline.
