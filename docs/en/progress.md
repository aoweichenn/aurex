# Current Progress

Version: 0.1.2
Stage: M3.1 generics

## Overall Status

The repository has moved from the closed M2 language-core-no-std baseline into
the M2.5 frontend-foundation stage. M2 does not continue the abandoned M1 track.
It recenters the project on the language core by removing the standard library
and M1 system examples from the active tree. M2.5 builds on that closed line and
starts the structured frontend work needed for queries, lossless syntax, and
IDE-native consumption.

As of 2026-05-25, the R5 Compilation Pipeline / Driver Action core is complete.
The driver has moved to a modern compiler layout: `CompilerInvocation` is pure
configuration, `Compiler` is a facade, `CompilationSession` owns
per-compilation services, `CompilationPipeline` orchestrates frontend/lowering/
backend stages, and `PipelineStage` records the single stage directory for
profile names, inputs, outputs, diagnostic ownership, and cache/query impact.
R5.4 introduced the lightweight IR pass manager and verifier gate; R5.5
introduced `ModuleAnalysisManager`; R5.6 added stable
`stage/profile/verifier/pass` context to IR verifier gate failures while
preserving the original verifier body and `ErrorCode`, and `LoweringPipeline`
now passes the IR pass pipeline stage names from `PipelineStageRecord`. R5.7
added optional `stage` metadata to driver main-stage phases in
`aurex-profile-v1`, sourced directly from `PipelineStageRecord`. R5.8 added
optional `parent_stage` metadata for incremental-cache profile sub-events,
threading source-stage reuse back to `incremental_cache.lookup` and query
diff / plan / pruning / provider-eval back to `incremental_cache.write`. R5.9
added a `DiagnosticCategory` to candidate owner-stage lookup in `PipelineStage`,
keeping lexer diagnostics attached to both `tokens.lex` and `module.lex` while
mapping sema-like diagnostics to `sema.analyze`; the diagnostics text/JSON
protocol remains unchanged. R5.10 wired `aurex_tooling` diagnostics to the same
stage directory through `IdeDiagnostic.owner_stages`, so later LSP/IDE stage
views can consume `PipelineStageRecord` metadata directly. R5.11 promoted the
stage directory header to a public read-only driver API and added
`PipelineStageMetadata` as the shared projection used by the profile writer and
tooling diagnostics. R5.12 typed the profile recording entry points so driver
main stages and incremental-cache sub-events enter the profiler through
`PipelineStageId` / `PipelineProfileSubeventId` instead of scattered profile
name strings. R5.13 added `pipeline_profile_phase_classification(...)`, so the
profile JSON writer and future viewer/LSP adapter consumers can classify a phase
name as a driver main stage, profile sub-event, or unknown through the same
stage directory. M3 implementation continues to reuse the R5
driver/session/query/diagnostics/pipeline path.

As of 2026-05-28, M3.0 module-system closure is complete and M3.1 Generics
Completion is active on the `m3.1` branch. M3.1 does not widen traits,
resources, or the standard library. It turns the existing usable generic
implementation into a stable query-backed system: `GenericTemplateSignature`,
`GenericInstanceSignature`, and `GenericInstanceBody` are the authority
boundaries; generic ABI suffixes, stable ids, and incremental keys derive from
`GenericInstanceKey` / canonical type identity; generic bodies, IR lowering,
LLVM lowering, and native execution consume the same instance identity and
side-table view; `sizeof[T]` / `alignof[T]` close through sema/IR/LLVM inside
generic functions; and method-local generics move into implementation after ABI
and query boundaries are stable. The first M3.1 code step changes generic
struct / enum / function ABI suffixes from session-only `TypeHandle.value`
concatenation to a stable `GenericInstanceKey` fingerprint, so separate compiler
sessions do not generate different instance symbols only because handle
allocation differed. The remaining M3.1 execution entry point is now the
[Aurex M3.1 Generics Completion Plan](m3.1-generics-plan.md); future work
advances by work package and reads only the required local context plus direct
callers/callees by default.

