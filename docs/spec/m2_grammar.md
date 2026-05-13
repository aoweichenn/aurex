# Aurex M2 Grammar

This document freezes the current M2 syntax implemented by the repository.
It describes existing language surface only. It does not add new language
features.

## 1. Lexical Structure

M2 source is tokenized as keywords, identifiers, literals, punctuators, and
comments.

Identifiers are ASCII-oriented names accepted by the current lexer. `_` is a
valid identifier and is also used as a wildcard pattern.

Comments:

```text
// line comment
/* block comment, nested */
```

Literal tokens:

```text
IntegerLiteral
FloatLiteral
StringLiteral
CStringLiteral
RawStringLiteral
ByteStringLiteral
ByteLiteral
CharLiteral
true
false
null
```

Integer literals may use `_` separators between digits. Array type lengths use
integer literal tokens only. M2 accepts integer type suffixes:
`i8`, `i16`, `i32`, `i64`, `isize`, `u8`, `u16`, `u32`, `u64`, and `usize`.

Float literals support `1.0`, `1e3`, `1.0e-3`, `.5`, and `1.`. M2 accepts
float suffixes `f32` and `f64`; integer suffixes are rejected on float
literals and float suffixes are rejected on integer literals.

String and character literal forms:

```aurex
"text"        // str, decoded as valid UTF-8
c"text"       // *const u8, FFI C string, rejects interior NUL
r"raw\n"      // str, escapes are not interpreted; may span lines
b"abc\n"      // [N]u8, ASCII byte string with simple escapes
b'a'          // u8 byte literal
'λ'           // char, Unicode scalar value
'\u{03BB}'    // char via Unicode scalar escape
```

Ordinary strings and char literals support `\0`, `\n`, `\r`, `\t`, `\\`,
`\"` where applicable, and `\u{...}` Unicode scalar escapes. Byte strings
support simple byte escapes and reject Unicode escapes and non-ASCII raw bytes.

## 2. Module And Import Declarations

```ebnf
Module
  = [ ModuleDecl ] { ImportDecl } { Item } EOF ;

ModuleDecl
  = "module" ModulePath ";" ;

ImportDecl
  = [ Visibility ] "import" ModulePath [ "as" Identifier ] ";" ;

Visibility
  = "pub"
  | "priv" ;

ModulePath
  = Identifier { "." Identifier } ;
```

Rules:

- `module` may appear at most once and only before imports/items.
- `import` declarations appear before ordinary items.
- `pub import` and `priv import` are accepted by current M2.
- Import aliases use `as Identifier`.

## 3. Top-Level Items

```ebnf
Item
  = ConstDecl
  | TypeAliasDecl
  | StructDecl
  | OpaqueStructDecl
  | EnumDecl
  | FnDecl
  | ExternBlock
  | ExportCFnDecl
  | ImplBlock ;
```

Default visibility is private. `pub` and `priv` may appear before normal item
declarations where the parser accepts visibility.

`export c` only introduces a function declaration. `extern c` blocks only
contain function declarations and opaque struct declarations.

## 4. Type Syntax

```ebnf
Type
  = { PointerTypePrefix | ArrayTypePrefix | SliceTypePrefix } TypeAtom ;

PointerTypePrefix
  = "*" ( "mut" | "const" ) ;

ArrayTypePrefix
  = "[" IntegerLiteral "]" ;

SliceTypePrefix
  = "[]" ( "const" | "mut" ) ;

TypeAtom
  = PrimitiveType
  | FunctionType
  | TupleType
  | ParenthesizedType
  | QualifiedType ;

TupleType
  = "(" Type "," [ Type { "," Type } [ "," ] ] ")" ;

ParenthesizedType
  = "(" Type ")" ;

FunctionType
  = [ "unsafe" ] [ "extern" "c" ] "fn" "(" [ FunctionTypeParamList ] ")" "->" Type ;

FunctionTypeParamList
  = FunctionTypeParam { "," FunctionTypeParam } [ "," ]
  | FunctionTypeParam { "," FunctionTypeParam } "," "..." ;

FunctionTypeParam
  = [ Identifier ":" ] Type ;

QualifiedType
  = Identifier [ "::" Identifier ] [ GenericTypeArgs ] ;

GenericTypeArgs
  = "[" Type { "," Type } [ "," ] "]" ;
```

Primitive types:

```text
void bool
i8 u8 i16 u16 i32 u32 i64 u64 isize usize
f32 f64
str char
```

Examples:

```aurex
i32
bool
str
char
*mut i32
*const u8
[4]i32
[]const i32
[]mut u8
*mut [4]i32
(i32, bool)
(i32,)
fn(i32, i32) -> i32
unsafe fn(*const i32) -> i32
extern c fn(*const u8, ...) -> i32
unsafe extern c fn(*const u8) -> i32
Box[i32]
Pair[i32, bool]
foo::Box[i32]
```

