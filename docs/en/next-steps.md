# Next Steps

## Current Highest Priority: M6 Resource And Access Semantics

Post-M5 design selection is complete. The next stage is the
[Aurex M6 Resource, Value Lifetime, And Access Semantics Research And Three-Pass Design Review Baseline](m6-resource-access-semantics-design.md),
with the execution route recorded in the
[M6 Resource, Value Lifetime, And Access Semantics Roadmap](m6-roadmap.md).

M6-WP1 completed three design-review passes:

1. Cross-language and research evidence: C++, Rust, Swift, Mojo, Move, Zig, Go,
   Hylo, Pony, Verona, Cyclone, Lean, Koka, Roc, Linear Haskell, Idris 2,
   Austral, Carbon, Clang, and related papers.
2. Aurex semantic selection: split `Copy`, `Discard`, `NeedsDrop`, and future
   `MustConsume`; keep the first release to whole-local moves, CFG-sensitive
   initialized state, deterministic cleanup, and generic drop glue.
3. User-case pressure review: regex manual `destroy`, owned strings/vectors,
   files, locks, FFI, overwrite, branches, loops, `?`, patterns, partial
   initialization, self-reference, shared-ownership cycles, and future
   `dyn Trait`.

M6-WP2/WP3 have completed the first implementation batch:
compiler-owned `Copy`, internal `Discard` / `NeedsDrop` / ownership resource
summaries, structural type classification, stable fingerprints, checked-dump
resource summaries, expression owned-use side tables, whole-local move
analysis, reinitialization after moves, and consume-origin diagnostics.

The next implementation package is M6-WP4 Cleanup Obligations, `defer`
Composition, And IR Elaborator. WP4 should add the lexical cleanup-action stack,
cover normal scope exit, overwrite, `return`, `break`, `continue`, and `?`
early return, and lower cleanup obligations to formal IR cleanup nodes or an
equivalent CFG shape. WP4 does not freeze destructor parser spelling, open user
`Drop` bounds, implement aggregate/generic drop glue, or implement the complete
borrow checker; those remain WP5 and M7 responsibilities.

## Closed Background: Post-M5 Design Selection

M5 default trait methods are now closed as a release baseline. M5-WP1 fixed the
[Aurex M5 Default Trait Methods Research And Design Baseline](m5-default-trait-methods-design.md),
the staged route is recorded in the
[M5 Default Trait Methods Roadmap](m5-roadmap.md), and the completed release
contract is recorded in
[Aurex M5 Default Trait Methods Release Baseline](m5-release-baseline.md).
M5-WP2 through M5-WP6 landed syntax / AST / body identity, trait-context
default body checking, impl completeness and method-origin facts, static
lowering, tooling / diagnostics, and incremental-cache projection. M5-WP7
closed usage notes, version notes, unsupported matrix, normal repository
samples, documentation tests, full build/test/coverage gates, query/cache
gates, and stress gates.

The closed M5 target is deliberately narrow:

- Allow a trait method requirement to carry a default body.
- Keep explicit `impl Trait for Type`, nominal identity, coherence, associated
  types, and static dispatch from M4.
- Treat impl methods as explicit overrides when signatures match.
- Treat omitted defaulted requirements as inherited defaults.
- Keep omitted non-default requirements as errors.
- Record method origin explicitly as `impl_override`, `trait_default`, or
  `param_env`.
- Type-check default bodies in trait context and lower selected default calls
  to direct trait-owned default body instances after monomorphization.
- Expose selected default origin in IDE/tooling and incremental-cache query
  records without introducing runtime dispatch.

M5 does not add `dyn Trait`, object safety, vtable ABI, specialization,
associated constants, default associated types, GATs, blanket impls, RAII /
resource semantics, Swift-style protocol extensions, Scala/Kotlin mixins, or
runtime interface dispatch. Those remain separate future design streams.

The next stage should start from a fresh design decision. The strongest
candidates are resource semantics, dynamic trait objects, specialization,
default associated types, associated constants, minimal implementation
annotations, package-level coherence, or a stronger trait solver. None of those
should reopen the M5 static default-method baseline.

