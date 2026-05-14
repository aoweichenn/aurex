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
| Type alias | `type MyInt = i32;` | yes | Structural alias |
| Type alias | `type Alias[T] = T;` | yes | Generic structural alias |
| Type alias | `type Alias[T] where T: Sized = *const T;` | yes | Built-in non-resource capabilities only |
| Struct | `struct Point { x: i32; }` | yes | Fields separated by `;` |
| Struct | `struct Box[T] { value: T; }` | yes | Basic generic struct |
| Enum | `enum E { a, b }` | yes | ADT-first, auto tag |
| Enum | `enum E: u8 { a = 0 }` | yes | C-like/repr-style form |
| Enum | `enum E { span(usize, usize) }` | yes | Multi-field payload supported |
| Enum | `enum Option[T] { some(T), none }` | yes | Generic ADT enum |
| Opaque | `opaque struct FILE;` | yes | Used by extern C |
| Function | `fn f() -> i32 { return 1; }` | yes | Normal function |
| Function | `unsafe fn f(p: *const i32) -> i32 { return *p; }` | yes | Body is an unsafe context |
| Function | `fn f();` | yes | Prototype; explicit return required |
| Function | `fn id[T](x: T) -> T { return x; }` | yes | Basic generic function |
| Function | `fn id[T: Copy](x: T) -> T` | no | Generic bounds are not part of M2 |
| Function | `fn f[T](x: T) -> bool where T: Eq` | yes | Built-in non-resource capability predicate |
| Function | `fn f[T]() where T: Copy` | no | Resource capabilities are deferred |
| C ABI | `extern c { fn puts(s: *const u8) -> i32; }` | yes | Explicit return required |
| C ABI | `export c fn main() -> i32 { return 0; }` | yes | Function only |
| C ABI | `extern c { fn id[T](x: T) -> T; }` | no | Not supported by M2 semantic analysis |
| Impl | `impl Point { fn move(self: *mut Point) {} }` | yes | Target must resolve to named aggregate |
| Impl | `impl *mut Point {}` | no | Not supported by M2 semantic analysis |
| Impl | `impl[T] Box[T] {}` | yes | Impl generic parameters must appear in the target type |
| Impl | `impl Box { fn id[T](self: *const Box, value: T) -> T { return value; } }` | no | Method-local generics remain outside M2 |
| Type | `*mut i32` / `*const u8` | yes | Pointer mutability required |
| Type | `*i32` | no | Missing `mut`/`const` |
| Type | `&i32` | yes | Safe shared reference, distinct from raw pointer |
| Type | `&mut i32` | yes | Safe mutable reference; pointee must be valid storage |
| Type | `&void` | no | Reference pointee must be valid storage |
| Type | `[4]i32` | yes | Integer literal length |
| Type | `[N]i32` | no | Const expr lengths are not part of M2 |
| Type | `[]const i32` | yes | Immutable borrowed slice, fat pointer value |
| Type | `[]mut i32` | yes | Mutable borrowed slice, fat pointer value |
| Type | `[]i32` | no | Slice mutability is required |
| Type | `(i32, bool)` | yes | Anonymous tuple/product type |
| Type | `(i32,)` | yes | One-element tuple uses a trailing comma |
| Type | `()` | no | Empty tuple type is not part of M2 |
| Type | `fn(i32, i32) -> i32` | yes | Non-capturing Aurex function pointer type |
| Type | `unsafe fn(*const i32) -> i32` | yes | Calls require unsafe context |
| Type | `fn(a: i32, b: i32) -> i32` | yes | Names are accepted in function type params |
| Type | `extern c fn(*const u8, ...) -> i32` | yes | C ABI function pointer; variadic only for `extern c fn` |
| Type | `unsafe extern c fn(*const u8) -> i32` | yes | Unsafe C ABI function pointer |
| Type | `fn(i32, ...) -> i32` | no | Variadic function types require `extern c fn` |
| Type | `Box[i32]` | yes | Generic type arguments |
| Type | `Box[]` | no | Empty type arguments rejected |
| Type | `Box<i32>` | no | `<>` are not generic delimiters |
| Type | `foo.Box[i32]` | yes | Dot selector, resolved by base kind |
| Type | `foo.bar.Box` | no | Multi-module expression-style paths are not part of M2 |
| Type | `foo::Box[i32]` | no | Selectors use `.`, not `::` |
| Expr | `arr[i]` | yes | Index expression |
| Expr | `arr[l:r]` | yes | Borrowed slice from array or slice |
| Expr | `arr[:r]` | yes | Omitted start defaults to zero |
| Expr | `arr[l:]` | yes | Omitted end defaults to array length or source slice length |
| Expr | `arr[:]` | yes | Full slice |
| Expr | `text[l:r]` | yes | Checked `str` slice by byte offsets; bounds must be UTF-8 code point boundaries |
| Expr | `let op: fn(i32) -> i32 = f; op(1)` | yes | Function name as value and indirect call |
| Expr | `let op: unsafe fn(*const i32) -> i32 = f; unsafe { op(p) }` | yes | Unsafe function value call fenced by unsafe block |
| Expr | `table.callback(1)` | yes | Struct fields of function type can be called |
| Expr | `id[i32](1)` | yes | Explicit generic function call |
| Expr | `id::[i32](1)` | no | Selectors use `.`, not `::` |
| Expr | `(1, true)` | yes | Tuple literal, type inferred from elements unless expected type is present |
| Expr | `(1,)` | yes | One-element tuple literal uses a trailing comma |
| Expr | `()` | no | Empty tuple literal is not part of M2 |
| Expr | `pair.0` | no | Anonymous tuple elements are accessed by destructuring, not field syntax |
| Expr | `pair.first` where `pair` is a tuple | no | Use a named struct for field access |
| Expr | `Point { x: 1 }` | yes | Struct literal |
| Expr | `Box[i32] { value: 1 }` | yes | Generic struct literal |
| Expr | `[1, 2, 3]` | yes | Array literal |
| Expr | `[0; 4]` | yes | Array repeat literal |
| Expr | `unsafe { *p }` | yes | Tail expression result; creates unsafe context |
| Expr | `unsafe { *p = 1; }` | yes | Statement-form unsafe block; no tail means `void` |
| Expr | `*p` outside `unsafe` | no | Raw pointer dereference requires unsafe context |
| Expr | `let r: &i32 = &value; *r` | yes | Safe reference dereference does not require unsafe |
| Expr | `let r: &mut i32 = &mut value; *r = 1;` | yes | Mutable reference requires a writable place |
| Expr | `let r: &mut i32 = &mut value;` where `value` is `let` | no | `&mut` requires writable storage |
| Expr | `let p: *mut i32 = &mut value;` | no | `&mut` is safe reference syntax, not raw pointer syntax |
| Expr | `ptrcast[T](p)` outside `unsafe` | no | `ptrcast`, `bitcast`, `ptrat`, and `strraw` are unsafe-only |
| Expr | `strvalid(bytes)` | yes | Safe UTF-8 validation for `[]const u8` / `[]mut u8` |
| Expr | `strfromutf8(bytes)` | yes | Safe checked construction returning `str`; failure returns empty `str` |
| Expr | `text[i]` where `text` is `str` | no | Use checked byte-range slicing; scalar/grapheme APIs are deferred |
| Expr | `value++` / `value--` | no | Use assignment operators |
| Block | `{ let x = 1; x + 1 }` | yes | Tail expression result |
| Block | `{ x = 1 }` | no | Assignment cannot be block result |
| If expr | `if c { 1 } else { 2 }` | yes | Branch types must match |
| If expr | `if opt is .some(v) { v } else { 0 }` | yes | Pattern bindings are scoped to then branch |
| If expr | `if c { 1 }` | no | Expression form requires else |
| If stmt | `if c { f(); }` | yes | Else optional |
| If stmt | `if opt is .some(v) { f(v); }` | yes | Pattern condition |
| While stmt | `while step is .next(v) { ... }` | yes | Pattern bindings are scoped to loop body |
| Match | `match e { .some(x) => x, .none => 0 }` | yes | Enum match |
| Match | `match b { true => 1, false => 0 }` | yes | Bool match |
| Pattern | `.span(a, b)` | yes | Binding count must match payload |
| Pattern | `.some((a, b))` | yes | Nested payload destructuring |
| Pattern | `Point { x, y: other }` | yes | Struct pattern |
| Pattern | `[head, .., tail]` | yes | Array/slice pattern with at most one `..` rest marker |
| Pattern | `.int(x) | .other(x)` | yes | Or-pattern bindings must use the same names and types |
| Pattern | `.some(x) | .none` | no | Binding sets differ across alternatives |
| Local pattern | `let (a, _) = pair;` | yes | Tuple destructuring for local `let`/`var` |
| Local pattern | `let Point { x, y } = point;` | yes | Struct destructuring for local `let`/`var` |
| Local pattern | `let .some(v) = opt else { return 0; };` | yes | Else block must not fall through; `v` is visible after the declaration |
| Local pattern | `let () = value;` | no | Empty tuple pattern is not part of M2 |
| Match pattern | `match pair { (true, true) => 1, (true, false) => 2, (false, true) => 3, (false, false) => 4 }` | yes | Finite structural coverage over bool/no-payload enum slots is accepted |
| Match pattern | `match pair { (a, b) => a }` | yes | Irrefutable structural arm remains valid |
| Match pattern | `match slice { [h, ..] => h, _ => 0 }` | yes | Slice matches need an irrefutable fallback arm |
| For | `for var i: i32 = 0; i < 10; i += 1 {}` | yes | C-style loop |
| Range-for | `for i in range(0, 10, 2) {}` | yes | `range` only |
| For-in | `for x in values {}` | no | Generic iteration is not part of M2 |
