# Aurex M0 User Guide

Version: M0V0.1.8

## 1. What M0 Is

Aurex M0 is the bootstrap core of Aurex. It is intentionally small: no type
inference, no generics, no overloads, no hidden copies, and no implicit
conversions. The first backend emits C.

The current production compiler is written in C++20 and is split into libraries:

```text
source -> lexer -> tokens -> parser -> AST -> sema -> checked module -> C emitter
```

The lexer and parser are handwritten. No ANTLR, Flex, Bison, or parser
generator is used.

## 2. Build

From `aurex/`:

```sh
cmake -S . -B build
cmake --build build -j
```

The compiler executable is:

```sh
build/m0c
```

## 3. Compile A Program

```sh
build/m0c examples/hello.ax -o build/hello.c
cc build/hello.c -o build/hello
build/hello
```

Expected output:

```text
hello from Aurex M0
```

## 4. CLI Reference

```sh
build/m0c --help
build/m0c --version
build/m0c --dump-tokens examples/hello.ax
build/m0c --dump-ast examples/hello.ax
build/m0c --dump-modules tests/positive/module_math.ax
build/m0c --check examples/hello.ax
build/m0c --emit=ast examples/hello.ax
build/m0c --emit=check examples/hello.ax
build/m0c --emit=c examples/hello.ax -o build/hello.c
build/m0c -I tests/imports tests/positive/import_path.ax -o build/import_path.c
```

Options:

- `--version`: print the compiler version.
- `--dump-tokens`: run lexer only and print tokens.
- `--dump-ast`: run lexer and parser, then print a stable AST dump.
- `--dump-modules`: resolve imports and print loaded module names plus paths.
- `--check`: run lexer, parser, and semantic analysis without emitting C.
- `--emit=tokens`: same as `--dump-tokens`.
- `--emit=ast`: same as `--dump-ast`.
- `--emit=modules`: same as `--dump-modules`.
- `--emit=check`: same as `--check`.
- `--emit=c`: emit C. This is the default.
- `-o path`: write generated C to `path`.
- `-I path`: add an import root. `import a.b;` searches for `a/b.ax` in the
  importing file's directory, then each `-I` root.

## 5. Language Snapshot

Supported top-level constructs:

- `module`
- `import` with recursive C++ Stage0 loading.
- `extern c`
- `export c fn`
- `opaque struct`
- `struct`
- `enum`
- `const`
- `fn`

Supported statements:

- `let`
- `var`
- `if` / `else`
- `while`
- `break`
- `continue`
- `return`
- expression statement
- assignment statement

Supported types:

- `void`, `bool`
- `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`
- `isize`, `usize`
- `f32`, `f64`
- `str`
- `*mut T`
- `*const T`
- `[N]T`
- named struct / enum / opaque struct types

Supported expressions:

- integer literals
- bool literals
- `null`
- string literals
- C string literals: `c"..."`
- byte literals
- names
- unary and binary expressions
- calls
- field access
- indexing
- struct literals
- `cast(T, x)`, `ptr_cast(T, x)`, `bit_cast(T, x)`

## 6. Semantic Rules Currently Enforced

M0V0.1.8 enforces:

- no function overloading
- no shadowing
- no assignment to `let`
- explicit return type matching
- `if` / `while` conditions must be `bool`
- array types cannot be function parameters or returns
- arrays and structs containing arrays cannot be assigned by value
- opaque structs can only be used behind pointers
- function argument types must match exactly, except integer literals into integer destinations
- break/continue are only valid inside `while`

Some industrial rules still need deeper lowering support:

- guaranteed left-to-right evaluation in emitted C for all complex expressions
- temporary generation for side-effectful call arguments
- complete constant evaluation
- explicit export/visibility controls beyond direct imports
- full C ABI validation

## 6.1 Module Loading

The C++ Stage0 compiler now resolves imports before semantic analysis:

```m0
module app;
import shared.util;
```