Rules:

- Pointer types require `mut` or `const` after `*`.
- Array type length is an integer literal, not a const expression.
- Slice types require `const` or `mut` after `[]`; bare `[]T` is rejected.
- Slices are fat values represented as data pointer plus length. They can be
  produced from arrays or other slices. `[]mut T` is assignable to `[]const T`;
  `[]const T` is not assignable to `[]mut T`.
- Tuple types are anonymous product types. `(A, B)` has two fields, and
  `(A,)` is the one-element tuple form. `()` is not part of M2 syntax.
  Numeric fields are zero-based and accessed with `.0`, `.1`, and so on.
- Function types are non-capturing function pointer types. `fn(...) -> T` uses
  the Aurex function ABI, while `extern c fn(...) -> T` uses the C ABI.
  `unsafe fn(...) -> T` and `unsafe extern c fn(...) -> T` are distinct
  function pointer types whose calls require an unsafe context. Calling
  convention, unsafe marker, fixed parameter types, variadic marker, and return
  type are part of type identity. Function type parameters may omit names;
  optional names are documentation only in the type syntax.
- Variadic function types are only supported for `extern c fn`.
- Function type parameters and return types cannot be by-value arrays or
  array-containing types.
- Generic type arguments use `[]`; empty `[]` is rejected.
- `<` and `>` are not generic delimiters.
- Type paths are currently unqualified or one-level `scope::Name`.

## 5. Function Declarations

```ebnf
FnDecl
  = [ "unsafe" ] "fn" Identifier [ GenericParams ] "(" [ ParamList ] ")"
    [ "->" Type ] [ AbiName ] ( Block | ";" ) ;

ParamList
  = Param { "," Param } [ "," ]
  | Param { "," Param } "," "..." ;

Param
  = Identifier ":" Type ;

AbiName
  = "@name" "(" StringLiteral ")" ;

ExportCFnDecl
  = "export" "c" FnDecl ;

ExternBlock
  = "extern" "c" "{" { ExternItem } "}" ;

ExternItem
  = FnDecl
  | OpaqueStructDecl ;
```

Rules:

- Private non-C functions may infer return type from returns.
- `pub fn`, `extern c fn`, `export c fn`, and prototypes require explicit
  return types in semantic analysis.
- `unsafe fn` bodies are checked in an unsafe context. Calling an `unsafe fn`
  requires an unsafe context, whether the callee is named directly, explicitly
  generic, a method, or a function pointer value.
- Variadic `...` is only supported for `extern c` declarations.
- Generic functions are supported only for normal non-C non-prototype
  functions.
- Function names can be used as non-capturing function pointer values. Values
  whose type is any supported function pointer type can be called with the
  normal call syntax; unsafe function pointer calls still require an unsafe
  context.

## 6. Struct Declarations

```ebnf
StructDecl
  = "struct" Identifier [ GenericParams ] "{"
      { StructField ";" }
    "}" ;

StructField
  = [ Visibility ] Identifier ":" Type ;

OpaqueStructDecl
  = "opaque" "struct" Identifier ";" ;
```

Examples:

```aurex
struct Point {
    x: i32;
    pub y: i32;
}

struct Box[T] {
    value: T;
}

opaque struct FILE;
```

## 7. Enum Declarations

Current M2 is ADT-first for non-generic enums. Base type and case
discriminants are optional. Explicit base/discriminant syntax remains available
for C-like/repr-style enums.

```ebnf
EnumDecl
  = "enum" Identifier [ ":" Type ] "{"
      { EnumCase [ "," ] }
    "}" ;

EnumCase
  = Identifier [ "(" Type { "," Type } [ "," ] ")" ] [ "=" IntegerLiteral ] ;
```

Examples:

```aurex
enum OptionI32 {
    some(i32),
    none,
}

enum Token {
    ident(str),
    span(usize, usize),
    eof,
}

enum Status: u8 {
    ok = 0,
    err = 1,
}
```

Rules:

- Generic enum declarations are not part of M2.
- If no base type is written, the current implementation uses an internal
  integer tag representation.
- If no discriminant is written, tag values are assigned in declaration order.
- Duplicate discriminant values are rejected.
- Payload fields must be valid storage types.
- Payloads containing array storage are currently rejected by semantic analysis.

## 8. Impl Blocks

```ebnf
ImplBlock
  = "impl" Type "{" { MethodDecl } "}" ;

MethodDecl
  = [ Visibility ] FnDecl ;
```

Current semantic rules require the resolved impl target to be a named struct,
enum, or opaque struct type. Generic impl blocks and generic methods are not
supported by M2 semantic analysis.

