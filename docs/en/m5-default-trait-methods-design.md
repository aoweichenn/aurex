# Aurex M5 Default Trait Methods Research And Design Baseline

## 1. Status And Scope

This document is the M5-WP1 design baseline. It fixes the language semantics,
compiler boundaries, risk constraints, and staged route for default trait
methods before any parser, sema, lowering, or backend implementation work
starts.

Current decision:

- M5 adds **default method bodies on existing nominal static traits**.
- M5 keeps the M4 conformance model: `trait`, explicit
  `impl Trait for Type`, nominal identity, orphan / overlap checks, associated
  types, and static dispatch.
- A trait method with a body is still a requirement. The body is a default
  implementation used only when a conforming impl does not provide an override.
- An impl method with a matching signature overrides the default. An impl that
  omits a non-default requirement remains invalid.
- Dispatch remains static. After monomorphization, a call to an overridden
  method lowers to the impl method, while a call to an omitted default lowers to
  a trait-owned default-method body instantiated for the selected `Self` /
  trait arguments / associated-type outputs.
- Method origin becomes an explicit semantic fact. A call cannot be recorded
  only as "trait method" once defaults exist.

M5 explicitly does not add:

- `dyn Trait`, trait objects, vtable ABI, object safety, or witness-table ABI.
- Specialization, overlapping defaults, negative impls, auto traits, unsafe
  traits, blanket impls, or "most specific" default selection.
- Associated constants, generic associated types, default associated type
  values, higher-ranked bounds, or a new solver.
- RAII, `Drop`, `Copy`, move-only values, borrow checking, destructor lowering,
  or resource capabilities.
- Swift-style protocol extensions, Scala-style mixin linearization, Kotlin /
  Java / C# runtime interface dispatch, or Go-style structural conformance.
- Implicit global instance search or extension methods.

This is deliberately narrow. M4 already established nominal static traits and
associated types. The useful next step is behavior reuse without freezing
dynamic-object ABI or specialization semantics.

## 2. Existing Aurex Baseline

The M4 release baseline provides the required foundation:

- Parser / AST already recognize `trait Name { ... }` and
  `impl Trait for Type { ... }`.
- `TraitSignature` records generic parameters, visibility, associated type
  requirements, and method requirements.
- `TraitImplInfo` records explicit impl facts, associated type assignments, and
  impl methods.
- `TraitPredicate`, `TraitObligation`, `TraitEvidence`, and `ParamEnvInfo`
  model generic trait bounds and evidence.
- `TraitMethodCallBinding` records trait method calls, currently as
  `param_env` or `explicit_impl`.
- Static trait calls are lowered to direct calls after monomorphization.
- Tooling indexes trait definitions, trait methods, impl methods, associated
  types, and rename identity through stable keys.

Important current limitations:

- `TraitMethodRequirement` has only a signature and metadata. It has no
  `BodyKey`, body syntax id, default-body flag, or default-body fingerprint.
- `TraitMethodDispatchKind` has only `param_env` and `explicit_impl`. M5 needs
  to distinguish at least `impl_override`, `trait_default`, and generic
  `param_env` calls whose final origin is selected after instantiation.
- `parse_trait_decl()` calls `parse_fn_decl(...,
  FunctionBodyPolicy::require_prototype)`. Existing parser tests deliberately
  reject a trait method body.
- `validate_trait_impl_block()` reports every missing method. It does not
  check whether the missing requirement has a default body.
- `resolve_impl_trait_method_call()` currently ignores a trait requirement if
  the selected impl omitted the matching method. With defaults, omission may be
  valid and must bind to the trait default method body instead.
- The query layer already has `BodySlotKind::trait_default_method`, but the
  M4 implementation does not materialize or type-check such bodies.

Therefore M5 is not a surface-only parser change. It must add explicit method
origin, stable default-body identity, type-checking under the trait parameter
environment, and lowering rules that preserve M4's direct-call model.

## 3. External Research Findings

### 3.1 Rust Traits

Rust trait methods can provide default bodies, and implementors may override
them. Default bodies can call other trait methods through the trait contract,
which is valuable for small behavior families such as "implement the primitive
operation, inherit the convenience operations".

What Aurex should adopt:

- Body presence, not a new keyword, marks a default method.
- Defaults are still part of the trait requirement set. Override compatibility
  is checked against the same signature.
- A default body is owned by the trait method, not copied textually into every
  impl.
- A default body can call other trait requirements through current trait
  evidence.

What Aurex should avoid in M5:

- Rust's full dyn-compatibility rules. Whether a default method can be called
  through a trait object is not an M5 question.
- Specialization or "default impl" blocks. They require overlap ordering and
  semver rules that are not in the current solver.
- Blanket impl interactions. M4 deliberately forbids arbitrary blanket impls;
  M5 should not re-open that boundary.

Adopted shape for Aurex: Rust-like default method bodies, but static-only and
trait-owned.

### 3.2 Swift Protocols And Protocol Extensions

Swift distinguishes protocol requirements from members supplied only by
protocol extensions. This split is expressive, but it is also one of the most
important warning signs for Aurex: calls to extension-only members can have
different dispatch behavior than calls to true requirements.

What Aurex should adopt:

- A default implementation should be tied to a declared requirement.
- Tooling should expose whether a call resolves to an override or a default.
- Default implementations should be useful for API evolution, but only inside
  a clearly bounded trait contract.

What Aurex should avoid in M5:

- Extension-only methods that look like trait methods but are not requirements.
- Dynamic existential dispatch and witness-table ABI.
- Retroactive behavior injection that changes method lookup without an
  explicit impl fact.

M5 therefore does not add protocol extensions. A method body inside the trait is
the only default-method surface.

### 3.3 Kotlin Interfaces

Kotlin interfaces allow abstract and default methods, but no object state.
When multiple inherited defaults conflict, Kotlin requires an explicit
override and qualified delegation to choose a parent implementation.

What Aurex should adopt:

- Traits remain stateless. Default methods may compute from parameters,
  receiver methods, associated types, constants available through current
  language rules, and helper functions; they do not introduce stored state.
- Conflicts must be explicit. If several visible trait methods are candidates,
  Aurex should keep the M4 ambiguity diagnostic instead of silently picking a
  default.

What Aurex should avoid in M5:

- Interface properties, backing fields, or stateful mixins.
- `super<Trait>.method()` style delegation before explicit qualified trait-call
  syntax exists.

M5 can accept a single selected default. It must reject ambiguous same-name
trait candidates as M4 already does.

### 3.4 Java Default Methods

Java added default interface methods primarily to evolve existing interfaces
without breaking every implementor. The experience shows both the value and
the risk: defaults improve compatibility, but class-vs-interface precedence
and conflicting defaults require detailed rules.

What Aurex should adopt:

- Impl override should win over a trait default.
- Adding a default to an existing trait can be a source-compatible change for
  impls that omitted the method only after M5 is in force.
- Diagnostics need to explain the selected origin.

What Aurex should avoid in M5:

- Runtime dispatch precedence across classes and interfaces. Aurex has no class
  inheritance layer in M5.
- Multiple-inheritance default conflict resolution through runtime hierarchy
  rules. Aurex keeps static ambiguity.

The Java lesson is that defaults are part of language evolution policy, not
only syntax.

### 3.5 C# Default Interface Members

C# default interface members show a more aggressive API-evolution path, with
interface members carrying implementations and "most specific implementation"
rules in the runtime model. This is powerful, but it relies on an object model
and runtime dispatch system Aurex does not have.

What Aurex should adopt:

- Adding a default method can reduce downstream boilerplate.
- Conflict and origin rules must be designed before implementation.

What Aurex should avoid in M5:

- Runtime "most specific" implementation selection.
- Versioning assumptions tied to a VM or dynamic dispatch ABI.

M5 keeps direct calls and compile-time origin selection.

### 3.6 Haskell / GHC Type Classes

Haskell type classes support default method definitions, and instances can
override them. GHC's `MINIMAL` pragma exists because a class with several
interdependent defaults can otherwise make it unclear which methods an
instance must implement to be meaningful.

What Aurex should adopt:

- Defaults should be permitted to call other required methods through evidence.
- The implementation checker must still guarantee every non-default method is
  implemented.
- The design should leave room for a later minimal-implementation annotation.

What Aurex should avoid in M5:

- Inferring complex minimal method sets from default dependency graphs.
- Allowing default cycles to become compile-time expansion cycles.

M5's first rule is simple: a requirement with a body may be omitted; a
requirement without a body must be implemented. Recursive defaults are ordinary
function recursion, not macro expansion. A future lint can detect likely
infinite default recursion.

### 3.7 Scala Traits And Mixins

Scala traits can contain concrete methods and can be mixed into classes, with
linearization rules deciding method resolution order. This gives powerful
composition, but also makes behavior depend on mixin order and class hierarchy.

What Aurex should adopt:

- Concrete reusable behavior inside an interface-like abstraction is valuable.

What Aurex should avoid in M5:

- Trait parameters, initialization order, fields, or mixin linearization.
- Stacking defaults from several traits and asking the compiler to pick by
  order.

Aurex traits stay behavior contracts plus optional default bodies. They are not
mixin classes.

### 3.8 Go Interfaces

Go interfaces are structural and do not carry default implementations. This is
an instructive rejection case for Aurex: once conformance is implicit, adding a
method to an interface can silently change which types match it.

Aurex keeps nominal explicit conformance. Default methods are attached to a
declared trait member and selected through an explicit impl fact or generic
trait predicate.

### 3.9 C++ Concepts, CRTP, And Inheritance

C++ has several ways to express reusable behavior: virtual inheritance,
abstract base classes with default methods, CRTP mixins, and C++20 concepts.
Each solves a real problem, but each also carries costs: object layout, virtual
destructors, fragile base classes, template diagnostics, and overload /
partial-ordering complexity.

Aurex should not use M5 to import class inheritance or arbitrary
requires-expressions. Default trait methods are a smaller and more local
reuse mechanism: they reuse checked behavior inside a nominal trait contract,
without subtyping or vtables.

### 3.10 Compiler-Architecture Lessons

LLVM/MLIR style compiler infrastructure reinforces a key implementation rule:
interfaces and models should be explicit, queryable facts. For Aurex this
means:

- The trait signature is the owner of a default method body.
- The impl registry records whether an impl provides an override.
- Method-call binding records the selected origin.
- Lowering consumes checked origin facts rather than redoing name lookup.
- Query identities must use `DefKey`, `MemberKey`, `BodyKey`, canonical type
  keys, and `ParamEnvKey`, not display strings.

## 4. User-Visible Language Model

### 4.1 Basic Syntax

M5 allows a body on a trait method requirement:

```aurex
trait Reader {
    fn read(self: &Self) -> i32;

    fn is_empty(self: &Self) -> bool {
        return self.read() == 0;
    }
}
```

An impl may omit `is_empty` because it has a default:

```aurex
impl Reader for FileReader {
    fn read(self: &FileReader) -> i32 {
        return self.value;
    }
}
```

An impl may override it:

```aurex
impl Reader for CachedReader {
    fn read(self: &CachedReader) -> i32 {
        return self.value;
    }

    fn is_empty(self: &CachedReader) -> bool {
        return self.cached_empty;
    }
}
```

No new keyword is required. The presence of a function body distinguishes a
defaulted requirement from a prototype-only requirement.

### 4.2 Requirement And Default Body

Every trait method has:

- A requirement signature: name, parameters, return type, unsafe / variadic
  shape, self-parameter shape, visibility, stable member key, and ordinal.
- An optional default body: source range, body syntax key, body fingerprint,
  body slot, and checked body result.

The default body does not change the requirement signature. Override matching
uses the same M4 signature rules after substituting `Self`, trait arguments,
and impl associated type outputs.

### 4.3 Impl Completeness And Override Rules

For each trait method requirement:

- If the impl provides a matching method, that method is the override.
- If the impl provides a method with the same name but incompatible signature,
  report the existing signature mismatch diagnostic.
- If the impl omits the method and the requirement has a default body, the impl
  is complete for that method and inherits the trait default.
- If the impl omits the method and the requirement has no default body, report
  missing method.
- Duplicate impl methods remain an error.
- Unknown impl methods remain an error.

The impl registry should record both explicit overrides and inherited defaults.
It should not need to synthesize source AST nodes for inherited defaults.

### 4.4 Dispatch And Method Origin

M5 introduces an explicit method-origin model:

| Origin | Meaning | Lowering target |
| --- | --- | --- |
| `impl_override` | The selected impl provides the method body. | Existing impl method function symbol. |
| `trait_default` | The selected impl omitted the method and the trait requirement has a default body. | Trait-owned default body instantiated for selected impl environment. |
| `param_env` | Generic body has only trait evidence at check time. | Re-selected at monomorphization to `impl_override` or `trait_default`. |

Inherent methods continue to win before trait methods. If multiple trait
candidates have the same call shape, M4 ambiguity rules remain in force. M5
does not use the presence of a default body to break ambiguity.

### 4.5 Type Checking Default Bodies

A default body is checked in the trait context:

- `Self` is the implicit implementing type parameter.
- Trait generic parameters are visible.
- The current trait predicate is available as evidence, so the body can call
  other requirements on `Self`.
- Associated type projections such as `Self.Item` use the M4 associated-type
  rules.
- Where constraints on the trait enter the default body's parameter
  environment.
- The default body cannot assume fields of a future concrete impl type.
- The default body cannot call an impl-only helper unless it is reachable
  through normal name lookup and does not depend on a concrete `Self` field.

Errors in the default body point to the trait declaration, not to every impl
that inherits it. If a default body calls a non-default requirement, that is
valid because the impl completeness check guarantees each concrete impl either
implements that requirement or inherits a valid default.

### 4.6 Associated Types

Default bodies may mention associated types:

```aurex
trait Source {
    type Item;
    fn next(self: &Self) -> Self.Item;
    fn first_or(self: &Self, fallback: Self.Item) -> Self.Item {
        return self.next();
    }
}
```

Rules:

- Associated types remain impl outputs, not impl-selection inputs.
- A default body is checked with projected types, then normalized when an impl
  supplies associated type outputs or equality predicates.
- Projection cycles are diagnosed using existing M4 cycle checks.
- M5 does not add default associated type values.

### 4.7 Generic Calls

In a generic body:

```aurex
fn empty[T](value: &T) -> bool
where T: Reader {
    return value.is_empty();
}
```

The call first binds to `param_env` evidence. When `empty[FileReader]` is
instantiated, the compiler selects either the `FileReader` override or the
`Reader.is_empty` default. This keeps generic type checking independent from a
particular concrete impl while preserving direct calls after monomorphization.

### 4.8 Recursion And Default Dependency

Default bodies are function bodies, not textual expansions. Therefore:

- A default method may recursively call itself. This is ordinary runtime
  recursion and can be a bug, but it is not a compile-time expansion cycle.
- Two default methods may call each other. This is also runtime recursion.
- Associated type projection cycles remain type-level cycles and are still
  compile-time errors.

M5 should not attempt to infer minimal method sets by dependency analysis. A
future M5.x or M6 feature may add an annotation similar in spirit to GHC's
`MINIMAL` pragma if users need richer contracts such as "implement either
`read` or `read_into`".

## 5. Rejected Alternatives

| Alternative | Strength | Rejection reason |
| --- | --- | --- |
| Copy default body into every impl during sema | Simple mental model | Duplicates AST/sema facts, creates unstable diagnostics, expands invalidation, and loses trait-owned source origin. |
| Require `default fn` syntax | Makes defaultness visually explicit | Adds a keyword without solving ambiguity; body presence is enough and matches common language practice. |
| Treat defaults as synthetic impl methods only | Easy lowering target | Hides the source owner; tooling, diagnostics, and query invalidation need trait method identity. |
| Type-check default body once per impl only | Sees concrete associated type outputs early | Repeats errors for every impl and prevents trait-local diagnostics. |
| Swift-style protocol extensions | Ergonomic, retroactive helper surface | Creates requirement-vs-extension dispatch traps and new lookup rules. |
| Kotlin / Scala mixin traits | Strong behavior reuse | Requires state, linearization, `super<Trait>`, or initialization rules outside M5. |
| Java / C# runtime default dispatch | Good API evolution for OO interfaces | Depends on runtime object model and dynamic dispatch ABI Aurex does not expose. |
| Specialization together with defaults | Very expressive | Requires overlap ordering, "more specific" selection, semver policy, and a stronger solver. |
| Default associated types in the same stage | Reduces associated-type boilerplate | Interacts with projection normalization and equality predicates; keep separate. |

