# Aurex 基础语法地基评估

日期：2026-05-10
状态：M2 基础语法专项审计

本文只讨论 Aurex 当前最基础的语法地基，不讨论高级语法和大型语义系统。

完整语言语法、已支持高级能力、未完成特性和 M2 优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。本文只保留基础语法专项评估。

M2 背景：M1 阶段已经舍弃。M1 的失败点在于标准库、host support、构建工具样例、自举实验和语言核心同时扩张，基础语法还没有稳定就开始承载过多目标。M2 的任务是先把语言地基重新做稳，再考虑恢复标准库和自举路线。

纳入范围：

- 词法、标识符、关键字、注释。
- 整数、字符串、byte、bool、null 等基础字面量。
- 模块、导入、可见性。
- 顶层声明、函数声明、局部变量声明。
- 基础类型语法、指针类型、数组类型。
- 普通表达式、运算符、调用、字段、索引、struct literal。
- 语句、赋值、块、`if`、`while`、`for`、`return`、`break`、`continue`、`defer`。
- 基础 const initializer。

暂不纳入范围：

- trait / interface / protocol。
- `where` / 泛型约束。
- borrow checker、lifetime、安全引用。
- `unsafe` 体系。
- Drop / destructor / 自动资源释放。
- class / inheritance / dynamic dispatch。
- macro、async、effect、comptime。
- pattern matching 的深层增强。

## 当前基础语法状态

当前基础语法已经足够写小型系统程序：

```aurex
module app.main;

import common.result as result;

pub const OK: i32 = 0;

struct Point {
    x: i32;
    y: i32;
}

fn distance2(p: *const Point) -> i32 {
    return p.x * p.x + p.y * p.y;
}

fn main() -> i32 {
    var total: i32 = 0;
    for i in range(10) {
        total = total + i;
    }
    return total;
}
```

代码层面的基础事实：

- lexer 已有 `integer_literal` 和 `float_literal`：`include/aurex/syntax/token.hpp`、`src/lex/lexer.cpp`。
- `f32` / `f64` 类型、浮点字面量和浮点运算已经存在。浮点字面量当前支持 `1.0`、`1e3`、`1.0e-3` 这类基础形式。
- statement `if` 和 expression `if` 都支持 `else if`。
- 普通 block statement 和 block expression 已共用 block body 解析规则；expression context 额外要求 final expression。
- parser 能生成任意 expression statement；sema 当前只允许函数调用和 `?` try expression 作为 expression statement。
- 顶层 item 和 struct field 默认 public，import 默认 private。
- 整数字面量允许 `_`，规则已收紧为只能出现在两个合法数字之间。
- `for i in range(end)` / `for i in range(start, end)` / `for i in range(start, end, step)` 已作为基础计数循环补齐；当前只支持整数半开区间和显式 step，不支持容器迭代。

## 总体判断

Aurex 的基础语法方向是对的：花括号、显式类型、`fn`、`let` / `var`、无隐式数值转换、表达式优先级清楚、C-style `for`、基础 `for i in range(...)`、`defer`、模块限定 `alias::item`，这些都适合系统语言。

现在最主要的问题不是“缺高级特性”，而是基础语法里有几处不一致：

- 有表达式块，但普通块和表达式块不是同一套语法。
- const initializer 已补齐基础纯标量运算，但还没有函数调用、控制流表达式或完整 comptime。
- 目前没有 `unsafe` 边界，unchecked 字符串、raw pointer、bit cast 等破坏不变量的操作仍在普通表达式层。
- `str` 已是语言内建，但还缺 slice/UTF-8 boundary/check-vs-unchecked 的最小安全 API。
- 可见性默认值对长期模块 API 不友好。
- enum 语法过早绑定 C ABI 形态，作为 ADT 过重。
- 指针语法在基础层承担了 safe borrow、raw pointer、method receiver 和 FFI 多种角色。
- 错误处理 `?` 已经可用，但仍依赖“形状约定”，缺正式标准核心定义和约束表达。
- 泛型基础语法只能列类型参数，不能表达能力约束。

这些都是地基问题，应该先修。否则继续往上加 trait、borrow、class、macro，只会把不一致扩大。

## M2 和 M1 的分界

M2 不应该继承 M1 的“先让完整系统样例跑起来，再回头补语言规则”的路线。基础语法阶段的正确验收标准应改成：

