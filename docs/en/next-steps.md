# Next Steps

Version: 0.1.2

## Current Stage

The old selfhost bootstrap track has been removed from the active tree. Current
work focuses on the C++ Stage0 compiler, Aurex IR, and the LLVM backend, with
language work aimed at stronger expressiveness, module isolation, and backend
contracts. M1 is no longer just a feature-completion milestone. Its target is
to make Aurex expressive enough to write two real system programs naturally: a
small self-hosting frontend example and a typed build tool similar in spirit to
CMake. A full replacement of the C++ Stage0 compiler can happen later, but M1
must prove these programs can be written cleanly in Aurex.

## Current Capabilities

Stage0 main path:

- Handwritten lexer and recursive-descent parser.
- Module declarations, import search paths, and standard-library lookup.
- Semantic analysis, type tables, symbols, ABI names, and basic diagnostics.
- Aurex typed CFG/SSA-like IR, IR verifier, and a conservative pass pipeline.
- LLVM IR lowering plus clang output for assembly, objects, and executables.
- Build-tree and install-tree std lookup with host-c backend support.

Current language slices:

- Structs, enums, type aliases, and opaque types.
- Basic generic struct / enum instantiation.
- `match` expressions, literal patterns, wildcard, or-patterns, and guards.
- Block / if expressions.
- Controlled local and return type inference slices.
- Function prototypes and recursive function checks.
- `impl` / method / associated-function MVP with explicit `self`, instance
  method calls, and `Type.function()` associated calls.
- Standard `Result` / `Option` / `?` slice, usable for explicit error
  propagation and early-return control flow.
- Standard-library container/text/path baseline started, including `VecU8` APIs
  on generic `Vec<T>`, an owned `String`, and an owned `Path`.
- `pub` / `priv` visibility keywords, cross-module private item filtering, and
  private field access checks.
- Examples now include system-level CLI, file IO, memory/arena, std-module,
  generic result, visibility, and re-export facade coverage.

## Key Language Gaps

- Visibility should extend to finer API boundaries, including constructors,
  enum payloads, type-alias propagation, and re-export rules.
- Module isolation still needs explicit package/crate boundaries, import
  aliases, selective imports, better cycle diagnostics, and a stable public
  surface dump.
- The call model has an `impl` / method MVP, but still needs generic impls,
  trait/class reuse, method public-surface tooling, and stronger diagnostics.
- Generics still need generic functions, constraints, where-like predicates,
  trait/interface design, monomorphization caching, and explainable diagnostics.
- Error handling still needs standard `Result<T, E>` / `Option<T>`, `?`
  propagation, and a composable diagnostic model.
- Resource management still needs `defer`, a minimal move/noncopyable model, and
  unified handling for files, processes, arenas, and other resources.
- The standard library still needs `Span<T>`, `String`, `Vec<T>`, `Map<K, V>`,
  `Path`, directory walking, file metadata, subprocess support, and OS features
  required by incremental builds.
- Aurex needs a compatibility class/object model for programmers coming from
  traditional OOP code: encapsulation, inheritance, and dynamic polymorphism.
  This should be a migration-friendly layer, not a replacement for the
  struct/enum/trait/generic core.
- Pattern matching needs stronger exhaustiveness, binding consistency, enum
  layout interaction, and lowering verification.
- AIR should continue to mature as the Stage0 internal backend contract:
  slot/lvalue descriptors, record/enum layout, phi/SSA joins, dominance, call
  signatures, and cross-module item bindings should all be verifiable.
- The LLVM backend must keep up with new frontend features so language work
  does not stop at check/dump coverage.

## M1 Acceptance Targets

M1 should finish with two Aurex-written system examples in the active tree, both
covered by integration tests:

1. Self-hosting frontend example  
   Implement a small compiler frontend in Aurex: source manager, lexer, token
   stream, parser subset, AST/IR dump, and diagnostics. It does not need to
   replace the C++ Stage0 compiler, but it must prove that Aurex can naturally
   express compiler core code.

2. Typed build-tool example  
   Implement a small CMake-like build tool in Aurex: project, target, library,
   executable, source list, include path, dependency, custom command,
   subprocess, incremental checks, build, clean, run, and test. Build
   definitions should be typed Aurex APIs, not shell-string concatenation.

## M1 Priority

1. Finish the method / associated-function / `impl` call model  
   The MVP has landed: explicit `self` parameters, method-call lowering,
   associated functions, and basic method visibility are supported. Follow-up
   work should add generic impls, method public-surface dumps, cross-module
   method diagnostics, and continued example migration from C-style helpers to
   method APIs.

