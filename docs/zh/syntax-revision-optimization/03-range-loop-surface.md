# Range Loop Surface：保留 `range(...)`，补齐 for-in 协议

日期：2026-06-18
状态：当前实现
关联问题：`for i in range(...)` 是 counted range loop 特例；`for x in expr` 同时支持 array/slice value iteration 和用户 protocol iterator。

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

同时保留并扩展 `for item in expr`：

```aurex
let values: [4]i32 = [1, 2, 3, 4];
for value in values {
    total += value;
}

let view: []i32 = values[:];
for value in view {
    total += value;
}

let counter: Counter = Counter { current: 0, end: 4 };
for value in counter {
    total += value;
}
```

## 当前状态

当前 parser/sema/IR 支持：

- `range(end)`：从 `0` 到 `end`，半开区间。
- `range(start, end)`：从 `start` 到 `end`，半开区间。
- `range(start, end, step)`：从 `start` 到 `end`，每次加 `step`。
- `for item in array_expr { ... }`：array 按值迭代。
- `for item in slice_expr { ... }`：slice 按值迭代。
- `for item in iterator_expr { ... }`：表达式本身作为 protocol iterator。
- `for item in iterable_expr { ... }`：如果表达式提供 `iter()` 且返回 protocol iterator，则使用 `iter()` 返回值作为 iterator。

`range(...)` 真实实现仍不是普通函数调用，也不是通用 iteration。相关入口：

- `src/frontend/parse/grammar/parser_control_stmt.cpp`
  - `next_for_is_range_loop()` 识别 `Identifier "in"`。
  - `parse_for_range_stmt()` 对 `range(` 走 counted range 分支，否则把 `in` 后表达式记录为 `range_iterable`。
  - 参数数量被限制为 1 到 3。
- `include/aurex/frontend/syntax/ast/stmt_nodes.hpp`
  - AST 已有专门的 `StmtKind::for_range`。
  - payload 是 `name`、`range_start`、`range_end`、`range_step`、`range_iterable`、`body`。
- `include/aurex/frontend/sema/checked_module.hpp`
  - `ForInIterationPlan` 记录 counted range、array value、slice value 和 protocol iterator 的 checked lowering 事实。
  - protocol plan 记录 iterator type、item type、source kind、`iter`/`has_next`/`next` 调用 kind 和具体函数 key。
- `src/frontend/sema/internal/expressions/sources/sema_statement_analyzer.cpp`
  - `analyze_for_range_bounds()` 要求 counted range 的 start、end、step 都是整数。
  - array/slice 分支要求元素类型满足 `Copy`。
  - protocol 分支先尝试表达式本身作为 iterator，再尝试 `expr.iter()`。
  - protocol iterator 必须提供 `has_next(self: &mut Iterator) -> bool` 和 `next(self: &mut Iterator) -> Item`。
  - `iter()` receiver 可以是 by-value、`&self` 或 `&mut self`，按现有 receiver 规则匹配。
  - protocol `Item` 不要求 `Copy`。
  - inherent method 和静态 trait dispatch 均可作为协议方法来源；dyn trait vtable-slot dispatch 暂不作为 for-in protocol 来源。
- `src/frontend/sema/internal/place/sources/move_analysis.cpp`
  - array/slice for-in 对 iterable 只要求 initialized place。
  - 直接 protocol iterator 和 consuming `iter(self)` 会 consume iterable。
  - `iter(&self)` / `iter(&mut self)` 分别记录 shared borrow / mutable borrow。
- `src/midend/ir/lowering/sources/lower_ast_stmt.cpp`
  - lowering 优先消费 checked `ForInIterationPlan`。
  - array/slice 分支用 `usize` cursor/end；array 长度来自类型，slice 长度来自 `slice_len`，元素地址用 `index_addr`，每轮 load 成不可变 local。
  - protocol 分支把 iterator 存入隐藏 slot；condition 调用 `has_next(&mut iterator)`，body 调用 `next(&mut iterator)` 并把返回值存成 loop local。
  - protocol iterator、`iter()` 临时 source 和每轮 item 都走现有 cleanup/drop 机制。

