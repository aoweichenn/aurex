# M3 Roadmap

## Stage Position

M3 builds on the M2.5 frontend-foundation work. M2.5 closed the query-key,
structured-diagnostic, lossless-syntax, and IDE-native snapshot foundations. M3
first closes the most important language-layer systems, then pushes those facts
down into query-backed sema architecture:

1. Modules.
2. Generics.
3. Query-backed sema and IDE/tooling-consumable semantic facts.
4. Tooling sessions, LSP adapter boundaries, and finer-grained incremental sema.
5. Real incremental sema execution over the query graph.
6. Incremental syntax and stable AST identity.
7. Project graph and persistent query database.
8. IDE semantic features and query-backed lowering closure.

As of 2026-05-25, the R5 Compilation Pipeline / Driver Action core is complete.
As of 2026-05-28, M3.0 module-system closure and the M3.1 generic release
baseline are complete. As of 2026-05-29, M3.2 Query-backed Sema and M3.3
Tooling Session And Incremental Sema have been merged back to `m3`. The next
active line is `m3.4`, focused on turning query reuse explanation into real
incremental sema execution. Every M3 implementation must reuse the R5
`CompilationSession`,
`CompilationPipeline`, `FrontendPipeline`, `LoweringPipeline`,
`BackendPipeline`, `PipelineStage`, query, diagnostics, and profile/tooling
contracts. Do not open a parallel compilation path in the module loader,
parser/sema, or query layers.

M3 explicitly does not implement RAII first, and it does not pull traits,
closures, iterators, derive, package management, or standard-library rebuilds
into this stage. Resource semantics affect ownership, drop timing, IR cleanup,
generic capabilities, and ABI, so they should wait until module and generic
identity are stable.

## Stage Goals

### M3.0: Modules

M3.0 upgrades the current "one file, one module, recursively import and append
into a combined AST" model into a model that separates logical modules from
source-file parts:

```text
PackageKey
  |
  +-- ModuleKey        logical module identity
        |
        +-- ModulePartKey    one source-file part of the module
        |
        +-- ModuleGraph      import / part / dependency edges
        |
        +-- ModuleExports    public API and re-export table
```

Status: M3.0 Phase 9A-D has closed the language-level module-system work. The
first stage only solves same-package module decomposition. It does not add a
package manager, version solver, or external dependency system.

Core deliverables:

- Design and implement module parts so one logical module can span multiple
  files.
- Make all parts of the same logical module share the `priv` visibility
  boundary.
- Preserve the current `import path as alias;` and `pub import` behavior first;
  do not add glob imports in the first slice.
- Align module graph, module exports, item list, and item signature query
  boundaries with `ModuleKey` / `ModulePartKey`.
- Produce precise diagnostics for module parts, import paths, duplicate parts,
  missing parts, and part/import cycles.
- Recover IDE/tooling source-part context from an owning primary when a real
  part buffer is listed by that primary; keep unowned buffers unresolved.
- Include selective re-exports in module graph/export query fingerprints without
  changing local item-list identity.

### M3.1: Generics Completion

M3.1 turns the currently usable generic template/instantiation implementation
into a stable query-backed generic system:

- `GenericTemplateSignature`, `GenericInstanceSignature`, and
  `GenericInstanceBody` become the authoritative boundaries for generic
  checking and reuse.
- Generic ABI suffixes no longer depend on session-only `TypeHandle` numbers or
  display strings. They derive from `GenericInstanceKey` / canonical type
  identity.
- `sizeof[T]` / `alignof[T]` work through sema, IR, and LLVM lowering inside
  generic function bodies.
- Method-local generics move from M2 unsupported into M3 design and
  implementation.
- Generic struct / enum / type alias / function / method visibility, module
  identity, and query invalidation stay consistent.

M3.1 still uses only the current built-in non-resource capabilities: `Sized`,
`Eq`, `Ord`, and `Hash`. User traits, associated types, const generics, and
resource capabilities do not enter M3.1.

The completed M3.1 execution record is the
[Aurex M3.1 Generics Completion Plan](m3.1-generics-plan.md). M3.1 reached its
release baseline at `59a2ddf Complete M3.1 generic closure audit` and has been
merged back to `m3`.

### M3.2: Query-backed Sema

M3.2 moves sema from "one eager analyzer produces a checked module" toward a
query-backed semantic authority:

- `ItemSignature`, `BodySyntax`, `TypeCheckBody`,
  `GenericTemplateSignature`, `GenericInstanceSignature`, and
  `GenericInstanceBody` form one query authority boundary.
- Eager sema may materialize query results, but it must not remain the only
  source of checked semantic facts.
- `CheckedModule` separates durable facts, session-local caches, and
  lowering-only side tables, and each fact class must identify its query
  authority.
- Incremental cache, query pruning, and provider-skip replay can explain sema
  result reuse, not only file-level reuse.
- `aurex_tooling::IdeSnapshot` and future LSP/IDE consumers read query-backed
  semantic facts without bypassing parser/sema/query.
- M3.2 inherits the M3.1 generic release baseline and must not reopen the closed
  identity, ABI, IR/native paths.

M3.2 still does not implement user traits, associated types, const generics,
resource capabilities, RAII, closures, async/iterators, or standard-library
rebuilds. Its execution entry point is the
[Aurex M3.2 Query-backed Sema Design And Execution Plan](m3.2-query-backed-sema-plan.md).

Status: as of 2026-05-29, M3.2 WP-1 through WP-6 are complete. Non-generic
item/body queries now use authority-backed provider inputs and shared result
helpers, and incremental-cache subjects / provider-skip replay can explain
sema-level reuse for item signatures, function body syntax, and type-check
bodies. Lookup/type/generic/body-check service boundaries are now on the sema
pipeline path, and `IdeSnapshot` exposes query-backed semantic facts, records,
and dependency edges.

### M3.3: Tooling Session And Incremental Sema

M3.3 turns the M3.2 `IdeSnapshot` surface into a long-lived tooling layer and a
small LSP-consumable adapter boundary:

- `ToolingSession` owns versioned open-document state, package/source-role
  configuration, and snapshot cache.
- LSP JSON-RPC is an adapter around tooling value types, not a compiler
  internal API.
- Diagnostics, hover, definition, and references consume `IdeSnapshot`,
  `PipelineStageMetadata`, and query-backed semantic facts.
- Incremental reuse planning uses `IdeEditImpact`, query records, and dependency
  edges to explain what an edit invalidates.
- A small workspace semantic index can merge open-file facts before any full
  background index exists.

M3.3 still does not implement full completion, rename, formatting, semantic
tokens, multi-threaded scheduling, remote indexing, package management, user
traits, resource semantics, RAII, closures, or const generics. The execution
entry point is the
[Aurex M3.3 Tooling Session And Incremental Sema Plan](m3.3-tooling-incremental-plan.md).

Status: as of 2026-05-29, M3.3 WP-1 through WP-6 are complete and merged back
to `m3`. Protocol-neutral `ToolingSession`, versioned open-document state,
snapshot cache, minimal `LspServer`, diagnostics, hover, definition,
references, document symbols, reuse planning, workspace semantic indexing, and
quality gates form the M3.3 baseline. Later tooling features must build on this
baseline instead of reading parser, sema, query, or driver internals directly.

### M3.4: Real Incremental Sema Execution

M3.4 turns M3.3's reuse explanation into actual partial recomputation. The
current compiler can record query facts, dependency edges, and reuse summaries;
M3.4 makes `ToolingSession`, `IdeSnapshot`, sema services, and query providers
consume previous snapshot/query context so local edits can reuse unaffected
semantic facts.

The execution entry point is the
[Aurex M3.4 Real Incremental Sema Execution Plan](m3.4-real-incremental-sema-plan.md).

Core deliverables:

- Add previous-snapshot and previous-query-cache input to the IDE/tooling
  snapshot build path without changing CLI semantics.
- Make query reuse decisions executable: unchanged query records must feed the
  next snapshot as reusable semantic facts rather than only appearing in a debug
  explanation.
- Preserve body-local edit behavior: edits inside one function body should
  recompute that body syntax/type-check fact and leave unrelated item
  signatures, bodies, generic instances, module exports, and workspace facts
  reusable.
- Promote signature and module-surface edits to the correct wider invalidation
  roots without falling back to a whole-workspace rebuild.
- Update the workspace semantic index by affected fact identity when possible.
- Add focused correctness, malformed-reuse, stress, profile, coverage, and
  regression gates.

M3.4 still does not implement completion, rename, semantic tokens, background
multi-threaded scheduling, package management, user traits, RAII, closures, or
resource semantics.

### M3.5: Incremental Syntax And Stable AST Identity

