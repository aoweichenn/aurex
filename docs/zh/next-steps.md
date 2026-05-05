# 下一步计划文档

版本：0.1.2

## 当前阶段

旧 selfhost 自举路线已经从 active tree 删除。当前项目重心是继续推进 C++ Stage0、Aurex IR 和 LLVM 后端，让语言核心具备足够强的表达力、模块隔离能力和后端契约。新的自举实现预计在 M3 用带有新特性的 Aurex 重新编写，不再维护旧 Stage1 种子代码。

## 当前已具备能力

Stage0 主链路：

- 手写 lexer 和递归下降 parser。
- 模块声明、import 搜索路径和标准库查找。
- 语义分析、类型表、符号表、ABI 名称和基础诊断。
- Aurex typed CFG/SSA-like IR、IR verifier 和保守 pass pipeline。
- LLVM IR lowering，并通过 clang 输出 assembly、object 和 executable。
- build-tree 与 install-tree 下的 std 查找和 host-c backend support。

当前语言切片：

- 结构体、枚举、类型别名和 opaque 类型。
- 泛型 struct / enum 的基础实例化。
- `match` 表达式、literal pattern、wildcard、or-pattern 和 guard。
- block / if 表达式。
- local 和 return 类型推导的受控切片。
- 函数原型与递归函数检查。
- `pub` / `priv` 可见性关键字、跨模块 private item 过滤和 private field 访问检查。

## 关键语言缺口

- 可见性还应继续扩展到更细粒度的 API 边界，例如构造器、枚举 payload、type alias 传播和 re-export 规则。
- 模块隔离还缺显式 package/crate 边界、导入别名、选择性导入、循环依赖诊断优化和稳定 public surface dump。
- 泛型仍缺约束、where-like predicate、trait/interface 设计、单态化缓存策略和诊断可解释性。
- pattern matching 还需要更完整的 exhaustiveness、绑定一致性、enum layout 交互和 lowering 验证。
- AIR 仍需要作为 Stage0 内部后端契约继续强化：slot/lvalue descriptor、record/enum layout、phi/SSA 合流、dominance、call signature 和跨模块 item binding 都应可验证。
- LLVM 后端需要继续跟进新特性的 lowering，避免语言前端特性只停留在 check/dump 层。

## 优先级

1. 完成模块可见性与隔离基线  
   `pub` / `priv` 已落地，下一步应补 re-export、导入别名、选择性导入和 public API dump。模块系统是后续自举、包管理和大型项目可维护性的前提。

2. 强化 sum type / pattern matching 到工业可用边界  
   重点是 exhaustiveness、unreachable arm、payload binding、guard 约束和 enum layout 与 LLVM lowering 的一致性。

3. 把泛型从基础实例化推进到可约束模型  
   先设计最小 trait/interface 或 capability predicate，再推进 generic function、method-like resolution 和单态化缓存。

4. 稳定 AIR/IR 后端契约  
   AIR 先作为 Stage0 内部设计目标推进到可验证形态，LLVM 继续作为当前生产后端。自举只要求未来能输出 AIR 级别的结构，不急于自举后端。

5. 改善诊断与 public surface tooling  
   给模块边界、泛型约束、match 覆盖和可见性错误提供稳定、可测试的错误信息。大型语言特性没有诊断就无法进入工程可用状态。

## 后续自举策略

M3 前不维护旧 selfhost。新的自举实现应基于届时稳定下来的 Aurex 特性重新写，包括模块隔离、显式可见性、泛型/约束、sum type、pattern matching 和 AIR 输出。阶段目标先做到能生成 AIR，再设计后端 handoff；LLVM 仍作为生产后端继续承载新特性。
