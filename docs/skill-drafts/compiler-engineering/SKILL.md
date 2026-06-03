---
name: compiler-engineering
description: Global mandatory skill for compiler and programming-language work. Load and use for compiler, programming language, interpreter, static analysis, IR, optimizer, code generation, diagnostics, runtime, self-hosting, and language tooling tasks. Provides deep design and implementation guidance for lexer/parser/AST/sema/type systems/IR/passes/backend/testing, with special awareness of Aurex project modules.
metadata:
  short-description: Compiler and language engineering
---

# Compiler Engineering

This is a global mandatory skill. Use it for compiler and programming-language design or implementation work. When C++ code is involved, also follow `cpp-project-standards`.

## Project Orientation

For Aurex, start from these module boundaries:

- `include/aurex/lex`, `src/lex`: tokenization, cursoring, trivia, literals, keywords, punctuators.
- `include/aurex/parse`, `src/parse`: token cursor, grammar parsing, recovery, parser parts, source ranges.
- `include/aurex/syntax`, `src/syntax`: AST, AST IDs, modules, AST dumps.
- `include/aurex/sema`, `src/sema`: symbols, types, generics, lookup, declarations, expression/statement checking.
- `include/aurex/ir`, `src/ir`: lowered IR, enum layout, verifier, pass pipeline, IR dumps.
- `include/aurex/backend`, `src/backend/llvm`: LLVM lowering, runtime ABI, module/function/type/value emission.
- `include/aurex/driver`, `src/driver`, `src/cli`: compiler orchestration, module loading, toolchain, invocation.
- `tests/gtest`, `tests/samples`, `examples`, `docs/compiler`, `docs/zh`, `docs/en`: validation, golden/sample behavior, design docs.

Directory decomposition is a first-class compiler invariant. Do not let a module collapse into a flat dumping ground such as `src/<stage>/internal/*.cpp`.
For any substantial compiler stage, `internal/` may exist only as an implementation root containing responsibility subdirectories; do not add files directly under `internal/`.
Split by domain and dataflow role, for example `src/sema/internal/borrow/`, `src/sema/internal/lifetime/`, `src/sema/internal/dropck/`,
`src/sema/internal/place/`, `src/sema/internal/diagnostics/`, `src/sema/internal/pipeline/`, and mirror the same structure in tests where practical.
Apply the same rule to parser, IR, driver, tooling, and backend internals: use directories for lexer/parser phases, facts, solvers, lowering, passes,
diagnostics, adapters, and runtime/backend boundaries instead of mixing unrelated files in one folder.

## Required Workflow

1. **Map the feature through the pipeline**
   - Identify which stages are affected: source text, token, parser grammar, AST, semantic model, type system, checked module, IR, optimization, backend/runtime, diagnostics, CLI, docs, tests.
   - Trace data ownership and invariants across stage boundaries before changing one stage.

2. **Define semantics before syntax**
   - For language features, write the semantic model first: values, types, mutability, ownership/lifetime if relevant, evaluation order, conversions, overload/lookup behavior, generics, visibility, ABI/runtime effects, and error cases.
   - Then design syntax and parsing rules that expose those semantics without ambiguity.

3. **Choose algorithms deliberately**
   - Compare plausible algorithms and data structures by correctness, complexity, memory behavior, diagnostic quality, incremental extensibility, and integration cost.
   - Prefer well-known compiler techniques when they fit: Pratt/precedence parsing, recursive descent with recovery sets, arena/index-based ASTs, scoped symbol tables, unification/constraint solving, CFG/dataflow analysis, SSA-like IR, verifier-enforced invariants, and staged lowering.
   - Do not introduce an advanced technique unless it solves a concrete problem in the current project.

4. **Research nontrivial designs**
   - For parser architecture, type systems, generics, pattern matching, module systems, IR design, optimization, diagnostics, code generation, self-hosting, or runtime ABI, consult primary or mature sources when useful: language specs, compiler docs, LLVM/MLIR docs, academic papers, mature open-source compilers, and industrial implementations.
   - Analyze references in terms of motivation, strengths, weaknesses, constraints, and which ideas should be adopted, adapted, or rejected for Aurex.