As of 2026-05-28, WP-1B Generic Instance Identity Propagation is complete:
`FunctionSignature`, `EnumCaseInfo`, `GenericEnumInstanceInfo`, and
`GenericTypeAliasInstanceInfo` carry structured `GenericInstanceKey` values.
Generic function and owner-generic method instantiation write identity into
checked signatures in both retained and non-retained side-table modes. Generic
type alias instances record their resolved target type and a target-sensitive
instance-signature incremental key while preserving transparent-alias semantics.
Incremental-cache generic instance signature subjects are collected from checked
metadata, deduplicated by key, and invalid keys are skipped. New white-box
coverage checks function signature identity, generic enum case identity, generic
type alias instance identity, checked module copy/move preservation, and driver
cache rows.

As of 2026-05-28, WP-2 Generic Query Authority is complete. Generic query
provider inputs now carry `GenericTemplateSignatureAuthority`,
`GenericInstanceSignatureAuthority`, and `GenericInstanceBodyAuthority` instead
of binding results only to a bare `IncrementalKey` or body fingerprint. Template
authority records the signature, `ModulePartKey`, namespace, visibility rank,
parameter count, and constraint count. Instance-signature authority records
instance kind, type/const argument counts, param-env predicate count,
value/generic parameter counts, return/receiver flags, unsafe/variadic/
definition flags, and the signature fingerprint. Body authority records the
checked body, signature result, generic side-table layout counts, sparse
fallback count, and retained/local-dense/sparse flags. Incremental-cache
subjects build these authorities from checked metadata and module records, and
provider execution, provider-skip replay, query pruning, and
`query_record_for_subject` all share the same result-fingerprint helpers.
Generic struct / enum instance upstream signature fingerprints are also
resolved-shape-sensitive now, so struct fields and enum payloads changing under
the same `GenericInstanceKey` change the signature result. New query/sema/driver
coverage checks authority valid/invalid paths, semantic fingerprint
sensitivity, generic signature/body dependencies, fallback/cycle behavior,
generic aggregate shape, generic cache rows, query-pruning reuse, and malformed
graph/identity repair.

As of 2026-05-28, WP-3 Generic Body And Lowering Closure is complete. Retained
generic function instances now store their `body` in `GenericFunctionInstanceInfo`,
and `CheckedModule::generic_function_instance_body_view(...)` binds the
instance, signature, side tables, AST item, and body into one sema-authority
view. IR generic declaration/body lowering consumes that view, and `lower_ast`
returns an internal error when a retained generic body view is missing instead
of silently skipping or reinterpreting generic identity in the lowerer.
Incremental-cache generic body checked-result fingerprinting now reads the
retained AST block range for the instance body. `--emit=typed` writes generic
body query rows without lower generic IR rows, while IR/LLVM/native emit modes
collect lower generic instance IR subjects. New and updated sema, IR white-box,
driver-cache, and sample runtime coverage exercise retained/discarded emit-mode
boundaries, generic body views, missing-view rejection, checked-module copy/move
body preservation, and native execution for `generics/basic_m2.ax`.

M1 was discarded because too many concerns expanded at once: standard library
APIs, host support, build-tool examples, selfhost experiments, resource rules,
and language syntax. The result made it hard to tell whether a failure came from
the language, the library, or the tooling. M2 keeps only the active C++ Stage0
compiler, Aurex IR, LLVM backend, and self-contained language samples as the
valid baseline.

The current tree has no `std/` or `selfhost/` directory. Historical std/selfhost
notes are design input only, not current progress.

## Completed

- CLI support for `--check`, `--dump-*`, `--emit=*`, `--opt-level`, `-I`, `-o`,
  `--clang`, and `--clang-arg`.
- Driver support for file IO, module loading, pipeline orchestration, temporary
  LLVM IR generation, and clang invocation. The main path is split into
  `CompilationSession`, `CompilationPipeline`, `FrontendPipeline`,
  `LoweringPipeline`, `BackendPipeline`, and `PipelineStage` records.
- Import resolution through the importer directory and explicit `-I` paths.
- Handwritten lexer/parser with ID-backed AST and dump paths for tokens, AST,
  modules, checked summaries, Aurex IR, and LLVM IR.
- Semantic analysis for types, symbols, functions, ABI names, structs, enums,
  generics, expression types, visibility, and pattern matching. Diagnostics now
  support error/warning/note/help severities; lookup misses for values,
  functions, and types emit `did you mean` help where a scoped candidate exists;
  common type mismatches emit expected/actual notes; duplicate main paths emit
  previous-declaration notes; parser paired-delimiter recovery emits opening
  delimiter notes. The first M2.5 item is now complete: sema diagnostics carry
  explicit kind/category/code metadata at creation time instead of deriving
  machine metadata from message text.
