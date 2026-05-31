# Aurex M4-WP1 Trait / Protocol System Research And Design Baseline

## 1. Status And Scope

This document is the M4-WP1 design baseline. It is not only a syntax sketch and
it is not an implementation patch. It fixes the language semantics, compiler
boundaries, risk constraints, and staged route that must exist before Aurex
starts implementing traits / protocols.

Current decision:

- Aurex adopts **nominal static traits** as the first M4 capability layer.
- The language keyword is `trait`; `protocol` remains design terminology for a
  behavioral contract rather than an object model.
- Conformance must be explicit through `impl Trait for Type`; M4 does not use
  Go-style structural or duck-typed conformance.
- Traits are initially generic constraints and static-dispatch evidence. M4.0
  does not add `dyn Trait`, trait objects, vtable ABI, or object-safety rules.
- M4.0 does not add RAII, `Drop`, `Copy`, move-only values, borrow checking,
  resource capabilities, or destructor lowering.
- M4.0 does not add default methods, associated constants, specialization,
  negative impls, unsafe traits, auto traits, or arbitrary blanket impls.
- Associated types are part of the M4 design, but they are phased: M4.0 first
  implements trait declarations, impl registries, coherence, and static method
  dispatch; M4.1 follows with associated-type projections and equality
  constraints.

This selection is not because the other designs are impossible. It is because
Aurex has just closed the M3 module, generic, query-backed sema, tooling, and
lowering authority boundaries. M4 must continue on that modern compiler path
instead of mixing class inheritance, dynamic objects, resource semantics, and a
full trait solver into one language-core change.

## 2. Existing Aurex Baseline

M3.9 fixed the architecture that later language work must reuse:

- `CompilationSession`, `CompilationPipeline`, `FrontendPipeline`,
  `LoweringPipeline`, `BackendPipeline`, and `PipelineStage` are the driver,
  profile, diagnostics, and cache path.
- The module / project layer has `ModuleKey`, `ModulePartKey`, `ProjectKey`,
  and the persistent query DB.
- Generics have `GenericInstanceKey`, `GenericTemplateSignature`,
  `GenericInstanceSignature`, and `GenericInstanceBody`; semantic identity must
  not regress to display strings or session-local handles.
- Sema has query-backed `ItemSignature`, `FunctionBodySyntax`,
  `TypeCheckBody`, and generic authority boundaries.
- Tooling and LSP only consume protocol-neutral value types. They do not bypass
  parser, sema, or query internals.
- Lowering and backend already separate target-independent IR units,
  layout / ABI fingerprints, and LLVM emission units.

The current code already reserves part of the trait identity model:

- `include/aurex/query/query_key.hpp` contains `DefNamespace::trait_`,
  `DefNamespace::impl_`, `DefKind::trait_`, `DefKind::trait_method`,
  `DefKind::associated_type`, `DefKind::associated_const`,
  `MemberKind::trait_method`, `MemberKind::associated_type`,
  `MemberKind::associated_const`, and `BodySlotKind::trait_default_method`.
- `include/aurex/query/canonical_type_key.hpp` contains
  `CanonicalTypeKind::associated_type_projection` and
  `CanonicalTypeKind::trait_object`.
- `include/aurex/query/generic_instance_key.hpp` has `ParamEnvKey`, which is
  the right shape for generic instances checked under predicate environments.
- Syntax does not yet have `kw_trait`; `ItemKind` has no `trait_decl`;
  `ImplBlockItemPayload` only describes inherent `impl Type { ... }`, not
  `impl Trait for Type { ... }`.
- Parser `where T: Eq + Hash` clauses currently become
  `GenericConstraintDecl.capability_names`.
- Sema's `CapabilityKind` currently accepts only `Sized`, `Eq`, `Ord`, and
  `Hash`, and explicitly rejects `Copy` / `Drop` resource capabilities.

M4 is therefore not adding traits from nowhere. It formalizes the reserved
query identity and upgrades built-in capabilities into structured trait
predicates while preserving the M3 incremental, query, and tooling boundaries.

## 3. Research Findings

### 3.1 Rust Traits

Rust is the closest reference for Aurex. A trait defines a behavioral contract,
an `impl` defines how a concrete type satisfies it, generic bounds are resolved
at compile time, and the default path uses monomorphization and static
dispatch.

