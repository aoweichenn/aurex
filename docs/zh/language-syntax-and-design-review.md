# Aurex 语法现状与设计评估

日期：2026-05-11
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

Aurex 现在已经不是只支持函数和表达式的小语言。当前语言核心已经具备系统语言雏形：模块、可见性、C FFI、结构体、枚举 payload、泛型、method、标准形状 `Result` / `Option` 约定与 `?`、`defer`、C-style `for`、基础 `for i in range(...)`、块表达式、`if` 表达式、`match` 表达式、指针、array/slice、tuple、内建 `str` 和显式 cast 都已经落地。

真正的问题不是“还缺多少语法”，而是核心模型还没有冻结。当前最危险的混乱点有五个：

1. 指针承担了太多职责：既像 C raw pointer，又像临时借用引用，又是 method receiver 和 FFI 边界。
2. 泛型缺少正式约束。当前 no-std 分支已经移除了旧 std 名字特判；未来重新设计库层前，约束系统需要重新设计，但不进入当前基础语法阶段。
3. M1 的 `noncopy` / `move` 所有权 MVP 已从 M2 当前语法和实现删除；当前阶段保持普通值语义，不继续推进未成熟的资源模型。
4. `enum` 已从“带 payload 的 C enum”扩展为 ADT-first：普通 enum 可省略 base type 和 discriminant，显式 C-like/repr enum 仍保留给 ABI 场景；剩余缺口主要在 pattern destructuring。
5. 默认可见性偏宽的问题已完成修正：顶层 item、字段、impl method 和 import 默认 `priv`，跨模块 API 必须显式 `pub`。

建议 Aurex 的长期语言定位保持清晰：面向编译器、构建系统、系统工具和可控 FFI 的现代静态系统语言。核心设计应该是：

- `struct` / `enum` / `trait` / `impl` 组成抽象模型。
- 标准形状的 `Result` / `Option` / `?` 作为主错误模型，暂不引入异常。
- `unsafe` 先成为底层 escape hatch 的边界；资源安全模型后续再整体设计。
- raw pointer、`ptrcast`、`bitcast`、`ptrat`、`strraw` 进入 `unsafe` 边界。
- 泛型必须尽快从“单态化模板”升级到“有 capability / trait 约束的单态化泛型”。
- class / inheritance / dynamic dispatch 可以做迁移友好层，但不应抢在基础语法、`unsafe` 边界和 ADT 收口之前成为核心。

M2 的直接结论：不要复活 M1 的标准库和 selfhost 路线来掩盖语言问题。default visibility、ADT-first enum、array literal / repeat literal、slice type/expression、tuple/destructuring、function type/function pointer 和最小 unsafe 已完成；下一步继续把 struct/match pattern、`str` checked API、capability/where 和 safe reference 这些基础规则定下来。

## 当前语法清单

### 词法

