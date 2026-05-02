# Aurex M0 Semantics

Version: M0V0.1.8

## Core Rule

M0 programs should make cost visible in source. If an operation may copy storage,
convert types, allocate memory, or cross an ABI boundary, it should be explicit.

## Names

- Module-scope functions cannot be overloaded.
- Shadowing is forbidden across all visible scopes.
- Local variables must be declared with `let` or `var`.
- `let` bindings are immutable.
- `var` bindings are mutable.

## Types

Pointer types are explicit:

```m0
*mut T
*const T
```

Invalid forms:

```m0
*T
const *T
```

Arrays are storage-only:

```m0
[16]u8
```

They cannot be passed or returned by value.

## Opaque Structs

Opaque structs represent incomplete C-like types. They can only appear behind
pointers:

```m0
extern c {
    opaque struct File;
}

fn ok(file: *mut File) -> i32 {
    return 0;
}
```

This is invalid:

```m0
fn bad(file: File) -> i32 {
    return 0;
}
```

## Expressions

Assignment is not an expression. It is a statement:

```m0
x = x + 1;
```

M0V0.1.8 allows integer literals to initialize or pass to integer destinations,
but does not allow general implicit numeric conversion between typed values.

## Control Flow

`if` and `while` conditions must be `bool`.

`break` and `continue` are only valid inside `while`.

## ABI

C ABI access must be declared through `extern c`.

```m0
extern c {
    fn puts(s: *const u8) -> i32 @name("puts");
}
```

`@name("...")` selects the emitted C symbol name.
