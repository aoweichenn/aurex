# Aurex M6 Resource, Value Lifetime, And Access Semantics Research And Three-Pass Design Review Baseline

## 1. Document Status

This document is the M6-WP1 design baseline. It closes three design-review
passes for the post-M5 resource-semantics stream:

1. Build an evidence matrix from modern languages, industrial compilers, and
   current research.
2. Fix the Aurex-specific semantic composition, compiler boundaries, and
   implementation order.
3. Stress the design from the user perspective with real cases, failure paths,
   and future-extension pressure.

This is not a proposal to copy the Rust borrow checker and it is not a syntax
sketch for adding an ordinary `Drop` trait immediately. M6 first establishes a
stable resource and value-lifetime foundation. Later borrow checking, owned
libraries, dynamic trait objects, and concurrency features must build on the
same fact model.

Current conclusions:

- The formal name is **M6 Resource And Access Semantics**.
- The first implementation line is value classification, whole-local moves,
  use-after-move checking, deterministic cleanup, and generic drop glue.
- A complete borrow checker is not part of the first M6 implementation. It
  starts as a separate M7 design stream after M6 is stable.
- `Copy`, permission to discard, cleanup obligation, and mandatory consumption
  are separate dimensions. They must not remain collapsed into an ambiguous
  `Drop` capability.
- `defer` remains a language feature and composes with compiler-inserted cleanup
  through one lexical cleanup-action model.
- The first release does not add partial moves, indexed move-out, unwind,
  async drop, managed-global destruction, complete lifetime syntax,
  `dyn Trait`, or broad array/aggregate ABI enablement.

## 2. Why M6 Starts Now

M5 closed the static trait default-method baseline. Aurex now has the required
foundations for a dedicated resource design:

| Foundation | Current State | Code Entry |
| --- | --- | --- |
| Reference and value-type basis | `TypeKind` distinguishes `pointer`, `reference`, `slice`, `str`, aggregates, and generic parameters | `include/aurex/sema/type.hpp` |
| Safe/raw boundary | `&T` / `&mut T` are separate from raw pointers; `&mut` requires a writable place; raw dereference requires `unsafe` | `src/sema` |
| Explicit lexical cleanup | `defer` covers normal exit, `return`, `break`, `continue`, and `?` failure paths | `src/ir/lower_ast_stmt.cpp` |
| Generic capabilities | `Sized`, `Eq`, `Ord`, and `Hash` exist; `Copy` / `Drop` are recognized and rejected explicitly | `src/sema/internal/sema_generic_analyzer.cpp` |
| Static traits | Nominal traits, explicit impls, associated types, defaults, and static direct calls are complete | M4 / M5 |
| Stable query identity | `BodySlotKind::destructor_drop`, `GenericParamKind::resource`, and `lifetime` are reserved | `include/aurex/query/query_key.hpp` |
| Real resource pressure | The regex library uses many `defer destroy(&mut value)` and manual `free_typed` calls | `examples/libs/regex` |

Current `defer` is not RAII. It cannot identify moved values, express drop
obligations, drop flags, field glue, overwrite cleanup, or generic resource
behavior. Current IR also has ordinary `alloca`, `load`, `store`, `call`, and
`aggregate` nodes, but no formal cleanup nodes. Assignment still emits a direct
`store`.

M6 is therefore a compiler-semantics main line, not a library wrapper task.

## 3. First Review Pass: Cross-Language And Research Evidence

### 3.1 Separate Four Problems First

Modern languages do not have one resource system that is optimal everywhere.
The design must distinguish:

| Problem | Typical Bug | Solution Layer |
| --- | --- | --- |
| Whether a value may be copied | double free, expensive hidden deep copy, copied lock handle | value capability |
| When a resource is released | leak, double close, overwrite leak, early-return leak | deterministic cleanup |
| Whether accesses conflict | mutable and shared access overlap, container mutation during iteration | access / alias analysis |
| Whether a reference dangles | returning local reference, using view after container reallocation | lifetime / origin analysis |

RAII primarily addresses the second problem. Borrow checking primarily covers
the third and fourth. They interact, but they are not the same design problem.

### 3.2 C++: Adopt RAII, Reject Historical Complexity

C++ contributes deterministic destruction:

- Bind resources to object lifetime.
- Destroy locals in reverse completion order.
- Handle old resources when overwriting values.
- Use scope-bound handles for files, locks, memory, and native handles.

Aurex adopts:

- Deterministic lexical cleanup.
- Activate cleanup after a named local finishes initialization.
- Keep the old target alive while evaluating an assignment RHS, then clean it
  before storing the new value.
- Layer custom cleanup above recursive component cleanup.

Aurex rejects:

- The implicit copy-constructor, move-constructor, copy-assignment,
  move-assignment, and destructor overload family.
- Rule-of-three/five/zero compatibility complexity.
- Exception-unwind ABI and cross-module implicit special-member synthesis.
- Letting ordinary overload resolution decide resource safety.

### 3.3 Rust And rustc: Adopt Layered Elaboration, Not The Entire Surface

Rust layers moves, initialization state, drop obligation, and borrow checking.
rustc MIR drop elaboration distinguishes static, dead, conditional, and open
drops, introducing drop flags when needed.

Aurex adopts:

