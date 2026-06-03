# Aurex M2 Grammar

This document freezes the current M2 syntax implemented by the repository.
It describes existing language surface only. It does not add new language
features.

## 1. Lexical Structure

M2 source is tokenized as keywords, identifiers, literals, punctuators, and
comments.

Identifiers are ASCII-oriented names accepted by the current lexer. `_` is a
valid identifier and is also used as a wildcard pattern.

The single-letter `c` is not a global keyword. It is a contextual ABI marker in
`extern c`, `export c fn`, and `extern c fn` type syntax; in all other
identifier positions, including parameter, local, function, and module path
names, `c` is an ordinary identifier.

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
- Import aliases use `as Identifier`; without `as`, the alias is the last
  module path segment, so `import core.mem;` imports module alias `mem`.
- Wildcard imports are not part of M2.
- Import aliases live in the module namespace and must not use the same name
  as a type or value member of the importing module.

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

```ebnf
ConstDecl
  = "const" Identifier ":" Type "=" Expr ";" ;

TypeAliasDecl
  = "type" Identifier [ GenericParams ] [ WhereClause ] "=" Type ";" ;
```

## 4. Type Syntax

```ebnf
Type
  = { PointerTypePrefix | ReferenceTypePrefix | ArrayTypePrefix | SliceTypePrefix } TypeAtom ;

PointerTypePrefix
  = "*" ( "mut" | "const" ) ;

ReferenceTypePrefix
  = "&" [ "mut" ] ;

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
  = Identifier { "." Identifier } [ GenericTypeArgs ] ;

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
&i32
&mut i32
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
foo.Box[i32]
core.mem.File
core.mem.Box[i32]
```

Rules:

- Pointer types require `mut` or `const` after `*`.
- Reference types use `&T` for a safe shared reference and `&mut T` for a
  safe mutable reference. References are semantic types distinct from raw
  pointers, though the current ABI lowers them as pointer-sized values.
  Reference pointees must be valid storage types, so `&void` and references to
  opaque value types are rejected. `&mut place` requires a writable place.
  M2 references do not include borrow checking, lifetimes, borrowed-return
  rules, or alias analysis.
- Array type length is an integer literal, not a const expression.
- Slice types require `const` or `mut` after `[]`; bare `[]T` is rejected.
- Slices are fat values represented as data pointer plus length. They can be
  produced from arrays or other slices. `[]mut T` is assignable to `[]const T`;
  `[]const T` is not assignable to `[]mut T`.
- Tuple types are anonymous product types. `(A, B)` has two elements, and
  `(A,)` is the one-element tuple form. `()` is not part of M2 syntax.
  Anonymous tuple elements are not directly field-accessible; destructure a
  tuple with a pattern, or use a named struct when field access is required.
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
- Type paths and expression selectors use the same dot spelling. A one-segment
  module qualifier such as `mem.File` resolves through an import alias. A
  multi-segment selector such as `core.mem.File` or `core.mem.page_size`
  resolves the visible module path first, then selects the exported type or
  value member. Semantic base kind decides whether a selector denotes a module,
  type, value, field, method, enum case, or associated function. `::` is not
  accepted.

## 4.1 Name Domains And Dot Selectors

M2 uses dot-only selectors with separate name domains. The parser keeps
postfix syntax uniform, and semantic analysis decides selector meaning from
the base expression or type:

```text
module.name       -> exported module member
module.path.name  -> exported member of a visible module path
type.name         -> enum case or associated function
value.name        -> field or method
```

Top-level names are split into module, type, and value domains:

- Module domain: module/package roots and import aliases.
- Type domain: structs, enums, opaque structs, type aliases, generic type
  parameters, and builtin primitive types.
- Value domain: functions, constants, globals, locals, and parameters.

Each type also has member domains:

- Type members: enum cases and associated functions.
- Instance members: fields and methods.

Rules:

- A module may export type and value members, but the same exported name cannot
  appear in both domains.
- Enum cases are type members. They are not inserted into the ordinary value
  namespace, so `some(1)` is rejected and `Option[i32].some(1)` is required.
- A type member name cannot be reused by an enum case and an associated
  function on the same type.
- A local or parameter name cannot shadow an import alias, visible root module
  name, generic type parameter, or visible type name. Same-scope duplicate
  locals/parameters are also rejected by the symbol table.
- A local declared in an inner lexical scope may shadow an outer local.
- Imported module members are selected through the import alias or through a
  visible full module path such as `samplelib.visibility.answer`. Unqualified
  lookup does not search imported modules.

## 5. Function Declarations

