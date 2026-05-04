# Aurex M0 Selfhost Track

This directory contains the M0-written compiler track. It is not the production
compiler yet; production Stage0 is still the C++ implementation in `src/`.

Current status:

- `src/aurex/selfhost/lexer/`: reusable M0 lexer core and token dump helpers.
- `src/aurex/selfhost/syntax/ast.ax`: ID-backed AST node pools for the parser seed.
- `src/aurex/selfhost/parser/`: iterative parser seed split into cursor, types,
  expression, and orchestration modules.
- `src/aurex/selfhost/compiler/ir/`: Stage1 TAC output path split into
  writer, name mangling, type printing, three-address-code expression printing,
  and module emission.
- `src/aurex/selfhost/compiler/driver.ax`: `aurexc_stage1` command-line driver.
- Standard-library native support is linked by Stage0 `aurexc` for executable
  outputs; selfhost tools no longer pass an explicit host-support source.

The old selfhost C emitter has been removed from the active tree. Stage1 now
emits `aurex_tac v0` snapshots instead of C source. The current seed parser can
lower modules with an `extern c` block and multiple `export c fn` items, and the
TAC snapshot path now emits expression temporaries only for the function block
being written. Stage1 records deterministic pending markers for parser, sema,
and AIR boundaries. The current selfhost compiler bundle parses and passes
Stage1 sema; remaining placeholders are AIR lowering/verifier gaps.

Important boundary: this is not full self-hosting yet. The next milestone is
to expand the M0 AST/parser coverage until Stage1 can lower the complete
selfhost compiler into executable IR that Stage0 can feed through LLVM.

Use:

```sh
make -C selfhost check
tools/bootstrap_chain.sh
```

These checks compile and run the current M0 lexer/parser smoke programs, compare
lexer golden output with Stage0, build `aurexc_stage1`, and verify Stage1 TAC
snapshots for `examples/hello.ax`, the seed program, the parser smoke program,
the Stage1 TAC smoke program, and the current selfhost compiler bundle.
