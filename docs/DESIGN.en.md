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
  -> Aurex IR
  -> LLVM IR
  -> clang -> output.s / output.o / executable
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

`m0_ir`

- lowers AST plus `CheckedModule` into Aurex's own IR;
- represents control flow with typed values, basic blocks, terminators, and
  `phi`;
- preserves `extern_c` / `export_c` linkage and ABI symbols;
- is the intended input for LLVM IR lowering and future native backends.

`m0_driver`

- orchestrates file IO and stage execution;
- owns diagnostics presentation.
- resolves imports by loading `a/b.ax` for `import a.b;`;
- merges imported ASTs into one Stage0 checked module with ID remapping and
  per-item module ownership.
- lets sema resolve top-level names through the current module and its direct
  imports, while IR lowering emits module-qualified ABI symbols.
- validates that imported files declare the module path they were imported as;
- reports missing imports and module-name mismatches with source ranges.
- detects cyclic imports while loading modules.
- exposes `--dump-modules` for inspecting the resolved module set.
- invokes clang for default native output, `--emit=asm`, `--emit=obj`, and
  `--emit=exe`.

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

## 7. Aurex IR Model

The new M0 IR is a typed CFG/SSA-like intermediate representation. It is not a
direct LLVM IR printer. The goal is to decouple frontend semantics from backend
printing: the AST stays parse-only, `CheckedModule` provides type and ABI side
tables, and IR describes a verifiable, optimizable program.

Current choices:

- functions record source name, ABI symbol, linkage, return type, and parameter
  signature;
- functions explicitly record ABI calling convention; `extern c` and
  `export c` use the C ABI path;
- locals remain explicit `alloca/load/store` slots until a later mem2reg pass;
- field and index operations lower to `field_addr` / `index_addr`, ready for
  LLVM GEP lowering;
- calls keep both final symbol text and an internal function id when resolvable;
- `&&` / `||` lower to control flow plus `phi`, so short-circuiting is explicit.

Recommended next steps are mem2reg, CFG cleanup, pass management, deeper LLVM
target configuration, and ABI tests for `extern c`, `export c`, and runtime
calls.

## 8. LLVM Backend And FFI

The Stage0 production backend is now LLVM-only: Aurex IR lowers to LLVM IR
text, and the driver invokes clang to produce assembly, object files, or native
executables. `extern c` / `export c` flow through ABI symbols, linkage, and
calling convention in IR.

Current coverage:

- positive M0 samples run through default native output;
- `--emit=llvm-ir` exposes the LLVM lowering result;
- `--emit=asm`, `--emit=obj`, and `--emit=exe` all consume LLVM output;
- repeated same-signature `extern c` declarations across modules merge to one
  LLVM declaration;
- runtime C sources can be linked with `--runtime-c`.

Remaining work:

- optimization pipeline and pass management;
- tighter runtime-linking rules for object/assembly modes;
- fuller ABI attributes and target triple/CPU/feature control;
- future custom native backends consuming the same IR/verifier/ABI contract.

## 9. Self-Hosting Design

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
- `selfhost/src/aurex/selfhost/bin/m0c_seed.ax` is the first M0 seed;
- `selfhost/src/aurex/selfhost/lexer/core.ax` is the shared M0 lexer core,
  including the `TokenSpan` token shape and `scan_token`;
- `selfhost/src/aurex/selfhost/lexer/dump.ax` is the shared token dump helper;
- `selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax` imports the shared core
  scanner and validates a small token sequence;
- `selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax` imports the shared core
  scanner and validates token kind plus `begin/end` byte ranges;
- `selfhost/src/aurex/selfhost/tool/lexer_dump.ax` imports the shared dump
  helper, prints a
  token-kind stream from M0, and is
  checked against a golden file;
- `selfhost/src/aurex/selfhost/tool/lexer_file.ax` imports the shared dump
  helper, reads a source file through explicit runtime IO, and emits a
  token-kind stream checked against a golden file;
- `selfhost/src/aurex/selfhost/parser/` is the first M0 parser seed, split into
  `cursor.ax`, `types.ax`, `expr.ax`, and `seed.ax`. Type parsing uses an
  explicit pointer-prefix stack, expression parsing uses explicit operator and
  frame stacks, and the parser produces an ID-backed `AstModule`;
- `selfhost/src/aurex/selfhost/smoke/parser_smoke.ax` is the executable parser
  seed smoke test;
- `tools/compare_selfhost_lexer.sh` directly compares that M0 lexer stream with
  the production C++ Stage0 lexer stream over `examples/hello.ax` and every
  local positive/negative test input;
- selfhost tests assert that `lexer_file.ax` and `parser_smoke.ax` load the
  shared lexer/parser modules, so import use is part of the regression suite;
- the actual production compiler is still C++.

The next real self-hosting milestone should keep expanding this iterative
parser seed's AST coverage, produce a stable AST summary or parse dump, then
compare that output with the C++ parser on a small shared corpus.

## 10. Industrial Hardening Roadmap

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