- 每条语法规则能用一小段自包含 `.ax` 样例解释。
- parser、AST、sema、IR lowering 对同一结构没有两套不同规则。
- 正例和负例能证明语言规则，而不是证明标准库包装或 host support 是否存在。
- 所有跨模块 API、资源转移、错误传播和 unsafe 操作都有明确语法或明确暂缓。
- 文档说法必须和当前仓库一致：当前没有 `std/`，没有 selfhost，M1 只是历史输入。

M1 的可取经验是：Aurex 确实需要 `str`、Result/Option、容器、路径、文件、构建工具和自举。但这些不能作为 M2 基础语法的前提。M2 应先回答“语言本身如何表达这些东西”。

## 现代语言对比下的基础准则

只看基础语法，现代工业语言有几个稳定共识：

1. 字面量体系要覆盖所有基础标量类型。C、Go、Rust、Zig、Swift 都不会提供 float 类型却不给 float literal。
2. block 最好是一种统一语法。Rust、Zig、Swift 这类 expression-oriented 语言都尽量避免“statement block 一套规则、expression block 另一套规则”。
3. 可见性应默认保守。Rust 默认 private，Swift 默认 internal 但跨 module API 仍需显式 public，Go 用命名约定区分 exported。Aurex 当前默认 public 偏危险。
4. 语法糖要少，但基础人体工程学不能缺。复合赋值、浮点字面量、稳定的 trailing comma、清楚的 `else if`，属于基础可用性，不是高级语法。
5. parser、sema、formatter、IDE 要能共享同一套语法模型。前端研究和现代工具链实践都强调语法树稳定、错误恢复稳定、语义阶段不要反向定义语法。

因此 Aurex 现在不需要追逐复杂高级特性，应该先把基础语法改成“规则少、例外少、可预测”。

## 扩大参照后的第一优先级

只和 Rust、Go、Zig、Kotlin、C++ 对比还不够。把 Swift、C#、TypeScript/JavaScript、Python、Dart、Scala、OCaml/F#、Haskell、Julia、Nim、D、V，以及 ML/ADT、pattern matching exhaustiveness、null safety、type soundness、unsafe boundary 相关研究一起纳入后，基础语法的缺口排序应调整为：先补现代语言共同证明过的基础表达能力，再进入 trait、borrow checker、macro、async、std 或 iterator protocol。

第一优先级是四件事：

1. 最小 `unsafe` block / `unsafe fn`

   raw pointer 解引用、`pcast`、`bit_cast`、`ptr_from_addr`、`str_from_bytes_unchecked` 当前都能在普通表达式里出现。它们会破坏 pointer validity、layout、aliasing、UTF-8 等语言不变量。M2 应先加语法和诊断边界：

   ```aurex
   unsafe {
       let p = ptr_from_addr(*mut Header, address);
       (*p).len = 4;
   }

   unsafe fn from_raw(data: *const u8, len: usize) -> str {
       return str_from_bytes_unchecked(data, len);
   }
   ```

2. ADT-first enum

   当前 enum 强制 base type 和 discriminant，适合 C-like repr，不适合作为主力 ADT。普通状态空间、`Option`、`Result`、compiler AST token/payload 应采用轻量 sum type：

   ```aurex
   enum Option<T> {
       some(T),
       none,
   }

   enum Result<T, E> {
       ok(T),
       err(E),
   }
   ```

   显式 layout 保留给 ABI/repr enum：

   ```aurex
   enum Status: u8 {
       ok = 0,
       err = 1,
   }
   ```

3. default private

   顶层 item 和 struct field 默认 public 会让 public surface 无意扩大。现代模块系统普遍要求导出边界清楚。M2 应把长期方向改为默认 private、显式 `pub`：

   ```aurex
   fn helper() -> i32 { return 1; }
   pub fn api() -> i32 { return helper(); }

   struct User {
       id: i32;
       pub name: str;
   }
   ```

4. array literal / repeat literal

   Aurex 已有数组类型 `[N]T`，但没有数组值语法。固定长度数组不依赖 std 容器，也不等同于 iterator protocol，应直接补齐：

   ```aurex
   let bytes: [4]u8 = [1, 2, 3, 4];
   let zeroes: [128]u8 = [0; 128];
   let matrix: [2][2]i32 = [[1, 2], [3, 4]];
   ```

