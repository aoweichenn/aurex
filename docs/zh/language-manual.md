# Aurex 语言参考手册

日期：2026-06-06
阶段：M7d-J cleanup marker query / tooling consumption closure，建立在 M7d-I move rejection facts、M7d-H
index/slice place-state conservative closure、M7d-G cleanup marker ABI policy、M7d-F tuple element
place-state、M7d-E aggregate rollback codegen、M7d-D RAII runtime lowering、M7d-C RAII user surface、
M7d-B struct field place-state、M7c lifetime/dropck facts、M7b borrow contract、M7a CFG-sensitive loan
facts、M6 resource semantics、M5 default trait methods、M4 trait/protocol、M3 module/generic 和 M2
language core 基线之上。
状态：按当前仓库实现编写的官方语言参考手册，不描述尚未落地的未来功能。

本文说明当前 Aurex 能写什么、怎么写、哪些地方会被拒绝。语法以
`include/aurex/frontend/syntax/core/token.hpp`、`include/aurex/frontend/syntax/ast/*`、
`src/frontend/parse/grammar/*`、`src/frontend/sema/internal/*`、`tests/samples/**`、`tests/gtest/frontend/**` 和
`examples/**` 的当前实现为准。`docs/spec/m2_grammar.md` 是早期 M2 语法冻结文档；遇到差异时，以本文和当前源码为准。

## 前言：范围、记号和当前能力

本手册是当前仓库的语言参考，不是未来路线图。它描述编译器已经能解析、检查、降低并在测试矩阵中覆盖的语言表面；
设计文档中仍处于规划、shadow mode 或后续方向的内容不会被写成已支持语法。

语法块采用近似 EBNF 的说明方式：

```text
Name          = production
"keyword"     表示必须出现的关键字或标点
Identifier    表示词法标识符
A?            表示 A 可选
A*            表示 A 可重复零次或多次
A+            表示 A 可重复一次或多次
A | B         表示二选一
```

代码块使用 `aurex` 标注时表示可按当前语法书写的 Aurex 源码片段；使用 `text` 标注时表示语法形式或 token 列表。

当前 Aurex 已经可以做这些事：

- 编译包含 `main` 的可执行程序，入口支持 `fn main() -> i32` 和 `fn main() -> void`。
- 编写多模块工程，使用 `module`、`import`、`pub import`、`pub use`、`pub(package)`、module part 和 package source-root。
- 定义 scalar、raw pointer、safe reference、array、slice、tuple、function pointer、struct、opaque struct、ADT enum 和 type alias。
- 使用 private return type inference、函数原型、C ABI `extern c` / `export c fn`、variadic extern C、`@name` ABI symbol 和 `@borrow` contract。
- 编写 `let` / `var`、pattern binding、let-else、if/while/for/range-for、match、block expression、try `?`、defer 和 unsafe block。
- 使用泛型函数、泛型类型、泛型 impl、method-local impl 泛型、`where` capability、nominal static trait、default trait method、associated type 和 associated-type equality。
- 使用 safe borrowed view：`&T`、`&mut T`、显式 origin reference、borrowed slice 和 `str`，并获得当前 CFG-sensitive local loan checking、borrow summary 和 lifetime/origin 诊断。
- 使用 M6/M7 resource semantics：非 `Copy` 值的 move 检查、reinit、struct 字段 partial move/reinit、字段级 cleanup/drop flag、cleanup/drop-glue 规划、`Copy` capability 和 defer/cleanup 交错检查。
- 使用 M7d-D RAII destructor runtime lowering：`impl Drop for T { fn drop(self: deinit T) -> void { ... } }`
  会进入 checked destructor facts、resource semantics、drop-glue/dropck、query fingerprint 和 IDE hover；静态可解析
  custom destructor 会在 cleanup lowering 中变成真实 direct call。
- 使用 M7d-E aggregate rollback lowering：函数体内含 droppable 元素的 struct/tuple/array literal 和多 payload
  enum 构造会在后续元素提前 `return` 时清理已经初始化成功的元素；constant initializer 和无 droppable 元素的
  aggregate 仍保持 lightweight lowering。
- 使用 M7d-G/M7d-J cleanup marker facts：IR `drop` / `drop_if` cleanup marker 会携带 compiler-owned
  cleanup ABI policy，并通过 stable query DTO 暴露给 lower-IR cache、IDE semantic fact、hover、workspace
  index 和 session reuse；generic / associated / opaque / unknown cleanup 当前仍是 marker-only。
- 使用 M7d-H/M7d-I resource hardening facts：index/slice place-state 采用 same-root conservative may-alias；
  pattern payload、try `?` payload 和 indexed move-out 的当前拒绝会进入 move rejection facts，供 checked dump、
  query authority 和 IDE/tooling 使用。
- 使用语言内建：数值 cast、pointer/address builtin、slice builtin、UTF-8 string builtin、`sizeof` 和 `alignof`。
- 通过 C FFI 和 unsafe raw pointer 实现底层库。仓库中的 `examples/libs/regex` 已经使用当前语言写出多模块正则库，并覆盖编译、执行、资源预算和错误路径。

当前仍不是语言能力的内容：

- 没有标准库级拥有型 `String`、容器库、用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、
  dynamic destructor ABI call、cleanup marker runtime ABI call 或 async/unwind-aware drop。
- 没有 package manager、workspace、dependency resolver、lockfile、glob import/use 或通用 selective import。
- 没有 trait object、dynamic dispatch trait object、generic associated type、associated const、specialization、const generic 或 `<T>` 风格泛型。
- 没有 closure capture、async/generator、语言级线程/atomic/concurrency memory model；可以通过 C FFI 调用外部并发 API，但 safe borrow checker 只为当前语言的本地控制流和函数 summary 建模。
- 没有完整 Rust-style apostrophe lifetime surface、full Polonius Datalog、raw pointer alias safe proof、indexed move-out 或 `replace` / `take` / `swap` 内建；本地 tuple 元素 partial move/reinit 已支持，但 array/slice/index place 仍保守。
- 本阶段明确不实现任何标准库 API 或标准库拥有型资源 wrapper；资源语义收口只发生在 compiler facts、IR marker、
  query/cache 和 tooling 投影。

## 1. 程序结构和入口

完整源文件由 module header、可选 module preamble 和 item 序列组成：

```text
SourceFile     = ModuleHeader ModulePreamble* Item*
ModuleHeader   = "module" ModulePath ("part" Identifier)? ";"
ModulePreamble = PartDecl | ImportDecl | UseDecl
```

```aurex
module hello;

fn main() -> i32 {
    return 0;
}
```

规则：

- 一个源文件属于一个模块，通常以 `module path;` 开头。
- 可执行程序入口是 `main`。常见形式是 `fn main() -> i32` 或 `fn main() -> void`。
- 语句以 `;` 结束，块用 `{ ... }`。
- 私有普通函数可以推导返回类型；`pub fn`、`extern c fn`、`export c fn` 和函数原型必须显式写返回类型。

## 2. 词法和 token

词法阶段把 UTF-8 源码按 ASCII 标识符、关键字、字面量、注释和标点 token 切分。语义阶段仍允许字符串和
`char` 使用 Unicode 内容，但标识符本身当前固定为 ASCII 形式。

### 2.1 关键字

当前关键字：

```text
module import as pub priv extern export unsafe
fn struct opaque enum trait const type impl where
match let var if else for in is while break continue defer return
true false null
void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str char
mut
cast ptrcast bitcast sizeof alignof ptraddr ptrat
sliceptr slicelen strptr strblen strvalid strfromutf8 strraw
```

`c`、`part`、`use`、`origin` 和 `deinit` 不是全局关键字。`c` 只在 `extern c`、`export c fn` 和
`extern c fn(...) -> T` 这类 ABI 语法中作为上下文标记；`part`、`use` 和 `origin`
只在各自语法位置作为上下文标记；`deinit` 只在参数冒号后的类型前缀位置作为 Drop self 参数修饰符。
在参数名、局部名、函数名和模块路径中，它们仍可作为普通标识符。

### 2.2 标识符

```text
Identifier = [A-Za-z_][A-Za-z0-9_]*
```

当前实现按 ASCII 标识符处理，不支持 Unicode 标识符。单独的 `_` 是合法标识符，也可在 pattern 中表示通配。

### 2.3 注释

```aurex
// 行注释

/* 块注释 */

/* 块注释可以
   /* 嵌套 */
*/
```

### 2.4 标点和操作符 token

```text
( ) { } [ ] , . ... ; : :: -> => @ ?
+ ++ += - -- -= * *= / /= % %=
& &= | |= ^ ^= ~ !
= == != < <= > >= << <<= >> >>= && ||
```

注意：

- `::`、`++`、`--` 会被词法器识别，但当前源码语法不支持这些写法，解析或语义阶段会诊断。
- 泛型使用 `[]`，不使用 `< >`。
- 模块、类型、字段、方法、枚举 case 都使用 `.` 选择，不使用 `::`。

## 3. 字面量

字面量是可直接出现在表达式或 const initializer 中的源代码值。当前实现覆盖整数、浮点、布尔、空指针、
UTF-8 字符串、C 字符串、raw string、byte string、byte literal 和 Unicode scalar `char`。

### 3.1 整数

```aurex
0
123
1_000
0xff
0Xff
0b1010
0B1010
1i8
42usize
```

规则：

- `_` 只能放在两个合法数字之间。
- `1_`、`1__2`、`0x_FF`、`0b_1010` 会被拒绝。
- 后缀支持 `i8`、`i16`、`i32`、`i64`、`isize`、`u8`、`u16`、`u32`、`u64`、`usize`。
- 无后缀整数字面量可根据上下文推导并做范围检查。

### 3.2 浮点

```aurex
1.0
0.5
.5
1.
1e3
1.0e-3
1.0f32
1.0f64
```

规则：

- 没有期望类型时默认 `f64`。
- 后缀只支持 `f32` 和 `f64`。
- 整数后缀不能用于浮点，浮点后缀不能用于整数。

### 3.3 字符串、C 字符串、byte、char

```aurex
"hello"        // str，UTF-8 文本
c"hello"       // *const u8，C FFI 字符串，拒绝内部 NUL
r"raw\n"       // str，不解释 escape，可跨行
b"abc\n"       // [N]u8，ASCII byte string
b'a'           // u8 byte literal
'\n'           // char
'λ'            // char，Unicode scalar value
'\u{03BB}'     // char
```

