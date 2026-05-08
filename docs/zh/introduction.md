# 介绍文档

Aurex 是一个系统语言编译器项目。当前 `language-core-no-std` 分支的目标是暂时冻结标准库，把工程重心拉回语言核心：语法、类型系统、泛型、sum type、pattern matching、控制流、所有权、后续 borrow/drop/capability/trait/where 设计，以及 IR/LLVM 后端的稳定性。

本分支已经删除 `std/` 源树、std driver 查找/链接代码、std CLI 选项、std/M1/system 样例和 std 专项测试。需要运行时能力的语言测试应通过局部 `extern c` 声明表达，不再依赖 Aurex 标准库包装。

短期目标：

- 继续完善 `for`、`defer`、`?`、match payload 和 noncopy ownership 语义。
- 把临时 ownership 特例沉淀为语言级 capability predicate，例如 `copy T`、`drop T`。
- 设计正式 borrow/checker、move-out、partial move、drop order 和 destructor 规则。
- 在语法稳定前避免扩张标准库，降低验证成本和测试耗时干扰。