第二优先级再补 slice type/expression、raw/multiline/bytes string、Unicode scalar `char`、function pointer / function type、tuple/destructuring、`if let` / `let ... else` / struct pattern。完整 closure 捕获、trait/interface/protocol、borrow checker、macro、async、package manager 和 std/core 恢复继续暂缓。

## P0 已补：浮点类型没有浮点字面量

补齐前：

```aurex
let zero: f64 = cast(f64, 0);
let nan: f64 = zero / zero;
```

原问题：

- `f32` / `f64` 是内建基础类型，但没有 `1.0`、`1e-3` 这类字面量。
- 这会让 primitive matrix 很不完整。
- 用户会被迫用 `cast` 写最普通的浮点值，语法噪声过高。
- 后端和 sema 已经有浮点路径，缺的是词法、AST、常量/类型推断规则。

已采用的基础形态：

```aurex
let a: f64 = 1.0;
let b: f64 = 1e-3;
let c: f32 = 0.5;
```

第一阶段不做复杂后缀。当前规则：

- `1.0`、`.5`、`1.` 三者里先只接受 `1.0`，避免和未来 range / field / method 语法冲突。
- 接受 exponent：`1e3`、`1.0e-3`。
- 没有期望类型时默认 `f64`。
- 有期望类型 `f32` / `f64` 时按期望类型检查。
- 暂不支持 `1.0f32` / `1.0_f32` 后缀，等整数后缀策略一起设计。

状态：已补。基础标量类型现在有对应 literal。

## P0 已补：整数字面量分隔符规则过松

现状：

lexer 扫描 integer 时把 `_` 当作可重复的 digit separator，parser 解析时直接跳过 `_`。

这意味着下面这类形式容易被接受或产生不够清楚的诊断：

```aurex
let a: i32 = 1_;
let b: i32 = 1__2;
let c: i32 = 0x_FF;
let d: i32 = 0b_1010;
```

问题：

- 数字分隔符是可读性工具，不应该变成随便放的噪声字符。
- 过宽的词法会让 formatter、diagnostic、未来 suffix 规则更难定。
- 如果以后加类型后缀，`1_u32`、`1__u32` 这类边界会更麻烦。

已采用严格规则：

```text
decimal = digit ( "_" ? digit )*
hex     = "0x" hex_digit ( "_" ? hex_digit )*
binary  = "0b" binary_digit ( "_" ? binary_digit )*
```

含义：

- `_` 只能出现在两个合法数字之间。
- 不能开头、不能结尾、不能连续。
- `0x` / `0b` 后必须立刻有合法数字。
- 暂不允许 `0x_FF`，除非以后明确选择 Go 风格。

状态：已补。越早收紧越好，后面放宽比收紧容易。

## P0 已补：block statement 和 block expression 主体不统一

补齐前：

普通 block：

```aurex
{
    let x = 1;
    if x > 0 {
        return x;
    }
}
```

block expression：

```aurex
let y = {
    let x = 1;
    x + 1
};
```

原问题：

- `parse_block()` 和 `parse_block_expr()` 是两套入口。
- block expression 里目前手写了一部分 statement 支持，和普通 block 不是完全同构。
- expression block 目前只直接接受 `let` / `var` / `defer` 和表达式/赋值，不能像普通 block 一样自然承载 `if` statement、`for`、`while`、`return`、`break`、`continue` 等完整 statement 形态。
- 这种差异会持续制造边界问题：哪些 statement 能在 expression block 里出现，哪些不能，用户很难记。
- 现代 expression-oriented 语言通常把 block 看成“一串 statement 加一个可选 tail expression”。

已采用的 M2 规则：

```text
block = "{" stmt* tail_expr? "}"
tail_expr = expr without trailing ";"
```

语义规则：

- block body 解析已经共用一套 statement 规则。
- 在 expression context 中，block 必须有 tail expression。
- tail expression 类型不能是 `void`，除非以后明确允许 `void` expression。
- expression block 现在可以包含普通 `if`、`while`、`for`、嵌套 block、`defer`、`return`、`break`、`continue`。
- `return`、`break`、`continue` 的合法性由语义阶段判断。
- 如果 block body 已经由 `return` / `break` / `continue` 保证不 fallthrough，后面的 tail expression 会诊断为不可达。
- 普通函数体和普通 statement block 暂不引入“最后表达式等于隐式 return”的语义；函数返回仍使用显式 `return`。

