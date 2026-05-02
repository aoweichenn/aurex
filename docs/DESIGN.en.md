# Aurex M0 Design Notes

Version: M0V0.1.8

## 1. Design Goals

Aurex M0 is the bootstrap core of Aurex. It is designed around five constraints:

- cost transparency: no hidden copies, destructors, allocations, or implicit conversions;
- deterministic semantics: evaluation order and ABI behavior must become explicit;
- small bootstrap surface: enough language to write the next compiler, not a full application language;
- replaceable stages: lexer, parser, sema, and codegen must be independently testable;
- readable implementation: a beginner should be able to trace a source file through the whole compiler.

## 2. Pipeline

```text
source.ax
  -> SourceManager
  -> handwritten Lexer
  -> Token[]
  -> handwritten recursive descent Parser
  -> AstModule
  -> SemanticAnalyzer
  -> CheckedModule
  -> CEmitter
  -> output.c
```

The parser never depends on the lexer class. It only consumes
`std::span<const Token>`. Sema does not depend on parser internals. Codegen does
not depend on parser internals.

## 3. Module Boundaries

`m0_base`

- owns source files, source ranges, diagnostics, integer aliases, and `Result<T>`;
- has no dependency on compiler language concepts.

`m0_syntax`

- owns token definitions and AST storage;
- AST nodes are plain structs, not virtual classes;
- IDs address nodes in vectors.

`m0_lex`

- handwritten byte-oriented lexer;
- produces tokens with byte ranges and string views into source storage.

`m0_parse`

- handwritten recursive descent parser;
- syntax only, no type checking;
- creates AST nodes and panic-mode diagnostics.

`m0_sema`

- resolves names and types;
- owns `TypeTable`, `SymbolTable`, and `CheckedModule`;
- records side tables such as expression types.

`m0_codegen_c`

- emits C from AST plus `CheckedModule`;
- should not guess types or resolve names.

`m0_driver`

- orchestrates file IO and stage execution;
- owns diagnostics presentation.
- resolves imports by loading `a/b.ax` for `import a.b;`;
- merges imported ASTs into one Stage0 checked module with ID remapping.
- validates that imported files declare the module path they were imported as;
- reports missing imports and module-name mismatches with source ranges.
- detects cyclic imports while loading modules.
- exposes `--dump-modules` for inspecting the resolved module set.

`m0c`

- parses command line arguments only.

## 4. AST Design

M0 uses compact IDs and vector storage:

```text
TypeId -> AstModule::types
ExprId -> AstModule::exprs
StmtId -> AstModule::stmts
ItemId -> AstModule::items
```

This avoids virtual dispatch and allows later stages to attach side tables
without mutating parse-only AST nodes.

Tradeoff: AST nodes are currently wide structs. This is simple and readable, but
later iterations can split payloads by node kind if memory use becomes a real
problem.

## 5. Error Handling

Compiler stages return `Result<T>`. Diagnostics are pushed into
`DiagnosticSink`. Exceptions are not the normal error path.

Diagnostic ranges are byte offsets. M0V0.1.8 prints:

- file path;
- line and column;
- severity;
- message;
- source line;
- caret marker.

## 6. Semantic Model

M0 rejects implicit work:

- no implicit numeric conversion;
- no implicit pointer conversion;
- no function overloads;
- no shadowing;
- no assignment to `let`;
- arrays are storage-only;
- opaque structs are only usable through pointers.

The current sema implementation intentionally records expression types in
`CheckedModule::expr_types`. This makes codegen simpler and keeps type logic out
of the emitter.

## 7. C Backend Model

The C backend maps M0 types to stable C spellings:

- `i32` -> `int32_t`
- `u8` -> `uint8_t`
- `usize` -> `size_t`
- `isize` -> `ptrdiff_t`
- `*mut T` -> `T*`
- `*const T` -> `const T*`
- `[N]T` -> `T[N]`

Known future work:

- introduce a lowering layer before C emission;
- generate temporaries for guaranteed left-to-right evaluation;
- normalize ABI names and exported symbols;
- split expression emission into value emission and statement lowering.

## 8. Self-Hosting Design

Self-hosting is tracked in `selfhost/`.

The intended fixed-point chain is:

```text
Stage0 C++ compiler
  -> compiles M0 compiler sources
  -> produces m0c-stage1
  -> m0c-stage1 compiles the same sources
  -> stage1/stage2 generated outputs match
```

Current M0V0.1.8 status:

- `bootstrap/` contains a standalone Stage0-mini compiler;
- `selfhost/src/m0c_seed.ax` is the first M0 seed;
- `selfhost/src/aurex/selfhost/lexer/core.ax` is the shared M0 lexer core,
  including the `TokenSpan` token shape and `scan_token`;
- `selfhost/src/aurex/selfhost/lexer/dump.ax` is the shared token dump helper;
- `selfhost/src/lexer_smoke.ax` imports the shared core scanner and validates a
  small token sequence;
- `selfhost/src/lexer_ranges.ax` imports the shared core scanner and validates
  token kind plus `begin/end` byte ranges;
- `selfhost/src/lexer_dump.ax` imports the shared dump helper, prints a
  token-kind stream from M0, and is
  checked against a golden file;
- `selfhost/src/lexer_file.ax` imports the shared dump helper, reads a source
  file through explicit runtime IO, and emits a token-kind stream checked
  against a golden file;
- `selfhost/src/aurex/selfhost/parser/seed.ax` is the first M0 parser seed. It
  validates `module`, `import`, `extern c`, function signatures, and an
  `export c fn` body shell using a recursive-descent cursor over `TokenSpan`;
- `selfhost/src/parser_smoke.ax` is the executable parser seed smoke test;
- `tools/compare_selfhost_lexer.sh` directly compares that M0 lexer stream with
  the production C++ Stage0 lexer stream over `examples/hello.ax` and every
  local positive/negative test input;
- selfhost tests assert that `lexer_file.ax` and `parser_smoke.ax` load the
  shared lexer/parser modules, so import use is part of the regression suite;
- the actual production compiler is still C++.

The next real self-hosting milestone should make the parser seed produce a tiny
AST summary or stable parse dump, then compare that output with the C++ parser
on a small shared corpus.

## 9. Industrial Hardening Roadmap

Near-term:

- real unit test executable instead of shell-only tests;
- golden file tests for diagnostics and C output;
- lowering layer for evaluation order;
- import path and module graph;
- richer source locations and multi-line diagnostics.

Mid-term:

- M0 lexer in M0;
- M0 parser in M0;
- ABI validation suite;
- fuzzing for lexer/parser;
- benchmarking with fixed corpus and trend tracking.

Long-term:

- Stage1/Stage2 self-host fixed point;
- deterministic build output;
- release artifacts;
- compatibility policy.
