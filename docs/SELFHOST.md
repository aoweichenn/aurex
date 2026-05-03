# Aurex M0 Self-Hosting Plan

## Current State

M0V0.1.8 has a visible self-hosting track, but it is not fully self-hosted.

Implemented now:

- `bootstrap/m0_bootstrap.cpp`: standalone Stage0-mini compiler, built with a
  plain Makefile.
- `selfhost/src/aurex/selfhost/bin/m0c_seed.ax`: first M0 source seed compiled
  by the production C++ Stage0 compiler.
- `selfhost/src/aurex/selfhost/lexer/core.ax`: shared M0 lexer core. It owns
  token constants, the `TokenSpan` token shape, character classification,
  keyword matching, trivia skipping, `scan_token`, and the compatibility wrapper
  `scan_next`.
- `selfhost/src/aurex/selfhost/lexer/dump.ax`: shared token-kind dump helper
  built on the core scanner.
- `selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax`: lexer-oriented M0 smoke
  test. It imports the shared core scanner, scans an embedded source string,
  validates the token kind sequence for a small corpus, and is compiled/run by
  the bootstrap chain.
- `selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax`: range-aware M0 lexer
  smoke test. It imports the shared core scanner and validates token kind plus
  `begin/end` byte ranges.
- `selfhost/src/aurex/selfhost/tool/lexer_dump.ax`: token dump generator written
  in M0. It imports the shared dump helper, scans an embedded source, and prints
  one token kind per line. The output is compared with
  `tests/golden/selfhost_lexer_dump.tokens`.
- `selfhost/src/aurex/selfhost/tool/lexer_file.ax`: file-backed lexer driver
  written in M0. It imports the shared dump helper, reads a source file through
  explicit runtime IO, and compares the token stream for `examples/hello.ax`
  with `tests/golden/selfhost_lexer_file_hello.tokens`.
- `selfhost/src/aurex/selfhost/parser/seed.ax`: first parser seed written in
  M0. It uses a one-token `TokenSpan` cursor and validates a small
  recursive-descent syntax subset.
- `selfhost/src/aurex/selfhost/smoke/parser_smoke.ax`: parser seed entry point
  covering `module`, `import`, `extern c`, function signatures, and an
  `export c fn` body shell.
- `selfhost/src/aurex/selfhost/compiler/io.ax`: explicit runtime IO bridge for
  the selfhost compiler slice.
- `selfhost/src/aurex/selfhost/compiler/imports.ax`: Stage1 import graph loader.
  It parses the entry module/import header with the M0 lexer, derives the import
  root from the entry file path, resolves imported modules to source paths,
  avoids duplicate source emission, and feeds sources to the bundle emitter in
  dependency order.
- `selfhost/src/aurex/selfhost/compiler/subset.ax`: first Stage1 compiler
  parser. It reuses the selfhost lexer and parses a deliberately small M0 subset:
  module/import declarations, an `extern c` block, and `export c fn main`
  containing `puts(c"..."); return <integer>;`.
- `selfhost/src/aurex/selfhost/compiler/emit_c.ax`: C emitter for that subset.
- `selfhost/src/aurex/selfhost/compiler/emit_subset.ax`: compatibility facade
  for the expanding token-stream emitter.
- `selfhost/src/aurex/selfhost/compiler/emit/`: modular token-stream emitter
  pieces split into cursor, writer, type, expression, statement, item, and bundle
  responsibilities.
- `selfhost/src/aurex/selfhost/compiler/driver.ax`: Stage1 compile pipeline
  wiring.
- `selfhost/src/aurex/selfhost/bin/m0c_stage1.ax`: executable entry point for
  the Stage1 compiler slice.
- `selfhost/runtime/runtime.c`: explicit runtime services used by the selfhost
  lexer driver and Stage1 compiler slice.
- `tools/bootstrap_chain.sh`: verifies both the selfhost seed and standalone
  bootstrap path.
- `tools/compare_selfhost_lexer.sh`: compares the M0 file-backed lexer output
  against the production C++ Stage0 lexer token-kind stream for
  `examples/hello.ax` plus every local positive/negative test corpus file.

