# Aurex M0 设计文档

版本：M0V0.1.8

## 1. 设计目标

Aurex M0 是 Aurex 的最小自举核心。它围绕五个约束设计：

- 成本透明：没有隐藏拷贝、隐藏析构、隐藏分配、隐式转换；
- 语义确定：求值顺序和 ABI 行为必须显式化；
- 自举面小：只保留能写下一阶段编译器的核心能力；
- 阶段可替换：lexer、parser、sema、codegen 必须能独立测试；
- 实现可读：初学者可以跟着一个源文件走完整条编译链。

## 2. 编译流水线

```text
source.ax
  -> SourceManager
  -> handwritten Lexer
  -> Token[]
  -> handwritten recursive descent Parser
  -> AstModule
  -> SemanticAnalyzer
  -> CheckedModule
  -> CEmitter
  -> output.c
```

Parser 不依赖 Lexer 类，只消费 `std::span<const Token>`。Sema 不依赖 parser
内部状态。Codegen 不依赖 parser 内部状态。

## 3. 模块边界

`m0_base`

- 管理 source、range、diagnostic、整数别名、`Result<T>`；
- 不依赖任何语言概念。

`m0_syntax`

- 管理 token 和 AST；
- AST 节点是普通 struct，不用 virtual；
- 用 ID 索引 vector storage。

`m0_lex`

- 纯手写 byte-oriented lexer；
- 生成带 byte range 的 token，token text 是指向 SourceManager 文本的 `string_view`。

`m0_parse`

- 纯手写 recursive descent parser；
- 只做语法，不做类型检查；
- 构建 AST，并做简单 panic-mode 错误恢复。

`m0_sema`

- 做名称解析和类型解析；
- 管理 `TypeTable`、`SymbolTable`、`CheckedModule`；
- 记录表达式类型等 side table。

`m0_codegen_c`

- 从 AST + `CheckedModule` 生成 C；
- 不应该猜类型，不应该解析名字。

`m0_driver`

- 编排文件 IO 和编译阶段；
- 负责诊断展示。
- 解析 import：`import a.b;` 会加载 `a/b.ax`；
- 合并 imported AST 到同一个 Stage0 checked module，并重映射 AST ID。
- 校验 imported 文件声明的 module path 必须和 import path 一致；
- 对缺失 import 和 module name 不匹配给出带源码范围的诊断。
- 加载阶段检测循环 import。
- 提供 `--dump-modules` 用来查看解析后的模块集合。

`m0c`

- 只解析命令行参数。

## 4. AST 设计

M0 使用紧凑 ID + vector storage：

```text
TypeId -> AstModule::types
ExprId -> AstModule::exprs
StmtId -> AstModule::stmts
ItemId -> AstModule::items
```

这样可以避免 virtual dispatch，也方便后续阶段通过 side table 附加信息，而不污染 parse-only AST。

当前 AST node 是较宽的 struct。这让实现简单、易读，但如果将来内存占用成为真实问题，可以按 node kind 拆分 payload。

## 5. 错误处理

各阶段返回 `Result<T>`，诊断写入 `DiagnosticSink`。异常不是主错误路径。

诊断位置使用 byte offset。M0V0.1.8 输出：

- 文件路径；
- 行列号；
- 严重级别；
- 消息；
- 源码行；
- caret 标记。

## 6. 语义模型

M0 拒绝隐式成本：

- 无隐式数值转换；
- 无隐式指针转换；
- 禁止函数重载；
- 禁止 shadowing；
- `let` 不可赋值；
- 数组是 storage-only；
- opaque struct 只能通过指针使用。

当前 sema 会把表达式类型记录在 `CheckedModule::expr_types` 中。这样 C emitter 不需要自己猜类型，也避免把类型逻辑散落到后端。

## 7. C 后端设计

C 后端把 M0 类型映射到稳定 C 拼写：

- `i32` -> `int32_t`
- `u8` -> `uint8_t`
- `usize` -> `size_t`
- `isize` -> `ptrdiff_t`
- `*mut T` -> `T*`
- `*const T` -> `const T*`
- `[N]T` -> `T[N]`

已知后续工作：

- 在 C emission 前增加 lowering 层；
- 为严格左到右求值顺序生成临时变量；
- 规范 ABI 名称和导出符号；
- 把表达式生成拆成 value emission 和 statement lowering。

## 8. 自举设计

自举路线在 `selfhost/` 中显式维护。

目标 fixed-point 链路是：

```text
Stage0 C++ compiler
  -> 编译 M0 编译器源码
  -> 生成 m0c-stage1
  -> m0c-stage1 再编译同一批源码
  -> stage1/stage2 输出一致
```

当前 M0V0.1.8 状态：

- `bootstrap/` 有 standalone Stage0-mini compiler；
- `selfhost/src/m0c_seed.ax` 是第一个 M0 seed；
- `selfhost/src/aurex/selfhost/lexer/core.ax` 是共享 M0 lexer core，包含 `TokenSpan` token 形状和 `scan_token`；
- `selfhost/src/aurex/selfhost/lexer/dump.ax` 是共享 token dump helper；
- `selfhost/src/lexer_smoke.ax` import 共享 core scanner，并校验一段小 token 序列；
- `selfhost/src/lexer_ranges.ax` import 共享 core scanner，并校验 token kind 加 `begin/end` byte range；
- `selfhost/src/lexer_dump.ax` import 共享 dump helper，用 M0 输出 token kind 流，并和 golden 文件对比；
- `selfhost/src/lexer_file.ax` import 共享 dump helper，通过显式 runtime IO 读取源码文件，并输出可 golden 对比的 token kind 流；
- `selfhost/src/aurex/selfhost/parser/seed.ax` 是第一个 M0 parser seed，基于 `TokenSpan` 递归下降光标校验 `module`、`import`、`extern c`、函数签名和一个 `export c fn` 函数体外壳；
- `selfhost/src/parser_smoke.ax` 是可执行 parser seed smoke test；
- `tools/compare_selfhost_lexer.sh` 会把 M0 lexer 输出和生产 C++ Stage0 lexer 输出在 `examples/hello.ax` 以及所有本地 positive/negative 测试输入上直接对比；
- selfhost 测试会检查 `lexer_file.ax` 和 `parser_smoke.ax` 确实加载了共享 lexer/parser 模块，因此 import 使用也进入了回归测试；
- 生产编译器仍然是 C++ 实现。

下一步真正自举里程碑应该是：让 parser seed 输出一个小型 AST summary 或稳定 parse dump，然后在小型共享语料上和 C++ parser 对比。

## 9. 工业级强化路线

近期：

- 从 shell-only 测试升级到真正的单元测试可执行文件；
- 增加 diagnostics 和 C output 的 golden file；
- 增加 lowering 层处理求值顺序；
- 实现 import path 和 module graph；
- 支持更完整的多行诊断。

中期：

- 用 M0 实现 lexer；
- 用 M0 实现 parser；
- C ABI 校验测试套件；
- lexer/parser fuzzing；
- 固定语料 benchmark 和趋势追踪。

长期：

- Stage1/Stage2 self-host fixed point；
- 确定性构建输出；
- release artifacts；
- 兼容性策略。
