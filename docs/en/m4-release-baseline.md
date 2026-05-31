# Aurex M4 Trait / Protocol Release Baseline

Status: complete.

Date: 2026-05-31.

M4 closes the first Aurex trait / protocol system as a release baseline. The
stage adds nominal static traits, explicit trait impls, generic trait
predicates, static trait method dispatch, associated types, and tooling /
diagnostic projection while preserving the M3 query-backed compiler
architecture.

## Release Scope

The M4 language surface is intentionally narrow:

```aurex
trait Source {
    type Item;
    fn get(self: &Self) -> Self.Item;
}

struct Bytes {
    value: i32;
}

impl Source for Bytes {
    type Item = i32;

    fn get(self: &Bytes) -> i32 {
        return self.value;
    }
}

fn read_i32[T](value: &T) -> i32 where T: Source[Item = i32] {
    return value.get();
}
```

The release contract is:

- `trait` is a nominal definition with a stable `DefKey`.
- Conformance is explicit through `impl Trait for Type`.
- Trait impls must satisfy every method and associated type requirement.
- Generic `where T: Trait` clauses lower to canonical trait predicates in the
  current `ParamEnv`.
- `Sized`, `Eq`, `Ord`, and `Hash` keep their compatibility capability checks
  while also producing compiler-owned built-in trait predicate facts.
- Trait method calls use static dispatch. Generic bodies bind through
  `ParamEnv`; concrete receivers bind to a unique visible impl method; lowering
  emits direct calls after monomorphization.
- Associated types are impl outputs, not impl-selection inputs.
- `Trait[Item = Type]` records associated-type equality constraints on the
  trait predicate.
- `Self.Item` and generic associated-type projections lower to canonical
  projection types.

## Compiler Boundary Audit

| Boundary | M4 release contract |
| --- | --- |
| Lexer/parser/AST | `trait`, trait requirements, trait impls, associated type declarations, associated type assignments, and where equalities are structured syntax, not ad hoc string parsing. |
| Query identity | Traits, impls, trait methods, associated types, obligations, evidence, and method-call bindings use stable query keys / member keys. |
| Sema | Trait declarations, impl registry facts, coherence, orphan rules, first-pass overlap checks, predicate lowering, candidate rejection, method resolution, and associated-type equality diagnostics are owned by sema. |
| IR/backend | Static trait method calls lower to concrete impl-method direct calls. No vtable, trait object layout, or drop glue is emitted in M4. |
| Driver/cache/profile | Trait facts participate in checked-module fingerprints and existing query/cache/profile gates without creating a separate trait-specific driver path. |
| Tooling/LSP | IDE and ToolingSession expose protocol-neutral `Ide*` / `Tooling*` values. LSP kinds are mapped only in the LSP adapter. |
| Diagnostics | Candidate impls, rejected candidates, associated-type actual values, orphan locations, and overlap locations are emitted as structured diagnostics/notes. |

## Validation Matrix

M4 release validation is normal repository validation, not temporary fixtures.

| Area | Coverage |
| --- | --- |
| Sema whitebox and checked dumps | `tests/gtest/sema/trait_tests.cpp` |
| Positive samples | `tests/samples/positive/traits/*.ax` |
| Negative samples | `tests/samples/negative/traits/*.ax` |
| Cross-module visibility | `tests/samples/imports/samplelib/traits.ax` and `trait_facade.ax` |
| Static dispatch / LLVM / native execution | positive trait method and associated-type samples through sample-suite checks |
| IDE/tooling/LSP projection | `tests/gtest/tooling/ide_tooling_tests.cpp`, `tests/gtest/tooling/session_lsp_tooling_tests.cpp` |
| Documentation stability | `tests/gtest/integration/documentation_tests.cpp` |

Required closure gates:

```sh
tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')
git diff --check
cmake --build cmake-build-release -j4
ctest --test-dir cmake-build-release --output-on-failure -j4
tools/check_coverage.sh -j4
make perf-stress-threshold
```

The CI release lane also keeps the heavier scheduled/manual
`make perf-release-threshold` and `make query-sanitizer` gates available for
large stress and sanitizer coverage. They are release-quality gates, not a new
M4 language surface.

## Unsupported Matrix

| Feature | M4 status | Reason | Future entry point |
| --- | --- | --- | --- |
| `dyn Trait`, trait objects, object safety | Not supported | Requires data/vtable representation, receiver compatibility, associated-item object rules, and ABI stability. | Dynamic trait/object-safety design. |
| Vtable ABI / dynamic dispatch | Not supported | M4 deliberately uses static dispatch only. | Dynamic trait ABI design. |
| RAII, `Drop`, `Copy`, move-only values | Not supported | Resource semantics affect ownership, drop timing, cleanup lowering, generics, and ABI. | Resource semantics design. |
| Borrow checker / lifetimes | Not supported | Requires a separate aliasing and region model. | Resource/borrow design. |
| Default trait methods | Not supported | Requires method-origin rules, override semantics, conflict diagnostics, and codegen policy. | Default-method design after M4. |
| Associated constants | Not supported | Requires const-eval authority and associated-item equality rules. | Associated item extension. |
| Generic associated types | Not supported | Requires a stronger solver, higher-ranked reasoning, and cycle/depth control. | Solver/GAT design. |
| Specialization | Not supported | Requires semver-aware overlap, partial ordering, and dispatch selection rules. | Specialization design. |
| Negative / unsafe / auto traits | Not supported | Interacts with coherence, safety, resources, and concurrency guarantees. | Trait-safety/resource design. |
| Go-style structural interfaces | Rejected for M4 | Would make conformance implicit and weaken query identity/coherence. | No current entry point. |
| Scala-style implicit/given search | Rejected for M4 | Adds global search and ambiguity pressure before the solver exists. | Explicitly designed contextual evidence system, if ever needed. |
| C++ arbitrary requires-expressions | Rejected for M4 | Too broad for the current canonical predicate model. | Future constraint-system design. |
| Package manager / version solver | Not supported | Orthogonal to the trait language surface. | Package/workspace design. |

## Release Conclusion

M4-WP1 through M4-WP8 are complete. The branch now has a coherent trait /
protocol baseline:

- WP1 closed research and design.
- WP2 closed syntax, AST, and query identity scaffolding.
- WP3 closed trait declarations and the impl registry.
- WP4 closed coherence and generic predicates.
- WP5 closed static trait method resolution and lowering.
- WP6 closed the first associated type model.
- WP7 closed tooling and diagnostics projection.
- WP8 closes release documentation, unsupported boundaries, and validation
  gates.

Post-M4 work must start as a separate design stream. The strongest candidates
are resource semantics, dynamic trait objects, package-level coherence, default
methods / specialization, class-like sugar, or a stronger trait solver. None of
those should reopen the M4 static trait baseline.
