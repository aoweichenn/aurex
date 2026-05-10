# Aurex 语法现状与设计评估

日期：2026-05-10
状态：M2 语法审计与设计建议

本文基于当前工作区代码与样例整理，重点阅读范围包括：

- `include/aurex/syntax/token.hpp`
- `include/aurex/syntax/ast.hpp`
- `src/lex/lexer.cpp`
- `src/parse/parser.cpp`
- `src/parse/parser_expr.cpp`
- `src/sema/*.cpp`
- `tests/samples/positive/**`
- `tests/samples/negative/**`
- `examples/**`

本文不是最终语言规范，而是把当前 Aurex 已经能写的语法、语义边界和后续设计方向先固定下来，避免继续在“加什么特性”上发散。

M2 说明：M1 阶段已经舍弃。M1 的问题不是单个语法点失败，而是标准库、host support、构建工具样例、自举实验和语言核心同时扩张，导致语言规则没有形成可验证的稳定基线。M2 不再沿着 M1 继续补丁式推进，而是回到 language-core-no-std，先修基础语法和核心语义。

## 结论先行

Aurex 现在已经不是只支持函数和表达式的小语言。当前语言核心已经具备系统语言雏形：模块、可见性、C FFI、结构体、枚举 payload、泛型、method、标准形状 `Result` / `Option` 约定与 `?`、`defer`、C-style `for`、基础 `for i in range(...)`、块表达式、`if` 表达式、`match` 表达式、指针、内建 `str` 和显式 cast 都已经落地。

真正的问题不是“还缺多少语法”，而是核心模型还没有冻结。当前最危险的混乱点有五个：

1. 指针承担了太多职责：既像 C raw pointer，又像临时借用引用，又是 method receiver 和 FFI 边界。
2. 泛型缺少正式约束。当前 no-std 分支已经移除了标准库名字特判，但只要恢复 `Vec`、`Map`、`Result` / `Option` 这类核心库，copyability / destructor 约束仍然需要语言级表达。
3. M1 的 `noncopy` / `move` 所有权 MVP 已从 M2 当前语法删除；资源语义需要用 `Drop` / borrow / capability / unsafe 边界重新设计。
4. `enum` 当前更像“带 payload 的 C enum”，每个 case 都必须显式 discriminant，不适合长期作为主力代数数据类型语法。
5. 默认可见性偏宽，顶层 item 和字段默认 `pub`，这和大型工程、模块隔离、public surface 稳定性是反方向的。

建议 Aurex 的长期语言定位保持清晰：面向编译器、构建系统、系统工具和可控 FFI 的现代静态系统语言。核心设计应该是：

- `struct` / `enum` / `trait` / `impl` 组成抽象模型。
- 标准形状的 `Result` / `Option` / `?` 作为主错误模型，暂不引入异常。
- `Drop` / borrow / capability / unsafe 共同组成资源安全模型。
- raw pointer、`pcast`、`bit_cast`、`ptr_from_addr`、`str_from_bytes_unchecked` 进入 `unsafe` 边界。
- 泛型必须尽快从“单态化模板”升级到“有 capability / trait 约束的单态化泛型”。
- class / inheritance / dynamic dispatch 可以做迁移友好层，但不应抢在 trait、borrow、Drop 之前成为核心。

M2 的直接结论：不要恢复 M1 的标准库和 selfhost 路线来掩盖语言问题。先把 block、const、enum ADT、unsafe、default visibility、capability/where 和 safe reference 这些基础规则定下来。

## 当前语法清单

### 词法

空白字符会被忽略。支持 `//` 行注释和 `/* ... */` 块注释。当前块注释不是嵌套注释。

标识符目前是 ASCII 风格：

```text
identifier = [A-Za-z_][A-Za-z0-9_]*
```

关键字包括：

```text
module import as pub priv extern export c
fn struct opaque enum const type impl match
let var if else for in while break continue defer return
true false null
void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str
mut cast pcast bit_cast size_of align_of
ptr_addr ptr_from_addr str_data str_byte_len str_from_bytes_unchecked
```

字面量：

- 整数字面量：十进制、`0x` / `0X` 十六进制、`0b` / `0B` 二进制，允许 `_` 分隔；`_` 只能出现在两个合法数字之间。
- 浮点字面量：`1.0`、`1e3`、`1.0e-3`，没有期望类型时默认 `f64`，有 `f32` / `f64` 期望类型时按期望类型检查。
- 普通字符串字面量：`"text"`，类型是 `str`，lexer 会校验解码后的内容必须是有效 UTF-8。
- C 字符串字面量：`c"text"`，类型是 `*const u8`，会拒绝内部 NUL，面向 FFI。
- byte 字面量：`b'a'`、`b'\n'`，类型是 `u8`。
- 布尔字面量：`true`、`false`。
- 空指针字面量：`null`，需要期望类型是指针。

普通字符串支持的 escape：

```text
\0 \n \r \t \\ \" \u{...}
```