- A source value becomes unusable as an owned value after a whole-local move.
- CFG-sensitive dataflow instead of lexical-block-only checking.
- Cleanup obligations become checked facts before elaboration.
- Conditional initialization and joins use drop flags.
- Raw pointers stay behind explicit `unsafe`.

Aurex defers:

- Partial field moves, indexed move-out, and arbitrary place-tree move paths.
- Full lifetime parameters and higher-ranked regions.
- Two-phase borrows.
- Polonius-level relational analysis.

These are valuable capabilities, but they must build on a stable value and
cleanup model.

### 3.4 Swift: Adopt Ownership Conventions And Noncopyable Generic Lessons

Swift `borrowing` / `consuming` parameters and noncopyable types demonstrate
that resources are not a single struct marker. They affect parameter
contracts, generics, destruction, ABI, patterns, caller readability, and
library evolution.

Aurex adopts:

- Distinguish owned consume, shared borrow, and mutable borrow semantically.
- Treat by-value parameters as ownership boundaries, not vague byte copying.
- Propagate noncopyable aggregate capabilities through fields.
- Explain copy, borrow, and consume uses in diagnostics.

Aurex does not copy Swift ARC as the default for all values. Shared ownership
belongs in later library types rather than every Aurex value's baseline cost.

### 3.5 Mojo: Adopt Transfer Lessons, Reject Default ASAP Destruction

Mojo clearly distinguishes borrowing, mutable borrowing, owned parameters, and
ownership transfer. It also enables last-use lifetime optimization.

Aurex adopts:

- A move is ownership transfer, not necessarily a physical memory copy.
- Optimizers may remove relocation or copying when observable behavior stays
  unchanged.
- API docs and tooling should expose parameter consumption mode.

Aurex rejects shortening named-resource lifetime to last use by default.
Release time for files, locks, transactions, and external handles is
observable. M6 uses stable lexical lifetime for named locals and a precise
full-expression boundary for temporaries. More aggressive optimization is
allowed only when cleanup has no observable side effect.

### 3.6 Move: The Key Lesson Is Capability Separation

Move represents `copy`, `drop`, `store`, and `key` as separate abilities. The
important lesson for Aurex is:

> Permission to copy, permission to discard silently, and the obligation to run
> cleanup when a lifetime ends are not the same property.

Aurex adopts a related split with adjusted terminology:

- `Copy`: ordinary value use may preserve the source value.
- `Discard`: an owned value may end lifetime without an explicit
  business-level consume operation.
- `NeedsDrop`: compiler cleanup glue must run when an initialized lifetime
  ends.
- `MustConsume`: future opt-in linear-resource direction; reserved but not
  exposed in the first release.

Move's `drop` ability is closer to Aurex `Discard`; it must not share a name
with the destruction protocol.

### 3.7 Zig And Go: Preserve The Value Of `defer`

Zig lexical `defer` and `errdefer` show that explicit exit actions remain useful
even with clear resource APIs:

- FFI raw handles.
- Multi-step initialization rollback.
- Temporary state restoration.
- Narrow resources that do not justify an owned wrapper.

Go uses function-scoped `defer` and evaluates arguments at registration. Aurex
currently behaves more like Zig lexical defer: the deferred call executes when
leaving its scope and actions unwind in reverse order.

Aurex chooses:

- Preserve lexical-scope `defer`.
- Put `defer` and compiler cleanup in one cleanup-action stack.
- Evaluate deferred call expressions and arguments when the action executes.
  Users who need a snapshot bind a local first.
- Record this behavior in language docs and tests instead of leaving it as an
  implementation accident.

### 3.8 Hylo: Place, Projection, And Access Are The Right Future Abstraction

Hylo mutable value semantics emphasizes controlled temporary access rather than
long-lived reference identity for every API. For Aurex:

- M7 should not model only variables that store references.
- Place, projection, read, mutate, and consume should form one vocabulary.
- Fields, indexes, and destructuring eventually belong in a place tree rather
  than independent patches.

M6 deliberately starts with whole-local moves to preserve a clear upgrade path
to place trees.

### 3.9 Pony And Verona: Reserve Space For Isolation, Regions, And Concurrency

Pony reference capabilities and Verona region/capability research show that
ownership can also serve:

- Data-race isolation.
- Region reclamation for object graphs.
- Safe transfer between actors or tasks.
- Arena lifetime management.

M6 does not add a capability lattice, region inference, or concurrency
`Send` / `Sync`. Resource classification and query identity must nevertheless
leave room for future isolation or region metadata.

### 3.10 Cyclone: Regions Are An Advanced Layer, Not The First Foundation

Cyclone demonstrates that region-based management can provide strong lifetime
guarantees in a C-like systems language and improve arena and object-graph use
cases. Region polymorphism also expands types, call contracts, diagnostics, and
inference complexity.

Aurex concludes:

- Regions are a valuable opt-in design stream for M8 or later.
- M6 cleanup glue and M7 origin analysis must not assume all owned values are
  independent heap objects.
- The first release still centers lexical owned locals and explicit borrows.

### 3.11 Lean, Koka, Roc, And Perceus: RC And Reuse Are Optimization Directions

Lean and the Perceus line show that precise reference counting, borrow
inference, and reuse analysis can update function-style values in place when
uniquely held and reduce allocations.

