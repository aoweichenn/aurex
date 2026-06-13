# Aurex M27c 语法易用性与审美问题清单

日期：2026-06-13
状态：设计讨论稿，基于 `209fa267 Add Aurex macro call-site admission`

本文记录当前 Aurex 语法表面的易读性、易写性和一致性问题。它不是最终规范，也不是要求照搬某门已有语言；目标是把开发期已经暴露的语法问题列成可逐项讨论、可逐项优化的设计清单。

当前代码仍在快速开发期，任何语法表面都可以调整。本文优先从以下标准判断问题：

- 读者看到一个符号时，能否不依赖太多后续上下文就预测它的角色。
- 常用代码是否好打、少噪音、少内部实现味。
- 同一概念在不同语法位置是否有稳定写法。
- parser 是否需要靠命名风格、上下文禁用或启发式猜测来解析。
- 文档、样例和实现是否形成一致的语言气质。

## 总结

当前 Aurex 最大的问题不是“缺语法”，而是语法表面还没有收敛。几个符号承担了太多职责，导致代码短但不清爽；一些 builtin 名字暴露了编译器内部 ABI 味道；部分语法表面给人的承诺大于实际语义。

最高优先级问题：

1. `[]` 过载严重，泛型、索引、slice、array、pattern、attribute 和 origin 都挤在同一符号上。
2. 泛型调用和索引的区分依赖大小写启发式，命名风格影响 parsing。
3. `strfromutf8`、`strblen`、`sliceptr`、`ptrat` 等 builtin 名字不适合作为用户可见关键字。
4. `for i in range(...)` 看起来像通用 `for-in`，实际上只识别 magic `range`。
5. `mut` / `const`、`fn`、`.`、pattern、macro surface 等局部语法各自能解释，但整体视觉层级不够稳定。

## P0：`[]` 过载

### 现状

当前 `[]` 至少承担这些职责：

```aurex
[2]u8                 // array type
[]const u8            // slice type
[]mut T               // mutable slice type
Box[T]                // generic type argument
fn f[T](x: T) -> T    // generic parameter
cast[i32](x)          // builtin generic call
sizeof[*mut Pair]     // builtin type argument
Source[Item = i32]    // associated type equality
arr[i]                // index
arr[l:r]              // slice expression
[1, 2]                // array literal
[x; n]                // repeat array literal
[head, ..]            // slice pattern
#[derive(Inspect)]    // attribute
&[origin] T           // explicit origin reference
```

相关实现入口：

- `src/frontend/parse/grammar/parser_type.cpp`：reference origin、pointer、array、slice、named type generic argument。
- `src/frontend/parse/grammar/parser_postfix.cpp`：表达式后缀的 index、slice、generic apply、struct literal。
- `src/frontend/parse/grammar/parser_fn.cpp`：函数泛型参数。
- `src/frontend/parse/grammar/parser_pattern.cpp`：slice pattern。
- `src/frontend/parse/grammar/parser_item.cpp`：attribute token tree。

### 问题

这不是简洁，而是符号预算崩了。读者看到 `[` 后，必须等上下文和后续 token 才知道这是类型、泛型、索引、slice、literal、pattern、attribute 还是 origin。

它还直接导致 parser 需要额外分类逻辑。尤其表达式后面的 `foo[bar]`，既可能是索引，也可能是泛型实例化或 builtin 类型参数调用。

### 候选优化方向

优先考虑把泛型从 `[]` 中移走。`[]` 最多保留给 index/slice/array/literal 这一类“序列与下标”语义，避免再承担 generic。

候选方向：

- `Box<T>` / `fn f<T>(...)`：泛型视觉最常见，但会带来 `<` / `>` 与比较表达式的解析成本。
- `Box(T)` / `fn f(T)(...)`：好打，但容易和调用、tuple、函数参数列表混淆。
- `Box{T}` / `fn f{T}(...)`：与 block / struct literal 冲突风险更高，不建议优先。
- `Box of T` / `fn f of T`：可读，但关键字噪音和多参数泛型写法需要设计。
- 保留 `[]`：不建议，除非放弃 `arr[i]` 或泛型后缀之一，否则启发式会继续膨胀。

### 待讨论

- Aurex 泛型应该优先追求“短”，还是优先追求“和下标完全分离”？
- 如果使用 `<T>`，parser 需要哪些 disambiguation 规则，能否完全不依赖命名风格？
- array type `[N]T` 和 slice type `[]mut T` 是否也需要一起重新设计，还是先只迁移泛型？

## P0：大小写启发式影响 parsing

### 现状

`src/frontend/parse/support/sources/bracket_suffix_classifier.cpp` 中的 bracket suffix classifier 会判断标识符首字母是否像类型名，再结合后续 token 判断 `foo[bar]` 更像 generic apply 还是 index。