```ebnf
FnDecl
  = { FunctionDecorator } [ "unsafe" ] "fn" Identifier [ GenericParams ] "(" [ ParamList ] ")"
    [ "->" Type ] [ WhereClause ] ( Block | ";" ) ;

ParamList
  = Param { "," Param } [ "," ]
  | Param { "," Param } "," "..." ;

Param
  = Identifier ":" Type ;

FunctionDecorator
  = AbiName
  | BorrowContract ;

AbiName
  = "@name" "(" StringLiteral ")" ;

BorrowContract
  = "@borrow" "(" "return" "=" "[" BorrowSelector { "," BorrowSelector } "]" ")" ;

BorrowSelector
  = Identifier
  | "self"
  | "static"
  | "unknown" ;

ExportCFnDecl
  = { FunctionDecorator } "export" "c" [ "unsafe" ] "fn" Identifier [ GenericParams ] "(" [ ParamList ] ")"
    [ "->" Type ] [ WhereClause ] Block ;

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
  = "struct" Identifier [ GenericParams ] [ WhereClause ] "{"
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

Current M2 is ADT-first for ordinary and generic enums. Base type and case
discriminants are optional. Explicit base/discriminant syntax remains available
for C-like/repr-style enums.

```ebnf
EnumDecl
  = "enum" Identifier [ GenericParams ] [ ":" Type ] [ WhereClause ] "{"
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

enum Option[T] {
    some(T),
    none,
}

enum Status: u8 {
    ok = 0,
    err = 1,
}
```

Rules:

- Generic enum declarations support type parameters only. Bounds, associated
  types, const generics, and trait declarations remain outside M2.
- If no base type is written, the current implementation uses an internal
  integer tag representation.
- If no discriminant is written, tag values are assigned in declaration order.
- Duplicate discriminant values are rejected.
- Payload fields must be valid storage types.
- Payloads containing array storage are currently rejected by semantic analysis.

## 8. Impl Blocks

```ebnf
ImplBlock
  = "impl" [ GenericParams ] Type [ WhereClause ] "{" { MethodDecl } "}" ;

MethodDecl
  = [ Visibility ] FnDecl ;
```

Current semantic rules require the resolved impl target to be a named struct,
enum, or opaque struct type. Generic impl blocks are supported when every impl
generic parameter appears in the impl target type, for example
`impl[T] Box[T] { ... }`. Method-local generic parameters that are independent
of the impl target are still rejected by M2 semantic analysis.

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
array[index]
array[start:end]
array[:end]
array[start:]
array[:]
text[start:end]
function(arg)
generic_fn[T](arg)
expr?
```

Parser note: postfix expressions are stored as explicit compact AST nodes.
Bracket syntax is classified in the parser by conservative syntactic
guardrails: type-only arguments and generic call/type-literal continuations
emit `generic_apply`; selector continuations emit `generic_apply` only for
type-shaped bases and arguments, preserving value selectors such as
`items[index].field`; colon syntax emits `slice`; remaining value syntax emits
`index`. In M2.1 a type-shaped selector base or bare type argument is
syntactic: it begins with an uppercase identifier or uses a type-only form such
as a primitive, pointer, reference, tuple, slice, array, or function type.
Lowercase `name[index].field` is always parsed as value indexing followed by a
field selector. Later stages consume those nodes directly instead of running any
second raw-chain lowering step.

The postfix `?` operator is accepted only for structurally recognized
result-like and option-like enums. A result-like enum must have exactly
`ok(payload)` and `err(payload)` cases, and the enclosing function must return
a result-like enum whose `err` payload type matches. An option-like enum must
have exactly `some(payload)` and payload-free `none` cases, and the enclosing
function must return an option-like enum. The enum type name is not special;
extra cases or malformed payload shapes are not treated as `?`-compatible.

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
let part: str = text[1:3];
```

Struct literals:

```aurex
Point { x: 1, y: 2 }
Pair[i32, bool] { first: 1, second: true }
remote.Point { x: 1, y: 2 }
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
- Slice expressions require an array, slice, or `str` operand. Bounds must be
  integer expressions. For array and borrowed slice operands, current M2 does
  not add runtime bounds checks and does not add container iteration.
- `str[start:end]`, `str[:end]`, `str[start:]`, and `str[:]` use byte offsets
  and return `str`. The operation is checked at runtime: `start <= end`,
  `end <= strblen(text)`, and both bounds must be UTF-8 code point boundaries.
  Failure returns the empty `str` instead of trapping or constructing invalid
  UTF-8. This is not grapheme-cluster indexing, Unicode scalar iteration, or
  locale-aware text segmentation.
- Tuple literals infer a tuple type from their element expressions unless an
  expected tuple type is present. Arity and element types must match the
  expected tuple type. Empty tuple literals are rejected.
- Anonymous tuple field access is rejected. `value.0`, `value.1`,
  `value.first`, and `value.second` are not tuple syntax in M2.
- Tuple destructuring is supported in local `let` and `var` declarations and
  in tuple patterns: `let (left, _) = pair;` and
  `match pair { (left, right) => ... }`.

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

Safe operations include address-of `&place`, `ptraddr(p_or_ref)`, `strptr(s)`,
`strblen(s)`, `strvalid(bytes)`, `strfromutf8(bytes)`, `sizeof[T]`,
`alignof[T]`, and checked numeric `cast[T](x)`.

