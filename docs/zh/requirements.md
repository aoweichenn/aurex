# 需求分析文档

版本：0.1.2

## 目标用户

- 编译器开发者：需要清晰的前端、IR、后端和 driver 边界。
- 语言设计者：需要可验证的语义规则和 ABI 行为。
- 自举推进者：需要先稳定 Stage0、IR 和语言特性，再在后续阶段重写自举链路。
- 工具链集成者：需要稳定 CLI、安装布局和标准库查找逻辑。

## 范围定义

0.1.2 的范围是 Stage0 可用、标准库和 IR 主链路可维护，并继续推进 M1 语言特性：

- Stage0 必须能完成 `.ax` 到 native 输出的主路径。
- Stage0 必须能暴露 tokens、AST、checked summary、Aurex IR 和 LLVM IR。
- 标准库必须能在 build tree 和 install tree 下被定位。
- 旧自举实验不再是当前验收目标；新的自举将在 M3 以新语言特性重新设计。

## 功能需求

1. 源码处理：读取 `.ax` 文件，保留路径和 source range，用于诊断。
2. 词法分析：输出稳定 token 流，支持 golden 对比。
3. 语法分析：构建 parse-only AST，不混入语义结果。
4. 模块加载：支持 `module` / `import`，按导入者目录、`-I`、标准库根查找。
5. 语义分析：完成类型解析、符号解析、函数签名检查、基本控制流约束和 ABI 名称记录。
6. IR lowering：把 AST + checked metadata 降到 Aurex IR。
7. IR 验证与优化：在 lowering 后运行 verifier，并按优化级别运行 pass pipeline。
8. LLVM 后端：把 Aurex IR lowering 到 LLVM IR。
9. 本机输出：通过 clang 生成 assembly、object 或 executable。
10. 标准库：默认加载 `std`，并在 executable 输出中链接所选 backend support。
11. M1 特性：Stage0 能检查并降低模块可见性、泛型、sum type、pattern matching、表达式和推导切片。

## 运行需求

- 构建环境需要 CMake、C++20 编译器、LLVM 开发库和 clang。
- native 输出需要 `clang` 可执行文件，或通过 `--clang` 指定替代路径。
- 需要标准库时，`std` 根必须包含 `core/text.ax`、`ffi/c/libc.ax` 和 `ffi/c/support/host_c.c`。
- 安装布局应包含 `bin/aurexc` 和 `share/aurex/std`。

## 非功能需求

- 可测试：每个阶段有可观察输出或行为测试。
- 可重定位：安装后的工具不依赖源码树绝对路径。
- ABI 稳定：标准库 host support 使用版本化符号。
- 可维护：文档按主题组织，中英文结构一致。
- 保守优化：未完整证明正确性的优化不得默认破坏语义。
- 诊断可定位：错误应带文件、行列和源码片段。
- 行为可复现：golden 输出和负向样例应稳定。

## 约束

- 当前生产后端以 LLVM 为主。
- M0 语言保持小核心：无类型推导、无泛型、无重载、无隐式类型转换。
- 标准库 backend support 当前默认实现是 host-c。

## 风险与边界

- IR pass pipeline 仍是保守实现，`O2`/`O3` 当前不会启用比 `O1` 更激进的额外 pass。
- C FFI 是临时桥接层，并隔离在 `std/ffi/c/`；新的语言层 std 代码应依赖这个边界，而不是直接声明 host C。
- 新自举在 M3 前不是 active deliverable；当前所有生产能力都必须由 C++ Stage0 和 LLVM 后端承载。
- 标准库 ABI v0 的符号必须保持稳定；破坏性变更应引入新 namespace。

## 验收标准

- `tools/run_tests.sh` 通过。
- 正向样例能编译并运行。
- 反向样例能稳定报错。
- 安装后的 `bin/aurexc` 能找到安装前缀下的 `share/aurex/std`。
- M1 正向和负向样例覆盖当前已实现语言特性。
- 文档入口只保留 `docs/zh/`、`docs/en/` 和 `docs/README.md`，不再恢复逐小版本文档。
