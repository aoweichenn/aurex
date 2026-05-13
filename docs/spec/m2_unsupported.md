# Aurex M2 Unsupported Syntax

This document lists syntax that M2 intentionally rejects or leaves outside the
current semantic boundary. The goal is to keep M2 narrow, predictable, and easy
to test.

## Generics

Not part of M2 syntax or semantics:

```aurex
fn add[T: Add](a: T, b: T) -> T {
    return a;
}

fn copy[T](value: T) -> T where T: Copy {
    return value;
}

impl Box {
    fn id[T](self: *const Box, value: T) -> T {
        return value;
    }
}
```

M2 uses `[]` for generic parameters and arguments. Legacy angle-bracket generic
forms are rejected because `<` and `>` are comparison tokens, not generic
delimiters.

Generic structs, generic enums, generic type aliases, generic functions, and
`impl[T] Box[T]`-style owner generics are supported. Inline bounds (`T: Add`),
resource capabilities such as `Copy` / `Drop`, method-local generic parameters,
and generic C ABI/prototype functions are not supported by M2 semantic
analysis.

## Type Syntax

Not part of M2:

```aurex
*i32
&void
[N]i32
[1 + 2]i32
[]i32
[-1]i32
Box[]
foo::bar::Baz
a.b.C
fn(i32, ...) -> i32
()
```

Rules:

- Pointer types require `*mut` or `*const`.
- Reference types are supported as `&T` and `&mut T`, but their pointee must be
  valid storage. `&void` and references to opaque value types are rejected.
- Array type length is an integer literal token.
- Type paths are currently plain names or one-level `scope::Name`.
- Variadic function types require `extern c fn`; plain `fn(..., ...) -> T`
  cannot use `...`.
- Tuple types use `(A, B)` or `(A,)`. Empty tuple type `()` is rejected.

## Literal Syntax

Not part of M2:

```aurex
r#"raw"#
b"\u{41}"
b"é"
'ab'
'\u{D800}'
1f32
1.0u8
1.0_f32
```

Rules:

- Raw strings use only `r"..."`. Rust-style delimiter-counted raw strings such
  as `r#"..."#` are not part of M2.
- Byte strings are ASCII byte sequences with simple escapes. Unicode escapes
  and non-ASCII raw bytes are rejected in `b"..."`.
- `char` literals contain exactly one Unicode scalar value. Surrogates and
  values above `U+10FFFF` are rejected.
- Integer literals accept only integer suffixes. Float literals accept only
  `f32` and `f64`; underscore suffix spellings such as `1.0_f32` are not part
  of M2.

## Expression Syntax

Not part of M2:

```aurex
id[i32](1)
()
value++
value--
let value = {
    x = 1
};
```

Rules:

- `id[i32](1)` is not a generic call. Explicit generic calls use
  `id::[i32](1)`.
- Tuple literals use `(a, b)` or `(a,)`. Empty tuple literal `()` is rejected.
- `str` has checked byte-range slicing with `text[l:r]`, but single-index
  `text[i]`, Unicode scalar iteration, grapheme-cluster APIs, and locale-aware
  text segmentation are not part of M2.
- Increment/decrement operators are not supported.
- Assignment is a statement and cannot be used as a block result.

## Unsafe Syntax

Unsafe boundaries are intentionally minimal. M2 does not include:

```aurex
unsafe extern c {
    fn f() -> void;
}

unsafe impl Trait for Type {
}

unsafe trait Trait {
}
```

Rules:

- `unsafe { ... }`, `unsafe fn`, and unsafe function pointer types are
  supported.
- Raw pointer dereference, `ptrcast`, `bitcast`, `ptrat`, `strraw`, and unsafe
  function calls require an unsafe context.
- Borrow checking, lifetimes, unsafe traits, unsafe impl blocks, unsafe extern
  blocks, and ownership/resource semantics are outside M2.

## Statement And Control Flow Syntax

Not part of M2:

```aurex
for x in values {
}
```

M2 range-for only supports:

```aurex
for i in range(end) {
}

for i in range(start, end) {
}

for i in range(start, end, step) {
}
```

Generic iteration, iterator protocols, slice iteration, and container iteration
are not part of M2.

## Block, If, And Match Boundaries

Expression-form `if` requires `else`:

```aurex
let value = if cond {
    1
};
```

Block expressions require a value-producing tail expression when used where a
value is needed. `return`, `break`, `continue`, `defer`, and assignment are
statements, not block results.

Current match syntax supports enum/integer/bool plus tuple, struct, array, and
slice destructuring. Binding alternatives inside or-patterns are supported only
when every alternative binds the same names with the same types.

Local tuple, struct, enum, and slice destructuring is supported in `let` / `var`
declarations:

```aurex
let (left, _) = pair;
let Point { x, y } = point;
let [head, ..] = array;
let .some(value) = opt else { return 0; };
```

Empty tuple patterns such as `let () = value;` are rejected.

`if value is pattern`, `while value is pattern`, and `if` expression pattern
conditions are the M2 pattern condition forms. Rust-style `if let` and
`while let` remain unsupported; M2 uses `if value is pattern` and
`while value is pattern` instead.

## Resource And Advanced Language Features

Not part of M2:

```text
trait/interface/protocol
ownership
borrow checking
lifetime parameters
advanced RAII/drop
macro system
lambda/function literal
capturing closure
async/effects
full const expression language
standard library abstraction layer
```

Minimal `&T` / `&mut T` references are part of M2. Borrow checking, lifetimes,
borrowed-return rules, alias/resource semantics, and RAII/drop are future design
topics and should not leak into current M2 parser acceptance as half-supported
syntax.