Aurex adopts:

- Future `Rc[T]`, strings, persistent collections, and COW containers may use
  RC, uniqueness, and reuse optimization.
- IR must not assume every move performs a physical copy.
- Optimizers may add retain/release elimination and reuse passes after the
  semantics stabilize.

Aurex rejects:

- Default RC as the replacement for every resource-management strategy.
- Any claim that RC alone solves cycles, file closing, lock release, or
  transaction semantics.

### 3.12 Linear Haskell, Idris 2, Granule, And Austral: Mandatory Consumption Is Separate

Linear and quantitative type research demonstrates that resource systems may
need to prove that a value is used exactly once, not just avoid double frees.
Examples include transaction tokens, authority tickets, typestate protocols,
and one-shot capabilities.

Aurex concludes:

- Reserve `MustConsume`, but do not expose it in the first M6 release.
- Ordinary files, containers, and lock guards should primarily use RAII rather
  than exact-once user proofs.
- Future must-consume types remain separate from `Discard` and `NeedsDrop`.

### 3.13 Carbon And Clang: Gradual Safety And Diagnostic Quality Matter

Clang Lifetime Safety Analysis uses owner, pointer, borrow, origin, and
control-flow concepts to find C++ lifetime defects. It is not a complete sound
type system, but it shows:

- Origin chains are important diagnostics.
- The source and invalidation point of a value must be first-class facts.
- Tooling projection cannot wait until the end.

Starting in WP3, M6 records move origins, cleanup origins, and classification
reasons. M7 extends the same shape with loan origins.

### 3.14 Oxide, RustBelt, Aeneas, Polonius, And New Region Research

Formal and verification work constrains the design:

- Oxide shows that ownership typing needs clear place, environment, and
  lifetime relationships.
- RustBelt shows that unsafe abstraction boundaries are library contracts, not
  just syntax.
- Aeneas shows long-term value in lowering borrowed programs into a smaller,
  verifiable intermediate language.
- Polonius demonstrates expressive relational borrow analysis with substantial
  implementation cost.
- New region research is worth tracking but does not replace a mature staged
  engineering plan.

The direct Aurex conclusion is that M6 must produce queryable, verifiable
resource facts before M7 adds loan relations.

## 4. First-Pass Conclusion: Aurex Does Not Copy One Language

Aurex selects this composition:

```text
C++ deterministic RAII
+ Move capability separation
+ Swift / Mojo consume and borrow call contracts
+ Rust / Clang CFG dataflow and origin diagnostics
+ Zig explicit defer escape hatch
+ Hylo place / access upgrade direction
+ Cyclone / Verona future region entry points
+ Lean / Koka future RC and reuse optimization entry points
```

This is an Aurex-specific layered model, not a Rust subset.

## 5. Second Review Pass: Aurex Semantic Decisions

### 5.1 Resource Properties Are An Orthogonal Vector

Every storable type receives a structured `ResourceSemanticsSummary`.
Conceptually:

```text
copy:        Copy | MoveOnly
discard:     Discard | MustConsume
cleanup:     Trivial | NeedsDrop
ownership:   OwnedValue | BorrowedView | RawPointer | SharedManaged
```

`SharedManaged` is a future library and optimization entry, not a default M6
ABI.

The dimensions cannot be compressed into one enum:

| Type | copy | discard | cleanup | ownership |
| --- | --- | --- | --- | --- |
| `i32` | `Copy` | `Discard` | `Trivial` | `OwnedValue` |
| `&T` | `Copy` | `Discard` | `Trivial` | `BorrowedView` |
| `str` | `Copy` | `Discard` | `Trivial` | `BorrowedView` |
| raw `*mut T` | `Copy` | `Discard` | `Trivial` | `RawPointer` |
| future `Vec[T]` | `MoveOnly` | `Discard` | `NeedsDrop` | `OwnedValue` |
| future transaction token | `MoveOnly` | `MustConsume` | type-specific | `OwnedValue` |

### 5.2 `Copy` Is Compiler-Owned

`Copy` means an ordinary owned use may preserve the source value.

Rules:

- Numbers, booleans, characters, function pointers, raw pointers, references,
  slices, and `str` are `Copy` by default.
- A tuple, array, struct, or enum satisfies `Copy` structurally only when every
  owned component is `Copy`.
- Generic `T` is copyable only with compiler-owned `T: Copy` evidence in the
  parameter environment.
- Future `Clone` is an explicit ordinary operation. It does not imply `Copy`,
  and assignments or calls do not invoke it silently.
- Users cannot disguise unsafe types as `Copy` through an ordinary trait impl.

A first nominal surface may allow a type to request `Copy`, but the compiler
must validate it structurally. There is no user-defined implicit C++-style copy
constructor.

### 5.3 Separate `Discard`, `NeedsDrop`, And `MustConsume`

Rules:

- `Discard` permits an owned lifetime to end without a business-level explicit
  consumer.
- `NeedsDrop` requires compiler cleanup glue when an initialized lifetime ends.
- A `NeedsDrop` resource is commonly still `Discard`, because automatic cleanup
  is its legal end-of-lifetime behavior.
- `MustConsume` forbids silent scope-end completion and requires an explicit
  proof of consumption in a future opt-in feature.