示例：

```aurex
let value = {
    var total = 0;
    if total == 0 {
        total = 1;
    }
    while total < 3 {
        total += 1;
    }
    total
};
```

状态：已补。后续 `unsafe {}`、`defer`、Drop、borrow scope、`if let`、`let ... else` 可以基于同一套 block body 规则扩展，不需要再为 expression block 单独补 statement 子集。

## P0 已补：`if` 表达式不支持 `else if`

补齐前：

statement 可写：

```aurex
if a {
    return 1;
} else if b {
    return 2;
} else {
    return 3;
}
```

expression 只能写：

```aurex
let value = if a { 1 } else { 2 };
```

不能自然写：

```aurex
let value = if a {
    1
} else if b {
    2
} else {
    3
};
```

原问题：

- statement 和 expression 规则不一致。
- 用户会被迫嵌套一层 block expression，语法很别扭。
- `if` expression 是基础表达式，不是高级语法，应该和 `if` statement 的链式条件一致。

已采用语法：

```text
if_expr = "if" expr block_expr "else" (if_expr | block_expr)
```

语义：

- 所有分支必须能产生同一类型。
- 最终必须有 `else`。
- `else if` 链只是右结合的 nested `if_expr`。

状态：已补。

## P0 已补：expression statement 的规则前后不一致

现状：

parser 能解析：

```aurex
foo();
bar()?;
x + y;
```

补齐前 sema 中 expression statement 只允许 `call`，否则报：

```text
expression statement must be a function call
```

问题：

- 语法阶段和语义阶段边界不清。
- `bar()?;` 这种“调用并传播错误”的写法在系统语言里很基础，但 AST kind 是 `try_expr`，会被当前规则拒绝。
- 如果只允许 call，用户会奇怪为什么 `foo();` 可以、`foo()?;` 不可以。
- 如果允许所有 expression statement，`x + y;` 这类无效代码又容易隐藏 bug。

建议选择一个明确规则，不要维持半状态。

已采用规则：

```text
expr_stmt =
    call_expr ";"
  | try_expr ";"
```

并在 sema 中细化：

- `foo();` 允许。
- `foo()?;` 允许。
- `object.method()?;` 允许。
- `x + y;` 仍然诊断。
- M1 的 `move(x);` 不再是 M2 当前语法；未来如果需要“显式丢弃/销毁”，应重新设计独立语法。

状态：已补。普通无效表达式语句仍然诊断。

## P0 已补：const initializer 的基础纯标量运算

现状允许部分纯表达式：

- literal。
- const 引用。
- struct literal。
- enum case。
- `!`、`-`、`~`。
- `+`、`-`、`*`、`/`、`%`、`<<`、`>>`、比较、相等、bitwise `&` / `^` / `|`、logical `&&` / `||`。
- `cast`、`pcast`、`bit_cast`、`ptr_addr`、`ptr_from_addr`。
- `size_of`、`align_of`。

当前仍不允许：

- 函数调用。
- `if` / block / match / `?`。
- `str_data` / `str_byte_len` / `str_from_bytes_unchecked`。

已补内容：

- `/`、`%`、`<<`、`>>`、`&&`、`||` 已进入 const initializer 允许集合。
- IR lowering 在全局常量 initializer 中把逻辑运算降成普通 bool constant binary，避免走运行时短路 CFG。
- 正向样例覆盖 division、modulo、shift、logical const；负向样例保留函数调用仍不能作为 const initializer。

```aurex
const PAGE_SIZE: usize = 1 << 12;
const MASK: u32 = 0xff << 8;
const HALF: i32 = 100 / 2;
const HAS_FLAG: bool = (MASK != 0) && true;
```

现有 literal 诊断继续覆盖：

- integer division by zero。
- integer modulo by zero。
- signed min / -1 overflow。
- shift amount negative。
- shift amount out of range。

状态：已补。

M2 判断：这是语言地基问题，不是优化器问题。系统语言的常量计算至少要覆盖 page size、alignment、mask、flag、tag 和数组长度推导所需的纯标量表达式。当前已补齐这层纯标量能力，避免用户把本该静态化的值搬到运行期。

