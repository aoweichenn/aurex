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

The current architecture baseline is M4. M2 removed the standard-library layer:

- No `std/` source tree.
- The driver does not locate a std root.
- The module loader does not append an implicit std import path.
- Native executable output does not append support sources.
- Install rules install only the compiler.

Language-core changes can now be validated with self-contained samples, without
std loading, host support, or M1 examples obscuring compiler semantics and
performance.

## Current Architecture Direction

The active compiler architecture is query-backed and static-trait-aware:

- `unsafe` boundaries cover raw pointers, unchecked strings, and bit-level casts.
- ADT enums, pattern matching, arrays, slices, strings, and function types form
  the std-independent value and ABI foundation.
- Nominal static traits, explicit impls, `where` trait predicates, static trait
  method dispatch, and associated types are part of the M4 baseline.
- Resource semantics, dynamic trait objects, object safety, default methods,
  specialization, associated constants, and generic associated types remain
  separate future design tracks.

## M2.5 Frontend Direction

The M2.5 architecture line is stable-ID-driven query work, not an LSP adapter
layer added first:

- The first query-key batch is now on the default incremental-cache path.
- Existing sema results are explicit typed identities, explicit diagnostic
  kinds, and stable fingerprints.
- File parse, module graph, item signature, function body, generic instance,
  and diagnostics have first-batch query row/edge, replay, and provider-skip
  profile coverage.
- Lossless CST / GreenTree storage preserves trivia while AST remains the
  lowered semantic structure consumed by later stages.
- CLI, JSON, and future IDE consumers share structured diagnostics instead of
  deriving semantics from message text.
