# 架构设计

## 组件边界

- `src/lex`：词法分析、token、关键字和 source range。
- `src/frontend/parse`：递归下降 parser、recovery、语法分区。
- `src/frontend/syntax`：AST、AST arena、module、dump。
- `src/frontend/sema`：名称解析、类型系统、泛型实例化、trait/capability、borrow/resource、控制流和诊断。
- `src/midend/ir`：Aurex IR、AST lowering、验证和 pass pipeline。
- `src/backend/llvm`：LLVM IR 类型和值生成、runtime ABI 形状映射。
- `src/application/driver`：模块加载、编译流水线、增量缓存、工具链调用。
- `src/application/tooling`：IDE/LSP/query/tooling 投影。
- `src/cli`：`aurexc` 命令行入口。

## 当前架构原则

- 标准库不在当前树中；driver 不自动查找 std root，也不在 native 输出中追加 support source。
- parser 只负责语法形状；类型、能力、借用、move 和 lowering 语义由 sema/IR 阶段验证。
- checked sema 结果是 IR lowering、backend、diagnostics、query/cache 和 tooling 的事实源。
- 泛型实例身份不能依赖 session-local handle；稳定身份由结构化 key/fingerprint 承担。
- borrow/resource 规则在 sema 中形成结构化 facts，再由 lowering 消费 cleanup/drop/loan 结果。
- 宏系统当前只保留 parser/query/admission 边界，不执行用户代码、不消费 generated token、不修改 AST。

## 当前语言数据流

1. driver 根据根文件和 `-I` 加载模块。
2. lexer/parser 生成 AST。
3. sema 解析名称、类型、泛型、trait/capability、控制流、move/borrow/resource。
4. checked module 保存类型、函数、泛型实例、lambda、borrow/resource、dyn ABI 和诊断 facts。
5. IR lowering 只消费 checked 事实，不回读 raw AST 猜语义。
6. IR verifier/pass pipeline 保持 target-independent IR 合法。
7. LLVM backend 输出 LLVM IR，再由 clang 生成 asm/object/executable。