## 6. Selected Compiler Design

### 6.1 Syntax And AST

Parser changes:

- In a trait body, parse `fn` items with either a semicolon or a body.
- Preserve current recovery when a function requirement has neither a semicolon
  nor a well-formed body.
- Keep associated type declarations unchanged.
- Keep impl parsing unchanged; impl methods already have bodies.

AST / syntax model:

- Keep trait methods as `ItemKind::fn_decl` entries owned by the trait body.
- Add an explicit way to know whether a trait method item has a body and to
  access its body slot.
- Use `BodySlotKind::trait_default_method` for default body identity.
- AST dump should show whether a trait method is a prototype requirement or a
  defaulted requirement.

### 6.2 Sema Facts

Extend `TraitMethodRequirement` with:

- `bool has_default_body`.
- `query::BodyKey default_body_key`.
- `query::StableFingerprint128 default_body_fingerprint`.
- A default body source range.
- A checked-body status or index into the same body-check authority used by
  ordinary functions.

Extend impl facts with a method origin record:

- Requirement ordinal.
- Method name/member key.
- `TraitImplMethodOrigin::explicit_override` or
  `TraitImplMethodOrigin::inherited_default`.
- Function key for overrides.
- Default body key for inherited defaults.

Extend call binding:

- Replace or refine `TraitMethodDispatchKind` so it does not overload "param
  env" with final origin.
- Record selected trait member key.
- Record selected impl key when concrete.
- Record selected function key for overrides.
- Record selected default body key for defaults.

### 6.3 Default Body Authority

The trait method default body should be a canonical trait-owned template:

- Syntax owner: trait method item.
- Stable body key: owner trait `DefKey` + `MemberKey` + ordinal +
  `BodySlotKind::trait_default_method`.
- Type-check environment: trait `Self`, trait generic params, trait where
  predicates, current trait evidence, and associated type projection facts.
- Lowering environment: selected concrete impl, concrete `Self`, trait args,
  associated type outputs, and generic instance identity.

This preserves query identity and avoids copying source bodies into impls.

### 6.4 Name Resolution And Visibility

- The default body sees the same lexical module imports and visibility as the
  trait declaration.
- It does not gain access to private members of arbitrary future impl types.
- Calling other trait methods on `Self` resolves through current trait
  evidence.
- Inherent methods on an abstract `Self` are not assumed unless the current
  type system can prove them through normal rules.

### 6.5 Lowering And Backend

Lowering consumes sema origin facts:

- `impl_override`: emit a direct call to the existing impl method symbol.
- `trait_default`: lower the trait default body as a function template
  specialized for concrete `Self`, trait args, and associated type outputs.
- `param_env`: allowed only before generic instantiation. Monomorphization must
  reselect a concrete origin before LLVM lowering.

Backend symbol naming:

- Override symbols keep existing impl method naming.
- Default symbols should derive from trait stable id, trait member key, self
  type canonical key, trait args, and associated type outputs.
- Display names may appear in diagnostics, not ABI keys.

### 6.6 Query And Incremental Invalidation

M5 must preserve M3 query boundaries:

- Changing a default body invalidates the default body's syntax, type-check,
  lowering, and call sites that actually use the default.
- Impls that override the method should not require body re-lowering just
  because the default body changed.
- Changing a requirement signature invalidates impl matching, call signatures,
  and inherited default checks.
- Changing an impl override invalidates calls selecting that override, not
  calls selecting the default in other impls.
- Query records must expose default body keys, method origin, and dependency
  edges so tooling reuse explanations remain truthful.

## 7. Diagnostics

M5 should add or refine these diagnostics:

| Case | Diagnostic goal |
| --- | --- |
| Trait default body has type error | Point to the trait method body and explain the failed expression. |
| Impl omits non-default method | Keep existing missing-method diagnostic. |
| Impl omits defaulted method | No error; checked dump should show inherited default. |
| Impl overrides default with wrong signature | Existing signature mismatch plus note that the trait method has a default but override still must match. |
| Duplicate impl override | Existing duplicate method diagnostic. |
| Unknown impl method | Existing unknown method diagnostic. |
| Ambiguous trait method with several candidates | Keep M4 ambiguity; note candidates and do not pick a default. |
| Generic call uses default after instantiation | Checked / IR dump should show final origin. |
| Default body calls method with missing bound | Point to default body and required trait evidence. |
| Default body references concrete fields of `Self` | Report normal field lookup failure on abstract `Self`. |

