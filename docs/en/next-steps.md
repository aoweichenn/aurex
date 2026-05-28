# Next Steps

## Current Highest Priority: M3.1 Generics Completion

The R5 Compilation Pipeline / Driver Action core is now closed:
`CompilerInvocation`, the `Compiler` facade, `CompilationSession`,
`CompilationPipeline`, `FrontendPipeline`, `LoweringPipeline`,
`BackendPipeline`, `PipelineStage`, the IR pass manager, analysis manager,
profile metadata, diagnostic stage owners, and tooling/profile consumer
contracts are all on the main path while preserving the existing CLI,
diagnostics JSON, profile JSON, incremental-cache, and emit-mode behavior.

M3.0 module-system closure is complete. The current highest priority moves to
M3.1 in the [M3 Roadmap](m3-roadmap.md):

- `GenericTemplateSignature`, `GenericInstanceSignature`, and
  `GenericInstanceBody` become the authoritative generic checking, reuse, and
  invalidation boundaries.
- Generic ABI suffixes, stable ids, and incremental keys must derive from
  `GenericInstanceKey` / canonical type identity instead of this session's
  `TypeHandle` numbers, display strings, or reverse-engineered C ABI names.
- `sizeof[T]` / `alignof[T]` must work inside generic function bodies through
  sema, IR lowering, and LLVM lowering.
- Method-local generics move from M2 unsupported into M3.1 design and
  implementation, including lookup, inference, ABI, query keys, diagnostics,
  IR/native behavior, and positive/negative samples.
- M3.1 still uses only the current built-in non-resource capabilities:
  `Sized`, `Eq`, `Ord`, and `Hash`. User traits, associated types, const
  generics, resource capabilities, trait objects, and RAII stay out of this
  stage.
- M3.1 work must consume the R5 driver/session/query/diagnostics/pipeline path
  instead of opening bypasses.
- R5 profile/tooling contracts remain the consumer boundary for future profile
  viewers, LSP adapters, and IDE stage views.

The concrete M3.1 execution entry point is the
[Aurex M3.1 Generics Completion Plan](m3.1-generics-plan.md). Future steps
advance one work package at a time, using that document's required files,
allowed scope, forbidden shortcuts, and acceptance gates.

R5.1 through R5.3 split the driver facade, frontend, lowering/backend, and
stage records. R5.4 added the lightweight IR pass manager, `PassResult`,
`PreservedAnalyses`, verifier gate, and pass-pipeline summary. R5.5 added
`ModuleAnalysisManager` with lazy CFG, dominance, and value-use analysis caching
plus invalidation from `PreservedAnalyses`. R5.6 added stable IR verifier
failure context: input, after-pass, and output verifier failures carry
`stage/profile/verifier/pass` context while preserving the original verifier
body and `ErrorCode`; `LoweringPipeline` fills the IR pass pipeline stage names
from `PipelineStageId::ir_pass_pipeline`.
R5.7 added profile JSON stage metadata: driver main-stage phases in
`aurex-profile-v1` now carry an optional `stage` object sourced from
`PipelineStageRecord`, with stage id, input, output, diagnostic ownership, and
cache/query impact. Existing phase `name` values and internal incremental-cache
query sub-events remain unchanged.
R5.8 added parent-stage metadata for cache/query profile sub-events:
`PipelineStage` now records `PipelineProfileSubeventRecord`, so
`incremental_cache.source_stage_reuse` points back to
`incremental_cache.lookup`, while query diff / plan / pruning / provider-eval
point back to `incremental_cache.write`. These sub-events still do not carry a
`stage` object and are not treated as driver main stages.
R5.9 added the first diagnostics-owner directory: `PipelineStage` can now map a
`DiagnosticCategory` back to candidate owner stages. Lexer diagnostics keep both
`tokens.lex` and `module.lex` owners, while parser, module-loader, and sema-like
diagnostics map to `module.parse`, `module.append`, and `sema.analyze`
respectively. The diagnostics text/JSON protocol is unchanged.
R5.10 wired IDE tooling diagnostics to that stage directory: `pipeline_stage.cpp`
is now a lightweight `aurex_pipeline_stage` target shared by driver and
`aurex_tooling`, and `IdeDiagnostic.owner_stages` carries stage
id/profile/input/output/diagnostic/cache-query metadata for later LSP/IDE views.
R5.11 made the stage directory a public read-only API:
`pipeline_stage.hpp` now lives at `include/aurex/driver/pipeline_stage.hpp`,
`PipelineStageMetadata` is the shared metadata projection consumed by the
profile writer and tooling diagnostics, and `aurex_tooling` no longer depends on
the private `src` include root. The profile JSON and diagnostics JSON protocols
remain unchanged.
R5.12 typed the profile recording entry points: `CompilationProfiler::record`
and `ScopedCompilationPhase` can now take `PipelineStageId`, while
incremental-cache sub-events enter the profiler through
`PipelineProfileSubeventId`. Call sites no longer scatter stage profile-name
strings, and the `aurex-profile-v1` fields remain unchanged.
R5.13 added the profile/tooling consumer classification contract:
`pipeline_profile_phase_classification(...)` classifies a profile phase name as
a driver main stage, a profile sub-event, or unknown. The profile JSON writer now
uses that API to emit the existing `stage` / `parent_stage` metadata without
changing the protocol fields. Future profile viewers and LSP/IDE stage views
should consume this classification API instead of carrying their own phase-name
maps.

