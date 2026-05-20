# Design And Implementation

## Module Loading

`ModuleLoader` merges the root file and imported files into one `AstModule`. It
tracks loading and loaded files to prevent cycles and duplicate merges. Imported
files must declare a `module` path that matches the import path.

Lookup uses only the importer directory and explicit `-I` paths. Automatic std
injection has been removed.

## Semantic Analysis

Sema currently handles:

- Names, modules, visibility, and re-exports.
- Type resolution, const checks, and layout checks.
- Generic function/struct instantiation.
- Tuple types, tuple literals, and tuple destructuring. Anonymous tuple field
  access is intentionally rejected.
- ADT-first enums with automatic tags, explicit C-like repr enums, multi-field
  payload destructuring in patterns, and pattern matching / exhaustiveness.
- Ordinary value-semantics checks, plus the current restriction that arrays and
  array-containing types cannot be used as by-value function parameters,
  returns, assignment targets, or enum payloads.
- Control-flow constraints for `for`, `defer`, `break`, and `continue`.

Diagnostics carry an explicit semantic kind plus `DiagnosticCategory` and
`DiagnosticCode` at creation time. Message text is presentation only; it no
longer participates in semantic classification. CLI text, `--diagnostics=json`,
and later LSP / diagnostics-query consumers can therefore share one event
stream.

The M1 `noncopy` / `move` / use-after-move semantics have been removed from the
current M2 implementation. std-specific ownership hardcodes have also been
removed. The current implementation does not track language-level copy/drop/move
state; future resource constraints should be redesigned as a separate
resource-semantics track.

## M2.5 Frontend Direction

The first M2.5 query-key batch is now on the default incremental-cache path.
Current typed identities and diagnostic metadata are query-safe data, and file
parse, module graph, item signature, function body, generic instance, and
diagnostics have first-batch query row/edge, replay, and provider-skip profile
coverage. Lossless CSTs, local incremental parsing, and IDE-native entry points
must build on that path instead of creating a second parallel frontend.

## Backend

The AST lowers to Aurex IR, then the pass pipeline and LLVM backend produce LLVM
IR. Native output is delegated to clang. Executable output no longer appends any
standard-library support source.

## Tests

The test harness runs cacheable compiler invocations through the C++ driver, so
tests do not pay script/process startup for every case. Native output still
invokes clang and executes the generated binary when required.
