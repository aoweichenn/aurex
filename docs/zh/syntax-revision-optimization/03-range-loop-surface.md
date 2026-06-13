# Range Loop Surface：拆掉伪 `for-in range(...)`

日期：2026-06-13
状态：语法修正优化第三手改动设计稿
关联问题：`docs/zh/m27c-syntax-ergonomics-review.md` 中的 P1 `for i in range(...)` 语义不诚实

本文固定 Aurex 第三项语法修正方向：把当前 `for i in range(...)` 从伪通用 `for-in` 表面迁移为诚实的 counted range loop 表面。

核心判断很直接：当前语言没有通用 iterator protocol，却写成了 `for i in ...`。这不是小瑕疵，是语法在承诺一件语言实际没有提供的能力。

## 现状

当前 parser 支持：

```aurex
for i in range(limit) {
    total += i;
}

for j in range(1, limit) {
    total += j;
}

for k in range(1, limit, 2) {
    total += k;
}
```

真实实现不是普通函数调用，也不是通用 iteration。相关入口：

- `src/frontend/parse/grammar/parser_control_stmt.cpp`
  - `next_for_is_range_loop()` 只看 `Identifier "in"`。
  - `parse_for_range_stmt()` 要求 `in` 后面的 callee 文本正好是 `range`。
  - 非 `range` 会报 `M2 range-for only supports range(...); generic iteration is not part of M2 syntax`。
  - 参数数量被限制为 1 到 3。
- `include/aurex/frontend/syntax/ast/stmt_nodes.hpp`
  - AST 已有专门的 `StmtKind::for_range`。
  - payload 是 `name`、`range_start`、`range_end`、`range_step`、`body`。
- `src/frontend/sema/internal/expressions/sources/sema_statement_analyzer.cpp`
  - `analyze_for_range_bounds()` 要求 start/end/step 是整数。
  - start/end 必须同类型，step 必须和 bounds 同类型。
  - loop local 是不可变局部。
- `src/midend/ir/lowering/sources/lower_ast_stmt.cpp`
  - `lower_for_range()` 把缺省 start 降成 `0`。
  - 缺省 step 降成 `1`。
  - 显式 step 会存入临时 slot。
  - condition 对正步长用 `cursor < end`，对负步长用 `cursor > end`，step 为 0 时循环零次。

也就是说，语义层已经是一个专门的整数 counted loop。问题集中在用户表面。

## 问题

### 1. `in` 给了错误承诺

这段代码看起来像通用 for-in：

```aurex
for x in values {
    ...
}
```

但语言实际不支持。当前只接受：

```aurex
for i in range(...) {
    ...
}
```

这种设计会直接误导用户：既然能 `for i in range(...)`，自然会尝试 `for x in slice`、`for item in vec`、`for ch in text`。parser 最后再告诉用户“只能 range”，这是语法表面先骗人、诊断再补救。

### 2. `range(...)` 像库函数，实际是 parser magic

`range` 在这里不是普通函数：

```aurex
for i in range(0, n) {
    ...
}
```

parser 会直接检查 callee 文本是否等于 `range`。这意味着：

- 用户不能通过导入、重载、namespace 或 trait 改变它。
- `range` 看起来在表达式层，实际是 statement parser 的特殊分支。
- 这和后续“减少 parser 特例、把普通能力交给 sema/name resolution”的方向冲突。

如果 `range(...)` 是真正的 range value，配合真正的 `for-in`，这个表面可以成立。当前不是。

### 3. 位置参数不自解释

下面三种写法都要靠记忆参数位置：

```aurex
range(end)
range(start, end)
range(start, end, step)
```

尤其是倒序和动态 step：

```aurex
for i in range(5, 0, -2) {
    ...
}
```

它能用，但读的时候要在脑子里解包第三个参数。counted loop 的三个概念本来就是 start、exclusive end、step，语法应该直接把这些词写出来。

### 4. 现在的语法占用了未来的 `for-in`