第一阶段没有引入完整 comptime，也不允许函数调用。后续如果要让 `false && BAD` 这类表达式按常量短路跳过未求值分支，需要单独实现值级 const evaluator；这不属于本次基础语法补齐。

## P1 已补：基础赋值语法太薄

现状：

```aurex
i = i + 1;
flags = flags | mask;
```

已支持：

```aurex
i += 1;
flags |= mask;
```

M2 明确不支持自增自减语法：

- `x++`、`++x`、`x--`、`--x` 都是语法错误。
- 计数更新统一写 `x += 1` / `x -= 1`。
- 词法层保留 `++` / `--` 为独立 token，只用于诊断和防止 `--x` 被拆成两个一元负号。
- 设计取向跟 Rust/Zig 更接近，不引入 C/C++/Java/JavaScript 的更新表达式值语义。

```text
+= -= *= /= %= <<= >>= &= ^= |=
```

语义：

- 左侧只求值一次。
- 左侧必须 writable place。
- 类型规则等价于 `lhs = lhs op rhs`，但诊断应落在 compound assignment 本身。

状态：已补。

## P1 缺陷：默认 public 不适合作为长期模块地基

现状：

- 顶层 item 默认 public。
- struct field 默认 public。
- import 默认 private。

问题：

- 默认 public 会让 public surface 无意扩大。
- 后续包管理、文档生成、API 稳定性都会受影响。
- 现代模块系统通常要求 public API 显式标注。

建议：

```aurex
// 推荐长期规则
fn helper() -> i32 { ... }       // private
pub fn api() -> i32 { ... }      // public

struct User {
    id: i32;                     // private
    pub name: str;               // public
}
```

迁移策略：

1. 当前阶段保留默认 public，但文档标记为过渡行为。
2. 增加 warning：跨模块可见但未写 `pub` 的 item/field。
3. M2 收口期切换到 default private。

优先级：中高。越晚改，样例和库越难迁移。

M1 失败的一个信号就是 API surface 没有被语言机制约束住。M2 应把“默认 private，显式 public”作为长期方向。当前样例可以有迁移成本，但这个成本越早支付越小。

## P1 缺陷：函数返回类型推导边界需要收紧

现状：

普通非 C、非 prototype、非泛型函数允许省略返回类型：

```aurex
fn answer() {
    return 42;
}
```

问题：

- 顶层函数是模块 API 的一部分，返回类型推导会让 API surface 不稳定。
- 递归函数已经需要特殊诊断。
- 工业系统语言通常倾向显式函数签名，尤其是 public API。

建议：

- `pub fn` 必须显式返回类型。
- `export c fn`、`extern c fn`、prototype 已经必须显式，保持。
- private helper 可以暂时允许推导，但文档要明确这是便利，不是推荐 API 风格。
- 如果以后引入局部函数，再考虑更宽的推导规则。

优先级：中。

## P1 缺陷：namespace 符号需要冻结

现状：

```aurex
module common.result;
import common.result as result;
let x = result::ok(1);
```

模块路径用 `.`，item 限定用 `::`。

这个设计可以保留，但必须冻结，不要同时引入多套写法。

建议：

- `.` 只用于 module path、field access、method access。
- `::` 只用于 import alias / module alias 下的 item、type、const、function、enum constructor。
- 不加 `common.result::Type` 这种混合长路径表达式，强制通过 `import ... as alias` 控制局部命名。
- `c`、`str` 这类关键字是否允许作为 path segment 要明确定义。当前 parser 特许 `c` 和 `str`，这属于历史兼容，不宜继续扩大。

优先级：中。

## P1 缺陷：enum 语法过重，ADT 表达能力不足

现状：

```aurex
enum Option<T>: u8 {
    some(T) = 1,
    none = 2,
}
```

问题：

- 每个 enum 都必须显式 base type。
- 每个 case 都必须显式 discriminant。
- payload 目前是单个 payload type，不支持多字段 payload。
- 这很适合 ABI 稳定的 C-like enum，但不适合作为编译器 AST、Result/Option、状态机、token payload 等主力 ADT。

长期建议：

```aurex
enum Option<T> {
    some(T),
    none,
}

enum Result<T, E> {
    ok(T),
    err(E),
}

enum Token {
    identifier(str),
    integer(str),
    keyword(str),
}

enum CStatus: u8 {
    ok = 0,
    err = 1,
}
```

