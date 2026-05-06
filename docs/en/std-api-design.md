# Standard Library API And Namespace Design

This document records the M1 naming, namespace, and compatibility rules for the public standard library. The design is not a copy of one language. It combines Rust-style short type vocabulary and `snake_case`, Zig-style explicit namespace discipline, the C++ container taxonomy, and Swift API Design Guidelines' focus on clarity at the use site.

## Goals

- Keep call sites short: the module name carries context, so function names should not repeat the type prefix.
- Keep type names stable: containers and views use durable short names such as `Vec<T>`, `Span<T>`, `MutSpan<T>`, `String`, and `Path`.
- Keep semantics readable: `new`, `with_capacity`, and `from_*` construct values; `as_*` returns borrowed views; future `into_*` APIs will represent consuming conversions.
- Preserve compatibility: old names such as `vec_u8_push`, `string_from_c`, and `path_join_c` remain as wrappers, but new docs and examples should prefer the short API.
- Keep error handling evolvable: M1 may keep low-level memory APIs returning `bool`, while user-facing constructors and conversions should prefer `Result<T, E>`.

## Naming Rules

Types use `UpperCamelCase`:

- `Vec<T>`: growable contiguous storage.
- `Span<T>`: read-only contiguous view.
- `MutSpan<T>`: mutable contiguous view.
- `String`: owned UTF-8/byte string.
- `Path`: owned filesystem path value.
- `Option<T>` and `Result<T, E>`: base algebraic data types.

Functions and methods use `snake_case`:

- Construction: `new`, `with_capacity`, `from_c`, `from_span`.
- Capacity and length: `len`, `capacity`, `is_empty`, `reserve`, `clear`.
- Mutation: `push`, `extend`, `insert`, `remove`, `swap_remove`, `pop`, `truncate`, `append_span`, `append_c`.
- Random access: `get`, `set`, `first`, `last`.
- Views: `as_span`, `as_mut_span`, `as_c`, `c_str`.
- Queries: `bytes_equal`, `starts_with`, `file_name`.

Do not encode the type name into every function name. New code should use `vec::push(&items, value)` instead of growing the `vec_u8_push` naming family.

## Namespace Syntax

M1 introduces explicit import aliases and module-qualified names:

```aurex
import std.core.text as text;
import std.core.vec as vec;

fn main() -> i32 {
    var bytes: vec::Vec<u8> = vec::new<u8>();
    if !vec::push<u8>(&bytes, b'a') {
        return 1;
    }
    let view: text::Span<u8> = vec::as_span<u8>(&bytes);
    return cast(i32, view.len);
}
```

Rules:

- `import a.b.c as c;` binds a direct import alias in the current module.
- `alias::name` looks up a public type, function, generic function, or global constant in that direct imported module.
- `::` is for module and namespace qualification; `.` remains for value fields and method calls.
- Unqualified imports still work as the compatibility path for existing examples and code.
- Aliases bind only direct imports, not public re-export chains. That keeps name origins predictable.

Member-access rules:

- `value.field` accesses public struct fields, with `pub` / `priv` enforced
  across module boundaries.
- `value.method(args)` calls public instance methods from `impl Type` when
  they have an explicit `self` parameter. Cross-module private methods report
  a dedicated diagnostic instead of degrading to an unknown-method error.
- `impl<T> Type<T>` can declare instance methods for generic types. The
  receiver type drives type-argument inference, so `items.push(value)`
  instantiates `Vec<T>.push`.
- Methods may also declare their own generic parameters, such as
  `value.ok_or<E>(err)` or `box.pair_with(value)`. The impl target infers `T`
  first, then call arguments or explicit `<E>` infer method-level parameters.
- `Type.function(args)` remains the current associated-function call model.

Future associated-item syntax should preserve this boundary: `module::item` is namespace qualification and `value.method()` is value method dispatch. `Type::associated_item` should land only after enum constructors and impl associated functions share one design.

## M1 Standard Library Shape

Recommended imports:

```aurex
import std.core.result as result;
import std.core.string as string;
import std.core.text as text;
import std.core.vec as vec;
import std.fs.path as path;
```

