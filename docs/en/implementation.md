# Design And Implementation

Version: 0.1.2

## Lexer

The lexer takes source text and source id, then returns a token array. Tokens
carry kind, text slice, and source range. The selfhost lexer uses `TokenSpan`
with kind, begin, and end for comparison with Stage0 lexer output.

Implementation notes:

- Tokens do not copy source text; text slices reference content owned by
  `SourceManager`.
- Diagnostics use source ranges, allowing the driver to print line, column, and
  caret output.
- Golden tests treat token dumps as a stable interface.

## Parser

The Stage0 parser is handwritten recursive descent. It consumes a token span and
produces AST. AST nodes are stored in ID-addressed pools to avoid node copies
and pointer lifetime issues. The selfhost parser seed is split into cursor,
type, expr, and seed modules; expression parsing uses explicit stacks.

AST design:

- `TypeId`, `ExprId`, `StmtId`, `ItemId`, and `ModuleId` index their respective
  vectors.
- Items record their owning module, so merged modules still preserve definition
  origin.
- AST dump is for structural regression; it is not a semantic report.

The selfhost parser seed currently supports module/import declarations, one
`extern c` block, multiple `export c fn` items, function parameters and return
types, expression statements, and return statements. Each block records its own
statement range and expression range, so Stage1 IR output can scope expression
values to the current function block.

## Module Loader

The module loader merges the root file and imported files into one `AstModule`.
It records loaded files and modules to avoid duplicate loading and uncontrolled
recursive imports. Module declarations must match import paths; for example,
`import std.text;` expects `module std.text;`.

## Sema

Semantic analysis produces `CheckedModule`:

- `TypeTable`
- expression type table
- syntax type handle table
- item ABI name table
- function signature table
- struct / enum metadata

Semantic rules emphasize explicit cost: no overloads, no shadowing, no implicit
conversions, no array by-value parameters or returns, and opaque structs only
behind pointers.

ABI names are decided in sema. Normal Aurex symbols use module-path mangling,
while `extern c` and `export c` connect to the C/host world through explicit ABI
names.

## IR

Aurex IR is a backend-independent typed CFG/SSA-like layer. Core entities:

- `Module`
- `Function`
- `BasicBlock`
- `Terminator`
- `Value`
- `RecordLayout`
- `GlobalConstant`

Functions record source name, ABI symbol, linkage, calling convention, return
type, and parameter signature.

The IR verifier checks block, value, terminator, type, and reference
consistency. The driver runs the verifier after lowering and again after the
pass pipeline by default.

## Pass Pipeline

`run_pass_pipeline` verifies input and output by default. `O0` performs no
optimization. `O1+` currently enables:

- Local mem2reg: promotes only same-block, non-escaping scalar slots.
- CFG cleanup: removes unreachable blocks, merges safe empty branch blocks, and
  folds conditional branches whose targets are identical.

This is not full SSA construction. Cross-block mem2reg and phi insertion remain
future work.

`O2` and `O3` currently reuse the same conservative pass set. They exist as
CLI/API compatibility and future extension points.

## LLVM Backend

The LLVM backend verifies Aurex IR, declares records, constants, and functions,
then lowers basic blocks and values. The resulting LLVM module is verified by
LLVM before being printed as LLVM IR text.

Native output does not generate code directly from AST. It always goes through
Aurex IR and LLVM IR, so dump paths and execution paths share the same IR
contract.

## Driver

The driver owns:

- file IO;
- module loading;
- sema / IR / backend orchestration;
- temporary LLVM IR files;
- clang invocation;
- std backend support lookup and linking.

The driver returns early for dump/check modes to avoid unnecessary backend
work. Native output creates a temporary `.ll` file, invokes clang, and then
cleans the temporary file.

## Standard Library

`.ax` standard-library modules define language-level APIs. The host-c backend
support provides host symbols needed by selfhost IO. Stable symbols use the
`aurex_std_v0_*` namespace; old `aurex_std_*` wrappers remain for compatibility.

`std/native_support.c` is a compatibility shim that includes the current default
backend support. The new driver path selects `std/support/host_c.c` by backend,
so future support backends can be added without changing `.ax` std APIs.

## Install Layout

CMake installs `aurexc` into `bin` and the `std` directory into
`share/aurex/std`. Runtime lookup derives the executable directory from
`tool_path`, so the installed prefix can be moved while std remains discoverable
inside the same prefix.

## Test Implementation

The main test script builds the project, checks documentation layout, verifies
CLI help, runs positive and negative samples, checks IR/LLVM output, tests std
backend behavior, validates installed std lookup, and compiles selfhost slices.
