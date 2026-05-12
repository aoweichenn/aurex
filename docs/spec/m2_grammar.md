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
ByteLiteral
true
false
null
```

Integer literals may use `_` separators between digits. Array type lengths use
integer literal tokens only.

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
  = { PointerTypePrefix | ArrayTypePrefix } TypeAtom ;

PointerTypePrefix
  = "*" ( "mut" | "const" ) ;

ArrayTypePrefix
  = "[" IntegerLiteral "]" ;

TypeAtom
  = PrimitiveType
  | QualifiedType ;

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
str
```

Examples:

```aurex
i32
bool
str
*mut i32
*const u8
[4]i32
*mut [4]i32
Box[i32]
Pair[i32, bool]
foo::Box[i32]
```

Rules:

- Pointer types require `mut` or `const` after `*`.
- Array type length is an integer literal, not a const expression.
- Generic type arguments use `[]`; empty `[]` is rejected.
- `<` and `>` are not generic delimiters.
- Type paths are currently unqualified or one-level `scope::Name`.

## 5. Function Declarations

```ebnf
FnDecl
  = "fn" Identifier [ GenericParams ] "(" [ ParamList ] ")"
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
- Variadic `...` is only supported for `extern c` declarations.
- Generic functions are supported only for normal non-C non-prototype
  functions.

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
  | Block ;

LetStmt
  = "let" Identifier [ ":" Type ] "=" Expr ";" ;

VarStmt
  = "var" Identifier [ ":" Type ] "=" Expr ";" ;

ReturnStmt
  = "return" [ Expr ] ";" ;

BreakStmt
  = "break" ";" ;

ContinueStmt
  = "continue" ";" ;

DeferStmt
  = "defer" Expr ";" ;
```

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
  | BinaryExpr ;
```

Postfix forms:

```text
value.field
array[index]
function(arg)
generic_fn::[T](arg)
expr?
```

Array expressions:

```aurex
[1, 2, 3]
[0; 4]
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

Rules:

- The result of a block expression is the last expression without `;`.
- Assignment cannot be a block result.
- `return`, `break`, `continue`, and `defer` are statements, not tail
  expressions.
- Empty blocks do not infer a value in value-required positions.

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

## 11. Patterns

```ebnf
Pattern
  = PatternAtom { "|" PatternAtom } ;

PatternAtom
  = "_"
  | IntegerLiteral
  | "true"
  | "false"
  | Identifier [ "." Identifier ] [ PayloadBindings ]
  | "." Identifier [ PayloadBindings ] ;

PayloadBindings
  = "(" Identifier { "," Identifier } [ "," ] ")" ;
```

Rules:

- Enum patterns may be unscoped, enum-scoped, or inferred from matched enum
  type via `.case`.
- Payload binding count must match the enum case payload field count.
- Or-pattern alternatives cannot bind payloads in current M2.
- Integer/bool matches use literal patterns and require wildcard coverage where
  needed.

## 12. Basic Generics

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

## 13. Explicitly Rejected Syntax

Examples:

```aurex
*i32
[N]i32
[1 + 2]i32
Box[]
Box<i32>
foo::bar::Baz
fn(i32) -> i32
type Alias[T] = T;
enum Option[T] { none }
impl[T] Box[T] {}
fn add[T: Add](a: T, b: T) -> T { return a; }
fn foo[T]() where T: Copy {}
id[i32](1)
for x in values {}
```