空白字符会被忽略。支持 `//` 行注释和 `/* ... */` 块注释；块注释支持嵌套。

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
void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str char
mut cast ptrcast bitcast sizeof alignof
ptraddr ptrat strptr strblen strraw
```

字面量：

- 整数字面量：十进制、`0x` / `0X` 十六进制、`0b` / `0B` 二进制，允许 `_` 分隔；`_` 只能出现在两个合法数字之间；支持 `i8`、`i16`、`i32`、`i64`、`isize`、`u8`、`u16`、`u32`、`u64`、`usize` 后缀。
- 浮点字面量：`1.0`、`.5`、`1.`、`1e3`、`1.0e-3`，支持 `f32` / `f64` 后缀；没有期望类型时默认 `f64`，有 `f32` / `f64` 期望类型时按期望类型检查。
- 普通字符串字面量：`"text"`，类型是 `str`，lexer 会校验解码后的内容必须是有效 UTF-8。
- C 字符串字面量：`c"text"`，类型是 `*const u8`，会拒绝内部 NUL，面向 FFI。
- raw string：`r"text"`，类型是 `str`，escape 不解释，并允许跨行。
- byte string：`b"abc"`，类型是 `[N]u8`，只接受 ASCII raw byte 和简单 byte escape。
- byte 字面量：`b'a'`、`b'\n'`，类型是 `u8`。
- `char` 字面量：`'λ'`、`'\u{03BB}'`，类型是 `char`，表示 Unicode scalar value。
- 布尔字面量：`true`、`false`。
- 空指针字面量：`null`，需要期望类型是指针。

普通字符串支持的 escape：

```text
\0 \n \r \t \\ \" \u{...}
```

字面量后缀边界：`1f32`、`1.0u8` 和 `1.0_f32` 会诊断；整数后缀只能用于 integer literal，`f32` / `f64` 后缀只能用于 float literal。

标点和操作符：

```text
( ) { } [ ] , . ... ; : :: -> => @ ?
+ - * / % & | ^ ~ !
= == != < <= > >= << >> && ||
```

当前已支持复合赋值和基础 `for i in range(...)`。当前没有语法层 `++`、`--`、`..` range operator、`?:` 三元表达式、lambda 箭头、属性列表或宏调用语法。

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
- import、顶层 item、struct field 和 impl method 默认 private；跨模块 API 必须显式 `pub`。

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

enum OptionI32 {
    some(i32),
    none,
}

enum Token {
    ident(str),
    span(usize, usize),
    eof,
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
Box[u8]
result::OutcomeI32
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

tuple 类型：

```aurex
(i32, bool)
(i32,)
```

当前 tuple 是匿名 product type；支持 tuple literal、零基数字字段、局部解构和 match tuple pattern：

```aurex
let pair: (i32, bool) = (1, true);
let (count, ok) = pair;
return match pair { (left, right) => left };
```

空 tuple 暂不属于 M2。

泛型类型：

```aurex
struct Pair[T, U] {
    left: T;
    right: U;
}

// generic enum 当前只被 parser 识别，M2 语义层明确不支持。
```

M2 基础泛型只包含类型参数和类型实参：

- 声明侧：`fn id[T](value: T) -> T`、`struct Box[T] { ... }`。
- 类型侧：`Box[i32]`、`Pair[i32, bool]`、`Box[Pair[i32, bool]]`。
- 调用侧：显式函数泛型调用必须写 `id::[i32](value)`。
- `[]` 列表不能为空；空列表不表示推导。
- `GenericParam` 只能是 identifier；bound、`where`、trait、associated type 和 const generic 都不属于 M2 基础泛型。

限制：

- `void` 不能作为普通 storage 类型。
- `opaque struct` 只能 behind pointer，不能按值存储、`sizeof` 或 `alignof`。
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
```

method 规则：

- `self` 如果存在，必须是第一个参数。
- `self` 类型必须是 impl 目标类型、`*const ImplType` 或 `*mut ImplType`。
- `*mut self` receiver 需要 writable place。
- rvalue 可以调用 const receiver method。
- generic method 和 generic impl block 当前不支持。
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
generic_fn::[T](value)
```

struct literal：

```aurex
Point { x: 1, y: 2 }
Pair[i32, bool] { left: 1, right: true }
module_alias::Point { x: 1, y: 2 }
```

规则：

- struct literal 必须初始化所有字段。
- 字段不能重复。
- 跨模块 private field 不能访问或初始化。
- 泛型 struct literal 当前必须显式写 `[]` 类型实参，例如 `Pair[i32, bool] { ... }`。
- `name[index]` 按 index 解析；显式函数泛型调用必须写 `name::[T](...)`，不用 `name[T](...)`。
- `id::[T]` 在 AST 中是独立 postfix `generic_apply` 节点；call 再以它作为 callee。
- `fn f[]`、`Box[]`、`id::[](...)` 均非法。
- `<` / `>` 已完全退出泛型语法，只保留为比较 token。

内建转换和查询：

```aurex
cast[i32](value)
ptrcast[*mut u8](pointer)
bitcast[u64](value)
sizeof[T]
alignof[T]
ptraddr(pointer)
ptrat[*mut T](addr)
strptr(text)
strblen(text)
strraw(data, len)
```

规则：

- `cast` 只用于 numeric / bool 之间的显式转换。
- `ptrcast` 只用于 pointer 到 pointer。
- `bitcast` 要求源/目标尺寸相同，并限制在非 `bool` / 非 `str` / 非 `void` 的内建数值标量或 pointer 形态。
- `ptraddr` 要求参数是 pointer，返回 `usize`。
- `ptrat` 目标类型必须是 pointer，地址参数是整数。
- `strptr` / `strblen` / `strraw` 是当前 `str` ABI 支撑点。

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

- operand 必须是标准形状的 result-like 或 option-like payload enum。
- result-like enum 要求当前函数返回同模块、同错误 payload 的 result-like enum。
- option-like enum 要求当前函数返回同模块的 option-like enum。
- `?` 当前按普通值语义处理 payload；未来资源 payload 需要在后续资源语义专题中重新定义。
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
case_name(first, second)
EnumName.case_name(first, second)
.case_name(first, second)
42
true
false
p1 | p2 | p3
```

