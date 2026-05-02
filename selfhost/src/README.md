# Aurex Selfhost Source Layout

`selfhost/src` is organized by module role instead of by temporary smoke-test
entry names.

- `aurex/selfhost/bin/`: executable entry points, including the Stage1 compiler
  driver and small seed binaries.
- `aurex/selfhost/compiler/`: M0 compiler implementation slices.
- `aurex/selfhost/compiler/emit/`: modular pieces of the expanding Stage1
  token-stream C emitter. `symbols.ax` owns the current Stage1 module-to-C
  symbol macro layer, while `item.ax`, `stmt.ax`, `expr.ax`, and `types.ax`
  keep top-level item, statement, expression, and C type emission separated.
- `aurex/selfhost/lexer/`: reusable lexer core and token dump helpers.
- `aurex/selfhost/parser/`: reusable parser seed modules.
- `aurex/selfhost/smoke/`: executable smoke tests for selfhost modules.
  `stage1_lang.ax` specifically guards the Stage1 emitter statement/type
  surface, and `stage1_core.ax` guards enum, opaque struct, pointer-to-array,
  low-level builtin emission, module-qualified C symbols, and pointer field
  access for pointer parameters with non-special names.
- `aurex/selfhost/tool/`: small command-line tools used by golden tests.

The import root is `selfhost/src`, so file paths intentionally mirror module
names. For example, `aurex.selfhost.smoke.parser_smoke` lives at
`aurex/selfhost/smoke/parser_smoke.ax`.
