# Version Document

## M6 Resource, Value Lifetime, And Access Semantics Three-Pass Design-Review Baseline

The active design stage is M6 Resource And Access Semantics. M6-WP1 has closed
three design-review passes. The complete baseline is recorded in the
[Aurex M6 Resource, Value Lifetime, And Access Semantics Research And Three-Pass Design Review Baseline](m6-resource-access-semantics-design.md),
with the execution route in the
[M6 Resource, Value Lifetime, And Access Semantics Roadmap](m6-roadmap.md).

M6-WP1 fixes semantics and implementation order only; it does not claim that
resource semantics are implemented. The next package is M6-WP2 Resource
Classification Scaffold: compiler-owned `Copy`, internal `Discard` /
`NeedsDrop`, structural classification, stable fingerprints, checked dumps,
and diagnostics. Complete borrow checking, lifetime surfaces, partial moves,
`dyn Trait`, regions, async drop, broad array ABI enablement, and
standard-library rebuilding remain deferred.

## M5 default trait methods release baseline

The current documentation baseline is M5 default trait methods release. M5
builds on the closed M4 trait/protocol release baseline and closes one focused
post-M4 stream: default method bodies on nominal static traits.

The M5 design baseline is recorded in
[Aurex M5 Default Trait Methods Research And Design Baseline](m5-default-trait-methods-design.md),
the staged route is recorded in the
[M5 Default Trait Methods Roadmap](m5-roadmap.md), and the release contract is
recorded in
[Aurex M5 Default Trait Methods Release Baseline](m5-release-baseline.md). M5
is scoped to trait-owned default bodies, explicit method origin, impl override
vs inherited-default completeness, static direct-call lowering after
monomorphization, and tooling/diagnostic/query projection.

M5 does not include dynamic trait objects, object safety, vtable ABI,
specialization, associated constants, default associated types, generic
associated types, blanket impls, package-level coherence expansion, class-like
sugar, or resource semantics.

## M4 trait/protocol release baseline

The current implemented trait baseline is M4. M4 builds on the closed M2
language-core-no-std baseline, the M2.5 frontend/query foundation, and the M3
module/generic/query-backed compiler architecture. It closes nominal static
traits, explicit trait impls, generic trait predicates, static trait method
dispatch, associated types, and IDE/tooling/diagnostic projection.

The release contract is recorded in
[Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md). M5 starts
from that baseline rather than reopening M4 WP1-WP8.

## M2 language-core-no-std

M2 was a deliberate contraction after the
failed M1 direction: it stops repairing the M1 standard-library, selfhost, and
system-example track, removes those distractions from the active tree, and
returns the project to language-core design.

M1 has been discarded. It expanded the standard library, host support,
build-tool examples, selfhost experiments, and language semantics at the same
time, before the syntax and type-system rules were stable. M2 no longer treats
M1 std/selfhost/build-tool artifacts as the current baseline.

Completed:

- Removed the `std/` tree and host-c support.
- Removed driver std lookup, import-path injection, support-source linking, and
  related headers/sources.
- Removed `--stdlib`, `--std-backend`, and `--no-stdlib`.
- Removed install rules for `share/aurex/std`.
- Removed std/M1/system examples and std-specific tests.
- Reworked `Result` / `Option` language samples to define local enums.
- Reworked defer/variadic samples to declare local `extern c` boundaries.
- Removed sema hardcodes for `std.core.vec/map/result` ownership constraints.
- Removed the M1 language-level `move(...)`, `noncopy struct`, and
  use-after-move tracking.
- Reduced the sample suite back to language-core coverage.

Current valid baseline:

- The C++20 Stage0 compiler.
- Handwritten lexer/parser and ID-backed AST.
- Sema, Aurex IR, IR verifier, and pass pipeline.
- LLVM backend and clang native output.
- Self-contained positive and negative `.ax` language samples.

Risk controls:

- Keep language-level positive/negative tests for match payloads, `?`, `for`,
  `defer`, and the basic type system.
- Keep native hello, IR/LLVM lowering, and installed compiler execution checks.
- Future resource constraints should be redesigned as a separate
  resource-semantics track rather than std hardcodes.

Deferred:

- Designing a future library layer.
- Designing any future selfhost/Stage1 route.
- Designing any future build-tool/system examples.
- Designing automatic host support linking, if it is still needed.

These should be revisited only after M2 syntax, value semantics, `unsafe`,
slices/strings, and generic constraints are stable. Owned resource libraries
also require the deferred resource-semantics design.
