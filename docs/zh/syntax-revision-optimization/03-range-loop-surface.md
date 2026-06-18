# Range Loop Surface：保留 `range(...)`，补齐 array/slice for-in

日期：2026-06-18
状态：当前实现
关联问题：`for i in range(...)` 是 counted range loop 特例；`for x in expr` 当前只支持 array/slice 子集

本轮结论：不引入更长的 counted-loop marker，继续使用：

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

同时已补齐当前最小可用的 array/slice for-in：

```aurex
let values: [4]i32 = [1, 2, 3, 4];
for value in values {
    total += value;
}

let view: []i32 = values[:];
for value in view {
    total += value;
}
```

真正完整的问题仍是 iterator / range protocol。后续涉及迭代器时，再把用户自定义 iterator、text iteration、
mutable iteration、range value 和 generic iterable capability 一起设计。

## 当前状态

当前 parser/sema/IR 支持：

- `range(end)`：从 `0` 到 `end`，半开区间。
- `range(start, end)`：从 `start` 到 `end`，半开区间。
- `range(start, end, step)`：从 `start` 到 `end`，每次加 `step`。
- `for item in array_expr { ... }`：array 按值迭代。
- `for item in slice_expr { ... }`：slice 按值迭代。

`range(...)` 真实实现不是普通函数调用，也不是通用 iteration。相关入口：

- `src/frontend/parse/grammar/parser_control_stmt.cpp`
  - `next_for_is_range_loop()` 识别 `Identifier "in"`。
  - `parse_for_range_stmt()` 对 `range(` 走 counted range 分支，否则把 `in` 后表达式记录为 `range_iterable`。
  - 参数数量被限制为 1 到 3。
- `include/aurex/frontend/syntax/ast/stmt_nodes.hpp`
  - AST 已有专门的 `StmtKind::for_range`。
  - payload 是 `name`、`range_start`、`range_end`、`range_step`、`range_iterable`、`body`。
- `src/frontend/sema/internal/expressions/sources/sema_statement_analyzer.cpp`
  - `analyze_for_range_bounds()` 要求 start、end、step 都是整数。
  - start/end 必须同类型，step 必须和 bounds 同类型。
  - iterable 分支要求表达式类型是 array 或 slice。
  - iterable element type 必须满足 `Copy`。
  - loop local 是不可变局部。
  - sema 会记录 checked `ForInIterationPlan`，把 counted range、array value iteration、slice value iteration 作为后续阶段可查询的语义事实。
- `src/midend/ir/lowering/sources/lower_ast_stmt.cpp`
  - lowering 优先消费 `ForInIterationPlan`，缺失时仅保留旧推断路径作为兼容 fallback。
  - `lower_for_range()` 把缺省 start 降成 `0`。
  - 缺省 step 降成 `1`。
  - 显式 step 会存入临时 slot。
  - condition 对正步长用 `cursor < end`，对负步长用 `cursor > end`，step 为 0 时循环零次。
  - iterable 分支用 `usize` cursor/end；array 长度来自类型，slice 长度来自 `slice_len`，元素地址用 `index_addr`，每轮 load 成不可变 local。

也就是说，当前语义层有一个专门的整数 counted loop，还有一个 array/slice value iteration 子集；仍未扩展成真正 iterator。

## 为什么保留

### 1. 原语法更短

`for i in range(...)` 虽然现在是 parser magic，但它短、直接、已有测试和样例覆盖。更长的 marker 方案只是把参数名摊开写到语法层，并没有解决 iterator 本身的问题。

### 2. 不为临时阶段冻结新表面

如果现在新增一套 counted loop 专用 marker，后续 iterator / range protocol 成型后还要重新判断它是否保留、兼容或删除。这个承诺太早，收益不够。

### 3. 完整问题仍属于 iterator 设计

后续真正要回答的是：

- `for x in expr` 如何查找 iterator。
- `range(...)` 是内建 range value、标准库函数，还是继续保留编译器内建。
- str 的默认 iteration item 是 byte、char、rune、shared reference，还是其它 view。
- array/slice 是否需要 `&item`、`&mut item` 或 consuming item iteration。
- `break`、`continue`、ownership、borrow 如何和 iterator 状态交互。
- generic 函数如何表达 iterable 能力。

这些问题没有一起定下来之前，不做局部语法替换。

## 当前语义继续冻结

当前 `for i in range(...)` 表示整数 counted range loop：

- 区间是半开区间 `[start, end)`。
- `range(end)` 的 start 默认为 `0`。
- `range(start, end)` 的 step 默认为 `1`。
- start、end、step 都必须是整数。
- start/end 必须同类型。
- 显式 step 必须和 bounds 同类型。
- `step > 0` 时，当 `cursor < end` 执行循环。
- `step < 0` 时，当 `cursor > end` 执行循环。
- `step == 0` 时循环零次，保持当前行为。
- loop variable 是不可变局部，只在 loop body 里可见。
- start、end、step 按源码顺序各求值一次。
- `continue` 跳到隐藏的 cursor 更新点。
- `break` 跳出整个 range loop。

当前 `for item in expr` 的 iterable 分支：

- `expr` 必须是 array 或 slice。
- loop item 是不可变局部，类型是元素类型。
- 每轮按值 load 一个元素，元素类型必须是 `Copy`。
- 不 move array/slice 本身。
- 不调用用户函数或 trait method。

## 本轮不做

- 不新增 counted loop 专用 marker 语法。
- 不新增 `from` / `until` / `by` contextual marker。
- 不把 `for i in range(...)` 标为 deprecated。
- 不迁移 tests、samples、examples 里的 range loop 写法。
- 不引入 `..` / `..<` / `..=` range expression。
- 不设计通用 iterator protocol。
- 不引入新的 builtin/intrinsic 名字。
- 不定义 user iterator trait、range value 或 standard-library adapter。

## 后续处理点

等进入 iterator / range protocol 专题时，再统一处理：

1. 决定 `for item in expr` 的正式语义和 desugar 规则。
2. 决定 `range(...)` 是否从 parser magic 下沉为真实 range value。
3. 决定 str、array、slice 和用户类型的默认 iteration item 类型。
4. 决定 mutable iteration 和 borrow checker 的交互。
5. 决定是否需要 range literal。
6. 决定当前 counted range parser 特例是直接替换、迁移诊断，还是在开发期直接硬切。

当前阶段的工程动作就是：保留原有 `for i in range(...)`，完成 array/slice for-in 的可运行子集，
不要继续推进更长的 counted loop 语法。
