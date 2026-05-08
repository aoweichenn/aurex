# Next Steps

Version: 0.1.7

## Current Stage

The old selfhost bootstrap track has been removed from the active tree. Current
work focuses on the C++ Stage0 compiler, Aurex IR, and the LLVM backend, with
language work aimed at stronger expressiveness, module isolation, and backend
contracts. M1 is no longer just a feature-completion milestone. Its target is
to make Aurex expressive enough to write two real system programs naturally: a
small self-hosting frontend example and a typed build tool similar in spirit to
CMake. A full replacement of the C++ Stage0 compiler can happen later, but M1
must prove these programs can be written cleanly in Aurex.

Latest Chinese progress report: [M1 progress report 2026-05-07](../zh/m1-progress-2026-05-07.md).
It records the current subprocess / stdout/stderr-capture / cwd / env baseline, file metadata /
mtime baseline, directory-create / directory-entry / recursive-directory-entry /
single-level source-discovery / recursive source-discovery baseline, Map /
CStringUsizeMap baseline, target-graph validation /
topological-build baseline, target-name lookup-cache baseline, target-graph diagnostic
/ message / name / cycle-path / cycle-path-name baseline, test direct-process
runner, M1 examples, integration-test coverage, and test-time baseline.

String foundation work now follows the Chinese design draft:
`str` is borrowed UTF-8 text, `String` owns UTF-8 bytes, `Bytes` /
`Span<u8>` hold raw bytes, `CStr` / `CString` cover C FFI, and `Path`
stores platform path bytes. The current tree has string-literal UTF-8 and
escape diagnostics, `std.core.str` borrowed APIs and scalar APIs,
UTF-8-preserving `String` APIs with `String.as_mut_span` removed, raw
`std.core.bytes.Bytes`, bytes-backed `std.fs.path.Path`, and first
`std.fs.file` `Path` / `str` entry points: `metadata_path`,
`read_bytes_path`, `read_text_path`, `write_bytes_path`, `write_text_path`,
`file_exists_path`, `remove_file_path`, `rename_file_path`, plus
`write_str` / `write_str_path` that write by the true `str` byte length.
`std.fs.dir` now also has `Path` wrappers for directory paths, `str`
wrappers for suffix arguments, and bytes-backed `DirectoryEntry` views:
`name_bytes()` / `path_bytes()` expose raw path bytes, while `name_utf8()` /
`path_utf8()` perform checked UTF-8 conversion. `examples/m1/axbuild`
directory scanning now uses `Path` + `str` suffixes + bytes entry-name
matching.

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
- Basic generic struct / enum instantiation, including nested generic type-arg
  substitution and expected-type / field-value inference for struct literals.
- `match` expressions, literal patterns, wildcard, or-patterns, and guards.
- Block / if expressions.
- Controlled local and return type inference slices.
- Function prototypes and recursive function checks.
- Generic function MVP with explicit `<T>`, call-site type arguments, and
  basic inference from arguments / expected return type.
- `extern c` variadic declarations and calls, including C ABI default argument
  promotions.
- Scope-level `defer` statements, run in reverse order on normal exits,
  `return`, and `break` / `continue` paths.
- `impl` / method / associated-function MVP with explicit `self`,
  `value.method()` instance calls, public `value.field` access,
  `Type.function()` associated calls, `impl<T> Type<T>` generic instance
  methods, and `fn method<U>` method-level generic parameters. Cross-module
  private-field and private-method access now have stable diagnostics.
- Standard `Result` / `Option` / `?` slice, usable for explicit error
  propagation and early-return control flow. `Option<T>` / `Result<T, E>` now
  also have baseline method APIs, including method-level generic
  `Option<T>.ok_or<E>`.
- Standard-library container/text/path baseline started, including generic
  `Span<T>` / `MutSpan<T>`, capacity, append, insert/remove, and random-access
  APIs on `Vec<T>`, generic `Vec<T>` method APIs, owned raw
  `std.core.bytes.Bytes`, a Vec-backed generic `Map<K, V>`, borrowed C-string
  -> usize `CStringUsizeMap`, borrowed UTF-8 `str` APIs and scalar APIs,
  UTF-8-oriented APIs on owned `String`, removal of `String.as_mut_span`, C FFI
  `CStr` / `CString` boundary types, and query/join APIs on bytes-backed
  `Path`.
