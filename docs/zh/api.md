# API 接口文档

版本：0.1.2

## 命令行接口

基本形式：

```sh
aurexc [primary-option] [secondary-options] input.ax [-o output]
```

通过 `--emit=asm`、`--emit=obj` 或 `--emit=exe` 选择 native 输出时需要
`-o output`；driver 风格的 `-S` 和 `-c` 在省略 `-o` 时会推导 `input.s` 和
`input.o`。dump 和 check 类模式写 stdout 或只返回状态。CLI 语法保持 clang
风格的扁平 flags，但内部和 `--help` 都按一级动作选项和二级修饰选项归类。
`--clang`、`--clang-arg` 这类 native backend 修饰选项只对 native 输出模式有效。

常用参数：

- `--help`：输出帮助。
- `--version`：输出编译器版本。
- `--dump-tokens` / `--emit=tokens`：输出 token。
- `--dump-lossless` / `--emit=lossless`：输出保留空白和注释的结构化 lossless syntax tree；当前
  形态是 `source_file` root 下挂声明节点、直接 trivia/eof token leaves，以及 `block` / 分隔符组节点。
  C++ API 已提供 parent、children、token span、offset lookup、结构校验、子树源码重建和
  lossless CST 到现有 AST parser 的 lowering façade。
- `--dump-ast` / `--emit=ast`：输出 AST。
- `--dump-modules` / `--emit=modules`：输出模块加载结果。
- `--dump-checked` / `--emit=checked`：输出 checked module 摘要。
- `--dump-ir` / `--emit=ir`：输出 Aurex IR。
- `--dump-llvm-ir` / `--emit=llvm-ir`：输出 LLVM IR。
- `--check` / `--emit=check`：只运行到语义分析。
- `--emit=asm`：输出汇编。
- `--emit=obj` / `--emit=object`：输出 object。
- `--emit=exe`：输出可执行文件，默认模式。
- `--opt-level O0|O1|O2|O3` / `-O O0|O1|O2|O3` / `-O0`：控制 IR pass pipeline。
- `--incremental-cache path`：读写 query-key 增量缓存。
- `--query-pruning`：显式确认 query-key pruning 默认路径。
- `--no-query-pruning`：显式退回 coarse source-fingerprint 兼容路径。
- `--clang path`：指定 clang。
- `--clang-arg arg`：透传 clang 参数。
- `-I path`：增加 import 搜索路径。
- `-o path`：指定输出路径。

退出码：

- `0`：成功。
- `1`：编译或工具链失败。
- `2`：参数错误。

## C++ Driver 接口

核心类型：`aurex::driver::CompilerInvocation`

关键字段：

- `input_path`
- `tool_path`
- `output_path`
- `emit_kind`
- `import_paths`
- `clang_path`
- `clang_args`
- `optimization_level`

入口：

```cpp
aurex::driver::Compiler compiler;
auto result = compiler.run(invocation);
```

`Compiler::run` 返回 `base::Result<void>`。失败时 `result.error().message` 保存面向用户的错误消息。lex/parse/sema 诊断由 driver 打印到 stderr。

## C++ Lossless Syntax 接口

头文件：

```cpp
#include <aurex/syntax/lossless.hpp>
#include <aurex/parse/lossless_parse.hpp>
```

核心 API：

```cpp
syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(tokens);
bool valid = tree.is_structurally_valid();
syntax::LosslessNodeId node = tree.node_at_offset(offset);
std::span<const syntax::Token> node_tokens = tree.token_span(node);
std::string text = tree.reconstruct_text(node);
auto ast = parse::lower_lossless_syntax_to_ast(tree, diagnostics);
```

`LosslessSyntaxTree` 保留完整 token/trivia 序列，并用 `LosslessNode` /
`LosslessElement` 表示 immutable-style tree。每个 node 记录 parent、children
区间、连续 token span 和 source range；`LosslessNodeKey` 提供以 kind/range/token
span/depth 组成的稳定节点身份，用于 query、局部重解析和 IDE tooling 的上层索引。

## C++ IDE Tooling 接口

头文件：

```cpp
#include <aurex/tooling/ide.hpp>
```

核心 API：

```cpp
tooling::IdeSnapshotRequest request;
request.path = "/workspace/main.ax";
request.text = source_text;
tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(request);

auto token = tooling::token_info_at_offset(snapshot, offset);
auto hover = tooling::hover_at_offset(snapshot, offset);
auto definition = tooling::definition_at_offset(snapshot, offset);
std::vector<tooling::IdeReference> refs =
    tooling::references_at_offset(snapshot, offset);
tooling::IdeEditImpact impact =
    tooling::edit_impact_for_range(snapshot, edit_begin, removed_length);
```

