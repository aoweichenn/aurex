# API Reference

Version: 0.1.2

## Command-Line Interface

Basic form:

```sh
aurexc [options] input.ax [-o output]
```

Native output modes require `-o output`. Dump and check modes write to stdout
or only return status.

Common options:

- `--help`: print help.
- `--version`: print compiler version.
- `--dump-tokens` / `--emit=tokens`: print tokens.
- `--dump-ast` / `--emit=ast`: print AST.
- `--dump-modules` / `--emit=modules`: print resolved modules.
- `--dump-checked` / `--emit=checked`: print checked module summary.
- `--dump-ir` / `--emit=ir`: print Aurex IR.
- `--dump-llvm-ir` / `--emit=llvm-ir`: print LLVM IR.
- `--check` / `--emit=check`: run through semantic analysis only.
- `--emit=asm`: emit assembly.
- `--emit=obj` / `--emit=object`: emit object file.
- `--emit=exe`: emit executable, the default mode.
- `--opt-level O0|O1|O2|O3` / `-O O0|O1|O2|O3` / `-O0`: control the IR pass pipeline.
- `--clang path`: select clang executable.
- `--clang-arg arg`: pass an argument to clang.
- `--stdlib path`: select std root.
- `--std-backend host-c|none`: select std backend support.
- `--no-stdlib`: disable default std import and support link.
- `-I path`: add import search path.
- `-o path`: set output path.

Exit codes:

- `0`: success.
- `1`: compilation or toolchain failure.
- `2`: argument error.

Environment variables:

- `AUREX_STDLIB`: select the std root, with lower priority than `--stdlib`.

## C++ Driver API

Core type: `aurex::driver::CompilerInvocation`

Important fields:

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

Entry point:

```cpp
aurex::driver::Compiler compiler;
auto result = compiler.run(invocation);
```

`Compiler::run` returns `base::Result<void>`. On failure,
`result.error().message` contains a user-facing error message. Lex/parse/sema
diagnostics are printed by the driver to stderr.

## IR Pass API

Header: `include/aurex/ir/pass_pipeline.hpp`

Core API:

```cpp
base::Result<void> run_pass_pipeline(Module& module, const PassPipelineOptions& options);
```

`PassPipelineOptions` controls:

- `optimization_level`
- `verify_input`
- `verify_output`
- `enable_mem2reg`
- `enable_cfg_cleanup`

Optimization levels:

- `OptimizationLevel::none`: `O0`
- `OptimizationLevel::basic`: `O1`
- `OptimizationLevel::standard`: `O2`
- `OptimizationLevel::aggressive`: `O3`

## Standard-Library API

Header: `include/aurex/driver/standard_library.hpp`

Core APIs:

- `standard_library_backend_name(backend)`
- `find_standard_library(invocation)`
- `standard_library_import_paths(invocation)`
- `standard_library_support_sources(layout, backend)`

Default backend:

```text
host-c -> std/support/host_c.c
```

Stable host support symbols:

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

Standard-library root requirements:

```text
text.ax
c.ax
support/host_c.c
```

## Aurex Source ABI API

C ABI is declared with `extern c`:

```m0
extern c {
    fn puts(s: *const u8) -> i32 @name("puts");
}
```

C ABI export:

```m0
export c fn main(argc: i32, argv: *mut *mut u8) -> i32 {
    return 0;
}
```
