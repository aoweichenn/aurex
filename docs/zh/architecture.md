# 架构设计文档

## 组件

- `src/lex`：词法分析。
- `src/parse`：递归下降 parser。
- `src/syntax`：AST、token、模块路径和 dump。
- `src/sema`：名称解析、类型系统、泛型实例化、pattern exhaustiveness、值语义和控制流检查。
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

未来库层重新设计前应先完成当前基础抽象：

- `unsafe` 边界：raw pointer、unchecked string 和 bit-level cast 不能长期留在 safe surface。
- ADT enum 与 pattern matching：让 Result/Option/AST 状态空间成为主力表达。
- array/slice/string/function type：补齐不依赖 std 的基础值和 ABI 表达。
- trait/where：最小非资源类 `where` capability 已落地；完整 trait / protocol 后置。
- 资源语义：`Copy` / `Drop` / borrow / move-out 暂缓为后续专题，不作为当前架构前置条件。

## M2.5 前端方向

M2.5 的架构主线是 stable-ID-driven query，而不是先堆一层 LSP 适配器：

- 当前第一优先级是冻结 [M2.5 Query Key 设计](m2.5-query-key-design.md)，先把
  Stable Semantic Query Key、Session Fast Handle、CanonicalTypeKey、
  GenericInstanceKey 和 diagnostics query 边界定牢。
- 现有 sema 结果先固定为显式 typed identity、显式诊断 kind 和稳定 fingerprint。
- file parse、module graph、item signature、function body、generic instance、
  diagnostics 逐步改为 query。
- lossless CST / GreenTree 保留 trivia，AST 继续作为语义层消费的 lowered 结构。
- CLI、JSON 和后续 IDE 统一消费结构化 diagnostics，不依赖 message 文本反推语义。
