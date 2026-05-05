# Aurex Examples

The examples directory is organized like a small production tree instead of a
flat list of snippets.

## Layout

- `hello.ax`: minimal ABI smoke example.
- `libs/common`: reusable modules shared by the system examples.
- `system/cli_probe`: command-line, environment, and status handling.
- `system/file_journal`: file IO, generic results, text spans, and match guards.
- `system/memory_probe`: arena allocation, buffers, pointer casts, recursive
  helpers, and layout checks.

Build a system example with the shared example modules on the import path:

```sh
build/bin/aurexc -I examples/libs examples/system/file_journal/main.ax -o build/tests/file_journal
build/tests/file_journal
```
