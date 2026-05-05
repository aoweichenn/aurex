# 介绍文档

版本：0.1.2

## 项目定位

Aurex 是一个面向自举路线的系统语言编译器项目。当前阶段的重点是先把 C++ Stage0、IR 契约、LLVM 后端、标准库查找、ABI 边界和当前语言特性做稳。

当前生产编译器由 C++20 实现，默认编译路径是：

```text
source -> lexer -> parser -> sema -> Aurex IR -> LLVM IR -> clang
```

## 设计原则

- 成本显式：复制、类型转换、ABI 调用、指针操作和内存分配都应在源码中可见。
- 阶段可替换：lexer、parser、sema、IR、backend 和 driver 之间保持明确边界。
- IR 优先：AST 只表达语法，语义和后端不把状态写回 AST。
- 自举后置：旧自举实验已移除，新的自举会在 M3 基于稳定后的语言特性重写。
- 标准库可重定位：安装后的 `aurexc` 能从同一前缀下找到 `share/aurex/std`。

## 仓库结构

- `include/aurex`：公共 C++ 头文件，定义阶段间接口和数据结构。
- `src`：Stage0 C++20 编译器实现。
- `std`：Aurex 标准库源码和 backend support。
- `examples`：可运行样例，包括共享模块和系统级小项目。
- `tests`：正向、反向、golden 和导入路径测试。
- `tools`：测试、golden 对比和 benchmark 脚本。
- `cmake`：构建、安装和工具链配置。

## 当前能力概览

- 手写 lexer 和递归下降 parser。
- 模块加载和 import 搜索路径。
- 语义分析，包括类型、符号、函数、结构体、枚举和基础诊断。
- Aurex typed CFG/SSA-like IR。
- IR verifier、保守的局部 mem2reg 和 CFG cleanup pass pipeline。
- LLVM IR lowering 和 clang 本机输出。
- `std` 模块和 host-c backend support。
- 当前语言切片，包括显式可见性、泛型基础、generic function MVP、sum type、pattern matching、
  表达式、受控推导、`extern c` 变长参数和作用域级 `defer`。

## 使用场景

- 作为小型系统语言的编译器实验平台。
- 验证 front-end、IR、LLVM lowering 和 native toolchain 的边界。
- 为 M3 之后重新设计自举链路准备语言特性和 IR 契约。
- 作为教学或研究用的可读编译器工程样本。

## 非目标

0.1.2 不声称完整 self-host fixed point，也不声称具备完整生产级优化器。旧自举切片已经移除；当前生产能力由 C++ Stage0 和 LLVM 后端承载。

当前文档描述的是 0.1.x 阶段收束后的整体状态，不按每个小版本重复列出细碎变更。历史细节以 git history 为准。