- M6 implements internal `Discard` / `NeedsDrop` facts but does not expose a
  user-authored `MustConsume` surface.

There is therefore no ambiguous `where T: Drop`. The compiler can construct
drop glue for every storable concrete owned `T`: no-op or recursive cleanup.
`NeedsDrop` is an internal optimization and lowering fact, not an ordinary
user-implementable trait.

### 5.4 Owned Use, Borrow, And Mutate

M6 fixes use modes:

| Use Context | `Copy` Type | `MoveOnly` Type |
| --- | --- | --- |
| `let b = a` | copy; `a` stays initialized | consume; `a` becomes moved |
| by-value parameter `fn f(x: T)` | copy | consume |
| `return a` | copy or elidable copy | transfer to caller |
| aggregate field / enum payload construction | copy | transfer to new owner |
| `&a` | shared borrow, no consume | shared borrow, no consume |
| `&mut a` | mutable borrow, no consume | mutable borrow, no consume |

M6 does not revive the M1 ad hoc `move(...)` builtin. Owned-use context decides
the semantics. A future `consume expr` intent marker may be designed if real
cases justify it, but compiler correctness must not depend on it.

### 5.5 Whole-Local Move Is A Hard First-Release Boundary

Allowed:

```aurex
let first: Resource = open();
let second: Resource = first; // consume first
use(second);
```

Rejected initially:

```aurex
let field = pair.left;  // move-only field move-out: later place-tree work
let value = values[i];  // indexed move-out: later container/place work
let .some(item) = opt;  // non-Copy payload extraction: staged later
```

Whole-local analysis needs a local bitset. Partial moves need move-path trees,
parent/child place states, field drop flags, enum active variants, and pattern
binding policy. Indexed move-out additionally needs container-hole policy.

### 5.6 CFG-Sensitive Move Analysis

Use an iterative function-body worklist, never recursive dataflow. Maintain for
each local:

```text
definitely_initialized
maybe_initialized
last_consume_origin
```

Transfers:

- Successful initialization or reinitialization adds to definitely and maybe.
- Non-`Copy` owned consume removes from definitely and maybe.
- Join intersects definitely and unions maybe.
- Before read, borrow, or consume:
  - not in maybe: diagnose definite use-after-move or use-before-init;
  - in maybe but not definitely: diagnose maybe-moved;
  - in definitely: allow.

Cleanup classification:

- in definitely: static drop;
- not in maybe: dead drop;
- in maybe but not definitely: conditional drop with flag.

Complexity target:

```text
O(CFG edges * local-bitset words * fixed-point iterations)
```

Store states at block boundaries. Do not copy a full local map for every
expression. Keep move origins in sparse side tables for diagnostics and tooling.

### 5.7 Assignment Order

For whole-local overwrite:

```text
evaluate target place
-> evaluate RHS completely
-> if old target remains initialized, run old-value cleanup
-> store new owned value
-> mark target initialized
```

This permits `x = transform(&x)` to observe the old value while evaluating the
RHS. `x = x` on a move-only value does not double-drop: evaluating the RHS
consumes the old slot, overwrite cleanup sees a dead drop, and the new owner is
stored back.

Managed-resource field overwrite and indexed overwrite remain rejected in the
first release. Ordinary trivial-copy field assignment keeps current behavior.

### 5.8 Cleanup Action Stack

M6 selects one cleanup-action stack.

Activation:

- Register a named-local drop action after initialization succeeds.
- Register a defer action when execution reaches `defer call(...)`.
- During staged aggregate construction, register temporary cleanup for
  completed fields and transfer them to the final owner when construction
  completes.
- Clean full-expression temporaries at the expression boundary unless
  transferred.

Unwind scope actions in reverse registration order for:

- normal exit;
- `return`;
- `break`;
- `continue`;
- `?` failure early return.

Example:

```aurex
var a: Resource = open_a();
defer log("after a");
var b: Resource = open_b();
defer log("after b");
```

Exit order:

```text
log("after b")
drop(b)
log("after a")
drop(a)
```

This composes better than running all defers or all locals as separate batches
and follows actual activation order.

### 5.9 Precise `defer` Semantics

M6 fixes:

- `defer` remains limited to call expressions.
- Deferred calls run at scope exit.
- Deferred call arguments are evaluated when the action executes, not
  snapshotted at the registration statement.
- Actions in one scope run in reverse activation order.
- Locals referenced by a deferred expression must remain usable on every
  reachable exit path.

This is intentionally different from Go's function-scoped `defer`. Go saves
the function value and arguments when the `defer` statement executes. Aurex
chooses exit-time evaluation for lexical actions so that existing scope
`defer`, `&mut` access, the RAII cleanup-action stack, and future place analysis
use one model. The tradeoff is visible when a deferred expression mentions a
local that is later reassigned: the exit action observes the current slot, not
the old registration-time value. Users who need a snapshot must bind an
independent local explicitly. For move-only owned resources, the preferred
long-term form is a compiler-managed destructor rather than persistent manual
`defer close(&mut value)`.

Example:

```aurex
var handle: Handle = open();
defer close(&mut handle);
consume(handle);
```

This is diagnosed because deferred cleanup still accesses moved `handle`. Once
`Handle` becomes compiler-managed, users ordinarily should not write manual
`defer close(&mut handle)`.

