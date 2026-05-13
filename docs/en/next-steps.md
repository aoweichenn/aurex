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

3. Unsafe boundary

   Minimal M2 `unsafe` is now part of the core language. `unsafe { ... }`
   creates an unsafe context and may be used as either a statement or an
   expression; a tail expression supplies the value, while a block without a
   tail expression has type `void`. `unsafe fn` marks a callable that can only
   be called from an unsafe context. Raw pointer dereference, `ptrcast`,
   `bitcast`, `ptrat`, and `strraw` require an unsafe context.

   This is deliberately not a resource-safety system: no borrow checker,
   lifetimes, unsafe traits, unsafe impl blocks, unsafe extern blocks, or
   ownership model are included in M2.

4. Resource semantics deferred

   `Copy`, `Drop`, destructors, borrow checking, lifetimes, and move-out are not
   near-term M2 tasks. Reopen them as a separate resource-semantics design only
   after `unsafe`, ADTs, arrays/slices/strings, function types, and patterns have
   settled.

5. Safe reference direction

   Keep `&T` / `&mut T` as the documented direction for separating safe
   references from raw pointers. Borrow checking, lifetimes, and borrowed returns
   remain deferred.

6. Tuple and pattern boundary

   Tuple basics are now in M2: `(A, B)` / `(A,)` types, `(a, b)` / `(a,)`
   literals, `value.0` numeric field access, and local tuple destructuring in
   `let` / `var`. Pattern ergonomics now include tuple match patterns, slice
   patterns, struct patterns, nested enum payload destructuring, local
   struct/slice/enum destructuring, binding or-pattern alternatives with
   same-name/same-type consistency, `let ... else`, and `if value is pattern` /
   `while value is pattern` conditions plus `if` expression pattern conditions.

7. Capability / trait / where

   Replace temporary hardcodes with language mechanisms. For this stage, only
   evaluate non-resource constraints such as `Eq`, `Ord`, `Hash`, and `Sized`;
   resource constraints belong to the deferred resource-semantics design.

8. String primitive

   Keep `str` as the language-level borrowed UTF-8 slice direction, but do not
   recreate old `String`/`Bytes` std implementations. First settle type
   identity, ABI, literals, slice boundaries, and the checked/unchecked
   construction boundary. `strraw(data, len)` is already fenced by `unsafe`;
   checked UTF-8 construction remains a later API/design question.

9. Test performance

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