也就是说，当前语义层有一个专门的整数 counted loop、一个 array/slice value iteration 子集，以及一个用户协议迭代器子集。

## Protocol Iterator

protocol iterator 是结构协议，不要求声明某个固定内建 trait。满足以下形状即可被 `for item in expr` 使用：

```aurex
impl Counter {
    fn has_next(self: &mut Counter) -> bool {
        return self.current < self.end;
    }

    fn next(self: &mut Counter) -> i32 {
        let value: i32 = self.current;
        self.current = self.current + 1;
        return value;
    }
}
```

也可以通过 trait bound 提供：

```aurex
trait IterI32 {
    fn has_next(self: &mut Self) -> bool;
    fn next(self: &mut Self) -> i32;
}

fn sum<T>(iter: T) -> i32 where T: IterI32 {
    var total: i32 = 0;
    for value in iter {
        total += value;
    }
    return total;
}
```

或者由 `iter()` 产生 iterator：

```aurex
impl Range {
    fn iter(self: &Range) -> RangeIter {
        return RangeIter { current: self.start, end: self.end };
    }
}

impl MutableRange {
    fn iter(self: &mut MutableRange) -> RangeIter {
        self.start = self.start + 1;
        return RangeIter { current: self.start, end: self.end };
    }
}
```

## 当前语义冻结

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

当前 array/slice for-in：

- `expr` 是 array 或 slice。
- loop item 是不可变局部，类型是元素类型。
- 每轮按值 load 一个元素，元素类型必须是 `Copy`。
- 不 move array/slice 本身。
- 不调用用户函数或 trait method。

当前 protocol for-in：

- 表达式本身可以是 iterator；这种情况下 iterable 会被消费进隐藏 iterator slot。
- 表达式可以提供 `iter()`；`iter(self)` 会消费 source，`iter(&self)` 和 `iter(&mut self)` 分别借用 source。
- iterator 必须提供 `has_next(self: &mut Iterator) -> bool`。
- iterator 必须提供 `next(self: &mut Iterator) -> Item`，且 `Item` 不能是 `void`。
- loop item 是不可变局部，类型是 `next()` 返回类型。
- protocol item 不要求 `Copy`。
- 每轮 item 在本轮 body 结束、`continue` 或 `break` 路径上按 cleanup 规则销毁。

## 本轮不做

- 不新增 counted loop 专用 marker 语法。
- 不新增 `from` / `until` / `by` contextual marker。
- 不把 `for i in range(...)` 标为 deprecated。
- 不迁移 tests、samples、examples 里的 range loop 写法。
- 不引入 `..` / `..<` / `..=` range expression。
- 不把 `range(...)` 下沉为普通 range value。
- 不定义标准库 iterator trait 或标准库 adapter。
- 不引入 str 默认 iteration。
- 不引入 reference item / mutable item iteration 表面。
- 不把 dyn trait vtable-slot dispatch 纳入 for-in protocol。

## 后续处理点

后续 iterator/range 专题只剩这些表面需要统一：

1. 决定 `range(...)` 是否从 parser magic 下沉为真实 range value。
2. 决定 str 的默认 iteration item 是 byte、char、rune、shared reference，还是其它 view。
3. 决定 array/slice 是否新增 `&item`、`&mut item` 或 consuming item iteration。
4. 决定是否需要 range literal。
5. 决定标准库 iterable trait/adapter 是否要和当前结构协议并存，还是迁移到名义 trait。
6. 决定 dyn trait iterator dispatch 是否进入语言表面。

当前阶段的工程动作就是：保留原有 `for i in range(...)`，完成 array/slice value for-in 和用户 protocol iterator for-in 的可运行子集，不继续推进更长的 counted loop 语法。