### 5.10 Custom Destructor Protocol

M6 fixes semantics before parser spelling:

- A nominal type has at most one custom destructor body.
- A destructor is a compiler-owned lifecycle protocol, not an ordinary method.
- Users cannot invoke or overload it explicitly, and ordinary trait candidate
  search does not select it.
- The body receives a mutable view of the current value.
- After the custom body, the compiler recursively drops fields by rule.
- A destructor does not return a business error. Fallible resources expose an
  explicit `close` API; destruction is the no-fail fallback.
- The first destructor body cannot move fields out of `self`, resurrect
  `self`, or unwind.

Parser spelling receives one focused design review before M6-WP5. Candidate
surfaces include a sealed lifecycle impl or a dedicated `deinit` declaration.
Whatever spelling wins, destruction must not become an ordinary trait method.

### 5.11 Aggregate Cleanup

Structural drop order:

| Type | Cleanup Order |
| --- | --- |
| struct | custom destructor body, then fields in reverse declaration order |
| tuple | elements in reverse positional order |
| enum | custom destructor body, then active payload |
| array | elements in reverse index order |
| generic aggregate | monomorphized glue for concrete components |

This is an explicit Aurex language choice, not a direct copy of Rust. Rust's
field drop order differs from the structural cleanup order selected here. Aurex
chooses reverse order so fields, tuples, arrays, aggregate-construction
rollback, and the cleanup-action stack all follow the same last-activated,
first-cleaned rule. Implementation must freeze this order with golden, IR, and
native tests instead of inheriting accidental backend or container iteration
order.

Array-local cleanup and array by-value ABI are separate problems. M6 may
generate local array glue without promising to lift current array-containing
parameter, return, assignment, or enum-payload restrictions.

### 5.12 Parameters, Returns, And Trait Receivers

M6 rules:

- Ordinary by-value `T` parameters are owned parameters. Passing non-`Copy`
  values consumes at the caller.
- `&T` is a shared borrow and `&mut T` is a mutable borrow. M6 preserves the
  current reference boundary; M7 proves aliases and lifetimes.
- Returning non-`Copy` values transfers ownership to the caller.
- Callees clean owned parameters that they do not transfer onward.
- Future `self: Self` trait receivers consume; `self: &Self` and
  `self: &mut Self` borrow.
- Default trait methods on generic `Self` may only perform resource operations
  permitted by their parameter environment.

### 5.13 FFI And `unsafe`

The first M6 release tightens FFI:

- `extern c` by-value parameters and returns accept only ABI-supported,
  `Trivial` cleanup types.
- C resource handles cross through raw pointers, integer handles, or explicit
  opaque wrappers.
- The compiler never assumes C runs an Aurex destructor.
- A raw pointer does not become an owner automatically.
- Future `forget`, `ManuallyDrop`, or ownership adoption operations must be
  narrow `unsafe` intrinsics or audited library APIs.
- `unsafe` can carry external contracts but cannot silently disable move/drop
  tracking for ordinary safe owned locals.

### 5.14 Globals, Constants, Exceptions, Async, And Concurrency

The first M6 release defers:

- Managed global initialization and process-exit destruction order.
- Thread-local destruction.
- Exception or panic unwind cleanup.
- Async state-machine drop.
- Generator suspension.
- Region / arena surface.
- `Send`, `Sync`, isolation, and actor transfer.

Aurex currently has no exception or async surface. Correct normal CFG exits are
more important than freezing an unwind ABI early.

### 5.15 Generic Drop Glue

For every concrete storable type, the compiler can query or generate
`DropGlueKey(type)`:

- `Trivial` types use no-op glue.
- Nominal resources invoke the custom destructor body and recursively clean
  components.
- Struct, tuple, and array glue visits components in reverse order.
- Enum glue selects the active payload from its tag.
- Generic glue uses concrete arguments after monomorphization.

Identity rules:

- Glue query identity uses stable type keys, nominal-owner `DefKey`,
  destructor `BodyKey`, generic arguments, and resource-classification
  fingerprints.
- Never use display strings.
- Never persist session-local `TypeHandle.value` as identity.

### 5.16 IR And Verifier Layering

M6 must not hand-write free calls in every AST-lowering branch. Use layers:

```text
checked AST
-> CFG + move/resource facts
-> cleanup obligations
-> cleanup elaboration
-> target-independent Aurex IR cleanup nodes / glue calls
-> IR verifier
-> LLVM lowering
```

IR design entries:

- Represent unconditional drop formally.
- Represent conditional drop or equivalent drop-flag CFG formally.
- Express drop-and-replace obligations before elaboration.
- Keep moves primarily as sema/elaboration facts; final physical
  representation may optimize to load/store or direct relocation.
- Verify glue targets, place types, flags, cleanup-block targets, and the
  double-elaboration invariant.

Specific IR enum spellings receive a focused review before M6-WP4. This
document fixes semantic requirements only.

### 5.17 Query, Incremental Cache, And Tooling

Required facts:

| Fact | Purpose |
| --- | --- |
| type resource summary | type checking, generic predicates, hover |
| move origin | use-after-move diagnostics, IDE notes |
| cleanup origin | explain why automatic destruction exists |
| destructor body identity | query invalidation, tooling definition |
| drop glue identity | monomorphization, cache reuse, backend |
| body resource-check fingerprint | body-local incremental invalidation |