未来如果 Aurex 要做真正 iterator protocol，最自然的表面就是：

```aurex
for item in values {
    ...
}
```

现在提前把 `for Identifier in ...` 用在一个 parser magic 上，会让迁移很脏：

- 老用户已经把 `in` 和 `range(...)` 绑定。
- 新用户会以为 `range(...)` 是 iterator 的普通一员。
- parser 需要同时解释“真 iterator”和“老 magic range”。

所以现在应该把 counted range loop 从 `in` 里移走。`in` 留给真正的 iterator/range protocol。

## 决策

当前 counted integer range loop 迁移到：

```aurex
for i from start until end {
    ...
}

for i from start until end by step {
    ...
}
```

对应当前写法：

```aurex
for i in range(end) {
    ...
}

for i in range(start, end) {
    ...
}

for i in range(start, end, step) {
    ...
}
```

迁移目标：

```aurex
for i from 0 until end {
    ...
}

for i from start until end {
    ...
}

for i from start until end by step {
    ...
}
```

具体例子：

```aurex
for i from 0 until 4 {
    sum += i;
}

for i from 2 until 5 {
    sum += i;
}

for i from 1 until 6 by 2 {
    sum += i;
}

for i from 5 until 0 by -2 {
    sum += i;
}

for i from 0 until 5 by 0 {
    // 保持当前语义：step 为 0 时零次迭代。
}
```

### 为什么用 `until`

Aurex 当前 range-for 是半开区间 `[start, end)`。如果写：

```aurex
for i from 0 to n {
    ...
}
```

`to n` 很容易被读成包含 `n`。`until n` 更诚实：一直走到 n 之前，不包含 n。

这是一个打字成本和语义清晰度的交换：

- `to` 更短，但终点包含性不清楚。
- `until` 多 3 个字母，但直接表达 exclusive end。

第一阶段建议选择 `until`，因为这轮修正的目标就是消灭不诚实表面。为了少打 3 个字母重新制造终点歧义，不值。

### 为什么用 `by`

显式步长写成：

```aurex
for i from 0 until n by 2 {
    ...
}
```

而不是：

```aurex
for i from 0 until n step 2 {
    ...
}
```

`step` 更机械，`by` 更短，读起来也更自然：from 0 until n by 2。

如果后续觉得 `by` 过于英语化，也可以换成 `step`。这不会影响 AST、sema、IR，只影响 parser marker 和文档。

## Parser 规则

目标 grammar：

```text
ForRangeStmt =
    "for" Identifier "from" Expr "until" Expr ("by" Expr)? Block
```

不提供省略 start 的短写。也就是说，不引入：

```aurex
for i until end {
    ...
}
```

原因：

- `range(end)` 的 start=0 已经是一个隐式规则，迁移时没有必要继续扩大隐式表面。
- `for i from 0 until end` 多打一个 `0`，但语义完整。
- 少一种 grammar shape，parser、formatter、诊断和教学成本都更低。

### 不做空格敏感

Aurex 不做空格敏感语法。空格、换行、comment 只分隔 token 和保留源码格式，不参与语法含义判断。

下面写法在 parser 层必须等价：

```aurex
for i from 0 until n by 2 {
    ...
}

for i   from   0   until   n   by   2 {
    ...
}

for i
from 0
until n
by 2 {
    ...
}
```

formatter 可以统一排成第一种风格，但那是格式化策略，不是语言规则。

### `from` / `until` / `by` 不应成为 hard keyword

刚把 builtin keyword 污染列为语法问题，就不应该立刻新增一批全局 hard keyword。

建议第一阶段把 `from`、`until`、`by` 做成 range-for grammar 内的 contextual marker：

```text
"for" Identifier identifier("from") Expr identifier("until") Expr (identifier("by") Expr)? Block
```

也就是说：