5. **Preserve compiler invariants**
   - Each stage should either reject invalid input with diagnostics or produce data satisfying explicit invariants for the next stage.
   - Prefer local invariants plus verifier checks over relying on fragile cross-stage assumptions.
   - Avoid making later stages compensate for missing validation in earlier stages unless recovery or better diagnostics require it.

6. **Keep module boundaries decomposed**
   - Before adding a new compiler file, choose a responsibility subdirectory. If a target `internal/` directory already contains direct files, do not add more direct files there; create or use a role-specific subdirectory.
   - Prefer fact/collector/solver/enforcer/diagnostic/tooling-adapter splits for analyses. Public headers expose stable facts and thin APIs; scratch worklists, traversal state, and diagnostic assembly stay in private `.cpp` or private subdirectories.
   - Use Strategy, Builder, Facade, Adapter, or small value-object patterns only when they reduce real coupling or isolate a changing policy. Do not introduce inheritance hierarchies, service locators, global mutable state, or string DSLs to make the design look formal.

## Stage Guidance

### Lexing

- Keep tokenization deterministic, single-pass where practical, and allocation-light.
- Preserve accurate byte/line/column/source ranges for diagnostics and downstream tools.
- Treat trivia, comments, string/character escapes, numeric literal forms, keywords, and punctuators as explicit design surfaces.
- Test edge cases: EOF, invalid bytes, unterminated literals/comments, maximal munch, ambiguous punctuators, Unicode/text assumptions, and performance on large files.

### Parsing

- Prefer grammar designs that are unambiguous, recoverable, and easy to evolve.
- Keep expression precedence and associativity explicit.
- Design recovery around synchronization sets, delimiter nesting, and high-value diagnostics.
- When splitting parser components, keep ownership of grammar areas clear and avoid a single parser class accumulating unrelated responsibilities.
- Test positive syntax, negative syntax, recovery quality, source ranges, AST shape, and ambiguous constructs.

### AST And Syntax Model

- AST nodes should represent language semantics cleanly, not parser implementation accidents.
- Keep stable IDs/ranges when they support diagnostics, tooling, or later lowering.
- Avoid mixing checked semantic data into raw syntax unless the architecture intentionally uses typed AST nodes.
- Dumps should be deterministic and useful for debugging/golden tests.

### Semantic Analysis And Types

- Define symbol scopes, visibility, imports, shadowing, overloads, generics, inference, conversions, mutability, and lvalue/rvalue/place rules explicitly.
- Prefer clear type representations with canonicalization or interning when equality/performance matter.
- Diagnostics should identify the primary error, relevant notes, and source ranges without cascading noise.
- Test successful checks, invalid programs, ambiguous lookup, cycles, generic instantiation, pattern exhaustiveness when applicable, and regression cases.

### Aurex M7 Origin, Loan, And Lifetime Work

When working on Aurex M7 borrow/lifetime design or implementation, first read
`docs/zh/m7-origin-loan-lifetime-design.md`, `docs/zh/m7-roadmap.md`, `docs/zh/next-steps.md`, `docs/zh/version.md`,
and the current
`src/sema/internal/sema_body_move_analysis.cpp` / `src/sema/internal/sema_statement_analyzer.cpp` borrow paths.