- Standard file and host-file IO now use `Result`-style owned-buffer APIs, with
  the old `BufferU8` and handwritten file-result structures removed from
  in-tree uses. `std.fs.file::FileMetadata` now provides an
  exists/is_file/is_dir/size/modified_time_ns baseline. `std.fs.file` also
  provides `Path` wrappers and `write_str` / `write_str_path`, so new text
  writes use the `str` byte length and do not truncate at interior `\0`.
  `std.fs.dir` provides directory creation, owned single-level / recursive
  directory-entry reads, `Path` wrappers for directory paths, `str` wrappers
  for suffixes, raw-bytes and checked-UTF-8 directory-entry views, and
  single-level / recursive source-discovery baselines for counting regular
  files by suffix.
- Standard process support has started. `std.sys.process::Command` provides
  typed argv, `arg()`, `cwd()`, `env()`, `run()`, `run_capture()`, and
  `destroy()`, backed by host-c `fork` / `execvp` / `waitpid`, with a
  stdout/stderr-capture, cwd, and env baseline. stdin/stdout/stderr pipes and
  timeout APIs are still missing.
- `pub` / `priv` visibility keywords, cross-module private item filtering, and
  private field access checks.
- Examples now include system-level CLI, file IO, memory/arena, std-module,
  generic result, visibility, and re-export facade coverage.
- M1 acceptance skeletons are now in the active tree: `examples/m1/frontend`
  covers source manager, diagnostics, lexer, token stream, parser subset, and
  AST/IR summary checks; `examples/m1/axbuild` covers project/target modeling,
  typed dependencies/sources/includes/custom commands, subprocess stdout/stderr
  capture, cwd/env, source/stamp mtime incremental checks, directory creation,
  owned single-level / recursive directory-entry reads, source discovery by entries, single-level and
  recursive source-discovery counts, target-name lookup caches, duplicate-target detection, target-graph
  validation, topological build order, structured graph diagnostics/messages/
  names/cycle index paths/cycle name paths, build, clean, run, and test flows.
  Both are covered by checked/IR/native integration tests.

## Key Language Gaps

- Visibility should extend to finer API boundaries, including constructors,
  enum payloads, type-alias propagation, and re-export rules.
- Module isolation still needs explicit package/crate boundaries, import
  aliases, selective imports, better cycle diagnostics, and a stable public
  surface dump.
- The call model has an `impl` / method MVP, generic impl instance methods,
  method-level generic parameters, and cross-module member-visibility
  diagnostics, but still needs trait/class reuse, method public-surface
  tooling, and stronger diagnostics for overload/trait cases.
- Generics still need constraints, where-like predicates, trait/interface
  design, monomorphization caching, and explainable diagnostics.
- Error handling has the standard `Result<T, E>` / `Option<T>` and `?`
  propagation slice, but still needs broader std API migration and a
  composable diagnostic model.
- Resource management still needs a minimal move/noncopyable model and unified
  handling for files, processes, arenas, and other resources.
- The string foundation still needs continued public API tightening:
  `std.fs.file` and `std.fs.dir` now have `Path` / `str` entry points, and
  directory suffixes plus M1 axbuild directory scanning have moved to
  `Path` / `str`. Remaining process, console, and FFI-facing code still has
  low-level `c"..."` / `*const u8` compatibility boundaries to retire.
- The standard library still needs broader `Vec<T>`, hash/bucketed `Map<K, V>`,
  owned string-key maps, streaming
  directory iterators / walk callbacks, file metadata, subprocess support, and OS features required by
  incremental builds.
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
   express compiler core code. A minimal runnable example now exists; follow-up
   work should add fuller source-span diagnostics, AST node hierarchy, import
   parsing, and more realistic error recovery.