### 问题

命名风格不应该影响语法。类型名大写可以是 lint 或风格约定，但不能参与语法分类。用户把类型写成 `box` 或把值写成 `Box`，不应该改变 parser 对同一 token 序列的解释。

这个问题本质上是 `[]` 过载的后果。如果泛型和索引使用不同表面，这类启发式应当可以删除。

### 候选优化方向

- 移除泛型 `[]`，让 `foo[bar]` 永远表示 index/slice 或明确的 sequence 语义。
- 如果短期不能迁移，至少把分类规则收敛为纯语法规则，不再使用标识符大小写。
- 增加 negative syntax tests，覆盖 lowercase type name、uppercase value name、generic call 与 index 的冲突。

### 待讨论

- 在迁移期是否允许旧 `Box[T]` 和新泛型语法并存？
- 并存时 parser 如何避免继续使用大小写启发式？

## P1：builtin 名字和关键字污染

### 现状

当前 builtin 直接进入关键字 / primary expression 入口，包括：

```text
cast ptrcast bitcast sizeof alignof
ptraddr ptrat sliceptr slicelen
strptr strblen strvalid strfromutf8 strraw
```

样例会出现：

```aurex
let pair_ptr: *mut Pair = unsafe { ptrat[*mut Pair](addr) };
let pointer_size: usize = sizeof[*mut Pair];
let text: str = strfromutf8(bytes);
let n: usize = strblen(text);
let raw: str = unsafe { strraw(strptr("raw"), strblen("raw")) };
```

相关实现入口：

- `include/aurex/frontend/syntax/core/token.hpp`
- `src/frontend/lex/sources/keyword.cpp`
- `src/frontend/parse/grammar/parser_primary.cpp`
- `src/frontend/parse/grammar/parser_builtin_expr.cpp`
- `tests/samples/positive/core/builtins.ax`

### 问题

这些名字像编译器内部 intrinsic 或 ABI helper，不像用户语言表面。`strfromutf8`、`strblen`、`sliceptr`、`ptrat` 都不好读，也不好打。

更严重的是，它们占用了硬关键字空间。语言越往后发展，关键字越难收回；库函数、method 或 intrinsic namespace 更适合承载这类能力。

### 候选优化方向

- 把普通能力移到库或 prelude：
  - `len(x)` 或 `x.len`
  - `data(x)` 或 `x.data`
  - `utf8(bytes)` / `str.from_utf8(bytes)`
- 把危险能力放到明确的 unsafe intrinsic namespace：
  - `intrinsic.ptr_at[T](addr)`
  - `intrinsic.str_raw(ptr, len)`
  - `intrinsic.bitcast[T](x)`
- 保留 `sizeof` / `alignof` 这类编译期 operator 时，也应统一泛型参数表面，避免 `sizeof[*mut Pair]` 继续消耗 `[]`。

### 待讨论

- 哪些 builtin 必须是语言关键字，哪些可以降级为库函数或 compiler intrinsic？
- `str` / slice 的长度、指针、UTF-8 构造应该是函数、method、属性，还是 namespace API？
- unsafe intrinsic 是否应该全部要求 `unsafe { ... }`，并在名字上显式体现危险性？

## P1：`for i in range(...)` 语义不诚实

### 现状

当前语法支持：

```aurex
for i in range(0, n) {
    ...
}
```

但 parser 会检查 callee 是否正好是 `range`。文档也说明 `for value in values {}` 当前不支持。

相关入口：

- `src/frontend/parse/grammar/parser_control_stmt.cpp`
- `docs/zh/language-manual.md`

### 问题

表面看是通用 `for-in`，实际是 magic range loop。这个设计最伤直觉：用户自然会尝试 `for x in values`，但语言并没有承诺对应的 iterator/range protocol。

### 候选优化方向

- 实现真正 `for x in expr`，由 iterator/range trait 或内建 range/slice protocol 支撑。
- 如果暂时不做通用 iterator，就把语法改成诚实的 range loop：

```aurex
for i = 0..n {
    ...
}

for i from 0 to n step 2 {
    ...
}
```

### 待讨论

- Aurex 是否近期要设计通用 iterator/range protocol？
- 如果不做，range loop 是否应该避免使用 `in`？
- `range(start, end, step)` 是库调用、内建语法，还是编译期特殊表面？

## P1：`mut` / `const` 位置不统一

### 现状

当前常见类型写法：

```aurex
*const T
*mut T
&T
&mut T
[]const T
[]mut T
```

binding 使用：

```aurex
let x: T = value;
var y: T = value;
```

### 问题

每个局部规则都能解释，但整体节奏不统一。pointer、reference、slice、binding 各自有修饰词位置，用户需要分别记。

