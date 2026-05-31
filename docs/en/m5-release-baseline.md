# Aurex M5 Default Trait Methods Release Baseline

Status: complete.

Date: 2026-05-31.

M5 closes default trait methods as a release baseline on top of the closed M4
trait / protocol system. The stage adds trait-owned default method bodies,
explicit override versus inherited-default origin facts, concrete default-body
instances for static lowering, and tooling / diagnostic / incremental-cache
projection while preserving the M4 static-dispatch-first architecture.

## Release Scope

The M5 language surface is intentionally narrow:

```aurex
trait Reader {
    fn read(self: &Self) -> i32;

    fn is_empty(self: &Self) -> bool {
        return self.read() == 0;
    }
}

struct File {
    value: i32;
}

impl Reader for File {
    fn read(self: &File) -> i32 {
        return self.value;
    }
}

fn check(file: &File) -> bool {
    return file.is_empty();
}
```

The release contract is:

- A trait method requirement may be a prototype or may carry a body.
- A requirement with a body is a defaulted requirement and may be omitted by an
  impl.
- A requirement without a body must still be implemented by every concrete
  impl.
- An impl method with a matching substituted signature is an explicit
  `impl_override`.
- An omitted defaulted requirement is recorded as `trait_default`.
- Generic bodies continue to bind through `param_env` and are reselected at
  instantiation.
- Default bodies are type-checked in trait context with abstract `Self`, trait
  generics, where predicates, and associated-type projections.
- Selected inherited defaults are materialized as concrete
  `TraitDefaultMethodInstanceInfo` records and lowered to internal direct-call
  functions.
- `BodySlotKind::trait_default_method` is the stable body identity for
  trait-owned default bodies and concrete default instances.
- Inherent methods still win before trait methods, and existing M4 ambiguity
  rules still reject ambiguous trait candidates.

## Compiler Boundary Audit

| Boundary | M5 release contract |
| --- | --- |
| Lexer/parser/AST | Trait bodies accept both `fn requirement(...);` and `fn requirement(...) { ... }`; AST nodes distinguish prototype requirements from trait default methods. |
| Query identity | Default method bodies use `BodySlotKind::trait_default_method`; selected default instances carry stable names, symbols, and incremental keys derived from trait, impl, receiver, trait args, and associated-type outputs. |
| Sema | `TraitMethodRequirement` records default-body metadata; `TraitImplInfo` records `impl_override` versus `trait_default`; `TraitMethodCallBinding` records `impl_override`, `trait_default`, or `param_env`; default bodies are checked once in trait context and concrete instances are rechecked under retained side tables only when selected. |
| IR/backend | Raw trait default method items are not lowered as ordinary functions; retained concrete default instances lower as internal static functions and selected calls lower to direct calls. No vtable, trait object ABI, or dynamic dispatch is introduced. |
| Driver/cache/profile | Incremental-cache subjects emit `trait_default_method_instance` rows and body-query edges for selected defaults while avoiding duplicate ordinary `function` rows for synthetic default instances. |
| Tooling/LSP | IDE hover/definition resolves inherited default calls to the trait method requirement and explicit override calls to the impl method; synthetic default instance functions are hidden from visible global symbols. |
| Diagnostics | Default-body errors point at trait source; bad explicit overrides still diagnose requirement mismatch and add a note that a default body does not relax override signatures. |

## Validation Matrix

M5 release validation uses normal repository validation, not temporary
fixtures.

| Area | Coverage |
| --- | --- |
| Parser / AST / dumps | `tests/gtest/frontend/parser_tests.cpp`, `tests/gtest/frontend/ast_dump_tests.cpp` |
| Query body identity | `tests/gtest/query/query_key_tests.cpp` |
| Sema whitebox and checked dumps | `tests/gtest/sema/trait_tests.cpp` |
| IR lowering guards and concrete default body lowering | `tests/gtest/ir/lower_ast_whitebox_tests.cpp` |
| IDE hover/definition/body-query behavior | `tests/gtest/tooling/ide_tooling_tests.cpp` |
| Incremental-cache rows and query edges | `tests/gtest/driver/cli_driver_tests.cpp` |
| Checked-origin fixtures | `tests/samples/checked/traits/trait_default_method_inherited.ax`, `tests/samples/checked/traits/trait_default_method_override.ax` |
| Positive lowering/native samples | `tests/samples/positive/traits/trait_default_method_*.ax` |
| Negative diagnostics | `tests/samples/negative/traits/trait_default_method_*.ax` |
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

The heavier scheduled/manual `make perf-release-threshold` and
`make query-sanitizer` gates remain available for larger release lanes. They
are quality gates and do not expand the M5 language surface.

## Unsupported Matrix

| Feature | M5 status | Reason | Future entry point |
| --- | --- | --- | --- |
| `dyn Trait`, trait objects, object safety | Not supported | Requires object-callability rules for receivers, associated items, default methods, and ABI-stable data/vtable layout. | Dynamic trait / object-safety design. |
| Vtable ABI / dynamic dispatch | Not supported | M5 deliberately keeps inherited defaults as statically selected direct calls. | Dynamic trait ABI design. |
| Specialization / overlapping defaults | Not supported | Requires partial ordering, semver-aware overlap, and dispatch selection rules that are independent of simple default inheritance. | Specialization design. |
| Default associated types | Not supported | Requires associated-type default selection, override rules, equality interaction, and cycle/depth controls. | Associated type default design. |
| Associated constants | Not supported | Requires const-eval authority and associated-item equality/fingerprint rules. | Associated item extension. |
| Generic associated types | Not supported | Requires a stronger solver, higher-ranked reasoning, and projection normalization depth control. | Solver/GAT design. |
| Minimal implementation annotations | Not supported | Requires user-visible dependency sets and diagnostics similar to GHC `MINIMAL`, without changing default inheritance semantics. | Minimal requirement design. |
| Blanket impls / package-level coherence expansion | Not supported | Would change candidate discovery, overlap, and semver boundaries beyond M4/M5 exact impl facts. | Coherence/package design. |
| RAII, `Drop`, `Copy`, move-only values | Not supported | Resource semantics affect method receivers, generic bounds, drop timing, cleanup lowering, and ABI. | Resource semantics design. |
| Borrow checker / lifetimes | Not supported | Requires a separate aliasing and region model. | Resource/borrow design. |
| Swift-style protocol extensions | Rejected for M5 | Extension defaults create static-versus-dynamic dispatch surprises and member identity ambiguity. | No current entry point. |
| Scala/Kotlin mixin state or linearization | Rejected for M5 | Requires state, initialization, `super<Trait>`, and linearization rules outside Aurex static traits. | Class/composition design, if ever selected. |
| C++ arbitrary requires-expressions | Rejected for M5 | Too broad for the current canonical predicate and trait evidence model. | Future constraint-system design. |

## Release Conclusion

M5-WP1 through M5-WP7 are complete. The branch now has a coherent default trait
method baseline:

- WP1 closed research and design.
- WP2 closed syntax, AST, and stable default-body identity.
- WP3 closed trait-context default body type checking.
- WP4 closed impl completeness and explicit method-origin facts.
- WP5 closed static lowering, backend, and monomorphization behavior.
- WP6 closed tooling, diagnostics, and incremental-cache projection.
- WP7 closes release documentation, unsupported boundaries, normal repository
  tests, coverage, query/cache gates, and stress gates.

Post-M5 work must start as a separate design stream. The strongest candidates
are resource semantics, dynamic trait objects, specialization, default
associated types, associated constants, minimal implementation annotations,
package-level coherence, or a stronger trait solver. None of those should
reopen the M5 static default-method baseline.
