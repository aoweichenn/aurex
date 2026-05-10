# Aurex 当前语法与特性清单

日期：2026-05-10
阶段：M2 language-core-no-std
状态：当前实现清单与基础语法优先级

本文记录当前仓库真实支持的 Aurex 语言表面、已具备的高级能力、未完成能力和 M2 下一步基础语法优先级。本文以 `include/aurex/syntax/token.hpp`、`include/aurex/syntax/ast.hpp`、`src/parse/*`、`src/sema/*`、`tests/samples/**` 为准。

M2 当前原则：

- M1 已舍弃，不再沿着 std/selfhost/build-tool 路线修补。
- 标准库不在当前树中，语言样例必须自包含。
- 语言级 `move(...)`、`noncopy struct` 和 use-after-move 追踪已从 M2 基线删除。
- 当前先稳定普通值语义；ownership、borrow、drop、capability/trait/where 后续重新设计。

## 总体判断

Aurex 现在已经具备一个小型系统语言核心：模块、import、可见性、函数、C FFI、struct、enum payload、泛型、method、pointer、array、`str`、block expression、`if` expression、`match` expression、`defer`、C-style `for`、`Result` / `Option` 形状约定和 `?` 都已经存在。

但它还不是“基础语法冻结”的语言。最需要先修的是基础层剩余不一致，而不是继续添加更高级特性：

- block statement 和 block expression 主体规则已统一；expression context 额外要求 final expression。
- const initializer 已补齐纯标量运算，但还不是完整 comptime。
- compound assignment 已补齐；M2 采用 Rust/Zig 风格，不提供 `++` / `--` 自增自减语法。
- trailing separator 策略已冻结：括号/角括号列表允许 trailing comma，comma 分隔的花括号列表允许但不强制最后一个 comma。
- enum 仍强制写 C-like base type 和 discriminant。
- 顶层 item 和 struct field 默认 public。
- raw pointer、unchecked string、bit cast 等没有 `unsafe` 边界。
- 泛型没有 `where` / trait / capability 约束。
- `str` 只有内建雏形，没有安全 slice / UTF-8 boundary 模型。

## 当前完整语法库存

### 词法

当前关键字：

```text
module import as pub priv extern export c
fn struct opaque enum const type impl match
let var if else for in while break continue defer return
true false null
void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str
mut cast pcast bit_cast size_of align_of
ptr_addr ptr_from_addr str_data str_byte_len str_from_bytes_unchecked
```

当前标点和操作符：

```text
( ) { } [ ] , . ... ; : :: -> => @ ?
+ - * / % & | ^ ~ !
= += -= *= /= %= &= |= ^= <<= >>=
== != < <= > >= << >> && ||
```

词法层仍保留 `++` / `--` 为独立 token，用于给出明确诊断并避免 `--x` 被误解析成两次一元负号；语法层不支持自增自减。

注释：

- `//` 行注释。
- `/* ... */` 块注释。
- 当前没有嵌套块注释。

标识符：

```text
identifier = [A-Za-z_][A-Za-z0-9_]*
```

当前是显式 ASCII 规则，不使用 locale。Unicode identifier 暂缓。

### 字面量

整数：

```aurex
0
123
1_000
0xff
0Xff
0b1010
0B1010
```

规则：

- `_` 只能出现在两个合法数字之间。
- 不允许 `1_`、`1__2`、`0x_FF`、`0b_1010`。
- 没有整数后缀。
- 整数字面量可根据期望整数类型做范围检查。

浮点：

```aurex
1.0
0.5
1e3
1.0e-3
```

规则：

- 没有期望类型时默认 `f64`。
- 期望类型是 `f32` / `f64` 时按期望类型检查。
- 当前不支持 `.5`、`1.`、`1.0f32` 或 `1.0_f32`。

字符串和 byte：

```aurex
"hello"      // str
c"hello"     // *const u8, 用于 C FFI
b'a'         // u8
b'\n'        // u8
```

普通字符串会校验 UTF-8。C 字符串拒绝内部 NUL。当前没有 raw string、multi-line string、bytes string 或 Unicode scalar `char` 字面量。

布尔和空指针：

```aurex
true
false
null
```

`null` 需要期望类型是 pointer。

### 模块与导入

```aurex
module app.main;

import common.result as result;
pub import common.status as status;
priv import internal.detail as detail;
```

规则：