M3.5 moves the same modern compiler architecture down to the syntax layer. It
does not add new language syntax; it makes the existing lossless syntax and AST
lowering usable for repeated IDE edits.

Core deliverables:

- Apply range-based text edits in the versioned document store instead of only
  full-text replacement.
- Reuse unchanged lossless syntax subtrees and keep syntax node keys stable
  across local edits.
- Preserve stable AST item/body identity when reparsing unaffected syntax.
- Make parser recovery diagnostics local and deterministic under incomplete
  editor buffers.
- Keep offset-to-token, syntax-node, AST-node, and semantic-fact projections
  aligned for tooling consumers.

This phase adapts rust-analyzer/rowan-style immutable syntax reuse and
SwiftSyntax/Roslyn-style lossless tree discipline to Aurex's existing parser
and AST arenas. It does not replace the parser with a second frontend.

### M3.6: Project Graph And Persistent Query DB

M3.6 promotes module/package identity into a project-level execution model.
M3.0 solved language-level module identity, and M3.3 added open-file indexing;
M3.6 makes CLI checks and tooling sessions share a project graph and a
persistent query database.

Core deliverables:

- Define `ProjectModel` / `WorkspaceModel` inputs: package root, source root,
  import roots, target configuration, command-line options, and open buffers.
- Make source-root, package identity, target configuration, module graph, and
  relevant driver options part of stable cache keys.
- Schedule module graph checks from explicit graph nodes instead of incidental
  recursive loading order.
- Persist semantic query results in a database shape shared by `--check` and
  tooling sessions.
- Expose invalidation and profile output that explains which project inputs
  changed and which query facts were reused.

This phase still does not introduce a package manager, dependency resolver,
lockfile, registry protocol, or version solver.

### M3.7: IDE Semantic Features

M3.7 adds higher-level IDE features only after M3.4 through M3.6 stabilize the
facts they need. The LSP layer remains an adapter around protocol-neutral
tooling value types.

Core deliverables:

- Completion from syntax context, sema scope, visible module exports, and
  checked generic/member facts.
- Rename with symbol identity, visibility checks, conflict detection, and
  cross-file edit planning.
- Semantic tokens from syntax plus checked semantic facts.
- Code actions and quick fixes only for diagnostics that carry structured
  fixable context.
- Inlay hints, workspace symbols, and cross-file references from the workspace
  semantic index.
- Request cancellation/generation handling that prevents stale snapshot results
  from being published.

M3.7 must not let LSP DTOs leak into compiler internals.

### M3.8: Query-backed Lowering, IR, And Backend Reuse

M3.8 extends the query architecture below sema. Checked semantic facts are
already query-backed by M3.2-M3.4; lowering, IR, and backend work still need
explicit fact boundaries so later native builds can reuse unaffected units.

Core deliverables:

- Add query authority for lowering one function body and one generic instance
  body.
- Add type layout, enum layout, ABI symbol, and lower-generic-IR query facts.
- Connect IR pass-manager analysis preservation/invalidation to query
  dependency invalidation where practical.
- Keep target-independent IR units separate from LLVM module/function emission
  units.
- Keep verifier gates, profile metadata, diagnostics ownership, and native
  execution behavior on the existing R5 pipeline path.

This phase adapts LLVM new pass manager analysis preservation and rustc-style
codegen-unit reuse without importing their full compilation-unit complexity.

### M3.9: M3 Closure And Release Baseline

M3.9 is a hardening and closure phase, not a new feature stage.

Core deliverables:

- Align English and Chinese documentation for M3.0 through M3.8.
- Remove stale roadmap entries, obsolete unsupported notes, dead code, and
  unreachable fallback paths discovered by the M3.4-M3.8 work.
- Keep `ctest`, coverage, query-pruning, query-graph fuzz, generic stress,
  module graph stress, incremental-edit stress, native smoke, and performance
  threshold gates green.
- Audit public APIs so parser, sema, query, tooling, LSP, lowering, and backend
  layers do not bypass each other's authority boundaries.
- Record the final M3 release baseline before starting trait/resource/language
  expressiveness work.

## Non-goals

- No RAII, `Drop`, `Copy`, move-only structs, borrow checker, lifetimes, or
  automatic resource cleanup.
- No user traits / protocols, trait objects, associated types, associated
  consts, or dynamic dispatch.
- No closure capture, `Fn` / `FnMut` / `FnOnce`, generators, async, or general
  iterator protocols.
