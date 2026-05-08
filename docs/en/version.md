# Version Document

Version: 0.1.7

## Version Positioning

0.1.7 is the current documentation baseline. This document no longer lists tiny
per-version changes for every 0.1.x step. Instead, it summarizes the 0.1.x
capability set as one coherent state.

The version document describes the current public state, compatibility strategy,
and future direction. Historical per-small-version changes are no longer kept
as standalone documents; use git history for that level of detail.

## 0.1.7 Scope

Included:

- Stage0 C++20 compiler main path.
- lexer/parser/sema/IR/LLVM/driver/cli layering.
- Aurex IR verifier and pass pipeline.
- Relocatable std lookup.
- std host-c backend support and stable `aurex_std_v0_*` symbols.
- Current language slices, including visibility, basic generics, generic function
  MVP, sum types, pattern matching, expressions, controlled inference,
  `extern c` variadics, and scope-level `defer`.
- M1 acceptance-example baseline, including `examples/m1/frontend` and
  `examples/m1/axbuild`.
- `std.sys.process::Command` subprocess / stdout/stderr-capture / cwd / env
  baseline through host-c `fork` / `execvp` / `waitpid` support, with separate
  pipes drained for stdout and stderr.
- `std.fs.file::FileMetadata` metadata / mtime baseline through host-c `stat`
  support, plus `std.fs.file` `Path` wrappers and `write_str` /
  `write_str_path` so new file APIs do not need raw `c"..."` paths for normal
  path/text use.
- `std.fs.dir` directory-create / directory-entry / recursive-directory-entry / source-discovery baseline
  through host-c `mkdir`, `opendir` / `readdir`, `stat`, and `lstat` support for
  owned single-level / recursive directory-entry reads and regular-file suffix
  counts, with single-level and recursive count entry points. Directory paths
  now have `Path` wrappers, suffixes have `str` wrappers, and `DirectoryEntry`
  stores name/path as bytes-backed `Path` values with raw-bytes and
  checked-UTF-8 views.
- `std.core.map` Vec-backed generic `Map<K, V>` and borrowed C-string -> usize
  `CStringUsizeMap` baseline.
- String primitive direction is now split as `str` = borrowed UTF-8 text slice,
  `String` = owned UTF-8 buffer, `Bytes` / `Span<u8>` = raw bytes, `CStr` /
  `CString` = C FFI, and `Path` = platform path bytes. The current baseline
  includes string-literal UTF-8 / escape diagnostics, `std.core.str` borrowed
  APIs and scalar APIs, the `std.core.string.String`
  `from_str/from_utf8/as_str/append(str)/push_scalar/insert_scalar/pop_scalar/remove_scalar_at/slice_bytes_checked/truncate_bytes_checked`
  UTF-8 surface, removal of `String.as_mut_span`, `std.core.bytes.Bytes`,
  bytes-backed `std.fs.path.Path`, first `std.fs.file` and `std.fs.dir`
  `Path` / `str` entry points, bytes-backed `DirectoryEntry` raw-bytes /
  checked-UTF-8 views, and `std.ffi.c.string.CStr` / `CString`.
- M1 axbuild target-graph validation / topological-build baseline, including
  dependency bounds, cycle / invalid-dependency status, and topological target
  build order.
- M1 axbuild target-name lookup-cache baseline, including name -> id lookup,
  lookup-cache / linear-scan parity checks, missing-lookup checks, and
  duplicate-target-name status.
- M1 axbuild target-graph diagnostic/message/name/cycle-path baseline,
  including `GraphDiagnostic`, status, target index, related index, message,
  target name, related name, cycle index path, cycle name path, and duplicate /
  invalid-dependency / cycle back-edge localization.
- M1 axbuild `Path` risk-closure baseline, including
  `Target.sources: Vec<Path>`, `Project.stamp_path: Path`, `Path` / `str` entry points for
  source/stamp metadata, stamp writes, clean, directory scanning, and
  temporary-source cleanup, plus explicit ownership transfer and failure
  rollback in `Project.add_target(&target)`.
- golden, positive, negative, and language-feature test flows.
- checked/IR/native integration-test coverage for the M1 examples.
- Chinese and English topic-based documentation sets.

Not included:

- Fixed-point self-host.
- Old bootstrap experiment code.
- Complete cross-block SSA/mem2reg and production optimizer.
- Complete M1 self-hosting frontend.
- Complete M1 typed build tool.
- Complete OS/process/pipe/timeout standard library.

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
- Internal structure and acceptance depth of the M1 examples.

## Future Version Direction

- Complete IR constant folding.
- Cross-block mem2reg and phi insertion.
- Fuller ABI attributes and target configuration.
- Complete module isolation, visibility, generic constraints, and pattern
  matching coverage.
- Grow the M1 frontend from a summary parser into real AST, diagnostics, name
  resolution, and type checking.
- Grow M1 axbuild from `Path`-backed source lists/stamps/cleanup,
  source/stamp mtime, directory creation, owned
  single-level / recursive directory-entry reads, source discovery through
  `Path` + `str` suffixes + bytes entry-name matching, single-level and recursive
  source-discovery, target-name lookup caches,
  target-graph smoke checks, stdout/stderr capture, cwd/env, and structured
  graph diagnostics/messages/names/cycle paths into streaming directory
  iterators / walk callbacks, glob/pattern support, hash/bucketed maps,
  dependency-value diagnostics, and structured output
  reporting.
- Redesigned bootstrap chain after M3 using newer language features.
- Fixed-point self-host validation.