- 每个文件声明一个 `module` path。
- module path 用 `.`。
- import 通过导入者目录和显式 `-I` 查找。
- import 默认 private。
- `pub import` 可 re-export。
- 使用 `alias::item` 做限定查找。
- 当前没有 package manifest、glob import、selective import、版本化依赖或 package root 语法。

### 可见性

```aurex
pub fn api() -> i32 { return 1; }
priv fn helper() -> i32 { return 2; }

struct User {
    pub id: i32;
    priv secret: i32;
}
```

当前行为：

- 顶层 item 默认 public。
- struct field 默认 public。
- import 默认 private。
- `impl` block、`extern` block、`export c fn` 不能写成 private。

长期方向建议改为 default private，但这仍未实现。

### 类型语法

基础类型：

```aurex
void bool
i8 u8 i16 u16 i32 u32 i64 u64 isize usize
f32 f64
str
```

命名类型和限定类型：

```aurex
Point
result::Result<i32, i32>
```

指针：

```aurex
*const T
*mut T
*mut *mut u8
```

数组：

```aurex
[4]u8
[16]Point
```

当前限制：

- 数组类型可以作为字段存在。
- 数组和含数组类型不能作为普通 by-value 参数、返回值、赋值目标或 enum payload。
- 这些限制来自当前 ABI/lowering 能力，不是最终语言设计。

当前没有：

- tuple。
- slice 类型。
- safe reference `&T` / `&mut T`。
- function pointer 类型。
- never type。
- type-level const generic。

### 顶层 item

const：

```aurex
const ANSWER: i32 = 42;
```

type alias：

```aurex
type Count = usize;
```

struct：

```aurex
struct Point {
    x: i32;
    y: i32;
}
```

generic struct：

```aurex
struct Pair<T, U> {
    first: T;
    second: U;
}
```

enum：

```aurex
enum Option<T>: u8 {
    some(T) = 1,
    none = 2,
}
```

当前 enum 必须有 base type，case 必须有 discriminant。payload 只能是一个类型。

opaque struct：

```aurex
opaque struct FILE;
```

function：

```aurex
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn inferred() {
    return 42;
}
```

普通非 C、非 prototype、非 generic 函数可以省略返回类型并由 return 推导。

function prototype：

```aurex
fn declared(a: i32) -> i32;
```

prototype 必须有定义，且签名必须一致。

C FFI：

```aurex
extern c {
    opaque struct FILE;
    fn puts(s: *const u8) -> i32 @name("puts");
    fn printf(format: *const u8, ...) -> i32 @name("printf");
}

export c fn exported(argc: i32, argv: *mut *mut u8) -> i32 @name("main") {
    return 0;
}
```

限制：

- variadic 只允许 `extern c`。
- C ABI 函数必须显式返回类型。
- generic extern/export C 函数不支持。
- `@name("...")` 只支持 ABI 名称属性。

impl / method：

```aurex
impl Point {
    fn len2(self: *const Point) -> i32 {
        return self.x * self.x + self.y * self.y;
    }
}

impl<T> Box<T> {
    fn value(self: *const Box<T>) -> T {
        return self.item;
    }
}
```

规则：

- impl target 必须是 named struct / enum / opaque struct 类型。
- method 的 `self` 如果存在，必须是第一个参数。
- `self` 类型必须是 impl 类型或其 pointer。
- method 可以是 generic。
- 当前 receiver 使用 raw pointer 或值，没有 safe reference。

### 语句

局部绑定：

```aurex
let x: i32 = 1;
let y = 2;
var z: i32 = 3;
```

规则：

- `let` 不可赋值。
- `var` 可赋值。
- initializer 必填。
- 类型可省略并推导。
- 当前不允许 shadowing。

赋值：

```aurex
z = z + 1;
z += 1;
point.x = 2;
ptr.value = 3;
array[index] = 4;
```

支持简单赋值和复合赋值：

```text
= -= *= /= %= <<= >>= &= ^= |=
```

不支持 `++` / `--`。计数更新必须显式写成 `x += 1` / `x -= 1`；`x++`、`++x`、`x--`、`--x` 都是语法错误。这个选择与 Rust/Zig 类似，避免 C/C++/Java/JavaScript 的“更新表达式返回旧值或新值”心智负担，也避免副作用表达式进入 M2 基础语法。

控制流：

```aurex
if cond {
    return 1;
} else if other {
    return 2;
} else {
    return 3;
}

while i < n {
    i = i + 1;
}

for var i: i32 = 0; i < 10; i = i + 1 {
    continue;
}

for i in range(10) {
    continue;
}

for i in range(2, 10) {
    continue;
}

for i in range(10, 0, -2) {
    continue;
}
```