当前浮点字面量不支持 `.5`、`1.` 或 `1.0f32` 这类后缀/省略形式。

标点和操作符：

```text
( ) { } [ ] , . ... ; : :: -> => @ ?
+ - * / % & | ^ ~ !
= == != < <= > >= << >> && ||
```

当前已支持复合赋值和基础 `for i in range(...)`。当前没有 `++`、`--`、`..` range operator、`?:` 三元表达式、lambda 箭头、属性列表或宏调用语法。

### 模块与导入

模块声明可选：

```aurex
module my.package.name;
```

导入语法：

```aurex
import common.algorithms;
import common.result as result;
pub import common.prelude as prelude;
priv import internal.helpers as helpers;
```

规则：

- import 路径使用 `.` 分隔。
- `as` 绑定直接导入别名。
- `pub import` 形成 public re-export。
- 未限定名字会在当前模块、直接 import 和 public re-export 中查找；冲突会报 ambiguous。
- 限定查找使用 `alias::item`。
- import 默认是 private；顶层 item 和 struct field 当前默认 public。

当前没有 package / crate 边界、选择性导入、glob import、版本化包名、模块 public surface dump 或包管理语法。

### 顶层 item

顶层 item 支持：

```aurex
const ANSWER: i32 = 42;

type Count = usize;

struct Point {
    x: i32;
    y: i32;
}

enum ResultCode: u8 {
    ok = 1,
    err = 2,
}

enum Option<T>: u8 {
    some(T) = 1,
    none = 2,
}

opaque struct CHandle;

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn prototype(a: i32) -> i32;

extern c {
    opaque struct FILE;
    fn puts(s: *const u8) -> i32 @name("puts");
    fn printf(format: *const u8, ...) -> i32 @name("printf");
}

export c fn exported(argc: i32, argv: *mut *mut u8) -> i32 @name("main") {
    return 0;
}

impl Point {
    pub fn sum(self: *const Point) -> i32 {
        return self.x + self.y;
    }
}
```

可见性：

```aurex
pub fn public_api() -> void {}
priv fn private_api() -> void {}

struct User {
    pub id: i32;
    priv secret: i32;
}
```

限制：

- `impl` block 不能 `priv`。
- `extern c` block 不能 `priv`。
- `export c fn` 强制 public，不能 private。
- `extern c` 只接受 `fn` 和 `opaque struct`。
- `extern c` 函数必须以 `;` 结束，没有 Aurex body。
- 变长参数 `...` 只允许 `extern c`，并且必须是最后一个参数。
- C ABI 函数、extern 函数和 prototype 必须显式返回类型。
- 泛型 extern/export/prototype/variadic 当前不支持。

ABI 名称：

```aurex
fn internal_name() -> i32 @name("stable_abi_symbol") {
    return 0;
}
```

当前只支持 `@name("...")` 这个 ABI attribute。

### 类型语法

内建类型：

```text
void bool
i8 u8 i16 u16 i32 u32 i64 u64 isize usize
f32 f64
str
```

命名类型：

```aurex
Point
Box<u8>
result::Outcome<i32>
```

指针类型：

```aurex
*const T
*mut T
*mut *mut u8
```

数组类型：

```aurex
[16]u8
[0x2A]u8
[0b1010]u8
[1_000]u8
```

泛型类型：

```aurex
struct Pair<T, U> {
    left: T;
    right: U;
}

enum Result<T, E>: u8 {
    ok(T) = 1,
    err(E) = 2,
}
```

限制：

- `void` 不能作为普通 storage 类型。
- `opaque struct` 只能 behind pointer，不能按值存储、`size_of` 或 `align_of`。
- 数组不能作为函数参数或返回值按值传递。
- 含数组的 struct 也不能按值传参、返回或赋值。
- 递归值类型非法；通过指针递归可以。
- enum base type 必须是整数类型。
- enum payload 不能含数组 storage。
- type alias 会检测循环。

### 函数、method 与调用

函数参数必须写名字和类型：

```aurex
fn f(x: i32, y: i32) -> i32 {
    return x + y;
}
```

普通非 C、非 prototype、非泛型函数允许返回类型推导：

```aurex
fn answer() {
    return 42;
}
```

限制：

- 递归函数如果没有显式返回类型，会诊断。
- 非 `void` 函数必须保证所有路径返回值。
- 普通 `fn main` 只允许 `main() -> i32`、`main() -> void`，或 `main(argc: i32, argv: *mut *mut u8) -> i32/void`。
- 普通 `fn main` 不能和 exported C `main` 同时存在。
- 普通 `fn main` 不能使用 ABI name `"main"`。

method 使用 `impl Type`：

```aurex
impl Counter {
    pub fn inc(self: *mut Counter) -> i32 {
        self.value = self.value + 1;
        return self.value;
    }

    pub fn get(self: *const Counter) -> i32 {
        return self.value;
    }
}
```

调用方式：

```aurex
counter.inc();
let value: i32 = counter.get();
Counter.new(1);
Box<i32>.make_pair<u8>(3, cast(u8, 4));
```