- lexer 仍把 `from`、`until`、`by` 当普通 identifier。
- parser 只在 `for Identifier ...` 这个位置把它们当语法标记。
- 其他位置用户仍可以使用 `from`、`until`、`by` 作为普通名字。

极端情况下这类代码仍可解析：

```aurex
let from: i32 = 0;
let until: i32 = 10;

for i from from until until by 1 {
    ...
}
```

这不好看，但它是上下文关键字的正常代价。formatter 和 style guide 可以建议避免这种命名；parser 不应该因此把三个常用英文词冻结成全局 keyword。

## 语义模型

新表面只改变语法，不改变当前 counted range loop 语义。

```aurex
for i from start until end by step {
    body
}
```

语义：

- 区间是半开区间 `[start, end)`。
- `start`、`end`、`step` 都必须是整数。
- `start` 和 `end` 必须同类型。
- 显式 `step` 必须和 bounds 同类型。
- 省略 `by step` 时，step 默认为 `1`。
- `step > 0` 时，当 `cursor < end` 执行循环。
- `step < 0` 时，当 `cursor > end` 执行循环。
- `step == 0` 时循环零次，保持当前行为。
- loop variable 是不可变局部，只在 loop body 里可见。
- `start`、`end`、`step` 按源码顺序各求值一次。
- `continue` 跳到隐藏的 cursor 更新点。
- `break` 跳出整个 range loop。

和当前实现的映射：

```text
range_start = start
range_end   = end
range_step  = step 或 INVALID_EXPR_ID
```

所以 AST 可以继续复用 `StmtKind::for_range`，不需要为了这次语法修正新增 AST kind。

## 候选方案比较

### 方案 A：`for i from start until end by step`

```aurex
for i from 0 until n by 2 {
    ...
}
```

优点：

- 不再伪装成通用 `for-in`。
- `until` 清楚表达不包含 end。
- `by` 短，整体读法自然。
- 不需要新增 range expression。
- 不需要改 AST/sema/IR 的核心 shape。
- `from` / `until` / `by` 可以做 contextual marker，不污染全局 keyword。

缺点：

- 比 `0..n` 长。
- 比 `to` 多打 3 个字母。
- 表达式 parser 需要在 `for-range` 头部给出更好的 marker 诊断。

结论：推荐采用。

### 方案 B：`for i from start to end step step`

```aurex
for i from 0 to n step 2 {
    ...
}
```

优点：

- `to` 很短。
- `from ... to ...` 很常见。
- `step` 的含义直白。

问题：

- `to n` 的终点包含性不够诚实。
- `step` 比 `by` 长，整体更像内部 IR 描述。
- 如果后续要支持 inclusive range，又会重新遇到 `to` 到底含不含终点的问题。

结论：不作为第一选择。除非项目更看重极短输入，否则不建议。

### 方案 C：`for i = start..end by step`

```aurex
for i = 0..n by 2 {
    ...
}
```

优点：

- 短。
- counted loop 味道强。
- 未来如果有 range expression，可以复用 `start..end`。

问题：

- 当前 lexer 没有独立 `..` token；slice rest `..` 是 parser 在 pattern 里匹配两个 `dot` token，`...` 才是 `ellipsis` token。
- `..` 在不同语言里既可能是 inclusive，也可能是 exclusive，不天然解决终点语义。
- range expression 会影响 slice、pattern、index、future iterator、operator precedence，不应该只为修 range loop 临时塞进去。
- `for i = ...` 容易和现有 C-style `for` 头部产生视觉混杂。

结论：这应该留给“range expression / slicing / iterator protocol”专题，不作为当前 range loop 表面修正的第一步。

### 方案 D：继续保留 `for i in range(...)`

只有一种情况下这个方案可以成立：Aurex 先实现真正的 `for-in`，并且 `range(...)` 是一个真实可迭代 range value。

在当前项目状态下它不成立，因为 parser 明确 hard-code `range`，没有 iterator protocol。

结论：开发期应迁移掉，不能把它当最终语法。