## Why This Is Not Yet Full Self-Hosting

Full self-hosting means the compiler implementation is written in M0, compiled
by an earlier compiler, and then capable of compiling itself again with stable
output.

The current production compiler is still C++20. The selfhost tree now has a real
Stage1 vertical slice: Stage0 compiles `aurex/selfhost/bin/m0c_stage1.ax`, that
M0 program reads M0 source files, emits C for the supported subset, and the
generated C is compiled and run. That is meaningful bootstrap progress, but it
is not yet a fixed-point self-host because Stage1 cannot compile the full
compiler source.

Current exact capability:

- Stage0 compiles the M0 selfhost lexer, parser seed, and `m0c_stage1.ax`.
- The M0 lexer stream is checked against the C++ Stage0 lexer over the local
  corpus.
- `m0c_stage1` compiles `examples/hello.ax`,
  `selfhost/src/aurex/selfhost/bin/m0c_seed.ax`, and a Stage2 smoke compiler
  bundle into runnable C.
- `m0c_stage1` can now compile `selfhost/src/aurex/selfhost/bin/m0c_stage1.ax`
  as a single entry file. Stage1 reads its imports and emits the compiler bundle
  itself, so the selfhost compiler is no longer limited to externally supplied
  source lists for this path.
- `emit_subset.ax` is the active expansion path for compiling broader selfhost
  files. It is now the first Stage1 backend attempted by the driver, with the
  original narrow emitter retained as fallback.
- `m0c_stage1` can bundle `lexer.core + lexer_smoke`,
  `lexer.core + lexer_ranges`, and `lexer.core + parser.seed + parser_smoke`,
  emit one C file, compile it with `cc`, and run the resulting executables
  successfully.
- The expanding token-stream emitter now handles the selfhost `cast` and
  `ptr_cast`/`bit_cast` forms it needs, maps all primitive scalar type spellings
  used by M0, supports simple assignment statements, `break`, `continue`, empty
  `return`, and emits C wrappers for `extern c @name("...")` declarations.
  `export c fn main` still receives the host entry wrapper; non-main
  `export c fn` declarations are emitted directly as C ABI functions with
  forward declarations and optional `@name("...")` ABI names. Assignment
  handling lives in `emit.assign` and now covers non-bare left sides used by the
  selfhost smoke path, including pointer-field writes. That lets
  Stage1 compile
  `lexer.core + lexer.dump + lexer_file`, link the result with
  `selfhost/runtime/runtime.c`, and reproduce the file-backed lexer golden
  output for `examples/hello.ax`.
- The Stage1 emitter has a dedicated `emit.symbols` module that emits
  module-qualified C symbol macros using the Stage0-compatible
  `m0_<module_path>_<name>` spelling. It also has a source-scanned pointer field
  access path so pointer parameters such as `pair_ptr.value` emit as
  `pair_ptr->value` without relying on special variable names.
- Stage1 statement emission accepts `else if` directly instead of requiring
  nested `else { if ... }` blocks. The production C++ Stage0 parser, semantic
  analyzer, AST dump, module remapper, and C backend support the same shape, so
  this syntax is checked before it enters the selfhost fixed-point smoke path.
- The Stage1 driver first attempts the import-aware entry path, then falls back
  to the original narrow subset parser for legacy single-file smoke sources.

## Milestones

### Stage A: Stable Stage0

- Keep C++ modular compiler clean and testable.
- Maintain exact lexer/parser/sema/codegen boundaries.
- Grow M0 language only with tests.

### Stage B: M0 Runtime

- Add explicit runtime modules for memory, files, diagnostics, and process IO.
- Keep runtime services explicit. No hidden allocator or hidden destructor model.

### Stage C: Lexer In M0

- Port token definitions and scanner to M0.
- Compile M0 lexer with Stage0 `m0c`.
- Compare token dumps between C++ lexer and M0 lexer.

M0V0.1.8 uses the Stage0 module loader in the selfhost tree: the embedded
source driver and file-backed driver both import shared lexer modules, and the
core scanner now returns token ranges through `TokenSpan`.

### Stage D: Parser In M0