Next, keep `PipelineStage` as the single profile/cache/query/diagnostics/IDE
stage directory. M3.1 generics, later LSP adapter work, and subtree reparse
must consume the R5 driver/session/query/diagnostics path,
including public `PipelineStageMetadata`,
`pipeline_profile_phase_classification(...)`, `stage` / `parent_stage` profile
metadata, and `IdeDiagnostic` owner-stage metadata, instead of bypassing it.

M3.1 first implementation order:

1. Stabilize generic ABI suffixes: generic struct / enum / function / later
   method instance symbols derive from `GenericInstanceKey`, not
   `TypeHandle.value`.
2. Propagate generic instance identity: record `GenericInstanceIdentity`,
   fingerprints, and semantic keys on generic struct, enum, type alias,
   function, and method metadata instead of letting ABI, dumps, lookup, and
   query subjects each recreate identity.
3. Harden generic query authority: move template signature, instance signature,
   and instance body computation behind query provider boundaries. Eager sema
   should consume or materialize query results.
4. Close generic body/lowering: retained typed bodies, IR lowering, LLVM
   lowering, and native execution consume the same generic instance body and
   side-table view.
5. Complete `sizeof[T]` / `alignof[T]`: add generic-function sema, IR, LLVM,
   and diagnostic coverage for built-in type operands.
6. Implement method-local generics after query/ABI boundaries are stable,
   including positive, negative, checked-dump, IR/native, and incremental-cache
   coverage.
7. Keep the quality gates green: generic gtests, samples, stress, query
   pruning, full tests, and coverage must not regress.

## Branch Principle

The standard library is frozen and removed from the current M2 tree. Do not expand std
or use std samples to prove language features. New features should be validated
with self-contained `.ax` samples first. Design any future library layer only
after core syntax, types, modules, and ABI boundaries stabilize.

## Current M2.5 Route

M2.1 Semantic + Performance + Security Closure is complete. The active stage is
now M2.5 frontend-foundation. M2.5 does not widen std or the language surface
first; it makes the frontend query-safe, lossless-syntax-ready, and
IDE-native-ready:

1. Explicit semantic diagnostic metadata: completed. Sema writes stable
   kind/category/code values when diagnostics are created instead of inferring
   them from message text.
2. Query keys and dependency graph: first batch complete. Stable query keys,
   invalidation boundaries, query row/edge persistence, replay, red-green
   pruning, and provider-skip profile gates are on the default incremental-cache
   path for files, modules, definitions, bodies, generics, and diagnostics.
3. Lossless syntax: complete for the current track. `--dump-lossless` now
   prints a lossless tree with a `source_file` root, top-level declaration
   nodes, direct trivia/eof token leaves, and `block` / delimiter-group nodes.
   It preserves whitespace, line comments, and block comments; the tree supports
   parent/children/token-span traversal, stable node keys, structural
   validation, offset lookup, subtree source reconstruction, and lossless CST ->
   AST parser lowering. Retain-trivia lex fingerprints and build-lossless parse
   fingerprints are wired.
