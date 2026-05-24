# API Reference

Version: 0.1.2

## Command-Line Interface

Basic form:

```sh
aurexc [primary-option] [secondary-options] input.ax [-o output]
```

Native output selected with `--emit=asm`, `--emit=obj`, or `--emit=exe`
requires `-o output`; the driver-style `-S` and `-c` forms infer `input.s` and
`input.o` when `-o` is omitted. Dump and check modes write to stdout or only
return status. The CLI keeps clang-style flat flags while classifying options
internally and in `--help` as primary actions or secondary modifiers. Native
backend modifiers such as `--clang` and `--clang-arg` are valid only for native
output modes.

Common options:

- `--help`: print help.
- `--version`: print compiler version.
- `--dump-tokens` / `--emit=tokens`: print tokens.
- `--dump-lossless` / `--emit=lossless`: print a structured lossless syntax tree that preserves whitespace and
  comments; the current shape is a `source_file` root with declaration nodes, direct trivia/eof token leaves,
  and `block` / delimiter-group nodes. The C++ API exposes parent links, children, token spans, offset lookup,
  structural validation, subtree source reconstruction, and a lossless CST to existing AST parser lowering facade.
- `--dump-ast` / `--emit=ast`: print AST.
- `--dump-modules` / `--emit=modules`: print resolved modules.
- `--dump-checked` / `--emit=checked`: print checked module summary.
- `--dump-ir` / `--emit=ir`: print Aurex IR.
- `--dump-llvm-ir` / `--emit=llvm-ir`: print LLVM IR.
- `--check` / `--emit=check`: run through semantic analysis only.
- `-fsyntax-only`: same as `--check`.
- `-S`: emit assembly, defaulting to `input.s` when `-o` is omitted.
- `-c`: emit object code, defaulting to `input.o` when `-o` is omitted.
- `--emit=asm`: emit assembly.
- `--emit=obj` / `--emit=object`: emit object file.
- `--emit=exe`: emit executable, the default mode.
- `--emit kind`: same as `--emit=kind`.
- `--opt-level O0|O1|O2|O3` / `--opt-level=O0` / `-O O0|O1|O2|O3` / `-O0`: control the IR pass pipeline.
- `--incremental-cache path`: read and write the query-key incremental cache.
- `--query-pruning`: explicitly select the default query-key pruning path.
- `--no-query-pruning`: explicitly use the coarse source-fingerprint compatibility path.
- `--clang path` / `--clang=path`: select clang executable.
- `--clang-arg arg` / `--clang-arg=arg`: pass an argument to clang.
- `-I path` / `-Ipath` / `--import-path path`: add import search path.
- `-o path`: set output path.

Exit codes:

- `0`: success.
- `1`: compilation or toolchain failure.
- `2`: argument error.

## C++ Driver API

Core type: `aurex::driver::CompilerInvocation`

Important fields:

- `input_path`
- `tool_path`
- `output_path`
- `emit_kind`
- `import_paths`
- `clang_path`
- `clang_args`
- `optimization_level`

Entry point:

```cpp
aurex::driver::Compiler compiler;
auto result = compiler.run(invocation);
```

`Compiler::run` returns `base::Result<void>`. On failure,
`result.error().message` contains a user-facing error message. Lex/parse/sema
diagnostics are printed by the driver to stderr.

## Profile JSON API

`--profile-output path` writes `aurex-profile-v1` JSON. Each phase keeps the
existing fields:

- `name`
- `detail`
- `elapsed_ms`
- `rss_mib_after`
- `rss_delta_mib`

If `name` matches a driver main stage through `PipelineStageRecord.profile_name`,
the phase also carries an optional `stage` object:

- `id`
- `input`
- `output`
- `diagnostic_ownership`
- `cache_query_impact`

These fields come from the `PipelineStage` directory and are intended for
profile viewers, cache/query debugging, and later IDE/LSP stage visualization.

