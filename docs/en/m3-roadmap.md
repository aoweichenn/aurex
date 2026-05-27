# M3 Roadmap

## Stage Position

M3 builds on the M2.5 frontend-foundation work. M2.5 closed the query-key,
structured-diagnostic, lossless-syntax, and IDE-native snapshot foundations. M3
does not keep widening that infrastructure track; it starts the next language
stage around two systems:

1. Modules.
2. Generics.

As of 2026-05-25, the R5 Compilation Pipeline / Driver Action core is complete.
M3.0 is now the highest priority, but every M3 implementation must reuse the R5
`CompilationSession`, `CompilationPipeline`, `FrontendPipeline`,
`LoweringPipeline`, `BackendPipeline`, `PipelineStage`, query, diagnostics, and
profile/tooling contracts. Do not open a parallel compilation path in the module
loader, parser/sema, or query layers.

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

The first stage only solves same-package module decomposition. It does not add a
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

## Recommended Implementation Order

1. Documentation closure: M2.5 keeps only frontend-foundation work, and M3 owns
   modules and generics.
2. Module AST and parser design: add part information to module declarations and
   decide the primary/part file syntax.
3. ModuleLoader design: allow multiple `ModulePartKey`s under one `ModuleKey`,
   while rejecting duplicate parts, mismatched paths, and cycles.
4. Sema visibility design: `priv` is visible across all parts of one logical
   module; cross-module access still follows imports/re-exports and public API.
5. Module graph/export queries: keep the current combined AST path as an
   implementation detail, but record queries by `ModuleKey` / `ModulePartKey`.
6. Generic ABI stabilization: derive generic instance symbol names and
   incremental keys from `GenericInstanceKey` instead of session handles.
7. Generic query authority: move instance signature/body computation from eager
   sema paths toward provider boundaries.
8. Method-local generics: implement after module and generic query boundaries are
   stable.

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
- Existing generic stress, query pruning, sample suite, and native execution
  tests do not regress.