## 9. Statements

```ebnf
Stmt
  = LetStmt
  | VarStmt
  | AssignStmt
  | ExprStmt
  | IfStmt
  | WhileStmt
  | ForStmt
  | RangeForStmt
  | BreakStmt
  | ContinueStmt
  | DeferStmt
  | ReturnStmt
  | UnsafeBlock
  | Block ;

LetStmt
  = "let" LocalBinding [ ":" Type ] "=" Expr [ LetElse ] ";" ;

VarStmt
  = "var" LocalBinding [ ":" Type ] "=" Expr [ LetElse ] ";" ;

LetElse
  = "else" Block ;

LocalBinding
  = Identifier
  | Pattern ;

ReturnStmt
  = "return" [ Expr ] ";" ;

BreakStmt
  = "break" ";" ;

ContinueStmt
  = "continue" ";" ;

DeferStmt
  = "defer" Expr ";" ;
```

Rules:

- A plain identifier local binding is the normal local declaration form.
- Pattern local bindings support tuple, struct, enum, slice, and or-pattern
  forms.
- Without `else`, a local pattern must be irrefutable.
- With `else`, refutable patterns are allowed; the `else` block must not fall
  through. Pattern bindings are visible after the declaration, and are not
  visible inside the `else` block.

Assignment is a statement, not an expression:

```ebnf
AssignStmt
  = Expr AssignOp Expr ";" ;

AssignOp
  = "=" | "+=" | "-=" | "*=" | "/=" | "%="
  | "<<=" | ">>=" | "&=" | "^=" | "|=" ;
```

Range-for is a restricted counting loop, not generic iteration:

```ebnf
RangeForStmt
  = "for" Identifier "in" "range" "(" Expr [ "," Expr [ "," Expr ] ] ")" Block ;
```

## 10. Expressions

```ebnf
Expr
  = IfExpr
  | MatchExpr
  | UnsafeBlock
  | BinaryExpr ;

UnsafeBlock
  = "unsafe" Block ;
```

Postfix forms:

```text
value.field
value.0
array[index]
array[start:end]
array[:end]
array[start:]
array[:]
function(arg)
generic_fn::[T](arg)
expr?
```

Array expressions:

```aurex
[1, 2, 3]
[0; 4]
```

Tuple expressions:

```aurex
(1, true)
(value,)
```

Slice expressions:

```aurex
let all: []const i32 = values[:];
let prefix = values[:3];
let suffix = values[1:];
let middle = values[1:3];
```

Struct literals:

```aurex
Point { x: 1, y: 2 }
Pair[i32, bool] { first: 1, second: true }
remote::Point { x: 1, y: 2 }
```

Block expressions:

```aurex
let value = {
    let x = 1;
    x + 1
};
```

Unsafe blocks:

```aurex
unsafe {
    *ptr = *ptr + 1;
}

let value = unsafe {
    *ptr
};
```

Rules:

- The result of a block expression is the last expression without `;`.
- The result of an unsafe block expression is its tail expression. An unsafe
  block without a tail expression has type `void` and can be used as a
  statement.
- Assignment cannot be a block result.
- `return`, `break`, `continue`, and `defer` are statements, not tail
  expressions.
- Empty blocks do not infer a value in value-required positions.
- Slice expressions require an array or slice operand. Bounds must be integer
  expressions. Current M2 does not add runtime bounds checks and does not add
  container iteration.
- Tuple literals infer a tuple type from their element expressions unless an
  expected tuple type is present. Arity and element types must match the
  expected tuple type. Empty tuple literals are rejected.
- Tuple field access uses zero-based numeric fields. Because `.5` remains a
  valid leading-dot float literal in expression-start position, postfix parsing
  treats `value.0` as tuple/field syntax only after an existing expression.
- Tuple destructuring is supported in local `let` and `var` declarations:
  `let (left, _) = pair;`. It is not a match-pattern syntax in M2.

If expressions:

```aurex
let value = if cond {
    1
} else {
    2
};
```

Expression-form `if` requires an `else` branch. Statement-form `if` may omit
`else`.

Match expressions:

```aurex
let value = match token {
    .span(start, end) => end - start,
    .eof => 0,
};
```

## 11. Minimal Unsafe

M2 `unsafe` is a narrow semantic boundary. It does not introduce borrow
checking, lifetimes, unsafe traits, unsafe impl blocks, unsafe extern blocks, or
an ownership/resource model.

The following operations require an unsafe context:

- Raw pointer dereference with unary `*`.
- `ptrcast[T](p)`.
- `bitcast[T](x)`.
- `ptrat[T](addr)`.
- `strraw(data, len)`.
- Calling an `unsafe fn` or a value whose type is `unsafe fn(...) -> T` /
  `unsafe extern c fn(...) -> T`.