Tooling should explain at least:

- Whether a type is `Copy` or `MoveOnly`.
- Whether a type `NeedsDrop`.
- Whether an owned use copies or consumes.
- The source range where a moved value was consumed.
- Whether cleanup comes from lexical exit, overwrite, early return, or
  aggregate rollback.

## 6. Second-Pass Rejected Alternatives

| Alternative | Why Rejected |
| --- | --- |
| Copy the complete Rust borrow checker immediately | move/drop foundation is not stable; partial moves, regions, and two-phase borrows multiply scope |
| Add only an ordinary user `Drop` trait | cannot express compiler obligations, field glue, overwrite, flags, or future dyn ABI safely |
| C++ special-member model | overload, implicit synthesis, ABI, and unwind complexity are too high |
| Default ARC for all values | runtime cost for every value; still does not solve files, locks, cycles, or FFI cleanup |
| Keep only manual `defer destroy` | regex already demonstrates boilerplate and leak risk; owned libraries remain blocked |
| Default last-use destruction for named locals | release timing is observable for locks, files, and transactions |
| Expose partial moves in the first release | requires place trees and field-level flags, creating double-drop risk |
| Lift array ABI restrictions together with resources | independent backend and ABI work multiplies risk |
| Implement `dyn Trait` first | freezes vtable drop ABI before erased ownership is stable |
| Add regions or isolation first | valuable research direction, but blocks basic deterministic cleanup |

## 7. Third Review Pass: User Cases And Counterexample Pressure

### 7.1 Ordinary `Copy` Values

```aurex
let first: i32 = 1;
let second: i32 = first;
return first + second;
```

Result: ordinary copy, no cleanup, current intuition preserved.

### 7.2 Regex Manual Resource Migration

Current:

```aurex
var compiled: regex.Regex = regex.compile(pattern);
defer regex.destroy(&mut compiled);
return regex.is_match(&compiled, input);
```

Target:

```aurex
var compiled: regex.Regex = regex.compile(pattern);
return regex.is_match(&compiled, input);
```

Result: initialization activates lexical cleanup; the return expression
evaluates first, then cleanup runs. Manual `destroy(&mut compiled)` must not
remain alongside compiler-managed destruction because that can double-close.
Regex migration needs a later dedicated library validation.

### 7.3 Use After Move

```aurex
let first: File = open(path);
let second: File = first;
read(&first);
```

Result: diagnose use-after-move at `read(&first)` and note the consume origin at
`let second = first`.

### 7.4 Maybe-Moved Join

```aurex
var file: File = open(path);
if condition {
    consume(file);
}
read(&file);
```

Result: `file` is maybe-moved after the join and use is rejected. Cleanup uses
a flag and runs only on still-initialized paths.

### 7.5 Reinitialize After Move

```aurex
var file: File = open(first_path);
consume(file);
file = open(second_path);
read(&file);
```

Result: allowed. Overwrite sees the old value is moved and skips cleanup. New
initialization reactivates cleanup.

### 7.6 Assignment RHS Ordering

```aurex
var file: File = open(path);
file = reopen_from(&file);
```

Result: evaluate `reopen_from(&file)`, clean old `file`, then store the new
owner. M7 must later reject a returned value that illegally borrows old
storage.

### 7.7 Interleaved `defer` And RAII

```aurex
var outer: File = open(a);
defer log("outer");
var inner: File = open(b);
defer log("inner");
```

Result:

```text
log("inner")
drop(inner)
log("outer")
drop(outer)
```

### 7.8 Deferred Use After Move

```aurex
var file: File = open(path);
defer inspect(&file);
consume(file);
```

Result: reject. `inspect(&file)` runs on exit and accesses a moved value. The
diagnostic points to both defer registration and consume origin.

### 7.9 Generic Identity

```aurex
fn identity[T](value: T) -> T {
    return value;
}
```

Result: works for `Copy` and move-only `T`; ownership transfers to caller.

### 7.10 Generic Duplicate Use

```aurex
fn duplicate[T](value: T) -> (T, T) {
    return (value, value);
}
```

Result: requires `T: Copy`. No hidden clone.

### 7.11 Resource Payload, Pattern, And `?`

```aurex
let result: Result[File, Error] = open(path);
let file: File = result?;
```

The full target needs success-payload move, failure-payload return transfer,
active enum-payload cleanup, and temporary cleanup. M6 design covers the
direction, but the first whole-local batch may reject non-`Copy` payload `?` and
consuming patterns until aggregate transfer is complete.

### 7.12 Partial Field Move

```aurex
let socket = connection.socket;
```

If `socket` is move-only, the parent becomes partially moved. Reject initially.
Add place-tree and field-flag design later.

### 7.13 Indexed Move-Out

```aurex
let item = values[index];
```

A move-only item leaves a hole. Reject initially. Future APIs use `take`,
`remove`, swap-remove, or container-specific operations.

### 7.14 Partially Initialized Aggregate Failure

```aurex
let pair = Pair {
    left: open(left_path)?,
    right: open(right_path)?,
};
```

If `right` fails, completed `left` must clean up. M6-WP5 aggregate construction
uses temporary cleanup actions and transfers them only after full construction.