2. Establish standard `Result` / `Option` / `?` error handling  
   The frontend and build tool both need many composable error paths. M1 should
   provide standard generic result types, error propagation, stable diagnostics,
   and example rewrites instead of continuing to rely on manual status helpers.

3. Add the `Span` / `String` / `Vec` / `Map` / `Path` standard-library baseline  
   The compiler frontend needs token buffers, AST lists, symbol tables, and
   source spans. The build tool needs path lists, target graphs, dependency
   maps, and command argv builders.

4. Move generics from basic instantiation to a constrained model  
   Add generic functions, minimal `where`, traits/interfaces, trait impls, and
   static dispatch. M1 does not need trait objects, but it must support
   containers, algorithms, and typed build graphs.

5. Add the compatibility class/object model  
   Provide an OOP-friendly layer with encapsulation, inheritance, and dynamic
   polymorphism. The recommended M1 shape is single inheritance, explicit
   `virtual`, `override`, `abstract`, `final`, `pub` / `priv` / `protected`
   visibility, and vtable dispatch through base pointers/references. Multiple
   inheritance is out of scope for M1; use traits/interfaces for polymorphic
   composition.

6. Establish resource management and OS engineering support  
   Support `defer`, minimal noncopyable resource rules, directory walking, file
   metadata, subprocesses, cwd/env handling, temporary files, and path
   normalization. Without this slice, the build tool remains a toy.

7. Push sum types and pattern matching to an industrial baseline  
   Prioritize exhaustiveness, unreachable arms, payload bindings, guard
   constraints, and consistency between enum layout and LLVM lowering. The
   self-hosting frontend will rely heavily on token and AST matching.

8. Stabilize the AIR/IR backend contract  
   AIR should first mature as a verifiable Stage0 design target, while LLVM
   remains the production backend. The frontend example should first produce a
   structured dump; full backend handoff can come later.

9. Improve diagnostics and public-surface tooling  
   Module boundaries, generic constraints, method/class dispatch, match
   coverage, and visibility errors need stable, testable diagnostics before
   they can be considered usable in larger codebases.

## Implementation Order

The `impl` / method MVP is now complete. When implementation resumes, start
with the standard `Result` / `Option` / `?` slice so file, CLI, parser, and
build-graph code can propagate errors naturally instead of continuing to use
manual status helpers.

1. `impl` / method MVP  
   Completed. The parser accepts `impl Type { ... }`, sema registers methods
   into a type-associated scope, and call resolution accepts
   `value.method(args)` and `Type.function(args)`. Tests cover parse, sema, IR
   lowering, negative diagnostics, and a small example migration from helper
   functions to methods.

2. `Result` / `Option` / `?`  
   Completed. The method foundation now has a standard error-propagation slice
   for `Result` and `Option`, including `?` early returns. Next, keep growing
   the std APIs so code like `File.read_all(path)?` and `Parser.next()?`
   becomes natural.

3. `Vec` / `String` / `Path`  
   Started. The tree now has a `Vec<T>` shape with `VecU8` operations, an owned
   `String`, and an owned `Path`, covered by a std integration sample combining
   method APIs, `Result` / `Option`, and `?`. Next, grow this into token-buffer,
   source-list, and more general path/build-graph scenarios.

4. Generic functions / traits / `where`  
   Remove single-type specialization pressure from containers, algorithms, and
   build graphs. Then add typed graph and map-like examples.

5. Class/object model MVP  
   Implement classes after methods and traits are stable so member resolution,
   visibility, and vtable lowering can reuse the existing call model. Then add
   an OOP-style plugin or task-runner example.

6. `defer` / noncopyable / OS support  
   Make files, processes, arenas, and temporary directories compose safely. Then
   start the `axbuild` example.

7. Self-hosting frontend and typed build-tool acceptance  
   Add both system examples to integration tests and keep coverage above 90%.

## Long-Term Priority

1. Finish the module visibility and isolation baseline  
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

Do not restore the old selfhost track. The new bootstrap should be rewritten
against the current roadmap: module isolation, explicit visibility, methods,
standard error handling, generics/constraints, traits, the necessary
class-compatibility layer, sum types, pattern matching, resource management, and
AIR output. The M1 bootstrap target is an Aurex-written frontend example; a full
replacement of C++ Stage0 and backend handoff can come later. LLVM remains the
production backend for current language features.