普通字符串和 `char` 支持常见 escape，例如 `\0`、`\n`、`\r`、`\t`、`\\`、`\"` 和 `\u{...}`。byte string 支持简单 byte escape，拒绝非 ASCII raw byte 和 Unicode escape。

### 3.4 布尔和空指针

```aurex
true
false
null
```

`null` 只能在期望类型是 raw pointer 的位置使用。

## 4. 模块、导入和可见性

模块系统负责源文件归属、跨文件查找、package 边界和 public surface。当前实现支持 primary module、module part、
普通导入、module re-export、selective item re-export 和 package visibility；它不是 package manager。

### 4.1 模块声明和 part 文件

```text
ModulePath   = Identifier ("." Identifier)*
ModuleHeader = "module" ModulePath ("part" Identifier)? ";"
PartDecl     = "part" Identifier ";"
```

```aurex
module app.main;
```

模块路径用 `.` 分隔。导入其他模块时，编译器会在当前文件相对目录和命令行 `-I` import path 下查找。

Primary module 文件还可以在 `module path;` 之后、import 和 item 之前声明 part 文件：

```aurex
module app.parser;

part lexer;
part grammar;
```

对应的 part 文件使用同一个 module path，并在 module header 里写 part 名：

```aurex
module app.parser part lexer;

fn helper() -> i32 {
    return 1;
}
```

规则：

- `part name;` 只能写在 primary module 的 `module` 声明之后、import/use/item 之前。
- part 文件不能声明 nested part list。
- part 文件不能声明 selective re-export；`pub use` / `pub(package) use` 只能写在 primary module 文件的 import/re-export 区域。
- `part` 是上下文标记，不是全局关键字；普通函数、参数和局部变量仍可命名为 `part`。

### 4.2 导入

导入把一个可见模块绑定到当前模块的 alias。未限定名字不会自动搜索导入模块；跨模块调用通常写成
`alias.name`。

```text
ImportDecl = Visibility? "import" ModulePath ("as" Identifier)? ";"
UseDecl    = ("pub" | "pub" "(" "package" ")") "use" ModulePath "." Identifier ("as" Identifier)? ";"
```

```aurex
import common.status;
import common.result as result;
pub import common.metrics as metrics;
pub(package) import common.package_metrics as package_metrics;
priv import internal.detail as detail;
```

规则：

- `import path;` 默认 alias 是最后一个路径段。
- `import path as name;` 显式指定 alias。
- `pub import` 可 re-export 给其他模块。
- `pub(package) import` 只在同一 `PackageKey` 内 re-export；跨 package 消费者不能通过该 facade
  看到被重导出的模块。
- `priv import` 明确表示私有导入。
- primary module 文件可以声明 selective re-export：

```aurex
pub use common.status.Status;
pub use common.status.make as make_status;
pub(package) use common.status.PackageStatus as PackageStatus;
```

规则：

- `pub use module.Item;` 只把目标模块的单个 item 暴露为当前模块 facade API。
- `as Alias` 会改变 facade 暴露名，不改变目标 item 的定义身份。
- `pub(package) use` 只在同一 `PackageKey` 内可见。
- `pub use` 只能写在 primary module 的 import/re-export 区域，不能写在 part 文件或 item 之后。
- 当前不支持 glob use、bare `use` 或 `priv use`；`use` 是上下文标记，不是全局关键字。

- `PackageKey` 可由 CLI `--package` 指定；如果未指定，driver 会从输入文件所在目录向上识别
  `aurex.toml` 的 `[package] name/version` 作为 package identity。manifest 可选
  `source-root = "src"`，用于把 logical module path 映射到 package source root 下的文件路径。
  启用 source-root 后，primary module 文件路径必须和 `module path;` 一致；例如
  `module app.util;` 必须位于 `src/app/util.ax`。
  编译器会把该 source-root topology 记录到 module graph / incremental cache 边界中，因此 package
  source layout 变化不会只停留在文件查找层。
  `-I` import root 也会优先使用其 manifest identity 和 source-root；缺失 manifest 时回退到
  import root 路径 identity 与旧的 import root 文件布局。
- 当前不支持 glob import/use、general selective import、dependency resolver、workspace、lockfile、
  version solving 或 package manager。
- 未限定名字不会自动搜索导入模块，跨模块成员需要写 `alias.name` 或可见完整模块路径。

### 4.3 可见性

可见性是 item、field、method 和 import 的 public surface 规则。Aurex 默认 private，导出 API 必须显式标注。

```text
Visibility = "pub" | "priv" | "pub" "(" "package" ")"
```

```aurex
pub fn api() -> i32 { return 1; }
pub(package) fn package_api() -> i32 { return 2; }
priv fn helper() -> i32 { return 2; }

pub struct PublicBox {
    pub value: i32;
    pub(package) package_value: i32;
    priv secret: i32;
}
```

规则：

- 顶层 item 默认 private。
- struct field 默认 private。
- impl method 默认 private。
- import 默认 private。
- 跨模块 API、字段和方法必须显式 `pub`。
- `pub(package)` 表示同一 package 内可见，跨 `-I` import path 派生的 package 不可见。
- `pub(crate)`、`pub(in path)` 和其他 scoped visibility 当前不支持。
- `export c fn` 是对外 ABI 符号，不能作为 private API。

## 5. 类型系统和类型语法

类型语法描述值的存储形态、借用形态、函数 ABI 形态和泛型实例形态。语义分析会拒绝无效 storage type、
不完整 opaque value、非法 ABI 类型和不满足能力约束的类型。

```text
Type              = PrimitiveType
                  | NamedType
                  | PointerType
                  | ReferenceType
                  | ArrayType
                  | SliceType
                  | TupleType
                  | FunctionType
NamedType         = ModulePath ("[" Type ("," Type)* ","? "]")?
PointerType       = "*" ("const" | "mut") Type
ReferenceType     = "&" "mut"? OriginQualifier? Type
OriginQualifier   = "[" Identifier ("|" Identifier)* "]"
ArrayType         = "[" IntegerLiteral "]" Type
SliceType         = "[]" ("const" | "mut") Type
TupleType         = "(" Type "," ")" | "(" Type "," Type ("," Type)* ","? ")"
FunctionType      = "unsafe"? ("extern" "c")? "fn" "(" FunctionTypeParams? ")" "->" Type
FunctionTypeParams = FunctionTypeParam ("," FunctionTypeParam)* ("," "...")? | "..."
FunctionTypeParam = (Identifier ":")? Type
```

### 5.1 基础类型

```text
void bool
i8 u8 i16 u16 i32 u32 i64 u64 isize usize
f32 f64
str char
```

`str` 是借用的 UTF-8 文本值，当前 ABI 表示为数据指针加 byte length。它不是拥有型 `String`。

### 5.2 命名类型和泛型类型参数

命名类型可以是当前模块类型、导入模块类型、泛型参数或 associated type projection。泛型实参使用 `[]`。

```aurex
Point
Box[i32]
Pair[i32, bool]
common.status.Health
```

规则：

- 泛型实参使用 `[]`。
- `Box[]` 会被拒绝。
- `Box<i32>` 不是 Aurex 泛型语法。
- 类型路径也使用 `.`。

### 5.3 Raw pointer

Raw pointer 是 unsafe/FFI 边界类型。它能表达地址和可变性，但不参与 safe borrow 证明。

```aurex
*const i32
*mut i32
*mut *const u8
```

规则：

- `*` 后必须写 `const` 或 `mut`。
- raw pointer 解引用、raw pointer 字段/索引投影、`ptrcast`、`ptrat` 等操作受 `unsafe` 约束。
- `*i32` 不是合法类型。

### 5.4 Safe reference

Safe reference 是当前 borrow/lifetime 系统建模的主要借用类型。共享引用 `&T` 可读，唯一引用 `&mut T`
可写；显式 origin qualifier 用于把函数签名中的 borrowed view 来源写成可检查 contract。

```aurex
&i32
&mut i32
&Pair
&mut Pair
&[data] i32
&mut [data] i32
&[left | right] i32
```

示例：

```aurex
fn read(value: &i32) -> i32 {
    return *value;
}

fn write(value: &mut i32, replacement: i32) -> void {
    *value = replacement;
}
```

规则：

- `&T` 是共享引用，`&mut T` 是可写引用。
- `&[origin] T` / `&mut [origin] T` 是带显式 origin 的引用类型。origin 名称在泛型参数列表中用
  `origin name` 声明，例如 `fn id[origin data](value: &[data] i32) -> &[data] i32`。
- `&[left | right] T` 表示返回或存储的 borrowed view 可能来自多个 origin。`|` 只在 reference
  origin qualifier 中表示 origin union；不要把它理解为 trait bound。
- `&T` / `&mut T` 没有显式 origin 时，语义分析会按函数签名、`@borrow` contract、函数体 summary 和 elision
  规则推导 region facts；public/prototype/extern 边界上含多个候选输入 origin 的 borrowed return 会被诊断为歧义。
- `&mut place` 要求 `place` 可写。
- `&mut T` 可赋给 `&T`；反向不允许。
- reference pointee 必须是有效 storage type；`&void` 和 opaque value reference 会被拒绝。
- raw pointer 不参与 safe borrow 证明；从 raw pointer 或 unchecked string path 派生的 borrowed return 会保守记录为
  unknown 或在本地来源可定位时诊断本地借用逃逸。

### 5.5 数组和 slice

数组拥有固定长度元素存储；slice 是 borrowed fat value，只携带 data pointer 和 element count。

```aurex
[4]i32
[]const i32
[]mut u8
```

数组长度只能是整数字面量，不是 const 表达式：

```aurex
let values: [4]i32 = [1, 2, 3, 4];
let all: []const i32 = values[:];
let middle: []const i32 = values[1:3];
```

规则：

- `[N]T` 是固定长度数组。
- `[]const T` / `[]mut T` 是 borrowed slice。
- `[]T` 会被拒绝，必须写 `const` 或 `mut`。
- `[]mut T` 可赋给 `[]const T`；反过来不行。
- slice 是 fat value：data pointer + length。

### 5.6 Tuple

Tuple 是结构化值类型，当前主要用于返回多个值和 pattern 解构。

```aurex
(i32, bool)
(i32,)
```

示例：