Safe operations include address-of `&place`, `ptraddr(p)`, `strptr(s)`,
`strblen(s)`, `strvalid(bytes)`, `strfromutf8(bytes)`, `sizeof[T]`,
`alignof[T]`, and checked numeric `cast[T](x)`.

`strvalid(bytes)` accepts a `[]const u8` or `[]mut u8` borrowed byte slice and
returns `bool`. `strfromutf8(bytes)` accepts the same slice shape and returns
`(bool, str)`: field `.0` is the success flag, and field `.1` is the borrowed
`str` on success or the empty `str` on failure. A failed checked call never
wraps invalid input as `str`; unchecked construction remains `strraw(data, len)`
and is unsafe-only.

## 12. Patterns

```ebnf
Pattern
  = PatternAtom { "|" PatternAtom } ;

PatternAtom
  = "_"
  | IntegerLiteral
  | "true"
  | "false"
  | TuplePattern
  | SlicePattern
  | StructPattern
  | Identifier [ "." Identifier ] [ PayloadBindings ]
  | "." Identifier [ PayloadBindings ] ;

PayloadBindings
  = "(" DestructurePattern { "," DestructurePattern } [ "," ] ")" ;

TuplePattern
  = "(" DestructurePattern "," { DestructurePattern "," } [ DestructurePattern ] ")" ;

SlicePattern
  = "[" [ SlicePatternPart { "," SlicePatternPart } [ "," ] ] "]" ;

SlicePatternPart
  = DestructurePattern
  | ".." ;

StructPattern
  = Identifier "{" StructPatternField { "," StructPatternField } [ "," ] "}" ;

StructPatternField
  = Identifier [ ":" DestructurePattern ] ;

DestructurePattern
  = "_"
  | Identifier
  | IntegerLiteral
  | "true"
  | "false"
  | TuplePattern
  | SlicePattern
  | StructPattern
  | Identifier "." Identifier [ PayloadBindings ]
  | "." Identifier [ PayloadBindings ] ;
```

Rules:

- Enum patterns may be unscoped, enum-scoped, or inferred from matched enum
  type via `.case`.
- Payload binding count must match the enum case payload field count.
- Payload patterns may destructure tuple payloads and multi-field enum payloads.
- Struct patterns use field shorthand `Point { x, y }` or explicit field
  patterns `Point { x: left }`.
- Slice patterns use `[a, b]` for exact length and one optional `..` rest
  marker for open length, for example `[head, ..]`, `[.., tail]`, and
  `[head, .., tail]`. They match arrays and slices.
- `if value is pattern` and `while value is pattern` introduce pattern bindings
  only inside the taken block. `if` expressions may also use the same pattern
  condition form.
- Or-pattern alternatives may bind names only when every alternative binds the
  same names with the same types. The bindings are visible in the matched arm
  or taken pattern block.
- Integer/bool matches use literal patterns and require wildcard coverage where
  needed.
- Tuple, struct, array, and slice matches require an irrefutable arm because M2
  does not yet implement full structural exhaustiveness for those shapes.

## 13. Basic Generics

```ebnf
GenericParams
  = "[" GenericParam { "," GenericParam } [ "," ] "]" ;

GenericParam
  = Identifier ;

GenericTypeArgs
  = "[" Type { "," Type } [ "," ] "]" ;

ExplicitGenericCall
  = NameExpr "::" GenericTypeArgs "(" [ ArgumentList ] ")" ;
```

Supported generic positions:

| Syntax position | M2 support | Notes |
| --- | ---: | --- |
| `struct Name[T]` | yes | Basic generic struct |
| `fn name[T]` | yes | Normal non-C non-prototype function |
| `Name[T]` type arguments | yes | Type context |
| `Name[T] { ... }` | yes | Generic struct literal |
| `name::[T](...)` | yes | Explicit generic function call |
| `type Alias[T]` | no | Not part of M2 |
| `enum E[T]` | no | Not part of M2 |
| `impl[T]` | no | Not part of M2 |
| generic method | no | Not supported by M2 semantic analysis |
| generic bounds | no | Not part of M2 |
| `where` | no | Not part of M2 |
| `<>` generics | no | Aurex uses `[]` |

`name[index]` is always an index expression. Explicit generic calls use
`name::[T](...)`.

## 14. Explicitly Rejected Syntax

Examples:

```aurex
*i32
[]i32
[N]i32
[1 + 2]i32
Box[]
Box<i32>
()
let () = value;
foo::bar::Baz
type Alias[T] = T;
enum Option[T] { none }
impl[T] Box[T] {}
fn add[T: Add](a: T, b: T) -> T { return a; }
fn foo[T]() where T: Copy {}
id[i32](1)
for x in values {}
```