- M2 baseline generics with `[]` syntax only, including explicit `id[T](x)`
  calls and non-empty generic parameter/type-argument lists.
- Literal system support for ordinary strings, C strings, raw/multiline raw
  strings, byte strings, byte literals, Unicode scalar `char`, and integer /
  float type suffixes.
- Fixed array value syntax: array literals `[1, 2, 3]` and repeat literals
  `[0; 128]`, including const, struct-field, IR, LLVM, and native paths.
- Tuple basics: tuple types `(A, B)` / `(A,)`, tuple literals `(a, b)` /
  `(a,)`, and tuple destructuring in local `let` / `var` declarations and
  patterns. Anonymous tuple field access is intentionally rejected.
- ADT-first enum basics, including automatic tags, explicit C-like repr enums,
  generic enums, and multi-field payload destructuring in patterns.
- Minimal non-resource generic constraints through `where`, with built-in
  `Sized`, `Eq`, `Ord`, and `Hash` capabilities. `Eq` / `Ord` are not the same
  as direct comparison operator availability, so `f32` / `f64` support direct
  comparison operators but do not satisfy generic `Eq` / `Ord`. `Hash` is
  marker-only for `bool`, `char`, integer, and pointer types and has no runtime
  hash operator or stable hash ABI yet. Resource capabilities such as `Copy` /
  `Drop` remain deferred.
- Generic type aliases and owner-generic impl blocks such as
  `impl[T] Box[T] { ... }`. Method-local generic parameters remain outside M2.
- Minimal M2 `unsafe` boundaries: `unsafe { ... }`, `unsafe fn`, unsafe
  function pointer types, unsafe call diagnostics, and unsafe-only checks for
  raw pointer dereference, `ptrcast`, `bitcast`, `ptrat`, and `strraw`.
  The no-std checked `str` boundary is now `strvalid(bytes) -> bool` and
  `strfromutf8(bytes) -> str`; failed checked construction returns an empty
  `str` instead of wrapping invalid input as text. `str` also supports checked
  byte-offset slicing with `text[l:r]`; out-of-range bounds or non-UTF-8
  boundary offsets return the empty `str`.
- Default-private visibility, explicit `pub fn` return types, compound
  assignment, unified block-expression bodies, nested block comments, and
  range-only `for i in range(...)`.
- Ordinary root-module `fn main` entry points.
- Typed Aurex IR, IR verifier, conservative pass pipeline, LLVM lowering, and
  native asm/object/executable output through clang. The IR pass path now uses
  `ModulePassManager`, `PassResult`, `PreservedAnalyses`, `VerifierGate`,
  `PassPipelineRunSummary`, and `ModuleAnalysisManager`; verifier gate failures
  carry stable stage/profile/verifier/pass context without changing the original
  verifier body.
- `aurex-profile-v1` driver main-stage phases carry optional `stage` metadata
  with stage id, input, output, diagnostic ownership, and cache/query impact,
  while existing phase names and internal incremental-cache query sub-events are
  preserved. Incremental-cache sub-events now carry optional `parent_stage`
  metadata so profile viewers can attach query sub-events to their owning
  driver stage without treating them as main stages. `PipelineStage` also
  provides diagnostic category to candidate owner-stage lookup, and
  `IdeDiagnostic.owner_stages` consumes those records through public
  `PipelineStageMetadata`. Profile main stages and incremental-cache sub-events
  now enter the profiler through `PipelineStageId` / `PipelineProfileSubeventId`,
  and profile phase consumers classify names through
  `pipeline_profile_phase_classification(...)`, so later IDE/LSP stage views do
  not maintain separate phase-name or diagnostic-stage tables.

## Removed From The Active Track

- The `std/` source tree.
- Host C support and implicit support-source linking.
- Driver std lookup and automatic import-path injection.
- `--stdlib`, `--std-backend`, and `--no-stdlib`.
- Install rules for `share/aurex/std`.
- std/M1/system/build-tool examples and std-specific tests.
- std-name resource-semantics hardcodes.
- The previous selfhost / Stage1 / AIR snapshot implementation in this tree.

These can return only after core syntax, module/package rules, `unsafe`,
slices/strings, and generic constraints are stable. Owned resource libraries
also require the deferred resource-semantics design.