- No standard-library rebuild, package manager, or version solving.
- No implicit directory scanning as module semantics. Filesystem layout may help
  lookup, but it cannot replace explicit language-level module/part
  declarations.

## Module Design Direction

The detailed M3 module design will live in a separate document. The roadmap
fixes these initial directions:

- `module path;` remains the primary module-file declaration.
- `module path part name;` is the recommended part-file declaration.
- The primary file explicitly lists module parts with `part name;`; the list
  prevents implicit directory scanning and hidden file discovery.
- `priv` means visible within the same logical module, including the primary
  file and all parts.
- `pub` continues to mean public API across modules.
- `pub(package)` is part of M3.0 package visibility. `pub(crate)` remains only a
  future spelling question.
- Primary-level `pub use module.Item [as Alias];` and
  `pub(package) use module.Item [as Alias];` are the M3.0 selective re-export
  forms.
- M3.0 still rejects glob import/use, part-local `pub use`, bare/private use,
  nested module trees, `pub(in path)`, workspace/dependency resolvers,
  lockfiles, version solving, and package management.

## Generic Design Direction

The detailed M3 generic design will also live separately. The roadmap fixes
these initial directions:

- Bracket generic syntax stays: `Name[T]` and `fn id[T](value: T)`. The old
  `<T>` syntax does not come back.
- `where T: Sized + Eq` continues to mean built-in non-resource capabilities,
  not user traits.
- Template checking and concrete instance checking must emit structured
  diagnostics instead of surfacing errors only during lowering or backend
  emission.
- Generic instance identity must come from stable keys, not display strings, C
  ABI names, or the current compilation's `TypeHandle` numbers.
- Method-local generics need lookup, inference, ABI, query-key, and diagnostic
  design before implementation.

The M3.1 implementation route is:

1. **1A generic ABI stabilization**: generic struct / enum / function / method
   instance symbols derive from `GenericInstanceKey` and stable fingerprints;
   do not concatenate session-only `TypeHandle.value`.
2. **1B generic instance identity propagation**: record
   `GenericInstanceIdentity`, fingerprint text, stable definition ids, and
   semantic keys on generic struct, enum, type alias, function, and method
   checked metadata. Dumps, lookup, query subjects, and lowering should not each
   rebuild identity independently.
3. **2 generic query authority**: upgrade `GenericTemplateSignature`,
   `GenericInstanceSignature`, and `GenericInstanceBody` from useful records to
   provider authorities. Eager sema materializes or consumes query results
   instead of being the only source of truth.
4. **3 generic body/lowering closure**: retained typed bodies, generic side
   tables, IR lowering, LLVM lowering, native execution, and checked dumps all
   consume the same instance-body view.
5. **4 `sizeof[T]` / `alignof[T]` closure**: give generic-function type
   operands verifiable sema semantics, carry them through IR/LLVM, and keep
   negative cases in sema diagnostics.
6. **5 method-local generics**: implement local generic parameter scope, lookup,
   inference, ABI, query keys, capability constraints, diagnostics, IR/native,
   and incremental-cache behavior.
7. **6 quality gates**: keep gtests, positive/negative samples, native
   execution, generic stress, query pruning, coverage, and full tests green.

## Recommended Implementation Order

The completed M3.0 module, M3.1 generic, M3.2 query-backed sema, and M3.3
tooling orders remain as historical acceptance. The active M3.4 order is:

1. Incremental snapshot build input. Completed.
2. Query record reuse execution.
3. Semantic fact stability for body-local and signature-local edits.
4. Workspace index incremental update.
5. Performance, malformed-reuse, coverage, and stress gates.
6. Documentation and release closure for the phase.

## Current Implementation Progress

2026-05-28: M3.1 Generics Completion has reached the current closure baseline
on the `m3.1` branch. `generic_instance_abi_suffix` now takes a
`GenericInstanceKey` instead of `std::vector<TypeHandle>`, and generic structs,
generic enums, generic type aliases, generic functions, owner-generic methods,
and method-local generic methods compute `GenericInstanceIdentity` before
deriving stable ids, ABI suffixes, instance-signature incremental keys, and
query subjects. `GenericTemplateSignature`, `GenericInstanceSignature`, and
`GenericInstanceBody` are the current query-authority boundaries; retained
bodies, side tables, IR lowering, LLVM lowering, and native execution consume
one instance-body view; generic builtin type operands and value-only builtins
close through sema/IR/LLVM. The WP-7 audit confirms that `TypeHandle.value`
remains only a session-local lookup/cache fast key, while display strings,
checked dumps, diagnostics, IR dumps, and c_names are outputs rather than
reverse generic identity inputs.