What Aurex should adopt:

- Traits have nominal identity. A type does not implement a trait just because
  it has methods with matching names.
- Traits and impls are separate, so behavior can be attached to existing types.
- Associated items include methods, associated types, and associated constants,
  which separates input types from output types.
- Coherence and orphan rules prevent unbounded cross-crate impl conflicts.
- Trait resolution has explicit obligations, selection, and fulfillment instead
  of ad-hoc boolean predicates.

What Aurex should avoid in M4.0:

- Dyn compatibility and object safety are complex. Associated constants, generic
  associated types, `Self` positions, and receiver shapes all affect object
  eligibility.
- Blanket impls, specialization, negative impls, and auto traits increase
  overlap checking, semver compatibility, and solver complexity.
- Chalk-style logical trait solving is powerful, but it is too large for the
  first Aurex trait implementation.

Aurex adopts nominal traits, explicit impls, orphan / coherence checks, an
obligation worklist, and the associated-type-as-output model. Aurex defers dyn
traits, default methods, specialization, negative impls, auto traits, GATs, and
unsafe traits.

### 3.2 Swift Protocols

Swift protocols require explicit conformance, can constrain generics, and can
enter existential form. Swift also uses associated types to describe placeholder
types supplied by the conforming implementation.

What Aurex should adopt:

- Types do not automatically conform just by having matching methods.
- Associated types are a good model for iterator items, collection indices,
  serializer outputs, and similar implementation-chosen types.
- Generic where clauses can combine protocol conformance and associated-type
  equality.

What Aurex should avoid in M4.0:

- Existential protocol values add storage, indirection, and witness-table ABI
  costs.
- Protocols with `Self` or associated-type requirements have long-standing
  existential complexity.
- Protocol extensions and default implementations improve ergonomics but make
  method origin and conflict diagnostics harder.

Aurex adopts explicit conformance, phased associated types, and future where
equality. Aurex defers existential-first design, protocol extensions, default
implementations, and dynamic witness-table ABI.

### 3.3 Kotlin Interfaces

Kotlin interfaces can contain abstract methods, accessor-like property
requirements, and default method implementations, but they cannot store object
state. Conflicting inherited default implementations require explicit
resolution.

What Aurex should adopt:

- A trait / interface should not own object state. State belongs to structs or a
  future class-like data layout.
- Multiple behavioral capabilities can be composed on one type.

What Aurex should avoid in M4.0:

- Default method conflicts turn "which implementation is called" into a
  language-level ambiguity.
- Interface properties can blur fields, getters, computed properties, and ABI
  layout if introduced too early.

Aurex adopts stateless traits and defers default methods, property
requirements, and diamond-style conflict resolution.

### 3.4 Go Interfaces

Go interfaces are structural. A type implements an interface when its method set
belongs to the interface type set. Since Go 1.18, interfaces can also describe
type sets.

What Aurex can learn:

- Small interfaces are good API boundaries and testing seams.
- Interface composition can keep abstractions lightweight.

Why Aurex does not adopt this model:

- Structural conformance can be accidental; a rename or method addition can
  change API relationships without an explicit conformance declaration.
- Type sets and `comparable` rules show that generic constraints quickly become
  more than "a few methods".
- Aurex query, tooling, and rename need stable symbol identity. Structural
  matching turns "who implements this?" into a global search problem.

M4 therefore uses nominal explicit conformance rather than Go-style structural
interfaces.

### 3.5 C++ Concepts And Class Systems

C++ provides two relevant experiences: inheritance / virtual dispatch, and
C++20 concepts / constraints.

Advantages of class-first design:

- Data, construction, destruction, methods, encapsulation, and polymorphism are
  in one mechanism.
- It integrates tightly with low-level ABI and performance control.
- Virtual functions are useful for plugin boundaries and heterogeneous
  collections.

Weaknesses of class-first design:

- Inheritance and subtyping are not the same concept. Inheritance is often used
  for implementation reuse, interface promises, and substitutability at the same
  time.
- Multiple inheritance, diamonds, virtual bases, object slicing, virtual
  destructors, override rules, and ABI layout are long-term language-core
  complexity.
