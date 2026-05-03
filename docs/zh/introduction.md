# 介绍文档

版本：0.1.2

## 项目定位

Aurex 是一个面向自举路线的系统语言编译器项目。当前 M0 阶段的重点不是追求语言特性数量，而是把编译器主链路、IR 契约、标准库查找、ABI 边界和自举路径做清楚。

当前生产编译器由 C++20 实现，默认编译路径是：

```text
source -> lexer -> parser -> sema -> Aurex IR -> LLVM IR -> clang
```

## 设计原则

- 成本显式：复制、类型转换、ABI 调用、指针操作和内存分配都应在源码中可见。
- 阶段可替换：lexer、parser、sema、IR、backend 和 driver 之间保持明确边界。
- IR 优先：AST 只表达语法，语义和后端不把状态写回 AST。
- 自举可度量：selfhost 路线按组件推进，并通过 smoke、golden 和 bootstrap 链路验证。
- 标准库可重定位：安装后的 `aurexc` 能从同一前缀下找到 `share/aurex/std`。

## 仓库结构

- `include/aurex`：公共 C++ 头文件，定义阶段间接口和数据结构。
- `src`：Stage0 C++20 编译器实现。
- `std`：Aurex 标准库源码和 backend support。
- `selfhost`：用 M0 编写的 Stage1 自举切片。
- `examples`：最小可运行样例。
- `tests`：正向、反向、golden 和导入路径测试。
- `tools`：测试、bootstrap、golden 对比和 benchmark 脚本。
- `cmake`：构建、安装和工具链配置。

## 当前能力概览

- 手写 lexer 和递归下降 parser。
- 模块加载和 import 搜索路径。
- 语义分析，包括类型、符号、函数、结构体、枚举和基础诊断。
- Aurex typed CFG/SSA-like IR。
- IR verifier、保守的局部 mem2reg 和 CFG cleanup pass pipeline。
- LLVM IR lowering 和 clang 本机输出。
- `std` 模块和 host-c backend support。
- M0 编写的 selfhost lexer/parser/IR emitter 切片。

## 使用场景

- 作为小型系统语言的编译器实验平台。
- 验证 front-end、IR、LLVM lowering 和 native toolchain 的边界。
- 推进 self-host，让 M0 逐步接管 lexer、parser、IR emitter 等组件。
- 作为教学或研究用的可读编译器工程样本。

## 非目标

0.1.2 不声称完整 self-host fixed point，也不声称具备完整生产级优化器。Stage1 当前仍是自举切片，完整 sema、IR verifier 和 LLVM 接入后续推进。

当前文档描述的是 0.1.x 阶段收束后的整体状态，不按每个小版本重复列出细碎变更。历史细节以 git history 为准。