### 7.15 Explicit File `close`

Closing a file may fail, while destruction cannot report a business error:

```aurex
var file: File = open(path);
let result = file.close();
```

The eventual library design should make explicit `close` consume the resource,
with destruction as a no-fail fallback. The concrete consuming-method surface
gets a focused library review later.

### 7.16 Lock Guard

```aurex
{
    let guard = mutex.lock();
    update();
}
```

Lexical cleanup gives the expected unlock point. Default last-use destruction
could release too early and is rejected.

### 7.17 FFI Raw Handle

```aurex
extern c {
    fn c_open() -> *mut void;
    fn c_close(handle: *mut void) -> void;
}

fn use_handle() -> void {
    let handle = c_open();
    defer c_close(handle);
}
```

The raw pointer remains `Copy + Discard + Trivial`. The compiler does not infer
ownership. Users keep narrow `defer` or later wrap it in an owned type.

### 7.18 Self Reference

```aurex
struct SelfRef {
    data: Buffer,
    ptr: *const u8,
}
```

The raw pointer may point into `data`; moving breaks the internal-address
contract. M6 does not provide pinning or self-reference guarantees. Such a
pattern belongs behind an audited `unsafe` abstraction until real demand
justifies a `Pin`-like library or immovable types.

### 7.19 Shared Ownership Cycles

Future `Rc[Node]` may form cycles. RAII and RC do not solve all cycles
automatically. Libraries need weak references or region/arena strategies.

### 7.20 Future `dyn Trait`

Trait objects eventually need erased drop glue. M6 first stabilizes concrete
glue, transfer, and borrowed-view rules. Vtable slots, object safety, and erased
storage come later.

## 8. Third-Pass Conclusion: First-Release Boundary

M6 first release must implement:

- compiler-owned `Copy`;
- internal `Discard` / `NeedsDrop`;
- whole-local moves and CFG-sensitive use-after-move;
- reinitialization;
- lexical cleanup-action stack;
- scope exit, overwrite, `return`, `break`, `continue`, and `?` exit cleanup;
- conditional drop flags;
- nominal, aggregate, and generic drop glue;
- IR verifier, LLVM lowering, query/cache/tooling, and diagnostics.

M6 first release explicitly does not implement:

- partial field moves;
- indexed move-out;
- arbitrary place-tree moves;
- complete borrow checking or lifetime parameters;
- a temporary lexical-only borrow checker;
- Polonius;
- two-phase borrows;
- unwind cleanup;
- async drop;
- managed-global destruction;
- regions, arenas, isolation, or `Send` / `Sync`;
- default ARC;
- `dyn Trait`, object safety, or vtable ABI;
- specialization;
- C++ special members;
- broad array / aggregate ABI enablement;
- standard-library rebuild.

## 9. M7 Borrow-Checking Entry

M7 builds on M6 facts with CFG-sensitive origin / loan analysis:

```text
place
-> projection
-> access kind: read | mutate | consume
-> loan origin
-> activation and use ranges
-> CFG-sensitive conflict check
-> borrowed-return and lifetime contract
```

M7 must not permanently settle for a lexical-only checker. It may start with a
narrow origin-dataflow implementation, but the model must allow NLL-style loan
termination. Two-phase borrows, regions, and relational solvers remain driven
by real cases.

## 10. Compiler Pipeline Impact Map

| Layer | M6 Impact |
| --- | --- |
| Lexer / parser | Keep destructor-surface entry open until the focused WP5 review; do not freeze tokens early |
| AST | destructor declaration, owned-use source ranges, cleanup origins |
| Sema type system | resource summary, structural propagation, generic evidence |
| Body checking | use modes, whole-local moves, reinit, maybe-moved diagnostics |
| CFG | iterative worklist, block bitsets, cleanup exit edges |
| Checked module | destructor facts, summaries, move origins, glue identities |
| Query | destructor body, body resource check, stable drop-glue keys |
| IR | cleanup obligation, drop/drop-if or equivalent CFG, overwrite elaboration |
| Verifier | cleanup type, flags, glue, exit edges, double-elaboration invariant |
| LLVM backend | glue functions, conditional branches, direct cleanup calls |
| Tooling | hover classification, move origin, cleanup reason, definition |
| Incremental cache | resource fingerprints, destructor invalidation, generic-glue reuse |
| Tests | unit, positive, negative, IR, LLVM, native, tooling, cache, stress, coverage |

## 11. Risk Matrix

| Risk | Severity | Control |
| --- | --- | --- |
| Ordinary `Drop` trait bypasses safety | Critical | compiler-owned `NeedsDrop` and destructor protocol |
| Deferred use after move | Critical | include defer actions in body analysis and check exits |
| Users expect `defer` arguments to be snapshots | High | document exit-time evaluation; require explicit snapshot locals; test reassignment cases |
| overwrite double-drop | Critical | evaluate RHS first, cleanup by state, verify flags |
| join path leaks | Critical | definitely/maybe bitsets and conditional drops |
| partially initialized aggregate leak | High | temporary field cleanup actions, transfer on completion |
| aggregate cleanup order conflicts with other-language intuition | Medium | state Aurex uses reverse activation order; freeze with golden, IR, and native tests |
| generic-glue cache contamination | High | stable type keys, `DefKey`, `BodyKey`; never display strings or session handles |
| partial move leaks into first release | High | hard whole-local boundary and negative tests |
| array ABI work overwhelms M6 | High | separate local glue from by-value ABI enablement |
| inferred FFI ownership | High | raw pointers stay non-owning; reject resource by-value C ABI |
| early destruction changes behavior | High | lexical locals; no default ASAP destruction |
| destructor failure cannot report error | Medium | explicit consuming `close` API plus no-fail fallback |
| per-expression state copies hurt performance | High | block bitsets, sparse origins, stress gates |
| premature dyn trait ABI freeze | High | delay dyn traits until concrete glue is stable |
| lexical borrow checker becomes permanent | High | M7 starts with a CFG-sensitive model |

