# Current Progress

Version: 0.1.2
Stage: M2 language-core-no-std

## Overall Status

The repository is now in the M2 stage. M2 does not continue the abandoned M1
track. It recenters the project on the language core by removing the standard
library and M1 system examples from the active tree.

M1 was discarded because too many concerns expanded at once: standard library
APIs, host support, build-tool examples, selfhost experiments, resource rules,
and language syntax. The result made it hard to tell whether a failure came from
the language, the library, or the tooling. M2 keeps only the active C++ Stage0
compiler, Aurex IR, LLVM backend, and self-contained language samples as the
valid baseline.

The current tree has no `std/` or `selfhost/` directory. Historical std/selfhost
notes are design input only, not current progress.

## Completed

- CLI support for `--check`, `--dump-*`, `--emit=*`, `--opt-level`, `-I`, `-o`,
  `--clang`, and `--clang-arg`.
- Driver support for file IO, module loading, pipeline orchestration, temporary
  LLVM IR generation, and clang invocation.
- Import resolution through the importer directory and explicit `-I` paths.
- Handwritten lexer/parser with ID-backed AST and dump paths for tokens, AST,
  modules, checked summaries, Aurex IR, and LLVM IR.
- Semantic analysis for types, symbols, functions, ABI names, structs, enums,
  generics, expression types, visibility, and pattern matching.
- M2 baseline generics with `[]` syntax only, including explicit `id::[T](x)`
  calls and non-empty generic parameter/type-argument lists.
- Literal system support for ordinary strings, C strings, raw/multiline raw
  strings, byte strings, byte literals, Unicode scalar `char`, and integer /
  float type suffixes.
- Fixed array value syntax: array literals `[1, 2, 3]` and repeat literals
  `[0; 128]`, including const, struct-field, IR, LLVM, and native paths.
- Tuple basics: tuple types `(A, B)` / `(A,)`, tuple literals `(a, b)` /
  `(a,)`, and tuple destructuring in local `let` / `var` declarations and
  patterns. Anonymous tuple field access is intentionally rejected.
- ADT-first enum basics, including automatic tags, explicit C-like repr enums,
  generic enums, and multi-field payload destructuring in patterns.
- Minimal non-resource generic constraints through `where`, with built-in
  `Sized`, `Eq`, `Ord`, and `Hash` capabilities. Resource capabilities such as
  `Copy` / `Drop` remain deferred.
- Generic type aliases and owner-generic impl blocks such as
  `impl[T] Box[T] { ... }`. Method-local generic parameters remain outside M2.
- Minimal M2 `unsafe` boundaries: `unsafe { ... }`, `unsafe fn`, unsafe
  function pointer types, unsafe call diagnostics, and unsafe-only checks for
  raw pointer dereference, `ptrcast`, `bitcast`, `ptrat`, and `strraw`.
  The no-std checked `str` boundary is now `strvalid(bytes) -> bool` and
  `strfromutf8(bytes) -> str`; failed checked construction returns an empty
  `str` instead of wrapping invalid input as text. `str` also supports checked
  byte-offset slicing with `text[l:r]`; out-of-range bounds or non-UTF-8
  boundary offsets return the empty `str`.
- Default-private visibility, explicit `pub fn` return types, compound
  assignment, unified block-expression bodies, nested block comments, and
  range-only `for i in range(...)`.
- Ordinary root-module `fn main` entry points.
- Typed Aurex IR, IR verifier, conservative pass pipeline, LLVM lowering, and
  native asm/object/executable output through clang.

## Removed From The Active Track

- The `std/` source tree.
- Host C support and implicit support-source linking.
- Driver std lookup and automatic import-path injection.
- `--stdlib`, `--std-backend`, and `--no-stdlib`.
- Install rules for `share/aurex/std`.
- std/M1/system/build-tool examples and std-specific tests.
- std-name resource-semantics hardcodes.
- The previous selfhost / Stage1 / AIR snapshot implementation in this tree.

These can return only after core syntax, module/package rules, `unsafe`,
slices/strings, and generic constraints are stable. Owned resource libraries
also require the deferred resource-semantics design.

## Quality Gates

Use:

```sh
tools/run_tests.sh
tools/bench.py
```

The test suite covers lexer/parser behavior, CLI/driver behavior, positive and
negative samples, modules, visibility, generics, functions, methods, pattern
matching, error handling, type-system diagnostics, IR lowering, IR verification,
LLVM lowering, native execution, and installed compiler execution.

## M2 Gaps

- Enum syntax now supports the M2 ADT-first form: ordinary and generic enums may omit the
  base type and case discriminants, tags are assigned automatically, and
  explicit `enum Status: u8 { ok = 0, err = 1 }` remains available for C-like
  repr enums.
- M2 `unsafe` is intentionally minimal. It is a semantic boundary only and does
  not include borrow checking, lifetimes, unsafe traits, unsafe impl blocks,
  unsafe extern blocks, or an ownership/resource model.
- Slices, checked `str` slicing, tuple basics, function pointer types, and M2 pattern ergonomics are
  implemented in the M2 core. Function types are non-capturing function pointer
  values, including `fn(...) -> T`, `unsafe fn(...) -> T`,
  `extern c fn(...) -> T`, and `unsafe extern c fn(...) -> T`; capturing
  closures are still intentionally out of scope. Pattern support includes tuple
  match patterns, slice patterns, struct patterns, nested enum payload
  destructuring, local struct/slice/enum destructuring, binding or-pattern
  alternatives with same-name/same-type consistency, `let ... else`,
  `if value is pattern` / `while value is pattern`, and `if` expression pattern
  conditions.
- Generics support minimal `where` capability predicates for `Sized`, `Eq`,
  `Ord`, and `Hash`. User-defined traits, associated types, const generics,
  trait objects, and resource capabilities are still outside M2.
- The M1 language-level `noncopy` / `move` MVP has been removed from the M2
  baseline. M2 keeps ordinary value semantics plus the current array-containing
  value restrictions; copy/drop/borrow/ownership are deferred to a later
  resource-semantics design.
- Minimal safe references are implemented: `&T`, `&mut T`, `&place`,
  `&mut place`, safe reference dereference, mutable-reference write checks, and
  pointer-sized ABI lowering. Raw pointer dereference still requires `unsafe`.
  Borrow checking, lifetimes, borrowed-return rules, alias analysis, and
  ownership/resource semantics remain deferred.

## Current Conclusion

M2 should freeze the basic language surface before any future library layer,
selfhost, or build-tool work is designed again. The active compiler can already
validate language-core samples and produce native output, but the next work
should be syntax and semantic stabilization rather than expanding library
surface area.
