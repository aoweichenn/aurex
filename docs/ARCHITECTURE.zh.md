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
- `src/codegen_c/`：C 后端，包括 C 类型格式化、表达式求值顺序和 emitter。
- `src/driver/`：编译驱动和模块加载器，负责 import 解析和跨模块合并。
- `src/cli/`：`m0c` 命令行入口。
- `cmake/`：按编译器组件拆分的构建定义，根 `CMakeLists.txt` 只负责组装。
- `runtime/`：Aurex 程序可显式 import 的运行时模块。
- `bootstrap/`：单文件 Stage0-mini 编译器，用于证明最小翻译链路。
- `selfhost/`：M0 编写的自举编译器源码、运行时和自举验证。
- `tests/`：正向、反向、import 和 golden 测试语料。
- `tools/`：测试、自举链路、golden 检查和基准脚本。

## 生产编译管线

当前 `m0c` 的主路径是：

1. `driver` 读取根源码，并通过 `module_loader` 解析 import。
2. `lex` 将每个源码文件扫描为 token。
3. `parse` 将 token 转为 AST。AST 只表达语法事实，不保存类型检查结果。
4. `sema` 建立类型、符号、函数、结构体和枚举 case 边表。
5. `codegen_c` 使用 AST 与 `CheckedModule` 生成 C。
6. 外部 C 编译器负责将生成的 C 编译为本机程序。

这种分层刻意避免让 parser 依赖 lexer 实现，也避免把语义信息写回 AST。
后续替换前端或把组件迁移到 M0 时，每个阶段都有清晰接口。

## CMake 组件边界

构建定义现在拆分在 `cmake/`：

- `AurexBase.cmake` 定义 `m0_base`。
- `AurexSyntax.cmake` 定义 `m0_syntax`。
- `AurexFrontend.cmake` 定义 `m0_lex` 和 `m0_parse`。
- `AurexSema.cmake` 定义 `m0_sema`。
- `AurexCodegenC.cmake` 定义 `m0_codegen_c`。
- `AurexDriver.cmake` 定义 `m0_driver`。
- `AurexTools.cmake` 定义 `m0c`。
- `AurexWarnings.cmake` 集中管理编译告警。

依赖方向保持单向：base -> syntax/frontend -> sema -> codegen -> driver -> cli。
新增目标时应优先放入对应组件文件，而不是把目标堆回根 CMake。

## 自举编译器边界

`selfhost/src/aurex/selfhost/` 按角色组织：

- `bin/`：可执行入口，当前包括 `m0c_seed.ax` 和 `m0c_stage1.ax`。
- `lexer/`：M0 词法器核心与 token dump 工具。
- `syntax/`：M0 AST 数据模块。当前已落地 ID-backed 节点池，覆盖路径、
  顶层 item、类型、参数、block、statement 和 expression。
- `parser/`：M0 parser seed，按职责拆成 `cursor.ax`、`types.ax`、`expr.ax`
  和 `seed.ax`。其中类型解析用显式指针前缀栈，表达式解析用 operator/frame
  栈，当前已能为覆盖的语法返回 `AstModule` 节点图。
- `compiler/`：Stage1 编译器切片。
- `compiler/emit/`：Stage1 token-stream C emitter 的模块化实现。
- `compiler/imports.ax`：Stage1 入口模块加载器，解析 `import`，推导 import
  root，按依赖优先顺序把源码送入 bundle emitter。
- `smoke/`：自举能力回归测试。
- `tool/`：golden 测试使用的小工具。

Stage1 目前不是完整 AST 编译器，而是 M0 编写的 token-stream C emitter。
它能读取受支持的 M0 子集并直接生成 C。这个实现方式适合 M0 自举阶段：
代码量小、依赖少、每个新增语法点都能通过固定点 smoke 链路验证。

## 当前 Stage1 能力

Stage1 已覆盖自举 smoke 所需的核心面：

- module/import 外壳和多源码 bundle 输出。
- parser seed 已从纯语法校验推进到生成 `AstModule` 节点图，覆盖 module path、
  import、extern block、extern fn、opaque struct、export fn、参数、类型、ABI 名称、
  block、表达式语句、`return` statement、调用、调用参数池、字面量、标识符、
  一元表达式和基于显式 operator 栈的二元表达式树。调用参数现在按完整表达式
  解析，支持分组、优先级和一元前缀。
- 单入口 import-aware 编译：`m0c_stage1 <入口.ax> <输出.c>` 能读取入口
  `import` 并自行加载依赖，当前已用于从 `m0c_stage1.ax` 生成 Stage2 编译器。
- `extern c`、`export c fn`、ABI 名称和主函数包装。
- 标量类型、`str`、指针、数组、命名类型、opaque C struct。
- `let`、`var`、赋值、`return`、`if`、`else`、`else if`、`while`、`break`、`continue`。
- `cast`、`ptr_cast`、`bit_cast`、`size_of`、`align_of`、`ptr_addr`、`ptr_from_addr`。
- struct/enum/const 输出、嵌套 struct literal、指针字段访问和字段赋值。
- Stage2/Stage3 smoke 编译器输出 byte-for-byte 固定点检查。

仍未完成的部分包括完整生产级 AST parser、完整语义分析、诊断质量、生产 C 后端迁移
以及全量生产编译器自编译。当前 Stage1 的 import loader 只覆盖
`module`/`import` 头部和 `selfhost/src` 风格的模块路径。相关状态以
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

- 新语言特性应先补生产 Stage0 的 parse/sema/codegen，再补 Stage1 支持。
- Stage1 新能力必须进入 `selfhost/src/aurex/selfhost/smoke/` 或自举链路断言。
- AST 保持语法层数据结构，语义结果继续放在 `CheckedModule` 边表。
- import 相关行为必须覆盖 `tests/imports/` 语料。
- C 后端改动需要同时考虑表达式求值顺序和生成 C 的可读性。
