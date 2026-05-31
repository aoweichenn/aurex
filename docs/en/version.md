# Version Document

## M4 trait/protocol release baseline

The current repository baseline is M4. M4 builds on the closed M2
language-core-no-std baseline, the M2.5 frontend/query foundation, and the M3
module/generic/query-backed compiler architecture. It closes nominal static
traits, explicit trait impls, generic trait predicates, static trait method
dispatch, associated types, and IDE/tooling/diagnostic projection.

The release contract is recorded in
[Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md). Post-M4
work should start as a separate design stream; resource semantics, dynamic
trait objects, default methods, specialization, associated constants, generic
associated types, package-level coherence, and class-like sugar are not part of
the M4 baseline.

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
