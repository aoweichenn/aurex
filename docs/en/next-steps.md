# Next Steps

Version: 0.1.2

## Current Stage

The old selfhost bootstrap track has been removed from the active tree. Current
work focuses on the C++ Stage0 compiler, Aurex IR, and the LLVM backend, with
M1 language work aimed at stronger expressiveness, module isolation, and backend
contracts. A new bootstrap implementation is expected around M3, rewritten in
Aurex with the newer language features instead of preserving the old Stage1
seed constraints.

## Current Capabilities

Stage0 main path:

- Handwritten lexer and recursive-descent parser.
- Module declarations, import search paths, and standard-library lookup.
- Semantic analysis, type tables, symbols, ABI names, and basic diagnostics.
- Aurex typed CFG/SSA-like IR, IR verifier, and a conservative pass pipeline.
- LLVM IR lowering plus clang output for assembly, objects, and executables.
- Build-tree and install-tree std lookup with host-c backend support.

M1 language slices:

- Structs, enums, type aliases, and opaque types.
- Basic generic struct / enum instantiation.
- `match` expressions, literal patterns, wildcard, or-patterns, and guards.
- Block / if expressions.
- Controlled local and return type inference slices.
- Function prototypes and recursive function checks.
- `pub` / `priv` visibility keywords, cross-module private item filtering, and
  private field access checks.

## Key M1 Gaps

- Visibility should extend to finer API boundaries, including constructors,
  enum payloads, type-alias propagation, and re-export rules.
- Module isolation still needs explicit package/crate boundaries, import
  aliases, selective imports, better cycle diagnostics, and a stable public
  surface dump.
- Generics still need constraints, where-like predicates, trait/interface
  design, monomorphization caching, and explainable diagnostics.
- Pattern matching needs stronger exhaustiveness, binding consistency, enum
  layout interaction, and lowering verification.
- AIR should continue to mature as the Stage0 internal backend contract:
  slot/lvalue descriptors, record/enum layout, phi/SSA joins, dominance, call
  signatures, and cross-module item bindings should all be verifiable.
- The LLVM backend must keep up with new frontend features so language work
  does not stop at check/dump coverage.

## Priority

1. Finish the M1 module visibility and isolation baseline  
   `pub` / `priv` has landed. Next steps are re-export rules, import aliases,
   selective imports, and public API dumps. The module system is the foundation
   for future self-hosting, packages, and large-codebase maintainability.

2. Push sum types and pattern matching to an industrial baseline  
   Prioritize exhaustiveness, unreachable arms, payload bindings, guard
   constraints, and consistency between enum layout and LLVM lowering.

3. Move generics from basic instantiation to a constrained model  
   Design the smallest viable trait/interface or capability predicate first,
   then extend generic functions, method-like resolution, and monomorphization
   caching.

4. Stabilize the AIR/IR backend contract  
   AIR should first mature as a verifiable Stage0 design target, while LLVM
   remains the production backend. Future bootstrap work only needs to reach AIR
   initially; it does not need to own a backend immediately.

5. Improve diagnostics and public-surface tooling  
   Module boundaries, generic constraints, match coverage, and visibility errors
   need stable, testable diagnostics before they can be considered usable in
   larger codebases.

## Future Bootstrap Strategy

Do not maintain the old selfhost track before M3. The new bootstrap should be
rewritten against the then-current Aurex feature set: module isolation, explicit
visibility, generics/constraints, sum types, pattern matching, and AIR output.
The first bootstrap target is AIR generation; backend handoff can be designed
after that. LLVM remains the production backend for current language features.
