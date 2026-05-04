# Next Steps

Version: 0.1.2

## Current Selfhost Stage

The current selfhost track is at the “measurable Stage1 frontend slice plus
TAC/AIR snapshot output” stage. It is not yet a complete compiler that can
replace Stage0.

- Stage0 is still the C++20 compiler and remains the production compiler.
- selfhost contains M0 implementations of the lexer core, token dump tools,
  parser seed, ID-backed AST, Stage1 CLI, Stage1 TAC snapshot emitter, and the
  first AIR model/lowering/verifier slice.
- Stage1 can read `.ax` files, parse the seed-covered syntax, and output
  `aurex_tac v0` text snapshots. Function bodies also embed `air_ir v0` and
  `air_cfg v0` comment snapshots.
- The bootstrap flow covers selfhost lexer golden comparison, parser smoke,
  Stage1 snapshot output, and selfhost bundle visibility.

## Current Capabilities

M0 lexer:

- Tokenizes the local corpus.
- Prints token kinds for comparison against Stage0.
- Passes range, smoke, and golden tests.

M0 parser seed:

- `module` / `import`.
- One `extern c` block.
- `opaque struct` and extern function signatures.
- Multiple `export c fn` items.
- Function parameters, return types, primitive/named/pointer types.
- Expression statements and `return`.
- Integer, identifier, string, c string, bool, null, unary/binary/call
  expressions.
- Basic operator precedence and call arguments.

Stage1 TAC/AIR snapshot:

- Emits the `aurex_tac v0` header.
- Emits extern functions, opaque records, and export function signatures.
- Emits three-address-code expression temporaries.
- AIR is split into model, binding, lowering, text, and verifier modules.
- AIR function snapshots include the function header, linkage, parameter table,
  local table, blocks, value DAG, instructions, and terminators.
- AIR values carry result type ids, sema type categories,
  identifier/field/struct name ranges, cast/type-op target type ids, and
  call/struct-literal arguments.
- AIR name values can bind to params, locals, and items. Let/var/assign
  instructions also carry local bindings.
- Sema has a standalone expression annotation table keyed by expr id. It
  persists inferred `type_id`, primitive, integer, null, item, and c-string
  result categories, and AIR lowering reads type information from that table.
- The AIR verifier covers headers, params, locals, value references, value
  arguments, bindings, instructions, and terminators.
- Temporaries are scoped to the current function block, so module-wide
  expressions are not repeated in every function.
- Modules outside parser seed coverage emit a deterministic
  `selfhost_module ... lowering(ast_pending)` placeholder, keeping the selfhost
  bundle measurable.

## Gaps Before Final M0 Self-Hosting

- Parser coverage is not yet enough to compile the entire selfhost compiler.
  The remaining work is mostly bundle edge cases and error recovery.
- Stage1 typed AST is not finalized yet. Expression annotations exist, but item
  bindings, local bindings, call signatures, record layout, and lvalue data are
  not all persisted in one backend annotation layer.
- Stage1 lowering does not yet produce a real backend handoff format. AIR is
  currently a structured comment snapshot.
- AIR still needs slots/alloca, record layout, global constant lowering,
  complex lvalue descriptors, phi/SSA joins, and cross-module item/import
  bindings.
- Stage1 does not yet have a complete TAC/AIR/backend verifier loop. The AIR
  verifier checks structure, but not type equivalence, call signatures, control
  dominance, or reachability.
- Stage1 has no LLVM handoff yet. It cannot pass its output to the existing LLVM
  backend to produce native code.
- There is no fixed point yet. Stage1 cannot compile itself completely, and
  `Stage0 -> Stage1 -> Stage1'` convergence has not been proven.

## Implementation Plan

1. Extend parser coverage to the minimum syntax needed by the selfhost compiler  
   Prioritize syntax actually used by current selfhost sources: ordinary `fn`,
   `let` / `var`, blocks, `if`, `while`, assignment, and calls.

2. Stabilize AST structure  
   Add missing item, statement, expression, and type nodes. Each syntax addition
   should get parser smoke coverage and Stage1 snapshot assertions.

3. Stabilize Stage1 typed AST / AIR annotations  
   Build on the expression type annotation table and persist item bindings,
   local bindings, call signatures, and lvalue data so AIR lowering does not
   keep re-resolving through source ranges.

4. Move Stage1 AIR from snapshots toward a real backend handoff  
   Cover functions, blocks, values, instructions, terminators, locals, return,
   call, binary/unary, and CFG for `if` and `while`.

5. Extend the Stage1 AIR verifier
   Add type equivalence, function parameter/return consistency, call
   target/argument checks, branch conditions, dominance, and block reachability.

6. Design LLVM handoff  
   Prefer making Stage1 emit a TAC format that Stage0 can read, or add a Stage0
   TAC reader first and feed Stage1 TAC into the existing LLVM backend.

7. Build the fixed-point bootstrap flow  
   Target flow:

   ```text
   Stage0(C++) builds Stage1(M0)
   Stage1 compiles selfhost compiler sources to TAC
   Stage0 TAC reader + LLVM backend builds Stage1'
   Stage1' repeats compile
   compare stable outputs
   ```

   Compare structured IR/golden first, then native behavior, and only then aim
   for bit-for-bit stability.

## Next Priority

The next most useful step is adding AIR slots/lvalue descriptors and moving
local-binding/lvalue information into the annotation layer. That is the shortest
path from “readable snapshot” to “backend-consumable IR.”
