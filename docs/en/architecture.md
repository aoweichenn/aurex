# Architecture Design

## Components

- `src/lex`: lexical analysis.
- `src/parse`: recursive-descent parser.
- `src/syntax`: AST, tokens, module paths, and dumps.
- `src/sema`: name resolution, types, generic instantiation, pattern
  exhaustiveness, value-semantics checks, and control-flow checks.
- `src/ir`: Aurex IR, lowering, verification, and pass pipeline.
- `src/backend/llvm`: LLVM IR emission.
- `src/driver`: file reads, module loading, compile pipeline, and clang calls.
- `src/cli`: `aurexc` command-line entry point.

## Branch Boundary

M2 `language-core-no-std` removes the standard-library layer:

- No `std/` source tree.
- The driver does not locate a std root.
- The module loader does not append an implicit std import path.
- Native executable output does not append support sources.
- Install rules install only the compiler.

Language-core changes can now be validated with self-contained samples, without
std loading, host support, or M1 examples obscuring compiler semantics and
performance.

## Next Architecture Direction

Before restoring std, the language needs the current core abstractions:

- `unsafe` boundaries for raw pointers, unchecked strings, and bit-level casts.
- ADT enums and pattern matching as the main way to model Result/Option and AST
  state spaces.
- Arrays, slices, strings, and function types as std-independent value and ABI
  foundations.
- Trait/where for non-resource generic constraints.
- Resource semantics: `Copy`, `Drop`, borrow checking, and move-out remain a
  later design track, not a current architecture prerequisite.