`IdeSnapshot` 面向内存 buffer：一次构建会保留 source manager、lossless syntax tree、
AST、checked module、结构化 diagnostics，以及 file/lex/parse/diagnostics 的 query
records 和 dependency edges。当前入口已覆盖 diagnostics、token/hover、顶层定义跳转、
同名 identifier references、checked-backed 全局符号与 AST 局部参数 / `let` 绑定 fallback，
以及编辑影响 node 选择；diagnostics 会先归一为结构化 event stream，再进入 query
fingerprint 或 CLI 渲染。它是 LSP adapter 的数据源，不直接绑定 LSP protocol。

## IR Pass 接口

头文件：

- `include/aurex/ir/pass_pipeline.hpp`
- `include/aurex/ir/pass_manager.hpp`
- `include/aurex/ir/analysis_manager.hpp`

核心 API：

```cpp
base::Result<void> run_pass_pipeline(Module& module, const PassPipelineOptions& options);
base::Result<PassPipelineRunSummary> run_pass_pipeline_with_summary(
    Module& module, const PassPipelineOptions& options);
```

`PassPipelineOptions` 控制：

- `optimization_level`
- `verify_input`
- `verify_output`
- `enable_mem2reg`
- `enable_cfg_cleanup`
- `verify_after_each_pass`
- `stage_name`
- `stage_profile_name`

`run_pass_pipeline` 保持兼容旧调用方，只返回成功或错误；`run_pass_pipeline_with_summary`
额外返回 scheduled/executed pass 数量、pipeline 是否改变 IR、以及最终保留的 analysis 集合。

轻量 pass manager API：

- `ModulePassManager`：顺序调度 `ModulePass`，不引入虚接口或复杂模板。
- `ModulePass`：记录 `PassId`、稳定 pass name 和 `ModulePassRun` 函数入口；pass 回调可访问
  `ModuleAnalysisManager`。
- `PassResult`：声明 pass 是否改变 IR，以及它保留的 `PreservedAnalyses`。
- `PreservedAnalyses`：表达 pass 后仍然有效的 analysis，例如 CFG、type table、symbol table 和 record layout。
- `ModuleAnalysisManager`：惰性构建并缓存 CFG、dominance 和 value-use analysis；pass 改变 IR 后按
  `PreservedAnalyses` 自动失效未保留的缓存。
- `VerifierGate`：统一控制 input verifier、output verifier，以及 opt-in 的 after-each-pass verifier；
  failure message 会携带稳定上下文 `stage=... profile=... verifier=...`，after-pass 还会携带
  `pass=...`。原始 verifier body 和 `ErrorCode` 不改变，body 保留在 `: ` 之后。

driver 主路径通过 `PipelineStageId::ir_pass_pipeline` 的 `PipelineStageRecord` 填充
`stage_name` / `stage_profile_name`。直接调用 IR API 时默认值为 `ir_pass_pipeline` /
`ir.pass_pipeline`，和 driver 阶段目录保持一致。

优化级别：

- `OptimizationLevel::none`：`O0`
- `OptimizationLevel::basic`：`O1`
- `OptimizationLevel::standard`：`O2`
- `OptimizationLevel::aggressive`：`O3`

## Aurex 源码 ABI 接口

程序入口是根模块中的普通函数：

```m0
fn main() -> i32 {
    return 0;
}
```

`fn main` 可以返回 `i32` 或 `void`，也可以选择接收
`(argc: i32, argv: *mut *mut u8)` 参数。

作用域清理可以使用 `defer`：

```m0
fn use_buffer(buffer: *mut u8) -> i32 {
    defer free_bytes(buffer);
    return 0;
}
```

`defer` 当前接受函数调用语句。清理调用在当前词法作用域退出时按反序执行，并覆盖正常
离开、`return`、`break` 和 `continue` 路径。返回语句会先求值返回表达式，再执行清理。

C ABI 通过 `extern c` 声明：

```m0
extern c {
    fn puts(s: *const u8) -> i32 @name("puts");
    fn printf(format: *const u8, ...) -> i32 @name("printf");
}
```

`...` 只支持 `extern c` 函数声明，并且必须放在参数列表末尾。变长实参会按 C ABI
规则做默认提升，例如 `bool` / `u8` / `i16` 提升到 `i32`，`f32` 提升到 `f64`。

通过 `export c fn` 导出 C ABI 符号：

```m0
export c fn plugin_entry() -> i32 @name("plugin_entry") {
    return 42;
}
```
