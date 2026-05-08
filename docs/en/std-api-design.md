# Standard Library API And Namespace Design

This document records the M1 naming, namespace, and compatibility rules for the public standard library. The design is not a copy of one language. It combines Rust-style short type vocabulary and `snake_case`, Zig-style explicit namespace discipline, the C++ container taxonomy, and Swift API Design Guidelines' focus on clarity at the use site.

## Goals

- Keep call sites short: the module name carries context, so function names should not repeat the type prefix.
- Keep type names stable: containers and views use durable short names such as `Vec<T>`, `Span<T>`, `MutSpan<T>`, `Bytes`, `String`, and `Path`.
- Keep semantics readable: `new`, `with_capacity`, and `from_*` construct values; `as_*` returns borrowed views; future `into_*` APIs will represent consuming conversions.
- Preserve compatibility: old names such as `vec_u8_push`, `string_from_c`, and `path_join_c` remain as wrappers, but new docs and examples should prefer the short API.
- Keep error handling evolvable: M1 may keep low-level memory APIs returning `bool`, while user-facing constructors and conversions should prefer `Result<T, E>`.

## Naming Rules

Types use `UpperCamelCase`:

- `Vec<T>`: growable contiguous storage.
- `Span<T>`: read-only contiguous view.
- `MutSpan<T>`: mutable contiguous view.
- `Bytes`: owned raw bytes, with no UTF-8 invariant.
- `String`: owned UTF-8 string, without a raw mutable byte view.
- `Path`: owned filesystem path value backed by platform path bytes, not ordinary text.
- `Option<T>` and `Result<T, E>`: base algebraic data types.

Functions and methods use `snake_case`:

