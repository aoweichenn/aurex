# 设计实现文档

版本：0.1.2

## Lexer

lexer 以 source text 和 source id 为输入，输出 token 数组。token 包含 kind、文本片段和 source range。

实现要点：

- 不在 token 内复制源码文本，文本片段引用 `SourceManager` 保存的内容。
- 诊断使用 source range，便于 driver 打印行列和 caret。
- golden 测试以 token dump 作为稳定接口。

## Parser

Stage0 parser 是手写递归下降 parser，消费 token span，输出 AST。AST 使用 ID 引用节点池，避免节点复制和指针生命周期问题。

AST 设计：

- `TypeId`、`ExprId`、`StmtId`、`ItemId`、`ModuleId` 指向各自 vector。
- item 记录所属 module，合并多个模块后仍能区分定义来源。
- AST dump 用于结构回归，不承担语义展示职责。

## Module Loader

module loader 负责把根文件和 import 文件合并为一个 `AstModule`。它会记录已加载文件和模块，避免重复加载和递归导入失控。模块声明必须与 import 路径一致，例如 `import std.core.text;` 对应 `module std.core.text;`。

## Sema

语义分析输出 `CheckedModule`：

- `TypeTable`
- expression type table
- syntax type handle table
- item ABI name table
- function signature table
- struct / enum metadata

语义规则强调显式成本：无重载、无 shadowing、无隐式类型转换、数组不能按值传参或返回、opaque struct 只能 behind pointer。

ABI 名称在 sema 阶段确定。普通 Aurex 符号使用模块路径参与 mangling，`extern c` 和 `export c` 通过显式 ABI 名称与 C/host world 对接。

## IR

Aurex IR 是后端无关 typed CFG/SSA-like 中间层。核心实体：

- `Module`
- `Function`
- `BasicBlock`
- `Terminator`
- `Value`
- `RecordLayout`
- `GlobalConstant`

函数记录源码名、ABI symbol、linkage、calling convention、返回类型和参数签名。

IR verifier 检查 block、value、terminator、类型和引用一致性。driver 在 lowering 后和 pass pipeline 后默认都运行 verifier。

## Pass Pipeline

`run_pass_pipeline` 默认在输入和输出处运行 verifier。`O0` 不做优化。`O1+` 当前启用：

- 局部 mem2reg：只提升同一 block 内未逃逸的 scalar slot。
- CFG cleanup：删除不可达 block，合并安全的空跳转 block，折叠同目标条件跳转。

这不是完整 SSA 构造；跨块 mem2reg 和 phi 插入后续实现。

`O2` 和 `O3` 目前复用同一组保守 pass，只作为 CLI/API 兼容和后续扩展点。

## LLVM Backend

LLVM backend 先验证 Aurex IR，再声明 record、constant 和 function，随后 lowering basic block 和 value。最终通过 LLVM verifier 后打印 LLVM IR 文本。

native 输出并不直接从 AST 生成代码，而是统一经过 Aurex IR 和 LLVM IR，保证 dump 路径与执行路径共享同一个 IR 契约。

## Driver

driver 负责：

- 文件 IO。
- 模块加载。
- 调用 sema / IR / backend。
- 写临时 LLVM IR。
- 调用 clang。
- 查找和链接 std backend support。

driver 对 dump/check 模式尽早返回，避免运行不必要的后端阶段。native 输出会创建临时 `.ll` 文件，调用 clang 后清理临时文件。

## Standard Library

`.ax` 标准库模块按职责目录表达语言层 API：`std/core`、`std/fs`、`std/io`、`std/sys`。临时 C FFI 声明和 host-c support 统一放在 `std/ffi/c/`，避免高层 std 模块直接绑定 host C。稳定主机符号统一使用 `aurex_std_v0_*`。

driver 在 `host-c` backend 下选择 `std/ffi/c/support/host_c.c`，后续可以替换 C 桥接层，而不改变 `.ax` 标准库 API。

## Install Layout

CMake 安装规则把 `aurexc` 安装到 `bin`，把 `std` 目录安装到 `share/aurex/std`。运行时查找通过 `tool_path` 推导可执行文件目录，因此安装目录移动后仍能定位同一前缀内的标准库。

## Test Implementation

主测试脚本会构建工程、检查文档布局、验证 CLI help、运行正向/反向样例、检查当前语言切片、检查 IR/LLVM 输出、测试 std backend，并验证安装后标准库查找。