- Base-class changes can affect many derived classes through overrides,
  protected state, and virtual dispatch, producing fragile-base-class problems.
- Templates, overloads, ADL, concepts, SFINAE, and partial ordering together
  make diagnostics and compile time expensive.

Advantages of C++ concepts:

- Template constraints become explicit compile-time predicates.
- Constraint satisfaction, normalization, conjunction, disjunction, and partial
  ordering provide strong generic expressiveness.

Weaknesses for Aurex M4:

- Arbitrary requires-expressions and constraint normalization are too heavy for
  a compiler that is just establishing its trait solver.
- Constraints are not conformance identity; the equality of constraint
  expressions and the identity of implementations can diverge.

Aurex does not start with class inheritance and does not add arbitrary
requires-expressions. M4 accepts canonical trait predicates and the minimal
associated-type equality needed later.

### 3.6 Scala 3 Given / Type Classes

Scala 3 expresses type classes with traits and `given` instances. This is
flexible: behavior can be defined separately from data types and found through
contextual parameters.

What Aurex should adopt:

- Behavior instances can be separate from data declarations, which is useful for
  open extension.
- Extension-method ergonomics are valuable, although they do not have to enter
  M4.0.

What Aurex should avoid in M4.0:

- Implicit / given search can make call resolution opaque to users.
- Imports, priorities, ambiguous givens, and derivation rules increase
  diagnostics and IDE explanation cost.

Aurex adopts separate impls, but not implicit search. Trait method resolution
must be explainable through lexical scope, explicit bounds, and the impl
registry.

### 3.7 Haskell / GHC Type Classes

Haskell type classes are the classic model of ad-hoc polymorphism. The language
and GHC ecosystem show that the model is powerful, but overlapping instances,
multi-parameter classes, functional dependencies, and undecidable instances
quickly raise solver complexity.

What Aurex should adopt:

- A constraint can produce dictionary / evidence that resolves operations in a
  checked way.
- Associated types and functional dependencies both address relationships where
  input types determine output types or other type parameters.

What Aurex should avoid in M4.0:

- Overlapping instances break the "exactly one impl" rule unless heavily
  controlled.
- Multi-parameter classes and functional dependencies introduce ambiguity,
  termination, and inference complexity.
- Undecidable instances are not suitable for a systems-language default path.

Aurex M4.0 only supports single-self-type traits. It does not add
multi-parameter traits, functional dependencies, or undecidable instances.

### 3.8 MLIR Interfaces

MLIR interfaces are not source-language traits, but their compiler architecture
is useful. An interface abstracts the fact that a class of IR objects can be
handled generically by a pass; models can be registered externally, but they
must be verified.

What Aurex should adopt:

- Trait declarations and implementation models should be separate facts in the
  compiler.
- Interface / trait models must be explicitly registered and verified.

What Aurex should avoid:

- A promised interface without a complete implementation produces poor failures
  at use sites.
- External models are flexible, but require strict ownership and coherence
  rules.

Aurex's impl registry must be queryable and verified. M4 must not permit
"declare now, fill in later" unverified impls.

### 3.9 Paper-Level Conclusions

The classic type-class literature shows that ad-hoc polymorphism becomes
valuable when overloaded operations are backed by typed constraints and
evidence. That supports migrating Aurex `Eq` / `Ord` / `Hash` from built-in
string-like capabilities to trait predicates.

Traits-as-composable-units research shows that the value of traits is
composable behavior, not inheritance trees. That supports Aurex's default route
of `struct + impl + trait + composition` instead of class inheritance.

The "Inheritance is not Subtyping" line of OO research shows that inheritance is
often misused as both implementation reuse and substitutability. Aurex therefore
does not make M4 class-first: implementation reuse stays in composition and
functions, behavioral constraints are traits, and dynamic objects are designed
later.

## 4. Class-System Comparison And Aurex's Choice

The user-visible desire behind "classes" is usually not inheritance itself. It
is often:

- Keeping data and methods together.
- Natural `obj.method(...)` calls.
- Clear encapsulation boundaries.
- Reusing capability interfaces.
- Safe future resource management.

These needs do not require one inheritance mechanism to solve everything. For
Aurex, the better split is:

| Need | Inheritance-style class answer | Aurex route |
|:--|:--|:--|
| Data layout | Class fields | `struct` |
| Method organization | Class methods | Inherent `impl Type` |
| Behavioral abstraction | Base class / interface | `trait` |
| Behavior reuse | Inheritance / mixin | Composition + helpers + later default methods |
| Static polymorphism | Templates / concepts | Generic `where T: Trait` |
| Runtime polymorphism | Virtual / interface object | Later explicit `dyn Trait` |
| Resource cleanup | Destructor / RAII | Later resource semantics / `Drop` |

M4 therefore implements traits before class inheritance. Advantages:

- It does not freeze object layout or vtable ABI.
- It avoids fragile base classes and inheritance/subtyping confusion.
- Existing `struct + impl` already preserves method ergonomics.
- Trait predicates connect directly to M3 generic and query authority.
- Static dispatch and monomorphization are friendlier to inlining,
  incremental invalidation, and backend reuse.

Accepted shortcomings:

- There is no single `class` declaration that combines fields, constructors,
  trait conformance, and destructors in the short term.
- Without dynamic objects, heterogeneous collections, plugin boundaries, and
  runtime polymorphism remain awkward.
- Without default methods, some impls will contain boilerplate.
- Static dispatch can increase generic instances and code size, so later
  codegen-unit and IR-reuse work must keep tracking it.

These are deliberate staged costs that preserve future design space.

## 5. Selected Aurex Semantic Model

### 5.1 Trait Declarations

M4 adds nominal trait declarations:

```aurex
pub trait Reader {
    fn read(self: &mut Self, buf: []mut u8) -> usize;
}
```

Rules:

- A trait is a named definition in the type namespace and has a stable `DefKey`.
- A trait does not define object layout and does not occupy value storage.
- A trait body contains requirement items. M4.0 only allows method
  requirements.
- Trait methods have no body. Default methods are deferred.
- `Self` inside a trait is an implicit type parameter representing the concrete
  implementing type.
- Traits can have visibility. Method visibility initially follows trait
  visibility; finer rules are fixed during implementation.

### 5.2 Trait Implementations

```aurex
impl Reader for File {
    fn read(self: &mut File, buf: []mut u8) -> usize {
        return file_read(self, buf);
    }
}
```

Rules:

- `impl Trait for Type` is a conformance fact, not a new nominal type.
- An impl has stable impl identity and enters the impl registry.
- Every impl method must satisfy a trait method requirement after substituting
  `Self := Type`.
- Inherent `impl Type` and trait `impl Trait for Type` are modeled separately.
- A trait impl cannot add methods that the trait did not require. Additional
  methods belong in an inherent impl.
- M4.0 does not allow partial impls, promised impls, or deferred
  implementation.

### 5.3 Generic Bounds And Static Dispatch

```aurex
fn copy_one[R](reader: &mut R, buf: []mut u8) -> usize
where R: Reader {
    return reader.read(buf);
}
```

Rules:

- `where R: Reader` creates `TraitPredicate{trait = Reader, self = R}`.
- Type checking turns trait requirements into `TraitObligation` values.
- The solver resolves obligations in the current `ParamEnv`.
- Trait method calls inside generic bodies bind to requirement evidence rather
  than string lookup.
- After monomorphization, trait method calls lower to direct calls to concrete
  impl methods. M4.0 does not emit vtables.

Method-resolution priority:

1. Look up inherent methods first.
2. Then inspect applicable trait methods from lexical bounds and imported
   traits.
3. If multiple trait candidates have the same call shape, require explicit
   disambiguation.

M4.0 diagnostics must explain:

- The type does not satisfy the trait bound.
- The trait is not imported or not visible.
- The impl is unavailable because of orphan or visibility rules.
- Multiple trait method candidates conflict.
- The impl method signature does not match the requirement.

### 5.4 Coherence And Orphan Rules

M4 needs coherence in the first implementation, otherwise future modules and
packages will make impl resolution unstable.

Rules:

- For one trait ref + self type, at most one impl applies in a package world.
- The current package may implement:
  - A current-package trait for any nameable type.
  - Any visible trait for a current-package nominal type.
- The current package may not implement an external trait for an external
  nominal type.
