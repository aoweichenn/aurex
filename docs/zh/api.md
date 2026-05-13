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

## IR Pass 接口

头文件：`include/aurex/ir/pass_pipeline.hpp`

核心 API：

```cpp
base::Result<void> run_pass_pipeline(Module& module, const PassPipelineOptions& options);
```

`PassPipelineOptions` 控制：

- `optimization_level`
- `verify_input`
- `verify_output`
- `enable_mem2reg`
- `enable_cfg_cleanup`

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
