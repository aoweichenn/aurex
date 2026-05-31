# M5 Default Trait Methods Roadmap

## Stage Position

M5 builds on the closed M4 trait / protocol release baseline. M4 gave Aurex
nominal static traits, explicit `impl Trait for Type`, generic trait
predicates, static trait method dispatch, associated types, tooling
projection, diagnostics, coherence, and direct lowering. M5 does not reopen
those decisions. It adds one focused capability: default method bodies inside
traits.

The complete M5 design baseline is
[Aurex M5 Default Trait Methods Research And Design Baseline](m5-default-trait-methods-design.md).

## Goals

1. Reduce impl boilerplate without adding inheritance, mixins, dynamic trait
   objects, or specialization.
2. Preserve M4's static-dispatch-first model.
3. Make method origin explicit for sema, checked dumps, tooling, lowering, and
   diagnostics.
4. Give default bodies stable query identity through
   `BodySlotKind::trait_default_method`.
5. Type-check default bodies once in trait context and instantiate them for
   concrete impls during lowering / monomorphization.
6. Keep future space open for dyn traits, specialization, minimal
   implementation annotations, and resource semantics.

## Non-Goals

- No `dyn Trait`, trait object layout, vtable ABI, witness-table ABI, or object
  safety.
- No specialization, overlapping defaults, blanket impls, negative impls,
  unsafe traits, or auto traits.
- No associated constants, default associated types, GATs, or new solver.
- No RAII, `Drop`, `Copy`, borrow checking, move-only values, or destructor
  lowering.
- No protocol extensions, mixin state, trait linearization, or implicit global
  search.

## Work Packages

### M5-WP1: Research And Design Baseline

Status: active design baseline.

Deliverables:

- English and Chinese design docs.
- English and Chinese M5 roadmap docs.
- Research and comparison across Rust, Swift, Kotlin, Java, C#, Haskell/GHC,
  Scala, Go, C++, and compiler-interface architecture.
- Rejected alternatives, risk matrix, compiler pipeline plan, diagnostics, and
  validation gates.
- Documentation index and integration test updates.

Acceptance:

- Documentation tests require the M5 design and roadmap.
- Format, diff, build, documentation test, and full ctest gates pass.

### M5-WP2: Syntax / AST / Body Identity

Goal: accept default bodies in trait declarations while preserving
prototype-only requirements.

Deliverables:

- Parser accepts `fn name(...) -> T { ... }` inside a trait.
- Parser still accepts `fn name(...) -> T;` as a non-default requirement.
- Malformed trait methods recover without swallowing the next trait item.
- AST / lossless syntax / AST dump expose prototype vs default method state.
- `BodyKey` generation uses `BodySlotKind::trait_default_method`.
- Query-key tests cover stable default method body identity.

Risk controls:

- Do not synthesize impl methods in the parser.
- Do not let default bodies enter ordinary top-level function validation.
- Do not change inherent impl parsing.

### M5-WP3: Default Body Type Checking

Goal: check default method bodies under trait context.

Deliverables:

- `TraitMethodRequirement` records default-body metadata.
- Type checking uses trait `Self`, trait generic params, trait where
  predicates, associated-type projections, and current trait evidence.
- Default body errors point to trait source.
- Default bodies can call other trait requirements through evidence.
- Negative tests cover concrete field lookup on abstract `Self`, return type
  mismatch, missing bound, associated-type ambiguity, and unsupported generic
  method edges.

Risk controls:

- Type-check the trait-owned body once; do not duplicate diagnostics per impl.
- Do not infer minimal method sets or dependency graphs in this package.

### M5-WP4: Impl Completeness And Method Origin

Goal: let impls omit defaulted requirements and record selected origin.

Deliverables:

- Missing non-default requirements remain errors.
- Missing defaulted requirements become inherited defaults.
- Overrides must still match the requirement signature after `Self`, trait arg,
  and associated type substitution.
- `TraitImplInfo` records explicit override vs inherited default by requirement
  ordinal.
- `TraitMethodCallBinding` records `impl_override`, `trait_default`, or
  `param_env`.
- Checked dumps show inherited defaults and selected call origin.

Risk controls:

- A default body never resolves ambiguity between two visible trait candidates.
- Inherent methods still win before trait methods.

### M5-WP5: Lowering / Backend / Monomorphization

Goal: keep default dispatch direct and static.

Deliverables:

- Concrete default calls lower to generated direct functions from the
  trait-owned default body.
- Generic calls remain `param_env` during generic body checking and reselect
  override vs default at instantiation.
- Backend symbol names use stable ids, member keys, canonical type keys, trait
  args, and associated type outputs.
- IR, LLVM, and native tests cover direct default calls, override calls,
  generic default selection, associated type normalization, and inherent-first
  priority.

Risk controls:

- No vtables or trait object ABI.
- No display-string ABI keys.
- No source-body copying into impls.

### M5-WP6: Tooling / Diagnostics / Incremental Reuse

Goal: expose default methods as first-class semantic facts.

Deliverables:

- Hover and definition distinguish override vs default origin for concrete
  calls.
- Rename remains based on the trait method `MemberKey`.
- Workspace semantic index records default method bodies.
- Diagnostics include origin notes for inherited defaults and override
  mismatches.
- Incremental tests prove that editing a default body invalidates default users
  but not override-only call paths.

Risk controls:

- Tooling remains protocol-neutral.
- LSP DTOs do not enter compiler internals.

### M5-WP7: Release Closure

Goal: close M5 as a release-quality static default-method baseline.

Deliverables:

- Release baseline doc, usage notes, version notes, unsupported matrix, and
  roadmap closure.
- Positive and negative samples in normal repository locations.
- Full build, unit, integration, sample-suite, native, coverage, query/cache,
  and stress gates green.
- Future entries documented for dyn traits, specialization, default associated
  types, minimal implementation annotations, and resource semantics.

## Completion Contract

M5 is complete when Aurex can:

- Declare a default method body inside a trait.
- Omit that method in an impl and inherit the default.
- Override that method in another impl.
- Type-check the default body in trait context.
- Call inherited defaults through concrete and generic receivers.
- Lower all selected calls to direct LLVM/native calls.
- Explain selected origin in checked dumps, diagnostics, and tooling.

Anything involving dynamic dispatch, specialization, trait-object callability,
or resource cleanup remains a later stage.
