# Aurex Examples

This branch freezes and removes the standard library so the examples stay at
the language-core layer.

## Layout

- `hello.ax`: minimal native ABI smoke example.
- `libs/common`: small reusable modules for imports, visibility, concrete enums,
  methods, aliases, recursion, and match guards.
- `libs/regex`: multi-module compiled regex library written in Aurex. It uses
  FFI heap allocation for the compiled NFA program and supports anchors, `.`,
  escaped literals, character classes, predefined ASCII classes, grouping,
  alternation, and `*`, `+`, `?`, `{m}`, `{m,n}`, `{m,}` quantifiers.
- `regex_demo.ax`: imports `regex.api` and runs full-match/search cases against
  both convenience APIs and a precompiled regex object.

Build the native hello example:

```sh
build/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Compile a file that imports the shared example modules with:

```sh
build/bin/aurexc -I examples/libs path/to/file.ax --emit=checked
```

Build and run the regex example with:

```sh
build/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
```