- Built-in primitive trait facts come from a compiler-owned built-in impl
  provider, not from user impls.
- M4.0 forbids arbitrary blanket impls such as
  `impl[T] Display for T where T: Debug`.
- M4.0 may allow local generic self-type impls, but the head must be a
  current-package nominal type, such as `impl[T] Reader for Buffer[T] where T:
  Sized`.
- Overlap checks use canonical trait refs, canonical self types, and param
  environments, not display strings.

This is a deliberately smaller version of the Rust / Chalk "unique impl"
principle, adapted to Aurex's current module / package baseline.

### 5.5 Associated Types

Associated types are in the M4 design and the first implementation slice landed
in M4-WP6. The implemented subset is intentionally the static-dispatch,
non-object, non-GAT model described here.

Target semantics:

```aurex
trait Iterator {
    type Item;
    fn next(self: &mut Self) -> Option[Self.Item];
}

impl Iterator for ByteIter {
    type Item = u8;
    fn next(self: &mut ByteIter) -> Option[u8] { ... }
}

fn sum[I](iter: &mut I) -> i32
where I: Iterator[Item = u8] {
    ...
}
```

Design principles:

- An associated type is a trait impl output, not an input to impl selection.
- Impl selection first finds the unique impl for `(Trait, SelfType, ParamEnv)`,
  then reads associated type values from that impl.
- `Iterator[Item = u8]` lowers to a trait predicate plus an associated-type
  equality predicate.
- Projection canonical types use the existing
  `CanonicalTypeKind::associated_type_projection`; the base is `Self` or a
  generic parameter, and the member key points to the trait associated type.
- If `T.Item` is not unique across trait bounds, report ambiguity. A later
  phase can add explicit `(T as Iterator).Item` disambiguation.
- M4 does not implement generic associated types. GATs would pull solver,
  lifetime/resource, and equality-constraint complexity into the first trait
  release.
- M4-WP6 deliberately keeps associated constants, default associated type
  values, explicit qualified projection syntax, and trait-object projection ABI
  out of scope. The current shorthand `T.Item` is accepted only when the
  associated type name is unique in the current trait bounds or normalized by a
  matching equality predicate.

### 5.6 Capability Migration

Current `Sized`, `Eq`, `Ord`, and `Hash` are built-in capabilities. M4 cannot
delete them outright without breaking M3.1 generic tests and samples.

Migration strategy:

1. Keep `CapabilityKind` as a compatibility entry point.
2. Create compiler-known built-in trait identities for the four capabilities.
3. Lower `where T: Eq + Hash` to built-in trait predicates instead of stopping
   at string-like capabilities.
4. Keep operator availability separate from trait satisfaction:
   - Direct `==` rules remain type-checker rules.
   - `T: Eq` is a generic predicate satisfied by built-in or user impls.
5. Continue rejecting `Copy` / `Drop` until resource semantics are designed
   separately.

## 6. Rejected Alternatives

| Alternative | Strength | Rejection reason |
|:--|:--|:--|
| Class-first inheritance | Familiar syntax; encapsulation, construction, and virtual dispatch in one model | Freezes object model, vtables, destructors, and inheritance/subtyping relations too early |
| Go structural interfaces | Lightweight and ergonomic | Accidental conformance, unstable rename/refactor behavior, weak query identity |
| Swift existential-first protocols | Good runtime-polymorphism experience | Witness tables, existential storage, and `Self` / associated-type restrictions freeze ABI too early |
| Rust dyn trait first | Explicit runtime polymorphism | Object safety, vtable layout, drop glue, and resource semantics are not designed yet |
| Kotlin default methods first | Less boilerplate | Multiple-default conflicts and method-origin diagnostics become part of M4.0 |
| Scala implicit / given search | Very extensible | Call resolution becomes less transparent; IDE and diagnostics cost grows |
| C++ arbitrary requires | Strong generic expressiveness | Constraint normalization and partial ordering are too heavy for M4.0 |
| Haskell multi-param / fundep | Expresses complex type relations | Ambiguity, termination, and solver complexity are too high |
| Auto / unsafe marker traits | Can encode deep semantic properties | Coupled to resource, concurrency, and unsafe boundaries; defer until those systems exist |

## 7. Risk Matrix

