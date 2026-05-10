# Next Steps

## Branch Principle

The standard library is frozen and removed from the current M2 tree. Do not expand std
or use std samples to prove language features. New features should be validated
with self-contained `.ax` samples first. Restore std only after syntax, types,
ownership, borrow, and drop rules stabilize.

## Priority Route

1. Ownership closure

   Unify rules for copy, move, noncopy enum payloads, match payloads, and `?`.
   Add coverage for partial move, field move-out, use-after-move, conditional
   control-flow joins, and loop moved-state diagnostics.

2. Drop / destructor design

   Turn `destroy(self: *mut T) -> void` recognition into a language-level drop
   capability. Decide drop order, interaction with early return / break /
   continue / defer, generic `drop T` constraints, and diagnostics for noncopy
   types without destructors.

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
