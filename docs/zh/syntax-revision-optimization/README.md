# Aurex 语法修正优化

日期：2026-06-13
状态：开发期设计决策目录

本目录记录 Aurex 在开发期对语言表面的修正优化。每份文档聚焦一个语法面，先固定问题、决策、parser 规则、迁移路径和测试要求，再进入代码实现。

当前条目：

1. [Angle Bracket Generics：泛型从 `[]` 迁移到 `<...>`](01-angle-bracket-generics.md)
2. [Builtin Surface：收窄类型查询和显式转换表面](02-builtin-surface.md)
3. [Range Loop Surface：保留原来的 `range(...)` 写法](03-range-loop-surface.md)
4. [Mut / Const Access Surface：把可写权限从深不可变里拆开](04-mut-const-access-surface.md)
5. [Function / Closure Surface：把闭包字面量从 `fn` 骨架里拆出来](05-function-closure-surface.md)
6. [Function / Closure Surface：C++ 风格 capture-list 第一批落地](06-function-closure-cpp-capture-list.md)
7. [Builtin Member Projection：str / slice 的 `.len` 和 `.ptr`](07-builtin-member-projection.md)