method 规则：

- `self` 如果存在，必须是第一个参数。
- `self` 类型必须是 impl 目标类型、`*const ImplType` 或 `*mut ImplType`。
- `*mut self` receiver 需要 writable place。
- rvalue 可以调用 const receiver method。
- method 可以有自己的泛型参数。
- `impl<T> Box<T>` 这类泛型 impl 支持实例方法和 associated function。
- 跨模块 private method 会给专门诊断。

### 局部变量与语句

局部声明：

```aurex
let x: i32 = 1;
let inferred = 1;
var count: i32 = 0;
```

规则：

- `let` 是不可写绑定。
- `var` 是可写绑定。
- 局部声明必须有 initializer。
- 类型可省略，但必须能推导。
- 当前不允许 shadowing；内层作用域不能重新定义同名 local/param。

赋值：

```aurex
count = count + 1;
point.x = 2;
ptr[0] = 42;
```

规则：

- 左侧必须是 writable place。
- 只支持简单 `=`，没有复合赋值。

控制流语句：

```aurex
if condition {
    return 1;
} else if other {
    return 2;
} else {
    return 3;
}

while i < n {
    i = i + 1;
}

for var i: i32 = 0; i < n; i = i + 1 {
    if i == 3 {
        continue;
    }
}

for i in range(n) {
    continue;
}

for i in range(2, n) {
    continue;
}

for ; ; {
    break;
}

defer cleanup();

return value;
return;
```

规则：

- `if` / `while` / `for` condition 必须是 `bool`。
- `for init; condition; update { ... }` 三段都可以为空。
- `break` / `continue` 只允许在 loop 内。
- `continue` 会先进入 for-update，再回 condition。
- `defer` 当前只允许函数调用表达式。
- 表达式语句当前只允许函数调用或 `?` try expression；其他无效果表达式作为 statement 会诊断。

### 表达式

基础表达式：

```aurex
42
true
false
null
"hello"
c"hello"
b'\n'
name
alias::name
(expr)
```

一元表达式：

```aurex
!value
-value
~value
&place
*pointer
```

二元表达式优先级从高到低：

| 优先级 | 操作符 |
| --- | --- |
| postfix | `.`, `[]`, `()`, `?`, explicit type args on callee |
| unary | `!`, `-`, `~`, `&`, `*`, builtins |
| multiplicative | `*`, `/`, `%` |
| additive | `+`, `-` |
| shift | `<<`, `>>` |
| compare | `<`, `<=`, `>`, `>=` |
| equality | `==`, `!=` |
| bit and | `&` |
| bit xor | `^` |
| bit or | `|` |
| logical and | `&&` |
| logical or | `||` |

`&&` 和 `||` 在 lowering 中走 short-circuit。

field / index / call：

```aurex
value.field
array[index]
pointer[index]
function(arg1, arg2)
value.method(arg)
Type.associated(arg)
generic_fn<T>(value)
value.method<U>(arg)
```

struct literal：

```aurex
Point { x: 1, y: 2 }
Pair<i32, bool> { left: 1, right: true }
module_alias::Point { x: 1, y: 2 }
```

规则：

- struct literal 必须初始化所有字段。
- 字段不能重复。
- 跨模块 private field 不能访问或初始化。
- 泛型 struct literal 可由期望类型或字段值推断；推断失败时需要显式 `<...>`。

内建转换和查询：

```aurex
cast(i32, value)
pcast(*mut u8, pointer)
bit_cast(u64, value)
size_of(T)
align_of(T)
ptr_addr(pointer)
ptr_from_addr(*mut T, addr)
str_data(text)
str_byte_len(text)
str_from_bytes_unchecked(data, len)
```

规则：

- `cast` 只用于 numeric / bool 之间的显式转换。
- `pcast` 只用于 pointer 到 pointer。
- `bit_cast` 要求源/目标 copyable，尺寸相同，并限制在标量或 pointer 形态。
- `ptr_addr` 要求参数是 pointer，返回 `usize`。
- `ptr_from_addr` 目标类型必须是 pointer，地址参数是整数。
- `str_*` builtins 是当前 `str` ABI 支撑点。

`if` 表达式：

```aurex
let x: i32 = if cond { 1 } else if other { 2 } else { 3 };
```

规则：

- `if` 表达式必须有 `else`。
- then / else block 必须产生值；`else` 可以继续接 `if` expression。
- 所有分支类型必须一致。
- 结果不能是 `void`。
- const initializer 中不能使用。

block expression：

```aurex
let value: i32 = {
    let base: i32 = 40;
    base + 2
};
```

规则：

- 末尾必须是无分号 final expression。
- final expression 结果不能是 `void`。
- const initializer 中不能使用。

`match` 表达式：

```aurex
let code: i32 = match result {
    Result.ok(value) => value,
    Result.err(err) => err,
};

let label: i32 = match flag {
    true => 1,
    false => 0,
};

let fallback: i32 = match number {
    0 => 10,
    1 | 2 => 20,
    _ => 30,
};
```

