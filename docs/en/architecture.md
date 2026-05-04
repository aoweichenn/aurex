# Architecture Design

Version: 0.1.2

## Layering

```text
base
  -> syntax
  -> lex / parse
  -> sema
  -> ir
  -> backend/llvm
  -> driver
  -> cli
```

Dependencies are one-way. AST does not depend on lexer/parser implementation,
sema does not write back into AST, and backend code does not inspect syntax
nodes.

## Architecture Principles

- Explicit stage outputs: each stage produces data that can be dumped, tested,
  or reused.
- Syntax/semantics separation: AST stores source structure; types, symbols, and
  ABI results live in checked metadata.
- Backend isolation: the LLVM backend consumes Aurex IR and does not read AST.
- Two-layer std design: language-level `.ax` APIs are separate from
  host/backend support.
- Relocatable install layout: runtime lookup is not tied to the developer's
  source-tree path.

## Components

- `include/aurex/base` / `src/base`: integer aliases, result type, diagnostics,
  source management, and ABI constants.
- `include/aurex/syntax` / `src/syntax`: tokens, AST IDs, module paths, and AST
  dumping.
- `src/lex`: handwritten lexer.
- `src/parse`: handwritten recursive-descent parser.
- `src/sema`: type table, symbol table, and semantic analysis.
- `src/ir`: Aurex IR, IR dump, lowering, verifier, and pass pipeline.
- `src/backend/llvm`: LLVM IR lowering.
- `src/driver`: module loading, std lookup, and native toolchain invocation.
- `src/cli`: command-line parsing.
- `std`: Aurex standard-library modules and backend support.
- `selfhost`: M0 bootstrap slices.

## Key Data Boundaries

- The lexer outputs `Token`: kind, text slice, and source range.
- The parser outputs `AstModule`: ID-backed type, expr, stmt, item, and module
  information.
- Sema outputs `CheckedModule`: type table, expression types, symbols, ABI
  names, function signatures, and record-layout metadata.
- IR lowering outputs `ir::Module`: typed CFG/SSA-like functions, blocks,
  values, terminators, and global constants.
- The LLVM backend outputs LLVM IR text, which clang turns into native
  artifacts.

## Compile Path

```text
root source
  -> ModuleLoader
  -> Lexer
  -> Parser
  -> SemanticAnalyzer
  -> IR Lowerer
  -> IR Pass Pipeline
  -> LLVM Backend
  -> clang
```

## Standard-Library Architecture

The standard library has two parts:

- language-level `.ax` modules under responsibility directories such as
  `std.core.text`, `std.core.mem`, `std.fs.file`, `std.io.console`, and
  `std.sys.process`.
- temporary C FFI declarations under `std/ffi/c/`.
- backend support, currently `std/ffi/c/support/host_c.c` by default.

Host support exports stable `aurex_std_v0_*` symbols. C FFI is isolated under
`std/ffi/c/` so a future backend engine can replace it without rewriting the
language-level std API.

ABI strategy:

- New declarations should use `aurex_std_v0_*`.
- If host support needs a breaking change, introduce `aurex_std_v1_*` instead
  of reusing v0 names.

Backend support strategy:

- `host-c` is the default backend for the current clang/native path.
- `none` is available for outputs that do not need std support linking.
- Future backends should be selected by the driver, not hardwired into
  language-level `.ax` std modules.

## Install And Lookup Architecture

Install target:

```text
<prefix>/bin/aurexc
<prefix>/share/aurex/std
```

Runtime std lookup uses explicit configuration first, then paths relative to
the `aurexc` executable, and finally a `std` directory in the current working
directory. This supports build-tree, install-tree, and local-development usage.

## Error And Diagnostic Architecture

`DiagnosticSink` collects errors with source ranges. On lex/parse/sema failure,
the driver prints file, line, column, severity, message, and a source caret.
Backend and IO failures are returned through `base::Result`.

## Self-Hosting Architecture

The selfhost tree is split by component:

- `lexer`: M0 lexer core and token dumping.
- `syntax`: ID-backed AST data structures.
- `parser`: cursor, types, expr, and seed modules.
- `compiler/ir`: writer, names, types, expr, and emit modules.
- `bin`: Stage1 CLI entry points.
- `smoke` / `tool`: validation programs.

Stage1 currently emits Aurex IR snapshots. It is not yet a full fixed-point
compiler. The seed parser covers a module shape with one `extern c` block plus
multiple `export c fn` items, and IR snapshots emit expression values scoped to
the current function block.

## Verification Architecture

- `tools/run_tests.sh`: main quality gate for build, CLI, IR, LLVM, native, std,
  selfhost, and documentation layout.
- `tools/check_golden.sh`: golden output comparison.
- `tools/bootstrap_chain.sh`: selfhost smoke path.
- `tools/compare_selfhost_lexer.sh`: Stage0/Stage1 lexer behavior comparison.
- `tools/bench.py`: lightweight performance smoke benchmark.
