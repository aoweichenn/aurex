# Introduction

Version: 0.1.2

## Project Positioning

Aurex is a systems-language compiler project with an explicit self-hosting
track. The M0 phase focuses on a clear compiler pipeline, a stable IR contract,
standard-library lookup, ABI boundaries, and measurable bootstrap progress.

The current production compiler is implemented in C++20. The default compile
path is:

```text
source -> lexer -> parser -> sema -> Aurex IR -> LLVM IR -> clang
```

## Design Principles

- Explicit cost: copies, conversions, ABI calls, pointer operations, and memory
  allocation should be visible in source.
- Replaceable stages: lexer, parser, sema, IR, backend, and driver have clear
  boundaries.
- IR first: AST is syntax-only; semantic and backend state do not get written
  back into the AST.
- Measurable self-hosting: the selfhost track advances by component and is
  verified through smoke, golden, and bootstrap-chain tests.
- Relocatable standard library: an installed `aurexc` can find
  `share/aurex/std` in the same prefix.

## Repository Layout

- `include/aurex`: public C++ headers for stage interfaces and data structures.
- `src`: Stage0 C++20 compiler implementation.
- `std`: Aurex standard-library sources and backend support.
- `selfhost`: Stage1 bootstrap slices written in M0.
- `examples`: minimal runnable examples.
- `tests`: positive, negative, golden, and import-path tests.
- `tools`: test, bootstrap, golden comparison, and benchmark scripts.
- `cmake`: build, install, and toolchain configuration.

## Current Capability Summary

- Handwritten lexer and recursive-descent parser.
- Module loading and import search paths.
- Semantic analysis for types, symbols, functions, structs, enums, and basic
  diagnostics.
- Aurex typed CFG/SSA-like IR.
- IR verifier plus conservative local mem2reg and CFG cleanup pass pipeline.
- LLVM IR lowering and clang native output.
- `std` modules and host-c backend support.
- Selfhost lexer/parser/IR-emitter slices written in M0.

## Use Cases

- A compiler experimentation platform for a small systems language.
- A place to validate frontend, IR, LLVM lowering, and native toolchain
  boundaries.
- A self-hosting track where M0 gradually takes over lexer, parser, IR emitter,
  and later compiler components.
- A readable compiler engineering sample for teaching or research.

## Non-Goals

0.1.2 does not claim a full self-host fixed point or a full production optimizer.
Stage1 is still a bootstrap slice; complete sema, IR verifier, and LLVM handoff
remain future work.

This documentation describes the consolidated 0.1.x state. It does not keep a
separate changelog file for every tiny version; historical details belong in
git history.