语义分层：

- 不写 base type 的 enum 是语言 ADT，layout 由编译器决定，不承诺 C ABI。
- 写 base type 和 discriminant 的 enum 是 ABI/repr enum。
- payload enum 可以有自动 tag；只有需要 ABI 稳定时才要求显式 tag。

优先级：中高。M2 如果继续要求所有 ADT 都写 C enum 形式，后续 Result/Option、AST 和 pattern matching 会一直显得笨重。

## P1 缺陷：泛型参数只有名字，没有约束语法

现状：

```aurex
fn identity<T>(value: T) -> T {
    return value;
}
```

当前能表达“有一个类型参数 T”，但不能表达：

```aurex
fn clone<T>(value: T) -> T where T: Copy
fn destroy<T>(value: *mut T) -> void where T: Drop
fn contains<K>(key: K) -> bool where K: Eq
```

问题：

- 当前 copyability、destructor、Result/Option payload 消费等规则无法通过函数签名表达。
- 泛型函数的错误常常只能在实例化体内报出，API 用户看不到约束。
- 恢复核心库后，`Vec<T>`、`Map<K,V>`、`Result<T,E>`、`Option<T>` 都需要 copy/drop/eq/hash 这类基础约束。

建议 M2 先设计最小 capability 语法，不急着实现完整 trait：

```aurex
fn get<T>(items: *const Vec<T>, index: usize) -> T
where T: Copy

fn destroy_all<T>(items: *mut Vec<T>) -> void
where T: Drop
```

优先级：中高。没有约束语法，泛型越多，诊断越晚，库设计越依赖文档约定。

## P1 缺陷：`unsafe` 边界缺失

现状这些操作是普通表达式：

```aurex
pcast(*mut T, p)
bit_cast(U, value)
ptr_from_addr(*mut T, address)
str_from_bytes_unchecked(data, len)
*ptr
```

问题：

- raw pointer 解引用、指针整数转换、unchecked UTF-8、bit cast 都可能破坏语言不变量。
- 这些能力如果长期保持 safe surface，后续优化器、borrow checker 和核心库都会缺少可信边界。
- M1 的标准库经验已经说明：底层 escape hatch 必须存在，但必须被隔离。

建议语法：

```aurex
unsafe {
    let p = ptr_from_addr(*mut Header, address);
    return (*p).len;
}

unsafe fn from_raw(data: *const u8, len: usize) -> str {
    return str_from_bytes_unchecked(data, len);
}
```

第一阶段可以只做语法和诊断框架：

- unsafe-only 内建在 safe context 下报警。
- unsafe block 允许这些内建。
- unsafe fn 的调用也必须在 unsafe context。
- `str_from_bytes_unchecked` 必须被纳入 unsafe-only 清单，避免 safe 代码直接伪造 `str` 的 UTF-8 不变量。

优先级：中高。它是 borrow/drop/optimizer 合约的前置地基。

## P1 缺陷：`str` 已是内建，但安全 API 边界还没形成

现状：

- 普通字符串字面量类型是 `str`。
- `str` 在 LLVM 后端降低为 `{ ptr, usize }`。
- `size_of(str)` / `align_of(str)` 已有 64-bit ABI 测试。
- 编译器内建已有 `str_data`、`str_byte_len`、`str_from_bytes_unchecked`。

问题：

- `str_from_bytes_unchecked` 能直接构造 `str`，但当前没有 `unsafe` 语法约束。
- 没有语言核心层面的 `slice_bytes_checked` / UTF-8 boundary 规则，恢复标准库后容易再次把安全边界推给库约定。
- `str_data` 暴露 raw pointer，长期需要和 borrow/lifetime/FFI 边界一起解释。
- `c"..."` 仍是 `*const u8`，这是合理 FFI 过渡，但不能变成普通文本 API 的替代品。

M2 最小方向：

```aurex
let text: str = "hello";
let n: usize = str_byte_len(text);

unsafe {
    let borrowed = str_from_bytes_unchecked(data, n);
}
```

建议：