规则：

- match value 当前支持 enum、integer、bool。
- arms 使用 `pattern => expr`，arm 之间用 `,`。
- arm 可有 guard：`pattern if condition => expr`。
- arm 结果类型必须一致，结果不能是 `void`。
- enum match 做 exhaustiveness 检查。
- integer match 当前需要 wildcard arm。
- guarded arm 不计入 exhaustiveness。
- const initializer 中不能使用。

后缀 `?`：

```aurex
let value: T = maybe_result?;
```

规则：

- operand 必须是标准形状的 `Result<T, E>` 或 `Option<T>` 泛型 enum。
- `Result<T, E>?` 要求当前函数返回 `Result<U, E>`。
- `Option<T>?` 要求当前函数返回 `Option<U>`。
- `?` 当前按普通值语义处理 payload；未来资源 payload 需要在新的 ownership/drop 设计中重新定义。
- const initializer 中不能使用。

### Pattern 语法

当前 pattern 支持：

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

- payload pattern 当前只支持一个 binding，不支持多字段解构。
- or-pattern alternatives 不能绑定 payload。
- enum match pattern 必须是 enum case 或 `_`。
- integer/bool match pattern 必须是 literal 或 `_`。
- wildcard 后面的 arm 会被判 unreachable。
- 重复 enum case arm 会诊断。

### Const initializer

`const` initializer 当前允许：

- integer / bool / null / string / c-string / byte literal。
- enum case 常量。
- 其他 const 名称引用，并做循环检查。
- struct literal，只要字段 initializer 都是 const evaluable。
- `!`、`-`、`~`。
- 部分二元运算：`+`、`-`、`*`、比较、相等、bitwise `&` / `^` / `|`。
- `cast` / `pcast` / `bit_cast` / `ptr_addr` / `ptr_from_addr`，前提是 operand 可 const eval。
- `size_of` / `align_of`。

当前不允许：

- 函数调用。
- `if` / block / match / `?`。
- 逻辑 `&&` / `||`。
- 除法、取模、shift。

### 类型与语义特性

当前语义规则偏“显式成本”：

- 没有函数重载。
- 没有局部 shadowing。
- 没有隐式 numeric conversion。
- integer literal 可以在期望整数类型下检查范围并赋值。
- `*mut T` 可以赋给 `*const T`。
- `null` 只在期望 pointer 时成立。
- 函数名不能作为普通值使用。
- equality 只支持 bool、integer、float、pointer 和无 payload enum 等 scalar。
- 比较只支持 numeric。
- bitwise / shift / mod 只支持 integer。
- shift amount literal 会做范围检查。
- integer 除零 / 模零的 literal 情况会诊断。
- signed min / -1 的除法和取模 literal 情况会诊断 overflow。

M2 当前值语义：

- M1 的 `noncopy struct`、`move(value)` 和 use-after-move 追踪已删除。
- struct / enum 先按普通值语义处理。
- 数组和含数组类型仍受当前后端/ABI 限制，不能作为普通 by-value 存储或赋值目标。
- 当前还没有 borrow checker、partial move、lifetime、automatic drop 或正式 destructor。

泛型约束缺口：

- 当前 no-std 分支没有 bundled `std/`，也没有针对标准库 API 的名字特判。
- 语言仍然支持泛型 struct / enum / function / method 的单态化和推断。
- 但语言还没有 `where`、trait、capability predicate，因此无法在语法上表达“`T` 必须 copyable”“`T` 必须可销毁”“`K` 必须可比较或可哈希”。
- 一旦恢复核心库，`Vec<T>` 的 by-value read / move-out、`Map<K,V>` 的 key/value API、`Result<T,E>` / `Option<T>` 的资源 payload API 都需要这套约束系统承接，不能继续靠约定或库名特判。

## 当前还没有的语法

这份清单很重要，因为它能防止后续误以为“应该已经有”：

- `char` / Unicode scalar 字面量。
- raw string、multi-line string、bytes string。
- tuple、tuple struct、anonymous record。
- lambda / closure。
- trait / interface / protocol。
- `where` / generic constraint。
- safe reference 类型，例如 `&T`、`&mut T`。
- `unsafe` block / `unsafe fn`。
- automatic Drop。
- partial move。
- borrow checker / lifetime syntax。
- class / inheritance / vtable dispatch。
- exception / throw / catch。
- async / await / coroutine。
- operator overloading。
- user-defined implicit conversion。
- macro system。
- attribute system beyond `@name`.
- module package manifest。
- selective import / namespace export list。
- `switch` statement；当前用 `match` 表达式。
- compound assignment。
- range / slice syntax。

## 设计诊断

### 1. 语言身份已经偏系统语言，不要再向脚本语言发散

当前 Aurex 的强项是 explicit layout、FFI、LLVM lowering、资源所有权和可预测控制流。它适合继续向 Rust / Zig / Swift systems subset 这个方向推进，而不是去追 JavaScript / Python 风格动态便利。