```aurex
let pair = (1, true);
let first = pair.0;
let second = pair . 1;
let (value, flag) = pair;
```

规则：

- `(A, B)` 是二元 tuple。
- `(A,)` 是一元 tuple。
- `()` 当前不是合法 tuple 类型。
- 匿名 tuple 支持数字字段访问：`value.0`、`value.1`，也支持带空格的 `value . 0`。
- 数字字段必须在 tuple 元素范围内；越界会报 `tuple field index is out of range`。
- 匿名 tuple 不支持命名字段，例如 `value.first` / `value.second`；这类访问会报 `tuple field access requires a numeric field`。需要命名字段时使用 named struct。

### 5.7 函数类型

函数类型表示非捕获函数指针。它不是 closure 类型，也不保存环境。

```aurex
fn(i32, i32) -> i32
unsafe fn(*const i32) -> i32
extern c fn(*const u8, ...) -> i32
unsafe extern c fn(*const u8) -> i32
```

规则：

- 函数类型是非捕获函数指针类型。
- `unsafe fn` 函数值调用也需要 unsafe context。
- `extern c fn` 使用 C ABI。
- variadic `...` 只支持 `extern c`。

## 6. 声明 item

item 是模块级或容器级声明。顶层 item 构成模块 API；trait、impl、extern block 内部也包含受限制的 item
子集。

```text
Item = ConstDecl
     | TypeAlias
     | StructDecl
     | EnumDecl
     | TraitDecl
     | ImplBlock
     | ExternBlock
     | FunctionDecl
     | ExportCFunction
```

### 6.1 常量

常量声明创建模块级 compile-time value。当前 const 不是完整 comptime 系统。

```text
ConstDecl = Visibility? "const" Identifier ":" Type "=" Expr ";"
```

```aurex
const ANSWER: i32 = 42;
const TEXT: str = "ok";
```

当前 const initializer 支持字面量、部分标量运算、struct/enum 常量等已落地路径，但不是完整 comptime。

### 6.2 类型别名

类型别名给类型表达式命名；别名可以泛型化，也可以带 `where` 约束。

```text
TypeAlias = Visibility? "type" Identifier GenericParams? WhereClause? "=" Type ";"
```

```aurex
type Count = i32;
type Bytes4 = [4]u8;
type BoxPtr[T] = *mut T;
```

泛型别名可以使用 `where` 约束：

```aurex
type Ptr[T] where T: Sized = *const T;
```

### 6.3 Struct

Struct 是 named field product type。字段有独立可见性，默认 private。

```text
StructDecl  = Visibility? "struct" Identifier GenericParams? WhereClause? "{" StructField* "}"
StructField = Visibility? Identifier ":" Type ";"
```

```aurex
struct Point {
    x: i32;
    pub y: i32;
}

struct Box[T] {
    value: T;
}
```

规则：

- 字段以 `;` 分隔。
- 字段默认 private。
- payload 或字段类型必须是有效 storage type。

### 6.4 Opaque struct

Opaque struct 用于 FFI 或隐藏布局类型。它只能通过 pointer/reference 这类间接形式使用。

```aurex
extern c {
    opaque struct FILE;
}
```

opaque struct 只能通过指针使用，不能按值构造、取 `sizeof` 或访问字段。

### 6.5 Enum

Enum 是 nominal ADT。case 属于 enum 类型命名空间，可以有 payload，也可以作为 C-like tag 值。

```text
EnumDecl = Visibility? "enum" Identifier GenericParams? WhereClause? (":" IntegerType)? "{" EnumCase* "}"
EnumCase = Identifier ("(" Type ("," Type)* ","? ")")? ("=" IntegerLiteral)? ","
```

```aurex
enum OptionI32 {
    some(i32),
    none,
}

enum Status: u8 {
    ok = 0,
    err = 1,
}

enum Result[T] {
    ok(T),
    err(i32),
}
```

规则：

- enum 是 ADT-first：可有 payload，可泛型，可省略底层整数类型。
- 显式 `: u8` / `: u16` 等可用于 C-like/repr enum。
- enum case 是类型成员，不进入普通 value namespace。写 `OptionI32.some(1)`，不要写 `some(1)`。
- payload 中包含数组存储目前会被拒绝。

### 6.6 Function

函数声明定义可调用实体。Aurex 允许 private 普通函数推导返回类型，但 public/ABI/prototype surface 必须显式写返回类型。

```text
FunctionDecl = FunctionDecorator* Visibility? "unsafe"? "fn" Identifier GenericParams?
               "(" ParamList? ")" ("->" Type)? WhereClause? (Block | ";")
ParamList    = Param ("," Param)* ","?
Param        = Identifier ":" Type
```

```aurex
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

pub fn exported(value: i32) -> i32 {
    return value + 1;
}

fn inferred(value: i32) {
    return value + 1;
}
```

规则：

- private 普通函数可推导返回类型。
- public、extern C、export C、prototype 必须显式返回类型。
- 参数必须写类型。
- 函数原型用 `;` 结束，定义用 block。

### 6.7 Extern C 和 Export C

`extern c` 声明外部 C ABI 符号；`export c fn` 定义从 Aurex 导出的 C ABI 符号。ABI symbol 可用
`@name` 指定。

```text
ExternBlock     = "extern" "c" "{" ExternItem* "}"
ExternItem      = FunctionDecorator* "unsafe"? "fn" Identifier "(" ExternParamList? ")" "->" Type ";"
ExternParamList = Param ("," Param)* ("," "...")? | "..."
ExportCFunction = FunctionDecorator* "export" "c" "unsafe"? "fn" Identifier "(" ParamList? ")" "->" Type Block
FunctionDecorator = "@" "name" "(" StringLiteral ")"
                  | "@" "borrow" "(" "return" "=" "[" BorrowSelector ("," BorrowSelector)* ","? "]" ")"
BorrowSelector = Identifier | "self" | "static" | "unknown"
```

```aurex
extern c {
    @name("strlen")
    fn strlen(text: *const u8) -> usize;
    @name("snprintf")
    fn snprintf(buffer: *mut u8, size: usize, format: *const u8, ...) -> i32;
}

@name("aurex_add")
export c fn aurex_add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

规则：

- `@name("...")` 是函数声明前装饰器，用于指定 ABI symbol。
- `@borrow(return = [...])` 也是函数声明前装饰器，用于声明 borrowed return 的来源。它可用于普通函数、原型、
  `extern c` 函数、trait requirement 和 impl method。
- variadic 只支持 `extern c` 函数。
- C ABI 函数不支持泛型。

Borrow contract 写法：

```aurex
@borrow(return = [value])
fn view(value: &i32) -> &i32 {
    return value;
}

trait Reader {
    @borrow(return = [self])
    fn item(self: &Self) -> &Self;
}

extern c {
    @borrow(return = [static, unknown])
    fn global_view() -> &i32;
}
```

selector 规则：

- selector 可以是参数名、`self`、`static` 或 `unknown`。
- `self` 表示 receiver 来源；只有函数第一个参数命名为 `self` 时才有意义。
- `static` 表示返回 borrowed view 来自静态存储。
- `unknown` 表示外部或 unsafe 边界无法证明具体来源；它会保守影响调用方 lifetime/borrow facts。
- selector 列表不能为空；重复 selector 或声明与函数体 summary 不一致时会诊断。
- postfix decorator 会被解析但诊断；推荐始终把 `@name` / `@borrow` 写在函数声明之前。

### 6.8 Impl 和方法

`impl` 把方法和 associated type 实现绑定到 named type，或把 trait conformance 显式绑定到类型。

```text
ImplBlock = "impl" GenericParams? Type ("for" Type)? WhereClause? "{" ImplItem* "}"
ImplItem  = FunctionDecl | TypeAlias
```

```aurex
struct Counter {
    value: i32;
}

impl Counter {
    pub fn new(value: i32) -> Counter {
        return Counter { value: value };
    }

    pub fn add(self: &mut Counter, delta: i32) -> i32 {
        self.value = self.value + delta;
        return self.value;
    }
}
```

规则：

- impl target 必须解析为 named struct、enum 或 opaque struct。
- receiver 参数必须是第一个参数，并命名为 `self`。
- 泛型 impl 支持 `impl[T] Box[T] { ... }` 这种 owner 泛型。
- method-local 独立泛型已支持，例如 `fn id[U](self: &Box[T], value: U) -> U`。trait requirement
  里的 method-local 泛型仍会被拒绝；trait 的泛型参数应写在 trait 自身或外层 impl 上。

## 7. 语句

语句构成函数体的控制流和副作用。当前语句系统覆盖局部绑定、pattern binding、赋值、条件、循环、
scope cleanup、early return 和 unsafe block。赋值不是表达式。

```text
Stmt = LetStmt
     | VarStmt
     | AssignStmt
     | IfStmt
     | WhileStmt
     | ForStmt
     | ForRangeStmt
     | BreakStmt
     | ContinueStmt
     | ReturnStmt
     | DeferStmt
     | UnsafeBlock
     | ExprStmt
     | Block
```

### 7.1 局部变量

局部绑定分为不可重新赋值的 `let` 和可重新赋值的 `var`。两者都可以显式写类型，也可以从 initializer 推导。

```text
LetStmt = "let" Pattern (":" Type)? "=" Expr ("else" Block)? ";"
VarStmt = "var" Pattern (":" Type)? "=" Expr ("else" Block)? ";"
```

```aurex
let value: i32 = 1;
let inferred = value + 1;
var count: i32 = 0;
```

规则：

- `let` 不可重新赋值。
- `var` 可重新赋值。
- 类型可省略，由 initializer 推导。

### 7.2 Pattern 局部绑定和 let-else

局部绑定左侧可以是 pattern。refutable pattern 必须配合 `else`，并且 `else` block 必须发散或提前退出。

```aurex
let (left, right) = pair;

