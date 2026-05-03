# API 接口文档

版本：0.1.2

## 命令行接口

基本形式：

```sh
aurexc [options] input.ax [-o output]
```

native 输出模式需要 `-o output`。dump 和 check 类模式写 stdout 或只返回状态。

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
- `--stdlib path`：指定标准库根。
- `--std-backend host-c|none`：选择 std backend support。
- `--no-stdlib`：关闭默认标准库 import 和 support 链接。
- `-I path`：增加 import 搜索路径。
- `-o path`：指定输出路径。

退出码：

- `0`：成功。
- `1`：编译或工具链失败。
- `2`：参数错误。

环境变量：

- `AUREX_STDLIB`：指定标准库根，优先级低于 `--stdlib`。

## C++ Driver 接口

核心类型：`aurex::driver::CompilerInvocation`

关键字段：

- `input_path`
- `tool_path`
- `output_path`
- `standard_library_path`
- `emit_kind`
- `import_paths`
- `clang_path`
- `clang_args`
- `optimization_level`
- `standard_library_backend`
- `use_standard_library`

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

## 标准库接口

头文件：`include/aurex/driver/standard_library.hpp`

核心 API：

- `standard_library_backend_name(backend)`
- `find_standard_library(invocation)`
- `standard_library_import_paths(invocation)`
- `standard_library_support_sources(layout, backend)`

默认 backend：

```text
host-c -> std/support/host_c.c
```

稳定 host support 符号：

```text
aurex_std_v0_read_file
aurex_std_v0_free_file
aurex_std_v0_write_text
aurex_std_v0_output_open
aurex_std_v0_output_write_text
aurex_std_v0_output_write_source_range
aurex_std_v0_output_write_i32
aurex_std_v0_output_close
```

标准库根判定条件：

```text
text.ax
c.ax
support/host_c.c
```

## Aurex 源码 ABI 接口

C ABI 通过 `extern c` 声明：

```m0
extern c {
    fn puts(s: *const u8) -> i32 @name("puts");
}
```

导出 C ABI 函数：

```m0
export c fn main(argc: i32, argv: *mut *mut u8) -> i32 {
    return 0;
}
```