## Quality Gates

Use:

```sh
tools/run_tests.sh
tools/bench.py
make perf
make perf-stress
make perf-stress-threshold
make perf-release-threshold
make perf-ast-stress
```

The test suite covers lexer/parser behavior, CLI/driver behavior, positive and
negative samples, modules, visibility, generics, functions, methods, pattern
matching, error handling, type-system diagnostics, IR lowering, IR verification,
LLVM lowering, native execution, and installed compiler execution.
`tools/bench.py` builds a Release `build/perf` tree and uses Google Benchmark
for frontend hot-path measurements. `make perf` prints the lightweight
JSON-derived Aurex frontend baseline for lexer, lookup-heavy sema, and
generic-instantiation-heavy sema paths plus the AST bulk sema path, then runs a Google Benchmark
process-level comparison against available modern frontend drivers (`clang++`,
`g++`, and `rustc`). `make perf-compare` runs only the cross-frontend
comparison lane.
`make perf-stress` runs `tools/generic_stress.py`, `tools/ast_stress.py`, and
`tools/diagnostic_stress.py`, generating mixed-feature generic, AST bulk, and
diagnostic sources, then recording `aurexc --check` elapsed time plus peak RSS
baselines. `make perf-ast-stress` runs only the AST bulk RSS/time lane. The
stress scripts accept `--max-elapsed-ms`, `--max-rss-mib`,
`--threshold-profile`, and `--threshold-scale`; `make perf-stress-threshold`
runs the default mixed-feature light gate (100/200 generic plus 1000/5000 AST
bulk plus 100/500 diagnostic errors), and CI runs the same gate in the
`stress-thresholds` job. `make perf-release-threshold` now runs the 5000
generic, 2M high-complexity AST, and 5000 diagnostic mixed-feature release gate
as Release+LTO from `build/perf-lto` by default. `make
perf-release-lto-threshold` and `make perf-release-all-threshold` are
compatibility aliases for that same release gate. The stress JSON records
raw/effective thresholds, machine info, profile/scale calibration,
Release/LTO build options, process wall/user/sys/RSS/page-fault metrics, and
the `aurex-profile-v1` compiler phase profile emitted by `--profile-output`.
Generic
function instance signatures, generic struct/enum
`TypeInfo`, and checked enum case display now keep internal semantic keys and
TypeHandle arguments separate from display names, so `--check` does not format
names such as `id[i32]`, `Box[i32]`, or `Maybe[i32]_some` on the hot path;
checked dumps, IR lowering, and diagnostics format them lazily when output
needs them.
`--check` mode no longer retains generic instance side tables. `--emit=typed`
keeps typed generic bodies without lowering, so retained-side-table memory can
be stressed independently from IR/codegen. The default stress shape is now
`--shape=mixed`: generic stress covers generic constraints, impl methods,
pointer aliases, tuple/pair helpers, slices, and pattern matching; AST stress
covers externs, type aliases, structs/enums, impl/methods, generics, tuples,
slices, patterns, try/defer/unsafe/loops, and sampled large-module bulk
expressions; diagnostic stress cycles multiple semantic error families instead
of only missing names. `tools/generic_stress.py --shape=templates` covers many
distinct generic templates at 2000/5000+ scale, while `--shape=instances`
keeps the old many-instantiations comparison shape. IR/native output mode keeps
the lowering tables so codegen behavior stays unchanged. The release AST gate
keeps the deliberately over-complex mixed source instead of falling back to a
throttled toy input, while machine-specific elapsed/RSS data lives in generated
stress JSON and `aurex-profile-v1` phase profiles rather than portable document
baseline numbers.
The expression P0 semantics line now separates intrinsic and final expression
types. Checked and generic side tables contain `expr_intrinsic_types` for
context-free expression types, `expr_types` for contextual final types,
`expr_expected_types` as the final-cache key, and `CoercionRecord` overlay
entries for contextual integer/float literals, `null` to pointer, and slice
coercions. Integer/float/null literals, unary/binary expressions, slices,
array/tuple literals, and if/block/match expressions keep intrinsic types
separate from final types under expected-type analysis. IR lowering continues to
read the final `expr_types` table.
The follow-up expression engineering pass splits the single dispatch body into
one canonical path: `analyze_expr(expr, expected)` owns final-cache lookup and
expected-key recording, `analyze_expr(expr, view, expected)` only classifies the
node, and literal, value/name/call, control, aggregate, projection, operator,
and builtin expressions enter separate helpers. Binary expression checking is
also split into operand contextual typing, operand mismatch diagnostics, integer
literal hazard diagnostics, and operator-result recording, without keeping a
parallel old/new analyzer.
The AST main path now follows the P0-Perf-4 plan: the driver owns the
parser/module AST and passes a mutable reference through sema and IR lowering,
`SemanticAnalyzer(const AstModule&)` is deleted to prevent implicit whole-tree
copies, `CheckedModule::normalized_ast` is a lightweight normalization overlay
and never owns an `AstModule`, sema construction no longer reserves
`exprs/types` as `size+4096`, and postfix suffix creation no longer copies fat
`ExprNode` / `TypeNode` values by value. Per-node C symbol side tables now store
`IdentId` entries in `expr_c_name_ids`, `pattern_c_name_ids`, and
`item_c_name_ids`, with the actual text deduplicated in the checked module
C-name interner instead of allocating one `std::string` per node. The
2026-05-15 compact AST storage pass now stores `TypeNode`,
`ExprNode`, `PatternNode`, `StmtNode`, and `ItemNode` as 32-byte compact headers
plus per-kind payload arenas; module loading moves payload-backed nodes before
remapping instead of depending on fat-vector addresses or `.data()` pointer
arithmetic. Sema item-owner lookup is now explicit `ItemId` lookup rather than
address-derived lookup through `items.data()`. The 2026-05-16 performance pass
also moved sema, IR lowering, and AST dump `ExprNode` hot paths to compact views
and direct payload reads, removing compatibility wrappers and literal fat-node
reconstruction from the sema hot path. The same 2026-05-16 line also landed a
reusable global bump allocator behind the syntax-layer `IdentifierInterner`;
AST type/expr/pattern/stmt/item/module/import name-bearing fields now carry
native `IdentId` payload fields, parser/module-loader/postfix suffix writes intern
through the current `AstModule`, and sema typed lookup keys reuse that AST
module interner instead of maintaining a second private interner. Function,
type, value, generic-template, enum-case, struct-field, method/member, and
local-scope lookup now use `IdentId` typed indexes; checked-module maps no
longer keep parallel string-key lookup paths. Generated ABI/display/dump text is
stored through the bump-backed `IdentifierInterner`; `FunctionSignature`,
`Symbol`, `StructInfo`, `EnumCaseInfo`, `TypeAliasInfo`, and `TypeInfo` keep
`InternedText` / typed ids instead of heap-backed `std::string` payloads. The 2026-05-16
performance line then removed the old fat `ExprNode` production type entirely:
parser construction, `AstModule` storage, module-loader append, and postfix
suffix creation now creates compact expression headers plus per-kind payloads
directly. The expression creation path now uses field-level append/set APIs for
name, unary/binary, call, if/block/match, array, field/index/slice,
struct literal, and cast-like nodes, so the parser no longer builds payload
struct temporaries before arena storage; postfix suffix parsing also writes
call/field/index/slice/generic-apply/struct-literal/try expressions directly into
compact payload storage. The follow-up bump pass backs the `TypeNodeList`, `ExprNodeList`,
`PatternNodeList`, `StmtNodeList`, and `ItemNodeList` header vectors and
per-kind payload vectors with `BumpAllocatorAdapter`; the `IdentifierInterner`
text vector and hash table buckets/nodes are also arena-backed. Parser startup now estimates expression header and per-kind payload capacities
from token shape, takes the vector backing storage from the bump arena up front,
and page-pre-touches the expression arena. Expression creation then only
sequentially emplaces into those reserved ranges, avoiding parser-time vector
growth and first touches of fresh pages on large modules. The reserve estimate is
payload-shaped rather than token-count-shaped, so unused rare payload vectors do
not pre-touch broad empty capacity.
The later lexer/sema bump pass moved lexer output to `TokenBuffer`, with token
vector backing storage owned by a bump arena and reserved up front; token
capacity is no longer capped at 262144, and the lexer no longer page-pre-touches
estimated token capacity that will never be written. Sema persistent storage now uses bump-backed containers for
`CheckedModule`, `GenericSideTables`, `PatternCaseNameTable`, `TypeTable`,
`SymbolTable`, analyzer lookup/cache tables, sema value payload lists
(`FunctionSignature` params/generic args, `StructInfo` fields, `EnumCaseInfo`
payloads, `TypeInfo` tuple/function/generic args), generic template parameter
lists, and generic constraint buckets. Persistent sema name / c-name /
discriminant-text / generic-key fields are `InternedText`, and `CheckedModule`,
`TypeTable`, and `SymbolTable` copies re-intern text into the destination arena
instead of cloning string buffers. Generic function instances use a
bump-backed deque so side-table
references remain stable during nested generic instantiation; retained generic
instances use function-local NodeSpan side tables and share module-level sparse
NodeSpan layouts only for templates with non-contiguous node-id mappings;
generic-method,
enum-case, and visible-module cache buckets are created explicitly from the
analyzer arena instead of default heap vectors from `operator[]`.
The later source-name compaction pass makes `FunctionSignature.name`,
`Symbol.name`, `StructInfo.name`, `StructFieldInfo.name`, `TypeAliasInfo.name`,
and enum case source-name fields borrow `AstModule::identifiers` on the normal
driver path, so source identifiers are not copied into the checked C-name
interner. `CheckedModule` moves now rebind only texts that originally belonged
to the moved module's `c_names`; explicit copies still re-intern into the
destination. ABI symbol validation also uses `std::string_view` keys instead of
building a second temporary `IdentifierInterner` for every C symbol.
IR lowering source-local lookup and verifier symbol de-duplication now also use
interned typed identifiers instead of persistent string-key maps.
Benchmark and stress profiles remain the source of truth for machine-specific
RSS and timing deltas. Generic side-table lifetime is now closed on the main path: sema-only
expected-type and pattern-case caches live in releasable arenas and are dropped
after analysis, retained instances keep only lowering-relevant tables,
non-contiguous NodeSpan sparse ID mappings are shared per template, and tiny
per-instance side-table arenas use 1 KiB blocks instead of the default 64 KiB
floor. The stress lane now includes 5000 generic instances; exact RSS/time
baselines are measurement work rather than a known retained-storage design gap.
Cross-module stable hashes / parallel global IDs now feed the driver
incremental cache: `--incremental-cache <path>` writes schema/version, root and
import paths, source content fingerprints, module rows, and checked
stable/incremental definition rows, and `--check` safely reuses the cache when
all recorded source fingerprints still match. Query-level reuse, lossless
syntax, LSP-native incrementality, and 2M-node / 5000-generic / 5000-error
cross-machine threshold calibration remain later work.
On 2026-05-17, M2.5 started by closing the diagnostic protocol boundary:
semantic diagnostics now map from explicit semantic kinds to stable
category/code values, while message text is presentation only. CLI text, JSON
output, and later diagnostics-query / LSP adapters will consume the same event
semantics. M2.5 then closed the first query-key and dependency-tracking batch;
the next frontend-foundation work moves to lossless syntax and IDE-native entry
points instead of expanding the language surface.
The first M2.5 query-key main-path batch is now closed: `--incremental-cache`
uses query-key pruning by default, and only explicit `--no-query-pruning`
selects the coarse source-fingerprint compatibility path. The current query
foundation covers stable keys, canonical type / generic-instance identity,
`QueryContext` row/edge persistence and replay, source-stage green reuse,
red-green provider skip, profile events, query-graph fuzzing, sanitizer gates,
and release/coverage gates. Future lossless syntax, IDE-native entry points,
and advanced language features must reuse this main path.
The lossless-syntax track is complete for the current M2.5 acceptance boundary:
the lexer has opt-in trivia token emission while the default compile path still
skips trivia; `LosslessSyntaxTree` stores the complete token sequence, exposes
structured node/element/token-leaf APIs, records parent links and contiguous
token spans, validates tree invariants, supports deepest-node lookup by offset,
and can reconstruct either the whole file or any node subtree. The dump shape
has advanced beyond the raw token stream: `source_file` now contains top-level
declaration nodes such as `module_decl`, `import_decl`, and `function_decl`,
direct trivia/eof token leaves, and `block` / `paren_group` / `bracket_group` /
`brace_group` delimiter nodes; `token_stream` remains only the conservative
fallback for non-monotonic hand-built token spans. The parser layer now has a
lossless CST -> AST lowering facade that filters trivia and proves AST parity
with the normal semantic token path. On the query-key side, retain-trivia
`LexFileKey` fingerprints lex with trivia enabled, build-lossless parse result
fingerprints include the CST shape, and the `lossless_tooling` parse provider
depends on the matching retain-trivia lex query. Full local subtree reparsing
remains a deeper follow-up optimization, not a remaining lossless syntax
baseline gap.
The IDE-native engineering entry point is now complete for the current
acceptance boundary through the new `aurex_tooling` target and
`include/aurex/tooling/ide.hpp`. `IdeSnapshot` is built for in-memory buffers and
produces the source manager, lossless tree, AST, checked module, structured
diagnostics, and file/lex/parse/diagnostics query records plus dependency
edges in one pass. Offset token queries, hover, top-level definition lookup,
same-name identifier references, checked-backed global lookup with AST fallback
for local parameters and `let` bindings, and edit-impact node selection are
exposed through this API. Diagnostics are converted to a structured event
stream before CLI rendering or query fingerprinting. It intentionally does not
bind to the LSP protocol; future LSP adapters consume snapshot data instead of
bypassing parser/sema/query.
The follow-up match-exhaustiveness pass replaced the former structural
cartesian-product enumerator with a pattern matrix / usefulness witness search.
Bool, enum payloads, tuples, structs, fixed arrays up to the explicit 4096-column
M2.1 boundary, open integer literals, and dynamic slices are checked through
constructor specialization, default matrices, and symbolic representative slice
lengths. Larger fixed arrays now require an irrefutable arm instead of silently
falling through a hidden implementation limit. Unguarded arms and literal
`if true` guards contribute to exhaustiveness, literal `if false` and dynamic
guards do not. Dynamic slices no longer require a single catch-all arm when
finite length partitions prove coverage, and open integer duplicate literal arms
are rejected as unreachable while missing remaining integer domains get an
open-domain wildcard diagnostic.