限制：

- payload pattern 的 binding 数量必须和 enum case payload 字段数一致，例如 `.some(value)`、`.span(start, end)`。
- or-pattern alternatives 可以绑定 payload，但每个 alternative 必须绑定同名且同类型的名字，例如 `.int(value) | .other(value)`。
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
- `cast` / `ptrcast` / `bitcast` / `ptraddr` / `ptrat`，前提是 operand 可 const eval。
- `sizeof` / `alignof`。
- array literal / repeat literal，只要元素 initializer 可 const eval 且 repeat count 是整数字面量。

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
- struct / enum 先按普通值语义处理；当前实现不维护语言级 `Copy` / `Drop` / move 状态。
- 数组和含数组类型仍受当前后端/ABI 限制，不能作为函数 by-value 参数/返回、赋值目标或 enum payload；数组可作为字段、局部变量和 const 存储，并可用数组字面量构造。
- tuple 支持普通 by-value storage、参数、返回和局部 destructuring；含数组 tuple 仍跟随“含数组类型”限制。
- 当前还没有 borrow checker、partial move、lifetime、automatic drop 或正式 destructor。

泛型约束缺口：

- 当前 no-std 分支没有 bundled `std/`，也没有针对标准库 API 的名字特判。
- 语言当前支持泛型 struct / function 的单态化和函数参数推断。
- 但语言还没有 `where`、trait、capability predicate，因此无法在语法上表达“`K` 必须可比较或可哈希”等通用约束。
- 未来重新设计库层时，`Vec[T]`、`Map[K,V]`、`Result[T,E]` / `Option[T]` 的 API 约束需要语言机制承接，不能继续靠约定或库名特判；资源相关约束等后续资源模型成型后再进入。

## 当前还没有的语法

这份清单很重要，因为它能防止后续误以为“应该已经有”：

- tuple struct、anonymous record。
- lambda / closure。
- trait / interface / protocol。
- `where` / generic constraint。
- safe reference 类型，例如 `&T`、`&mut T`。
- `unsafe` block / `unsafe fn`。
- automatic resource cleanup。
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

### 2. 默认 private 已完成

当前顶层 item、字段、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`。这是对早期默认 public 行为的硬切换，目标是让大型工程的 public surface 从一开始就可审计：

- 未写 `pub` 的 item、field 和 method 只在定义模块内可访问。
- AST dump 显示 `pub` / `priv`，不会隐藏默认可见性。
- `export c fn` 仍强制 public；`impl` / `extern` block 不能显式 `priv`。

后续 package API、文档生成和 public surface dump 应直接建立在这条规则上，不再设计默认 public 到 default private 的迁移期。

### 3. pointer 与 reference 必须分层

当前 `*const T` / `*mut T` 同时用于：

- C FFI raw pointer。
- method receiver。
- address-of `&place`。
- mutable place access。
- 未来库层内部 buffer / span。

这会让未来 borrow checker 很难落地。建议后续分成两层：

```aurex
&T        // safe shared borrow，不能写
&mut T    // safe unique mutable borrow
*const T  // raw pointer，只在 unsafe/FFI 边界使用
*mut T    // raw mutable pointer，只在 unsafe/FFI 边界使用
```

method receiver 长期应优先写：

```aurex
impl Vec[T] {
    pub fn len(self: &Vec[T]) -> usize { ... }
    pub fn push(self: &mut Vec[T], value: T) -> bool { ... }
}
```

短期不需要立刻改现有 `*const` / `*mut`，但设计文档应明确：raw pointer 不是最终 safe reference 模型。

### 4. 最小 `unsafe` 已进入语言

这些能力现在必须在 unsafe context 中使用：

```aurex
ptrcast[*mut T](p)
bitcast[U](x)
ptrat[*mut T](addr)
strraw(data, len)
```

当前写法：

```aurex
unsafe fn from_addr(addr: usize) -> *mut T { ... }