let .some(value) = option else {
    return 0;
};
```

规则：

- 没有 `else` 时，pattern 必须 irrefutable。
- 有 `else` 时可用 refutable pattern，但 `else` block 不能 fall through。

### 7.3 赋值和复合赋值

赋值目标必须是可写 place，例如 `var` 局部、可写字段、可写索引、`&mut` 解引用或 unsafe raw pointer projection。

```text
AssignStmt = Expr AssignOp Expr ";"
AssignOp   = "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "<<=" | ">>=" | "&=" | "^=" | "|="
```

```aurex
value = 1;
value += 2;
value <<= 1;
```

支持：

```text
= += -= *= /= %= <<= >>= &= ^= |=
```

赋值是语句，不是表达式。

### 7.4 If statement

Statement-form `if` 控制执行路径，不产生表达式值；没有 `else` 时只在条件为真时执行 then block。

```text
IfStmt = "if" Expr ("is" Pattern)? Block ("else" (IfStmt | Block))?
```

```aurex
if value > 0 {
    return 1;
} else if value == 0 {
    return 0;
} else {
    return -1;
}
```

statement-form `if` 可以没有 `else`。

### 7.5 While

`while` 重复执行 block，直到条件为 false。条件也可以带 `is Pattern` 做 pattern condition。

```text
WhileStmt = "while" Expr ("is" Pattern)? Block
```

```aurex
while i < 10 {
    i += 1;
}
```

Pattern condition 也支持：

```aurex
while current is .next(value) {
    current = Step.next(value - 1);
}
```

### 7.6 C-style for

C-style `for` 提供 init、condition、update 三段控制。init 和 update 是语句位置，condition 是表达式位置。

```text
ForStmt = "for" ForInit? ";" Expr? ";" ForUpdate? Block
ForInit = LetStmt | VarStmt | AssignStmt | ExprStmt
ForUpdate = AssignStmt | ExprStmt
```

```aurex
for var i: i32 = 0; i < 10; i += 1 {
    sum += i;
}

for ; ; {
    break;
}
```

### 7.7 Range for

Range for 是当前内建的整数 range 循环语法，不是通用 iterator protocol。

```text
ForRangeStmt = "for" Identifier "in" "range" "(" Expr ("," Expr ("," Expr)?)? ")" Block
```

```aurex
for i in range(4) {
    sum += i;
}

for i in range(2, 5) {
    sum += i;
}

for i in range(1, 6, 2) {
    sum += i;
}
```

规则：

- `range(end)` 从 0 到 end，不含 end。
- `range(start, end)` 不含 end。
- `range(start, end, step)` 使用显式步长。
- 这不是通用 iterator 语法；`for x in values {}` 当前不支持。

### 7.8 Break、continue、return、defer

`break`、`continue` 和 `return` 是控制流提前退出。`defer` 注册 scope exit 时执行的 call expression，并参与
move/borrow/cleanup 检查。

```text
BreakStmt    = "break" ";"
ContinueStmt = "continue" ";"
ReturnStmt   = "return" Expr? ";"
DeferStmt    = "defer" CallExpr ";"
```

```aurex
break;
continue;
return;
return value;
defer cleanup();
```

`defer` 后面必须是 call expression。

### 7.9 Unsafe block

Unsafe block 在局部范围内打开 unsafe context。它不会关闭类型检查、borrow summary 或 move analysis；它只允许执行明确标记为 unsafe 的操作。

```text
UnsafeBlock = "unsafe" Block
```

```aurex
unsafe {
    *ptr = *ptr + 1;
}

let value = unsafe {
    *ptr
};
```

unsafe block 可作为语句，也可作为带 tail expression 的表达式。

## 8. 表达式

表达式产生值或 place。当前表达式系统支持字面量、名字、函数调用、方法调用、字段/索引/slice、aggregate literal、
block/if/match/try、一元和二元操作、泛型调用以及语言内建。

```text
Expr = IfExpr
     | MatchExpr
     | BinaryExpr
     | UnaryExpr
     | PostfixExpr
     | PrimaryExpr
     | TryExpr
```

### 8.1 字面量、名字、调用

名字表达式按当前作用域和可见模块解析。函数名可以作为非捕获函数指针值传递。

```text
CallExpr = Expr "(" (Expr ("," Expr)* ","?)? ")"
NameExpr = Identifier ("." Identifier)*
```

```aurex
let a = 1;
let b = add(a, 2);
let c = module_name.function_name(3);
```

函数名可作为非捕获函数指针值：

```aurex
type Op = fn(i32) -> i32;
let op: Op = add_one;
let value = op(41);
```

### 8.2 泛型调用

泛型调用在 callee 后写显式类型实参。当前没有 turbofish 或 `<T>` 风格调用。

```text
GenericApply = Expr "[" Type ("," Type)* ","? "]"
```

```aurex
fn id[T](value: T) -> T {
    return value;
}

let value = id[i32](1);
```

显式泛型调用使用 `name[T](...)` 或 `module.name[T](...)`。`name::[T](...)` 不支持。

### 8.3 字段、方法、索引、slice

`.` 同时用于模块成员、字段、方法和 enum case 选择；`[]` 用于索引和 slice range。

```text
FieldExpr = Expr "." Identifier
IndexExpr = Expr "[" Expr "]"
SliceExpr = Expr "[" Expr? ":" Expr? "]"
```

```aurex
point.x
counter.add(1)
values[index]
values[start:end]
values[:end]
values[start:]
values[:]
text[start:end]
```

规则：

- array/slice 索引返回元素。
- raw pointer 字段/索引投影要求 unsafe context。
- `str` 不支持 `text[i]` 单字节索引；使用 `text[l:r]` 做 checked byte range slicing。
- `str` slice 按 byte offset 检查边界和 UTF-8 code point boundary；失败返回空 `str`。

### 8.4 Array、tuple、struct literal

Aggregate literal 构造结构化值。struct literal 使用字段名，array literal 可以列举元素或使用 repeat 形式。

```text
ArrayLiteral  = "[" Expr ("," Expr)* ","? "]" | "[" Expr ";" Expr "]"
TupleLiteral  = "(" Expr "," ")" | "(" Expr "," Expr ("," Expr)* ","? ")"
StructLiteral = Type "{" FieldInit ("," FieldInit)* ","? "}"
FieldInit     = Identifier ":" Expr
```

```aurex
let values = [1, 2, 3];
let zeroes: [4]u8 = [0; 4];
let pair = (1, true);
let single = (1,);
let point = Point { x: 1, y: 2 };
let box = Box[i32] { value: 42 };
```

### 8.5 Block expression

Block 可以作为表达式。最后一个没有分号的表达式是 block value；普通语句不产生 block value。

```text
Block = "{" Stmt* Expr? "}"
```

```aurex
let value = {
    let base = 40;
    base + 2
};
```

最后一个无分号表达式是 block 的值。空 block 或只有语句的 block 在需要值的位置会被拒绝。

### 8.6 If expression

Expression-form `if` 必须覆盖 false 分支，并且两个分支必须能形成兼容结果类型。

```text
IfExpr = "if" Expr ("is" Pattern)? Block "else" (IfExpr | Block)
```

```aurex
let sign = if value < 0 {
    -1
} else {
    1
};
```

expression-form `if` 必须有 `else`，两个分支类型必须兼容。

### 8.7 Match expression

`match` 根据 pattern 选择 arm。当前实现支持 enum、标量 literal、tuple、struct、array/slice pattern，
并做已实现范围内的穷尽性和 unreachable arm 检查。

```text
MatchExpr = "match" Expr "{" MatchArm* "}"
MatchArm  = Pattern ("if" Expr)? "=>" Expr ","
```

```aurex
let score = match choice {
    .yes => 1,
    .no => 0,
};
```

match 支持 enum、bool/integer literal、tuple/struct/array/slice 等 pattern。语义分析会做一定范围的 exhaustiveness 和 unreachable arm 检查。

### 8.8 Try expression

`?` 是 enum shape 驱动的 early return 表达式。它不依赖 enum 名字，只依赖 result-like 或 option-like 结构。

```text
TryExpr = Expr "?"
```

```aurex
let parsed = parse_digit(value)?;
```

`?` 只对结构上识别为 result-like 或 option-like 的 enum 生效：

- result-like：恰好有 `ok(payload)` 和 `err(payload)`。
- option-like：恰好有 `some(payload)` 和 payload-free `none`。

enum 名字不特殊，形状才重要。

## 9. 操作符

操作符语法由 parser 的 precedence 表定义。当前没有用户自定义操作符，也没有操作符重载语法；trait 能力只用于已实现的 builtin capability 和 trait predicate 检查。

常用二元操作：

```text
* / %
+ -
<< >>
< <= > >=
== !=
&
^
|
&&
||
```

常用一元操作：

```text
-x
!flag
~bits
&place
&mut place
*ptr_or_ref
```

规则要点：

- 算术和位运算要求相应数值类型。
- 逻辑运算要求 `bool`。
- `&place` 只产生 safe reference；不会因为 expected type 是 raw pointer 而退化为 `*const T` / `*mut T`。
- `*raw_pointer` 要求 unsafe；`*reference` 是 safe projection。
- 不支持 `++` / `--`。

## 10. 语言内建

本章列出的名字是语言内建，不是普通模块函数。它们由 parser、semantic analysis、IR 和 backend 共同识别，因此可以使用类型实参形式如
`sizeof[T]`、`ptrat[*mut i32](addr)`。

| 写法 | 安全性 | 结果 | 说明 |
| --- | --- | --- | --- |
| `cast[T](x)` | safe | `T` | 数值/布尔转换。支持整数、浮点、bool 之间转换。 |
| `ptrcast[T](p)` | unsafe | `T` | raw pointer / reference 到 raw pointer 的指针转换，`T` 必须是 pointer。 |
| `bitcast[T](x)` | unsafe | `T` | 等 ABI size 的 bit reinterpret。标量对标量、pointer 对 pointer。 |
| `sizeof[T]` | safe | `usize` | 查询有效 storage type 的 ABI size。 |
| `alignof[T]` | safe | `usize` | 查询有效 storage type 的 ABI alignment。 |
| `ptraddr(p_or_ref)` | safe | `usize` | 读取 raw pointer 或 reference 的地址值。 |
| `ptrat[T](addr)` | unsafe | `T` | 从整数地址构造 raw pointer，`T` 必须是 pointer。 |
| `sliceptr(slice)` | safe | `*const T` 或 `*mut T` | 返回 slice 底层 data pointer；`[]const T` 返回 `*const T`，`[]mut T` 返回 `*mut T`。 |
| `slicelen(slice)` | safe | `usize` | 返回 slice element count。 |
| `strptr(s)` | safe | `*const u8` | 返回 `str` 底层 byte pointer。 |
| `strblen(s)` | safe | `usize` | 返回 `str` byte length。 |
| `strvalid(bytes)` | safe | `bool` | 检查 `[]const u8` 或 `[]mut u8` 是否是有效 UTF-8。 |
| `strfromutf8(bytes)` | safe | `str` | 成功时借用 byte slice 作为 `str`；失败返回空 `str`。 |
| `strraw(data, len)` | unsafe | `str` | 不检查 UTF-8，从 `*const u8` 和 length 构造 `str`。 |

示例：

```aurex
fn from_bytes(bytes: []const u8) -> str {
    if !strvalid(bytes) {
        return "";
    }
    return strfromutf8(bytes);
}

