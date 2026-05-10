# Version Document

## M2 language-core-no-std

The repository is now in the M2 stage. M2 is a deliberate contraction after the
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
- Future copy/drop constraints should be implemented through capability, trait,
  and `where` design rather than std hardcodes.

Deferred:

- Restoring the standard library.
- Restoring selfhost/Stage1.
- Restoring M1 build-tool/system examples.
- Restoring automatic host support linking.

These should be revisited only after M2 syntax, value semantics, ownership,
borrow/drop, and generic constraints are stable.
