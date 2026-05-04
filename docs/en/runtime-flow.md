# Runtime Flow

Version: 0.1.2

## CLI Flow

1. `src/cli/main.cpp` parses arguments.
2. It builds a `CompilerInvocation`.
3. It calls `driver::Compiler::run`.
4. The driver selects dump, check, IR, LLVM IR, or native output based on
   `EmitKind`.

Exit codes:

- `0`: success.
- `1`: compilation, IO, backend, or native toolchain failure.
- `2`: command-line argument error.

## Module Loading Flow

1. Read the root input file.
2. Run lexer/tokenize.
3. Build root AST through parser.
4. Walk imports.
5. Resolve module files through importer directory, `-I`, and std paths.
6. Validate that module declaration matches the import path.
7. Merge AST while preserving module IDs and module paths.

Lookup uses the importer directory first, then `-I` paths and standard-library
import roots. When std is enabled, the driver adds the parent directory of the
std root to the import path, so `import std.core.text;` resolves to
`std/core/text.ax`.

## Semantic Flow

1. Register type names.
2. Analyze struct properties.
3. Register functions, constants, enum cases, and other value names.
4. Analyze constant initializers.
5. Analyze function bodies.
6. Produce `CheckedModule` with type table, expression types, ABI symbols,
   function signatures, and record layout metadata.

The semantic stage does not mutate AST. Later stages query types and ABI
information from `CheckedModule` by AST ID.

## IR Flow

1. Copy the type table from `CheckedModule`.
2. Build record layouts.
3. Declare global constants and functions.
4. Lower global constant initializers.
5. Lower function bodies into basic blocks, terminators, and typed values.
6. Run `verify_module`.
7. At `--opt-level O1` and above, run local mem2reg and CFG cleanup.
8. Run verifier again.

Optimization-level behavior:

- `O0`: input/output verifier only.
- `O1`: current local mem2reg and CFG cleanup.
- `O2` / `O3`: currently the same conservative pass set as `O1`, reserved as
  future extension points.

## Native Output Flow

1. LLVM backend emits LLVM IR text.
2. Driver writes a temporary `.ll` file.
3. If executable output uses std, locate std and select backend support.
4. Invoke clang:
   - `--emit=asm` adds `-S`
   - `--emit=obj` adds `-c`
   - `--emit=exe` links directly
5. Remove the temporary LLVM IR file.

Native output requires `-o`. Dump, check, IR, and LLVM IR modes write to stdout
or only return status and do not require `-o`.

## Standard-Library And Backend Support Link Flow

1. `--no-stdlib` disables std import paths and support linking.
2. When std is enabled, module loading tries to add the std import root, so
   imports such as `std.core.text` resolve to `std/core/text.ax`.
3. Only executable output links std backend support.
4. `--std-backend host-c` links `std/ffi/c/support/host_c.c`.
5. `--std-backend none` links no support source file.

## Installed Standard-Library Lookup

Lookup order:

1. `--stdlib path`
2. `AUREX_STDLIB`
3. Built-in std path from the build
4. Paths relative to the `aurexc` executable:
   - `bin/std`
   - `bin/../std`
   - `bin/../../std`
   - `bin/../share/aurex/std`
   - `bin/../lib/aurex/std`
   - `bin/../../share/aurex/std`
5. Current working directory's `std`

A candidate directory is accepted as a std root only if it contains
`core/text.ax`, `ffi/c/libc.ax`, and `ffi/c/support/host_c.c`.

## Failure Flow

- Lexer/parser/sema failure: print diagnostics and source caret, then return an
  error.
- IR verifier failure: return the verifier error and block backend execution.
- LLVM verifier or clang failure: return a backend/toolchain error.
- Missing std: executable output errors and suggests `AUREX_STDLIB` or
  `--no-stdlib`.