因此不应优先加入：

- dynamic object。
- implicit conversion。
- exception。
- 运行时反射。
- 隐式 GC 语义。
- 任意 operator overloading。

这些特性会立刻削弱当前“显式成本”和“可预测 lowering”的路线。

### 2. 默认 public 是长期风险

当前顶层 item 和字段默认 public，import 默认 private。这个默认值对早期样例很方便，但对大型工程是风险：

- public surface 会意外扩大。
- private field / method 的诊断价值被稀释。
- 后续 package API 稳定时很难知道哪些是承诺。

建议在 M2 收口期切换到默认 private，所有跨模块 API 必须显式 `pub`。如果担心破坏样例，可以先做迁移期：

```text
0. 当前版本继续接受默认 public。
1. 开启 warning：跨模块可访问但未写 pub 的 item/field 提示。
2. M2 结束前改为默认 private。
```

### 3. pointer 与 reference 必须分层

当前 `*const T` / `*mut T` 同时用于：

- C FFI raw pointer。
- method receiver。
- address-of `&place`。
- mutable place access。
- 未来核心库内部 buffer / span。

这会让未来 borrow checker 很难落地。建议后续分成两层：

```aurex
&T        // safe shared borrow，不能写
&mut T    // safe unique mutable borrow
*const T  // raw pointer，只在 unsafe/FFI 边界使用
*mut T    // raw mutable pointer，只在 unsafe/FFI 边界使用
```

method receiver 长期应优先写：

```aurex
impl Vec<T> {
    pub fn len(self: &Vec<T>) -> usize { ... }
    pub fn push(self: &mut Vec<T>, value: T) -> bool { ... }
}
```

短期不需要立刻改现有 `*const` / `*mut`，但设计文档应明确：raw pointer 不是最终 safe reference 模型。

### 4. `unsafe` 必须尽快进入语言

当前这些能力都暴露为普通表达式：

```aurex
pcast(*mut T, p)
bit_cast(U, x)
ptr_from_addr(*mut T, addr)
str_from_bytes_unchecked(data, len)
```

这不适合长期工业语言。建议加入：

```aurex
unsafe fn from_addr(addr: usize) -> *mut T { ... }

let p: *mut T = unsafe {
    ptr_from_addr(*mut T, addr)
};
```

规则：

- raw pointer dereference 未来应要求 unsafe。
- pointer-from-integer 应要求 unsafe。
- unchecked UTF-8 / unchecked str construction 应要求 unsafe。
- bit_cast 对非平凡布局应要求 unsafe 或更严格限制。
- safe 核心库可以内部用 unsafe，但必须封装成安全 API。

RustBelt 和 Stacked Borrows 的经验说明，系统语言可以有 unsafe，但必须有清晰封装边界和 aliasing 语义，否则优化器与核心库会互相踩语义红线。

### 5. 泛型需要 capability / trait 约束，不能靠库约定

当前 no-std 分支已经移除了标准库 API 的名字特判，这让语言核心更干净。但约束问题并没有消失：只要核心库重新出现，`Vec<T>`、`Map<K,V>`、`Result<T,E>` 这类 API 就必须能表达 copyability、destructor、equality、hash 等能力要求。这些要求应该进入语言的泛型约束，而不是停留在文档约定。

建议引入两个层次：

第一层是内建 capability：

```aurex
Copy
Drop
Move
Sized
```

第二层是用户 trait：

```aurex
trait Eq {
    fn eq(self: &Self, other: &Self) -> bool;
}

trait Hash {
    fn hash(self: &Self) -> usize;
}
```

函数约束：

```aurex
fn contains<T>(items: &vec::Vec<T>, value: &T) -> bool
where T: Eq
{
    ...
}

fn destroy_deep<T>(items: &mut vec::Vec<T>) -> void
where T: Drop
{
    ...
}

fn get<T>(items: &vec::Vec<T>, index: usize) -> Option<T>
where T: Copy
{
    ...
}
```

这会让未来 `Vec<T>.get`、`Map<K,V>.get`、`destroy_deep<T>` 这类 API 的限制成为函数签名的一部分，而不是隐藏在库实现或编译器特例里。

### 6. 资源语义应改为正向能力模型

M1 的 `noncopy struct` 是失败路线里的临时 MVP，M2 已经移除。长期更推荐正向能力：

- 默认值是否 `Copy` 由字段决定。
- 是否需要禁止自动 Copy 不应靠单个语法标记硬塞进 struct 声明，而应能和 `Drop`、borrow、unsafe 边界一起解释。
- 有 `drop(self: &mut Self)` 或 `destroy(self: *mut Self)` 的类型自动是 `Drop`。
- generic constraint 用 `T: Copy` / `T: Drop`，而不是函数内部手写限制。

短期路线：

1. 先保持 M2 当前普通值语义，不恢复 `noncopy` / `move`。
2. 加 `Drop` capability 的内部模型。
3. 让未来 `Vec<T>.destroy_deep` 等核心库 API 使用 `where T: Drop` 表达约束。
4. 再考虑自动 scope drop。