Tooling expectations:

- Definition from a call to an override should point to the impl method.
- Definition from a call to an inherited default should point to the trait
  default method body.
- Rename identity remains the trait method `MemberKey`.
- Hover should state whether the selected implementation is an override or a
  default when the receiver type is concrete.

## 8. Risk Matrix

| Risk | Cause | Evidence from other languages | Aurex mitigation |
| --- | --- | --- | --- |
| Silent dispatch surprise | Defaults look like ordinary methods | Swift protocol-extension dispatch can surprise users | Only trait-declared requirements get defaults; call binding records origin. |
| Multiple default conflict | Several traits define same-name default | Kotlin and Java require explicit conflict rules | Keep M4 ambiguity; no default tie-breaker. |
| Query invalidation too broad | Default body copied into impls | Large compilers need stable per-body identity | Trait-owned body key; invalidate only default users. |
| Query invalidation too narrow | Body changes not connected to call sites | Incremental compilers require dependency edges | Record selected default body key in call/lowering facts. |
| ABI instability | Symbol names based on display text | M3/M4 already reject display-string identity | Use stable ids, member keys, canonical type keys. |
| Code size growth | Static monomorphized default bodies | Rust/C++ monomorphization can grow code | Reuse M3.8 lowering/backend fingerprints and add stress tests. |
| Repeated diagnostics | Default type-checked per impl | Haskell/Rust defaults are trait-owned source | Type-check once in trait context; instantiate later. |
| Default recursion | Defaults call each other or themselves | Type classes allow recursive defaults | Treat as runtime recursion; future lint, not compile-time expansion. |
| Associated type ambiguity | Default body uses `Self.Item` under multiple bounds | Rust/Swift associated items need careful projection | Use M4 member keys and existing ambiguity/cycle checks. |
| Future dyn trait lock-in | Defaults might imply object-callability | Rust dyn compatibility is complex | M5 is static-only; object safety remains future design. |
| Future specialization conflict | Defaults resemble specialization | Rust specialization is hard and semver-sensitive | No overlapping defaults or partial ordering in M5. |
| API evolution breakage | Adding a non-default requirement breaks impls | Java defaults were designed for evolution | Document: adding default can be compatible; adding non-default is breaking. |

## 9. Work Packages

### M5-WP1: Research And Design Baseline

Status: this document.

Deliverables:

- English and Chinese design documents.
- English and Chinese M5 roadmap.
- Research over Rust, Swift, Kotlin, Java, C#, Haskell/GHC, Scala, Go, C++,
  and compiler-interface architecture.
- Selected semantic model, rejected alternatives, risk matrix, diagnostics,
  compiler pipeline route, and validation gates.
- README, next-steps, progress, and documentation integration tests.

### M5-WP2: Syntax / AST / Body Identity

Goal: allow trait methods to carry bodies without changing trait semantics yet.

Deliverables:

- Parser accepts trait method bodies and preserves prototype requirements.
- Existing negative parser tests are updated to reject only malformed bodies.
- AST / lossless syntax / dump distinguish prototype and defaulted
  requirements.
- `BodyKey` materialization uses `BodySlotKind::trait_default_method`.
- Query tests cover stable default-body keys.

### M5-WP3: Default Body Type Checking

Goal: type-check trait-owned default bodies in trait context.

Deliverables:

- `TraitMethodRequirement` records default body metadata.
- Default body checking runs with trait `Self`, trait generic params, trait
  where constraints, current trait evidence, and associated-type projection
  rules.
- Default-body diagnostics point to trait source.
- Normal repository tests cover positive and negative default body checking.

### M5-WP4: Impl Completeness And Method Origin

Goal: make omitted defaulted methods valid and record origin explicitly.

Deliverables:

- Impl validation accepts omitted defaulted requirements.
- Override signature checks remain strict.
- `TraitImplInfo` records explicit override vs inherited default per
  requirement.