## 迁移路径

### 阶段 1：新增新语法，旧语法继续过

同时支持：

```aurex
for i from 0 until end {
    ...
}

for i in range(end) {
    ...
}
```

旧语法给出迁移 warning 或 deprecation diagnostic：

```text
`for i in range(end)` is deprecated; use `for i from 0 until end`
```

fix-it 映射：

```text
for i in range(end)              -> for i from 0 until end
for i in range(start, end)       -> for i from start until end
for i in range(start, end, step) -> for i from start until end by step
```

### 阶段 2：项目样例和文档迁移

需要改：

- `docs/zh/language-manual.md`
- `docs/zh/language-feature-inventory.md`
- `tests/gtest/frontend/parse/parser_tests.cpp`
- `tests/samples/positive/control_flow/for_range.ax`
- `tests/samples/positive/control_flow/for_loop.ax`
- `tests/samples/negative/control_flow/for_range_*.ax`

### 阶段 3：旧语法变成错误

当样例、测试和文档都迁移后，`for i in range(...)` 应该报错并给出替换建议。

保留：

```aurex
for x in values {
    ...
}
```

作为未来真正 iterator protocol 的语法空间。当前如果用户写它，诊断应该明确：

```text
generic `for-in` is not supported yet; use `for i from start until end` for counted integer loops
```

## 实现计划

### Parser

修改 `src/frontend/parse/grammar/parser_control_stmt.cpp`：

- `next_for_is_range_loop()` 从 `Identifier kw_in` 改为识别 `Identifier identifier("from")`。
- 新增 contextual marker helper，例如：

```text
check_identifier_text("from")
expect_identifier_text("until", ...)
match_identifier_text("by")
```

- `parse_for_range_stmt()` 解析：

```text
name
from
start expr
until
end expr
optional by
step expr
body block
```

表达式边界不靠空格。`until` 和 `by` 是 token/text marker，不是 trivia marker。

### AST

无需新增节点：

```text
StmtKind::for_range
range_start
range_end
range_step
```

区别只是新语法永远显式提供 `range_start`。旧 `range(end)` 在迁移期仍可继续存成 `range_start = INVALID_EXPR_ID`，lowering 继续补 `0`。

如果阶段 3 删除旧语法，可以考虑让 `range_start` 对新语法必定有效，并在 parser/sema 里收紧 invariant。但这不是第一步必须做的。

### Sema

核心类型规则不变：

- bounds 必须是整数。
- start/end 同类型。
- step 同类型。
- loop local 不可变。

需要更新 diagnostic 文案，把 `range(...)` 改成新语法。

### IR lowering

无需核心变化。

当前 lowering 已经是 counted loop：

- start/end/step 求值。
- cursor/end/step 存 slot。
- condition block 判断方向。
- body block 绑定不可变 loop local。
- update block 做 cursor += step。

### Tests

新增 parser positive：

```aurex
for i from 0 until limit {
    total += i;
}

for j from 1 until limit {
    total += j;
}

for k from 1 until limit by 2 {
    total += k;
}
```

新增 parser negative：

```aurex
for i from 0 limit {}
for i from until limit {}
for i from 0 until {}
for i from 0 until limit by {}
```

保留迁移期旧语法测试，但标记为 legacy/deprecated。

执行样例需要覆盖：

- 正步长。
- 负步长。
- step 为 0。
- start/end 类型不一致。
- step 类型不一致。
- loop variable 作用域。
- `continue` 走更新点。

## 第一阶段结论

建议把 Aurex counted range loop 第一手改成：

```aurex
for i from start until end {
    ...
}

for i from start until end by step {
    ...
}
```

并明确规定：

- 不做空格敏感。
- 不提供省略 start 的短写。
- `from` / `until` / `by` 作为 contextual marker，不作为 hard keyword。
- `for x in expr` 留给未来真正 iterator protocol。
- 当前 `for i in range(...)` 进入迁移期，最终删除。
