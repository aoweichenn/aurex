# Aurex M2 Syntax Matrix

This matrix records whether a syntax position is supported by current M2.

| Area | Syntax | Status | Boundary |
| --- | --- | ---: | --- |
| Module | `module app.main;` | yes | At most once, before imports/items |
| Import | `import foo.bar;` | yes | Before ordinary items |
| Import | `pub import foo as f;` | yes | Re-export import |
| Import | `priv import foo;` | yes | Current parser accepts it |
| Const | `const X: i32 = 1;` | yes | Const initializer subset only |
| Literal | `"text"` | yes | `str`, decoded as valid UTF-8 |
| Literal | `r"raw\n"` | yes | Raw `str`; escapes are not interpreted and newlines are allowed |
| Literal | `b"abc\n"` | yes | Fixed array `[N]u8`; ASCII bytes plus simple escapes |
| Literal | `b'a'` | yes | `u8` byte literal |
| Literal | `'λ'` / `'\u{03BB}'` | yes | `char`, Unicode scalar value |
| Literal | `1u8` / `42usize` | yes | Integer suffixes select the literal type |
| Literal | `.5` / `1.` / `1.0f32` | yes | Float forms and `f32` / `f64` suffixes |
| Literal | `1f32` | no | Float suffixes are not accepted on integer literals |
| Literal | `1.0u8` | no | Integer suffixes are not accepted on float literals |
| Literal | `1.0_f32` | no | Underscore suffix spelling is not part of M2 |
| Type alias | `type MyInt = i32;` | yes | Non-generic only |
| Type alias | `type Alias[T] = T;` | no | Not part of M2 |
| Struct | `struct Point { x: i32; }` | yes | Fields separated by `;` |
| Struct | `struct Box[T] { value: T; }` | yes | Basic generic struct |
| Enum | `enum E { a, b }` | yes | ADT-first, auto tag |
| Enum | `enum E: u8 { a = 0 }` | yes | C-like/repr-style form |
| Enum | `enum E { span(usize, usize) }` | yes | Multi-field payload supported |
| Enum | `enum E[T] { none }` | no | Not part of M2 |
| Opaque | `opaque struct FILE;` | yes | Used by extern C |
| Function | `fn f() -> i32 { return 1; }` | yes | Normal function |
| Function | `fn f();` | yes | Prototype; explicit return required |
| Function | `fn id[T](x: T) -> T { return x; }` | yes | Basic generic function |
| Function | `fn id[T: Copy](x: T) -> T` | no | Generic bounds are not part of M2 |
| Function | `fn f[T]() where T: Copy` | no | `where` is not part of M2 |
| C ABI | `extern c { fn puts(s: *const u8) -> i32; }` | yes | Explicit return required |
| C ABI | `export c fn main() -> i32 { return 0; }` | yes | Function only |
| C ABI | `extern c { fn id[T](x: T) -> T; }` | no | Not supported by M2 semantic analysis |
| Impl | `impl Point { fn move(self: *mut Point) {} }` | yes | Target must resolve to named aggregate |
| Impl | `impl *mut Point {}` | no | Not supported by M2 semantic analysis |
| Impl | `impl[T] Box[T] {}` | no | Not part of M2 |
| Type | `*mut i32` / `*const u8` | yes | Pointer mutability required |
| Type | `*i32` | no | Missing `mut`/`const` |
| Type | `[4]i32` | yes | Integer literal length |
| Type | `[N]i32` | no | Const expr lengths are not part of M2 |
| Type | `[]const i32` | yes | Immutable borrowed slice, fat pointer value |
| Type | `[]mut i32` | yes | Mutable borrowed slice, fat pointer value |
| Type | `[]i32` | no | Slice mutability is required |
| Type | `fn(i32, i32) -> i32` | yes | Non-capturing Aurex function pointer type |
| Type | `fn(a: i32, b: i32) -> i32` | yes | Names are accepted in function type params |
| Type | `extern c fn(*const u8, ...) -> i32` | yes | C ABI function pointer; variadic only for `extern c fn` |
| Type | `fn(i32, ...) -> i32` | no | Variadic function types require `extern c fn` |
| Type | `Box[i32]` | yes | Generic type arguments |
| Type | `Box[]` | no | Empty type arguments rejected |
| Type | `Box<i32>` | no | `<>` are not generic delimiters |
| Type | `foo::Box[i32]` | yes | One-level scope |
| Type | `foo::bar::Box` | no | Too deep for M2 type syntax |
| Expr | `arr[i]` | yes | Index expression |
| Expr | `arr[l:r]` | yes | Slice from array or slice |
| Expr | `arr[:r]` | yes | Omitted start defaults to zero |
| Expr | `arr[l:]` | yes | Omitted end defaults to array length or source slice length |
| Expr | `arr[:]` | yes | Full slice |
| Expr | `let op: fn(i32) -> i32 = f; op(1)` | yes | Function name as value and indirect call |
| Expr | `table.callback(1)` | yes | Struct fields of function type can be called |
| Expr | `id::[i32](1)` | yes | Explicit generic function call |
| Expr | `id[i32](1)` | no | Use `id::[i32](...)` |
| Expr | `Point { x: 1 }` | yes | Struct literal |
| Expr | `Box[i32] { value: 1 }` | yes | Generic struct literal |
| Expr | `[1, 2, 3]` | yes | Array literal |
| Expr | `[0; 4]` | yes | Array repeat literal |
| Expr | `value++` / `value--` | no | Use assignment operators |
| Block | `{ let x = 1; x + 1 }` | yes | Tail expression result |
| Block | `{ x = 1 }` | no | Assignment cannot be block result |
| If expr | `if c { 1 } else { 2 }` | yes | Branch types must match |
| If expr | `if c { 1 }` | no | Expression form requires else |
| If stmt | `if c { f(); }` | yes | Else optional |
| Match | `match e { .some(x) => x, .none => 0 }` | yes | Enum match |
| Match | `match b { true => 1, false => 0 }` | yes | Bool match |
| Pattern | `.span(a, b)` | yes | Binding count must match payload |
| Pattern | `.some(x) | .none` | no | Or-pattern alternatives cannot bind payloads |
| For | `for var i: i32 = 0; i < 10; i += 1 {}` | yes | C-style loop |
| Range-for | `for i in range(0, 10, 2) {}` | yes | `range` only |
| For-in | `for x in values {}` | no | Generic iteration is not part of M2 |