- `str` 继续作为语言内建，不依赖标准库存在。
- `str_byte_len` 可以保持 O(1) 基础观察能力。
- checked UTF-8 构造和 checked slice 可以先设计语义，再决定是 compiler builtin、core intrinsic 还是未来 `core.str` API。
- unchecked 构造必须进入 `unsafe`。
- `c"..."` 只用于 FFI，不能隐式转 `str`。

优先级：中高。字符串是诊断、模块名、路径显示、未来 std API 的共同地基；如果 `str` 的 safe/unsafe 边界不先定，M1 的标准库问题会在恢复 `String`、`Path`、`CString` 时再次出现。

## P1 缺陷：raw pointer 和 safe borrow 没有分层

现状：

```aurex
fn len(self: *const Buffer) -> usize
fn push(self: *mut Buffer, value: u8) -> void
```

`*const` / `*mut` 现在同时承担：

- C FFI raw pointer。
- method receiver。
- address-of 结果。
- 未来 borrow 语义。
- buffer/span 类数据结构的内部引用。

问题：

- raw pointer 没有生命周期、别名、有效性保证。
- 如果 method receiver 长期使用 raw pointer，safe API 和 unsafe API 的边界会混在一起。
- borrow checker 很难在这种语法上自然落地。

长期建议：

```aurex
fn len(self: &Buffer) -> usize
fn push(self: &mut Buffer, value: u8) -> void

extern c {
    fn write(fd: i32, data: *const u8, len: usize) -> isize;
}
```

分层原则：

- `&T` / `&mut T` 是 safe borrow。
- `*const T` / `*mut T` 是 raw pointer，主要用于 FFI 和 unsafe 内部实现。
- `&place` 应产生 safe borrow，而不是默认产生 raw pointer；raw address-of 可以另设 unsafe 操作。

优先级：中。实现可以晚于 M2 语法冻结，但文档必须明确方向。

## P1 已补：标识符规则应显式 ASCII，而不是 locale 相关

历史问题：

lexer 用 `std::isalpha` / `std::isalnum` 判断标识符字符。

问题：

- 这在 C/C++ 里可能受 locale 影响。
- 文档上说是 ASCII 风格，但实现上最好用显式范围保证一致。
- 编译器前端最忌讳“不同机器 locale 下词法行为不同”。

建议：

```text
identifier_start = "A".."Z" | "a".."z" | "_"
identifier_continue = identifier_start | "0".."9"
```

当前实现已经使用显式 ASCII char class table，不再依赖 locale。文档仍应固定这个规则，避免未来误引入平台相关行为。

暂不建议现在加入 Unicode identifier。Unicode identifier 需要 normalization、confusable、tooling、diagnostic 策略，不是 M2 基础地基必须项。

状态：已补实现，继续保留为语法规范要求。

## P1 已补：trailing separator 策略统一

当前冻结规则：

- 圆括号列表允许 trailing comma：函数参数、调用参数、ABI 属性等。
- 角括号列表允许 trailing comma：generic parameter、type argument。
- comma 分隔的花括号列表允许 trailing comma，但不强制最后一个 comma：struct literal field、enum case、match arm。
- struct field declaration 继续使用 `;`，按 statement-like 风格保留最后一个 `;`。

动机：

- 用户需要记很多小规则。
- formatter 和 diff 友好性依赖稳定 trailing separator。
- enum case 和 match arm 的最后一项不应为了 parser 方便而强制 comma。

示例：

```aurex
struct Pair<T, U,> {
    left: T;
    right: U;
}

enum Choice<T, E>: u8 {
    ok = 1,
    err(E) = 2
}

let pair = Pair<i32, bool> {
    left: 1,
    right: false,
};

let result = match value {
    .ok => 0,
    .err(flag) => if flag { 1 } else { 2 }
};
```

状态：已补。parser 单测覆盖统一策略，positive sample `core/trailing_separator_policy.ax` 覆盖 sema / IR / LLVM 路径。

## P2 可暂缓：shadowing 策略

现状：

当前不允许 local / param shadowing，内层作用域也不能重名。

这个规则简单、安全，早期实现成本低，但长期会影响可写性：

```aurex
let value = read();
let value = normalize(value); // 当前不允许
```

现代语言分歧很大：

- Rust、Swift 允许 shadowing，适合表达值转换阶段。
- Go 不允许同一作用域重复声明，但内层可以遮蔽。
- C/C++ 允许内层遮蔽，但容易出 bug。

