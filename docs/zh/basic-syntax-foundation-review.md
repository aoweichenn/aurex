# Aurex 基础语法地基评估

日期：2026-05-08
状态：基础语法专项审计

本文只讨论 Aurex 当前最基础的语法地基，不讨论高级语法和大型语义系统。

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
    for var i: i32 = 0; i < 10; i = i + 1 {
        total = total + i;
    }
    return total;
}
```

代码层面的基础事实：

- lexer 只有 `integer_literal`，没有 `float_literal`：`include/aurex/syntax/token.hpp`、`src/lex/lexer.cpp`。
- `f32` / `f64` 类型和浮点运算已经存在，但用户只能通过 `cast(f64, 0)` 等方式构造浮点值。
- statement `if` 支持 `else if`，expression `if` 只支持 `else { ... }`。
- 普通 block statement 和 block expression 分别由两个 parser 入口处理。
- parser 能生成任意 expression statement，但 sema 又把 expression statement 限制为函数调用。
- 顶层 item 和 struct field 默认 public，import 默认 private。
- 整数字面量允许 `_`，但当前规则过宽。

## 总体判断

Aurex 的基础语法方向是对的：花括号、显式类型、`fn`、`let` / `var`、无隐式数值转换、表达式优先级清楚、C-style `for`、`defer`、模块限定 `alias::item`，这些都适合系统语言。

现在最主要的问题不是“缺高级特性”，而是基础语法里有几处不一致：

- 有类型但没有对应字面量。
- 有表达式块，但普通块和表达式块不是同一套语法。
- 有 `if` statement 的 `else if`，但没有 `if` expression 的 `else if`。
- parser 和 sema 对 expression statement 的边界不一致。
- const initializer 支持的基础运算比运行时表达式少太多。
- 可见性默认值对长期模块 API 不友好。

这些都是地基问题，应该先修。否则继续往上加 trait、borrow、class、macro，只会把不一致扩大。

## 现代语言对比下的基础准则

只看基础语法，现代工业语言有几个稳定共识：

1. 字面量体系要覆盖所有基础标量类型。C、Go、Rust、Zig、Swift 都不会提供 float 类型却不给 float literal。
2. block 最好是一种统一语法。Rust、Zig、Swift 这类 expression-oriented 语言都尽量避免“statement block 一套规则、expression block 另一套规则”。
3. 可见性应默认保守。Rust 默认 private，Swift 默认 internal 但跨 module API 仍需显式 public，Go 用命名约定区分 exported。Aurex 当前默认 public 偏危险。
4. 语法糖要少，但基础人体工程学不能缺。复合赋值、浮点字面量、稳定的 trailing comma、清楚的 `else if`，属于基础可用性，不是高级语法。
5. parser、sema、formatter、IDE 要能共享同一套语法模型。前端研究和现代工具链实践都强调语法树稳定、错误恢复稳定、语义阶段不要反向定义语法。

因此 Aurex 现在不需要追逐复杂高级特性，应该先把基础语法改成“规则少、例外少、可预测”。

## P0 缺陷：浮点类型没有浮点字面量

现状：

```aurex
let zero: f64 = cast(f64, 0);
let nan: f64 = zero / zero;
```

问题：

- `f32` / `f64` 是内建基础类型，但没有 `1.0`、`1e-3` 这类字面量。
- 这会让 primitive matrix 很不完整。
- 用户会被迫用 `cast` 写最普通的浮点值，语法噪声过高。
- 后端和 sema 已经有浮点路径，缺的是词法、AST、常量/类型推断规则。

建议：

```aurex
let a: f64 = 1.0;
let b: f64 = 1e-3;
let c: f32 = 0.5;
```

第一阶段不要急着加复杂后缀。建议规则：

- `1.0`、`.5`、`1.` 三者里先只接受 `1.0`，避免和未来 range / field / method 语法冲突。
- 接受 exponent：`1e3`、`1.0e-3`。
- 没有期望类型时默认 `f64`。
- 有期望类型 `f32` / `f64` 时按期望类型检查。
- 暂不支持 `1.0f32` / `1.0_f32` 后缀，等整数后缀策略一起设计。

优先级：最高。基础标量类型不能没有对应 literal。

## P0 缺陷：整数字面量分隔符规则过松

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

建议先定严格规则：

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

优先级：最高。越早收紧越好，后面放宽比收紧容易。

## P0 缺陷：block statement 和 block expression 不统一

现状：

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

问题：

- `parse_block()` 和 `parse_block_expr()` 是两套入口。
- block expression 里目前手写了一部分 statement 支持，和普通 block 不是完全同构。
- 这种差异会持续制造边界问题：哪些 statement 能在 expression block 里出现，哪些不能，用户很难记。
- 现代 expression-oriented 语言通常把 block 看成“一串 statement 加一个可选 tail expression”。

建议把 block 统一成一种基础语法：

```text
block = "{" stmt* tail_expr? "}"
tail_expr = expr without trailing ";"
```

语义规则：

- 在 statement context 中，block 的结果可以被忽略。
- 在 expression context 中，block 必须有 tail expression。
- tail expression 类型不能是 `void`，除非以后明确允许 `void` expression。
- `return`、`break`、`continue` 的类型问题不要在 parser 特判，交给语义阶段处理。

示例：

```aurex
let value = {
    var total = 0;
    if total == 0 {
        total = 1;
    }
    total
};
```

优先级：最高。它会简化 parser 和用户心智模型。

## P0 缺陷：`if` 表达式不支持 `else if`

现状：

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

问题：

- statement 和 expression 规则不一致。
- 用户会被迫嵌套一层 block expression，语法很别扭。
- `if` expression 是基础表达式，不是高级语法，应该和 `if` statement 的链式条件一致。

建议：

```text
if_expr = "if" expr block_expr "else" (if_expr | block_expr)
```

语义：

- 所有分支必须能产生同一类型。
- 最终必须有 `else`。
- `else if` 链只是右结合的 nested `if_expr`。

优先级：高。

## P0 缺陷：expression statement 的规则前后不一致

现状：

parser 能解析：

```aurex
foo();
bar()?;
x + y;
move(value);
```

但 sema 中 expression statement 只允许 `call`，否则报：

```text
expression statement must be a function call
```

问题：

- 语法阶段和语义阶段边界不清。
- `bar()?;` 这种“调用并传播错误”的写法在系统语言里很基础，但 AST kind 是 `try_expr`，会被当前规则拒绝。
- 如果只允许 call，用户会奇怪为什么 `foo();` 可以、`foo()?;` 不可以。
- 如果允许所有 expression statement，`x + y;` 这类无效代码又容易隐藏 bug。

建议选择一个明确规则，不要维持半状态。

推荐规则：

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
- `move(x);` 暂不作为普通 expression statement 允许，除非未来定义“显式丢弃/销毁”语法。

优先级：高。

## P0 缺陷：const initializer 的基础运算不完整

现状允许部分纯表达式：

- literal。
- const 引用。
- struct literal。
- enum case。
- `!`、`-`、`~`。
- `+`、`-`、`*`、比较、相等、bitwise `&` / `^` / `|`。
- `cast`、`ptr_cast`、`bit_cast`、`ptr_addr`、`ptr_from_addr`。
- `size_of`、`align_of`。

但不允许：

- `/`
- `%`
- `<<`
- `>>`
- `&&`
- `||`

问题：

- 这些都是基础、纯、可静态求值的标量操作。
- runtime 已经支持，部分 literal 诊断也已经存在。
- 对系统语言来说，常量 mask、alignment、flag、size 计算非常常见。

建议 const initializer 第一阶段补齐所有纯标量运算：

```aurex
const PAGE_SIZE: usize = 1 << 12;
const MASK: u32 = 0xff << 8;
const HALF: i32 = 100 / 2;
const HAS_FLAG: bool = (MASK != 0) && true;
```

诊断必须覆盖：

- integer division by zero。
- integer modulo by zero。
- signed min / -1 overflow。
- shift amount negative。
- shift amount out of range。
- bool short-circuit 的常量求值顺序。

优先级：高。

## P1 缺陷：基础赋值语法太薄

现状：

```aurex
i = i + 1;
flags = flags | mask;
```

没有：

```aurex
i += 1;
flags |= mask;
```

问题：

- 复合赋值是基础语法，不是高级特性。
- 系统代码里计数、flag、mask、pointer offset 会频繁出现。
- 如果不支持，代码会显得比 C、Rust、Zig、Go 都更啰嗦。

建议加入：

```text
+= -= *= /= %= <<= >>= &= ^= |=
```

语义：

- 左侧只求值一次。
- 左侧必须 writable place。
- 类型规则等价于 `lhs = lhs op rhs`，但诊断应落在 compound assignment 本身。
- 不建议加入 `++` / `--`。它们的前置/后置值语义容易制造歧义，现代系统语言没必要依赖它们。

优先级：中高。

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
3. M1 前切换到 default private。

优先级：中高。越晚改，样例和库越难迁移。

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

## P1 缺陷：标识符规则应显式 ASCII，而不是 locale 相关

现状：

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

暂不建议现在加入 Unicode identifier。Unicode identifier 需要 normalization、confusable、tooling、diagnostic 策略，不是 M0/M1 基础地基必须项。

优先级：中。

## P1 缺陷：trailing separator 策略要统一

现状：

- struct field declaration 用 `;`。
- enum case 用 `,`，当前每个 case 后都要求 `,`。
- parameter list、argument list、struct literal field list、match arm list 有各自处理。

问题：

- 用户需要记很多小规则。
- formatter 和 diff 友好性依赖稳定 trailing separator。

建议：

- 圆括号列表：允许 trailing comma。
- 花括号列表中如果元素用 comma 分隔：允许 trailing comma，但不要强制最后一个必须 comma。
- struct field declaration 继续用 semicolon，可以保留。
- enum case 建议允许最后一个 case 省略 comma。
- match arm 建议允许 trailing comma，并要求多行 match 使用 comma。

优先级：中。

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

优先级：低。不是 M0/M1 必须。

## 建议执行顺序

第一批最值得马上做：

1. 增加浮点字面量。
2. 收紧整数字面量 `_` 规则。
3. 统一 block statement / block expression 语法。
4. 让 `if` expression 支持 `else if`。
5. 明确 expression statement 规则，至少允许 `foo()?;`。
6. 补齐 const initializer 的纯标量运算。

第二批再做：

1. 增加 compound assignment。
2. 设计 default private 迁移。
3. 收紧 public 函数返回类型推导。
4. 冻结 namespace `.` / `::` 规则。
5. 显式 ASCII identifier 实现。
6. 统一 trailing separator。

第三批暂缓：

1. shadowing 策略。
2. `char` / raw string / bytes string。
3. 嵌套 block comment。
4. 更复杂 literal suffix。

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
    for var i: i32 = 0; i < 10; i += 1 {
        counter.add(i);
    }
    return counter.value;
}
```

这套语法没有引入高级系统，但已经比当前基础层更完整、更现代，也更适合继续承载后续高级设计。
