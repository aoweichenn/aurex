# Requirements Analysis

## Branch Goal

The M2 `language-core-no-std` stage isolates language-core validation:

- The compiler must build, install, and run without a standard-library tree.
- Imports come only from the importing directory and explicit `-I` entries.
- Native output must not implicitly link std support sources.
- Samples and tests should expose language feature time without std loading or
  external script noise.
- Language-core samples should be self-contained; `Result` / `Option` used for
  `?` tests are defined inside the sample.

## Retained Capabilities

- Handwritten lexer/parser.
- Modules, imports, visibility, and re-exports.
- Primitive types, structs, ADT-style enums, explicit C-like repr enums,
  generic structs/functions/enums/type aliases, owner-generic impl blocks, and
  minimal `where` capabilities.
- Tuple types, tuple literals, and tuple destructuring. Anonymous tuple field
  access is intentionally rejected.
- Pattern matching, multi-field enum payload destructuring, guards, and
  or-patterns.
- `if` expressions, block expressions, `while`, `for`, `break`, and `continue`.
- `defer` and `?`.
- Ordinary value semantics, with the current restriction that arrays and
  array-containing types cannot be by-value function parameters, returns,
  assignment targets, or enum payloads.
- Aurex IR, pass pipeline, LLVM IR, and clang native output.

## Deferred Capabilities

- Standard-library APIs, containers, file/dir/process/console support.
- M1 frontend/build-tool examples.
- std host support and installed std lookup.

The M1 language-level `move(...)` and `noncopy struct` syntax is no longer part
of the current M2 requirements. Resource semantics should be redesigned after
core syntax, `unsafe`, slices/strings, the safe-reference direction, and
minimal non-resource `where` capabilities are stable.