The M3 release baseline is closed, and M4 trait/protocol work has completed
WP1, WP2, WP3, WP4, WP5, WP6, WP7, and WP8. M4-WP1 fixed the
[Aurex M4-WP1 Trait / Protocol System Research And Design Baseline](m4-trait-protocol-system-design.md),
with the staged route in the
[M4 Trait / Protocol System Roadmap](m4-roadmap.md). M4-WP2 completed the token,
parser, AST, AST dump, lossless syntax, and query identity scaffolding. M4-WP3
completed query-backed sema support for trait declarations and the impl
registry. M4-WP4 completed coherence / generic predicates:
`CheckedModule` records `TraitPredicate`, `TraitObligation`, `TraitEvidence`,
and `ParamEnvInfo`; `where T: Trait` lowers into predicates; built-in
capabilities also produce compiler-owned built-in trait predicates; generic
instantiation performs candidate rejection; and the trait impl registry now has
canonical coherence fingerprints, orphan rules, and first-pass overlap checks.
M4-WP5 completed static trait method resolution and lowering: inherent methods
win first, trait impl methods do not pollute ordinary method lookup, generic
body trait calls bind through `ParamEnv` as `param_env` call facts, concrete
receivers bind through visible traits plus the impl registry as `impl` direct
calls, and LLVM IR directly calls the concrete impl method after
monomorphization.
M4-WP6 completed the associated type model: trait declarations can declare
`type Item;`, trait impls can assign `type Item = Type;`, `Self.Item` /
generic projections have canonical associated-projection types,
`Trait[Item = Type]` lowers to trait predicates plus equality facts, impl
requirement matching substitutes associated type outputs, and diagnostics cover
ambiguity, cycles, missing bounds, duplicate/missing/unknown associated types,
built-in equality misuse, and unsatisfied equality predicates.
M4-WP7 completed the first IDE/tooling and diagnostics projection over trait
facts: completion after `where T:`, hover/definition for traits, trait methods,
impl methods, and associated types, semantic-token classification, workspace
member indexing, rename identity through `DefKey` / `MemberKey`, LSP adapter
projection without leaking LSP DTOs into compiler internals, and diagnostic
notes for candidate impls, rejected candidates, associated-type equality
mismatches, orphan checks, and overlap locations.
M4-WP8 is complete and records the release contract in
[Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md): docs,
language-surface notes, unsupported matrix, normal repository tests, coverage,
query/cache/profile stress gates, and future entry points now agree on the same
M4 boundary.

The current implemented surface is nominal static traits with default method
bodies. The language keyword is `trait`, and conformance is explicit through
`impl Trait for Type`.
`CheckedModule::traits` records `TraitSignature`, generic parameters,
visibility, and structured requirements. `CheckedModule::trait_impls` records
exact impl facts. Sema covers requirement matching, `Self` substitution, trait
generic parameter substitution, qualified trait references, visibility, trait
generic arity, missing methods, duplicate methods, unknown methods, signature
mismatches, non-trait impl targets, non-named self targets, and duplicate exact
impls, orphan rules, first-pass overlap, and generic candidate rejection. The
associated type surface covers declarations, impl assignments, projection
normalization, equality predicates, method requirement substitution, and checked
facts for trait signatures, impls, and predicates. The default method surface
covers trait-owned bodies, inherited defaults, explicit overrides, static
default instance lowering, IDE origin projection, and incremental-cache rows.
The
tests live in normal repository locations:
`tests/gtest/sema/trait_tests.cpp`,
`tests/samples/positive/traits/trait_impl_registry.ax`,
`tests/samples/positive/traits/trait_predicate_where_generic.ax`,
`tests/samples/positive/traits/trait_method_static_dispatch.ax`,
`tests/samples/positive/traits/trait_method_associated_static_dispatch.ax`,
`tests/samples/positive/traits/trait_method_inherent_precedence.ax`,
`tests/samples/positive/traits/trait_method_function_field_precedence.ax`,
`tests/samples/positive/traits/trait_associated_type_basic.ax`,
`tests/samples/positive/traits/trait_associated_type_where_equality.ax`,
`tests/samples/positive/traits/trait_default_method_*.ax`,
`tests/samples/negative/traits/*.ax`, and
`tests/samples/imports/samplelib/traits.ax`. WP7 tooling coverage lives in
`tests/gtest/tooling/ide_tooling_tests.cpp` and
`tests/gtest/tooling/session_lsp_tooling_tests.cpp`.
M5 release documentation coverage lives in
`tests/gtest/integration/documentation_tests.cpp`.

The next step is no longer M5 implementation. Resource semantics, dynamic trait
objects, package-level coherence, specialization, class-like sugar, default
associated types, minimal implementation annotations, and a stronger trait
solver are post-M5 candidates that need independent design before code changes.

M5 does not include dynamic trait objects or RAII/resource semantics. Dynamic
trait objects, vtable ABI/object safety, associated constants, default
associated types, specialization, generic associated types, minimal
implementation annotations, and the resource system remain outside the current
M5 target. The M5 surface supports identifier trait predicates with
associated-type equality constraints, static default methods, and tooling
projection; qualified where predicates and generic trait predicate arguments
remain future solver work.

