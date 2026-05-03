# Aurex Selfhost Source Layout

`selfhost/src` is organized by module role instead of by temporary smoke-test
entry names.

- `aurex/selfhost/bin/`: executable entry points, including the Stage1 compiler
  driver and small seed binaries.
- `aurex/selfhost/compiler/`: M0 compiler implementation slices.
- `aurex/selfhost/compiler/emit/`: modular pieces of the expanding Stage1
  token-stream C emitter. `symbols.ax` owns the current Stage1 module-to-C
  symbol macro layer, while `item.ax`, `stmt.ax`, `assign.ax`, `expr.ax`, and
  `types.ax` keep top-level item, statement dispatch, assignment, expression,
  and C type emission separated.
- `aurex/selfhost/compiler/imports.ax`: import-aware Stage1 entry loader. It
  scans module/import headers, resolves imported modules to source files, avoids
  duplicate emission, and feeds dependencies to the bundle emitter before the
  entry source.
- `aurex/selfhost/lexer/`: reusable lexer core and token dump helpers.
- `aurex/selfhost/syntax/`: selfhost syntax data modules. `ast.ax` currently
  holds the ID-backed node-pool AST produced by the parser seed.
- `aurex/selfhost/parser/`: reusable parser seed modules. `cursor.ax` owns
  token movement, `types.ax` owns type/signature parsing, `expr.ax` owns the
  iterative operator/frame-stack expression parser, and `seed.ax` owns
  module/item/block orchestration.
- `aurex/selfhost/smoke/`: executable smoke tests for selfhost modules.
  `stage1_lang.ax` specifically guards the Stage1 emitter statement/type
  surface, including `else if` emission. `stage1_core.ax` guards enum, opaque struct, pointer-to-array,
  low-level builtin emission, module-qualified C symbols, and pointer field
  access for pointer parameters with non-special names, plus nested struct
  literal emission, pointer-field assignment emission, and non-main
  `export c fn` output with optional ABI names.
- `aurex/selfhost/tool/`: small command-line tools used by golden tests.

The import root is `selfhost/src`, so file paths intentionally mirror module
names. For example, `aurex.selfhost.smoke.parser_smoke` lives at
`aurex/selfhost/smoke/parser_smoke.ax`.