建议暂不急着改。等基础 block 统一后再决策：

- 至少可以考虑“只允许内层 scope shadow 外层，不允许同一 scope 重名”。
- 如果引入 move-only / borrow 更强规则，shadowing 需要和所有权诊断一起设计。

优先级：低到中。不是当前最卡的基础缺陷。

## P2 可暂缓：`char` 字面量和 raw string

当前已有：

```aurex
"text"      // str
c"text"     // *const u8
b'a'        // u8
```

还没有：

```aurex
'x'         // char?
r"raw"      // raw string?
b"bytes"    // bytes?
```

建议：

- 不要把 `'x'` 设计成 C char。Aurex 已经有 `b'x' -> u8`。
- 如果加 `char`，应表示 Unicode scalar value。
- raw string 可以晚一点加，等 `str` / bytes / C string 边界完全冻结。

优先级：低。不是 M2 必须。

## P2 可暂缓：模块 package 语法

当前 module/import 已经能支撑多文件样例：

```aurex
module common.result;
import common.status as status;
```

但还没有：

- package manifest。
- package root。
- module visibility surface dump。
- selective import。
- glob import。
- versioned dependency。

M2 不建议马上做包管理。原因是 package 设计会反向影响 module path、visibility、public API 和 std/core 分层。现在更应该先冻结：

- 文件路径如何映射 module path。
- import alias 是否是唯一推荐限定方式。
- default private 迁移。
- public re-export 规则。

优先级：低到中。

## 建议执行顺序

第一批最值得马上做：

1. 增加浮点字面量。已补。
2. 收紧整数字面量 `_` 规则。已补。
3. 统一 block statement / block expression 语法。已补。
4. 让 `if` expression 支持 `else if`。已补。
5. 明确 expression statement 规则，至少允许 `foo()?;`。已补。
6. 为 `unsafe` block / `unsafe fn` 定最小语法和 unsafe-only 诊断清单。
7. 给 ADT-first enum 语法写正式草案，并保留显式 C-like/repr enum。
8. 设计 default private 迁移。
9. 实现 array literal / repeat literal。

第二批再做：

1. 固定 `str` 的 safe/unsafe API 边界。
2. 设计 slice type/expression。
3. 设计 raw/multiline/bytes string 和 Unicode scalar `char`。
4. 设计 function pointer / function type。
5. 收紧 public 函数返回类型推导。
6. 冻结 namespace `.` / `::` 规则。
7. 设计 capability / `where` 约束语法。
8. 设计 safe reference `&T` / `&mut T`。

第三批暂缓：

1. shadowing 策略。
2. tuple/destructuring 和 pattern 扩展。
3. 嵌套 block comment。
4. 更复杂 literal suffix。
5. package manifest 和包管理。

## 不建议现在做

为了保持基础语法稳定，当前阶段不建议把这些拉进来：

- optional semicolon。
- significant indentation。
- operator overloading。
- user-defined implicit conversion。
- lambda / closure。
- class。
- trait / where。
- borrow / lifetime。
- macro。
- async。

这些不是不好，而是它们会改变更大的语义系统。Aurex 现在最需要的是先把普通程序的词法、表达式、语句、块和模块 API 规则做稳。

## 最小目标语法草案

下面是本文建议的基础语法形态，不包含高级特性：

```aurex
module app.main;

import common.status as status;

pub const PAGE_BITS: usize = 12;
pub const PAGE_SIZE: usize = 1 << PAGE_BITS;

pub struct Counter {
    value: i32;
}

impl Counter {
    pub fn new(value: i32) -> Counter {
        return Counter { value: value };
    }

    pub fn add(self: *mut Counter, delta: i32) -> void {
        self.value += delta;
    }

    pub fn score(self: *const Counter) -> f64 {
        let base: f64 = 1.5;
        return base * cast(f64, self.value);
    }
}

pub fn classify(value: i32) -> i32 {
    return if value < 0 {
        -1
    } else if value == 0 {
        0
    } else {
        1
    };
}

pub fn run() -> i32 {
    var counter = Counter.new(0);
    for i in range(10) {
        counter.add(i);
    }
    return counter.value;
}
```

这套语法没有引入高级系统，但已经比当前基础层更完整、更现代，也更适合继续承载后续高级设计。
