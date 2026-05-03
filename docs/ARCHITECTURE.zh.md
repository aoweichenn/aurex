# Aurex M0 架构说明

本文档描述当前项目结构、编译管线和自举边界。Aurex M0 仍处于 M0
阶段：生产编译器是 C++20 实现，`selfhost/` 中的 M0 代码是正在扩展的
Stage1 自举编译器切片。

## 顶层结构

- `include/aurex/`：生产编译器公共头文件，按编译器阶段分层。
- `src/base/`：基础设施，包括源码缓冲区、诊断、整数和文本工具。
- `src/syntax/`：AST、模块路径和 AST dump。该层不依赖 lexer/parser。
- `src/lex/`：手写词法分析器，输出稳定 token 流。
- `src/parse/`：手写递归下降解析器，输入 token，输出 parse-only AST。
- `src/sema/`：类型表、符号表和语义检查，输出 `CheckedModule` 边表。
- `src/ir/`：Aurex 自有中间代码，负责把 AST + sema 边表降为 typed CFG/SSA
  形态。该层不依赖 LLVM，是后端无关的内部契约。
- `src/backend/llvm/`：LLVM 后端，把 Aurex IR lowering 到 LLVM IR。当前
  默认输出、`--emit=llvm-ir`、`--emit=asm`、`--emit=obj` 和 `--emit=exe`
  都消费这个后端。
- `src/driver/`：编译驱动、模块加载器和 clang 本机输出封装，负责 import
  解析、跨模块合并以及 LLVM IR/汇编/object/可执行文件输出选择。
- `src/cli/`：`aurexc` 命令行入口。
- `cmake/`：按编译器组件拆分的构建定义，根 `CMakeLists.txt` 只负责组装。
- `std/`：Aurex 标准库模块。`std.*` 默认进入 import 搜索路径；
  `std/support/host_c.c` 是当前默认 native 输出自动链接的 host-c backend
  support，稳定主机侧符号采用 `aurex_std_v0_*` 命名。
- `bootstrap/`：单文件 Stage0-mini 编译器，用于证明最小翻译链路。
- `selfhost/`：M0 编写的自举编译器源码、运行时和自举验证。
- `tests/`：正向、反向、import 和 golden 测试语料。
- `tools/`：测试、自举链路、golden 检查和基准脚本。

## 生产编译管线

当前 `aurexc` 的主路径是：

1. `driver` 读取根源码，并通过 `module_loader` 解析 import。
2. `lex` 将每个源码文件扫描为 token。
3. `parse` 将 token 转为 AST。AST 只表达语法事实，不保存类型检查结果。
4. `sema` 建立类型、符号、函数、结构体和枚举 case 边表。
5. `ir` 可以把 AST 与 `CheckedModule` 降为 Aurex IR，用 `--emit=ir` 观察。
6. `ir` pass pipeline 根据 `--opt-level` 运行 verifier、局部 mem2reg 和 CFG cleanup。
7. `backend/llvm` 消费 Aurex IR 并 lowering 到 LLVM IR，用 `--emit=llvm-ir` 观察。
8. `driver` 把 LLVM IR 交给 clang，输出汇编、object 或本机可执行文件。默认输出是本机可执行文件；生成可执行文件时会按 `--std-backend` 自动链接 std backend support。

这种分层刻意避免让 parser 依赖 lexer 实现，也避免把语义信息写回 AST。
后续替换前端或把组件迁移到 M0 时，每个阶段都有清晰接口。
当前 clang 集成是 driver 层的封装：`--emit=llvm-ir` 输出 LLVM lowering 结果，
默认输出、`--emit=asm`、`--emit=obj` 和 `--emit=exe` 复用 LLVM 后端并调用 clang。
生产 C 后端已经从 Stage0 构建链路中移除。

## Aurex IR 与 LLVM 路线

当前新增的 Aurex IR 不是 LLVM IR 文本直出，而是项目自己的 typed CFG/SSA
中间层。这样做的原因是：前端语义、C ABI 互操作、后端优化和 LLVM lowering
需要一个稳定的内部协议，不能把 AST 直接绑死到 C 或 LLVM 的打印格式上。

IR 当前具有这些性质：

- `Module` 持有 `TypeTable`、函数表和值表。
- 全局 `const` 和 enum case 进入 `Module.constants`，由普通 `Value` 表达初始化式；
  LLVM 后端会把它们物化成只读 `global constant`，运行期引用再生成 load。
- `Function` 显式记录源码名、ABI symbol、linkage、调用约定、返回类型和参数签名。
- `Linkage` 区分 `internal`、`export_c`、`extern_c`，LLVM lowering 可直接映射到符号可见性和外部声明。
- 控制流由 basic block + terminator 表示，terminator 包含 `br`、`br_if` 和 `ret`。
- 局部变量、参数 shadow copy 和可写 storage 先降为 `alloca/load/store`，后续可以做 mem2reg/SSA 构造。
- 字段和下标访问先降为 `field_addr` / `index_addr`，再由 `load/store` 使用，便于 LLVM GEP lowering。
- 函数调用记录最终 ABI symbol，也记录可解析的 `FunctionId`，避免后续阶段重新查 AST 名字。
- `&&` / `||` 降成 CFG + `phi`，不再当作普通二元指令，因此短路语义在 IR 中是显式的。
- 编译期常量单独进入 IR 常量表，避免 `const` 和 enum case 被误当成普通局部值。

后续推荐的后端路线：