尤其 `[]const T` / `[]mut T` 的视觉重点很怪：`[]` 是 slice，但 mutability 插在 slice 符号之后、元素类型之前。和 `&mut T` 类似，却又不像 pointer 的 `*mut T` 那样有明显 unary type constructor。

### 候选优化方向

- 明确 mutability 是修饰“引用/指针/slice 的访问权限”，还是修饰“被指向/被借用元素”。
- 为 pointer、reference、slice 统一一个读法模型。
- 如果保留 `let` / `var`，需要说明它和 `mut` 的分工：binding mutability 与 pointed-to / borrowed mutability 不应混淆。

### 待讨论

- `[]mut T` 表示 mutable slice view，还是 slice of mutable T？这个读法要不要在语法上更明确？
- 是否允许不可变 slice 简写 `[]T`，把 `[]const T` 变成低频显式形式？

## P2：`fn` 同时承载声明、类型和 lambda

### 现状

当前 `fn` 出现在多种表面：

```aurex
fn add(a: i32, b: i32) -> i32 { ... }
fn(i32, i32) -> i32
fn(x: i32) -> i32 => x + 1
unsafe fn(...) -> T
export c fn ...
extern c fn ...
```

相关入口：

- `src/frontend/parse/grammar/parser_fn.cpp`
- `src/frontend/parse/grammar/parser_primary.cpp`
- `src/frontend/parse/grammar/parser_type.cpp`

### 问题

`fn` 本身可以保留，但当前函数声明、函数类型、lambda、unsafe ABI 都共用同一视觉骨架。lambda 的 `fn(...) -> T => expr` 尤其不清爽：打字并不少，视觉上像函数声明写到一半转成表达式。

### 候选优化方向

- 保留 `fn` 给函数声明和函数类型，单独设计 lambda：

```aurex
|x: i32| -> i32 { x + 1 }
lambda (x: i32) -> i32 => x + 1
```

- ABI/export 相关语法集中到 attribute 或 modifier，减少 `export c fn`、`extern c fn`、`unsafe fn` 的组合散落。

### 待讨论

- Aurex 是否需要频繁写 lambda？如果 lambda 高频，当前写法太重。
- 函数类型和函数声明是否必须共享 `fn`，还是可以给 function type 单独 spelling？

## P2：`.` 过载导致路径和成员访问视觉模糊

### 现状

`.` 用于：

- module/import path
- type path
- field access
- tuple field
- enum case / shorthand enum case
- method / associated item

相关入口：

- `src/frontend/parse/grammar/parser_module_item.cpp`
- `src/frontend/parse/grammar/parser_type.cpp`
- `src/frontend/parse/grammar/parser_postfix.cpp`
- `src/frontend/parse/grammar/parser_pattern.cpp`

### 问题

`.` 好打，不应轻易放弃。但它和 `[]` 一起过载后，代码会出现很多层级糊在一起的形态：

```aurex
t.RegexStatus.ok
.ok
Type[Args].case
module.path.Type
value.field.method()
```

读者需要靠语义环境区分 module、type、value、enum case 和 field。

### 候选优化方向

- 先解决 `[]` 泛型过载，降低 `Type[Args].case` 这种组合噪音。
- 对 enum case 是否使用单独构造语法重新评估，避免 `.ok` 和 field/member fragment 过像。
- import path / type path / value member 是否需要在文档和 formatter 上形成更强风格约束。

### 待讨论

- `.case` 简写是否值得保留？它省字符，但降低大型 match 的自说明性。
- module path 和 type associated item 是否需要不同分隔符，还是保持 `.` 但加强上下文规则？

## P2：pattern 语法短但不够自说明

### 现状

pattern 里存在：

```aurex
.ok
const NAME
[head, ..]
```

相关入口：

- `src/frontend/parse/grammar/parser_pattern.cpp`
- `examples/libs/regex/api.ax`

### 问题

`.ok` 很短，但不像完整名字，容易像 field/member 片段。`const NAME` 在 pattern 位置表示常量 pattern，也需要读者知道“这里不是声明”。slice rest 的 `..` 还会和 range 设计、ellipsis token 等未来表面互相影响。

### 候选优化方向

- enum case pattern 默认要求完整路径，简写由 formatter 或局部 `use` 解决。
- 常量 pattern 使用更明显的标记，或通过名字解析和大写约定配合诊断，而不是在 pattern 位置复用 `const`。
- 如果未来引入 `..` range operator，要提前统一 `..` 在 pattern、range、struct update 中的含义。

### 待讨论

- Aurex 更重视 match arm 的短，还是重视大型 match 的可读性？
- `const NAME` pattern 是否值得保留？

## P2：`{}` 同时做 block 和 struct literal

### 现状