### 7. enum 需要从 C enum 语法扩展成主力 ADT 语法

当前 enum 必须写 base type 和每个 discriminant：

```aurex
enum Option<T>: u8 {
    some(T) = 1,
    none = 2,
}
```

这对 ABI enum 很好，但对业务 ADT 太重。建议长期支持：

```aurex
enum Option<T> {
    some(T),
    none,
}

enum Result<T, E> {
    ok(T),
    err(E),
}

enum TokenKind: u8 {
    identifier = 1,
    kw_fn = 2,
}
```

规则：

- 没写 `: u8` / `: i32` 时，由编译器选择内部 tag 表示，不承诺 C ABI。
- 写了 base type 和 discriminant 的 enum 是 `repr` / ABI 稳定 enum。
- payload enum 默认是 ADT，不要让用户每次写数字 tag。

### 8. pattern matching 应变成 ADT 的主要消费方式

当前 pattern 已经能覆盖 enum case、payload binding、literal、wildcard、or-pattern 和 guard。下一步应该补：

```aurex
match value {
    Result.ok(x) => x,
    Result.err(e) => return Result.err(e),
}

match point {
    Point { x, y } => x + y,
}

let Result.ok(value) = result else {
    return Result.err(1);
};
```

优先级：

1. 多字段 enum payload。
2. struct pattern。
3. `let` pattern / `if let` / `while let`。
4. payload binding 在 or-pattern 中的一致性规则。
5. 更精确的 guard exhaustiveness 诊断。

### 9. 错误处理路线应继续坚持 `Result` / `Option` / `?`

Aurex 现在已经有 `?`，它按名称和形状识别泛型 `Result<T,E>` / `Option<T>` enum。建议保持这条线，不要现在引入 exception；如果后续恢复核心库，核心库应提供标准 `Result` / `Option` 定义。

理由：

- 编译器、构建工具、文件系统、进程 API 都需要显式错误类型。
- `?` 的 control flow 可 lowering、可分析，未来可和资源 payload 的 drop/borrow 规则结合。
- 异常会绕过当前 `defer` / ownership / Drop 的许多语义边界。

未来可以考虑 typed effects，但不建议 M2 引入用户可见 effect syntax。Koka 的 effect typing 和 effect handlers 很值得参考，但 Aurex 现阶段更需要先把 `Result`、Drop、borrow、unsafe 做稳。

### 10. class 可以做，但不应抢在 trait 之前

如果 Aurex 想吸引传统 OOP 用户，class/object model 有价值。但 class 不应替代 `struct` / `enum` / `trait`。

建议顺序：

1. 先完成 trait / interface。
2. 再完成 trait object 或 `dyn Trait` 风格动态分派。
3. 最后做 class 作为迁移层：封装、单继承、virtual/override、abstract/final。

不建议：

- 多继承。
- 隐式虚调用。
- class-only 世界观。
- 在 trait / borrow / Drop 之前做复杂对象生命周期。

### 11. 语法表面应该继续克制

当前 Aurex 的语法接近 C/Rust/Zig：显式类型、花括号、分号、无括号 condition。这个方向是合理的。

建议保留：

- `fn`。
- `let` / `var`。
- `struct` / `enum` / `impl`。
- `match`。
- `?`。
- `defer`。
- `for init; condition; update {}`。
- `alias::item` 做模块限定。

建议谨慎加入：

- trailing closure。
- optional semicolon。
- operator overloading。
- implicit conversion。
- overloaded function set。
- property getter/setter sugar。

这些东西都会放大 parser、diagnostic、type inference 和 tooling 成本。

## 建议的特性路线图

### P0：立刻整理和冻结当前语法

目标：停止语义漂移，让后续讨论有共同基线。

任务：

1. 维护本文或拆出正式 `language-reference.md`。
2. 给当前 grammar 写一份 EBNF。
3. 把所有 parser 可接受的语法都做 syntax sample。
4. 给“当前没有”的语法做 negative tests。
5. 决定默认可见性是否在 M2 收口期切到 private。
6. 为 `unsafe` 设计最小语法，不一定立刻改所有 lowering。
7. 浮点字面量已补齐基础形式；后续只需决定是否增加后缀方案。
8. 明确 byte / text / C string 的字面量边界。

### P1：类型系统主线

目标：把所有权、泛型和核心库约束正规化。

任务：

1. 内部引入 capability：`Copy`、`Drop`、`Sized`。
2. 增加 `where` 语法。
3. 为未来 `Vec` / `Map` / `Result` / `Option` 这类核心库 API 准备正式约束，而不是依赖约定或特判。
4. 固化 destructor 形状，短期兼容 `destroy(self: *mut T) -> void`。
5. 设计 `drop(self: &mut Self)` 或 `Drop` trait 的长期语法。
6. 引入 safe reference 类型 `&T` / `&mut T` 的语法草案。
7. raw pointer 操作进入 `unsafe`。

