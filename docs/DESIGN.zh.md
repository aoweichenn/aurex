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
  -> Aurex IR
  -> LLVM IR
  -> clang -> output.s / output.o / executable
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

`m0_ir`

- 把 AST + `CheckedModule` 降为 Aurex 自有 IR；
- 使用 typed value、basic block、terminator 和 `phi` 表达控制流和值；
- 保留 `extern_c` / `export_c` linkage 和 ABI symbol；
- 作为 LLVM IR lowering 和未来自研后端的主要输入；
- 显式表示编译期常量引用，避免把 `const` 和 enum case 误降成 load。

`m0_driver`

- 编排文件 IO 和编译阶段；
- 负责诊断展示。
- 解析 import：`import a.b;` 会加载 `a/b.ax`；
- 合并 imported AST 到同一个 Stage0 checked module，并重映射 AST ID，同时保留每个顶层 item 的所属模块。
- sema 会通过当前模块和直接 import 的模块解析顶层名字，IR lowering 会输出模块限定的 ABI 符号。
- 校验 imported 文件声明的 module path 必须和 import path 一致；
- 对缺失 import 和 module name 不匹配给出带源码范围的诊断。
- 加载阶段检测循环 import。
- 提供 `--dump-modules` 用来查看解析后的模块集合。
- 在 `--emit=llvm-ir` 模式下输出 LLVM lowering 结果；默认输出、
  `--emit=asm`、`--emit=obj` 和 `--emit=exe` 模式下调用 clang。

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

## 7. Aurex IR 设计

M0 新增的 IR 是 typed CFG/SSA-like 中间代码，不是 LLVM IR 直出。它的目标是隔离前端和后端：AST 只表达语法，`CheckedModule` 提供类型/ABI 边表，IR 则表达可优化、可验证、可 lowering 的程序。

当前 IR 选择：

- 函数有源码名、ABI symbol、linkage、返回类型和参数签名。
- 函数显式记录调用约定，`extern c` 和 `export c` 进入 C ABI 路线。
- 值全局编号，block 内保存 value 序列。
- 局部 storage 先用 `alloca/load/store` 表示，后续统一做 mem2reg。
- 字段/下标使用地址指令 `field_addr` / `index_addr`，便于 lowered 到 LLVM GEP。
- 调用同时保存最终 symbol 和可解析的内部 `FunctionId`。
- `&&` / `||` lowering 为 block + `phi`，短路语义不隐藏在普通二元 op 里。

后续 IR 强化顺序：

1. IR verifier：检查 terminator、value type、block predecessor 和 call signature。
2. mem2reg：把局部 slot 提升为 SSA value。
3. CFG cleanup：删除不可达 block，合并空跳转 block。
4. LLVM lowering：把 Aurex type/value/block/function 映射到 LLVM IR。
5. C ABI 测试：验证 `extern c`、`export c`、runtime 调用和 host linker 行为。

## 8. LLVM 后端与 FFI

Stage0 生产后端现在只有 LLVM 路线：Aurex IR 先 lowering 为 LLVM IR 文本，
driver 再用 clang 生成汇编、object 或可执行文件。`extern c` / `export c`
通过 IR 中的 ABI symbol、linkage 和调用约定进入 LLVM 声明或定义。

当前已经覆盖：

- M0 正向样例经默认 native 输出运行；
- `--emit=llvm-ir` 可观察 LLVM IR；
- `--emit=asm`、`--emit=obj`、`--emit=exe` 均走 LLVM；
- 多模块中重复声明的同名 `extern c` 会在 LLVM 侧合并到同一个外部声明；
- runtime C 源可通过 `--runtime-c` 链接到 native 输出。

仍需补强：

- 优化 pipeline 和 pass 管理；
- object/assembly 模式下更细的 runtime 链接约束；
- 更完整的 ABI 属性、目标 triple/CPU/feature 配置；
- 未来自研后端复用同一 IR/verifier/ABI 描述。

## 9. 自举设计

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
- `selfhost/src/aurex/selfhost/bin/m0c_seed.ax` 是第一个 M0 seed；
- `selfhost/src/aurex/selfhost/lexer/core.ax` 是共享 M0 lexer core，包含 `TokenSpan` token 形状和 `scan_token`；
- `selfhost/src/aurex/selfhost/lexer/dump.ax` 是共享 token dump helper；
- `selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax` import 共享 core scanner，并校验一段小 token 序列；
- `selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax` import 共享 core scanner，并校验 token kind 加 `begin/end` byte range；
- `selfhost/src/aurex/selfhost/tool/lexer_dump.ax` import 共享 dump helper，用 M0 输出 token kind 流，并和 golden 文件对比；
- `selfhost/src/aurex/selfhost/tool/lexer_file.ax` import 共享 dump helper，通过显式 runtime IO 读取源码文件，并输出可 golden 对比的 token kind 流；
- `selfhost/src/aurex/selfhost/parser/` 是第一个 M0 parser seed，已按职责拆成 `cursor.ax`、`types.ax`、`expr.ax` 和 `seed.ax`；类型解析使用显式指针前缀栈，表达式解析使用显式 operator/frame 栈，并生成 ID-backed `AstModule`；
- `selfhost/src/aurex/selfhost/smoke/parser_smoke.ax` 是可执行 parser seed smoke test；
- `tools/compare_selfhost_lexer.sh` 会把 M0 lexer 输出和生产 C++ Stage0 lexer 输出在 `examples/hello.ax` 以及所有本地 positive/negative 测试输入上直接对比；
- selfhost 测试会检查 `lexer_file.ax` 和 `parser_smoke.ax` 确实加载了共享 lexer/parser 模块，因此 import 使用也进入了回归测试；
- 生产编译器仍然是 C++ 实现。

下一步真正自举里程碑应该是：继续扩大这个迭代式 parser seed 的 AST 覆盖面，输出稳定 AST summary 或 parse dump，然后在小型共享语料上和 C++ parser 对比。

## 10. 工业级强化路线

近期：

- 从 shell-only 测试升级到真正的单元测试可执行文件；
- 增加 diagnostics 和 C output 的 golden file；
- 增加 IR verifier 和 LLVM lowering 骨架；
- 实现 import path 和 module graph；
- 支持更完整的多行诊断。

中期：

- 用 M0 实现 lexer；
- 用 M0 实现 parser；
- C ABI 校验测试套件；
- Aurex IR mem2reg 和 CFG cleanup；
- lexer/parser fuzzing；
- 固定语料 benchmark 和趋势追踪。

长期：

- Stage1/Stage2 self-host fixed point；
- 确定性构建输出；
- release artifacts；
- 兼容性策略。