- Treat M6 resource facts, `OwnedUseMode`, whole-local move analysis, cleanup/drop flags, and drop-glue planning as M7 inputs. Do not reimplement or bypass them.
- Model the feature around explicit internal facts: `Place`, `Origin`, `Loan`, `Point`, `BorrowAction`, and `BorrowSummary`.
- Prefer a Polonius-style fact vocabulary with an Aurex-local deterministic worklist/bitset solver before considering a full Datalog engine.
- M7a should stay conservative at the language surface: do not force Rust-style lifetime parameters until internal function summaries, diagnostics, and tooling facts are stable.
- Replace tree-scanning borrow escape special cases with CFG-sensitive loan liveness and projection-aware conflict checking. Do not keep adding ad hoc cases to `BorrowEscapeAnalyzer`.
- Shared and mutable loans must interact with move/write/drop/reinit/cleanup actions. Implicit cleanup drops are borrow-checker actions, not only IR lowering details.
- Projection conflict must be explicit: same place and prefix places conflict; known disjoint struct/tuple fields may be separated; array/slice/index paths stay conservative until proven otherwise.
- Function calls need summaries: parameter origins, returned origin dependencies, receiver/argument access requirements, and conservative unknown handling for extern/generic/trait calls without summaries.
- `BorrowSummary` must support origin dependency sets/subset constraints, not only a single returned origin. Branches, matches, wrappers, and inferred returns can produce multiple candidate origins.
- For inferred-return functions, collect return-carrier facts before assuming the final signature return type is available; solidify summaries after return inference or from expression/carrier types.
- Stage M7 enforcement through collect-only facts, diagnostic-shadow mode, and then enforced diagnostics. Do not remove `BorrowEscapeAnalyzer` until parity tests cover the current borrowed-view escape matrix.
- Keep raw pointer aliasing out of safe borrow proofs. Unsafe/raw alias semantics are a later design layer, not an M7a shortcut.
- Diagnostics must connect the conflict point to the loan creation point, later carrier use, and invalidating action. Suppress cascades after the primary borrow failure.
- Use iterative CFG/dataflow algorithms, not recursive traversals, and expose stable checked facts for query/cache/tooling instead of making IDE/LSP recompute borrow semantics.

### IR And Lowering

- Make IR invariants explicit and enforce them with a verifier.
- Lower from checked semantics, not raw parser assumptions.
- Keep evaluation order, control flow, temporaries, addressability, and side effects explicit.
- IR dumps should support stable golden tests and debugging.
- Optimization passes should declare preconditions, preserved invariants, invalidated analyses, and complexity.

### Backend, Runtime, And ABI

- Keep target-independent IR separate from backend-specific emission details.
- Document ABI decisions: type layout, enum layout, calling convention, symbol naming, module linkage, runtime helpers, memory representation, and error/exit behavior.
- For LLVM, validate generated IR, test constants/types/functions/control flow, and compare generated behavior with source-level expectations.
- Avoid backend shortcuts that encode frontend assumptions without verifier coverage.

### Driver, Modules, And Tooling

- Module loading, file caching, diagnostics, CLI flags, import paths, and native toolchain behavior are part of the compiler contract.
- Keep reproducibility and deterministic output in mind.
- For tooling-facing changes, preserve source ranges and stable names.

## Testing And Validation

- Add tests at the earliest affected stage and at the highest user-visible stage.
- Use unit tests for lexer/parser/sema/IR utilities, golden tests for dumps and diagnostics, integration tests for complete compilation, and execution tests when runtime behavior matters.
- Include negative tests for diagnostics and recovery, not only successful programs.
- For performance-sensitive compiler paths, add or update benchmarks, representative large inputs, or targeted measurement notes.
- Run relevant build, unit, integration, golden, self-host/bootstrap, and coverage commands available in the project. If a command cannot be run, state the blocker.
- For static-analysis reports such as Qodana or clang-tidy, first verify the profile, compile database, source filters, generated/build exclusions, and raw-vs-deduplicated counting. Large report counts often come from profile mismatch, missing CMake/bootstrap context, generated or build artifacts, test helper style churn, or repeated diagnostics from headers. Triage by compiler-stage risk before fixing style-only findings.
- In compiler reports, prioritize findings that can change compiler output or crash behavior: missing returns in backend/lowering switches, maybe-uninitialized semantic or IR views, local stack addresses escaping through context pointers, unreachable lowering/sema branches, unused parse/sema state updates, branch-clone diagnostics, and avoidable copies of AST/IR/type/symbol payloads on hot paths.
- Treat style-heavy report clusters in large semantic modules as a separate cleanup stream. Repeated redundant qualifiers, const opportunities, elements-view/range-algorithm suggestions, and template argument elision should not displace correctness, lifetime, and output-affecting diagnostics.

## Design Output Expectations

When asked for compiler design or language-feature design, provide:

- Current project analysis with relevant file/function references.
- Semantic model and affected pipeline stages.
- Candidate designs and tradeoff comparison.
- Selected design with motivation and rejected alternatives.
- Algorithm/data-structure choices and complexity.
- Diagnostics, tests, coverage, performance, migration, and documentation plan.

Keep the analysis deep enough to support implementation, not just a high-level sketch.