let p: *mut T = unsafe {
    ptrat[*mut T](addr)
};
```

规则：

- raw pointer dereference 已要求 unsafe。
- pointer-from-integer `ptrat` 已要求 unsafe。
- unchecked UTF-8 / unchecked str construction `strraw` 已要求 unsafe。
- `bitcast` 已要求 unsafe；后续仍可继续收紧 legality matrix。
- 未来安全库可以内部用 unsafe，但必须封装成安全 API。
- 当前不包含 borrow checker、lifetime、unsafe trait、unsafe impl、unsafe extern block 或资源模型。

RustBelt 和 Stacked Borrows 的经验说明，系统语言可以有 unsafe，但必须有清晰封装边界和 aliasing 语义，否则优化器与核心库会互相踩语义红线。

### 5. 泛型需要约束系统，不能靠库约定

当前 no-std 分支已经移除了旧 std API 的名字特判，这让语言核心更干净。但约束问题并没有消失：只要未来重新设计容器和错误处理库，`Vec[T]`、`Map[K,V]`、`Result[T,E]` 这类 API 就必须能表达 equality、hash、ordering 等能力要求。这些要求应该进入语言的泛型约束，而不是停留在文档约定。

建议先引入两个不涉及资源语义的层次：

第一层是少量内建 predicate：

```aurex
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
fn contains[T](items: &Vec[T], value: &T) -> bool
where T: Eq
{
    ...
}

```

这会让未来 `Vec[T]`、`Map[K,V]` 这类 API 的普通约束成为函数签名的一部分，而不是隐藏在库实现或编译器特例里。资源相关的 `Copy` / `Drop` / move-out 约束本阶段不设计。

### 6. 资源语义暂缓

M1 的 `noncopy struct` 是失败路线里的临时 MVP，M2 已经移除。当前实现也不再维护语言级 copyability、destructor 形状或 use-after-move 状态。短期规则保持简单：

- 不恢复 `noncopy` / `move`。
- 不增加 `Drop` capability 内部模型。
- 不固化 `destroy` / `drop` 方法形状。
- 不设计 automatic scope cleanup。
- 数组和含数组类型的限制只作为当前 ABI/lowering 限制存在。

资源语义要等基础语法、`unsafe`、slice/string、function type、ADT 和 pattern 地基稳定后再重新开设计，不在当前路线中提前占位。

### 7. enum 已从 C enum 语法扩展成主力 ADT 语法

当前 M2 enum 支持普通 ADT 形态：

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
```

显式 C-like/repr enum 仍可写：

```aurex
enum TokenKind: u8 {
    identifier = 1,
    kw_fn = 2,
}
```

规则：

- 没写 `: u8` / `: i32` 时，由编译器选择内部 tag 表示，不承诺 C ABI。
- 写了 base type 和 discriminant 的 enum 是 `repr` / ABI 稳定 enum。
- payload enum 默认是 ADT，不要让用户每次写数字 tag。
- M2 仍不支持 generic enum；`Option[T]` / `Result[T, E]` 要等泛型 enum 专题。

### 8. pattern matching 应变成 ADT 的主要消费方式

当前 pattern 已经能覆盖 enum case、payload binding、multi-field payload destructuring、literal、wildcard、or-pattern、guard、slice pattern、局部 tuple/struct/slice/enum destructuring、`let ... else`，以及 `if value is pattern` / `while value is pattern` / if 表达式 pattern condition。典型写法是：

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

