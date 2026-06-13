# Function / Closure Surface：把闭包字面量从 `fn` 骨架里拆出来

日期：2026-06-13
状态：语法修正优化第五手改动设计稿
关联问题：`docs/zh/m27c-syntax-ergonomics-review.md` 中的 P2 `fn` 同时承载声明、类型和 lambda

本文固定 Aurex 函数、函数类型和闭包字面量的第一阶段修正方向。核心判断：`fn` 可以继续作为“函数声明”和“薄函数指针类型”的表面，但不应该继续作为闭包字面量的起始符号。当前 `fn(x: i32) -> i32 => x + 1` 视觉上像函数声明写到一半突然变成表达式，这个形状不清爽，也让用户把 named function、function pointer 和 closure value 混在一起。

第一阶段只修闭包字面量表面，不重做 closure capture 语义，不引入 `Fn` / `FnMut` / `FnOnce` trait，不引入 heap boxed closure，也不改 `extern c fn` 函数类型。

## 当前现状

当前 `fn` 至少出现在这些位置：

```aurex
fn add(a: i32, b: i32) -> i32 { ... }      // 函数声明
fn(i32, i32) -> i32                        // 薄函数指针类型
unsafe fn(*const i32) -> i32               // unsafe 薄函数指针类型
extern c fn(*const u8, ...) -> i32         // C ABI 薄函数指针类型
fn(value: i32) -> i32 => value + 1         // lambda / closure literal
fn(value: i32) -> i32 { return value + 1; } // block body closure literal
export c fn aurex_add(...) -> i32 { ... }  // 导出 C ABI 函数定义
extern c { fn strlen(...) -> usize; }      // 外部 C ABI 函数声明
```

当前实现入口：

- `src/frontend/parse/grammar/parser_fn.cpp`
  - `parse_fn_decl()` 解析函数声明、prototype、extern function item、export function item。
- `src/frontend/parse/grammar/parser_primary.cpp`
  - `parse_primary()` 看到 `kw_fn` 会进入 `parse_lambda_expr()`。
  - `parse_lambda_expr()` 解析 `fn(params) -> Type => expr` 或 `fn(params) -> Type { ... }`。
- `src/frontend/parse/grammar/parser_type.cpp`
  - `parse_type_atom()` 看到 `kw_fn` / `kw_extern` / `kw_unsafe` 会进入 `parse_function_type()`。
  - `parse_function_type()` 解析 `fn(...) -> T`、`unsafe fn(...) -> T`、`extern c fn(...) -> T`。
- `include/aurex/frontend/syntax/ast/expr_nodes.hpp`
  - `LambdaExprPayload` 只保存 `params`、`return_type`、`body`，没有把 `fn` 作为语义数据保存。
- `src/frontend/sema/internal/expressions/sources/sema_statement_analyzer.cpp`
  - lambda 无捕获时得到 `fn(...) -> T` 薄函数值。
  - 捕获时生成匿名 environment record，不能赋给 `fn(...) -> T`。

这说明闭包改语法是低风险 surface change：AST 和 sema 不需要知道闭包是用 `fn(...)` 写的，还是用 `|...|` 写的。

## 必须拆开的三个概念

### 1. Function declaration

函数声明定义一个命名 item：

