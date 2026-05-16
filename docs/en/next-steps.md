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
   `ptraddr(p)`, `ptrat[T](addr)`, `strptr(s)`, `strblen(s)`,
   `strvalid(bytes)`, `strfromutf8(bytes)`, and `strraw(data, len)`. The old
   function-like names are no longer the language surface.

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

5. Safe references

   Minimal `&T` / `&mut T` references are now part of the M2 core. Keep the
   boundary narrow: references are distinct from raw pointers, `&mut` requires a
   writable place, reference dereference is safe, and raw pointer dereference
   remains unsafe-only. Borrow checking, lifetimes, borrowed returns, alias
   models, and ownership/resource semantics remain deferred.

6. Tuple and pattern boundary

   Tuple basics are now in M2: `(A, B)` / `(A,)` types, `(a, b)` / `(a,)`
   literals, and tuple destructuring in `let` / `var` and patterns. Anonymous
   tuples do not support `.0` / `.1` or `.first` / `.second`; use a named
   struct when field access is required. Pattern ergonomics now include tuple
   match patterns, slice
   patterns, struct patterns, nested enum payload destructuring, local
   struct/slice/enum destructuring, binding or-pattern alternatives with
   same-name/same-type consistency, `let ... else`, and `if value is pattern` /
   `while value is pattern` conditions plus `if` expression pattern conditions.

7. Capability / trait / where

   The minimal M2 mechanism is now in place: `where T: Eq + Hash` supports
   built-in non-resource capabilities `Sized`, `Eq`, `Ord`, and `Hash`, with
   diagnostics during instantiation and generic body checking. Resource
   capabilities, user-defined traits, associated types, const generics, trait
   objects, and protocol-style abstraction remain deferred.

8. String primitive

   Keep `str` as the language-level borrowed UTF-8 slice direction, but do not
   recreate old `String`/`Bytes` std implementations. The M2 core now settles
   type identity, ABI, literals, checked byte-offset slicing, and the
   checked/unchecked construction boundary. `strraw(data, len)` is fenced by
   `unsafe`; checked UTF-8 construction is frozen as no-std builtins:
   `strvalid(bytes) -> bool` and `strfromutf8(bytes) -> str`, where failure
   returns the empty `str` instead of wrapping invalid input as text.
   `text[l:r]` returns `str` only when the byte bounds are ordered, in range,
   and on UTF-8 code point boundaries; scalar/grapheme iteration remains a
   future library-layer topic.

9. Test performance

   Keep the test harness on direct C++ driver calls for cacheable compiler work.
   Separate check/IR/native tests, and only build/run binaries when runtime
   behavior is the actual subject.

10. Frontend storage performance

   The current M2 storage line has closed the main AST/Sema copying and page-fault
   hot spots: parser-owned AST nodes use compact header/payload arenas, lexer
   tokens are returned through bump-backed `TokenBuffer` with one upfront reserve
   and no pre-touch of unwritten estimated slack, checked-module side tables
   store `IdentId` instead of per-node strings, sema persistent side-table /
   lookup-cache buckets are bump-backed, sema lookup maps no longer keep
   parallel string-key fallback paths, and sema value payload lists
   (`FunctionSignature` params/generic args, `StructInfo` fields, `EnumCaseInfo`
   payloads, `TypeInfo` tuple/function/generic args, generic template params,
   and generic constraint buckets) are arena-backed. Persistent sema text fields
   (`FunctionSignature`, `Symbol`, `StructInfo`, `EnumCaseInfo`, `TypeAliasInfo`,
   `TypeInfo`, and generic template names/keys) now store `InternedText` from a
   bump-backed interner instead of heap-backed `std::string` buffers. IR lowering source-local
   lookup also uses interned typed identifiers. Keep future work focused on cross-module
   stable identifiers, measured generic/AST stress thresholds, and CI
   performance thresholds rather than reintroducing whole-AST copies or per-node
   heap string/vector side tables.

## Explicitly Deferred

- std containers, file/dir/process/console APIs.
- M1 frontend / axbuild examples; the M1 track has been discarded and should not
  continue as the current route.
- host support C shims.
- Installed std lookup.

These return only after core syntax, modules, `unsafe`, ADTs, slices/strings, and
generic constraints have stable language-level design and test matrices. Owned
resource libraries additionally require the deferred resource-semantics design.
