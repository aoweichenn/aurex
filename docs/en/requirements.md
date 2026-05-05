# Requirements Analysis

Version: 0.1.2

## Target Users

- Compiler developers who need clear frontend, IR, backend, and driver
  boundaries.
- Language designers who need verifiable semantics and ABI behavior.
- Bootstrap contributors who need Stage0, IR, and language features to settle
  before the bootstrap chain is rewritten in a later phase.
- Toolchain integrators who need stable CLI behavior, install layout, and std
  lookup.

## Scope

The 0.1.2 scope is a usable Stage0, maintainable standard-library and IR main
path, and continued language work:

- Stage0 must complete the `.ax` to native-output path.
- Stage0 must expose tokens, AST, checked summary, Aurex IR, and LLVM IR.
- The standard library must be locatable in both build-tree and install-tree
  layouts.
- The old bootstrap experiment is no longer an acceptance target; a new
  bootstrap will be designed around M3 using newer language features.

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
11. Language features: Stage0 can check and lower module visibility, generics, sum
    types, pattern matching, expression forms, and inference slices.

## Runtime Requirements

- The build environment needs CMake, a C++20 compiler, LLVM development
  libraries, and clang.
- Native output requires a `clang` executable, or an alternate path through
  `--clang`.
- When std is required, the std root must contain `core/text.ax`,
  `ffi/c/libc.ax`, and `ffi/c/support/host_c.c`.
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
- M0 stays small: no type inference, generics, overloads, or implicit
  conversions.
- The default std backend support implementation is host-c.

## Risks And Boundaries

- The IR pass pipeline is still conservative. `O2` and `O3` currently do not
  enable extra passes beyond the `O1` set.
- C FFI is temporary and isolated under `std/ffi/c/`; new language-level std
  code should depend on that boundary instead of declaring host C directly.
- A new bootstrap is not an active deliverable before M3; current production
  capability must be carried by the C++ Stage0 compiler and LLVM backend.
- Standard-library ABI v0 symbols must stay stable. Breaking host-support
  changes should introduce a new namespace.

## Acceptance Criteria

- `tools/run_tests.sh` passes.
- Positive examples compile and run.
- Negative examples fail deterministically.
- Installed `bin/aurexc` can find `share/aurex/std` in the same prefix.
- Positive and negative samples cover the currently implemented language
  features.
- Documentation entry points remain `docs/zh/`, `docs/en/`, and
  `docs/README.md`; per-small-version files are not restored.