2. Typed build-tool example  
   Implement a small CMake-like build tool in Aurex: project, target, library,
   executable, source list, include path, dependency, custom command,
   subprocess, incremental checks, build, clean, run, and test. Build
   definitions should be typed Aurex APIs, not shell-string concatenation. A
   minimal runnable example, stdout/stderr-capture baseline, cwd/env baseline,
   source/stamp mtime incremental checks, directory creation, owned
   single-level / recursive directory-entry reads, source discovery by entries, single-level and
   recursive source-discovery counts, target-name lookup caches,
   duplicate-target detection, target-graph validation, topological build
   order, and structured graph diagnostics/messages/names/cycle index paths/
   cycle name paths now exist; follow-up work should add streaming directory
   iterators / walk callbacks, glob/pattern support, and richer user-facing reports with dependency
   values.

## M1 Priority

1. Finish the method / associated-function / `impl` call model  
   The MVP has landed: explicit `self` parameters, method-call lowering,
   associated functions, public field access, `impl<T> Type<T>` generic
   instance methods, method-level generic parameters, and cross-module method
   visibility diagnostics are supported. Follow-up work should add method
   public-surface dumps, overload/trait diagnostics, and continued example
   migration from C-style helpers to method APIs.

2. Establish standard `Result` / `Option` / `?` error handling  
   The frontend and build tool both need many composable error paths. M1 should
   provide standard generic result types, error propagation, stable diagnostics,
   and example rewrites instead of continuing to rely on manual status helpers.

3. Add the `Span` / `String` / `Vec` / `Map` / `Path` standard-library baseline  
   The compiler frontend needs token buffers, AST lists, symbol tables, and
   source spans. The build tool needs path lists, target graphs, dependency
   maps, and command argv builders.

4. Move generics from basic instantiation to a constrained model  
   Generic function MVP has landed. Next add minimal `where`,
   traits/interfaces, trait impls, and static dispatch. M1 does not need trait
   objects, but it must support containers, algorithms, and typed build graphs.

5. Add the compatibility class/object model  
   Provide an OOP-friendly layer with encapsulation, inheritance, and dynamic
   polymorphism. The recommended M1 shape is single inheritance, explicit
   `virtual`, `override`, `abstract`, `final`, `pub` / `priv` / `protected`
   visibility, and vtable dispatch through base pointers/references. Multiple
   inheritance is out of scope for M1; use traits/interfaces for polymorphic
   composition.

6. Establish resource management and OS engineering support  
   The `defer` MVP has landed. Follow-up work should add minimal noncopyable
   resource rules, streaming directory iterators / walk callbacks, file metadata, subprocesses, cwd/env
   handling, temporary files, and path normalization. Without this slice, the
   build tool remains a toy.

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
   Completed. The parser accepts `impl Type { ... }`,
   `impl<T> Type<T> { ... }`, and `method<U>` method-level generic
   parameters. Sema registers methods into a type-associated scope, and call
   resolution accepts `value.field`, `value.method(args)`,
   `value.method<U>(args)`, and `Type.function(args)`. Cross-module member
   access obeys `pub` / `priv`. Tests cover parse, sema, IR lowering, negative
   diagnostics, and a small example migration from helper functions to methods.

2. `Result` / `Option` / `?`  
   Completed. The method foundation now has a standard error-propagation slice
   for `Result` and `Option`, including `?` early returns. `Option<T>` /
   `Result<T, E>` also expose baseline methods such as `is_some`, `is_ok`,
   `unwrap_or`, and `ok_or<E>`. Next, keep growing the std APIs so code like
   `File.read_all(path)?` and `Parser.next()?` becomes natural.

