# Aurex M0 Self-Hosting Plan

## Current State

M0V0.1.8 has a visible self-hosting track, but it is not fully self-hosted.

Implemented now:

- `bootstrap/m0_bootstrap.cpp`: standalone Stage0-mini compiler, built with a
  plain Makefile.
- `selfhost/src/m0c_seed.ax`: first M0 source seed compiled by the production
  C++ Stage0 compiler.
- `selfhost/src/aurex/selfhost/lexer/core.ax`: shared M0 lexer core. It owns
  token constants, the `TokenSpan` token shape, character classification,
  keyword matching, trivia skipping, `scan_token`, and the compatibility wrapper
  `scan_next`.
- `selfhost/src/aurex/selfhost/lexer/dump.ax`: shared token-kind dump helper
  built on the core scanner.
- `selfhost/src/lexer_smoke.ax`: lexer-oriented M0 smoke test. It imports the
  shared core scanner, scans an embedded source string, validates the token kind
  sequence for a small corpus, and is compiled/run by the bootstrap chain.
- `selfhost/src/lexer_ranges.ax`: range-aware M0 lexer smoke test. It imports
  the shared core scanner and validates token kind plus `begin/end` byte ranges.
- `selfhost/src/lexer_dump.ax`: first token dump generator written in M0. It
  imports the shared dump helper, scans an embedded source, and prints one token
  kind per line. The output is compared with
  `tests/golden/selfhost_lexer_dump.tokens`.
- `selfhost/src/lexer_file.ax`: file-backed lexer driver written in M0. It
  imports the shared dump helper, reads a source file through explicit runtime
  IO, and compares the token stream for `examples/hello.ax` with
  `tests/golden/selfhost_lexer_file_hello.tokens`.
- `selfhost/src/aurex/selfhost/parser/seed.ax`: first parser seed written in
  M0. It uses a one-token `TokenSpan` cursor and validates a small
  recursive-descent syntax subset.
- `selfhost/src/parser_smoke.ax`: parser seed entry point covering `module`,
  `import`, `extern c`, function signatures, and an `export c fn` body shell.
- `selfhost/src/aurex/selfhost/compiler/io.ax`: explicit runtime IO bridge for
  the selfhost compiler slice.
- `selfhost/src/aurex/selfhost/compiler/subset.ax`: first Stage1 compiler
  parser. It reuses the selfhost lexer and parses a deliberately small M0 subset:
  module/import declarations, an `extern c` block, and `export c fn main`
  containing `puts(c"..."); return <integer>;`.
- `selfhost/src/aurex/selfhost/compiler/emit_c.ax`: C emitter for that subset.
- `selfhost/src/aurex/selfhost/compiler/driver.ax`: Stage1 compile pipeline
  wiring.
- `selfhost/src/m0c_stage1.ax`: executable entry point for the Stage1 compiler
  slice.
- `selfhost/runtime/runtime.c`: explicit runtime services used by the selfhost
  lexer driver and Stage1 compiler slice.
- `tools/bootstrap_chain.sh`: verifies both the selfhost seed and standalone
  bootstrap path.
- `tools/compare_selfhost_lexer.sh`: compares the M0 file-backed lexer output
  against the production C++ Stage0 lexer token-kind stream for
  `examples/hello.ax` plus every local positive/negative test corpus file.

## Why This Is Not Yet Full Self-Hosting

Full self-hosting means the compiler implementation is written in M0, compiled
by an earlier compiler, and then capable of compiling itself again with stable
output.

The current production compiler is still C++20. The selfhost tree now has a real
Stage1 vertical slice: Stage0 compiles `m0c_stage1.ax`, that M0 program reads an
M0 source file, validates the supported subset, emits C, and the generated C is
compiled and run. That is meaningful bootstrap progress, but it is not yet a
fixed-point self-host because Stage1 cannot compile the full compiler source.

## Milestones

### Stage A: Stable Stage0

- Keep C++ modular compiler clean and testable.
- Maintain exact lexer/parser/sema/codegen boundaries.
- Grow M0 language only with tests.

### Stage B: M0 Runtime

- Add explicit runtime modules for memory, files, diagnostics, and process IO.
- Keep runtime services explicit. No hidden allocator or hidden destructor model.

### Stage C: Lexer In M0

- Port token definitions and scanner to M0.
- Compile M0 lexer with Stage0 `m0c`.
- Compare token dumps between C++ lexer and M0 lexer.

M0V0.1.8 uses the Stage0 module loader in the selfhost tree: the embedded
source driver and file-backed driver both import shared lexer modules, and the
core scanner now returns token ranges through `TokenSpan`.

### Stage D: Parser In M0

- Expand the parser seed into a larger recursive descent parser.
- Produce a stable parse summary from M0.
- Compare parse summaries or AST dumps between C++ parser and M0 parser.

### Stage E: Sema In M0

- Port TypeTable, SymbolTable, and semantic checks.
- Compare diagnostics and checked metadata.

### Stage F: Codegen In M0

- Port C emitter.
- Build `m0c-stage1`.
- Compile selfhost sources with Stage1.

M0V0.1.8 now has the first Stage F slice: `m0c_stage1.ax` can compile
`examples/hello.ax` through the M0-written lexer/parser/emitter path.

### Stage G: Fixed Point

- Stage0 builds Stage1.
- Stage1 builds Stage2.
- Stage1 and Stage2 generated C outputs match after normalizing timestamps and
  paths.

## Current Command

```sh
tools/bootstrap_chain.sh
```

Expected output:

```text
bootstrap chain passed: Stage0 m0c + selfhost lexer smoke/ranges/dump/file + parser seed + M0 stage1 compiler slice + standalone bootstrap seed
```

The selfhost subtree also has its own entry point:

```sh
make -C selfhost check
```

Both `bootstrap_chain.sh` and `make -C selfhost check` now prove the same
important properties: the M0 lexer driver and the C++ Stage0 lexer agree on the
token kind sequence for the local corpus, the first M0 parser seed can parse a
fixed module/import/extern/function-signature source, and the M0-written Stage1
compiler slice can compile `examples/hello.ax` into runnable C.

They also assert that `selfhost/src/lexer_file.ax` and
`selfhost/src/parser_smoke.ax` load the expected shared lexer/parser modules.
The same check now covers `selfhost/src/m0c_stage1.ax` and its compiler modules,
so selfhost module usage is part of the regression suite rather than a
documentation-only claim.
