# Usage Guide

This document describes the current **M4 trait/protocol release baseline**.
The standard library remains frozen and removed, so examples and tests should
target syntax, semantics, IR, backend behavior, and the static trait surface
directly.

## Build

```sh
cmake -S . -B build/full-llvm
cmake --build build/full-llvm -j
```

## Run Hello

```sh
build/full-llvm/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Expected output:

```text
hello from Aurex M2
```

## Emission Modes

```sh
build/full-llvm/bin/aurexc --emit=tokens examples/hello.ax
build/full-llvm/bin/aurexc --emit=lossless examples/hello.ax
build/full-llvm/bin/aurexc --emit=ast examples/hello.ax
build/full-llvm/bin/aurexc --emit=checked examples/hello.ax
build/full-llvm/bin/aurexc --emit=ir examples/hello.ax
build/full-llvm/bin/aurexc --emit=llvm-ir examples/hello.ax
build/full-llvm/bin/aurexc -S examples/hello.ax -o build/tests/hello.s
build/full-llvm/bin/aurexc -c examples/hello.ax -o build/tests/hello.o
build/full-llvm/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/full-llvm/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/full-llvm/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` is the default mode. Native output from `--emit=asm`,
`--emit=obj`, and `--emit=exe` requires `-o`. The driver-style `-S` and `-c`
forms infer `input.s` and `input.o` when `-o` is omitted.
`--emit=lossless` / `--dump-lossless` prints a structured lossless syntax tree
that preserves whitespace, line comments, and block comments. The current dump
has a `source_file` root with top-level declaration nodes such as
`module_decl`, `import_decl`, and `function_decl`, direct trivia/eof token
leaves, and delimiter groups such as `block`, `paren_group`, `bracket_group`,
and `brace_group`. It can reconstruct the original source text; `token_stream`
is now only the conservative fallback for non-monotonic hand-built token spans.
The lossless tree also records parent links, contiguous token spans, and stable
node keys, supports deepest-node lookup by offset and subtree source
reconstruction, and can enter the existing AST parser through
`parse::lower_lossless_syntax_to_ast`. In-memory IDE consumers use
`aurex/tooling/ide.hpp`, which reuses this lossless/query path for diagnostics,
hover, definition lookup, references, and edit-impact node selection. The
snapshot now resolves checked-backed globals first and falls back to AST-local
parameters and `let` bindings when checked data does not retain a symbol. The
diagnostic path is also normalized through a structured event stream before CLI
rendering or query fingerprinting.

The command syntax stays clang-style and flat, but `--help` groups options into
primary actions and secondary modifiers. Native backend modifiers such as
`--clang` and `--clang-arg` apply only to native output modes and are rejected
for frontend-only modes such as `--emit=ir` or `--check`.

## Incremental Cache

```sh
build/full-llvm/bin/aurexc --check --incremental-cache build/main.axic examples/hello.ax
```

`--incremental-cache` uses query-key pruning by default: when source
fingerprints do not all match, the driver first tries query-key source-stage
green reuse and then records red/green provider-skip profile data during cache
writes. `--query-pruning` only confirms the default behavior; `--no-query-pruning`
explicitly selects the coarse source-fingerprint compatibility path.

## Trait / Protocol Surface

M4 supports nominal static traits, explicit impls, generic trait predicates,
static trait method calls, and associated-type equality constraints:

```aurex
trait Source {
    type Item;
    fn get(self: &Self) -> Self.Item;
}

struct Bytes {
    value: i32;
}

impl Source for Bytes {
    type Item = i32;

    fn get(self: &Bytes) -> i32 {
        return self.value;
    }
}

fn read_i32[T](value: &T) -> i32 where T: Source[Item = i32] {
    return value.get();
}
```

This is a static-dispatch surface. Dynamic trait objects, vtable ABI, object
safety, default methods, specialization, generic associated types, associated
constants, and RAII/resource semantics are still future design tracks.

## Imports

Module lookup order:

1. The importing file's directory.
2. Explicit `-I path` entries.

There is no implicit standard-library path on this branch:

```sh
build/full-llvm/bin/aurexc -I tests/samples/imports --emit=checked tests/samples/positive/modules/import_path.ax
build/full-llvm/bin/aurexc --import-path tests/samples/imports --emit checked tests/samples/positive/modules/import_path.ax
```

## C FFI

This branch does not provide std C FFI wrappers. Language-core tests that need C
functions declare the smallest local `extern c` boundary:

```aurex
extern c {
    fn puts(text: *const u8) -> i32 @name("puts");
}
```

## Tests

```sh
tools/run_tests.sh
```

The suite covers lexer/parser, modules, visibility, generics, sum types,
pattern matching, `?`, `defer`, `for`, value semantics, IR, LLVM lowering,
native execution, and installed compiler execution. Standard-library APIs, std
host support, and M1 system/build-tool examples are out of scope for M2.