- Expand the parser seed into a larger recursive descent parser.
- Produce a stable parse summary from M0.
- Compare parse summaries or AST dumps between C++ parser and M0 parser.

### Stage E: Sema In M0

- Port TypeTable, SymbolTable, and semantic checks.
- Compare diagnostics and checked metadata.

### Stage F: Codegen In M0

- Port C emitter.
- Build `m0c-stage1`.
- Compile selfhost sources with Stage1.

M0V0.1.8 now has a Stage F/G smoke slice: `m0c_stage1.ax` can compile
`examples/hello.ax`, `selfhost/src/aurex/selfhost/bin/m0c_seed.ax`, and the
current selfhost compiler smoke bundle through the M0-written lexer/parser/
emitter path.

### Stage G: Fixed Point

- Stage0 builds Stage1.
- Stage1 builds Stage2.
- Stage2 builds Stage3 from the same selfhost compiler smoke sources.
- Stage2 and Stage3 generated compiler C outputs match byte-for-byte.
- Stage2 and Stage3 generated seed/core smoke outputs match byte-for-byte.

## Current Command

```sh
tools/bootstrap_chain.sh
```

Expected output:

```text
bootstrap chain passed: Stage0 m0c + selfhost lexer smoke/ranges/dump/file + parser seed + M0 stage1/stage2/stage3 fixed-point smoke + standalone bootstrap seed
```

The selfhost subtree also has its own entry point:

```sh
make -C selfhost check
```

Manual Stage1 usage:

```sh
cmake -S . -B build
cmake --build build -j
build/m0c -I selfhost/src selfhost/src/aurex/selfhost/bin/m0c_stage1.ax -o build/m0c_stage1.c
cc build/m0c_stage1.c selfhost/runtime/runtime.c -o build/m0c_stage1
build/m0c_stage1 selfhost/src/aurex/selfhost/bin/m0c_seed.ax build/m0c_seed.stage1.c
cc build/m0c_seed.stage1.c -o build/m0c_seed.stage1
build/m0c_seed.stage1
```

Expected output:

```text
Aurex M0 selfhost seed
```

Both `bootstrap_chain.sh` and `make -C selfhost check` now prove the same
important properties: the M0 lexer driver and the C++ Stage0 lexer agree on the
token kind sequence for the local corpus, the first M0 parser seed can parse a
fixed module/import/extern/function-signature source, and the M0-written Stage1
compiler slice can compile `examples/hello.ax`, the selfhost seed, and a Stage2
smoke compiler bundle into runnable C. The script now also uses Stage2 to build
a Stage3 smoke compiler and requires Stage2/Stage3 compiler output, seed output,
and core smoke output to match byte-for-byte. It also proves Stage1 bundle paths
for lexer smoke, lexer ranges, the parser seed smoke, and the file-backed lexer
tool can compile and run. It also proves the new import-aware single-entry
compiler path by building a Stage2 compiler from only `m0c_stage1.ax`, using
that Stage2 compiler to rebuild the same entry as Stage3 with byte-for-byte C
output, and then using the Stage2 compiler to rebuild the seed. The file-backed
Stage1 bundle also proves explicit
runtime ABI bindings survive through the M0-written emitter path. `stage1_lang.ax`
separately covers the
newly expanded Stage1 statement/type surface, including scalar primitives,
`str`, assignment, loop jumps, and empty `return`. `stage1_core.ax` covers the
next core type layer: `enum`, `opaque struct`, `*mut [N]T` pointer-to-array
declarators, `size_of`, `align_of`, `ptr_addr`, `ptr_from_addr`, module-qualified
C symbol output, pointer field access for non-special pointer parameter names,
recursive emission of nested struct literal values, and non-main
`export c fn` output/prototypes/ABI names, plus pointer-field assignment
emission through `emit.assign`.

They also assert that `selfhost/src/aurex/selfhost/tool/lexer_file.ax` and
`selfhost/src/aurex/selfhost/smoke/parser_smoke.ax` load the expected shared
lexer/parser modules. The same check now covers
`selfhost/src/aurex/selfhost/bin/m0c_stage1.ax` and its compiler modules, so
selfhost module usage is part of the regression suite rather than a
documentation-only claim.