3. `Span` / `Vec` / `Map` / `Bytes` / `String` / `Path`
   Started. The tree now has `Span<T>` / `MutSpan<T>`, a `Vec<T>` shape with
   capacity, append, insert/remove, random-access, and generic `Vec<T>` method
   operations, owned raw `std.core.bytes.Bytes`, a Vec-backed generic
   `Map<K, V>`, borrowed C-string -> usize `CStringUsizeMap`, borrowed UTF-8
   `str` APIs and scalar APIs, owned UTF-8 `String`
   `from_str/from_utf8/as_str/append(str)/push_scalar/insert_scalar/pop_scalar/remove_scalar_at/slice_bytes_checked/truncate_bytes_checked`
   APIs, removal of `String.as_mut_span`, C FFI `CStr` / `CString` boundary
   types, and bytes-backed `Path` absolute-path, parent, file-name, file-stem,
   extension, from_str, span/c-string join, and with-extension APIs, covered by
   std integration samples combining method APIs,
   `Result` / `Option`, and `?`.
   The old `BufferU8` use has moved to `VecU8`, and `std.fs.file` /
   `std.sys.host` file IO now exposes M1-style APIs such as
   `Result<FileBytes, i32>` and `Result<usize, i32>`. `std.fs.file` now has
   `Path` wrappers and `write_str_path`, with `std_file` covering
   `"path\0text"` so C-string truncation cannot leak back into this path.
   `std.fs.dir` now has `Path` wrappers for directory paths, `str` wrappers for
   suffixes, and bytes-backed `DirectoryEntry` raw-bytes / checked-UTF-8
   views, with `std_dir` covering directory creation, direct/recursive reads,
   suffix counts, null-entry defenses, and `defer` cleanup.
   `examples/m1/axbuild`
   now uses `CStringUsizeMap` for its target-name -> id lookup cache and uses
   `Path` + `str` suffixes + bytes entry-name matching for directory scanning.
   Next,
   grow this into token-buffer, source-list, owned string-key maps,
   hash/bucketed maps, and more general path/build-graph scenarios.

4. Generic constraints / traits / `where`
   Generic function, generic impl method, and method-specific generic
   parameters have landed. Next add the smallest trait/interface or capability
   predicate, then constraints, method-like resolution, and monomorphization
   caching. Then add typed graph and map-like examples.

5. Class/object model MVP  
   Implement classes after methods and traits are stable so member resolution,
   visibility, and vtable lowering can reuse the existing call model. Then add
   an OOP-style plugin or task-runner example.

6. `defer` / noncopyable / OS support  
   Started. `defer call();` now runs in reverse order when the current lexical
   scope exits, including normal exits, `return`, and `break` / `continue`
   lowering. A subprocess / stdout/stderr-capture / cwd / env baseline is now
   available through `std.sys.process::Command` and host-c support, and a file metadata / mtime
   baseline is available through `std.fs.file::FileMetadata`. Directory-create,
   owned single-level / recursive directory-entry, and source-discovery count baselines are available
   through `std.fs.dir`, including single-level and recursive suffix counts.
   Next, add noncopyable resource rules, streaming directory iterators / walk callbacks,
   stdin/stdout/stderr pipes, and temporary-directory support so files, processes,
   arenas, and temporary directories compose safely.

7. Self-hosting frontend and typed build-tool acceptance  
   Started. `examples/m1/frontend` and `examples/m1/axbuild` are now in the
   active tree and covered by checked-surface, IR-surface, and native smoke
   integration tests. Axbuild also covers the `GraphDiagnostic` checked/IR
   surface, message surface, target/related-name surface, cycle index/name path
   surface, and duplicate-target, invalid-dependency, and cycle back-edge diagnostics.
   Keep growing them from minimal acceptance examples into realistic M1
   engineering benchmarks while keeping coverage above 90%.

## Long-Term Priority

1. Finish the module visibility and isolation baseline  
   `pub` / `priv` has landed. Next steps are re-export rules, import aliases,
   selective imports, and public API dumps. The module system is the foundation
   for future self-hosting, packages, and large-codebase maintainability.

2. Push sum types and pattern matching to an industrial baseline  
   Prioritize exhaustiveness, unreachable arms, payload bindings, guard
   constraints, and consistency between enum layout and LLVM lowering.

3. Move generics from basic instantiation to a constrained model  
   Generic function MVP has landed. Next design the smallest viable
   trait/interface or capability predicate, then extend constraints,
   method-like resolution, and monomorphization caching.

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