1. struct pattern、match tuple pattern、nested enum payload pattern 已补齐。
2. `if value is pattern` / `while value is pattern` / if 表达式 pattern condition 已补齐。
3. payload binding 在 or-pattern 中的一致性规则已补齐：每个 alternative 必须绑定同名且同类型的名字。
4. slice pattern 和 `let ... else` 已补齐。
5. 更精确的 guard exhaustiveness 诊断。

### 9. 错误处理路线应继续坚持 `Result` / `Option` / `?`

Aurex 现在已经有 `?`，它按名称和形状识别 result-like / option-like enum。建议保持这条线，不要现在引入 exception；未来如果重新设计错误处理库，应提供标准 `Result` / `Option` 形状，泛型拼写统一使用 `Result[T,E]` / `Option[T]`。

理由：

- 编译器、构建工具、文件系统、进程 API 都需要显式错误类型。
- `?` 的 control flow 可 lowering、可分析，未来可和资源 payload 的释放/借用规则结合。
- 异常会绕过当前 `defer` 和普通控制流语义，暂时没有必要引入。

未来可以考虑 typed effects，但不建议 M2 引入用户可见 effect syntax。Koka 的 effect typing 和 effect handlers 很值得参考，但 Aurex 现阶段更需要先把 `Result`、ADT、pattern 和 `unsafe` 做稳。

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
- 在 trait、safe reference 和资源语义之前做复杂对象生命周期。

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

### P0：现代基础语法第一优先级

目标：停止语义漂移，并优先补齐现代语言和类型系统研究共同证明过的基础表达能力。对照范围不只限 Rust、Go、Zig、Kotlin、C++，也包括 Swift、C#、TypeScript/JavaScript、Python、Dart、Scala、OCaml/F#、Haskell、Julia、Nim、D、V，以及 ML/ADT、pattern matching exhaustiveness、null safety、type soundness、unsafe boundary 相关研究。

任务：

1. 维护本文或拆出正式 `language-reference.md`。
2. 给当前 grammar 写一份 EBNF。
3. 把所有 parser 可接受的语法都做 syntax sample。
4. 给“当前没有”的语法做 negative tests。
5. 最小 `unsafe` block / `unsafe fn` 已补齐：unsafe-only builtin、raw pointer dereference 和 unsafe callable 诊断都已落地；后续不扩展到资源模型。
6. ADT-first enum 已补齐：普通 enum 的 base type 和 discriminant 可选，显式 C-like/repr enum 保留 `enum Status: u8 { ok = 0, err = 1 }` 形态。
7. array literal / repeat literal 已补齐，让已有 `[N]T` 数组类型具备基础值语法。
8. 浮点字面量和 numeric suffix 已补齐基础形式；后续只需保持诊断边界。
9. default private 已完成；后续只需要 public surface dump 和文档生成基于当前规则继续完善。

### P1：基础值语法与 pattern 人体工程学

目标：补齐数组、slice、字符串/bytes、函数类型和 ADT destructuring 的基础表达能力，但不提前引入完整 closure、iterator protocol 或 std 容器。

任务：

1. slice type / slice expression 已落地；继续和 `str` 的 UTF-8 boundary 规则对齐。
2. raw/multiline raw string、bytes string、Unicode scalar `char` 的字面量边界已补齐；`b'a'` 继续是 `u8`。
3. function pointer / function type 已落地，包括 `fn(...) -> T`、`extern c fn(...) -> T`、函数名作为值和函数指针间接调用；后续只需要补完整 closure 设计。
4. tuple / destructuring declaration、struct pattern、slice pattern、match tuple pattern、nested enum payload pattern、binding or-pattern alternatives、`let ... else`、`if value is pattern`、`while value is pattern` 和 if 表达式 pattern condition 已落地；保持空 tuple 暂缓边界。
5. 后续主要改进 exhaustiveness 和 unreachable diagnostics，避免 pattern 扩展后退化为 ad-hoc 检查。

### P1：类型系统主线

目标：把泛型、safe reference 方向和非资源类核心库约束正规化。

任务：