`std.core.vec`:

- Types: `vec::Vec<T>` and compatibility alias `vec::VecU8`.
- New API: `vec::new<T>`, `vec::with_capacity<T>`, `vec::destroy<T>`, `vec::len<T>`, `vec::capacity<T>`, `vec::is_empty<T>`, `vec::reserve<T>`, `vec::push<T>`, `vec::insert<T>`, `vec::extend<T>`, `vec::pop<T>`, `vec::remove<T>`, `vec::swap_remove<T>`, `vec::get<T>`, `vec::set<T>`, `vec::first<T>`, `vec::last<T>`, `vec::truncate<T>`, `vec::clear<T>`, `vec::as_span<T>`, `vec::as_mut_span<T>`, `vec::from_span<T>`.
- Method API: `Vec<T>.destroy`, `len`, `capacity`, `is_empty`, `as_span`, `as_mut_span`, `reserve`, `push`, `insert`, `extend`, `pop`, `remove`, `swap_remove`, `get`, `set`, `first`, `last`, `clear`, `truncate`.
- Compatibility API: `vec::vec_u8_new`, `vec::vec_u8_push`, and the other `vec_u8_*` names remain available but are not the primary documentation path.

`std.core.string`:

- Type: `string::String`.
- New API: `string::new`, `string::from_c`, `string::destroy`, `string::len`, `string::is_empty`, `string::reserve`, `string::push`, `string::insert`, `string::append_span`, `string::append_c`, `string::pop`, `string::remove`, `string::truncate`, `string::clear`, `string::as_span`, `string::as_mut_span`, `string::c_str`, `string::equals_span`, `string::ends_with_byte`.
- Compatibility API: `string::string_new`, `string::string_from_c`, and related wrappers remain available.

`std.fs.path`:

- Type: `path::Path`.
- New API: `path::from_c`, `path::from_span`, `path::destroy`, `path::as_c`, `path::as_span`, `path::is_absolute`, `path::parent`, `path::file_name`, `path::file_stem`, `path::extension`, `path::join_c`, `path::join_span`, `path::with_extension`.
- Compatibility API: `path::path_from_c`, `path::path_join_c`, and related wrappers remain available.

`std.core.text`:

- Types: `text::Span<T>`, `text::MutSpan<T>`, and compatibility aliases `text::SpanU8`, `text::MutSpanU8`.
- API: `text::span<T>`, `text::mut_span<T>`, `text::c_span`, `text::bytes_equal`, `text::bytes_starts_with`, `text::bytes_find_byte`, `text::bytes_trim_ascii_space`, and ASCII classification/case helpers.

`std.core.result`:

- Types: `result::Option<T>` and `result::Result<T, E>`.
- Method API: `Option<T>.is_some`, `is_none`, `unwrap_or`, `ok_or<E>`, plus `Result<T, E>.is_ok`, `is_err`, and `unwrap_or`.
- `Option<T>.ok_or<E>` is the first public standard-library API that uses method-level generic parameters, proving that impl parameters and method-specific parameters can be inferred together at one call site.

## Migration Policy

1. New examples and docs use `import ... as ...` plus `alias::item`.
2. Old long names remain as wrappers so existing M1 examples continue to compile.
3. New functionality should be added to the short API first; old names should receive only necessary compatibility wrappers.
4. Deprecation diagnostics for old long names can be considered after coverage and migrated examples are stable.
5. Standard-library modules may move to alias imports internally, but broad mechanical migration should be done separately.

## References

- Rust API Guidelines, Naming: https://rust-lang.github.io/api-guidelines/naming.html
- Rust `std::vec` and slices: https://doc.rust-lang.org/std/vec/index.html, https://doc.rust-lang.org/std/primitive.slice.html
- Rust collections overview: https://doc.rust-lang.org/std/collections/index.html
- Zig language reference: https://ziglang.org/documentation/master/
- C++ containers draft: https://eel.is/c++draft/containers.general
- Swift API Design Guidelines: https://www.swift.org/documentation/api-design-guidelines/
