# M6 Resource, Value Lifetime, And Access Semantics Roadmap

## Stage Position

M6 builds on the closed M5 static default-method baseline. It does not reopen
M4/M5 trait-dispatch decisions or combine borrow checking, dynamic trait
objects, standard-library rebuilding, and array ABI enablement into one giant
stage.

The complete design baseline is
[Aurex M6 Resource, Value Lifetime, And Access Semantics Research And Three-Pass Design Review Baseline](m6-resource-access-semantics-design.md).

M6 targets a release-quality resource and value-lifetime baseline:

```text
resource summary
-> whole-local move
-> CFG-sensitive initialized state
-> deterministic cleanup
-> aggregate / generic drop glue
-> tooling / query / cache
-> M7 borrow checker entry
```

## M6-WP1: Three-Pass Research And Design Review

Status: complete.

Deliverables:

- Cross-language evidence matrix: C++, Rust, Swift, Mojo, Move, Zig, Go, Hylo,
  Pony, Verona, Cyclone, Lean, Koka, Roc, Linear Haskell, Idris 2, Austral,
  Carbon, and Clang.
- Research review: regions, linear and quantitative types, reference
  capabilities, RC reuse, formal ownership semantics, and relational borrow
  analysis.
- Aurex four-dimensional resource model: `Copy`, `Discard`, `NeedsDrop`, and
  future `MustConsume`.
- Hard first-release whole-local move boundary.
- Cleanup-action stack, `defer` composition, overwrite order, destructor
  protocol, FFI, and `unsafe` rules.
- User-case and counterexample stress review.
- Chinese and English design docs, roadmap, navigation, and documentation
  integration test.

Risk controls:

- Fix semantics before destructor parser spelling.
- Do not downgrade destruction into an ordinary user `Drop` trait.
- Do not mis-expand M6 into the complete Rust borrow checker.

## M6-WP2: Resource Classification Scaffold

Status: complete.

Deliverables:

- Add compiler-owned `Copy` capability while keeping `Drop` out of user-written
  bounds.
- Add the internal four-axis resource summary: `Copy` / `MoveOnly`,
  `Discard` / `MustConsume`, `Trivial` / `NeedsDrop`, and owned / borrowed /
  raw / shared ownership.
- Propagate structurally or conservatively through builtins, pointers,
  references, slices, `str`, tuples, arrays, structs, enums, generic
  parameters, associated projections, and opaque structs.
- Add stable resource fingerprints and deterministic resource summaries to the
  checked dump.
- Keep user `Drop` bounds rejected until the destructor-protocol surface lands
  separately.

Acceptance:

- Sema whitebox tests cover structural classification, `Copy` capability, and
  fingerprints.
- Checked-dump coverage verifies deterministic resource summaries.
- Normal repository samples cover the positive `where T: Copy` path, and the
  old `Copy` rejection negative sample is removed.
- `Drop` user bounds remain rejected by negative coverage.

## M6-WP3: Owned Use Modes And Whole-Local Move Analysis

Status: complete.

Deliverables:

- Add `owned_copy`, `owned_consume`, `shared_borrow`, `mutable_borrow`, and
  `place_only` expression-use facts to checked side tables.
- Add a focused body move analysis module using iterative CFG construction and
  worklist dataflow for initialized / moved / maybe-moved state.
- Support reinitialization after move.
- Point diagnostics at consume origins.
- Restrict the first release to whole-local moves.

Explicitly rejected with normal negative samples:

- Partial field moves.
- Indexed move-out.
- Consuming payload patterns.
- Non-`Copy` payload `?` until the aggregate-transfer WP proves complete
  cleanup.

## M6-WP4: Cleanup Obligations, `defer` Composition, And IR Elaborator

Status: complete.

Deliverables:

- Add one lexical cleanup-action stack.
- Cover normal scope exit, overwrite, `return`, `break`, `continue`, and `?`
  early return.
- Classify static, dead, and conditional drop.
- Lower drop flags.
- Add formal IR cleanup nodes or equivalent CFG shape.
- Verify glue targets, place types, flags, and double-elaboration invariants.

Risk controls:

- Named locals keep lexical cleanup; no default ASAP destruction.
- Deferred calls evaluate on exit; deferred use-after-move is diagnosed.

## M6-WP5: Destructor Protocol And Aggregate / Generic Drop Glue

Status: complete for the M6 baseline.

Deliverables:

- Reserve stable destructor body identity through `BodySlotKind::destructor_drop`.
- Add stable drop-glue identity from canonical type keys plus resource
  fingerprints.
- Add a target-independent drop-glue planner for struct fields, tuple
  elements, array elements, enum payloads, generic values, and opaque values.
- Freeze structural cleanup order as reverse activation / reverse declaration
  order where applicable.
- Keep custom user destructor syntax closed until the focused parser and
  lowering review can land it without treating destruction as an ordinary
  overloadable trait.

Explicitly deferred beyond the M6 baseline:

- Final destructor parser spelling.
- User-authored destructor bodies and body lowering.
- Partially initialized aggregate rollback codegen.
- Proven non-`Copy` enum-payload transfer and `?` relaxation.

- Destructor overload resolution.
- Explicit user destructor calls.
- Unwind cleanup.
- Managed-global destruction.
- Broad array-containing by-value ABI enablement.

## M6-WP6: Tooling, Query, Cache, And Performance Closure

Status: complete for the M6 baseline.

Deliverables:

- IDE hover exposes `Copy` / `MoveOnly`, `Discard`, `Trivial` / `NeedsDrop`,
  and ownership classification for parameters and locals.
- Generic parameter hover falls back to checked generic-param handles when the
  syntax type side table has no template body type.
- Query identity covers destructor body slots and stable drop-glue keys.
- The LSP stdio entry point `aurex-lsp` is buildable, installed, argument
  parsed, and covered by framed initialize / exit loop tests.
- Focused query, sema, tooling, and LSP regression tests cover the new surface.

Deferred:

- Full cleanup-origin and destructor-definition projection before user
  destructor syntax exists.
- Broad stress and coverage gate automation beyond the focused M6 regression
  lanes.

## M6-WP7: Release Closure And M7 Entry

Status: complete.

Deliverables:

- Usage, version, requirements, progress, next-step, and roadmap docs record the
  M6 implementation baseline and the explicit deferred surface.
- Documentation tests cover the M6 WP5/WP6/WP7 closure strings.
- M7 entry is the CFG-sensitive origin / loan / lifetime checker.
- `dyn Trait`, regions, isolation, async drop, standard-library rebuilding, and
  user destructor syntax remain separate future packages.

## M7 Preview: Borrow And Lifetime Safety

M7 does not use a lexical-only checker as a permanent foundation:

```text
place / projection
-> read | mutate | consume
-> loan origin
-> CFG-sensitive conflict check
-> borrowed-return contract
-> lifetime surface
```

Two-phase borrows, region surfaces, relational solvers, and concurrency
capabilities enter later packages only when real cases require them.

## Per-Package Validation Gates

```sh
tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') \
  $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')
git diff --check
cmake --build cmake-build-release -j4
ctest --test-dir cmake-build-release --output-on-failure -j4
tools/check_coverage.sh -j4
make perf-stress-threshold
make query-sanitizer
```

Large closure:

```sh
make perf-release-threshold
```
