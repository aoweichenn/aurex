# 运行流程文档

版本：0.1.2

## CLI 流程

1. `src/cli/main.cpp` 解析参数。
2. 构造 `CompilerInvocation`。
3. 调用 `driver::Compiler::run`。
4. 根据 `EmitKind` 选择 dump、check、IR、LLVM IR 或 native 输出。

退出码：

- `0`：成功。
- `1`：编译、IO、后端或 native toolchain 失败。
- `2`：命令行参数错误。

## 模块加载流程

1. 读取根输入文件。
2. lexer/tokenize。
3. parser 构建根 AST。
4. 遍历 import。
5. 按导入者目录、`-I`、标准库路径查找模块文件。
6. 检查模块名与 import 路径匹配。
7. 合并 AST，同时保留模块 ID 和模块路径。

模块查找以导入者目录为第一优先级，然后追加 `-I` 和标准库 import root。标准库启用时，driver 会把 `std` 根的父目录加入 import path，使 `import std.core.text;` 解析到 `std/core/text.ax`。

## 语义流程

1. 注册类型名。
2. 分析结构体属性。
3. 注册函数、const、enum case 等值名。
4. 分析 const initializer。
5. 分析函数体。
6. 输出 `CheckedModule`，包含类型表、表达式类型、ABI symbol、函数签名和记录布局信息。

语义阶段不修改 AST。后续阶段通过 AST ID 查询 `CheckedModule` 中的类型和 ABI 信息。

## IR 流程

1. 根据 `CheckedModule` 复制类型表。
2. 生成 record layout。
3. 声明全局常量和函数。
4. 降低全局常量 initializer。
5. 降低函数体为 basic block、terminator 和 typed value。
6. 运行 `verify_module`。
7. `--opt-level O1` 及以上运行局部 mem2reg 和 CFG cleanup。
8. 再次运行 verifier。

优化级别行为：

- `O0`：只运行输入/输出 verifier。
- `O1`：启用当前局部 mem2reg 和 CFG cleanup。
- `O2` / `O3`：当前与 `O1` 使用同一组保守 pass，作为后续扩展接口保留。

## Native 输出流程

1. LLVM backend 生成 LLVM IR 文本。
2. driver 写入临时 `.ll` 文件。
3. 如输出 executable 且未关闭 std，查找标准库并选择 backend support。
4. 调用 clang：
   - `--emit=asm` 添加 `-S`
   - `--emit=obj` 添加 `-c`
   - `--emit=exe` 直接链接
5. 删除临时 LLVM IR 文件。

native 输出必须提供 `-o`。dump、check、IR 和 LLVM IR 模式直接写 stdout，不需要 `-o`。

## 标准库与 backend support 链接流程

1. `--no-stdlib` 关闭标准库 import path 和 support 链接。
2. 未关闭标准库时，模块加载阶段尝试加入标准库 import root，例如 `std.core.text` 会解析到 `std/core/text.ax`。
3. 只有 executable 输出会链接 std backend support。
4. `--std-backend host-c` 链接 `std/ffi/c/support/host_c.c`。
5. `--std-backend none` 不链接 support 源文件。

## 安装后标准库查找流程

查找顺序：

1. `--stdlib path`
2. `AUREX_STDLIB`
3. 构建时内置 std 路径
4. `aurexc` 可执行文件相对路径：
   - `bin/std`
   - `bin/../std`
   - `bin/../../std`
   - `bin/../share/aurex/std`
   - `bin/../lib/aurex/std`
   - `bin/../../share/aurex/std`
5. 当前工作目录的 `std`

候选目录必须同时包含 `core/text.ax`、`ffi/c/libc.ax` 和 `ffi/c/support/host_c.c`，才会被接受为标准库根。

## 失败流程

- lexer/parser/sema 失败：打印诊断和源码 caret，然后返回错误。
- IR verifier 失败：返回 verifier 错误，阻止后端继续执行。
- LLVM verifier 或 clang 失败：返回后端/toolchain 错误。
- 找不到标准库：executable 输出报错，提示设置 `AUREX_STDLIB` 或使用 `--no-stdlib`。