当前 `for i in range(...)` 是 M2 基础语法糖，不是通用 iterator：

- `range(end)` 是半开区间 `[0, end)`。
- `range(start, end)` 是半开区间 `[start, end)`。
- `range(start, end, step)` 支持显式步长；`step > 0` 时向上迭代，`step < 0` 时向下迭代，`step == 0` 时零次迭代。
- `start` / `end` / `step` 只求值一次，必须是同一种整数类型。
- loop 变量默认不可变，作用域只在循环体内。
- 当前没有 `for item in container`、数组/切片/字符串迭代或 iterator protocol。

`break` / `continue` 只能在 loop 中使用；在 range-for 中，`continue` 跳到隐藏的 cursor 更新点，隐式 step 使用 `1`，显式 step 使用用户给定的步长。

defer：

```aurex
defer cleanup();
```

当前 `defer` 只允许 call expression。

return：

```aurex
return;
return value;
```

expression statement：

```aurex
foo();
foo()?;
```

语义层只允许 call 和 try expression 作为 expression statement。`x + y;` 会诊断。

### 表达式

name / qualified name：

```aurex
value
math::add
Status.ok
remote::Status.ok
```

grouping：

```aurex
(a + b)
```

unary：

```aurex
!ok
-value
~mask
&place
*ptr
```

binary precedence 从高到低：

| 层级 | 操作符 |
| --- | --- |
| postfix | `.`, `[]`, `()`, `?`, callee type args |
| unary | `!`, `-`, `~`, `&`, `*`, builtins |
| multiplicative | `*`, `/`, `%` |
| additive | `+`, `-` |
| shift | `<<`, `>>` |
| comparison | `<`, `<=`, `>`, `>=` |
| equality | `==`, `!=` |
| bit and | `&` |
| bit xor | `^` |
| bit or | `|` |
| logical and | `&&` |
| logical or | `||` |

postfix：

```aurex
value.field
array[index]
ptr[index]
function(arg)
generic_fn<T>(arg)
value.method(arg)
value.method<T>(arg)
expr?
```

struct literal：

```aurex
Point { x: 1, y: 2 }
Pair<i32, bool> { first: 1, second: true }
remote::Point { x: 1, y: 2 }
```

if expression：

```aurex
let value = if ok {
    1
} else if fallback {
    2
} else {
    3
};
```

规则：

- condition 必须是 bool。
- expression form 必须有 else。
- 分支类型必须一致。
- result 不能是 void。
- const initializer 中不能使用。

block expression：

```aurex
let value = {
    let x = 1;
    if x > 0 {
        cleanup();
    }
    x + 1
};
```

当前规则：

- block body 语法与普通 statement block 共用同一套 statement 解析。
- block expression 可以包含 `let` / `var`、赋值、允许的 expression statement、普通 `if` statement、`while`、`for`、嵌套 block、`defer`、`return`、`break`、`continue`。
- expression context 要求 tail expression，且 result 不能是 void。
- `return`、`break`、`continue` 的合法性由语义阶段判断。
- 如果 block body 已经保证不 fallthrough，tail expression 会被诊断为不可达。
- 普通函数体和普通 statement block 暂不把最后表达式当作隐式 `return`。

match expression：

```aurex
let value = match result {
    .ok(x) => x,
    .err(code) => code,
};
```

当前支持：

- enum match。
- integer / bool literal match。
- wildcard `_`。
- enum case payload binding。
- guarded arm：`pattern if guard => value`。
- or-pattern：`a | b | c`。
- exhaustiveness 和 unreachable 检查。

当前限制：

- payload pattern 只能绑定一个名字。
- or-pattern alternatives 不能绑定 payload。
- 没有 struct pattern。
- 没有 `if let` / `while let` / `let ... else`。
- match 不能用于 const initializer。

try expression：

```aurex
let value = result?;
```

规则：

- operand 必须是形状匹配的 generic enum `Result<T, E>` 或 `Option<T>`。
- `Result<T, E>?` 要求当前函数返回同模块的 `Result<U, E>`。
- `Option<T>?` 要求当前函数返回同模块的 `Option<U>`。
- 形状通过 enum 名称、泛型参数数量和 case 名称检查：`ok` / `err`，`some` / `none`。
- const initializer 中不能使用。

内建表达式：