2026-05-29: `m3.1` has been fast-forward merged into `m3`, and `m3.2` has been
created for Query-backed Sema design and implementation. The M3.2 design entry
point is now fixed as `m3.2-query-backed-sema-plan.md`; subsequent work should
advance by M3.2 work package instead of rereading the full M3 history.

2026-05-29: M3.2 WP-1 through WP-6 have been fast-forward merged back to `m3`,
and `m3.3` has been created from that closed baseline. The M3.3 design entry
point is `m3.3-tooling-incremental-plan.md`; WP-1 through WP-6 are complete and
merged back to `m3`, forming the `ToolingSession`, LSP adapter, reuse planner,
and workspace semantic index baseline.

2026-05-29: M3.3 WP-1 through WP-6 have been fast-forward merged back to `m3`,
and `m3.4` has been created from that closed baseline. The M3.4 design entry
point is `m3.4-real-incremental-sema-plan.md`; the phase now prioritizes real
incremental sema execution before syntax incrementality, project graph
persistence, advanced IDE features, and query-backed lowering.

2026-05-29: M3.4 WP-1 Incremental Snapshot Build Input is complete.
`ToolingSession` preserves previous materialized snapshots across document
changes, and `ToolingSnapshotHandle` now exposes
`ToolingIncrementalSnapshotResult` for clean builds, cache hits, accepted
previous context, stale/mismatched context, and malformed context.

## Acceptance

M3.0 module acceptance:

- One logical module can be composed from multiple part files.
- Private items, fields, and methods in one part can be accessed from other
  parts of the same module, but not from outside modules.
- Import, public import, module path lookup, qualified type/value lookup, and
  re-export behavior stay deterministic.
- Duplicate modules, duplicate parts, missing parts, part/module name mismatch,
  and dependency cycles have stable diagnostics.
- Module graph/export query keys, dependencies, and invalidation boundaries are
  inspectable.

M3.1 generic acceptance:

- Generic template signatures, generic instance signatures, and generic instance
  bodies have explicit query boundaries.
- Generic ABI suffixes are stable and do not depend on this session's
  `TypeHandle` values.
- `sizeof[T]` / `alignof[T]` work in generic functions through IR/LLVM.
- Method-local generics have positive/negative samples, diagnostics, and
  lowering coverage.
- Owner-generic and method-local generic methods have stable identity, lookup,
  ABI, IR/native behavior in the release baseline.
- Existing generic stress, query pruning, sample suite, and native execution
  tests do not regress.

M3.2 query-backed sema acceptance:

- Sema authority boundaries are explicit for item signatures, body syntax, body
  type checking, and generic template/instance signatures and bodies.
- Eager checked-module materialization consumes or records query facts instead
  of becoming the only semantic fact source.
- Durable checked facts, session-local caches, and lowering-only side tables are
  separated and documented.
- Provider skip, query pruning, and incremental cache traces can explain sema
  reuse at query granularity.

M3.3 tooling/incremental sema acceptance:

- A protocol-neutral `ToolingSession` owns versioned open-document state and
  snapshot cache.
- LSP JSON-RPC handlers consume tooling value types and do not bypass
  parser/sema/query internals.
- Diagnostics, hover, definition, and references reuse `IdeSnapshot`,
  `PipelineStageMetadata`, and query-backed semantic facts.
- Edit-impact and query dependency records explain unchanged, recomputed, and
  invalidated facts.
- JSON-RPC fixture tests, normal gtests, coverage, query pruning, fuzz, and
  stress gates remain green.
- Tooling-facing semantic snapshots consume the same query-backed facts as the
  compiler pipeline.

M3.4 real incremental sema execution acceptance:

- Snapshot construction can consume previous snapshot/query context without
  changing one-shot callers.
- Query reuse decisions are executable and classify facts as reused,
  recomputed, invalidated, or malformed.
- Body-local edits reuse unrelated item signatures, bodies, generic instances,
  module facts, and workspace index entries.
- Signature and module-surface edits widen invalidation only to dependent facts.
- Malformed previous reuse input falls back to recomputation without assertions.
- Tests, coverage, query pruning, edit stress, and relevant generic/module
  stress gates remain green.
