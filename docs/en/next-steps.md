# Next Steps

## Branch Principle

The standard library is frozen and removed from the current M2 tree. Do not expand std
or use std samples to prove language features. New features should be validated
with self-contained `.ax` samples first. Restore std only after syntax, types,
ownership, borrow, and drop rules stabilize.

## Priority Route

1. Value semantics and resource-model redesign

   M2 now removes the M1 `move(...)` / `noncopy struct` experiment instead of
   treating that move-only MVP as the language foundation. First define unified
   rules for ordinary value passing, struct/enum payloads, match payloads, `?`,
   and the current array-containing type restrictions. Then decide how
   copy/drop/ownership re-enter the type system.

2. Drop / destructor design

   Design a language-level drop capability instead of reusing the M1 destructor
   convention. Decide drop order, interaction with early return / break /
   continue / defer, generic `T: Drop` constraints, and diagnostics for owned
   resource types without a release capability.

3. Borrow semantics

   Design shared borrow, mutable borrow, borrowed returns, aliasing rules, and
   lifetime regions. Start with local borrow checking, then expand across
   function signatures.

4. Capability / trait / where

   Replace temporary hardcodes with language mechanisms. Start with `copy T` and
   `drop T`; later add `eq T`, `ord T`, and `hash T`. Candidate syntax:

   ```aurex
   fn clone_or<T>(value: T, fallback: T) -> T where T: Copy
   fn destroy_all<T>(items: *mut T, len: usize) -> void where T: Drop
   ```

5. String primitive

   Keep `str` as the language-level borrowed UTF-8 slice direction, but do not
   restore `String`/`Bytes` std implementations yet. First settle type identity,
   ABI, literals, slice boundaries, and builtin operation boundaries.

6. Test performance

   Keep the test harness on direct C++ driver calls for cacheable compiler work.
   Separate check/IR/native tests, and only build/run binaries when runtime
   behavior is the actual subject.

## Explicitly Deferred

- std containers, file/dir/process/console APIs.
- M1 frontend / axbuild examples; the M1 track has been discarded and should not
  continue as the current route.
- host support C shims.
- Installed std lookup.

These return only after ownership, borrow/drop, capability, trait, and `where`
have stable language-level design and test matrices.
