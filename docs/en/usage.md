# Usage Guide

This document describes the **M2 language-core-no-std** stage. The standard library
is frozen and removed here, so examples and tests should target syntax,
semantics, IR, and backend behavior directly.

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

## Run Hello

```sh
build/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Expected output:

```text
hello from Aurex M2
```

## Emission Modes

```sh
build/bin/aurexc --emit=tokens examples/hello.ax
build/bin/aurexc --emit=ast examples/hello.ax
build/bin/aurexc --emit=checked examples/hello.ax
build/bin/aurexc --emit=ir examples/hello.ax
build/bin/aurexc --emit=llvm-ir examples/hello.ax
build/bin/aurexc -S examples/hello.ax -o build/tests/hello.s
build/bin/aurexc -c examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` is the default mode. Native output from `--emit=asm`,
`--emit=obj`, and `--emit=exe` requires `-o`. The driver-style `-S` and `-c`
forms infer `input.s` and `input.o` when `-o` is omitted.

The command syntax stays clang-style and flat, but `--help` groups options into
primary actions and secondary modifiers. Native backend modifiers such as
`--clang` and `--clang-arg` apply only to native output modes and are rejected
for frontend-only modes such as `--emit=ir` or `--check`.

## Imports

Module lookup order:

1. The importing file's directory.
2. Explicit `-I path` entries.

There is no implicit standard-library path on this branch:

```sh
build/bin/aurexc -I tests/samples/imports --emit=checked tests/samples/positive/modules/import_path.ax
build/bin/aurexc --import-path tests/samples/imports --emit checked tests/samples/positive/modules/import_path.ax
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