### P1：ADT 与 pattern matching

目标：让 `enum` 成为表达 compiler AST、token、result、build graph 状态的主力。

任务：

1. enum base type 和 discriminant 改为可选。
2. 支持多字段 payload 或 tuple payload。
3. 支持 struct pattern。
4. 支持 `if let` / `let ... else`。
5. 改进 exhaustiveness 和 unreachable diagnostics。
6. 明确 payload move / borrow / drop 规则。

### P2：模块、包和 API surface

目标：让大型项目能维护 public API。

任务：

1. package manifest。
2. package root / module path 规则。
3. public surface dump。
4. default private 迁移。
5. re-export 规则冻结。
6. import alias 继续作为推荐形式。
7. selective import 可以晚点做，不需要先加 glob。

### P2：未来核心库人体工程学

目标：让系统程序不用绕 raw pointer。

任务：

1. `Path` / `str` / `String` / `Bytes` / `CStr` / `CString` 分层继续冻结。
2. `Vec<T>` 增加 move-out API，例如 `take_at` / `swap_take`。
3. `Map<K,V>` 增加 borrowed-key 查询和 entry API。
4. 如果恢复文件、目录、进程 API，它们应全部迁到 `Result` 和 owned resource。
5. iterator / slice 语法和容器 `for item in container` 在 borrow/slice 基线后再定；当前只承认基础整数 `for i in range(end)` / `for i in range(start, end)` / `for i in range(start, end, step)`。

### P3：高级能力

这些不建议现在做，但可以规划：

- trait object / dynamic dispatch。
- class compatibility layer。
- hygienic macro 或 derive。
- comptime / const generic。
- async / coroutine。
- typed effects。
- concurrency capability，例如 `Send` / `Sync`。
- package manager。

## 不建议现在加入的东西

为了降低困惑，下面这些应明确推迟：

- 异常系统。先把 `Result` / `?` / Drop 做稳。
- 复杂 class 继承。先做 trait。
- GC 语义。当前语言定位不是托管语言。
- operator overloading。会显著增加 type checking 和诊断复杂度。
- 隐式 numeric conversion。会破坏当前强类型边界。
- 用户自定义隐式 conversion。暂不需要。
- 任意宏系统。没有稳定 AST 和 hygiene 前不要做。
- async/await。没有 borrow、Drop、effect 模型前很容易形成二次设计债。
- 高阶类型、GADT、dependent types。Aurex 当前目标不需要。

## 工业语言启发

Rust 的启发：

- ownership、borrow、trait、module、unsafe island 是一个整体，不是单个语法糖。
- `unsafe` 不是失败，而是系统语言必须有的受控边界。
- `match` 和 ADT 是错误处理、状态机、编译器前端的核心表达。
- 但完整 Rust borrow checker 一次性照搬成本很高，Aurex 应阶段化：move-only -> Drop -> reference -> borrow check -> unsafe aliasing model。

Zig 的启发：

- `defer`、显式错误路径、`comptime` 和透明 ABI 对系统语言很有价值。
- Zig 的“少语法、显式成本”很适合 Aurex 当前路线。
- 但 Aurex 已经选择 `str`、泛型 ADT 和所有权模型，不能退回“所有文本都是 `[]u8`”。

Swift 的启发：

- API 设计必须关注调用点可读性。
- protocol / extension / method surface 对大型核心库很重要。
- Swift 的 noncopyable types 说明主流工业语言也在向 move-only 能力靠近。
- 但 Swift 的高度抽象和 Unicode 默认语义不应原样进入 Aurex core。

Go 的启发：

- 小语法、明确控制流、`defer`、简单工具链很强。
- 但 Go 的 error-by-convention 和字符串任意 bytes 模型不适合 Aurex 的类型安全目标。

Kotlin / TypeScript / C# / Scala 的启发：

- null-safety、union / narrowing、sealed ADT、pattern matching、extension method 和 protocol/typeclass 风格抽象，是现代语言可读性的关键。
- 这些语言证明“可表达状态空间”的语法比传统 class hierarchy 更重要。
- Aurex 应吸收 ADT + match + trait，不应优先复制 OOP 继承树。

C++ 的启发：

- RAII、模板、concepts、zero-cost abstraction 都重要。
- 但 C++ 同时也证明：隐式转换、重载、模板错误、历史 ABI 和多范式叠加会让语言复杂度长期不可控。
- Aurex 应学习 concepts / RAII，不学习隐式复杂性。

## 学术与前沿研究启发

RustBelt / Oxide / Stacked Borrows 给 Aurex 的提醒：

- 如果语言要同时提供 unsafe、raw pointer、优化器和安全核心库，就必须认真定义 aliasing、ownership 和 unsafe 封装边界。
- borrow checker 不是单纯 parser 特性，它是类型系统、MIR/AIR、optimizer 合约和核心库 unsafe 代码共同组成的系统。
- Aurex 在引入 borrow 前，应先把 `unsafe`、Drop、move-only 和 AIR place/lvalue 模型固定。

