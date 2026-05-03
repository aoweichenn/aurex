# Aurex M0 Self-Hosting Plan

## Current State

M0V0.1.8 has a visible self-hosting track, but it is not fully self-hosted.
The production compiler is still the C++20 Stage0 implementation.

Implemented now:

- `selfhost/src/aurex/selfhost/lexer/core.ax`: shared M0 lexer core with
  `TokenSpan`, token constants, and `scan_token`.
- `selfhost/src/aurex/selfhost/lexer/dump.ax`: token-kind dump helper.
- `selfhost/src/aurex/selfhost/syntax/ast.ax`: ID-backed AST node pools for
  imports, top-level items, types, parameters, blocks, statements, expressions,
  and call arguments.
- `selfhost/src/aurex/selfhost/parser/`: parser seed split into cursor,
  type/signature parsing, iterative expression parsing, and module/item/block
  orchestration.
- `selfhost/src/aurex/selfhost/compiler/ir/`: Stage1 Aurex IR emission modules
  split by writer, names, types, expressions, and AST-to-IR emission.
- `selfhost/src/aurex/selfhost/compiler/driver.ax`: `m0c_stage1` CLI driver.
- `selfhost/runtime/runtime.c`: explicit host runtime shims for selfhost IO.

The old selfhost C emitter path has been removed from the active source tree.
Stage1 now writes `aurex_ir v0` snapshots. For files not yet covered by the
parser seed, it emits deterministic `selfhost_module ... lowering(ast_pending)`
markers so compiler bundle progress is measurable without pretending the full
compiler has been lowered.

## Current Capability

- Stage0 compiles the M0 selfhost lexer, parser seed, smoke programs, and
  `m0c_stage1.ax`.
- The M0 lexer stream is compared with the C++ Stage0 lexer over the local
  corpus.
- The parser seed builds an `AstModule` for the covered module/import/extern/
  export-function subset and validates source ranges and expression trees.
- `m0c_stage1` can emit Aurex IR snapshots for `examples/hello.ax`,
  `m0c_seed.ax`, and the Stage1 IR smoke source.
- `m0c_stage1` can scan the current selfhost compiler source bundle and emit
  deterministic IR/pending-lowering records for the modules it cannot yet fully
  lower.

This is a frontend/IR bootstrap slice, not a fixed-point executable compiler.
The next hard target is to expand the M0 parser and semantic model until
Stage1 can lower the entire selfhost compiler into executable Aurex IR, then
feed that IR through the Stage0 LLVM backend.

## Milestones

### Stage A: Stable Stage0

- Keep the C++ compiler modular and testable.
- Keep the production path on Aurex IR -> LLVM IR -> clang.

### Stage B: M0 Frontend

- Expand lexer and parser coverage in M0.
- Keep expression parsing iterative with explicit stacks.
- Compare M0 AST/parse summaries against Stage0 on shared corpora.

### Stage C: M0 Sema

- Port symbol tables, type tables, name resolution, and diagnostics.
- Compare checked metadata and diagnostics against Stage0.

### Stage D: M0 IR

- Lower the selfhost AST into the same Aurex IR contract used by Stage0.
- Add verifier coverage for Stage1-produced IR.
- Preserve C FFI through explicit `extern c` / `export c` ABI metadata.

### Stage E: LLVM-Backed Stage1

- Feed Stage1-produced Aurex IR through the existing Stage0 LLVM backend.
- Build runnable Stage2 compiler artifacts without reintroducing a C backend.
- Defer custom native backend work until LLVM-backed selfhost is stable.

### Stage F: Fixed Point

- Stage0 builds Stage1.
- Stage1 builds Stage2 from the same selfhost sources.
- Stage2 builds Stage3.
- Stage2/Stage3 IR and executable behavior match deterministically.

## Commands

```sh
tools/bootstrap_chain.sh
make -C selfhost check
```

Expected bootstrap-chain marker:

```text
bootstrap chain passed: Stage0 m0c + selfhost lexer/parser + Stage1 Aurex IR snapshots + standalone bootstrap seed
```

Manual Stage1 IR run:

```sh
cmake -S . -B build
cmake --build build -j
build/m0c -I selfhost/src selfhost/src/aurex/selfhost/bin/m0c_stage1.ax --runtime-c selfhost/runtime/runtime.c -o build/m0c_stage1
build/m0c_stage1 examples/hello.ax build/hello.stage1.air
```

`build/hello.stage1.air` starts with `aurex_ir v0` and contains the Stage1
IR snapshot for the supported parser-seed subset.