4. IDE-native engineering entry points: complete for the current acceptance
   boundary. `aurex_tooling` exposes an in-memory-buffer `IdeSnapshot` that
   produces the lossless tree, AST, checked module, structured diagnostics,
   file/lex/parse/diagnostics query records, and dependency edges. Offset token,
   hover, top-level definition lookup, same-name identifier references, and
   edit-impact node selection all go through this API. Future LSP adapters
   should consume this layer instead of bypassing parser/sema/query.
5. M3 preparation boundary: M2.5 no longer owns the trait, resource, closure,
   iterator, and related expressiveness topics exposed by the regex audit. It
   keeps `CanonicalTypeKey`, `GenericInstanceKey`, `ModuleKey`, `ModulePartKey`,
   diagnostics, and query boundaries ready for the next stage. Module fragments,
   package visibility, generic backend completion, and method-local generics move
   to M3.

See [M2.5 Roadmap](m2.5-roadmap.md) for the current frontend-foundation plan and
[M3 Roadmap](m3-roadmap.md) for the next module/generic stage.

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
   diagnostics during instantiation and generic body checking. Capability
   satisfaction is distinct from direct operator availability: `f32` / `f64`
   support direct comparison operators but do not satisfy generic `Eq` / `Ord`.
   `Hash` is currently a marker-only admission predicate for `bool`, `char`,
   integer, and pointer types; it does not provide a hash operator, stable hash
   ABI, or user-defined implementations. Resource capabilities, user-defined
   traits, associated types, const generics, trait objects, and protocol-style
   abstraction remain deferred.

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
   `TypeInfo`, and generic template names/keys) now store `InternedText`: source
   names borrow the AST identifier interner, while generated ABI/display names
   use the checked bump-backed interner instead of heap-backed `std::string`
   buffers. ABI symbol validation borrows those strings through `std::string_view`
   keys rather than building a second interner. IR lowering source-local
   lookup also uses interned typed identifiers. The driver now has a concrete
   incremental cache file path: `--incremental-cache <path>` records
   schema/version, root and import paths, source content fingerprints, loaded
   modules, and checked stable/incremental definition rows; `--check` reuses the
   cache only when every recorded source fingerprint still matches. Keep future
   work focused on query-level reuse, lossless syntax, IDE-native incrementality,
   and additional machine profiles for the existing mixed-feature release stress
   gates rather than reintroducing whole-AST copies or per-node heap
   string/vector side tables. The light generic/AST/diagnostic threshold gate is
   available through `make perf-stress-threshold` and CI; the heavier 2M
   high-complexity AST / 5000 generic / 5000 diagnostic release gate is
   available through `make perf-release-threshold`, which now runs Release+LTO
   from `build/perf-lto` by default and records process metrics plus compiler
   phase profile JSON. `make perf-release-lto-threshold` remains a compatibility
   alias for the same gate.

11. Expression type-cache closure

   Contextual expression typing no longer relies on a final-only expression type
   cache. Checked and generic side tables now store `expr_intrinsic_types` for
   context-free expression types, `expr_types` for contextual final types,
   `expr_expected_types` as the final-cache key, and `CoercionRecord` entries for
   explicit adjustments such as contextual integer/float literals, `null` to
   pointer, and slice coercions. Integer/float/null literals, unary/binary
   expressions, slices, array/tuple literals, and if/block/match expressions keep
   intrinsic types separate from final types under an expected type. Internal
   `analyze_expr` dispatch is now split into literal, value, control,
   aggregate, projection, operator, and builtin helpers; binary expression
   analysis separately handles operand contextual typing, mismatch diagnostics,
   literal hazard checks, and operator result recording. Future work here is
   richer coercion categories, not a P0 cache-pollution fix.

## Explicitly Deferred

- std containers, file/dir/process/console APIs.
- M1 frontend / axbuild examples; the M1 track has been discarded and should not
  continue as the current route.
- host support C shims.
- Installed std lookup.

These return only after core syntax, modules, `unsafe`, ADTs, slices/strings, and
generic constraints have stable language-level design and test matrices. Owned
resource libraries additionally require the deferred resource-semantics design.