## M3 Closure Context

The R5 Compilation Pipeline / Driver Action core is now closed:
`CompilerInvocation`, the `Compiler` facade, `CompilationSession`,
`CompilationPipeline`, `FrontendPipeline`, `LoweringPipeline`,
`BackendPipeline`, `PipelineStage`, the IR pass manager, analysis manager,
profile metadata, diagnostic stage owners, and tooling/profile consumer
contracts are all on the main path while preserving the existing CLI,
diagnostics JSON, profile JSON, incremental-cache, and emit-mode behavior.

M3.0 module-system closure, M3.1 generics closure, M3.2 Query-backed Sema, and
M3.3 Tooling Session And Incremental Sema have all been merged back to `m3`.
M3.4 Real Incremental Sema Execution is now closed on `m3.4`, focused on
turning M3.3 reuse explanation into real incremental sema execution:

- Thread previous snapshot/query context through `ToolingSession`.
- Convert query reuse decisions into executable semantic fact reuse.
- Preserve body-local, signature-local, generic, and module-surface invalidation
  boundaries.
- Update the workspace semantic index by affected fact identity where possible.
- Keep CLI, diagnostics JSON, profile JSON, incremental-cache, and LSP protocol
  behavior compatible unless an explicit work package changes them.
- Keep tests, coverage, query pruning, fuzz, and stress gates green.

The closed M3.4 execution entry point is the
[Aurex M3.4 Real Incremental Sema Execution Plan](m3.4-real-incremental-sema-plan.md).
The wider M3.4-M3.9 route is recorded in the [M3 Roadmap](m3-roadmap.md).
M3.5 is closed in the
[Aurex M3.5 Incremental Syntax And Stable AST Identity Plan](m3.5-incremental-syntax-stable-ast-plan.md).
M3.6 is closed in the
[Aurex M3.6 Project Graph And Persistent Query DB Plan](m3.6-project-graph-persistent-query-db-plan.md).
M3.7 is closed in the
[Aurex M3.7 IDE Semantic Features Plan And Closure Record](m3.7-ide-semantic-features-plan.md),
covering completion, rename, semantic tokens, inlay hints, code actions,
workspace symbols, and LSP projection. M3.8 is now closed in the
[Aurex M3.8 Query-backed Lowering / Backend Reuse Plan And Closure Record](m3.8-query-backed-lowering-backend-reuse-plan.md);
M3.9 is closed in
[Aurex M3.9 M3 Release Baseline And Authority Audit](m3.9-m3-release-baseline.md).
The next work should be designed as a post-M3 stream, not as another M3 feature
slice.

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

M3.5 current completed surface:

1. Range-based document edits: `ToolingDocumentTextEdit`,
   `change_document_range(...)`, and
   `change_document_range_with_reuse_plan(...)` are complete.
2. Syntax stable identity: `LosslessNodeStableKey` is complete, so unchanged
   subtrees are not matched through absolute ranges/token indexes.
3. Syntax reuse stats: `compare_lossless_stable_nodes(...)` and
   `ToolingIncrementalSnapshotResult::syntax_reuse` are complete.
4. AST projection: `IdeAstNodeInfo` / `ToolingAstNode` are complete, projecting
   offsets to AST item/function-body stable `DefKey` / `BodyKey` values.
5. Test coverage: prefix-edit stable syntax keys, range-edit syntax reuse,
   stable AST body keys, and the plain range-edit path are covered.

M3.6 current completed surface:

1. Project/workspace model: `ProjectModel` / `WorkspaceModel`, `ProjectKey`,
   and stable identity are complete.
2. Shared driver/tooling input: CLI invocations and `ToolingSession` share
   package root, source root, import roots, target config, command options, and
   open-buffer inputs.
3. Project graph query: `QueryKind::project_graph`, provider,
   executor/context/provider set wiring, and stable key decoder support are
   complete.
4. Persistent query DB: incremental cache schema v2 writes project inputs and
   persists `project_graph` rows/edges.
5. Invalidation/profile: `incremental_cache.project_inputs` explains
   reuse/reject and changed inputs.
6. Tests/docs: query, driver-cache, tooling workspace model, and M3.6 docs are
   closed.

M3.7 current completed surface:

1. Protocol-neutral completion merges syntax context, sema scope, workspace
   facts, and keywords.
2. Protocol-neutral rename uses symbol identity, identifier/keyword/conflict
   checks, and workspace edit plans.