## 12. Acceptance Gates

Every implementation WP keeps:

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

Large closure additionally runs:

```sh
make perf-release-threshold
```

Coverage targets:

- New code lines / functions / regions at least `95%`.
- Affected modules at least `95%` where practical.
- New samples live in normal repository directories, never temporary fixtures.

## 13. Frozen And Open Items After Three Reviews

Frozen:

- Four-dimensional resource summary.
- Whole-local first-release move boundary.
- CFG-sensitive move analysis.
- Lexical named-local cleanup.
- Full-expression temporary cleanup.
- One cleanup-action stack interleaving `defer` and compiler drops.
- Overwrite order.
- Sealed lifecycle destructor protocol, not an ordinary method.
- Generic glue and stable identity direction.
- FFI and `unsafe` boundary.
- Defer complete borrow checking to M7.

Open:

- Final destructor surface spelling.
- Whether a future user-visible `consume expr` marker is needed.
- Library convention for explicit consuming `close`.
- Exact M6 batch that opens non-`Copy` enum-payload patterns and `?`.
- Later phase number for place-tree partial moves.
- Post-M6 ordering among regions, RC, isolation, and dyn traits.

These open items have explicit entries and do not block the M6-WP2 resource
classification scaffold.

## 14. References

### Industrial Languages And Compilers

- C++ Core Guidelines, RAII:
  https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-raii
- Rust Reference, destructors:
  https://doc.rust-lang.org/reference/destructors.html
- rustc dev guide, drop elaboration:
  https://rustc-dev-guide.rust-lang.org/mir/drop-elaboration.html
- Rust RFC 2094, non-lexical lifetimes:
  https://rust-lang.github.io/rfcs/2094-nll.html
- Rust RFC 2025, two-phase borrows:
  https://rust-lang.github.io/rfcs/2025-nested-method-calls.html
- Polonius current status:
  https://rust-lang.github.io/polonius/current_status.html
- Swift SE-0377, borrowing and consuming parameter ownership modifiers:
  https://github.com/swiftlang/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md
- Swift SE-0390, noncopyable structs and enums:
  https://github.com/swiftlang/swift-evolution/blob/main/proposals/0390-noncopyable-structs-and-enums.md
- Mojo manual, ownership:
  https://docs.modular.com/mojo/manual/values/ownership/
- Move Book, abilities:
  https://move-language.github.io/move/abilities.html
- Zig language reference, `defer`:
  https://ziglang.org/documentation/master/#defer
- Go specification, defer statements:
  https://go.dev/ref/spec#Defer_statements
- Pony tutorial, reference capabilities:
  https://tutorial.ponylang.io/reference-capabilities/reference-capabilities.html
- Hylo language tour, bindings and mutable value semantics:
  https://docs.hylo-lang.org/language-tour/bindings
- Austral specification, linear types, borrowing, and capabilities:
  https://austral-lang.org/spec/spec.html
- Vale guide, generational references and regions:
  https://vale.dev/guide/regions
- Roc `Str` documentation, reference counting and opportunistic mutation:
  https://roc-lang.org/builtins/main/Str/
- Clang, Lifetime Safety Analysis:
  https://clang.llvm.org/docs/LifetimeSafety.html
- Carbon, safety strategy:
  https://docs.carbon-lang.dev/docs/design/safety/
- Project Verona, safe scalable memory management and compartmentalisation:
  https://www.microsoft.com/en-us/research/project/project-verona/
- Lean reference manual, reference counting:
  https://lean-lang.org/doc/reference/latest/Run-Time-Code/Reference-Counting/

### Papers And Research

- Cyclone regions:
  https://www.cs.cornell.edu/projects/cyclone/papers/cyclone-regions.pdf
- Oxide: The Essence of Rust:
  https://arxiv.org/abs/1903.00982
- RustBelt:
  https://plv.mpi-sws.org/rustbelt/popl18/paper.pdf
- Aeneas:
  https://arxiv.org/abs/2206.07185
- Perceus: Garbage Free Reference Counting with Reuse:
  https://www.microsoft.com/en-us/research/publication/perceus-garbage-free-reference-counting-with-reuse/
- Linear Haskell:
  https://www.microsoft.com/en-us/research/publication/linear-haskell-practical-linearity-higher-order-polymorphic-language/
- Idris 2 quantitative type theory:
  https://arxiv.org/abs/2104.00480
- Verona reference capabilities:
  https://www.microsoft.com/en-us/research/publication/reference-capabilities-for-flexible-memory-management/
