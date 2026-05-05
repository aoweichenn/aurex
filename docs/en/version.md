# Version Document

Version: 0.1.2

## Version Positioning

0.1.2 is the current documentation baseline. This document no longer lists tiny
per-version changes for every 0.1.x step. Instead, it summarizes the 0.1.x
capability set as one coherent state.

The version document describes the current public state, compatibility strategy,
and future direction. Historical per-small-version changes are no longer kept
as standalone documents; use git history for that level of detail.

## 0.1.2 Scope

Included:

- Stage0 C++20 compiler main path.
- lexer/parser/sema/IR/LLVM/driver/cli layering.
- Aurex IR verifier and pass pipeline.
- Relocatable std lookup.
- std host-c backend support and stable `aurex_std_v0_*` symbols.
- Current language slices, including visibility, basic generics, sum types, pattern
  matching, expressions, controlled inference, and `extern c` variadics.
- golden, positive, negative, and language-feature test flows.
- Chinese and English topic-based documentation sets.

Not included:

- Fixed-point self-host.
- Old bootstrap experiment code.
- Complete cross-block SSA/mem2reg and production optimizer.

## Compatibility Strategy

- New std host support symbols use `aurex_std_v0_*`.
- C FFI declarations and host-c support now live under `std/ffi/c/`.
- New documentation entry points are `docs/zh/` and `docs/en/`.
- Per-small-version files in the form `docs/M0V0.1.x.md` are not restored.

## Public Stable Surface

- CLI option names and `--emit=` modes.
- Basic Aurex IR dump structure.
- `std` module paths and install directory `share/aurex/std`.
- std host support ABI v0 symbols.
- C++ driver, IR pass, and standard-library helper header APIs.

## Allowed Evolution Surface

- M0 syntax and semantic details.
- Number and strength of IR passes.
- New bootstrap coverage after M3.
- std backend support backend types.
- LLVM lowering internals.

## Future Version Direction

- Complete IR constant folding.
- Cross-block mem2reg and phi insertion.
- Fuller ABI attributes and target configuration.
- Complete module isolation, visibility, generic constraints, and pattern
  matching coverage.
- Redesigned bootstrap chain after M3 using newer language features.
- Fixed-point self-host validation.
