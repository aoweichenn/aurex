# Introduction

Aurex is a systems-language compiler project. The current stage is **M2
language-core-no-std**. M2 freezes the standard library and moves the active
work back to the language core: syntax, type checking, generics, sum types,
pattern matching, control flow, `unsafe` boundaries, and the IR/LLVM backend.

M1 has been discarded. It advanced the standard library, host support,
build-tool examples, selfhost experiments, and language semantics at the same
time, before the basic syntax and type rules had a stable baseline. M2 stops
repairing that track and recenters the project on the language foundation.

This branch removes the `std/` source tree, std driver lookup/linking code, std
CLI options, std/M1/system examples, and std-specific tests. Language tests that
need runtime functions declare a local `extern c` boundary instead of relying on
Aurex std wrappers.

Near-term goals:

- Continue refining `for`, `defer`, `?`, and match payload semantics.
- Remove the M1 `move(...)` / `noncopy struct` experiment from the M2 baseline
  and stabilize ordinary value semantics first.
- Defer ownership, borrow checking, move-out, partial move, drop order,
  destructor rules, and resource capabilities to a separate later design.
- Avoid expanding the standard library before syntax and semantics settle.
