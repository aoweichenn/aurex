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

As of 2026-05-25, the R5 Compilation Pipeline / Driver Action core is complete.
As of 2026-05-28, M3.0 module-system closure and the M3.1 generic release
baseline are complete. As of 2026-05-29, M3.2 Query-backed Sema WP-1 through
WP-6 have been merged back to `m3`, and `m3.3` has been created for tooling
sessions, LSP adapter boundaries, and finer-grained incremental sema. Every
M3 implementation must reuse the R5 `CompilationSession`,
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

Status: as of 2026-05-29, the `m3.3` branch has been created and the bilingual
design plan is the active entry point. The first implementation target is WP-1
Tooling Session And VFS Boundary.

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

The completed M3.0 module, M3.1 generic, and M3.2 query-backed sema orders
remain as historical acceptance. The active order starts from M3.3:

1. Tooling session and versioned document store.
2. Snapshot cache and session-level IDE wrappers.
3. LSP JSON-RPC protocol shell.
4. Diagnostics, hover, definition, and references routed through
   `ToolingSession`.
5. Incremental reuse planner over `IdeEditImpact` and query dependency edges.
6. Small workspace semantic index for open files and package-local facts.

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
point is `m3.3-tooling-incremental-plan.md`; the next implementation package is
WP-1 Tooling Session And VFS Boundary.

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
