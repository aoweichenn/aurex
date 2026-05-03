# Requirements Analysis

Version: 0.1.2

## Target Users

- Compiler developers who need clear frontend, IR, backend, and driver
  boundaries.
- Language designers who need verifiable semantics and ABI behavior.
- Self-hosting contributors who need incrementally portable M0 compiler slices.
- Toolchain integrators who need stable CLI behavior, install layout, and std
  lookup.

## Scope

The 0.1.2 scope is a usable Stage0, measurable Stage1, and maintainable
standard-library and IR main path:

- Stage0 must complete the `.ax` to native-output path.
- Stage0 must expose tokens, AST, checked summary, Aurex IR, and LLVM IR.
- The standard library must be locatable in both build-tree and install-tree
  layouts.
- Selfhost does not need to reach fixed point, but its slices must remain
  buildable, runnable, and comparable.

## Functional Requirements

1. Source handling: read `.ax` files and preserve paths and source ranges for
   diagnostics.
2. Lexing: emit a stable token stream suitable for golden comparison.
3. Parsing: build a parse-only AST without semantic side effects.
4. Module loading: support `module` / `import` through importer directory, `-I`,
   and standard-library roots.
5. Semantic analysis: resolve types and symbols, check function signatures,
   enforce core control-flow rules, and record ABI names.
6. IR lowering: lower AST plus checked metadata into Aurex IR.
7. IR verification and optimization: verify after lowering and run passes based
   on optimization level.
8. LLVM backend: lower Aurex IR into LLVM IR.
9. Native output: invoke clang to produce assembly, object files, or executables.
10. Standard library: load `std` by default and link the selected backend support
    for executable output.
11. Self-hosting: Stage0 can compile selfhost smoke programs and Stage1 slices.

## Runtime Requirements

- The build environment needs CMake, a C++20 compiler, LLVM development
  libraries, and clang.
- Native output requires a `clang` executable, or an alternate path through
  `--clang`.
- When std is required, the std root must contain `text.ax`, `c.ax`, and
  `support/host_c.c`.
- The install layout should contain `bin/aurexc` and `share/aurex/std`.

## Non-Functional Requirements

- Testability: every compiler stage has observable output or behavioral tests.
- Relocatability: installed tools do not depend on source-tree absolute paths.
- ABI stability: std host support uses versioned symbols.
- Maintainability: documentation is topic-oriented and has matching Chinese and
  English layouts.
- Conservative optimization: unproven optimizations must not silently change
  semantics.
- Locatable diagnostics: errors should include file, line, column, and source
  excerpt.
- Reproducible behavior: golden output and negative samples should be stable.

## Constraints

- LLVM is the current production backend.
- Stage1 is not required to reach fixed point in 0.1.2.
- M0 stays small: no type inference, generics, overloads, or implicit
  conversions.
- The default std backend support implementation is host-c.

## Risks And Boundaries

- The IR pass pipeline is still conservative. `O2` and `O3` currently do not
  enable extra passes beyond the `O1` set.
- `std/native_support.c` is retained as a compatibility entry. New logic should
  use replaceable backend support.
- Stage1 does not yet have complete sema, an IR verifier, or LLVM backend
  handoff, so it is not a production compiler.
- Standard-library ABI v0 symbols must stay stable. Breaking host-support
  changes should introduce a new namespace.

## Acceptance Criteria

- `tools/run_tests.sh` passes.
- Positive examples compile and run.
- Negative examples fail deterministically.
- Installed `bin/aurexc` can find `share/aurex/std` in the same prefix.
- Selfhost lexer/parser/Stage1 smoke programs build and verify through Stage0.
- Documentation entry points remain `docs/zh/`, `docs/en/`, and
  `docs/README.md`; per-small-version files are not restored.
