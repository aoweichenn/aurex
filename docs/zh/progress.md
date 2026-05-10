# 当前进度文档

版本：0.1.2
阶段：M2 language-core-no-std

## 总体状态

当前仓库已经进入 M2 阶段。M2 的目标不是继续修补 M1，而是重新收口语言核心：冻结并删除标准库和 M1 系统样例，把注意力放回基础语法、类型系统、所有权、模式匹配、IR 和 LLVM 后端。

M1 阶段已经舍弃。主要原因不是单个功能失败，而是整体设计方向不稳：

- 标准库、host support、构建工具样例和语言核心同时扩张，导致测试结果很难判断是语言问题、库问题还是工具链问题。
- 部分能力依赖标准库名字或样例约定，而不是语言级规则，例如 copy/drop 约束、资源释放、`Result` / `Option` 形状。
- 语法地基还没有冻结时就推进 selfhost/build-tool 路线，造成前端、库 API、所有权模型互相牵制。
- M1 的很多样例验证的是“能跑通当前 demo”，不是验证语言核心是否稳定、可解释、可长期扩展。

因此 M2 只承认当前 C++ Stage0 编译器、Aurex IR、LLVM backend 和自包含语言样例作为有效基线。当前仓库没有 `std/` 目录，也没有 `selfhost/` 目录；相关旧路线只作为历史输入，不再代表当前进度。

## 已完成能力

当前生产编译器位于 `src/` 和 `include/`，使用 C++20 实现。

- CLI 支持 `--check`、`--dump-*`、`--emit=*`、`--opt-level`、`-I`、`-o`、`--clang` 和 `--clang-arg`。
- driver 能完成文件 IO、模块加载、编译流水线调度、LLVM IR 临时文件生成和 clang 调用。
- module loader 支持根模块和 import 模块合并，能检测模块名不匹配、缺失 import、循环 import、重复模块名和重复加载。
- lexer/parser 是手写实现，输出 ID-backed AST；token、AST、module、checked、IR、LLVM IR 都有 dump 路径。
- 语义分析已具备类型表、符号表、函数签名、ABI 名称、struct/enum 元数据、泛型实例化、表达式类型表和 source range 诊断。
- 当前语言样例覆盖函数、`extern c`、`export c fn`、普通 `fn main`、import、可见性、const、type alias、struct、enum、opaque struct、泛型、method、指针、数组、字段访问、索引、cast、内建 size/align、`if` 表达式、block 表达式、`match`、`while`、`for`、`defer` 和 `?`。
- 程序入口支持根模块普通 `fn main`，签名包括无参数或 `argc/argv`，返回 `i32` 或 `void`。
- Aurex IR 是 typed CFG/SSA-like 中间层，后端只消费 IR，不回读 AST。
- IR verifier 会检查函数、block、value、terminator、类型和引用一致性。
- pass pipeline 支持 `O0` 到 `O3` 选项；当前实际优化以保守的局部 mem2reg 和 CFG cleanup 为主。
- LLVM backend 使用 LLVM C++ API 从 Aurex IR 生成 LLVM IR，并通过 clang 生成 asm、object 和 executable。

## 已删除能力

M2 明确删除并暂缓这些 M1 内容：

- `std/` 源树。
- host C support 源文件和自动链接。
- driver 的标准库查找、import path 自动注入和安装后 std 查找。
- CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 安装规则中的 `share/aurex/std`。
- std/M1/system/build-tool 样例。
- 依赖标准库名字的 ownership 特判。
- selfhost / Stage1 / AIR snapshot 路线在当前仓库中的实现。

这些内容不是永远不能回来，而是必须等语言级 ownership、borrow/drop、capability/trait/where、module/package 和基础语法稳定后再恢复。

## 质量门

当前主要质量门：

```sh
tools/run_tests.sh
tools/bench.py
```

测试覆盖：

- lexer/parser/AST dump。
- driver/CLI/native toolchain。
- positive/negative `.ax` sample suite。
- 模块、可见性、泛型、函数、方法、pattern matching、error handling 和类型系统诊断。
- Aurex IR lowering、IR verifier、pass pipeline、LLVM backend。
- native execution 和安装后 compiler 执行。

当前 `build` 目录可能不是完整测试配置；可信验证应以 `tools/run_tests.sh` 重新 configure/build/ctest 为准。

## M2 当前短板

M2 的核心短板集中在语言地基，不在标准库规模：

- block statement 和 block expression 仍是两套 parser 入口，用户心智模型和实现模型不统一。
- const initializer 支持的纯标量运算不完整，`/`、`%`、shift 和逻辑短路仍被排除。
- 没有 compound assignment，计数、flag、mask 代码偏啰嗦。
- 顶层 item 和 struct field 默认 public，不适合作为长期模块 API 基线。
- enum 仍偏 C enum 语法，base type 和 discriminant 必填，不适合作为主力 ADT 表达。
- 泛型没有 `where`、trait 或 capability predicate，不能表达 `T: Copy`、`T: Drop`、`K: Eq + Hash` 这类基础约束。
- M1 的语言级 `noncopy` / `move` MVP 已从 M2 基线删除。当前先保留普通值语义和必要的数组/含数组类型限制；copy/drop/borrow/ownership 需要重新设计成 capability / trait / where 能表达的语言模型。
- raw pointer 同时承担 FFI、method receiver、临时借用和地址操作，长期需要 safe reference 与 `unsafe` 边界分层。
- `str` 已有语言级雏形，但还缺稳定的 slice、UTF-8 边界和安全/unsafe API 分层。

当前完整语法库存、已支持高级能力、未完成特性和基础语法优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

## 当前结论

M2 的正确目标是先把基础语法和核心语义做窄做稳，再谈标准库、自举和构建工具。当前编译器已经能支撑语言核心实验和 native 输出，但不应把 M1 的 std/selfhost 经验继续当作有效路线推进。

下一步最重要的是冻结 M2 语法基线：统一 block、补齐 const 运算、决定 default private 迁移、设计 `unsafe`、设计 capability/trait/where，并把 enum/pattern matching 从“能用”推进到“适合作为语言核心表达状态空间”。
