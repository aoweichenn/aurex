# Aurex M0 Selfhost Track

This directory makes the self-hosting goal explicit.

Current status:

- `bootstrap/`: standalone C++20 Stage0-mini compiler. It compiles a small M0
  subset to C and is built with a plain Makefile.
- `src/`: M0 source seeds for the future compiler written in M0.
  - `m0c_seed.ax`: minimal executable seed.
  - `src/aurex/selfhost/lexer/core.ax`: shared M0 lexer core. It owns token kind
    constants, the `TokenSpan` token shape, character classification, keyword
    recognition, trivia skipping, `scan_token`, and the compatibility wrapper
    `scan_next`.
  - `src/aurex/selfhost/lexer/dump.ax`: shared token-kind dump helper built on the
    core scanner.
  - `lexer_smoke.ax`: small lexer-oriented M0 component. It imports the shared
    core scanner, scans an embedded source string, and validates the token kind
    sequence for a small corpus.
  - `lexer_ranges.ax`: parser-readiness smoke test. It imports the shared core
    scanner and verifies token kind plus `begin/end` byte ranges.
  - `lexer_dump.ax`: token dump generator written in M0. It scans an embedded
    source string through the shared dump module and prints one token kind per
    line, then the test chain compares that output with a golden file.
  - `lexer_file.ax`: file-backed lexer driver written in M0. It reads a source
    file through explicit runtime IO, scans it through the shared dump module,
    and prints token kinds.
  - `src/aurex/selfhost/parser/`: first parser seed written in M0. It is split
    into cursor, type/signature, expression, and orchestration modules; type
    parsing uses an iterative pointer-prefix stack, and expression parsing uses
    explicit operator/frame stacks.
  - `parser_smoke.ax`: parser seed entry point. It checks `module`, `import`,
    `extern c`, function signatures, an `export c fn` body, calls, unary
    expressions, full call arguments, and binary precedence.
- `runtime/`: future M0 runtime shims needed by the self-hosted compiler.
  - `runtime.c` currently provides explicit file buffer allocation/freeing for
    the selfhost lexer driver.

Important boundary: M0V0.1.8 is **not fully self-hosting yet**. The production
compiler is still the modular C++ implementation in `src/`. The selfhost track
exists so each iteration can move real compiler components from C++ into M0
without hiding the gap.

Target chain:

```text
Stage0 C++ m0c
  -> compiles selfhost/src/*.ax to C
  -> host cc builds the generated C into m0c-stage1
  -> m0c-stage1 compiles the same selfhost sources
  -> output comparison proves bootstrap stability
```

Use:

```sh
tools/bootstrap_chain.sh
```

The current chain verifies the sequence-only lexer smoke test, the range-aware
token smoke test, the parser seed smoke test, and the golden token dumps. That
makes the selfhost frontend path measurable before it is complete enough to
become the production frontend.

M0V0.1.8 also verifies that the selfhost lexer/parser programs really use M0
module imports. `make -C selfhost check` and `tools/bootstrap_chain.sh` both
dump the loaded module set for `lexer_file.ax` and `parser_smoke.ax`, then
require the shared `dump`, `parser.cursor`, `parser.types`, `parser.expr`,
`parser.seed`, and `core` modules to appear.

This directory can also be built directly:

```sh
make -C selfhost check
```

That command compiles all current M0 selfhost sources with the Stage0 compiler,
runs them, validates token ranges, validates the parser seed, and compares both
lexer dumps with golden files.
