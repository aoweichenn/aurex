# 架构设计文档

## 组件

- `src/lex`：词法分析。
- `src/parse`：递归下降 parser。
- `src/syntax`：AST、token、模块路径和 dump。
- `src/sema`：名称解析、类型系统、泛型实例化、pattern exhaustiveness、所有权/移动检查。
- `src/ir`：Aurex IR、lowering、验证和 pass pipeline。
- `src/backend/llvm`：LLVM IR 生成。
- `src/driver`：文件读取、模块加载、编译流水线和 clang 调用。
- `src/cli`：`aurexc` 命令行入口。

## 分支边界

M2 `language-core-no-std` 移除了标准库层：

- 没有 `std/` 源树。
- driver 不查找 std root。
- module loader 不自动追加 std import path。
- native executable 不自动追加 support source。
- install 规则只安装 compiler。

这使语言核心变更能直接通过自包含样例验证，避免标准库加载、host support 和 M1 样例掩盖编译器本身的语义与性能问题。

## 后续架构方向

标准库恢复前应先完成语言级抽象：

- capability predicate：`copy T`、`drop T`，之后扩展 `eq K`、`hash K`。
- destructor/drop model：显式析构形状、自动 drop order、panic/early-return 语义。
- borrow model：shared/mutable borrow、生命周期区域、借用返回。
- move-out model：容器或字段的显式取出、partial move 状态。
- trait/where：把泛型约束从 hardcode 迁到可解释、可诊断的语言机制。
