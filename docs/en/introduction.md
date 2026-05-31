# Introduction

Aurex is a systems-language compiler project. The active design baseline is the
**M6 resource, value lifetime, and access semantics implementation baseline**.
M2 froze the standard library and moved active work back to the language core;
M3 closed modules, generics, query-backed sema, tooling, incremental syntax,
and backend reuse; M4 added nominal static traits, explicit trait impls, generic
trait predicates, static trait method dispatch, associated types, and
tooling/diagnostic projection; M5 closed default method bodies on that static
trait model as a release baseline; M6-WP1 closed the three-pass resource and
value-lifetime design review, and M6-WP2/WP3/WP4 now land resource
classification, whole-local move analysis, and cleanup lowering.

M1 has been discarded. It advanced the standard library, host support,
build-tool examples, selfhost experiments, and language semantics at the same
time, before the basic syntax and type rules had a stable baseline. M2 stops
repairing that track and recenters the project on the language foundation.

This branch removes the `std/` source tree, std driver lookup/linking code, std
CLI options, std/M1/system examples, and std-specific tests. Language tests that
need runtime functions declare a local `extern c` boundary instead of relying on
Aurex std wrappers.

Near-term goals:

- Keep the M4 static trait baseline stable through normal repository tests,
  coverage, query/cache/profile gates, and documentation checks.
- Keep the M5 static default-method baseline stable through the same repository
  tests, coverage, query/cache/profile gates, stress gates, and documentation
  checks.
- Follow the M6 route through compiler-owned `Copy`, internal `Discard` /
  `NeedsDrop`, whole-local moves, and deterministic cleanup; defer complete
  borrow checking, partial moves, and lifetime surfaces to a separate stage.
- Defer dynamic trait objects, object safety, specialization, associated
  constants, default associated types, and generic associated types to explicit
  future designs.
