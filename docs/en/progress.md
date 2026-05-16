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
- M2 baseline generics with `[]` syntax only, including explicit `id[T](x)`
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
make perf
make perf-stress
make perf-ast-stress
```

The test suite covers lexer/parser behavior, CLI/driver behavior, positive and
negative samples, modules, visibility, generics, functions, methods, pattern
matching, error handling, type-system diagnostics, IR lowering, IR verification,
LLVM lowering, native execution, and installed compiler execution.
`tools/bench.py` builds a Release `build-perf` tree and uses Google Benchmark
for frontend hot-path measurements. `make perf` prints the lightweight
JSON-derived Aurex frontend baseline for lexer, lookup-heavy sema, and
generic-instantiation-heavy sema paths plus the AST bulk sema path, then runs a Google Benchmark
process-level comparison against available modern frontend drivers (`clang++`,
`g++`, and `rustc`) without enforcing thresholds yet. `make perf-compare` runs
only the cross-frontend comparison lane.
`make perf-stress` runs `tools/generic_stress.py` and `tools/ast_stress.py`,
generating 200/500/1000/2000 generic-instantiation sources and
10000/50000/100000 AST bulk statement sources, then recording `aurexc --check`
elapsed time plus peak RSS baselines. `make perf-ast-stress` runs only the AST
bulk RSS/time lane. Generic function instance signatures, generic struct/enum
`TypeInfo`, and checked enum case display now keep internal semantic keys and
TypeHandle arguments separate from display names, so `--check` does not format
names such as `id[i32]`, `Box[i32]`, or `Maybe[i32]_some` on the hot path;
checked dumps, IR lowering, and diagnostics format them lazily when output
needs them.
`--check` / checked-dump mode also releases backend-lowering sparse side tables
after each generic function instance is analyzed; IR/native output mode keeps
those tables so codegen behavior stays unchanged.
The AST main path now follows the P0-Perf-4 plan: the driver owns the
parser/module AST and passes a mutable reference through sema and IR lowering,
`SemanticAnalyzer(const AstModule&)` is deleted to prevent implicit whole-tree
copies, `CheckedModule::normalized_ast` no longer keeps an AST snapshot by
default, sema construction no longer reserves `exprs/types` as `size+4096`, and
postfix materialization no longer copies fat `ExprNode` / `TypeNode` values by
value. The 2026-05-15 compact AST storage pass now stores `TypeNode`,
`ExprNode`, `PatternNode`, `StmtNode`, and `ItemNode` as 32-byte compact headers
plus per-kind payload arenas; module loading moves payload-backed nodes before
remapping instead of depending on fat-vector addresses or `.data()` pointer
arithmetic. Sema item-owner lookup is now explicit `ItemId` lookup rather than
address-derived lookup through `items.data()`. The 2026-05-16 performance pass
also moved sema, IR lowering, and AST dump `ExprNode` hot paths to compact views
and direct payload reads, removing compatibility wrappers and literal fat-node
reconstruction from the sema hot path. The same 2026-05-16 line also landed a
reusable global bump allocator behind the syntax-layer `IdentifierInterner`;
AST type/expr/pattern/stmt/item/module/import name-bearing fields now carry
native `IdentId` payload fields, parser/module-loader/postfix writes intern
through the current `AstModule`, and sema typed lookup keys reuse that AST
module interner instead of maintaining a second private interner. Function,
type, value, generic-template, enum-case, method/member, and local-scope lookup
now use `IdentId` typed indexes; string keys remain only at checked semantic
storage, ABI/display, dump, and diagnostic boundaries. The 2026-05-16
performance line then removed the old fat `ExprNode` production type entirely:
parser construction, `AstModule` storage, module-loader append, and postfix
materialization now create compact expression headers plus per-kind payloads
directly. The expression creation path now uses field-level append/set APIs for
name, unary/binary, call, if/block/match, array, postfix-chain, field/index/slice,
struct literal, and cast-like nodes, so the parser no longer builds payload
struct temporaries before arena storage; postfix materialization also rewrites
call/field/index/slice/generic-apply/struct-literal/try expressions directly into
compact payload storage. The follow-up bump pass backs the `TypeNodeList`, `ExprNodeList`,
`PatternNodeList`, `StmtNodeList`, and `ItemNodeList` header vectors and
per-kind payload vectors with `BumpAllocatorAdapter`; the `IdentifierInterner`
text vector and hash table buckets/nodes are also arena-backed. Parser startup
now estimates AST storage from token shape and reserves hot payload arenas up
front with page pre-touch for the expression arena, avoiding parser-time first
touches of fresh pages and repeated bump-vector growth buffers on large modules.
On the local
`tools/ast_stress.py --skip-build --counts 10000,50000,100000` baseline, the
100000 AST bulk statement case moved from roughly 575 MiB RSS / 135 ms to
roughly 158.4 MiB RSS / 74.4 ms. Google Benchmark `sema_ast_bulk/1024` is now
roughly 128 ns/expr, and the local `tools/frontend_compare.py` baseline has
Aurex `--check` at roughly 10.1 ms for lookup/96 and 9.6 ms for generics/96,
versus Clang++ at roughly 21.2 ms / 24.3 ms and G++ at roughly 25.1 ms /
24.3 ms. The current 2000 generic-instance stress case is roughly 124.4 MiB
RSS / 389.8 ms; remaining memory work is in payload-local small vectors and
generic side-table lifetime rather than the main AST header/payload storage.
Cross-module stable hashes / parallel global IDs and CI perf thresholds remain
later performance work.
The follow-up match-exhaustiveness pass replaced the former structural
cartesian-product enumerator and 4096-combination cap with a pattern matrix /
usefulness witness search. Bool, enum payloads, tuples, structs, and fixed
arrays are checked through constructor specialization and default matrices;
guarded arms do not contribute to exhaustiveness, while dynamic slices and open
integer domains still require a wildcard or irrefutable arm under the current
M2 boundary.

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
  conditions. Structural match exhaustiveness now uses pattern matrix /
  usefulness witness search instead of cartesian-product enumeration.
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