```aurex
cast(i32, value)
pcast(*const u8, ptr)
bit_cast(u32, value)
size_of(T)
align_of(T)
ptr_addr(ptr)
ptr_from_addr(*mut T, address)
str_data(text)
str_byte_len(text)
str_from_bytes_unchecked(data, len)
```

当前这些内建都在普通表达式层，没有 `unsafe` 语法保护。

### Pattern 语法

```aurex
_
case_name
EnumName.case_name
.case_name
case_name(binding)
EnumName.case_name(binding)
.case_name(binding)
42
true
false
p1 | p2 | p3
```

限制：

- enum match pattern 必须是 enum case 或 `_`。
- integer/bool match pattern 必须是 literal 或 `_`。
- payload binding 只能绑定一个名字。
- or-pattern 不能绑定 payload。
- wildcard 后续 arm 会诊断 unreachable。

### const initializer

当前允许：

- integer / float / bool / null / string / c-string / byte literal。
- const 引用。
- enum case 常量。
- struct literal，只要字段 initializer 都是 const evaluable。
- `!`、`-`、`~`。
- `+`、`-`、`*`、`/`、`%`、`<<`、`>>`、比较、相等、bitwise `&` / `^` / `|`、logical `&&` / `||`。
- `cast`、`pcast`、`bit_cast`、`ptr_addr`、`ptr_from_addr`。
- `size_of`、`align_of`。

当前不允许：

- 函数调用。
- `if` / block / match / `?`。
- `str_data` / `str_byte_len` / `str_from_bytes_unchecked`。

## 已支持的高级特性

当前已经具备的高级能力：

1. 手写 lexer/parser、错误恢复和 ID-backed AST。
2. 多文件 module/import/re-export。
3. C FFI：`extern c`、`export c fn`、`@name`、variadic extern。
4. 泛型 struct / enum / function / method，采用单态化实例化。
5. 泛型 struct literal 推断和泛型函数参数推断。
6. enum payload 和 enum constructor，包括 generic enum constructor。
7. pattern matching：enum/integer/bool、payload binding、guard、or-pattern、exhaustiveness。
8. `Result` / `Option` 形状约定和 `?` lowering。
9. method / associated function 查找，支持跨模块可见性。
10. block expression 和 `if` expression。
11. `defer` 和早返回路径中的 lowering 支持。
12. typed Aurex IR、IR verifier、pass pipeline、LLVM backend 和 native 输出。
13. 普通 `fn main` 入口：无参数或 `(argc: i32, argv: *mut *mut u8)`，返回 `i32` 或 `void`。

这些能力说明 Aurex 已经超过“基础表达式语言”，但其中不少能力依赖还没冻结的基础语法。例如 `?` 依赖形状约定，generic 依赖缺失的 capability/where，method receiver 依赖 raw pointer。

## 已删除或明确不在 M2 基线的能力

M2 已删除：

- `std/` 源树。
- host C support 自动链接。
- driver std 查找和隐式 import path 注入。
- `--stdlib`、`--std-backend`、`--no-stdlib`。
- M1 std/system/build-tool/selfhost 样例。
- 标准库名字特判。
- `move(...)`。
- `noncopy struct`。
- use-after-move 追踪。

这些不是永久禁止，而是不能作为当前语言地基继续推进。资源语义必须等 `Drop` / borrow / capability / trait / where 重新设计后再进入。

## 未完成特性总清单

### 基础语法未完成

- enum base type / discriminant 可选。
- 多字段 enum payload。
- raw string、bytes string、Unicode scalar `char`。
- literal suffix 策略：整数/浮点类型后缀。
- 嵌套块注释。
- default private 迁移。
- public API 显式返回类型策略。

### 类型系统未完成

- `where` 约束。
- trait / interface / protocol。
- capability predicate：`Copy`、`Drop`、`Sized`、`Eq`、`Hash` 等。
- associated type / type member。
- function pointer type。
- tuple / tuple struct / anonymous record。
- safe reference：`&T` / `&mut T`。
- slice type。
- const generic。
- never type。

### 资源与安全未完成

- `unsafe` block / `unsafe fn`。
- raw pointer dereference 和 unchecked builtin 的 unsafe-only 诊断。
- Drop / destructor / automatic scope drop。
- borrow checker。
- lifetime / region。
- partial move / field move-out。
- aliasing model。
- owned resource API 约束。

### 表达式与 pattern 未完成