控制流条件解析使用 `ExprContext::no_struct_literal`，避免 `if` / `while` / `for` 条件里的 struct literal 与 block 冲突。struct literal 在 postfix 解析中处理。

相关入口：

- `src/frontend/parse/grammar/parser_control_stmt.cpp`
- `src/frontend/parse/grammar/parser_postfix.cpp`

### 问题

这说明语法表面本身有歧义，需要 parser 根据上下文禁用某种表达式。实现能处理不代表用户读起来清楚。只要 `{}` 既是 block 又是 aggregate literal，就要考虑条件、lambda、match arm、macro token tree 等位置的可读性。

### 候选优化方向

- 保留现状，但用文档和诊断明确说明哪些位置禁止 struct literal。
- 引入 struct literal 的更明确前缀或构造符。
- 调整控制流条件语法，让 condition 和 block 的边界更明显。

### 待讨论

- 这个问题当前是否只影响 parser，还是已经影响用户书写体验？
- 是否值得为了 struct literal 清晰度牺牲一点短写法？

## P2：macro surface 风格割裂

### 现状

M27/M27b/M27c admission-only macro surface 包括：

```aurex
macro Name {
    match expr_list(xs) -> { xs }
}

macro derive Inspect {
    match item(target) -> { target }
}

macro const TokenBuild {
    match tokens(input) -> { input }
}

macro call Name {
    ...
}

#[derive(Inspect)]
```

相关文档和实现：

- `src/frontend/parse/grammar/parser_item.cpp`
- `docs/zh/m27-aurex-macro-surface-admission.md`
- `docs/zh/language-manual.md`

### 问题

这些单独看都能解释，但放在一起像几套小 DSL：声明是 `macro derive`，使用是 `#[derive]`，调用 admission 是 `macro call`，规则又是 `match expr_list(xs) -> { xs }`。风格没有收束。

当前阶段如果只是 admission-only，可以接受它作为内部里程碑；但不应过早把未收敛的 macro 表面当作长期用户语法。

### 候选优化方向

- 明确 macro surface 是临时 admission 语法还是长期用户语法。
- attribute、macro declaration、macro call、matcher rule 使用同一套命名和分层。
- 在真实 expansion、hygiene、source map、diagnostic model 定之前，避免继续扩张用户可见 macro 语法。

### 待讨论

- `macro call Name { ... }` 是最终用户语法，还是 parser admission 测试入口？
- `match expr_list(xs) -> { xs }` 这种 matcher 表面是否应该复用普通 `match` 关键字？

## P3：空 tuple / unit 规则不自然

### 现状

当前 tuple 存在，但 `()` type/literal/pattern 不支持；无返回使用 `void`。

相关入口：

- `src/frontend/parse/grammar/parser_type.cpp`
- `src/frontend/parse/grammar/parser_primary.cpp`
- `src/frontend/parse/grammar/parser_pattern.cpp`
- `docs/zh/language-manual.md`

### 问题

用户一边可以写 tuple，一边不能写最自然的 empty tuple。`void` 作为无返回类型可以保留，但 tuple/pattern 系统缺少 `()` 会显得规则任性。

### 候选优化方向

- 明确 `void` 和 unit 是否是同一概念。
- 如果不同，说明 `void` 只用于函数返回，`()` 是可值化 unit。
- 如果相同，决定是否允许 `()` 作为 `void` literal/pattern，或者完全禁止 empty tuple 并在文档里解释理由。

### 待讨论

- Aurex 是否需要 unit value？如果有 expression-oriented block、lambda、match，unit value 往往会自然出现。
- `fn main() -> void` 是否长期保留，还是允许 `fn main()` 省略返回？

## 建议的讨论顺序

建议按影响面从大到小讨论：

1. 泛型语法是否迁出 `[]`。
2. 删除大小写 parsing 启发式的迁移计划。
3. builtin 关键字降级为库 API / intrinsic namespace 的边界。
4. range loop 和未来 iterator protocol 的关系。
5. mutability / constness 在 pointer、reference、slice、binding 上的统一模型。
6. lambda 与 function type 是否继续共用 `fn`。
7. enum case、pattern shorthand 和 `.` 的整体风格。
8. struct literal 与 block 的歧义是否需要语法层调整。
9. macro admission surface 是否冻结、改名或隐藏。
10. `void` / `()` / unit 的长期模型。

## 设计原则草案

后续每个优化方案都应过这几条原则：

- 不靠命名风格解析语法。
- 不让一个短符号承担太多互不相关的语义。
- 语法表面不要伪装成比实际语义更通用的能力。
- 用户常写的名字应像 API，不应像 compiler ABI helper。
- unsafe 能力要在语法或命名上清楚暴露危险边界。
- admission-only、shadow mode 和真实用户语法要在文档里明确分层。
