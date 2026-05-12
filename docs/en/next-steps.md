# Next Steps

## Branch Principle

The standard library is frozen and removed from the current M2 tree. Do not expand std
or use std samples to prove language features. New features should be validated
with self-contained `.ax` samples first. Design any future library layer only
after core syntax, types, modules, and ABI boundaries stabilize.

## Priority Route

1. Modern builtin spelling

   Source-level builtin spellings are normalized to `sizeof[T]`,
   `alignof[T]`, `cast[T](x)`, `ptrcast[T](p)`, `bitcast[T](x)`,
   `ptraddr(p)`, `ptrat[T](addr)`, `strptr(s)`, `strblen(s)`, and
   `strraw(data, len)`. The old function-like names are no longer the language
   surface.

2. Value semantics boundary

   M2 now removes the M1 `move(...)` / `noncopy struct` experiment instead of
   treating that move-only MVP as the language foundation. For now, define unified
   rules for ordinary value passing, struct/enum payloads, match payloads, `?`,
   and the current array-containing type restrictions without reintroducing a
   resource model.

3. Resource semantics deferred

   `Copy`, `Drop`, destructors, borrow checking, lifetimes, and move-out are not
   near-term M2 tasks. Reopen them as a separate resource-semantics design only
   after `unsafe`, ADTs, arrays/slices/strings, function types, and patterns have
   settled.

4. Safe reference direction

   Keep `&T` / `&mut T` as the documented direction for separating safe
   references from raw pointers. Borrow checking, lifetimes, and borrowed returns
   remain deferred.

5. Capability / trait / where

   Replace temporary hardcodes with language mechanisms. For this stage, only
   evaluate non-resource constraints such as `Eq`, `Ord`, `Hash`, and `Sized`;
   resource constraints belong to the deferred resource-semantics design.

6. String primitive

   Keep `str` as the language-level borrowed UTF-8 slice direction, but do not
   recreate old `String`/`Bytes` std implementations. First settle type
   identity, ABI, literals, slice boundaries, and builtin operation boundaries.

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

These return only after core syntax, modules, `unsafe`, ADTs, slices/strings, and
generic constraints have stable language-level design and test matrices. Owned
resource libraries additionally require the deferred resource-semantics design.