- `TraitMethodCallBinding` records selected member and final origin for
  concrete receivers.
- Checked dumps show inherited defaults and method origin.

### M5-WP5: Lowering / Backend / Monomorphization

Goal: lower default method calls to direct calls.

Deliverables:

- Concrete default calls lower to trait-owned default method instances.
- Generic `param_env` calls reselect override vs default during
  monomorphization.
- LLVM/native tests cover default dispatch, override dispatch, generic
  dispatch, associated type normalization, and inherent-first priority.
- IR / checked dumps expose origin for regression tests.

### M5-WP6: Tooling, Diagnostics, Incremental Reuse

Goal: expose defaults through IDE and query-backed reuse without protocol leaks.

Deliverables:

- Hover/definition distinguishes override and default origins.
- Workspace index records default method bodies under the trait method member
  identity.
- Rename remains member-key based.
- Incremental tests show default-body edits invalidate default users but not
  overriding impl call paths.
- Diagnostics include candidate/default origin notes.

### M5-WP7: Release Closure

Goal: close M5 as a static default-method baseline.

Deliverables:

- Docs, usage notes, version notes, unsupported matrix, samples, and release
  baseline agree on the M5 surface.
- Full build, unit/integration tests, sample suite, coverage, query/cache, and
  stress gates pass.
- Future entries are documented for dyn traits, specialization, minimal
  implementation annotations, default associated types, and resource semantics.

## 10. Acceptance Gates For M5-WP1

M5-WP1 is complete when:

- Chinese and English design docs exist and cover research, semantic model,
  selected design, rejected alternatives, risk matrix, compiler pipeline,
  diagnostics, work packages, and references.
- Chinese and English M5 roadmap docs exist.
- `README`, `next-steps`, and `progress` state that M5 default trait methods
  are the active design stream.
- Documentation integration tests require the new M5 docs.
- At least these commands pass:
  - `tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')`
  - `git diff --check`
  - `cmake --build cmake-build-release -j4`
  - `./cmake-build-release/bin/aurex_tests --gtest_color=auto --gtest_filter='AurexIntegrationTest.DocumentationLayoutIsStable:AurexIntegrationTest.M5DefaultTraitMethodsDesignIsPlanned'`
  - `ctest --test-dir cmake-build-release --output-on-failure -j4`

## 11. References

- Rust Book: Default implementations,
  https://doc.rust-lang.org/book/ch10-02-traits.html#default-implementations
- Rust Reference: traits,
  https://doc.rust-lang.org/reference/items/traits.html
- Rust Reference: implementations,
  https://doc.rust-lang.org/reference/items/implementations.html
- rustc-dev-guide: trait resolution,
  https://rustc-dev-guide.rust-lang.org/traits/resolution.html
- Swift Book: Protocols,
  https://docs.swift.org/swift-book/documentation/the-swift-programming-language/protocols/
- Kotlin Documentation: Interfaces,
  https://kotlinlang.org/docs/interfaces.html
- Java Language Specification, Chapter 9 Interfaces,
  https://docs.oracle.com/javase/specs/jls/se22/html/jls-9.html
- C# language reference: interface,
  https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/keywords/interface
- GHC User Guide: `MINIMAL` pragma,
  https://ghc.gitlab.haskell.org/ghc/doc/users_guide/exts/pragmas.html#minimal-pragma
- Haskell 2010 Report: Classes and instances,
  https://www.haskell.org/onlinereport/haskell2010/haskellch4.html#x10-770004.3
- Scala 3 Book: Traits,
  https://docs.scala-lang.org/scala3/book/domain-modeling-tools.html#traits
- Go Language Specification: Interface types,
  https://go.dev/ref/spec#Interface_types
- C++ working draft: constraints and concepts,
  https://eel.is/c++draft/temp.constr
- MLIR Interfaces,
  https://mlir.llvm.org/docs/Interfaces/
- Scharli, Ducasse, Nierstrasz, Black: Traits: Composable Units of Behaviour,
  https://scg.unibe.ch/archive/papers/Scha03aTraits.pdf
- Cook, Hill, Canning: Inheritance Is Not Subtyping,
  https://www.cs.utexas.edu/~wcook/papers/InheritanceSubtyping90/CookHillCanning90.pdf