fn raw_roundtrip(text_value: str) -> str {
    return unsafe { strraw(strptr(text_value), strblen(text_value)) };
}

fn first_byte(bytes: []const u8) -> u8 {
    if slicelen(bytes) == 0usize {
        return 0u8;
    }
    let data: *const u8 = sliceptr(bytes);
    return unsafe { data[0usize] };
}

fn pointer_roundtrip(value: &mut i32) -> *mut i32 {
    let address: usize = ptraddr(value);
    return unsafe { ptrat[*mut i32](address) };
}
```

注意：

- `strfromutf8` 失败返回空 `str`，所以需要区分“合法空输入”和“非法输入”时先调用 `strvalid`。
- `strraw` 不做 UTF-8 检查，必须包在 `unsafe` 内。
- `sliceptr` / `slicelen` 要求参数是 slice value，不接受 array、str、pointer 或普通 scalar；array 需要先通过 `values[:]` 形成 slice。
- `sizeof[void]`、`sizeof[opaque]`、`sizeof` 非 storage type 会被拒绝。
- `ptraddr(1)` 会被拒绝，因为参数不是 pointer/reference。

## 11. Pattern

Pattern 用于 `let`/`var` 绑定、let-else、`while value is pattern`、`if value is pattern` 和 `match` arm。
Pattern 可以绑定名字、匹配 literal/const、解构 tuple/struct/slice 和匹配 enum case。

```text
Pattern = OrPattern
OrPattern = PatternAtom ("|" PatternAtom)*
PatternAtom = "_"
            | Identifier
            | Literal
            | "const" Identifier
            | "." Identifier PayloadPattern?
            | Type "." Identifier PayloadPattern?
            | TuplePattern
            | StructPattern
            | SlicePattern
PayloadPattern = "(" Pattern ("," Pattern)* ","? ")"
TuplePattern   = "(" Pattern "," ")" | "(" Pattern "," Pattern ("," Pattern)* ","? ")"
StructPattern  = Type "{" StructPatternField ("," StructPatternField)* ","? "}"
StructPatternField = Identifier | Identifier ":" Pattern
SlicePattern   = "[" SlicePatternElement ("," SlicePatternElement)* ","? "]"
SlicePatternElement = Pattern | ".."
```

### 11.1 基础 pattern

基础 pattern 包括 wildcard、binding、literal 和 const pattern。

```aurex
_
name
42
true
false
const ANSWER
```

规则：

- 裸 identifier pattern 永远是绑定，不是 enum case。
- 常量 pattern 使用 `const NAME`，当前主要用于整数和 bool 常量。

### 11.2 Enum case pattern

Enum case pattern 可以省略 enum 类型前缀，前提是被匹配值的 enum 类型已知。

```aurex
match option {
    .some(value) => value,
    .none => 0,
};

match option {
    OptionI32.some(value) => value,
    OptionI32.none => 0,
};
```

规则：

- 已知被匹配 enum 类型时可用 `.case`。
- 显式写法是 `Type.case` 或 `Type[Args].case`。
- 不支持裸 `some(value)`。

### 11.3 Tuple、struct、slice pattern

解构 pattern 会在语义阶段校验被匹配类型、字段存在性、payload arity 和绑定类型。

```aurex
let (left, right) = pair;

match point {
    Point { x, y } => x + y,
};

match values {
    [head, .., tail] => head + tail,
    [single] => single,
    _ => 0,
};
```

规则：

- struct pattern 支持字段 shorthand 和 `field: pattern`。
- slice pattern 最多一个 `..`。
- 动态 slice 的 match 通常需要 `_` 或 `[..]` 这类覆盖剩余长度的 arm。

### 11.4 Or-pattern 和 guard

Or-pattern 合并多个 pattern 分支；guard 是 arm 级别的额外布尔条件。

```aurex
match value {
    1 | 2 => true,
    _ => false,
};

match packet {
    .int(value) if value > 0 => value,
    .int(value) => -value,
    .none => 0,
};
```

Or-pattern 每个分支若绑定名字，必须绑定相同名字和类型。

## 12. 泛型、trait 和 where

泛型系统支持类型参数、origin 参数、泛型函数、泛型类型、泛型 impl、method-local impl 泛型和 `where` 子句。
Trait 是 nominal static trait，不是 structural interface，也不是 trait object。

```text
GenericParams = "[" GenericParam ("," GenericParam)* ","? "]"
GenericParam  = Identifier | "origin" Identifier
WhereClause   = "where" WherePredicate ("," WherePredicate)* ","?
WherePredicate = Identifier ":" Capability ("+" Capability)*
Capability    = Identifier AssociatedTypeEquality?
AssociatedTypeEquality = "[" Identifier "=" Type "]"
TraitDecl     = Visibility? "trait" Identifier GenericParams? WhereClause? "{" TraitItem* "}"
TraitItem     = TypeAlias | FunctionDecl
TraitImpl     = "impl" GenericParams? Type "for" Type WhereClause? "{" ImplItem* "}"
```

```aurex
fn id[T](value: T) -> T {
    return value;
}

struct Box[T] {
    value: T;
}

enum Option[T] {
    some(T),
    none,
}

impl[T] Box[T] {
    fn get(self: &Box[T]) -> T {
        return self.value;
    }

    fn echo[U](self: &Box[T], value: U) -> U {
        return value;
    }
}

fn view[origin data](value: &[data] i32) -> &[data] i32 {
    return value;
}

fn pick[origin left, origin right](
    left: &[left] i32,
    right: &[right] i32,
    choose_left: bool,
) -> &[left | right] i32 {
    return if choose_left { left } else { right };
}
```

泛型参数列表使用 `[]`，可以混合普通类型参数和上下文 `origin` 参数：

```text
GenericParams = "[" GenericParam ("," GenericParam)* ","? "]"
GenericParam  = Identifier | "origin" Identifier
```

规则：

- 普通 `Identifier` 声明类型参数。
- `origin Identifier` 声明 lifetime/origin 参数，只能用于 reference origin qualifier，例如 `&[data] T`。
- `origin` 是上下文标记，不是全局关键字。
- 泛型参数列表不能为空；`fn f[]`、`struct Box[]`、`Box[]` 和 `id[](...)` 都会被拒绝。
- inline bound 不支持；`fn f[T: Copy](...)` 会被拒绝，约束必须写在 `where` 子句。

`where` 当前支持 compiler-owned builtin capability 和 nominal static trait predicate：

```aurex
fn same[T](left: T, right: T) -> bool where T: Eq {
    return left == right;
}

fn duplicate[T](value: T) -> (T, T) where T: Copy {
    return (value, value);
}
```

支持：

- `Sized`
- `Eq`
- `Ord`
- `Hash`
- `Copy`
- 用户定义 `trait`
- 显式 `impl Trait for Type`
- trait method requirement
- trait default method body
- associated type requirement
- `Trait[Item = Type]` associated-type equality

示例：

```aurex
trait Source {
    type Item;
    fn get(self: &Self) -> Self.Item;

    fn fallback(self: &Self, value: Self.Item) -> Self.Item {
        return value;
    }
}

struct Bytes {
    value: i32;
}

impl Source for Bytes {
    type Item = i32;

    fn get(self: &Bytes) -> i32 {
        return self.value;
    }
}

fn read_i32[T](value: &T) -> i32 where T: Source[Item = i32] {
    return value.fallback(value.get());
}
```

Trait 规则：

- trait 是 nominal identity，不按方法形状做 structural conformance。
- conformance 必须显式写成 `impl Trait for Type`。
- trait method 默认静态分派；显式 override lowering 到具体 impl method direct call，inherited default
  lowering 到 concrete trait-owned default method instance direct call。
- 带 body 的 trait method requirement 可以被 impl 省略；不带 body 的 requirement 仍必须实现。
- 显式 override 必须在 `Self`、trait args 和 associated type output 替换后匹配 requirement 签名。
- associated type 是 impl output，不作为 impl selection input。
- `Self.Item` shorthand 只在当前 trait bounds 中 associated type 名称唯一或 equality 可归一化时使用。
- `Copy` 是 compiler-owned capability，用资源语义分类判定；标量、raw pointer、reference、slice、函数指针和只含
  `Copy` 组件的结构化类型可满足。generic parameter 只有显式 `where T: Copy` 后才按 `Copy` 使用。
- `Drop` 是 reserved destructor surface，不是普通用户 trait。可以为具体 named struct/enum/opaque struct 写窄
  `impl Drop for T` destructor；`where T: Drop`、`trait Drop { ... }` 和 generic Drop bound 仍会被诊断为资源 capability 暂不支持。

暂不支持：

- const generic
- trait object
- 用户 `Drop` bound、generic Drop impl、trait-object Drop dispatch、dynamic cleanup runtime ABI call 和 async/unwind-aware drop
- generic associated type
- associated const
- specialization
- `<T>` 风格泛型

## 13. Borrow、lifetime 和资源语义

本章描述当前已经生效的 ownership/borrow/lifetime 规则。Aurex 现在有可用的 safe reference 和本地 loan checking，
但用户表面仍比 Rust lifetime 语法窄；显式来源主要通过 origin-qualified reference 和 `@borrow` contract 表达。

当前 Aurex 已经有 safe reference、borrowed slice、`str` borrowed view、函数级 borrow summary、显式
`@borrow` contract、显式 origin-qualified reference type、region facts、local loan checker 和 whole-local move
analysis。它们是当前语言的一部分，但仍保持比 Rust 更窄的用户表面。

### 13.1 Borrowed view 类型

下列类型可以携带 borrow/lifetime 来源：

- `&T` / `&mut T`
- `&[origin] T` / `&mut [origin] T`
- `[]const T` / `[]mut T`
- `str`
- 包含上述类型的 struct、enum、tuple、array、函数返回类型或 associated projection

示例：

```aurex
struct View[origin data] {
    item: &[data] i32;
}

