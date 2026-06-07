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

当前架构基线是 M8 borrowed dyn trait runtime dispatch closure。M2 已移除标准库层：

- 没有 `std/` 源树。
- driver 不查找 std root。
- module loader 不自动追加 std import path。
- native executable 不自动追加 support source。
- install 规则只安装 compiler。

这使语言核心变更能直接通过自包含样例验证，避免标准库加载、host support 和 M1 样例掩盖编译器本身的语义与性能问题。

## 当前架构方向

当前编译器架构已经进入 query-backed、borrow/resource-aware 且支持 borrowed dyn trait dispatch 的形态：

- `unsafe` 边界覆盖 raw pointer、unchecked string 和 bit-level cast。
- ADT enum、pattern matching、array、slice、string 和 function type 构成不依赖 std 的基础值和 ABI 表达。
- nominal static trait、显式 impl、`where` trait predicate、static trait method dispatch、associated type 和 trait
  default method body 已进入稳定 baseline。
- `&dyn Trait` / `&mut dyn Trait` borrowed erased view 已接入 frontend/sema、checked vtable facts、IR verifier、
  LLVM vtable global 和 indirect call；这是 borrowed-only dynamic dispatch，不是 owning object model。
- owning dyn、`Box<dyn Trait>`、allocator、dynamic Drop dispatch、supertrait upcasting、多 trait object
  composition、specialization、default associated type、associated const 和 generic associated type 仍是后续独立设计流。

## M2.5 前端方向

M2.5 的架构主线是 stable-ID-driven query，而不是先堆一层 LSP 适配器：

- 第一批 [M2.5 Query Key 设计](m2.5-query-key-design.md) 已经进入默认增量缓存主路径，
  Stable Semantic Query Key、Session Fast Handle、CanonicalTypeKey、
  GenericInstanceKey 和 diagnostics query 边界已经定牢。
- 现有 sema 结果已经固定为显式 typed identity、显式诊断 kind 和稳定 fingerprint。
- file parse、module graph、item signature、function body、generic instance、
  diagnostics 已有第一批 query row/edge、replay 和 provider-skip profile 覆盖。
- lossless CST / GreenTree 保留 trivia，AST 继续作为语义层消费的 lowered 结构。
- CLI、JSON 和后续 IDE 统一消费结构化 diagnostics，不依赖 message 文本反推语义。
