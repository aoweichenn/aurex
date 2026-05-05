# Usage Guide

Version: 0.1.2

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

Dependencies:

- CMake.
- A C++ compiler with C++20 support.
- LLVM development libraries.
- clang for `--emit=asm`, `--emit=obj`, and `--emit=exe`.

## Compile And Run

```sh
build/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Expected output:

```text
hello from Aurex M0
```

## Inspect Intermediate Output

```sh
build/bin/aurexc --emit=tokens examples/hello.ax
build/bin/aurexc --emit=ast examples/hello.ax
build/bin/aurexc --emit=modules tests/positive/module_math.ax
build/bin/aurexc --emit=checked examples/hello.ax
build/bin/aurexc --emit=ir examples/hello.ax
build/bin/aurexc --emit=llvm-ir examples/hello.ax
```

## Native Output

```sh
build/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` is the default mode.

If clang is not in `PATH`:

```sh
build/bin/aurexc --clang /path/to/clang examples/hello.ax -o build/tests/hello
```

Pass arguments through to clang:

```sh
build/bin/aurexc --clang-arg -g --clang-arg -O2 examples/hello.ax -o build/tests/hello
```

## Optimization Level

```sh
build/bin/aurexc --emit=ir --opt-level O1 examples/hello.ax
```

`O0` verifies IR only. `O1` and above enable the current conservative passes.

Equivalent forms:

```sh
build/bin/aurexc --emit=ir -O1 examples/hello.ax
build/bin/aurexc --emit=ir -O O1 examples/hello.ax
```

## Imports And Standard Library

Manual import path:

```sh
build/bin/aurexc -I tests/imports tests/positive/import_path.ax -o build/tests/import_path
```

Explicit standard library:

```sh
build/bin/aurexc --stdlib /path/to/std tests/positive/std_text.ax -o build/tests/std_text
```

Disable std:

```sh
build/bin/aurexc --no-stdlib examples/hello.ax -o build/tests/hello
```

Select std backend:

```sh
build/bin/aurexc --std-backend host-c tests/positive/std_text.ax -o build/tests/std_text
build/bin/aurexc --std-backend none examples/hello.ax -o build/tests/hello
```

Select std through the environment:

```sh
AUREX_STDLIB=/path/to/std build/bin/aurexc tests/positive/std_text.ax -o build/tests/std_text
```

`--stdlib` has higher priority than `AUREX_STDLIB`.

## Tests

```sh
tools/run_tests.sh
tools/check_golden.sh
tools/bench.py
```

## Install

```sh
cmake --install build --prefix build/install
build/install/bin/aurexc tests/positive/std_text.ax -o build/tests/std_text.installed
```

Installed std location:

```text
build/install/share/aurex/std
```

## Troubleshooting

- `native output requires -o`: native output needs an explicit `-o output`.
- `failed to locate Aurex standard library`: set `--stdlib`, set
  `AUREX_STDLIB`, or ensure the install prefix contains `share/aurex/std`.
- clang invocation failed: use `--clang` to select the executable, or
  `--clang-arg` for target-platform arguments.
- To check semantics only, use `--check` or `--emit=check`; no native artifact
  is generated.