| Risk | Cause | Evidence from other languages | Aurex mitigation |
|:--|:--|:--|:--|
| Impl conflict | Multiple impls apply | Rust / Chalk require coherence and overlap checks | M4.0 uses orphan rules, no blanket impls, and unique impl checking |
| Cross-package semver breakage | Upstream impl additions conflict downstream | Rust orphan rules model compatible worlds | A package may implement only local traits or traits for local nominal types |
| Solver nontermination | Recursive bounds and projection cycles | GHC undecidable instances and Rust solvers need limits | M4.0 uses finite obligation worklists, cycle detection, and depth budgets |
| Associated-type ambiguity | Multiple traits define the same associated type name | Rust needs qualified projection; Swift PATs complicate existentials | M4.1 requires unique shorthand and later explicit projection if needed |
| Dynamic ABI lock-in | Trait objects need data/vtable/drop/layout rules | Rust dyn compatibility is complex | M4 does not expose dyn traits; `trait_object` remains reserved |
| Default method conflicts | Several traits provide same-name defaults | Kotlin requires explicit conflict resolution | M4.0 has no default methods |
| Structural accidental conformance | Matching method names imply implementation | Go interfaces can surprise refactors | M4 uses nominal explicit impls |
| Opaque implicit search | Automatic instance selection hides origin | Scala given / Haskell instance search can be subtle | M4.0 has no implicit search; candidates come from bounds, scope, and registry |
| Capability compatibility | `Eq` and similar move from capability to trait | Direct replacement would break generic tests | Built-in trait identities migrate compatibility gradually |
| Query-cache instability | Predicates use display strings | M3 already rejected display-string identity | Use `DefKey`, `MemberKey`, `GenericInstanceKey`, and canonical type keys |
| Diagnostics regression | Solver failures collapse into "bound not satisfied" | C++ concepts and Rust trait errors can be long | Obligations carry source range, bound source, and candidate rejection reasons |
| Tooling leaks internals | LSP reads sema-private objects | M3 forbids LSP DTOs in compiler internals | IDE consumes protocol-neutral trait facts |
| Resource pressure | `Drop` / `Copy` look like ordinary traits | Rust marker/drop traits interact with safety | M4-WP1 keeps RAII/resource out of scope |
| Code-size growth | Static dispatch / monomorphization | Rust and C++ need codegen-unit and LTO controls | Reuse M3.8 IR-unit fingerprints and add trait-instantiation profiling later |

## 8. Compiler Pipeline Impact

### Lex / Parse / AST

- Add `kw_trait`.
- Add `ItemKind::trait_decl`.
- Add `TraitItemPayload` with at least name, generic params, where
  constraints, and requirements.
- Extend `ImplBlockItemPayload` with an optional trait type / trait path to
  distinguish inherent impls from trait impls.
- Evolve `where` constraints from capability names to trait predicate syntax.
- Parser recovery must handle `trait Name { ... }`, `impl Trait for Type {
  ... }`, and trait-body requirements.

### Sema / Query

- Add a `TraitSignature` query or extend item signature authority so trait
  requirements are durable facts.
- Add an impl registry query indexed by package / module / trait / self type.
- Add `TraitPredicate`, `TraitObligation`, `TraitEvidence`, and predicate lists
  inside `ParamEnv`.
- Make coherence checking a sema gate. Failed coherence stops lowering.
- Migrate capabilities through a built-in trait provider instead of expanding
  string capability checks.

### Lowering / Backend

- M4.0 trait calls use static dispatch only.
- After generic monomorphization, a trait call becomes a direct call to the
  concrete impl method symbol.
- Do not emit vtables, trait object layout, or drop glue.
- IR dumps should either show the selected impl or keep enough debug metadata
  for tests.

### Tooling

- Semantic tokens should classify trait names, trait methods, impl blocks, and
  associated types.
- Completion should offer visible traits after `where T:`.
- Hover / definition should navigate from trait requirements to impl methods and
  support reverse implementation listing later.
- Rename must use `DefKey` / `MemberKey`, not textual trait method names.

## 9. M4 Work Packages

Implementation status as of 2026-05-31: M4-WP1 through M4-WP8 are complete.
The release baseline is recorded in
[Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md).

