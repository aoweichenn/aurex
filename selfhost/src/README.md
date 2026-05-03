# Aurex Selfhost Source Layout

`selfhost/src` mirrors module names and is organized by compiler responsibility:

- `aurex/selfhost/bin/`: executable entry points, including `m0c_stage1.ax`.
- `aurex/selfhost/compiler/`: M0 compiler implementation slices.
- `aurex/selfhost/compiler/ir/`: Stage1 Aurex IR output modules. `writer.ax`
  owns output primitives, `names.ax` owns stable module/name spelling,
  `types.ax` owns type text, `expr.ax` owns expression value text, and `emit.ax`
  connects parser AST to the IR snapshot format.
- `aurex/selfhost/lexer/`: reusable lexer core and token dump helpers.
- `aurex/selfhost/syntax/`: syntax data modules, currently the ID-backed
  `AstModule` node pools.
- `aurex/selfhost/parser/`: parser seed modules. `cursor.ax` owns token
  movement, `types.ax` owns type/signature parsing with an explicit pointer
  stack, `expr.ax` owns the iterative operator/frame-stack expression parser,
  and `seed.ax` owns module/item/block orchestration.
- `aurex/selfhost/smoke/`: executable smoke tests for selfhost modules.
- `aurex/selfhost/tool/`: small command-line tools used by golden tests.

The import root is `selfhost/src`. For example,
`aurex.selfhost.smoke.parser_smoke` lives at
`aurex/selfhost/smoke/parser_smoke.ax`.
