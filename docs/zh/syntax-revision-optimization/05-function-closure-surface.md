# Function / Closure Surface：从 `fn` 闭包骨架迁出

日期：2026-06-13
状态：已被第六手 C++ 风格 capture-list 方案取代
后续规范：`06-function-closure-cpp-capture-list.md`

本文只保留第五手讨论结论：闭包字面量不能继续使用 `fn(...) -> T => ...` 作为源码成功路径。

当时的核心问题是：

- `fn name(...) -> T { ... }` 是函数声明。
- `fn(...) -> T` 是薄函数指针类型。
- `fn(value: T) -> R => expr` 看起来像函数声明写到一半，但实际是闭包表达式。

因此 Aurex 已经把 `fn(...) -> T => ...` / `fn(...) -> T { ... }` 从闭包源码语法中删除。

第五手曾考虑过 pipe lambda，但这个方向现在不再作为当前设计：

```aurex
|value: i32| -> i32 => value + 1
```

它的问题是没有显式捕获列表，后续 value capture、shared reference capture、mutable reference capture 都缺少清晰 source anchor。

当前有效方案是第六手文档固定的 C++ 风格 capture-list：

```aurex
[](value: i32) -> i32 => value + 1
[base](value: i32) -> i32 => value + base
[&base](value: i32) -> i32 => value + base      // 当前 sema 明确拒绝，后续实现引用捕获后放开
[&mut base](value: i32) -> i32 => value + base  // 当前 sema 明确拒绝，后续实现引用捕获后放开
```

这也保持了用户要求的原则：

- 不做空格敏感。
- 不保留旧闭包成功路径。
- 不把 parser recovery 当 legacy 兼容。
- 捕获模式进入 AST，而不是只停留在 parser 装饰层。