```text
AST + CheckedModule
  -> Aurex IR
  -> IR verifier
  -> pass pipeline: local mem2reg / CFG cleanup / 后续常量折叠
  -> 后端选择：LLVM IR / 未来自研后端
  -> LLVM target machine 输出 asm/object/exe
```

当前状态可以概括为：LLVM 主链路已经搭好，M0 正向样例、`std` 样例和
selfhost smoke 入口都可以经 Stage0 的 Aurex IR -> LLVM IR -> clang 编译运行。
IR pass pipeline 已有独立入口，`O0` 只验证，`O1` 及以上启用当前保守的局部
mem2reg 和 CFG cleanup。还不能称为完整工业级后端的部分包括：完整 SSA 构造、
跨块 mem2reg、常量折叠、更完整 ABI 属性和未来自研后端代码生成。LLVM 只是当前第一个生产后端，
不能把 LLVM 私有语义写回 Aurex IR；自研后端后续应消费同一个 IR、verifier 和 ABI 描述。

## CMake 组件边界

构建定义现在拆分在 `cmake/`：

- `AurexBase.cmake` 定义 `m0_base`。
- `AurexSyntax.cmake` 定义 `m0_syntax`。
- `AurexFrontend.cmake` 定义 `m0_lex` 和 `m0_parse`。
- `AurexSema.cmake` 定义 `m0_sema`。
- `AurexIr.cmake` 定义 `m0_ir`。
- `AurexLLVM.cmake` 定义 `m0_llvm`，集中发现 LLVM 并暴露 include/link 设置。
- `AurexBackendLLVM.cmake` 定义 `m0_backend_llvm`。
- `AurexDriver.cmake` 定义 `m0_driver`。
- `AurexTools.cmake` 定义 `aurexc`。
- `AurexWarnings.cmake` 集中管理编译告警。

依赖方向保持单向：base -> syntax/frontend -> sema -> ir/backend -> driver -> cli。
新增目标时应优先放入对应组件文件，而不是把目标堆回根 CMake。

## 自举编译器边界

`selfhost/src/aurex/selfhost/` 按角色组织：

- `bin/`：可执行入口，当前包括 `aurexc_seed.ax` 和 `aurexc_stage1.ax`。
- `lexer/`：M0 词法器核心与 token dump 工具。
- `syntax/`：M0 AST 数据模块。当前已落地 ID-backed 节点池，覆盖路径、
  顶层 item、类型、参数、block、statement 和 expression。
- `parser/`：M0 parser seed，按职责拆成 `cursor.ax`、`types.ax`、`expr.ax`
  和 `seed.ax`。其中类型解析用显式指针前缀栈，表达式解析用 operator/frame
  栈，当前已能为覆盖的语法返回 `AstModule` 节点图。
- `compiler/`：Stage1 编译器切片。
- `compiler/ir/`：Stage1 Aurex IR 输出模块，按 writer、name mangling、type、
  expression 和 AST-to-IR emission 拆分。
- `smoke/`：自举能力回归测试。
- `tool/`：golden 测试使用的小工具。

Stage1 目前不是完整 AST 编译器，而是 M0 编写的 AST-to-Aurex-IR seed。
它能把 parser seed 覆盖的 M0 子集输出为 `aurex_ir v0` 快照；对尚未覆盖
的 selfhost 编译器模块，会输出稳定的 `lowering(ast_pending)` 记录，避免
把未完成的 lowering 伪装成可执行编译结果。

## 当前 Stage1 能力

Stage1 已覆盖自举 smoke 所需的核心面：

- module/import 外壳和多源码 bundle 输出。
- parser seed 已从纯语法校验推进到生成 `AstModule` 节点图，覆盖 module path、
  import、extern block、extern fn、opaque struct、export fn、参数、类型、ABI 名称、
  block、表达式语句、`return` statement、调用、调用参数池、字面量、标识符、
  一元表达式和基于显式 operator 栈的二元表达式树。调用参数现在按完整表达式
  解析，支持分组、优先级和一元前缀。
- `aurexc_stage1 <输入.ax> <输出.air>` 能为 `examples/hello.ax`、selfhost seed
  和 parser smoke 输出 Aurex IR 快照。
- 多源码输入会生成一个 IR bundle；对完整 lowering 尚未覆盖的 selfhost 模块，
  输出 deterministic pending-lowering marker。
- 旧 selfhost C emitter、C bundle emitter 和 C 固定点检查已经从活跃链路移除。

仍未完成的部分包括完整生产级 AST parser、完整语义分析、诊断质量、Stage1 IR
verifier、把 Stage1 IR 接入现有 LLVM 后端，以及全量生产编译器自编译。相关状态以
`docs/SELFHOST.md` 和 `tools/bootstrap_chain.sh` 为准。

## 质量门

常用命令：

```sh
tools/run_tests.sh
tools/bootstrap_chain.sh
make -C selfhost check
```

`tools/run_tests.sh` 是总质量门；`tools/bootstrap_chain.sh` 重点验证自举链路；
`make -C selfhost check` 适合只改 selfhost 代码时快速回归。

## 扩展约定

- 新语言特性应先补生产 Stage0 的 parse/sema/IR/LLVM lowering，再补 Stage1 支持。
- Stage1 新能力必须进入 `selfhost/src/aurex/selfhost/smoke/` 或自举链路断言。
- AST 保持语法层数据结构，语义结果继续放在 `CheckedModule` 边表。
- import 相关行为必须覆盖 `tests/imports/` 语料。
- FFI 改动必须同时覆盖 Aurex IR verifier、LLVM lowering 和标准库 native 支持链接测试。