1. 增加 `where` 语法草案。
2. 为未来 `Vec` / `Map` / `Result` / `Option` 这类核心库 API 准备非资源类正式约束，而不是依赖约定或特判。
3. 引入 safe reference 类型 `&T` / `&mut T` 的语法草案。
4. raw pointer 操作进入 `unsafe`。这是 P0 语法边界的类型系统落地，而不是独立高级特性。
5. `Copy` / `Drop` / destructor / move-out 继续暂缓到资源语义专题，不作为当前 P1 类型系统任务。

### P1：ADT 与 pattern matching

目标：让 `enum` 成为表达 compiler AST、token、result、build graph 状态的主力。

任务：

1. enum base type 和 discriminant 可选已完成；显式 C-like/repr enum 继续使用 `enum Status: u8 { ok = 0, err = 1 }`。
2. 多字段 payload 构造和 `.case(a, b)` destructuring 已完成。
3. struct pattern、slice pattern、match tuple pattern、nested enum payload pattern、binding or-pattern alternatives、`let ... else`、`if value is pattern` / `while value is pattern` / if 表达式 pattern condition 已完成。
4. 后续继续改进 structural exhaustiveness 和 guard/unreachable diagnostics。
5. 明确 payload 的普通值传递、匹配绑定和数组/含数组类型限制；资源相关的 move / borrow / drop 规则暂缓。

### P2：模块、包和 API surface

目标：让大型项目能维护 public API。

任务：

1. package manifest。
2. package root / module path 规则。
3. `pub fn` 返回类型显式化：已补，public API 不再依赖返回类型推导。
4. public surface dump。
5. re-export 规则冻结。
6. import alias 继续作为推荐形式。
7. selective import 可以晚点做，不需要先加 glob。

### P2：未来核心库人体工程学

目标：让系统程序不用绕 raw pointer。

任务：

1. `Path` / `str` / `String` / `Bytes` / `CStr` / `CString` 分层继续冻结。
2. `Vec[T]` 的取出/移除 API 等资源语义专题后再定。
3. `Map[K,V]` 增加 borrowed-key 查询和 entry API。
4. 如果未来重新设计文件、目录、进程 API，它们应全部迁到 `Result` 和 owned resource。
5. 容器 `for item in container` 和 iterator protocol 在 borrow/slice 基线后再定；slice type/expression 已落地，当前循环仍只承认基础整数 `for i in range(end)` / `for i in range(start, end)` / `for i in range(start, end, step)`。

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

- 异常系统。先把 `Result` / `?` 和基础控制流做稳。
- 复杂 class 继承。先把 ADT、method、capability/trait 路线做清楚，再评估动态分发。
- GC 语义。当前语言定位不是托管语言。
- operator overloading。会显著增加 type checking 和诊断复杂度。
- 隐式 numeric conversion。会破坏当前强类型边界。
- 用户自定义隐式 conversion。暂不需要。
- 任意宏系统。没有稳定 AST 和 hygiene 前不要做。
- async/await。没有资源语义和 effect 模型前很容易形成二次设计债。
- 高阶类型、GADT、dependent types。Aurex 当前目标不需要。

## 工业语言启发

Rust 的启发：

- 资源语义、borrow、trait、module、unsafe island 是一个整体，不是单个语法糖。
- `unsafe` 不是失败，而是系统语言必须有的受控边界。
- `match` 和 ADT 是错误处理、状态机、编译器前端的核心表达。
- 但完整 Rust borrow checker 一次性照搬成本很高，Aurex 当前不应推进 move-only / Drop；应先冻结 `unsafe`、ADT、slice/string 和 safe reference 方向。

Zig 的启发：

- `defer`、显式错误路径、`comptime` 和透明 ABI 对系统语言很有价值。
- Zig 的“少语法、显式成本”很适合 Aurex 当前路线。
- 但 Aurex 已经选择 `str`、泛型 ADT 和显式低层边界，不能退回“所有文本都是 `[]u8`”。

Swift 的启发：