Resolution maps `shared.util` to `shared/util.ax`. The importer directory is
searched first, followed by every `-I` path. Stage0 keeps each loaded module's
top-level namespace while still using one checked module internally.

Current limitations:

- imports expose direct top-level names; there is not yet an explicit public/private export model;
- the importing module's own top-level names take precedence over imported names;
- if two direct imports provide the same referenced name, sema reports an ambiguity;
- imported module declarations must match the import path;
- missing imports and module-name mismatches are reported at the import/module
  source range;
- if multiple `-I` roots contain the same module path, the first match wins;
- `--dump-tokens` still dumps only the root source file.

## 7. Tests

```sh
tools/run_tests.sh
```

This runs:

- CMake configure/build
- version/help checks
- token dump and AST dump checks
- semantic positive and negative tests
- hello end-to-end codegen
- standalone bootstrap compiler check
- selfhost seed check
- selfhost lexer smoke check
- selfhost lexer dump golden check
- selfhost file-backed lexer golden check

## 8. Performance Smoke Test

```sh
tools/bench.py
```

This generates a synthetic M0 source file, then measures token dump, AST dump,
and C emission wall time. It is a smoke test, not a statistically rigorous
benchmark suite.

## 9. Bootstrap And Selfhost

There are two related directories:

- `bootstrap/`: standalone C++20 Stage0-mini compiler with a Makefile.
- `selfhost/`: M0 source seeds and runtime placeholders for the future
  self-hosted compiler.

Run:

```sh
tools/bootstrap_chain.sh
```

M0V0.1.8 is not fully self-hosted yet. The goal is explicit and testable:
move compiler components into `selfhost/src`, compile them with Stage0 `m0c`,
then compare Stage1 output against Stage0 output. The current tree already
contains `selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax`, a small M0
lexer-oriented program that validates a token kind sequence for an embedded
source string.

M0V0.1.8 splits the selfhost lexer into actual M0 modules:

- `selfhost/src/aurex/selfhost/lexer/core.ax`: token constants, `TokenSpan`,
  character predicates, keyword matching, trivia skipping, `scan_token`, and
  the compatibility wrapper `scan_next`.
- `selfhost/src/aurex/selfhost/lexer/dump.ax`: token-kind printing and
  `dump_tokens`.
- `selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax` and
  `lexer_ranges.ax`: small smoke entry programs that import and reuse those
  modules.
- `selfhost/src/aurex/selfhost/tool/lexer_dump.ax` and `lexer_file.ax`: small
  golden-test tools built on the shared lexer modules.

This is not full self-hosting yet, but it is an important structural step: the
M0 selfhost code now depends on the Stage0 module loader instead of copying the
same scanner into every entry point.

`selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax` proves the scanner now
returns parser-ready `kind/begin/end` byte offsets for a fixed source string.
`selfhost/src/aurex/selfhost/smoke/parser_smoke.ax` is the first parser seed
smoke. The parser is now split into `parser.cursor`, `parser.types`,
`parser.expr`, and `parser.seed`; it produces an ID-backed `AstModule` and
covers `module`, `import`, `extern c`, function signatures, an `export c fn`
body, call expressions, full call-argument expressions, unary expressions, and
binary precedence expressions.

The selfhost directory now also includes
`selfhost/src/aurex/selfhost/tool/lexer_file.ax`, which reads
`examples/hello.ax` through explicit runtime file IO and compares its token-kind
stream with `tests/golden/selfhost_lexer_file_hello.tokens`.
`tools/compare_selfhost_lexer.sh` additionally compares that M0 lexer stream
with the production C++ Stage0 lexer stream over the local corpus.

```sh
make -C selfhost check
```

When compiling selfhost sources manually, pass the selfhost import root:

```sh
build/m0c -I selfhost/src selfhost/src/aurex/selfhost/tool/lexer_file.ax -o build/lexer_file.c
build/m0c -I selfhost/src selfhost/src/aurex/selfhost/smoke/parser_smoke.ax -o build/parser_smoke.c
```