@borrow(return = [value])
fn identity(value: &i32) -> &i32 {
    return value;
}
```

函数体有定义时，编译器会收集 returned borrow origins。原型、extern C、trait requirement
或 public borrowed-return 边界若无法从签名唯一推导来源，需要用显式 origin 或 `@borrow(return = [...])`
把 contract 写清楚。

### 13.2 Local loan checking

编译器会按 CFG 收集 borrow action，并检查活跃 shared/mutable loan 与后续 read/write/move/drop/reinit/cleanup
的冲突。

```aurex
fn bad[T](value: T) -> void {
    let borrowed: &T = &value;
    let consumed: T = value;  // move
    inspect(borrowed);        // 诊断：borrowed carrier 在 move 后仍被使用
}
```

当前 checker 支持：

- shared borrow、mutable borrow、reborrow 和 two-phase mutable receiver auto-borrow facts。
- projection-aware conflict：同一 place 或 prefix place 冲突；已知 disjoint struct field 可分开处理。
- local/reinit/drop/cleanup invalidation；`defer` 和隐式 cleanup 都进入检查。
- borrowed local/temporary escape 诊断；从 `strraw` raw pointer path 派生且能追踪到本地存储的 return 也会诊断。
- 函数调用会使用 callee borrow summary 或 `@borrow` contract；函数值调用和未知外部来源保守记录 unknown。

仍不支持或仍保守处理：

- 完整 Rust-style lifetime surface 和 apostrophe lifetime 语法。
- safe proof for raw pointer aliasing。
- trait-object Drop dispatch、dynamic cleanup runtime ABI call、async/unwind-aware drop 和完整标准库级 RAII runtime integration。
- indexed move-out、array/slice/index 精确 disjoint proof、consuming pattern payload 和 non-`Copy` `?` payload transfer。
- borrowed/reference field resource overwrite，例如通过 `&mut Box[T]` 执行 `box.value = replacement`，当前仍因缺少本地字段 drop flag proof 而拒绝。
- `replace` / `take` / `swap` 还没有语言内建或标准库 intrinsic。
- full Polonius Datalog、`dyn Trait`、async/generator borrow。

### 13.3 当前借用权系统实现水平

当前 Aurex 的借用权系统是一个窄表面、CFG-sensitive、safe-borrow-only 的语义分析系统。它已经接入函数体
semantic analysis 主路径，并以 enforced diagnostic 方式报告错误；它不是只为 IDE 或 checked dump 记录 facts。

实现分层如下：

1. Typed expression 和 statement analysis 先确定类型、可写 place、调用绑定、receiver auto-borrow 和 `OwnedUseMode`。
2. Move analysis 按 whole-local 数据流判定 `Copy` 读取、非 `Copy` consume、move 后使用和重新初始化。
3. Body flow graph 把函数体展开成 CFG point、edge、place 和 action。action 包括 read、write、reinit、
   move candidate、shared borrow、mutable borrow、call returned borrow、two-phase receiver reservation /
   activation、return、cleanup scope 和 cleanup storage。
4. Body loan checker 从 borrow action 生成 `shared` / `mutable` loan，计算 borrowed carrier 的 CFG last-use
   liveness，并检查 active loan 与后续访问的冲突。
5. Borrow summary 和 `@borrow` contract 把函数返回 borrowed view 的来源传递到调用方；调用方会把 returned
   borrow 重新转成本地 loan。
6. Lifetime analyzer 收集 origin/region、return region、storage escape、type-outlives 和 live-range facts，并对
   return 来源、歧义 elision、本地逃逸和 type outlives 约束发诊断。
7. Drop check 使用 body loan conflict、drop-glue plan 和 lifetime live ranges 检查 cleanup、overwrite、early
   exit 和 dropck outlives 交互。
8. Place-state facts 记录 local/field projection 的 initialized/moved/dropped 状态，支持本地 owned struct 字段
   partial move、字段 reinit 和字段级 cleanup/drop flag。
9. M7d-C RAII analyzer 在声明阶段识别 reserved `impl Drop for T`，把合法析构器写入 `CheckedModule::destructors`，
   并把对应函数签名标记为 `is_destructor`。
10. M7d-G cleanup marker ABI policy 把 IR `drop` / `drop_if` marker 分成 structural/static custom destructor、
    generic marker-only、associated projection marker-only、opaque marker-only 和 unknown marker-only 等策略；
    verifier、dump、clone/copy 和 fingerprint 都消费该 policy。
11. M7d-I move rejection facts 把当前仍拒绝的 pattern payload、try payload 和 indexed element move 记录为
    checked facts，用于 query authority、checked dump 和 IDE/tooling，而不是放宽这些语法。
12. M7d-J cleanup marker facts 把 IR cleanup marker policy 稳定投影为 query DTO，并接入 lower-IR query
    result、incremental cache、IDE semantic fact、hover、workspace index 和 session reuse。它是工具链消费面，
    不代表新增用户语法或 runtime destructor ABI。

当前已经强制检查的核心规则：

- `&place` 创建 shared loan；`&mut place` 创建 mutable loan。
- shared loan 活跃时允许读取和再次 shared borrow；禁止写入、reinit、move、drop、mutable borrow 和 cleanup。
- mutable loan 活跃时按独占借用处理，会与后续 read/write/reinit/move/drop/shared borrow/mutable borrow/cleanup 冲突。
- 短借用按 borrowed carrier 的 CFG last-use 结束；最后一次使用之后再写原 place 可以通过。
- move candidate 只有在对应表达式的 `OwnedUseMode` 是 `owned_consume` 时才作为 move invalidation；普通
  `Copy` 读取不会被误当成 move。
- 函数调用返回 borrowed view 时，编译器会使用 callee borrow summary 或 `@borrow` contract，在调用点为返回值
  发出本地 loan。
- extern/prototype/unknown borrowed return 会走 conservative unknown path：返回值本身记录 unknown loan，并把可疑参数来源保守纳入检查。
- slice、`strfromutf8` 和包含 reference/slice/`str` 的 struct、enum、tuple、array 会作为 borrow carrier 参与检查。
- `defer`、scope cleanup、return、reinit/overwrite 和 drop-glue 相关 action 会进入 borrow/dropck 交互。

当前 place 精度：

| Place 形态 | 当前处理 |
| --- | --- |
| 不同 local root | 不冲突 |
| 同一 local root | 冲突，除非已知 projection 不相交 |
| struct field | 已知不同字段可以分开处理 |
| tuple element | 已知不同数字元素可以分开处理，例如 `pair.0` 与 `pair.1` |
| prefix place | 冲突，例如借用 `value.field` 后写 `value` |
| array index | 保守冲突；不证明 `values[0]` 和 `values[1]` 不相交 |
| slice range | 保守冲突；不做区间 disjoint proof |
| dereference / reborrow | 有 parent/child loan；复杂 alias 仍保守 |
| qualified 或 unknown root | 保守处理 |
| raw pointer | 不进入 safe borrow proof |

当前 M7d-B/M7d-F/M7d-H/M7d-I/M7d-J 已支持的 place-level resource 子集：

- 从本地 owned struct 的非 `Copy` 字段 move-out，例如 `let moved: T = current.left;`。
- move-out 后同一字段处于 uninitialized/moved 状态，继续使用该字段或读取整个父 struct 会按 place-state 诊断。
- 对同一本地 owned struct 字段重新赋值会作为 `reinit`，例如 `current.left = replacement;`；reinit 后父 struct 可以再次整体返回或 cleanup。
- 对包含 droppable 字段的 struct local，body-flow cleanup 和 IR lowering 会拆出字段级 cleanup/drop flag，例如只对仍 initialized 的字段执行 `drop_if`。
- 从本地 owned tuple 的非 `Copy` 元素 move-out，例如 `let moved: T = current.0;`。
- 对同一本地 owned tuple 元素重新赋值会作为 `reinit`，例如 `current.0 = replacement;`；reinit 后父 tuple 可以再次整体返回或 cleanup。
- 对包含 droppable 元素的 tuple local，body-flow cleanup 和 IR lowering 会拆出元素级 cleanup/drop flag，例如只对仍 initialized 的 tuple 元素执行 `drop_if`。
- 泛型模板 body 会使用 generic side table 类型信息，因此 `Box[T]` 这类模板内字段 cleanup 能看到字段类型和 drop 需求。
- 调用返回 owned generic `T` 时，borrow summary 的 origin dependency 仍保留给 borrow/lifetime checker，但 place-state 不再把它误当成对已 moved 实参的借用访问。
- index/slice place-state 采用 same-root conservative identity：同一 array/slice root 的不同 index 或 slice
  projection 不会被 AST expr id 误拆成互不相关的 storage。
- IR cleanup marker 会携带 cleanup ABI policy，lower-IR query/cache 和 tooling 能看到 marker 数量、kind、
  target type 和 policy；generic/associated/opaque/unknown cleanup 当前仍只是 marker-only facts。

当前仍不属于 M7d-B/M7d-F/M7d-H/M7d-I/M7d-J 已支持范围：

- indexed move-out 仍拒绝；array/slice/index place 也继续保守冲突，不证明不同 index 不相交。
- consuming pattern payload、non-`Copy` `?` payload transfer 仍拒绝。
- borrowed/reference 字段覆盖仍拒绝，例如 `fn overwrite[T](box: &mut Box[T], replacement: T) { box.value = replacement; }`。
- `replace` / `take` / `swap` 没有可调用的 compiler-known primitive 或标准库入口。

M7d-I 增加的是拒绝事实链，不是新语法接受能力。对 non-`Copy` resource 执行 match arm payload binding、
struct pattern destructuring、`if value is .case(payload)` / `while value is .case(payload)`、`value?` payload
transfer 或 `values[i]` indexed move-out 时，当前仍会报 unsupported；同时编译器会把这类拒绝记录到
`move_rejection_facts`，供 checked dump、query authority 和 IDE/tooling 使用。

M7d-J 增加的是 cleanup marker facts 的 query/tooling 消费面，也不是新语法接受能力。`drop` / `drop_if`
marker 上的 cleanup ABI policy 会进入 stable DTO、lower-IR query result、driver incremental cache、IDE hover、
semantic facts、workspace index 和 reuse plan；但 generic / associated / opaque / unknown cleanup 仍不生成未知
runtime destructor call，标准库资源 API 也仍不在本阶段实现。

当前语法特性仍需完善的重点：

- consuming pattern payload 的真实转移语义：需要定义 enum/struct/slice pattern payload move 后原 place 的 drop
  flag、partial initialization、rollback 和 borrow invalidation 规则。
- non-`Copy` `?` payload transfer：需要区分当前 operand type 与 ok/some payload type，明确 failure branch
  cleanup、success branch payload ownership 和泛型 Result-like 类型的事实表达。
- indexed move-out：需要 array/slice/index 的 per-element ownership、hole policy、不同 index/range disjoint proof
  和 cleanup/drop flag 语义。
- borrowed/reference 字段 overwrite：需要把 `&mut Box[T]` 这类投影写入与 active loan、dropck 和 place-state
  统一起来，避免通过借用别名破坏拥有值生命周期。
- replace/take/swap primitive：需要 compiler-known primitive 或后续标准库 intrinsic，但当前阶段明确不实现标准库。
- array repeat resource rollback：需要 `[expr; N]` 对 droppable/non-`Copy` 元素的初始化次数、失败路径和 cleanup
  精确事实。
- closure/lambda capture、async/generator borrow、HRTB/full variance、`dyn Trait` object lifetime bounds、dynamic
  Drop dispatch、marker-only cleanup 的 runtime ABI call 和 unsafe/raw pointer alias proof 仍是后续阶段，不属于当前 M7d-J。
- 标准库资源 wrapper、拥有型 `String`/容器、`replace`/`take`/`swap` 标准库入口和任何标准库 Drop helper 都故意
  不在当前阶段实现。

当前 reborrow 和 two-phase 水平：

- 对局部 borrowed carrier 的 reborrow 有 parent/child loan 模型。
- child loan 活跃时使用 mutable parent carrier 会产生 reborrow parent-use conflict。
- mutable receiver auto-borrow 有 two-phase reservation/activation；reservation 期间允许不破坏 receiver place 的读取，
  activation 时执行 mutable borrow 冲突检查。
- 这不是完整 Rust reborrow/lifetime subtyping；raw pointer reborrow、closure/generator 捕获和复杂跨函数 reborrow
  proof 仍不属于当前能力。

当前 lifetime/region 水平：

- 支持 `origin name` 参数、origin-qualified reference、`@borrow(return = [...])` 和函数体 inferred summary。
- 支持 parameter、self、static、explicit origin、local、temporary、unknown 等 region facts。
- 支持 return origin subset、ambiguous elision、local/temporary escape 和 type-outlives diagnostics。
- 当前求解不是 full Polonius Datalog；没有完整 origin subset closure、HRTB、variance 或 Rust-style apostrophe lifetime surface。

当前 drop/cleanup 水平：

- cleanup storage、reinit/overwrite、return early-exit 和 defer cleanup 都可以形成 dropck action。
- body loan checker 已经报告的 drop/reinit/cleanup conflict 会进入 dropck facts。
- dropck 会根据 active lifetime regions 为 drop-glue 相关类型生成 `T: outlives(region)` 约束。
- `impl Drop for T` 已作为 reserved user surface 接入 checked destructor facts、resource semantics、drop-glue/dropck、
  query authority、checked dump 和 IDE hover。
- lowerer 已把静态可解析 custom destructor 降低为普通 direct `call`；cleanup exit、overwrite、early-exit 和
  drop flag guarded path 会触发真实 destructor call，LLVM backend 复用普通 call emission 发出调用。
- `drop` / `drop_if` 在 IR 中仍是 target-independent cleanup marker，LLVM backend 对 marker 本身仍是 no-op；
  当前 runtime 副作用来自 lowering 旁路生成的 direct call。
- 带 custom destructor 且含 droppable 字段的 struct local 会先执行根 custom destructor，再执行字段 cleanup；
  字段 partial move/reinit 通过根 flag 和字段 flag 协同，避免对部分 moved 对象调用根 destructor。
- aggregate literal / tuple literal / array literal / 多 payload enum synthetic payload 构造现在会在需要时使用临时
  aggregate slot 和 rollback drop flag；后续元素表达式提前终止时，只 cleanup 已经求值并写入成功的 droppable
  元素。constant/global initializer、无 droppable 元素的 aggregate 和 scalar aggregate 不走该 staged 路径。
- `drop` / `drop_if` marker 现在携带 `CleanupAbiPolicy`，并通过 cleanup marker facts 进入 lower-IR query/cache
  和 IDE/tooling；这只是 compiler facts，不是 runtime ABI call。
- generic、associated projection、opaque 和 unknown cleanup marker 当前仍保持 marker-only；用户可写 `Drop`
  bound、generic Drop impl、trait-object Drop dispatch、dynamic cleanup ABI call 和 async/unwind-aware drop
  仍不是当前能力。

因此，Aurex 当前的借用权系统可描述为：已经可用的 CFG-sensitive local loan checker，带跨函数 borrowed-return
summary/contract 传播，并与 whole-local move analysis、struct 字段 place-state、cleanup、Drop destructor facts 和 dropck facts 协同；
它还不是完整 Rust borrow checker 复刻，也不提供 raw pointer alias safe proof、indexed move-out、完整
lifetime generics、dynamic Drop dispatch 或 unwind-aware RAII。

### 13.4 RAII Drop surface

M7d-C 开放一个窄的、compiler-owned destructor surface：

```aurex
struct File {
    fd: i32;
}