Internal cache/query sub-events are still not treated as driver main stages, so
they do not carry a `stage` object. If a sub-event matches
`PipelineProfileSubeventRecord.profile_name`, the phase also carries an optional
`parent_stage` object:

- `id`
- `profile`
- `input`
- `output`
- `diagnostic_ownership`
- `cache_query_impact`

For example, `incremental_cache.query_diff`, `incremental_cache.query_plan`,
`incremental_cache.query_pruning`, and `incremental_cache.query_provider_eval`
belong to `incremental_cache.write`, while
`incremental_cache.source_stage_reuse` belongs to `incremental_cache.lookup`.
Profile viewers can use `stage` for driver main stages and `parent_stage` to
thread cache/query sub-events back to the owning main stage.

The driver also keeps the `DiagnosticCategory` to candidate owner-stage mapping
in `PipelineStage`. Lexer diagnostics may belong to `tokens.lex` or
`module.lex`; parser diagnostics belong to `module.parse`; module-loader
diagnostics belong to `module.append`; sema/type/name-resolution/visibility/
pattern/safety/unsupported/capability/internal diagnostics belong to
`sema.analyze`. This is a shared driver/tooling stage-directory contract, and the
`aurex-diagnostics-v1` text/JSON output fields remain unchanged.

The C++ stage directory API is intentionally read-only:

```cpp
#include <aurex/driver/pipeline_stage.hpp>
```

`PipelineStageRecord` remains the canonical directory row. `PipelineStageMetadata`
is the lightweight projection used by profile JSON emission, tooling diagnostics,
and later profile-viewer/LSP adapters:

```cpp
const driver::PipelineStageRecord& record =
    driver::pipeline_stage_record(driver::PipelineStageId::sema_analyze);
driver::PipelineStageMetadata metadata = driver::pipeline_stage_metadata(record);
std::span<const driver::PipelineStageId> owners =
    driver::pipeline_stage_ids_for_diagnostic_category(base::DiagnosticCategory::type);
```

## C++ Lossless Syntax API

Headers:

```cpp
#include <aurex/syntax/lossless.hpp>
#include <aurex/parse/lossless_parse.hpp>
```

Core API:

```cpp
syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens);
bool valid = tree.is_structurally_valid();
syntax::LosslessNodeId node = tree.node_at_offset(offset);
std::span<const syntax::Token> node_tokens = tree.token_span(node);
std::string text = tree.reconstruct_text(node);
auto ast = parse::lower_lossless_syntax_to_ast(tree, diagnostics);
```

`LosslessSyntaxTree` retains the full token/trivia sequence and stores an
immutable-style tree through `LosslessNode` / `LosslessElement`. Each node
records its parent, child range, contiguous token span, and source range.
`LosslessNodeKey` provides a stable identity from kind/range/token span/depth
for query, local reparse, and IDE tooling indexes.

## C++ IDE Tooling API

Header:

```cpp
#include <aurex/tooling/ide.hpp>
```

Core API:

```cpp
tooling::IdeSnapshotRequest request;
request.path = "/workspace/main.ax";
request.text = source_text;
tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request);

auto token = tooling::token_info_at_offset(snapshot, offset);
auto hover = tooling::hover_at_offset(snapshot, offset);
auto definition = tooling::definition_at_offset(snapshot, offset);
std::vector<tooling::IdeReference> refs =
    tooling::references_at_offset(snapshot, offset);
tooling::IdeEditImpact impact =
    tooling::edit_impact_for_range(snapshot, edit_begin, removed_length);
```

`IdeSnapshot` is built for in-memory buffers. A snapshot owns the source manager,
lossless syntax tree, AST, checked module, structured diagnostics, and the
file/lex/parse/diagnostics query records plus dependency edges. The current
entry point covers diagnostics, token/hover queries, top-level definition
lookup, same-name identifier references, checked-backed globals with AST-local
fallbacks for parameters and `let` bindings, and edit-impact node selection.
Each `IdeDiagnostic` also carries `owner_stages` metadata sourced from
`PipelineStageMetadata` for later LSP/IDE stage visualization; the
`aurex-diagnostics-v1` output protocol is unchanged.
Diagnostics are normalized into a structured event stream before query
fingerprinting or CLI rendering. It is the data source for an LSP adapter, not
a direct dependency on the LSP protocol.

