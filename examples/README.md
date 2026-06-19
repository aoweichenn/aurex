# Aurex Examples

This branch freezes and removes the standard library so the examples stay at
the language-core layer.

## Layout

- `hello.ax`: minimal native ABI smoke example.
- `diagnostic_showcase.ax`: intentionally invalid program that emits a compact
  set of representative semantic diagnostics in one run.
- `libs/common`: small reusable modules for imports, visibility, concrete enums,
  methods, aliases, recursion, and match guards.
- `libs/regex`: multi-directory compiled regex library written in Aurex. Its
  stable facade is `regex.api`; internals are split across `core`, `config`,
  `syntax`, `runtime`, `compile`, `vm`, and `ops`. It uses FFI heap allocation
  for the compiled NFA program and supports anchors, `.`, escaped literals,
  character classes, predefined ASCII classes, POSIX/property classes, inline
  flags, scoped flags, lazy/ungreedy quantifiers, word boundaries, absolute
  anchors, capturing groups, named captures, non-capturing groups, alternation,
  `*`, `+`, `?`, `{m}`, `{m,n}`, `{m,}` quantifiers, find/captures cursors,
  buffer-based replacement, split cursors, and compile error offsets.
- `regex_demo.ax`: imports `regex.api` and runs full-match/search cases against
  both convenience APIs and a precompiled regex object.
- `regex_phase1.ax`: exercises first-phase regex APIs: named captures,
  `find_iter`, `captures_iter`, `replace_all`, `split_iter`, and error
  diagnostics.
- `regex_industrial.ax`: exercises the safer industrial regex surface:
  flags, lazy/ungreedy matching, boundaries, expanded escapes, Unicode scalar
  matching, Unicode properties/case folding, convenience APIs, and invalid
  escape diagnostics.
- `regex_stress.ax`: runs repeated compiled regex searches/fullmatches and
  checks the regex resource budget APIs.
- `regex_suite.ax`: imports the regex smoke examples and runs their exported
  `run_*` entry points in one native executable.

Build the native hello example:

```sh
build/full-llvm/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Print a representative batch of diagnostics:

```sh
build/full-llvm/bin/aurexc --check examples/diagnostic_showcase.ax
build/full-llvm/bin/aurexc --check --diagnostics=json examples/diagnostic_showcase.ax
```

Compile a file that imports the shared example modules with:

```sh
build/full-llvm/bin/aurexc -I examples/libs path/to/file.ax --emit=checked
```

Build and run the full regex example suite with:

```sh
build/full-llvm/bin/aurexc -I examples -I examples/libs examples/regex_suite.ax -o build/tests/regex_suite
build/tests/regex_suite
```

Each regex smoke file also keeps its own `fn main() -> i32`, so individual
examples can still be built directly while tests use `regex_suite.ax` to avoid
recompiling the shared regex library for every checked-surface and runtime
check.
