# Aurex 语法修正优化

日期：2026-06-13
状态：开发期设计决策目录

本目录记录 Aurex 在开发期对语言表面的修正优化。每份文档聚焦一个语法面，先固定问题、决策、parser 规则、迁移路径和测试要求，再进入代码实现。

当前条目：

1. [Angle Bracket Generics：泛型从 `[]` 迁移到 `<...>`](01-angle-bracket-generics.md)
2. [Builtin / Intrinsic Surface：删除硬关键字污染，收束内建能力](02-builtin-intrinsic-surface.md)
3. [Range Loop Surface：拆掉伪 `for-in range(...)`](03-range-loop-surface.md)
4. [Mut / Const Access Surface：把可写权限从深不可变里拆开](04-mut-const-access-surface.md)
5. [Function / Closure Surface：把闭包字面量从 `fn` 骨架里拆出来](05-function-closure-surface.md)
