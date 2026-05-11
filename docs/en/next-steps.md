# Next Steps

## Branch Principle

The standard library is frozen and removed from the current M2 tree. Do not expand std
or use std samples to prove language features. New features should be validated
with self-contained `.ax` samples first. Restore std only after syntax, types,
ownership, borrow, and drop rules stabilize.

## Priority Route

1. Modern builtin spelling

   Source-level builtin spellings are normalized to `sizeof[T]`,
   `alignof[T]`, `cast[T](x)`, `ptrcast[T](p)`, `bitcast[T](x)`,
   `ptraddr(p)`, `ptrat[T](addr)`, `strptr(s)`, `strlen(s)`, and
   `strraw(data, len)`. The old function-like names are no longer the language
   surface.

2. Value semantics and resource-model redesign

   M2 now removes the M1 `move(...)` / `noncopy struct` experiment instead of
   treating that move-only MVP as the language foundation. First define unified
   rules for ordinary value passing, struct/enum payloads, match payloads, `?`,
   and the current array-containing type restrictions. Then decide how
   copy/drop/ownership re-enter the type system.

3. Drop / destructor design

   Design a language-level drop capability instead of reusing the M1 destructor
   convention. Decide drop order, interaction with early return / break /
   continue / defer, generic `T: Drop` constraints, and diagnostics for owned
   resource types without a release capability.

4. Borrow semantics

   Design shared borrow, mutable borrow, borrowed returns, aliasing rules, and
   lifetime regions. Start with local borrow checking, then expand across
   function signatures.

5. Capability / trait / where

   Replace temporary hardcodes with language mechanisms. Start with `copy T` and
   `drop T`; later add `eq T`, `ord T`, and `hash T`. Candidate syntax:

   ```aurex
   fn clone_or<T>(value: T, fallback: T) -> T where T: Copy
   fn destroy_all<T>(items: *mut T, len: usize) -> void where T: Drop
   ```

6. String primitive

   Keep `str` as the language-level borrowed UTF-8 slice direction, but do not
   restore `String`/`Bytes` std implementations yet. First settle type identity,
   ABI, literals, slice boundaries, and builtin operation boundaries.

7. Unsafe boundary

   Raw pointer dereference, `ptrcast`, `bitcast`, `ptrat`, and `strraw` remain
   ordinary expressions today. M2 should introduce minimal `unsafe` block /
   `unsafe fn` syntax and diagnostics before these operations become part of the
   stable safe surface.

8. Test performance

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
