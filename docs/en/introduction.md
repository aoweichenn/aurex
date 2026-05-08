# Introduction

Aurex is a systems-language compiler project. The `language-core-no-std` branch
freezes the standard library and moves the active work back to the language
core: syntax, type checking, generics, sum types, pattern matching, control
flow, ownership, future borrow/drop/capability/trait/where design, and the
IR/LLVM backend.

This branch removes the `std/` source tree, std driver lookup/linking code, std
CLI options, std/M1/system examples, and std-specific tests. Language tests that
need runtime functions declare a local `extern c` boundary instead of relying on
Aurex std wrappers.

Near-term goals:

- Continue refining `for`, `defer`, `?`, match payloads, and noncopy ownership.
- Replace temporary ownership special cases with language-level capability
  predicates such as `copy T` and `drop T`.
- Design borrow checking, move-out, partial move, drop order, and destructor
  rules.
- Avoid expanding the standard library before syntax and semantics settle.