## IR Pass API

Headers:

- `include/aurex/ir/pass_pipeline.hpp`
- `include/aurex/ir/pass_manager.hpp`
- `include/aurex/ir/analysis_manager.hpp`

Core API:

```cpp
base::Result<void> run_pass_pipeline(Module& module, const PassPipelineOptions& options);
base::Result<PassPipelineRunSummary> run_pass_pipeline_with_summary(
    Module& module, const PassPipelineOptions& options);
```

`PassPipelineOptions` controls:

- `optimization_level`
- `verify_input`
- `verify_output`
- `enable_mem2reg`
- `enable_cfg_cleanup`
- `verify_after_each_pass`
- `stage_name`
- `stage_profile_name`

`run_pass_pipeline` remains compatible with existing callers and only reports
success or failure. `run_pass_pipeline_with_summary` additionally reports the
scheduled/executed pass counts, whether the pipeline changed IR, and the final
preserved analysis set.

Lightweight pass manager API:

- `ModulePassManager`: schedules `ModulePass` records in order without virtual
  dispatch or heavy templates.
- `ModulePass`: records `PassId`, stable pass name, and a `ModulePassRun`
  function entry; pass callbacks can access `ModuleAnalysisManager`.
- `PassResult`: declares whether a pass changed IR and which
  `PreservedAnalyses` it preserved.
- `PreservedAnalyses`: describes analyses that remain valid after a pass, such
  as CFG, type table, symbol table, and record layout.
- `ModuleAnalysisManager`: lazily builds and caches CFG, dominance, and
  value-use analyses; when a pass changes IR, unpreserved cached analyses are
  invalidated according to `PreservedAnalyses`.
- `VerifierGate`: centralizes input verifier, output verifier, and opt-in
  after-each-pass verifier control. Failure messages carry stable
  `stage=... profile=... verifier=...` context; after-pass failures also carry
  `pass=...`. The original verifier body and `ErrorCode` are preserved, with
  the body kept after `: `.

The driver path fills `stage_name` / `stage_profile_name` from the
`PipelineStageRecord` for `PipelineStageId::ir_pass_pipeline`. Direct IR API
calls default to `ir_pass_pipeline` / `ir.pass_pipeline`, matching the driver
stage directory.

Optimization levels:

- `OptimizationLevel::none`: `O0`
- `OptimizationLevel::basic`: `O1`
- `OptimizationLevel::standard`: `O2`
- `OptimizationLevel::aggressive`: `O3`

## Aurex Source ABI API

Program entry is an ordinary root-module function:

```m0
fn main() -> i32 {
    return 0;
}
```

`fn main` may return `i32` or `void`, and may optionally accept
`(argc: i32, argv: *mut *mut u8)`.

Scope cleanup can use `defer`:

```m0
fn use_buffer(buffer: *mut u8) -> i32 {
    defer free_bytes(buffer);
    return 0;
}
```

`defer` currently accepts function-call statements. Cleanup calls run in reverse
order when the current lexical scope exits, including normal exits, `return`,
`break`, and `continue` paths. Return statements evaluate the return expression
before running cleanup.

C ABI is declared with `extern c`:

```m0
extern c {
    fn puts(s: *const u8) -> i32 @name("puts");
    fn printf(format: *const u8, ...) -> i32 @name("printf");
}
```

`...` is supported only on `extern c` declarations and must appear at the end of
the parameter list. Variadic arguments use C ABI default promotions, for example
`bool` / `u8` / `i16` to `i32` and `f32` to `f64`.

C ABI symbols are exported with `export c fn`:

```m0
export c fn plugin_entry() -> i32 @name("plugin_entry") {
    return 42;
}
```