## M2 Gaps

- Enum syntax now supports the M2 ADT-first form: ordinary and generic enums may omit the
  base type and case discriminants, tags are assigned automatically, and
  explicit `enum Status: u8 { ok = 0, err = 1 }` remains available for C-like
  repr enums.
- M2 `unsafe` is intentionally minimal. It is a semantic boundary only and does
  not include borrow checking, lifetimes, unsafe traits, unsafe impl blocks,
  unsafe extern blocks, or an ownership/resource model.
- Slices, checked `str` slicing, tuple basics, function pointer types, and M2 pattern ergonomics are
  implemented in the M2 core. Function types are non-capturing function pointer
  values, including `fn(...) -> T`, `unsafe fn(...) -> T`,
  `extern c fn(...) -> T`, and `unsafe extern c fn(...) -> T`; capturing
  closures are still intentionally out of scope. Pattern support includes tuple
  match patterns, slice patterns, struct patterns, nested enum payload
  destructuring, local struct/slice/enum destructuring, binding or-pattern
  alternatives with same-name/same-type consistency, `let ... else`,
  `if value is pattern` / `while value is pattern`, and `if` expression pattern
  conditions. Structural match exhaustiveness now uses pattern matrix /
  usefulness witness search instead of cartesian-product enumeration.
- Generics support minimal `where` capability predicates for `Sized`, `Eq`,
  `Ord`, and `Hash`. User-defined traits, associated types, const generics,
  trait objects, and resource capabilities are still outside M2.
- The M1 language-level `noncopy` / `move` MVP has been removed from the M2
  baseline. M2 keeps ordinary value semantics plus the current array-containing
  value restrictions; copy/drop/borrow/ownership are deferred to a later
  resource-semantics design.
- Minimal safe references are implemented: `&T`, `&mut T`, `&place`,
  `&mut place`, safe reference dereference, mutable-reference write checks, and
  pointer-sized ABI lowering. Raw pointer dereference still requires `unsafe`.
  Borrow checking, lifetimes, borrowed-return rules, alias analysis, and
  ownership/resource semantics remain deferred.

## Current Conclusion

M2 should freeze the basic language surface before any future library layer,
selfhost, or build-tool work is designed again. The active compiler can already
validate language-core samples and produce native output, but the next work
should be syntax and semantic stabilization rather than expanding library
surface area.