- Construction: `new`, `with_capacity`, `from_str`, `from_utf8`, `from_c_utf8`, `from_c`, `from_span`.
- Capacity and length: `len`, `byte_len`, `capacity`, `is_empty`, `reserve`, `clear`.
- Mutation: `push_scalar`, `insert_scalar`, `pop_scalar`, `remove_scalar_at`, `append`, `push`, `extend`, `insert`, `remove`, `swap_remove`, `pop`, `truncate`, `append_span`, `append_c`.
- Random access: `get`, `set`, `first`, `last`.
- Views: `as_str`, `as_str_checked`, `as_bytes`, `as_span`, `as_mut_span`, `as_c`, `c_str`. `as_mut_span` belongs to raw storage such as `Vec<T>` and `Bytes`, not to `String`.
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
import std.core.str as strings;
import std.core.string as string;
import std.core.bytes as bytes;
import std.core.text as text;
import std.core.vec as vec;
import std.ffi.c.string as cstring;
import std.fs.dir as directory;
import std.fs.file as file;
import std.fs.path as path;
```

`std.core.vec`:

- Types: `vec::Vec<T>` and compatibility alias `vec::VecU8`.
- New API: `vec::new<T>`, `vec::with_capacity<T>`, `vec::destroy<T>`, `vec::len<T>`, `vec::capacity<T>`, `vec::is_empty<T>`, `vec::reserve<T>`, `vec::push<T>`, `vec::insert<T>`, `vec::extend<T>`, `vec::pop<T>`, `vec::remove<T>`, `vec::swap_remove<T>`, `vec::get<T>`, `vec::set<T>`, `vec::first<T>`, `vec::last<T>`, `vec::truncate<T>`, `vec::clear<T>`, `vec::as_span<T>`, `vec::as_mut_span<T>`, `vec::from_span<T>`.
- Method API: `Vec<T>.destroy`, `len`, `capacity`, `is_empty`, `as_span`, `as_mut_span`, `reserve`, `push`, `insert`, `extend`, `pop`, `remove`, `swap_remove`, `get`, `set`, `first`, `last`, `clear`, `truncate`.
- Compatibility API: `vec::vec_u8_new`, `vec::vec_u8_push`, and the other `vec_u8_*` names remain available but are not the primary documentation path.

`std.core.string`:

- Type: `string::String`.
- New API: `string::new`, `string::from_str`, `string::from_utf8`, `string::from_c_utf8`, `string::destroy`, `string::len`, `string::byte_len`, `string::is_empty`, `string::reserve`, `string::append`, `string::push_scalar`, `string::insert_scalar`, `string::pop_scalar`, `string::remove_scalar_at`, `string::as_str`, `string::as_str_checked`, `string::as_bytes`, `string::slice_bytes_checked`, `string::truncate_bytes_checked`, `string::is_valid_utf8`, `string::equals`, `string::starts_with`, `string::ends_with`.
- Compatibility / transition API: `string::from_c`, `string::push`, `string::insert`, `string::append_span`, `string::append_c`, `string::pop`, `string::remove`, `string::truncate`, `string::clear`, `string::as_span`, `string::c_str`, `string::equals_span`, `string::ends_with_byte`. These APIs preserve the UTF-8 invariant where applicable; raw mutable bytes moved to `Bytes.as_mut_span`.
- Compatibility API: `string::string_new`, `string::string_from_c`, and related wrappers remain available.

`std.core.bytes`:

- Type: `bytes::Bytes`, an owned raw byte buffer.
- New API: `bytes::new`, `bytes::with_capacity`, `bytes::from_span`, `bytes::destroy`, `bytes::len`, `bytes::is_empty`, `bytes::capacity`, `bytes::reserve`, `bytes::push`, `bytes::append`, `bytes::pop`, `bytes::remove`, `bytes::truncate`, `bytes::clear`, `bytes::as_span`, `bytes::as_mut_span`, `bytes::equals_span`.
- Method API: `Bytes.new`, `with_capacity`, `from_span`, `destroy`, `len`, `is_empty`, `capacity`, `reserve`, `push`, `append`, `pop`, `remove`, `truncate`, `clear`, `as_span`, `as_mut_span`, `equals_span`.
- Constraint: `Bytes` does not validate UTF-8 and may contain arbitrary bytes. `Bytes.append` supports self-aliasing, so `raw.append(raw.as_span())` remains valid across reallocation.

`std.core.str`:

- Type: builtin borrowed UTF-8 text slice `str`.
- API: `strings::byte_len`, `strings::is_empty`, `strings::as_bytes`, `strings::equals`, `strings::starts_with`, `strings::ends_with`, `strings::is_boundary`, `strings::slice_bytes_checked`, `strings::is_scalar_value`, `strings::scalar_utf8_width`, `strings::scalar_at`, `strings::scalar_width_at`, `strings::next_boundary`, `strings::previous_boundary`, `strings::scalar_count`, `strings::is_valid_utf8`, `strings::from_utf8`.

`std.ffi.c.string`:

- Types: `cstring::CStr`, `cstring::CString`.
- `CStr` API: `CStr.from_c`, `byte_len`, `as_c`, `as_bytes`, `as_str_utf8`.
- `CString` API: `CString.from_str`, `from_utf8`, `from_c_utf8`, `destroy`, `byte_len`, `as_c`, `as_cstr`, `as_str`, `as_str_checked`.
- Constraint: `CString.from_str` rejects interior NUL; `CString.from_utf8` validates both UTF-8 and interior NUL.

`std.fs.path`:

- Type: `path::Path`.
- New API: `path::from_c`, `path::from_span`, `path::from_str`, `path::destroy`, `path::as_c`, `path::as_span`, `path::is_absolute`, `path::parent`, `path::file_name`, `path::file_stem`, `path::extension`, `path::join_c`, `path::join_span`, `path::with_extension`.
- Method API: `Path.from_c`, `from_span`, `from_str`, `destroy`, `as_c`, `as_span`, `file_name`, `is_absolute`, `parent`, `extension`, `file_stem`, `join_c`, `join_span`, `with_extension`.
- Constraint: `Path` stores platform path bytes and does not validate UTF-8. `from_span` and `join_span` reject interior NUL because the current POSIX/C FFI compatibility view is NUL-terminated. `from_str` is a convenience constructor from UTF-8 text.
- Compatibility API: `path::path_from_c`, `path::path_join_c`, and related wrappers remain available.

`std.fs.file`:

- Types: `file::FileBytes` and `file::FileMetadata`.
- C compatibility API: `file::metadata`, `file::read_bytes`, `file::read_text`, `file::write_bytes`, `file::write_text`, `file::file_exists`, `file::remove_file`, and `file::rename_file` still accept `*const u8` paths as low-level compatibility entry points.
- `Path` API: `file::metadata_path`, `file::read_bytes_path`, `file::read_text_path`, `file::write_bytes_path`, `file::write_text_path`, `file::file_exists_path`, `file::remove_file_path`, and `file::rename_file_path` accept `*const path::Path`; null path pointers return `Result.err(1)` or `false`.
- Text API: `file::write_str(path, text: str)` and `file::write_str_path(path, text: str)` write by the `str` byte length, allow interior `\0`, and do not use `c_strlen`.
- Constraint: `Path.from_span` already rejects interior NUL, so the `*_path` wrappers can safely call the current POSIX/C FFI through `Path.as_c()`. New application code should prefer `Path` and `str` entry points; direct `c"..."` paths should stay in FFI or legacy compatibility code.

`std.fs.dir`:

- Types: `directory::DirectoryEntry` and `directory::DirectoryEntryKind`.
- C compatibility API: `directory::create_directory`, `directory::read_entries`, `directory::read_entries_recursive`, `directory::count_files_with_suffix`, `directory::has_file_with_suffix`, `directory::count_files_with_suffix_recursive`, and `directory::has_file_with_suffix_recursive` still accept `*const u8` paths / suffixes as low-level compatibility entry points.
- `Path` API: `directory::create_directory_path`, `directory::read_entries_path`, `directory::read_entries_recursive_path`, `directory::count_files_with_suffix_path`, `directory::has_file_with_suffix_path`, `directory::count_files_with_suffix_recursive_path`, and `directory::has_file_with_suffix_recursive_path` accept `*const path::Path`; null path pointers return `false` or `Result.err(1)`.
- Suffix text API: `directory::count_files_with_suffix_str`, `directory::count_files_with_suffix_path_str`, `directory::has_file_with_suffix_str`, `directory::has_file_with_suffix_path_str`, and the recursive variants accept `str` suffixes, build FFI arguments through `CString.from_str`, and reject interior NUL.
- Directory entry API: `DirectoryEntry` stores host-returned names and paths as bytes-backed `Path` values. `name_bytes()` / `path_bytes()` return raw path bytes, while `name_utf8()` / `path_utf8()` return `Result<str, i32>` checked text views. `name_c_data()` and `path_c_data()` remain as C compatibility entry points. Null entry pointers are defensive: C compatibility entry points return an empty C string, bytes entry points return an empty span, and checked UTF-8 entry points return `Result.err(1)`.
- Constraint: directory entries do not expose unchecked `str` because POSIX path bytes are not guaranteed to be UTF-8. Application code should call `name_utf8()` / `path_utf8()` only when it expects text; otherwise, use raw bytes views or stay at the `Path` / C compatibility boundary.

`std.core.text`:

- Types: `text::Span<T>`, `text::MutSpan<T>`, and compatibility aliases `text::SpanU8`, `text::MutSpanU8`.
- API: `text::span<T>`, `text::mut_span<T>`, `text::c_span`, `text::bytes_equal`, `text::bytes_starts_with`, `text::bytes_find_byte`, `text::bytes_trim_ascii_space`, and ASCII classification/case helpers.

`std.core.result`:

- Types: `result::Option<T>` and `result::Result<T, E>`.
- Method API: `Option<T>.is_some`, `is_none`, `is_some_ref`, `is_none_ref`, `unwrap_or`, `ok_or<E>`, plus `Result<T, E>.is_ok`, `is_err`, `is_ok_ref`, `is_err_ref`, and `unwrap_or`.
- The `*_ref` state checks read only the enum tag and do not consume the payload; they are the low-risk state-checking entry points once noncopy payloads enter `Result` / `Option`.
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
