# API Reference

Version: 0.1.2

## Command-Line Interface

Basic form:

```sh
aurexc [primary-option] [secondary-options] input.ax [-o output]
```

Native output selected with `--emit=asm`, `--emit=obj`, or `--emit=exe`
requires `-o output`; the driver-style `-S` and `-c` forms infer `input.s` and
`input.o` when `-o` is omitted. Dump and check modes write to stdout or only
return status. The CLI keeps clang-style flat flags while classifying options
internally and in `--help` as primary actions or secondary modifiers. Native
backend modifiers such as `--clang` and `--clang-arg` are valid only for native
output modes.

Common options:

- `--help`: print help.
- `--version`: print compiler version.
- `--dump-tokens` / `--emit=tokens`: print tokens.
- `--dump-lossless` / `--emit=lossless`: print a structured lossless syntax tree that preserves whitespace and
  comments; the current shape is a `source_file` root with declaration nodes, direct trivia/eof token leaves,
  and `block` / delimiter-group nodes for future CST / GreenTree lowering.
- `--dump-ast` / `--emit=ast`: print AST.
- `--dump-modules` / `--emit=modules`: print resolved modules.
- `--dump-checked` / `--emit=checked`: print checked module summary.
- `--dump-ir` / `--emit=ir`: print Aurex IR.
- `--dump-llvm-ir` / `--emit=llvm-ir`: print LLVM IR.
- `--check` / `--emit=check`: run through semantic analysis only.
- `-fsyntax-only`: same as `--check`.
- `-S`: emit assembly, defaulting to `input.s` when `-o` is omitted.
- `-c`: emit object code, defaulting to `input.o` when `-o` is omitted.
- `--emit=asm`: emit assembly.
- `--emit=obj` / `--emit=object`: emit object file.
- `--emit=exe`: emit executable, the default mode.
- `--emit kind`: same as `--emit=kind`.
- `--opt-level O0|O1|O2|O3` / `--opt-level=O0` / `-O O0|O1|O2|O3` / `-O0`: control the IR pass pipeline.
- `--incremental-cache path`: read and write the query-key incremental cache.
- `--query-pruning`: explicitly select the default query-key pruning path.
- `--no-query-pruning`: explicitly use the coarse source-fingerprint compatibility path.
- `--clang path` / `--clang=path`: select clang executable.
- `--clang-arg arg` / `--clang-arg=arg`: pass an argument to clang.
- `-I path` / `-Ipath` / `--import-path path`: add import search path.
- `-o path`: set output path.

Exit codes:

- `0`: success.
- `1`: compilation or toolchain failure.
- `2`: argument error.

## C++ Driver API

Core type: `aurex::driver::CompilerInvocation`

Important fields:

- `input_path`
- `tool_path`
- `output_path`
- `emit_kind`
- `import_paths`
- `clang_path`
- `clang_args`
- `optimization_level`

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

## Aurex Source ABI API

Program entry is an ordinary root-module function:

```m0
fn main() -> i32 {
    return 0;
}
```

`fn main` may return `i32` or `void`, and may optionally accept
`(argc: i32, argv: *mut *mut u8)`.

Scope cleanup can use `defer`:

```m0
fn use_buffer(buffer: *mut u8) -> i32 {
    defer free_bytes(buffer);
    return 0;
}
```

`defer` currently accepts function-call statements. Cleanup calls run in reverse
order when the current lexical scope exits, including normal exits, `return`,
`break`, and `continue` paths. Return statements evaluate the return expression
before running cleanup.

C ABI is declared with `extern c`:

```m0
extern c {
    fn puts(s: *const u8) -> i32 @name("puts");
    fn printf(format: *const u8, ...) -> i32 @name("printf");
}
```

`...` is supported only on `extern c` declarations and must appear at the end of
the parameter list. Variadic arguments use C ABI default promotions, for example
`bool` / `u8` / `i16` to `i32` and `f32` to `f64`.

C ABI symbols are exported with `export c fn`:

```m0
export c fn plugin_entry() -> i32 @name("plugin_entry") {
    return 42;
}
```
