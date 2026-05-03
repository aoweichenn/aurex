# Aurex M0 Selfhost Track

This directory contains the M0-written compiler track. It is not the production
compiler yet; production Stage0 is still the C++ implementation in `src/`.

Current status:

- `src/aurex/selfhost/lexer/`: reusable M0 lexer core and token dump helpers.
- `src/aurex/selfhost/syntax/ast.ax`: ID-backed AST node pools for the parser seed.
- `src/aurex/selfhost/parser/`: iterative parser seed split into cursor, types,
  expression, and orchestration modules.
- `src/aurex/selfhost/compiler/ir/`: Stage1 Aurex IR output path split into
  writer, name mangling, type printing, expression printing, and module emission.
- `src/aurex/selfhost/compiler/driver.ax`: `m0c_stage1` command-line driver.
- `runtime/runtime.c`: explicit host IO/runtime shims used by selfhost tools.

The old selfhost C emitter has been removed from the active tree. Stage1 now
emits `aurex_ir v0` snapshots instead of C source. For syntax not yet covered
by the parser seed, Stage1 records a deterministic `selfhost_module ... lowering(ast_pending)`
placeholder, so the compiler bundle remains visible without pretending it is
fully lowered.

Important boundary: this is not full self-hosting yet. The next milestone is
to expand the M0 AST/parser coverage until Stage1 can lower the complete
selfhost compiler into executable IR that Stage0 can feed through LLVM.

Use:

```sh
make -C selfhost check
tools/bootstrap_chain.sh
```

These checks compile and run the current M0 lexer/parser smoke programs, compare
lexer golden output with Stage0, build `m0c_stage1`, and verify Stage1 IR
snapshots for `examples/hello.ax`, the seed program, the parser smoke program,
the Stage1 IR smoke program, and the current selfhost compiler bundle.
