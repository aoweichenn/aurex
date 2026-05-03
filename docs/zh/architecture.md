# 架构设计文档

版本：0.1.2

## 总体分层

```text
base
  -> syntax
  -> lex / parse
  -> sema
  -> ir
  -> backend/llvm
  -> driver
  -> cli
```

依赖方向保持单向。AST 不依赖 lexer/parser 实现，sema 不写回 AST，backend 不读取源码语法节点。

## 架构原则

- 阶段输出显式化：每个阶段产生可 dump、可测试或可复用的数据。
- 语法与语义分离：AST 只保存源码结构，类型、符号和 ABI 结果进入 checked metadata。
- 后端隔离：LLVM 后端只消费 Aurex IR，不回读 AST。
- 标准库双层设计：语言层 `.ax` API 与 host/backend support 解耦。
- 安装布局可重定位：运行时查找不绑定开发机源码路径。

## 组件说明

- `include/aurex/base` / `src/base`：整数别名、结果类型、诊断、源码管理、ABI 常量。
- `include/aurex/syntax` / `src/syntax`：token、AST ID、模块路径和 AST dump。
- `src/lex`：手写 lexer。
- `src/parse`：手写递归下降 parser。
- `src/sema`：类型表、符号表和语义分析。
- `src/ir`：Aurex IR、IR dump、lowering、verifier、pass pipeline。
- `src/backend/llvm`：LLVM IR lowering。
- `src/driver`：模块加载、标准库查找、native toolchain 调用。
- `src/cli`：命令行解析。
- `std`：Aurex 标准库模块和 backend support。
- `selfhost`：M0 编写的自举切片。

## 关键数据边界

- lexer 输出 `Token`：kind、文本片段、source range。
- parser 输出 `AstModule`：ID-backed type、expr、stmt、item 和 module 信息。
- sema 输出 `CheckedModule`：类型表、表达式类型、符号、ABI 名称、函数签名和记录布局元数据。
- IR lowering 输出 `ir::Module`：typed CFG/SSA-like 函数、block、value、terminator 和全局常量。
- LLVM backend 输出 LLVM IR 文本，再由 clang 生成 native 产物。

## 编译路径

```text
root source
  -> ModuleLoader
  -> Lexer
  -> Parser
  -> SemanticAnalyzer
  -> IR Lowerer
  -> IR Pass Pipeline
  -> LLVM Backend
  -> clang
```

## 标准库架构

标准库由两部分组成：

- `.ax` 模块：例如 `std.c`、`std.text`、`std.mem`、`std.file`。
- backend support：当前默认是 `std/support/host_c.c`。

host support 使用 `aurex_std_v0_*` 稳定符号。`std/native_support.c` 仅作为旧路径兼容入口。

ABI 策略：

- 新声明应使用 `aurex_std_v0_*`。
- 旧 `aurex_std_*` wrapper 只用于兼容旧 Stage1/selfhost 片段。
- 如果 host support 发生不兼容变更，应新增 `aurex_std_v1_*`，不复用 v0 名称。

backend support 策略：

- `host-c` 是默认 backend，适合当前 clang/native 链路。
- `none` 可用于不需要 std support 链接的场景。
- 后续 backend 应通过 driver 选择，不应把实现绑死到 `.ax` 标准库模块。

## 安装与查找架构

安装目标：

```text
<prefix>/bin/aurexc
<prefix>/share/aurex/std
```

运行时标准库查找优先使用显式配置，然后尝试 `aurexc` 可执行文件相对路径，最后回退到当前工作目录的 `std`。这使 build tree、install tree 和本地开发目录都能工作。

## 错误与诊断架构

`DiagnosticSink` 收集带 source range 的错误。driver 在 lex/parse/sema 失败时打印文件、行列、错误级别、消息和源码 caret。后端与 IO 错误通过 `base::Result` 返回。

## 自举架构

selfhost 目录按组件拆分：

- `lexer`：M0 lexer core 和 token dump。
- `syntax`：ID-backed AST 数据结构。
- `parser`：cursor、types、expr、seed。
- `compiler/ir`：writer、names、types、expr、emit。
- `bin`：Stage1 CLI 入口。
- `smoke` / `tool`：验证程序。

当前 Stage1 输出 Aurex IR snapshot，不是完整 fixed-point 编译器。seed parser 已覆盖一个 `extern c` block 加多个 `export c fn` item 的模块形态；IR snapshot 按当前函数 block 输出表达式值。

## 验证架构

- `tools/run_tests.sh`：主质量门，覆盖构建、CLI、IR、LLVM、native、std、selfhost 和文档布局。
- `tools/check_golden.sh`：golden 输出对比。
- `tools/bootstrap_chain.sh`：selfhost 路线 smoke。
- `tools/compare_selfhost_lexer.sh`：Stage0/Stage1 lexer 行为对比。
- `tools/bench.py`：轻量性能 smoke。
