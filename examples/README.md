# Aurex Examples

This branch freezes and removes the standard library so the examples stay at
the language-core layer.

## Layout

- `hello.ax`: minimal native ABI smoke example.
- `libs/common`: small reusable modules for imports, visibility, concrete enums,
  methods, aliases, recursion, and match guards.

Build the native hello example:

```sh
build/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

Compile a file that imports the shared example modules with:

```sh
build/bin/aurexc -I examples/libs path/to/file.ax --emit=checked
```
