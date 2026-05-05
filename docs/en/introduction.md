# Introduction

Version: 0.1.2

## Project Positioning

Aurex is a systems-language compiler project with a future self-hosting goal.
The current phase focuses on stabilizing the C++ Stage0 compiler, IR contract,
LLVM backend, standard-library lookup, ABI boundaries, and current language
features.

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
- Deferred bootstrap: the old bootstrap experiment has been removed; the next
  bootstrap will be rewritten around M3 using the stabilized language features.
- Relocatable standard library: an installed `aurexc` can find
  `share/aurex/std` in the same prefix.

## Repository Layout

- `include/aurex`: public C++ headers for stage interfaces and data structures.
- `src`: Stage0 C++20 compiler implementation.
- `std`: Aurex standard-library sources and backend support.
- `examples`: runnable examples, including shared modules and small system
  projects.
- `tests`: positive, negative, golden, and import-path tests.
- `tools`: test, golden comparison, and benchmark scripts.
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
- Current language slices, including explicit visibility, basic generics, generic
  function MVP, sum types, pattern matching, expressions, controlled inference,
  `extern c` variadics, and scope-level `defer`.

## Use Cases

- A compiler experimentation platform for a small systems language.
- A place to validate frontend, IR, LLVM lowering, and native toolchain
  boundaries.
- A base for redesigning the bootstrap chain after M3 with stronger language
  features and a clearer IR contract.
- A readable compiler engineering sample for teaching or research.

## Non-Goals

0.1.2 does not claim a full self-host fixed point or a full production
optimizer. The old bootstrap slices have been removed; current production
capability is carried by the C++ Stage0 compiler and LLVM backend.

This documentation describes the consolidated 0.1.x state. It does not keep a
separate changelog file for every tiny version; historical details belong in
git history.