3. Semantic tokens and inlay hints combine syntax token kinds with checked
   semantic facts.
4. Code actions generate lookup-suggestion quick fixes from structured help
   diagnostics.
5. Workspace symbols and LSP projection cover workspace-index materialization,
   stale-generation guards, and completion/rename/semanticTokens/codeAction/
   workspaceSymbol/inlayHint providers.

M3.8 is complete:

1. WP-1: Function-body lowering query authority.
2. WP-2: Generic-instance lowering query authority.
3. WP-3: Type-layout / enum-layout / ABI-symbol query facts.
4. WP-4: Connect IR pass analysis preservation to query invalidation.
5. WP-5: Split LLVM emission units from target-independent IR units.

M3.9 is complete:

1. The M3.0-M3.8 documentation set is aligned against the final baseline.
2. Public API authority boundaries are recorded in the release-baseline audit.
3. Remaining unsupported/resource/trait/package topics are classified as
   post-M3 non-goals rather than active M3 work.
4. Full tests, coverage, query gates, generic stress, format, and diff checks
   are the final M3 quality baseline.

Post-M4 work should start with design, not implementation, for one of these
tracks:

1. Resource semantics, ownership/drop timing, and ABI impact.
2. Dynamic trait objects, object safety, and vtable ABI.
3. Package/dependency resolver and workspace database beyond the current
   project/query cache boundary.
4. Default trait methods, specialization, and a stronger trait solver.
5. Backend codegen-unit scheduling and multi-target reuse policy.

2026-05-29 M3.3 WP-1/2/3 implementation update: `aurex_tooling` now has a
versioned `ToolingSession`, in-place `IdeSnapshot` cache construction, and a
minimal `LspServer` adapter for JSON-RPC lifecycle, full text sync,
diagnostics, hover, definition, references, and document symbols. WP-4
incremental reuse planning, WP-5 workspace semantic indexing, and WP-6 quality
gates are now complete for this batch.

2026-05-29 M3.4 planning update: M3.3 has been fast-forward merged back to
`m3`, `m3.4` has been created, and the M3.4-M3.9 route is now explicit:
M3.4 real incremental sema execution, M3.5 incremental syntax/stable AST
identity, M3.6 project graph/persistent query DB, M3.7 IDE semantic features,
M3.8 query-backed lowering/backend reuse, and M3.9 release closure.

2026-05-29 M3.4 closure update: `ToolingSession` now preserves previous
materialized snapshots, precise edit impact, and pending workspace facts across
document changes. `IdeIncrementalSnapshotInput` threads previous query records
into snapshot construction, `QueryContext` executes green reuse for unchanged
file/module/signature/body query records, and `ToolingIncrementalSnapshotResult`
exposes the executed reuse plan, reuse counters, and workspace-index update
stats. Focused tests cover accepted/rejected previous context, body-local reuse,
removed-definition invalidation, repeated stable-fact edits, generic body-edit
reuse, malformed reuse plans, and stale-version-free workspace index updates.
That phase is closed; M3.6 project graph and persistent query DB is also
closed, and the next implementation target has moved to M3.7 IDE semantic
features.

2026-05-29 M3.5 closure update: `ToolingSession` supports range-based text
edits. `LosslessNodeStableKey` and `compare_lossless_stable_nodes(...)` turn
syntax subtree reuse into reportable stable-key multiset counters.
`ToolingIncrementalSnapshotResult::syntax_reuse` exposes syntax
reused/recomputed/invalidated counters. `IdeAstNodeInfo` / `ToolingAstNode`
project offsets to AST items/function bodies and expose stable `DefKey` /
`BodyKey` strings. M3.6 project graph and persistent query DB is closed. Next
implementation target: M3.7 IDE semantic features.

2026-05-28 closure update: the original M3.1 work packages have been reviewed
through WP-7 Generic Closure Audit And Release Baseline. The generic release
baseline is now fixed: generic structs, enums, type aliases, functions,
owner-generic methods, and method-local generic methods use
`GenericInstanceKey` / `GenericInstanceIdentity` as the authority for stable
ids, ABI suffixes, incremental keys, query subjects, and checked metadata.
`TypeHandle.value` is allowed only as a session-local lookup/cache fast key.
Display strings, checked dumps, diagnostics, IR dumps, and c_names are outputs,
not semantic identity inputs. Any later generic query-provider deepening,
trait/resource/const-generic design, or LSP/IDE consumption must start from this
M3.1 baseline instead of reopening the closed identity and ABI paths.

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
