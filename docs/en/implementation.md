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
- Generic function/struct/enum/method instantiation.
- Pattern matching and exhaustiveness.
- Noncopy ownership, `move`, use-after-move, match/try payload ownership.
- Control-flow constraints for `for`, `defer`, `break`, and `continue`.

std-specific ownership hardcodes have been removed. Future constraints should
be expressed through language-level capability, trait, and `where` mechanisms.

## Backend

The AST lowers to Aurex IR, then the pass pipeline and LLVM backend produce LLVM
IR. Native output is delegated to clang. Executable output no longer appends any
standard-library support source.

## Tests

The test harness runs cacheable compiler invocations through the C++ driver, so
tests do not pay script/process startup for every case. Native output still
invokes clang and executes the generated binary when required.