impl Drop for File {
    fn drop(self: deinit File) -> void {
        close_fd(self.fd);
    }
}
```

语法和语义规则：

- `Drop` 不是普通 trait 名称。`trait Drop {}` 会被拒绝；普通 trait impl 校验也会跳过合法的 destructor impl。
- Drop impl 必须写成未限定、无类型实参的 `impl Drop for T`；`impl pkg.Drop for T` 和 `impl Drop[i32] for T`
  都会被拒绝。
- 目标 `T` 必须是 named `struct`、`enum` 或 `opaque struct`。不能为 `i32`、pointer、reference、tuple、array、slice
  或泛型参数直接实现 Drop。
- 当前不支持 generic Drop impl、`where` 约束、associated type、额外方法、method-level generic、prototype、
  `extern c`、`export c`、variadic、unsafe Drop method 或 `@borrow(...)` contract。
- impl 中必须且只能有一个方法，名字必须是 `drop`。
- 方法签名必须精确为 `fn drop(self: deinit T) -> void { ... }`。
- `self` 必须是第一个且唯一参数，参数名必须是 `self`，类型必须是 impl 目标类型本身，不能是 `*mut T`、`*const T`、
  `&T` 或 `&mut T`。
- `deinit` 是参数冒号后的上下文修饰符，只在当前 Drop self surface 中有语义；它不是全局关键字。

当前实现会记录：

- `CheckedModule::destructors` 中的 `DestructorInfo`，包含 impl item、method item、self type、function key 和稳定 fingerprint。
- 对应 `FunctionSignature::is_destructor = true`。
- resource classifier 把带 custom destructor 的类型视为 `MoveOnly/Discard/NeedsDrop/OwnedValue`。
- drop-glue plan 会先生成 `custom_destructor` step，然后继续展开结构字段、tuple 元素、array 元素、enum payload、
  generic value 或 opaque value。
- dropck action 对 custom destructor 使用 destructor info fingerprint 作为 destructor key，并在 drop fact 中记录
  destructor function key。
- `TypeCheckBodyAuthority` 混入 destructor count/fingerprint；IDE hover 对该类型显示 `destructor=custom`。

当前重要限制：

- `Drop` 仍不是用户可写 bound；`where T: Drop` 仍拒绝。
- 静态可解析 custom destructor 已会在 cleanup lowering 中发出 direct call；generic/associated/opaque/unknown
  cleanup marker policy 已正式化为 marker-only facts，但 runtime destructor ABI call 仍未实现。
- 没有 drop dispatch、trait object destructor、async drop、panic/unwind cleanup model 或 `may_dangle` 类 unsafe escape hatch。
- 标准库级 RAII 资源封装不属于当前阶段；外部资源仍应通过显式 FFI/API 管理。

### 13.5 Move、Copy 和 cleanup

资源语义按 compiler-owned summary 分类：

- `Copy`：可重复读取或按值复制。
- `MoveOnly`：按值使用会 consume 原位置，之后使用原位置会诊断，除非重新初始化。
- `Discard` / `MustConsume`：当前用户表面主要暴露 discard；must-consume 仍是后续方向。
- `Trivial` / `NeedsDrop`：用于 compiler cleanup/drop-glue 规划。
- `OwnedValue` / `BorrowedView` / `RawPointer`：区分拥有值、借用视图和 raw pointer。

示例：

```aurex
fn duplicate[T](value: T) -> (T, T) where T: Copy {
    return (value, value);
}

fn reinitialize[T](value: T, replacement: T) -> T {
    var current: T = value;
    let consumed: T = current;
    current = replacement;
    return current;
}
```

`defer` 与 compiler cleanup 交错执行；`return`、`break`、`continue` 和 `?` early return 都会触发相应 cleanup
路径。当前静态可解析 `impl Drop` 会在 cleanup lowering 中变成真实 direct call；`drop` / `drop_if` marker
本身仍只是 target-independent cleanup marker。

本地 owned struct 字段可以被单独 move-out 并重新初始化：

```aurex
struct Box[T] {
    left: T;
    right: T;
}

fn replace_left[T](box: Box[T], replacement: T) -> Box[T] {
    var current: Box[T] = box;
    let moved: T = current.left;
    current.left = replacement;
    return current;
}
```

这项能力目前只覆盖 named struct field projection。`current.left` move 后，`current.right` 仍可 cleanup；`current`
整体在 `left` reinit 前不可按值读取或返回。通过 borrowed/reference base 覆盖资源字段仍拒绝，因为当前没有为外部 alias
下的旧字段值建立本地可证明 drop flag。

## 14. Unsafe 边界

Unsafe 是对特定操作的显式权限，不是关闭编译器检查的模式。Unsafe block 或 `unsafe fn` 允许 raw pointer
解引用、raw projection、unsafe builtin 和 unsafe function call，但类型、ABI、move、borrow 和 lifetime 检查仍继续运行。

需要 unsafe context 的操作：

- raw pointer 解引用：`*ptr`
- raw pointer 字段/索引投影：`ptr.field`、`ptr[index]`
- `ptrcast[T](p)`
- `bitcast[T](x)`
- `ptrat[T](addr)`
- `strraw(data, len)`
- 调用 `unsafe fn` 或 `unsafe fn(...) -> T` 函数值

不需要 unsafe 的操作：

- `&place`
- `&mut place`
- safe reference 解引用：`*reference`
- `ptraddr(pointer_or_reference)`
- `strptr(s)`
- `strblen(s)`
- `strvalid(bytes)`
- `strfromutf8(bytes)`
- `sizeof[T]`
- `alignof[T]`
- `cast[T](x)`

示例：

```aurex
unsafe fn read_raw(value: *const i32) -> i32 {
    return *value;
}

