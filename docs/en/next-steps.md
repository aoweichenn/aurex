# Next Steps

Version: 0.1.2

## Current Selfhost Stage

The current selfhost track is at the “measurable Stage1 frontend slice plus AIR
snapshot output” stage. It is not yet a complete compiler that can replace
Stage0.

- Stage0 is still the C++20 compiler and remains the production compiler.
- selfhost contains M0 implementations of the lexer core, token dump tools,
  parser seed, ID-backed AST, Stage1 CLI, and Stage1 AIR snapshot emitter.
- Stage1 can read `.ax` files, parse the seed-covered syntax, and output
  `aurex_ir v0` text snapshots.
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

Stage1 AIR snapshot:

- Emits the `aurex_ir v0` header.
- Emits extern functions, opaque records, and export function signatures.
- Emits expression value snapshots.
- Expression values are scoped to the current function block, so module-wide
  expressions are not repeated in every function.
- Modules outside parser seed coverage emit a deterministic
  `selfhost_module ... lowering(ast_pending)` placeholder, keeping the selfhost
  bundle measurable.

## Gaps Before Final M0 Self-Hosting

- Parser coverage is incomplete: ordinary `fn`, `struct`, `enum`, `const`,
  `let` / `var`, assignment, blocks, `if`, `while`, field access, index, casts,
  struct literals, arrays, and other syntax are not fully covered.
- Stage1 does not have complete sema: type resolution, symbol resolution,
  scopes, duplicate definition checks, function signature checks, call argument
  checks, return type checks, and ABI/mangling rules still need implementation.
- Stage1 does not have a real IR verifier. It currently focuses on snapshot
  output rather than full executable IR validity.
- Stage1 lowering is incomplete: locals/slots, CFG, branches, loops, phi,
  record layout, and global constant lowering are still missing.
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

3. Implement Stage1 minimal sema  
   Start with a module item table, function table, local symbol table,
   expression types, and return/call/assign type checks. Full recovery can come
   later.

4. Move Stage1 lowering from snapshots toward a real AIR subset  
   Cover functions, blocks, values, terminators, locals, return, call,
   binary/unary, and CFG for `if` and `while`.

5. Implement a Stage1 IR verifier  
   First check block/value reference ranges, terminator presence, function
   parameter and return type consistency, and call target/argument counts.

6. Design LLVM handoff  
   Prefer making Stage1 emit an AIR format that Stage0 can read, or add a Stage0
   AIR reader first and feed Stage1 AIR into the existing LLVM backend.

7. Build the fixed-point bootstrap flow  
   Target flow:

   ```text
   Stage0(C++) builds Stage1(M0)
   Stage1 compiles selfhost compiler sources to AIR
   Stage0 AIR reader + LLVM backend builds Stage1'
   Stage1' repeats compile
   compare stable outputs
   ```

   Compare structured IR/golden first, then native behavior, and only then aim
   for bit-for-bit stability.

## Next Priority

The next most useful step is parser/AST coverage for `ordinary fn + let/var +
if/while`. This is the shortest path toward compiling selfhost itself and is a
better next move than jumping directly to complete sema or LLVM handoff.