| Work package | Goal | Done when |
|:--|:--|:--|
| M4-WP1 | Research and design baseline | This document, roadmap, risk matrix, and documentation tests are closed |
| M4-WP2 | Syntax / AST / query identity scaffolding | `trait` token, trait AST, trait impl AST, stable dumps, parser tests |
| M4-WP3 | Trait declaration and impl registry | Trait signature facts, impl facts, requirement matching, registry query |
| M4-WP4 | Coherence and generic predicates | Orphan / overlap checks, `where T: Trait` obligations, built-in capability migration |
| M4-WP5 | Static method resolution and lowering | Trait method call binding, monomorphized direct calls, IR/backend tests |
| M4-WP6 | Associated type model | Associated type declarations/impls, projection, equality predicates, ambiguity diagnostics |
| M4-WP7 | Tooling and diagnostics | Completion, hover, definition, rename, semantic tokens, candidate rejection notes |
| M4-WP8 | Release closure | Docs, coverage, stress, query/cache/profile gates, unsupported matrix updates |

## 10. M4-WP1 Acceptance Gates

M4-WP1 is complete when:

- Chinese and English design docs exist and cover selected design, rejected
  alternatives, risk matrix, class comparison, and implementation route.
- `next-steps` states that the highest priority is the M4 trait/protocol design
  baseline.
- `progress` records the M4-WP1 status.
- README documentation indexes include the M4 design and roadmap.
- The documentation integration test requires the new docs.
- At least these commands pass:
  - `clang-format -i tests/gtest/integration/documentation_tests.cpp`
  - `cmake --build cmake-build-release -j4`
  - `./cmake-build-release/bin/aurex_tests --gtest_color=auto --gtest_filter='AurexIntegrationTest.DocumentationLayoutIsStable'`
  - `tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')`
  - `git diff --check`

## 11. References

- Rust Reference: traits and dyn compatibility, https://doc.rust-lang.org/stable/reference/items/traits.html
- Rust Reference: implementations and coherence, https://doc.rust-lang.org/stable/reference/items/implementations.html
- Rust Reference: associated items, https://doc.rust-lang.org/stable/reference/items/associated-items.html
- rustc-dev-guide: trait resolution, https://rustc-dev-guide.rust-lang.org/traits/resolution.html
- rustc-dev-guide: Chalk-based trait solving, https://rustc-dev-guide.rust-lang.org/traits/chalk.html
- Chalk book: coherence, https://rust-lang.github.io/chalk/book/clauses/coherence.html
- Swift Book: Protocols, https://docs.swift.org/swift-book/documentation/the-swift-programming-language/protocols/
- Swift Book source: Protocols, https://raw.githubusercontent.com/swiftlang/swift-book/main/TSPL.docc/LanguageGuide/Protocols.md
- Swift Book source: Generics, https://raw.githubusercontent.com/swiftlang/swift-book/main/TSPL.docc/LanguageGuide/Generics.md
- Kotlin Documentation: Interfaces, https://kotlinlang.org/docs/interfaces.html
- Kotlin Documentation: Generics, https://kotlinlang.org/docs/generics.html
- Go Language Specification: Interface types, https://go.dev/ref/spec
- C++ working draft: constraints and concepts, https://eel.is/c++draft/temp.constr
- Scala 3 Reference: Type Classes, https://docs.scala-lang.org/scala3/reference/contextual/type-classes.html
- GHC User Guide: Type classes and instances, https://downloads.haskell.org/ghc/latest/docs/users_guide/exts/instances.html
- GHC User Guide: Functional dependencies, https://downloads.haskell.org/ghc/latest/docs/users_guide/exts/functional_dependencies.html
- MLIR Interfaces, https://mlir.llvm.org/docs/Interfaces/
- Wadler and Blott, How to make ad-hoc polymorphism less ad hoc, https://homepages.inf.ed.ac.uk/wadler/papers/class/class.pdf
- Scharli, Ducasse, Nierstrasz, Black, Traits: Composable Units of Behaviour, https://scg.unibe.ch/archive/papers/Scha03aTraits.pdf
- Cook, Hill, Canning, Inheritance is not Subtyping, https://www.cs.utexas.edu/~wcook/papers/InheritanceSubtyping90/CookPOPL90.pdf
