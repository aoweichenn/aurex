# Architecture Design

## Components

- `src/lex`: lexical analysis.
- `src/parse`: recursive-descent parser.
- `src/syntax`: AST, tokens, module paths, and dumps.
- `src/sema`: name resolution, types, generic instantiation, pattern
  exhaustiveness, and ownership/move checks.
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

Before restoring std, the language needs:

- Capability predicates: `copy T`, `drop T`, then `eq K`, `hash K`.
- Destructor/drop model: destructor shape, automatic drop order, early-return
  behavior.
- Borrow model: shared/mutable borrow, regions, borrowed returns.
- Move-out model: explicit extraction from fields and containers, partial move
  state.
- Trait/where: move generic constraints out of hardcoded checks and into a
  diagnosable language mechanism.