fn read(value: &i32) -> i32 {
    return *value;
}

fn call_raw(value: *const i32) -> i32 {
    return unsafe { read_raw(value) };
}
```

## 15. 当前明确不支持的语法

下列写法不是“还没写进本手册”，而是当前 parser 或 semantic analysis 会拒绝的语言表面。它们可以作为读者迁移代码时的快速排错矩阵。

```aurex
*i32                 // pointer 必须写 *const / *mut
[]i32                // slice 必须写 []const / []mut
[N]i32               // 数组长度不是标识符
[1 + 2]i32           // 数组长度不是 const expr
Box[]                // 泛型实参不能为空
Box<i32>             // 泛型不用 <>
()
let () = value;
foo::bar             // selector 不用 ::
id::[i32](1)
fn add[T: Add](a: T, b: T) -> T { return a; }
trait Reader { fn read[T](self: &Self, value: T) -> i32; }
where T: Drop
for value in values {}
text[0]              // str 不支持单 index
value++              // 不支持自增
let x = tuple.first;  // 匿名 tuple 命名字段访问拒绝；使用 tuple.0 / tuple.1
let x = values[i];    // 非 Copy indexed move-out 仍拒绝
let File { fd } = file; // 若 payload/field 是非 Copy resource，consuming pattern payload 仍拒绝
let ok: File = result?; // non-Copy ? payload transfer 仍拒绝
box.value = value;    // 若 box 是 &mut Box[T] 且字段为资源值，当前仍拒绝
replace(a, b)         // 当前没有 replace / take / swap 内建
```

## 16. 当前工程表达能力：正则库案例

本章不是正则语法规范，而是说明当前 Aurex 已经能承载的工程规模：多模块 facade、内部实现拆分、FFI 分配、unsafe
raw pointer、resource cleanup、match、泛型别名、method、函数指针和字符串内建。

仓库现在包含一个用当前 Aurex 语言写的多模块编译型正则库。它不是 `text`
子模块，而是独立的 `regex` 模块目录：

- 入口模块：`examples/libs/regex/api.ax`
- 内部目录：
  `regex/core` 放类型和 opcode，
  `regex/config` 放资源上限，
  `regex/syntax` 放 ASCII byte 语法工具，
  `regex/runtime` 放 FFI 分配，
  `regex/compile` 放 parser 和 NFA program 构造，
  `regex/vm` 放匹配引擎，
  `regex/ops` 放 find/captures iterator、replace 和 split 操作。
- 演示程序：`examples/regex_demo.ax`
- 第一阶段 API 程序：`examples/regex_phase1.ax`
- 工业语法面程序：`examples/regex_industrial.ax`
- 压力程序：`examples/regex_stress.ax`
- 详细语法/API/模块说明：`docs/zh/regex.md`

编译方式：

```sh
build/full-llvm/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
build/full-llvm/bin/aurexc -I examples/libs examples/regex_phase1.ax -o build/tests/regex_phase1
build/tests/regex_phase1
build/full-llvm/bin/aurexc -I examples/libs examples/regex_industrial.ax -o build/tests/regex_industrial
build/tests/regex_industrial
build/full-llvm/bin/aurexc -I examples/libs examples/regex_stress.ax -o build/tests/regex_stress
build/tests/regex_stress
```

最小调用示例：

```aurex
module regex_minimal;

import regex.api as regex;

fn main() -> i32 {
    if !regex.fullmatch("^(cat|dog)s?$", "dogs") {
        return 1;
    }
    if !regex.search("\\w+@\\w+\\.com", "mail: team@example.com;") {
        return 2;
    }

    var compiled: regex.Regex = regex.compile("(?<year>\\d{2,4})");
    defer regex.destroy(&mut compiled);
    if !regex.is_valid(&compiled) {
        return 3 + regex.status_code(compiled.status);
    }
    let result: regex.MatchResult = regex.fullmatch_compiled(&compiled, "2026");
    if !result.ok() {
        return 10 + regex.status_code(result.status);
    }
    var captures: regex.Captures = regex.captures_fullmatch_compiled(&compiled, "2026");
    defer regex.destroy_captures(&mut captures);
    if !captures.ok() || !regex.capture_at(&captures, regex.capture_index(&compiled, "year")).matched {
        return 20;
    }
    return 0;
}
```

这个库用来验证当前语言的实际工程表达能力：

- 模块和 import：`import regex.api as regex;`
- 多模块拆分：API facade、ASCII 常量、FFI 分配、程序表示、parser、VM engine、结果管理、操作层、类型定义。
- public/private API：外部只需要 `regex.api`，内部 helper 保持 private。
- type alias：`pub type Regex = t.Regex`、`pub type MatchResult = t.MatchResult`、`pub type Captures = t.Captures` 等。
- enum 和 struct：`RegexStatus`、`Regex`、`MatchResult`、`Captures`、`FindIter`、`SplitIter`、`State`、`ClassRange` 等。
- impl 方法：`Regex.valid`、`MatchResult.ok`、`Captures.ok`、`ReplaceResult.ok`。
- FFI 和 unsafe：通过 `calloc/free` 分配状态表、字符类表、捕获结果和 VM 工作区。
- raw pointer 字段/索引投影：编译程序和 VM 列表都用指针数组访问。
- `defer`：示例和 API convenience 函数用它保证释放。
- match：状态码和演示结果判定使用 enum pattern。
- 函数类型：演示中使用 `type Matcher = fn(str, str) -> bool`。
- `str` 内建：`strblen`、`strptr` 保留 UTF-8 byte offset；regex 自身按 Unicode scalar value 解析和匹配。
- 资源预算 API：`state_count`、`range_count`、`capture_count`、`program_bytes`、`workspace_bytes`。
- 第一阶段 API：`captures_compiled`、`find_iter`、`captures_iter`、`replace_all`、`split_iter`、`error_offset`。
- 工业安全语法面：inline flags/scoped flags、greedy/lazy/ungreedy、word boundary、absolute anchors、hex/unicode escape、quoted literal、POSIX/Unicode property class。
- 压力测试：`regex_stress.ax` 通过数百次重复匹配验证 compiled API 复用、内存预算和错误路径。

当前正则语法是 UTF-8 text regex 语法，详细定义见
`docs/zh/regex.md`。概要如下：

- 字面量：普通字面量按 Unicode scalar value 匹配。
- 转义：`\a`、`\e`、`\f`、`\n`、`\r`、`\t`、`\v`、`\0`、`\xNN`、`\x{...}`、`\uNNNN`、`\u{...}`、`\Q...\E`、`\N`、`\R`。
- 预定义类：`\d`、`\D`、`\w`、`\W`、`\s`、`\S` 采用 Unicode 17.0 属性语义。
- 通配：`.` 默认不匹配 Unicode line break，`(?s)` dotall 下匹配任意 scalar。
- 锚点：`^` / `$`，`(?m)` 下识别行首/行尾；`\A` / `\z` 匹配绝对开头/结尾。
- 边界：`\b` / `\B` 使用 Unicode word 定义。
- 字符类：`[abc]`、`[^abc]`、`[a-z]`、`[\u{0400}-\u{04ff}]`，类内支持 `\d`、`\D`、`\w`、`\W`、`\s`、`\S`、POSIX class 和 Unicode `\p{...}` / `\P{...}`。
- 分组：`(...)` 捕获，`(?:...)` 不捕获，`(?<name>...)` 命名捕获，`(?i:...)` scoped flags。
- 交替：`a|b`，支持在分组内使用。
- flags：`(?i)`、`(?m)`、`(?s)`、`(?x)`、`(?U)`、`(?u)`。
- 量词：`*`、`+`、`?`、`{m}`、`{m,n}`、`{m,}`，以及 `*?`、`+?`、`??`、`{m,n}?` lazy 形式。

当前正则库的有意边界：

- 不支持反向引用、lookaround、原子组、条件组、递归/子例程调用和带 closure 捕获的复杂替换回调；当前支持非捕获函数指针 callback。
- `unicode.ucd` 模块提供可复用 UTF-8 scalar 解码、Unicode 17.0 general category/binary/script property、simple/full case folding 和 Grapheme_Cluster_Break 支撑；当前 text regex 支持多 scalar full case fold 和显式 `\X` extended grapheme cluster atom。regex 另有 raw byte facade、exact-literal `RegexSet` 标量 Aho-Corasick fast path、database 和 stream API。
- 不支持 possessive 量词。
- `{m,n}` 展开上限是实现常量，超出返回 `RegexStatus.repeat_too_large`。
- pattern、program、capture 和 VM workspace 都有显式上限，超出分别返回 `pattern_too_large`、`program_too_large`、`capture_too_large` 或 `workspace_too_large`。
- 没有标准库 RAII，`compile` 得到的 `Regex` 必须手动 `destroy` 或 `defer destroy`；`Captures` 必须手动 `destroy_captures`。

## 17. 规范对应的测试入口

本手册中的语言面主要由 parser、sema、integration sample 和 examples 测试覆盖。下面列出和内建函数、正则库案例最直接相关的入口。

内建函数集中运行样例位于：

```text
tests/samples/positive/core/builtins.ax
```

它覆盖：

```text
cast ptrcast bitcast sizeof alignof ptraddr ptrat
sliceptr slicelen strptr strblen strvalid strfromutf8 strraw
```

集成测试会把该样例编译为可执行文件并运行：

```text
tests/gtest/integration/sample_suite_tests.cpp
```

正则库案例也有 examples 集成测试：

```text
tests/gtest/integration/examples_tests.cpp
```
