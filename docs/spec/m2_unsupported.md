# Aurex M2 Unsupported Syntax

This document lists syntax that M2 intentionally rejects or leaves outside the
current semantic boundary. The goal is to keep M2 narrow, predictable, and easy
to test.

## Generics

Not part of M2 syntax:

```aurex
type Alias[T] = T;

enum Option[T] {
    none,
}

impl[T] Box[T] {
}

fn add[T: Add](a: T, b: T) -> T {
    return a;
}

fn copy[T](value: T) -> T where T: Copy {
    return value;
}
```

M2 uses `[]` for generic parameters and arguments. Legacy angle-bracket generic
forms are rejected because `<` and `>` are comparison tokens, not generic
delimiters.

Generic methods and generic C ABI/prototype functions are not supported by M2
semantic analysis.

## Type Syntax

Not part of M2:

```aurex
*i32
[N]i32
[1 + 2]i32
[]i32
[-1]i32
Box[]
foo::bar::Baz
a.b.C
fn(i32, ...) -> i32
```

Rules:

- Pointer types require `*mut` or `*const`.
- Array type length is an integer literal token.
- Type paths are currently plain names or one-level `scope::Name`.
- Variadic function types require `extern c fn`; plain `fn(..., ...) -> T`
  cannot use `...`.

## Expression Syntax

Not part of M2:

```aurex
id[i32](1)
value++
value--
let value = {
    x = 1
};
```

Rules:

- `id[i32](1)` is not a generic call. Explicit generic calls use
  `id::[i32](1)`.
- Increment/decrement operators are not supported.
- Assignment is a statement and cannot be used as a block result.

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

Current match syntax is enum/integer/bool oriented. Struct patterns, tuple
patterns, slice patterns, guards with payload binding consistency across
or-pattern alternatives, and destructuring beyond current enum payload binding
rules are not part of M2.

## Resource And Advanced Language Features

Not part of M2:

```text
trait/interface/protocol
ownership
borrow checking
lifetime parameters
safe references
advanced RAII/drop
macro system
lambda/function literal
capturing closure
async/effects
full const expression language
standard library abstraction layer
```

These are future design topics and should not leak into current M2 parser
acceptance as half-supported syntax.
