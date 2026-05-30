# M4 Trait / Protocol System Roadmap

## Stage Position

M4 builds on the M3 release baseline. M3 closed module identity, generic
instance identity, query-backed sema, tooling sessions, incremental syntax,
project graph, IDE semantic features, and query-backed lowering / backend
reuse. M4 does not reopen those boundaries; it adds the trait / protocol
language layer on top of them.

The first M4 target is **nominal static traits**:

- `trait` is the language keyword.
- `protocol` remains design terminology.
- Conformance is explicit through `impl Trait for Type`.
- Generic bounds are canonical trait predicates.
- Method calls use static dispatch by default and lower to direct calls to
  concrete impl methods after monomorphization.
- Associated types are part of M4's second layer, but they come after the basic
  trait / impl / coherence system.

M4 explicitly does not mix RAII, `Drop`, `Copy`, move-only values, borrow
checking, dynamic trait objects, vtable ABI, class inheritance, closures,
async/generators, derive, macros, or package management into this stage. The
resource system is a separate future design.

The complete design baseline is
[Aurex M4-WP1 Trait / Protocol System Research And Design Baseline](m4-trait-protocol-system-design.md).

## Goals

1. Gradually migrate the current `Sized`, `Eq`, `Ord`, and `Hash` capabilities
   into compiler-known built-in trait predicates.
2. Add user-defined traits, explicit trait impls, and generic trait bounds.
3. Implement coherence / orphan / overlap rules from the first release so
   modules and future packages remain maintainable.
4. Add queryable trait declaration facts, impl registry facts, trait
   obligations, and trait evidence.
5. Preserve the existing query-backed sema, incremental cache, tooling,
   diagnostics, IR lowering, and backend-reuse authority boundaries.
6. Follow with associated types, tooling projection, and release closure.

## Work Packages

### M4-WP1: Research And Design Baseline

Status: complete.

Deliverables:

- Chinese and English design documents.
- Research over Rust, Swift, Kotlin, Go, C++ concepts, Scala, Haskell/GHC,
  MLIR, and relevant papers.
- Class-system comparison and the reason Aurex does not start with class
  inheritance.
- Selected design, rejected alternatives, risk matrix, and work package route.
- README, next-steps, progress, and documentation integration test updates.

Acceptance:

- Documentation tests require the M4 design and roadmap.
- Format, documentation test, and diff checks pass.

### M4-WP2: Syntax / AST / Query Identity Scaffolding

Status: complete.

Goal: add syntax and structured identity only, without full trait sema.

Deliverables:

- Add the `trait` keyword in the lexer.
- Parse the basic shapes of `trait Name { ... }` and `impl Trait for Type {
  ... }`.
- Add trait item payloads and trait impl payloads to the AST.
- Cover trait declarations and impl blocks in AST dumps, lossless syntax, and
  stable syntax identity.
- Let query keys / item signature authority identify trait definitions and impl
  definitions.

Risk controls:

- Parser recovery must not break ordinary item parsing when a trait body is
  incomplete.
- Trait impls and inherent impls must be distinguishable in the AST. Sema must
  not infer the distinction later.

### M4-WP3: Trait Declaration And Impl Registry

Status: complete.

Goal: make trait declarations and impl facts query-backed sema facts.

Deliverables:

- `CheckedModule::traits` owns `TraitSignature` facts with stable trait
  identity, visibility, generic parameters, and structured method
  requirements.
- Trait requirement prototypes do not enter ordinary top-level function /
  prototype validation, so a trait contract is not reported as a missing
  function definition.
- `CheckedModule::trait_impls` provides the first impl-registry fact keyed by
  trait, self type, and trait arguments.
- `impl Trait for Type` performs requirement matching across `Self`, trait
  generic substitution, parameters, return type, unsafe shape, and variadic
  shape.
- Qualified trait references, visibility checks, trait generic arity
  diagnostics, and named impl self-target checks are covered.
- Positive and negative tests are normal repository tests:
  `tests/gtest/sema/trait_tests.cpp`,
  `tests/samples/positive/traits/trait_impl_registry.ax`, and
  `tests/samples/negative/traits/*.ax`.

Risk controls:

- No promised impls or partial impls.
- WP3 only checks duplicate exact impl keys. It does not claim full coherence /
  orphan / overlap coverage.
- Generic trait impl blocks, `where T: Trait`, `ParamEnv` obligations, trait
  method call resolution, lowering/backend direct calls, associated types, and
  built-in capability migration remain WP4/WP5/WP6 work.
