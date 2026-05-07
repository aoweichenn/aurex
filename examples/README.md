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
- `m1/frontend`: M1 acceptance frontend slice with a source manager, diagnostics,
  lexer, token stream, parser subset, and AST/IR summary checks.
- `m1/axbuild`: M1 acceptance typed build-tool slice with project/target models,
  dependency lists, custom commands, subprocess stdout/stderr capture, cwd/env
  options, source/stamp mtime incremental checks, directory creation, owned
  single-level / recursive directory-entry reads, source discovery by entries, single-level and recursive
  source-discovery counts, target-name lookup caches, duplicate-target detection, target-graph
  validation, topological build order, structured graph diagnostics/messages/
  names/cycle index paths/cycle name paths, build, clean, run, and test flows.

Build a system example with the shared example modules on the import path:

```sh
build/bin/aurexc -I examples/libs examples/system/file_journal/main.ax -o build/tests/file_journal
build/tests/file_journal
```

Build an M1 acceptance example directly:

```sh
build/bin/aurexc examples/m1/frontend/main.ax -o build/tests/m1_frontend
build/tests/m1_frontend
build/bin/aurexc examples/m1/axbuild/main.ax -o build/tests/m1_axbuild
build/tests/m1_axbuild
```