```aurex
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

它有 item 名字、visibility、generic params、where constraints、body/prototype、linkage/ABI 属性。它不是一个匿名表达式。

### 2. Function pointer type

函数类型表示薄函数指针：

```aurex
fn(i32, i32) -> i32
unsafe fn(*const i32) -> i32
extern c fn(*const u8, ...) -> i32
```

它不带捕获环境，不保存闭包状态。函数名和无捕获闭包可以作为这种值；捕获闭包不能。

### 3. Closure literal

闭包字面量是表达式：

```aurex
fn(value: i32) -> i32 => value + 1
```

它可能无捕获，也可能捕获外层值。捕获后它的值不是薄函数指针，而是编译器生成的匿名环境 record 加内部 thunk。这个语义和 `fn(...) -> T` 类型完全不是一个东西。

所以闭包字面量不应该继续使用 `fn` 起手。`fn` 起手应该让用户第一眼想到 named function 或 function pointer type，而不是 closure value。

## 当前问题

### 1. `fn(...) -> T => expr` 像声明残片

当前写法：

```aurex
let inc: fn(i32) -> i32 = fn(value: i32) -> i32 => value + 1;
```

左边 `fn(i32) -> i32` 是 function pointer type，右边 `fn(value: i32) -> i32 => ...` 是 closure expression。两边都以 `fn(` 起手，但语义完全不同。读者必须看到参数里有没有名字、后面是 `=>` 还是别的，才能判断自己看到的是类型还是表达式。

这不是简洁，是视觉欺骗。

### 2. 闭包是高频表达式，当前写法太重

即使当前 Aurex 还没有完整标准库 iterator/adapter，闭包一旦进入 map/filter/sort/callback/visitor 这类 API，就会变高频。高频表达式不该长得像小函数声明。

当前：

```aurex
let add_base = fn(next: i32) -> i32 => next + base;
```

目标：

```aurex
let add_base = |next: i32| -> i32 => next + base;
```

第一阶段仍保留显式参数类型和返回类型，所以打字量只少一点；真正收益是视觉角色明确。后续如果做 contextual closure inference，可以进一步写成：

```aurex
let add_base: fn(i32) -> i32 = |next| => next + base;
```

但 inference 是第二阶段语义工作，不能和本次 surface cleanup 混在一起。

### 3. `fn` 同时带 ABI / unsafe / extern 组合，闭包不应该继承这些期待

函数类型可以写：

```aurex
unsafe fn(*const i32) -> i32
extern c fn(*const u8, ...) -> i32
unsafe extern c fn(*const u8) -> i32
```

函数声明可以写：

```aurex
unsafe fn read_raw(...) -> i32 { ... }
export c fn aurex_add(...) -> i32 { ... }
extern c { fn strlen(...) -> usize; }
```

闭包字面量当前虽然不支持 `extern c`、`unsafe`、variadic，但因为它也以 `fn` 起手，用户自然会问：

```aurex
unsafe fn(p: *const i32) -> i32 => *p
extern c fn(x: i32) -> i32 => x
fn(args: ...) -> i32 => 0
```

这些都不应该成立。闭包是语言内部表达式，不是 ABI 边界 item。unsafe 操作应该写在闭包体里：

```aurex
|p: *const i32| -> i32 => unsafe { *p }
```

如果需要一个 `unsafe fn` 值，应定义命名 `unsafe fn`，不要让闭包字面量伪装成 unsafe function item。

### 4. Parser context 能处理，不等于用户读起来好

当前 parser 在 expression primary 位置看到 `kw_fn` 就解析 lambda；在 type 位置看到 `kw_fn` 就解析 function type；在 item 位置看到 `kw_fn` 就解析 function declaration。实现上能分开，但用户看到源码时没有 parser 的上下文栈。

语法表面应该让不同概念在第一眼就分层：

```aurex
fn name(...) -> T { ... }      // item declaration
fn(...) -> T                  // thin function pointer type
|...| -> T => expr             // closure expression
```

## 决策

### 决策 1：`fn` 保留给函数声明和函数类型

保留函数声明：

```aurex
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

pub fn exported(value: i32) -> i32 {
    return value;
}

unsafe fn read_raw(value: *const i32) -> i32 {
    return unsafe { *value };
}
```

保留函数类型：

```aurex
type BinaryOp = fn(i32, i32) -> i32;
type UnsafeRead = unsafe fn(*const i32) -> i32;
type CCallback = extern c fn(*const u8, ...) -> i32;
```

理由：

- `fn` 作为 named function 和 function pointer type 的标记足够短。
- 当前 sema、IR、ABI 和文档都已经围绕 `fn(...) -> T` 表达薄函数指针。
- function type 本身不是高频 inline 表达式，保留 `fn` 没有明显审美问题。

### 决策 2：闭包字面量迁移到 pipe 参数表

目标写法：

```aurex
|value: i32| -> i32 => value + 1

|value: i32| -> i32 {
    let local = value + 2;
    return local;
}

|| -> i32 => 1
```

旧写法：

```aurex
fn(value: i32) -> i32 => value + 1

fn(value: i32) -> i32 {
    let local = value + 2;
    return local;
}
```

迁移目标：

```aurex
|value: i32| -> i32 => value + 1

|value: i32| -> i32 {
    let local = value + 2;
    return local;
}
```

### 决策 3：第一阶段继续要求显式参数类型和返回类型

第一阶段不做 closure type inference：

```aurex
let inc: fn(i32) -> i32 = |value: i32| -> i32 => value + 1;
```

暂不接受：

```aurex
let inc: fn(i32) -> i32 = |value| => value + 1;
let inc = |value: i32| => value + 1;
```

原因：

- 当前实现已经要求 lambda 参数和返回类型显式标注。
- 只改表面可以复用 `LambdaExprPayload` 和当前 sema。
- contextual closure inference 需要 expected type 传播、参数类型占位、返回表达式约束、错误恢复和闭包 capture 诊断一起设计，不能塞进这个小修正里。

第二阶段可以单独讨论：

```aurex
let inc: fn(i32) -> i32 = |value| => value + 1;
map(values, |value| => value + 1);
```

### 决策 4：表达式体继续用 `=>`

保留：

```aurex
|x: i32| -> i32 => x + 1
```

块体保留：

```aurex
|x: i32| -> i32 {
    let y = x + 1;
    return y;
}
```

规则：

- `=> expr` 是表达式体，语义等价于返回该表达式。
- `{ ... }` 是块体，继续按函数体规则检查 `return`。
- 第一阶段不引入 closure tail expression block。

这个决定避免把“闭包表面修正”和“block 是否表达式化”绑死。后者在总审查文档里是另一个语法问题，应该单独设计。

### 决策 5：闭包字面量不支持 ABI / unsafe / variadic modifier

不支持：

```aurex
unsafe |p: *const i32| -> i32 => *p
extern c |x: i32| -> i32 => x
|args: ...| -> i32 => 0
```

如果闭包体里需要 unsafe 操作：

```aurex
|p: *const i32| -> i32 => unsafe { *p }
```

如果需要 ABI 边界或 unsafe callable item：

```aurex
unsafe fn read_raw(p: *const i32) -> i32 {
    return unsafe { *p };
}
```

## 最终表面表

| 写法 | 概念 | 是否命名 item | 是否可捕获环境 | 是否可带 ABI |
|---|---|---:|---:|---:|
| `fn name(...) -> T { ... }` | 函数声明 | 是 | 否 | 是 |
| `fn(...) -> T` | Aurex ABI 薄函数指针类型 | 否 | 否 | 否 |
| `unsafe fn(...) -> T` | unsafe 薄函数指针类型 | 否 | 否 | 否 |
| `extern c fn(...) -> T` | C ABI 薄函数指针类型 | 否 | 否 | 是 |
| `|...| -> T => expr` | 闭包字面量表达式 | 否 | 是 | 否 |
| `|...| -> T { ... }` | 闭包字面量表达式 | 否 | 是 | 否 |

示例：

```aurex
type UnaryOp = fn(i32) -> i32;

fn apply(op: UnaryOp, value: i32) -> i32 {
    return op(value);
}

fn choose(flag: bool) -> UnaryOp {
    if flag {
        return |value: i32| -> i32 => value + 1;
    }
    return |value: i32| -> i32 {
        return value + 2;
    };
}

fn direct_capture(value: i32) -> i32 {
    let base: i32 = 40;
    let add_base = |next: i32| -> i32 => next + base;
    return add_base(value);
}
```

## Parser 规则

目标 grammar：

```text
ClosureExpr =
    ClosureParamList "->" Type ClosureBody

ClosureParamList =
    "||"
  | "|" ClosureParamItems? "|"

ClosureParamItems =
    ClosureParam ("," ClosureParam)* ","?

ClosureParam =
    Identifier ":" Type

ClosureBody =
    "=>" Expr
  | Block
```

说明：

- 空参数闭包可以写 `|| -> T => expr`。
- parser 也应接受 token 序列 `|` `|` 作为空参数闭包，和 lexer 产生的 `||` token 等价。
- `|x: i32| -> i32 => x + 1` 是闭包表达式。
- `| x : i32 | -> i32 => x + 1` 是同一个闭包表达式。
- 换行和注释不改变语义。

这不是空格敏感规则。`||` 和 `| |` 的等价来自 token grammar：闭包参数表允许 `pipe_pipe`，也允许 `pipe` 后立刻看到关闭 `pipe`。parser 不读取 trivia 判断。

表达式歧义：

```aurex
a | b
a || b
```

这两个在已有 left operand 的中缀位置仍是 binary expression。闭包只在 primary expression 起始位置解析：

```aurex
let f = |x: i32| -> i32 => x + 1;
let g = (|x: i32| -> i32 => x + 1);
call(|x: i32| -> i32 => x + 1);
```

也就是说，分歧来自 grammar position，不来自空格。

## AST / Sema 影响

第一阶段可以复用现有 AST：

```text
ExprKind::lambda
LambdaExprPayload {
    params
    return_type
    body
}
```

parser 只是把 `LambdaExprPayload` 的来源从 `kw_fn` 改成 `pipe` / `pipe_pipe` 起手。

sema 规则不变：

- 无捕获闭包可以作为 `fn(...) -> T` 薄函数值。
- 捕获闭包生成匿名 environment record。
- 捕获闭包不能赋给 `fn(...) -> T`。
- 继续只支持非泛型依赖、非 borrowed-view、满足 `Copy` capability 的按值捕获核心子集。
- 继续拒绝 captured non-Copy、borrowed-view、generic-dependent capture。

IR / lowering 规则不变：

- 无捕获闭包可降低成内部函数符号。
- 捕获闭包保留当前 environment record + thunk 路径。

## 迁移计划

### Stage 1：接受新闭包语法

- parser 接受 `|...| -> T => expr`。
- parser 接受 `|...| -> T { ... }`。
- parser 接受 `|| -> T => expr` 和 `|| -> T { ... }`。
- 旧 `fn(...) -> T => expr` / `fn(...) -> T { ... }` 继续接受，最好发 deprecation warning。

### Stage 2：更新文档、样例和测试

- `docs/zh/language-manual.md` 中闭包示例迁移到 `|...|`。
- `tests/samples/positive/functions/lambda_function_literal.ax` 迁移到 `|...|`。
- `tests/samples/positive/functions/lambda_closure_capture.ax` 迁移到 `|...|`。
- negative samples 中 `fn(...) -> T => ...` 也同步迁移，除非专门测试 legacy 诊断。
- AST dump / checked dump 如果只展示 semantic lambda，可不强制保留旧 spelling。

### Stage 3：收紧旧语法

- expression primary 中 `kw_fn` 不再作为正常闭包起点。
- 旧 `fn(...) -> T => ...` 从 warning 升级为 error。
- error 指向新写法：

```text
legacy closure literal syntax is no longer supported; write `|x: T| -> R => expr`
```

## 诊断规则

目标诊断：

```text
expected closure parameter list after `|`
expected `|` after closure parameters
expected `->` and return type after closure parameter list
expected closure body after return type
closure literals cannot be extern c
closure literals cannot be variadic
closure literals cannot be declared unsafe; put unsafe operations in an unsafe block
legacy `fn(...) -> T` closure literal is deprecated; write `|...| -> T`
```

需要避免的诊断：

```text
expected function name
expected fn keyword
function declaration is invalid here
```

用户写闭包写错时，应按 closure literal 报错，不要把它导向函数声明。

## 不采用的方案

### 不采用 `lambda (...) -> T => expr`

```aurex
lambda (x: i32) -> i32 => x + 1
```

问题：

- 新增硬关键字或 contextual keyword。
- 比当前 `fn(...)` 还长。
- 高频表达式不适合这么重。

### 不采用 `(x: T) -> R => expr`

```aurex
(x: i32) -> i32 => x + 1
```

问题：

- `(` 已经承担 grouped expression、tuple、function call、function parameter list。
- 在表达式开头看到 `(`，parser 必须做更多 lookahead 才能判断是 grouped expr 还是 closure。
- 和 function type / function declaration 的视觉边界仍不够强。

### 不采用 `{ x: T => expr }`

```aurex
{ x: i32 => x + 1 }
```

问题：

- `{}` 已经是 block、struct literal、match arm 周边语法的高压区域。
- 会和未来 expression-oriented block、macro token tree、record literal 继续互相挤压。
- 闭包参数列表没有稳定的视觉封闭边界。

### 不采用继续保留 `fn(...) -> T => expr`

这个方案实现成本最低，但问题没有解决。继续保留会让 `fn` 永久同时表示：

- item declaration
- function pointer type
- closure expression
- unsafe function type
- extern C function type
- export C function item

这不是“统一”，是把不同语义压成同一外形。

## 后续单独问题

### Closure inference

未来可以单独设计：

```aurex
let inc: fn(i32) -> i32 = |x| => x + 1;
```

需要解决：

- expected type 如何传播进 closure params。
- 返回表达式如何约束 closure return type。
- capture 诊断如何在 inference 失败时保持清楚。
- 无 expected type 时是否允许 fallback。

### Callable trait / closure trait

当前 Aurex 没有 `Fn` / `FnMut` / `FnOnce`。如果未来要做通用 adapter，需要单独设计 callable trait 或 capability：

```aurex
fn map<T, U, F>(xs: []T, f: F) -> Vec<U>
where F: Fn<T, Output = U> {
    ...
}
```

这不是本次 surface change 的内容。

### ABI / export surface

`export c fn`、`extern c { ... }`、`@name`、`@borrow` 这些 ABI/linkage 表面也值得单独清理。本文只规定闭包不参与 ABI modifier，不把 ABI 设计和闭包语法绑在一起。

## 测试要求

Parser：

- 接受 `|x: i32| -> i32 => x + 1`。
- 接受 `|x: i32| -> i32 { return x + 1; }`。
- 接受 `|| -> i32 => 1`。
- 接受 `| | -> i32 => 1`，语义等价于 `|| -> i32 => 1`。
- 接受换行排版：

```aurex
|
    x: i32,
    y: i32,
| -> i32 => x + y
```

- `a | b` 继续是 bitwise-or binary expression。
- `a || b` 继续是 logical-or binary expression。
- 迁移期旧 `fn(x: i32) -> i32 => x` 仍可解析为 lambda 并产生 warning。

Sema：

- 无捕获 `|x: i32| -> i32 => x + 1` 可赋给 `fn(i32) -> i32`。
- 捕获 `|x: i32| -> i32 => x + base` 不能赋给 `fn(i32) -> i32`。
- 捕获 non-Copy、borrowed-view、generic-dependent 的诊断保持不变。
- 闭包体内 unsafe 操作仍要求 `unsafe` context。
- `extern c`、`unsafe`、variadic closure literal 被拒绝。

Display / dumps：

- AST dump 可继续称为 `lambda` 或迁移到 `closure`，但用户诊断建议使用 `closure literal`。
- Checked lambda info 不需要保留旧源码 spelling。

## 结论

第一阶段做这个修正：

```aurex
// 保留
fn name(...) -> T { ... }
fn(...) -> T
unsafe fn(...) -> T
extern c fn(...) -> T

// 修改
fn(x: T) -> R => expr  ->  |x: T| -> R => expr
fn(x: T) -> R { ... }  ->  |x: T| -> R { ... }
```

这能把 Aurex 的函数表面拆成清楚的三层：`fn name` 是声明，`fn(...) -> T` 是薄函数指针类型，`|...| -> T` 是闭包表达式。语义不扩大，parser 不靠空格，后续如果要做 closure inference 或 callable trait，可以在这个干净边界上继续设计。