- struct pattern。
- nested pattern。
- multi-payload destructuring。
- `if let` / `while let` / `let ... else`。
- range pattern。
- string/byte pattern。
- match guard exhaustiveness 更精确建模。
- lambda / closure。
- operator overloading。
- user-defined implicit conversion。
- `for item in container` / iterator protocol。

### 模块、包和工程能力未完成

- package manifest。
- package root 和 package/module 映射。
- public surface dump。
- selective import。
- glob import。
- versioned dependency。
- std/core 恢复。
- selfhost / Stage1。
- build tool。

### 高级能力暂缓

- class / inheritance / dynamic dispatch。
- trait object。
- macro / derive。
- comptime。
- async / coroutine。
- effect system。
- concurrency capability：`Send` / `Sync` 等。
- GC 或托管运行时。

## 基础语法优先级评估

M2 接下来应先完善基础语法。建议不要先做 trait、borrow、class、macro、async 或 std。

### P0：必须先做

1. 统一 block 语法：已补

   block expression 已经可以完整承载普通 statement，主体规则与普通 block 共用。当前冻结为：

   ```text
   block = "{" stmt* tail_expr? "}"
   ```

   expression context 要求 tail；statement/function body context 暂不引入隐式 return。

2. 冻结 expression statement 规则：已补

   当前语义已收口到 call / try expression，并已有负例保持 `x + y;` 这类无效语句的诊断。

### P1：基础语言形态收口

1. enum ADT 语法

   让 base type 和 discriminant 可选：

   ```aurex
   enum Option<T> {
       some(T),
       none,
   }
   ```

   保留 `enum Status: u8 { ok = 0, err = 1 }` 作为 ABI/repr enum。

2. trailing separator 策略已冻结

   当前规则：圆括号列表和角括号列表允许 trailing comma；comma 分隔的花括号列表允许 trailing comma，但最后一个元素不强制 comma；struct field declaration 继续使用 `;`，最后一个 field 仍按 statement-like 风格保留 `;`。这条已由 parser 单测和 `tests/samples/positive/core/trailing_separator_policy.ax` 覆盖。

3. default private 迁移

   顶层 item 和 field 默认 public 是长期 API 风险。建议先文档固定长期方向，再增加 warning，最后切换。

4. public 函数返回类型显式化

   public API 的返回类型不应依赖推导。private helper 可以暂时保留推导。

5. namespace 规则冻结

   `.` 只用于 module path、field、method；`::` 只用于 import alias 下的 item/type/const/function/enum constructor。不要引入混合长路径表达式。

### P2：基础安全边界

1. 最小 `unsafe`

   raw pointer dereference、`pcast`、`bit_cast`、`ptr_from_addr`、`str_from_bytes_unchecked` 应进入 unsafe-only 清单。先做语法和诊断框架，不必一次完成 borrow checker。

2. `str` 安全边界

   保留 `str` 语言内建，但把 unchecked 构造放入 unsafe。checked UTF-8 构造和 checked slicing 可先设计语义，再决定 builtin 还是未来 core API。

3. safe reference 草案

   文档先冻结 `&T` / `&mut T` 方向，让 raw pointer 回到 FFI/unsafe 内部实现的位置。

### P3：基础层稳定后再做

- `where` / capability / trait。
- Drop / destructor。
- borrow checker。
- struct pattern / `if let` / `let ... else`。
- package。
- std/core 恢复。

这些都重要，但不应抢在 block、const、assignment、enum ADT 和 unsafe 边界之前。

## 近期执行建议

建议按这个顺序开工：

1. 写当前 grammar 的 EBNF，并用 parser tests 锁住。
2. 合并 block statement / block expression。
3. 统一 trailing separator 策略。
4. 设计 enum ADT 的最小语法。
5. 给 `unsafe` 设计最小 AST/语义框架。
6. 再进入 `where` / capability / Drop / borrow。

## 参考

这些资料只作为 M2 设计参考，不表示 Aurex 要照搬：

- Rust Reference: Block expressions: https://doc.rust-lang.org/reference/expressions/block-expr.html
- Rust Reference: The unsafe keyword: https://doc.rust-lang.org/reference/unsafe-keyword.html
- Rust Compiler Development Guide: Unsafety checking: https://rustc-dev-guide.rust-lang.org/unsafety-checking.html
- Zig Language Reference: https://ziglang.org/documentation/master/
- Go Language Specification: https://go.dev/ref/spec
- Swift API Design Guidelines: https://www.swift.org/documentation/api-design-guidelines/