- API 设计必须关注调用点可读性。
- protocol / extension / method surface 对大型库层很重要。
- Swift 的 noncopyable types 可作为后续资源语义研究材料。
- 但 Swift 的高度抽象和 Unicode 默认语义不应原样进入 Aurex core。

Go 的启发：

- 小语法、明确控制流、`defer`、简单工具链很强。
- 但 Go 的 error-by-convention 和字符串任意 bytes 模型不适合 Aurex 的类型安全目标。

Kotlin / TypeScript / C# / Scala 的启发：

- null-safety、union / narrowing、sealed ADT、pattern matching、extension method 和 protocol/typeclass 风格抽象，是现代语言可读性的关键。
- 这些语言证明“可表达状态空间”的语法比传统 class hierarchy 更重要。
- Aurex 应吸收 ADT + match + trait，不应优先复制 OOP 继承树。

C++ 的启发：

- RAII、模板、concepts、zero-cost abstraction 都重要，但 RAII 对应的资源模型不进入当前阶段。
- 但 C++ 同时也证明：隐式转换、重载、模板错误、历史 ABI 和多范式叠加会让语言复杂度长期不可控。
- Aurex 应学习 concepts / RAII，不学习隐式复杂性。

## 学术与前沿研究启发

RustBelt / Oxide / Stacked Borrows 给 Aurex 的提醒：

- 如果语言要同时提供 unsafe、raw pointer、优化器和安全库层，就必须认真定义 aliasing、ownership 和 unsafe 封装边界。
- borrow checker 不是单纯 parser 特性，它是类型系统、MIR/AIR、optimizer 合约和安全库层 unsafe 代码共同组成的系统。
- Aurex 在引入 borrow 前，应先把 `unsafe`、safe reference 方向和 AIR place/lvalue 模型固定；Drop / move-only 继续暂缓。

Koka 和 algebraic effects 给 Aurex 的提醒：

- effect typing 很适合表达 IO、state、exception、async、resource 等能力。
- 但 effect system 会显著改变函数类型、泛型和 inference。Aurex 当前更应该先把 `Result` / ADT / pattern / `unsafe` 做稳。
- 可以先在文档层面记录未来能力需求，不要现在暴露复杂 effect syntax。

## 推荐的核心语法方向

下面是一套较稳的 Aurex 未来核心语法方向。示例只说明语言表面，不表示当前存在 `std`、`core` 或任何 bundled 标准库：

```aurex
module app.main;

pub enum Option[T] {
    some(T),
    none,
}

pub enum Result[T, E] {
    ok(T),
    err(E),
}

pub trait Eq {
    fn eq(self: &Self, other: &Self) -> bool;
}

pub struct Vec[T] {
    // storage layout intentionally omitted in this syntax sketch.
}

pub struct Buffer {
    bytes: Vec[u8];
}

impl Buffer {
    pub fn new() -> Buffer {
        return Buffer { bytes: Vec[u8] { } };
    }

    pub fn len(self: &Buffer) -> usize {
        return self.bytes.len();
    }
}

pub fn find[T](items: &Vec[T], needle: &T) -> Option[usize]
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
- 资源释放后续专题处理，不在当前核心语法目标内。
- `Result` / `?` 负责错误传播。
- `unsafe` 负责低层逃生口。

## 近期最应该做的 11 件事

1. 把本文拆成正式 language reference 和 design notes 两份文档。
2. 给 parser 写 EBNF，并用 tests 锁住每类语法。
3. 加 `unsafe` block / `unsafe fn` 的语法和诊断框架。
4. 冻结浮点字面量后缀策略。
5. 补 struct pattern / nested pattern，避免 ADT 和 struct 只能靠字段访问手动拆解。
6. slice type/expression 已落地；冻结 `str` 边界并明确与普通 slice 的关系。
7. function pointer / function type 已落地；后续补充 closure/lambda 捕获专题。
8. 设计并实现最小非资源类 `where` 约束。
9. 设计 `&T` / `&mut T`，先文档冻结。
10. 增强 `match`：更精确 structural exhaustiveness、guard 和 unreachable 诊断。
11. 把 `Copy` / `Drop` / destructor / move-out 明确留给后续资源语义专题。

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