Koka 和 algebraic effects 给 Aurex 的提醒：

- effect typing 很适合表达 IO、state、exception、async、resource 等能力。
- 但 effect system 会显著改变函数类型、泛型和 inference。Aurex 当前更应该先把 `Result` / capability / trait 做稳。
- 可以先在内部设计 capability lattice，为未来 typed effects 留接口，不要现在暴露复杂 effect syntax。

## 推荐的核心语法方向

下面是一套较稳的 Aurex 未来核心。示例假设未来恢复 `core` / `std` 风格核心库；当前 no-std 分支不要求这些模块已经存在：

```aurex
module app.main;

import std.core.vec as vec;

pub enum Option<T> {
    some(T),
    none,
}

pub enum Result<T, E> {
    ok(T),
    err(E),
}

pub trait Eq {
    fn eq(self: &Self, other: &Self) -> bool;
}

pub struct Buffer {
    bytes: vec::Vec<u8>;
}

impl Buffer {
    pub fn new() -> Buffer {
        return Buffer { bytes: vec::new<u8>() };
    }

    pub fn drop(self: &mut Buffer) -> void {
        self.bytes.destroy();
    }

    pub fn len(self: &Buffer) -> usize {
        return self.bytes.len();
    }
}

pub fn find<T>(items: &vec::Vec<T>, needle: &T) -> Option<usize>
where T: Eq
{
    var i: usize = 0;
    while i < items.len() {
        if items.get_ref(i).eq(needle) {
            return Option.some(i);
        }
        i = i + 1;
    }
    return Option.none;
}
```

这不是要求马上实现，而是给语法演进一个收束方向：

- `enum` 负责 ADT。
- `trait` / `where` 负责泛型约束。
- `&` / `&mut` 负责 safe borrow。
- `*const` / `*mut` 退回 raw pointer / FFI / unsafe。
- `drop` / `Drop` 负责资源释放。
- `Result` / `?` 负责错误传播。
- `unsafe` 负责低层逃生口。

## 近期最应该做的 12 件事

1. 把本文拆成正式 language reference 和 design notes 两份文档。
2. 给 parser 写 EBNF，并用 tests 锁住每类语法。
3. 决定默认可见性迁移策略，建议 M2 收口期切到 default private。
4. 加 `unsafe` block / `unsafe fn` 的语法和诊断框架。
5. 冻结浮点字面量后缀策略。
6. 让 enum base type / discriminant 可选。
7. 引入 capability 内部模型：`Copy`、`Drop`、`Sized`。
8. 设计并实现最小 `where` 约束。
9. 用 capability 约束承接未来核心库的 copyability / destructor 要求。
10. 继续完善 destructor / `destroy` / `Drop` 路线。
11. 设计 `&T` / `&mut T`，先文档冻结，再实现 borrow MVP。
12. 增强 `match`：struct pattern、multi-payload、`let ... else`。

## 参考资料

工业语言官方资料：

- Rust Book: Ownership: https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html
- Rust Reference: Traits: https://doc.rust-lang.org/reference/items/traits.html
- Rust Reference: Match expressions: https://doc.rust-lang.org/reference/expressions/match-expr.html
- Rust Reference: Visibility and privacy: https://doc.rust-lang.org/reference/visibility-and-privacy.html
- Zig Language Reference: https://ziglang.org/documentation/master/
- Go Language Specification: https://go.dev/ref/spec
- Swift API Design Guidelines: https://www.swift.org/documentation/api-design-guidelines/
- Swift Evolution SE-0390 Noncopyable structs and enums: https://github.com/swiftlang/swift-evolution/blob/main/proposals/0390-noncopyable-structs-and-enums.md
- Kotlin Null safety: https://kotlinlang.org/docs/null-safety.html
- Kotlin Sealed classes and interfaces: https://kotlinlang.org/docs/sealed-classes.html
- TypeScript Handbook: Narrowing: https://www.typescriptlang.org/docs/handbook/2/narrowing.html
- TypeScript Handbook: Generics: https://www.typescriptlang.org/docs/handbook/2/generics.html
- C# Pattern matching: https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/functional/pattern-matching
- C# Nullable reference types: https://learn.microsoft.com/en-us/dotnet/csharp/nullable-references
- Scala 3 Reference: Enums: https://docs.scala-lang.org/scala3/reference/enums/enums.html
- C++ draft: Constraints and concepts: https://eel.is/c++draft/temp.constr
- C++ draft: Modules: https://eel.is/c++draft/module

研究资料：

- RustBelt project: https://plv.mpi-sws.org/rustbelt/
- Oxide: The Essence of Rust: https://arxiv.org/abs/1903.00982
- Stacked Borrows: An Aliasing Model for Rust: https://plv.mpi-sws.org/rustbelt/stacked-borrows/
- The Koka Programming Language and effect typing: https://koka-lang.github.io/koka/doc/book.html