`&place` always creates `&T`; `&mut place` always creates `&mut T` and requires
a writable place. Address-of does not silently become a raw pointer in
initializers, calls, returns, struct literals, or any other expected-type
context. Code that needs a raw pointer address must use the explicit
`ptraddr(...)` / `ptrat[T](...)` raw-pointer boundary. Unary `*` on a safe
reference is a safe load or place projection; unary `*` on a raw pointer remains
unsafe-only.

`strvalid(bytes)` accepts a `[]const u8` or `[]mut u8` borrowed byte slice and
returns `bool`. `strfromutf8(bytes)` accepts the same slice shape and returns
`str`: on success it borrows the original byte slice as text, and on failure it
returns the empty `str`. A caller that needs to distinguish failure from valid
empty input must call `strvalid(bytes)`. A failed checked call never wraps
invalid input as `str`; unchecked construction remains `strraw(data, len)` and
is unsafe-only.

## 12. Patterns

```ebnf
Pattern
  = PatternAtom { "|" PatternAtom } ;

PatternAtom
  = "_"
  | IntegerLiteral
  | "true"
  | "false"
  | "const" Identifier
  | TuplePattern
  | SlicePattern
  | StructPattern
  | ExplicitEnumCasePattern
  | Identifier
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
  | "const" Identifier
  | TuplePattern
  | SlicePattern
  | StructPattern
  | ExplicitEnumCasePattern
  | "." Identifier [ PayloadBindings ] ;

ExplicitEnumCasePattern
  = NamedType "." Identifier [ PayloadBindings ] ;

NamedType
  = Identifier { "." Identifier } [ "[" Type { "," Type } [ "," ] "]" ] ;
```

Rules:

- A bare identifier pattern is always a binding. Bare enum case patterns such
  as `some(v)` are rejected.
- Enum case patterns are either explicit `Type.case` / `Type[Args].case` or
  inferred shorthand `.case` from the matched enum type.
- Constant patterns use `const NAME` and currently match integer and bool
  constants only.
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
- Integer/bool matches use literal patterns and require wildcard coverage for
  open integer domains. Bool supports full `true`/`false` coverage.
- Tuple, struct, and fixed-array matches can be proven exhaustive when all
  structural slots are finite leaf domains such as `bool` or no-payload enum
  values. Unguarded arms and literal `if true` guards contribute to
  exhaustiveness; literal `if false` and dynamic guards do not. Fixed arrays
  longer than 4096 elements have an explicit M2.1 boundary and require an
  irrefutable arm. Other tuple, struct, array, and slice shapes still require an
  irrefutable arm.

## 13. Basic Generics

```ebnf
GenericParams
  = "[" GenericParam { "," GenericParam } [ "," ] "]" ;

GenericParam
  = Identifier ;

GenericTypeArgs
  = "[" Type { "," Type } [ "," ] "]" ;

ExplicitGenericCall
  = SelectorExpr GenericTypeArgs "(" [ ArgumentList ] ")" ;

; The parser emits an explicit generic_apply callee before the call suffix.

WhereClause
  = "where" WhereConstraint { "," WhereConstraint } ;

WhereConstraint
  = Identifier ":" Capability { "+" Capability } ;

Capability
  = "Sized" | "Eq" | "Ord" | "Hash" ;
```

Supported generic positions:

| Syntax position | M2 support | Notes |
| --- | ---: | --- |
| `struct Name[T]` | yes | Basic generic struct |
| `fn name[T]` | yes | Normal non-C non-prototype function |
| `Name[T]` type arguments | yes | Type context |
| `Name[T] { ... }` | yes | Generic struct literal |
| `name[T](...)` | yes | Explicit generic function call |
| `type Alias[T]` | yes | Structural generic type alias |
| `enum E[T]` | yes | Generic ADT enum |
| `impl[T] Type[T]` | yes | Impl generics must appear in the impl target |
| method-local generics | no | Generic parameters independent of the impl target are not supported |
| generic bounds | no | Not part of M2 |
| `where T: Eq + Hash` | yes | Built-in non-resource capabilities only |
| `<>` generics | no | Aurex uses `[]` |

Postfix brackets are emitted as explicit parser AST nodes. Type-only arguments
and generic call/type-literal continuations emit `generic_apply`, colon syntax
emits `slice`, and the remaining value-shaped form emits `index`. Selector
continuations only force `generic_apply` for type-shaped bases and arguments, so
`Type[T].case` stays a type selector while `items[index].field` stays value
indexing. Explicit generic calls use `name[T](...)` or `module.name[T](...)`.

The M2 `where` clause is deliberately small. `Sized`, `Eq`, `Ord`, and `Hash`
are built-in non-resource capability predicates used for type checking and
diagnostics. `Copy`, `Drop`, user-defined traits, associated types, const
generics, trait objects, and impl-local generic methods remain outside M2.

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
foo::bar
foo::bar::Baz
fn add[T: Add](a: T, b: T) -> T { return a; }
fn foo[T]() where T: Copy {}
impl Box { fn id[T](self: *const Box, value: T) -> T { return value; } }
id::[i32](1)
for x in values {}
```
