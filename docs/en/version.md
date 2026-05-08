# Version Document

## language-core-no-std

This branch was split from `m1` to freeze the standard library and continue
language-core design.

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
- Reduced the sample suite back to language-core coverage.

Risk controls:

- Keep language-level positive/negative tests for noncopy enums, match payloads,
  `?`, `for`, and `defer`.
- Keep native hello, IR/LLVM lowering, and installed compiler execution checks.
- Future copy/drop constraints should be implemented through capability, trait,
  and `where` design rather than std hardcodes.