- The current trait-argument fingerprint in the impl key is a WP3 registry
  display-stability compromise; WP4 coherence must move impl / predicate
  identity to canonical type identity.
- Built-in primitive trait facts are owned by a compiler provider and cannot be
  forged by user modules.

### M4-WP4: Coherence And Generic Predicates

Status: current next step.

Goal: make trait bounds formal obligations and implement first-pass coherence.

Deliverables:

- `TraitPredicate`, `TraitObligation`, `TraitEvidence`, and predicate lists in
  `ParamEnv`.
- Lower `where T: TraitA + TraitB` to canonical predicates.
- Orphan rules, overlap checks, and candidate rejection diagnostics.
- Migrate `Sized`, `Eq`, `Ord`, and `Hash` to built-in trait predicates while
  preserving existing sample compatibility.

Risk controls:

- M4.0 forbids arbitrary blanket impls.
- The solver must have cycle detection and a depth budget.

### M4-WP5: Static Method Resolution And Lowering

Goal: bind trait method calls from sema through lowering / backend.

Deliverables:

- Inherent methods win first; trait methods resolve through lexical bounds,
  imported traits, and the impl registry.
- Trait calls inside generic bodies bind to evidence.
- Monomorphized calls become direct calls to concrete impl methods.
- IR dump / LLVM / native smoke tests cover trait calls.
- Diagnostics cover ambiguous trait methods, missing bounds, missing impls, and
  method signature mismatch.

Risk controls:

- No vtable emission.
- No trait object layout.
- Trait method resolution is not implicit global search.

### M4-WP6: Associated Type Model

Goal: add associated types after the basic trait system is stable.

Deliverables:

- Trait associated type declarations.
- Impl associated type assignments.
- Canonical types for `Self.Item` / generic projections.
- `Trait[Item = Type]` equality predicates.
- Ambiguity diagnostics and projection-cycle diagnostics.

Risk controls:

- Associated types are impl outputs, not impl-selection inputs.
- No generic associated types.
- No associated constants.

### M4-WP7: Tooling And Diagnostics

Goal: make IDE/tooling consume trait facts instead of sema internals.

Deliverables:

- Completion offers visible traits after `where T:`.
- Hover / definition handles traits, trait methods, impl methods, and associated
  types.
- Semantic tokens classify trait names, trait methods, impl blocks, and
  associated types.
- Rename is based on `DefKey` / `MemberKey`.
- Diagnostic notes show candidate impls, rejection reasons, and orphan /
  overlap locations.

Risk controls:

- LSP DTOs do not enter compiler internals.
- Tooling output remains protocol-neutral value types.

### M4-WP8: Release Closure

Goal: close the M4 trait system as a release baseline that later features can
extend.

Deliverables:

- Documentation, language manual, unsupported matrix, and progress closure.
- Tests, coverage, query pruning, driver cache, tooling, IR/backend, stress, and
  diff gates.
- Trait / associated type release audit.
- Explicit future entry points for the resource system, dynamic traits,
  class-like sugar, default methods, and specialization.

## Non-Goals

- No RAII, `Drop`, `Copy`, resource semantics, borrow checking, lifetimes, or
  move-only structs.
- No `dyn Trait`, trait objects, vtable ABI, object safety, or dynamic dispatch.
- No class inheritance, virtual methods, base-class state, or constructor /
  destructor object model.
- No default methods, associated constants, specialization, negative impls,
  unsafe traits, or auto traits.
- No Go-style structural interfaces, Scala-style implicit / given search, or
  C++ arbitrary requires-expressions.
- No package manager, dependency resolver, lockfile, registry protocol, or
  version solver.

## Current Next Step

M4-WP1, WP2, and WP3 are complete. The current next step is M4-WP4: Coherence
And Generic Predicates.

WP4 must build on the WP3 registry by adding formal `TraitPredicate`,
`TraitObligation`, `TraitEvidence`, and `ParamEnv` boundaries, lowering
`where T: TraitA + TraitB` into canonical predicates, and implementing the
first orphan / overlap / candidate-rejection diagnostics. WP4 also needs to fix
the migration entry point for the current `Sized`, `Eq`, `Ord`, and `Hash`
capabilities as compiler-owned built-in trait providers while preserving
existing sample compatibility.

WP4 still does not include trait method lowering, associated types, dynamic
trait objects, or RAII/resource semantics. Those remain WP5, WP6, and later
resource-system design work.
