# Aurex M0 使用手册

版本：M0V0.1.8

## 1. M0 是什么

Aurex M0 是 Aurex 语言的最小自举核心。它刻意保持小而透明：没有类型推导、没有泛型、没有重载、没有隐藏拷贝、没有隐式类型转换。当前第一后端生成 C。

当前生产编译器用 C++20 编写，分层如下：

```text
source -> lexer -> tokens -> parser -> AST -> sema -> checked module -> C emitter
```

Lexer 和 Parser 都是纯手写实现，没有使用 ANTLR、Flex、Bison 或任何 parser generator。

## 2. 构建

在 `aurex/` 目录下执行：

```sh
cmake -S . -B build
cmake --build build -j
```

编译器可执行文件是：

```sh
build/m0c
```

## 3. 编译一个程序

```sh
build/m0c examples/hello.ax -o build/hello.c
cc build/hello.c -o build/hello
build/hello
```

预期输出：

```text
hello from Aurex M0
```

## 4. CLI 命令

```sh
build/m0c --help
build/m0c --version
build/m0c --dump-tokens examples/hello.ax
build/m0c --dump-ast examples/hello.ax
build/m0c --dump-modules tests/positive/module_math.ax
build/m0c --check examples/hello.ax
build/m0c --emit=ast examples/hello.ax
build/m0c --emit=check examples/hello.ax
build/m0c --emit=c examples/hello.ax -o build/hello.c
build/m0c -I tests/imports tests/positive/import_path.ax -o build/import_path.c
```

参数说明：

- `--version`：输出编译器版本。
- `--dump-tokens`：只运行 lexer，并输出 token。
- `--dump-ast`：运行 lexer 和 parser，并输出稳定 AST dump。
- `--dump-modules`：解析 import，并输出已加载模块名和路径。
- `--check`：运行 lexer、parser、语义分析，但不生成 C。
- `--emit=tokens`：等价于 `--dump-tokens`。
- `--emit=ast`：等价于 `--dump-ast`。
- `--emit=modules`：等价于 `--dump-modules`。
- `--emit=check`：等价于 `--check`。
- `--emit=c`：生成 C，这是默认行为。
- `-o path`：把生成的 C 写入指定路径。
- `-I path`：增加 import 搜索根。`import a.b;` 会查找 `a/b.ax`，先查导入者所在目录，再查每个 `-I` 路径。

## 5. 当前语言能力

支持的顶层结构：

- `module`
- `import`：C++ Stage0 已经支持递归加载。
- `extern c`
- `export c fn`
- `opaque struct`
- `struct`
- `enum`
- `const`
- `fn`

支持的语句：

- `let`
- `var`
- `if` / `else`
- `while`
- `break`
- `continue`
- `return`
- 表达式语句
- 赋值语句

支持的类型：

- `void`、`bool`
- `i8`、`u8`、`i16`、`u16`、`i32`、`u32`、`i64`、`u64`
- `isize`、`usize`
- `f32`、`f64`
- `str`
- `*mut T`
- `*const T`
- `[N]T`
- 具名 struct / enum / opaque struct 类型

支持的表达式：

- 整数字面量
- 布尔字面量
- `null`
- 字符串字面量
- C 字符串字面量：`c"..."`
- byte 字面量
- 名字引用
- 一元和二元表达式
- 函数调用
- 字段访问
- 下标访问
- struct literal
- `cast(T, x)`、`ptr_cast(T, x)`、`bit_cast(T, x)`

## 6. 当前已检查的语义规则

M0V0.1.8 已经检查：

- 禁止函数重载
- 禁止 shadowing
- `let` 不可被赋值
- return 类型必须匹配
- `if` / `while` 条件必须是 `bool`
- 数组类型不能作为函数参数或返回值
- 数组和包含数组的 struct 不能按值赋值
- opaque struct 只能作为指针目标使用
- 函数参数类型必须精确匹配，整数 literal 到整数目标类型是当前允许的特例
- `break` / `continue` 只能出现在 `while` 内部

仍需后续补强的工业级规则：

- 所有复杂表达式在 C 后端中的严格左到右求值顺序
- 对带副作用的函数实参生成临时变量
- 完整常量求值
- 模块 visibility/export 规则和命名空间限定
- 完整 C ABI 校验

## 6.1 模块加载

C++ Stage0 编译器现在会在语义分析前解析 import：

```m0
module app;
import shared.util;
```

解析规则是把 `shared.util` 映射为 `shared/util.ax`。先搜索当前导入文件所在目录，再搜索所有 `-I` 路径。第一版实现会把 imported AST 合并进同一个 checked module。

当前限制：

- 暂无模块间 visibility/export 模型；
- 已加载模块之间的重复全局名字仍然会被 sema 拒绝；
- imported module 的声明必须和 import 路径一致；
- 缺失 import 和 module name 不匹配会定位到 import/module 的源码范围；
- 如果多个 `-I` 根目录里存在同一个模块路径，使用第一个匹配结果；
- `--dump-tokens` 仍然只输出根源码文件的 token。

## 7. 测试

```sh
tools/run_tests.sh
```

这个脚本会执行：

- CMake configure/build
- 版本和 help 检查
- token dump 和 AST dump 检查
- 语义正例和负例测试
- hello 端到端 codegen
- standalone bootstrap 编译器检查
- selfhost seed 检查
- selfhost lexer smoke 检查
- selfhost lexer dump golden 检查
- selfhost 文件输入 lexer golden 检查

## 8. 性能 smoke test

```sh
tools/bench.py
```

脚本会生成一个合成 M0 源文件，并测量 token dump、AST dump、C emission 的墙钟时间。这是轻量性能烟测，不是严格统计 benchmark。

## 9. 自举与 selfhost

当前有两个相关目录：

- `bootstrap/`：单文件 C++20 Stage0-mini 编译器，用 Makefile 独立构建。
- `selfhost/`：未来用 M0 编写编译器自身的源码种子和 runtime 占位。

执行：

```sh
tools/bootstrap_chain.sh
```

M0V0.1.8 还没有完全自举。当前目标是让自举路线可见、可测试、可逐步替换：把编译器组件逐步迁移到 `selfhost/src`，用 Stage0 `m0c` 编译出 Stage1，再比较 Stage1 和 Stage0 的输出稳定性。当前已经有 `selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax`，它是一个小型 M0 lexer 方向程序，可以校验内置源码的 token kind 序列。

M0V0.1.8 把 selfhost lexer 拆成真正的 M0 模块：

- `selfhost/src/aurex/selfhost/lexer/core.ax`：token 常量、`TokenSpan`、字符分类、关键字识别、trivia 跳过、`scan_token` 和兼容包装 `scan_next`。
- `selfhost/src/aurex/selfhost/lexer/dump.ax`：token kind 打印和 `dump_tokens`。
- `selfhost/src/aurex/selfhost/smoke/lexer_smoke.ax`、`lexer_ranges.ax`：很薄的 smoke 入口程序，通过 import 复用这些模块。
- `selfhost/src/aurex/selfhost/tool/lexer_dump.ax`、`lexer_file.ax`：用于 golden 测试的小工具入口。

这还不是完整自举，但已经是一个重要结构进展：M0 selfhost 代码开始依赖 Stage0 模块加载器，而不是在每个入口文件里复制同一份 scanner。

`selfhost/src/aurex/selfhost/smoke/lexer_ranges.ax` 证明 scanner 现在能返回 parser 所需的 `kind/begin/end` byte offset。
`selfhost/src/aurex/selfhost/smoke/parser_smoke.ax` 是第一个 parser seed smoke，会校验一个小型递归下降子集：`module`、`import`、`extern c`、函数签名和一个 `export c fn` 函数体外壳。

现在 `selfhost/src/aurex/selfhost/tool/lexer_file.ax` 也已经加入。它通过显式 runtime file IO 读取 `examples/hello.ax`，输出 token kind 流，并和 `tests/golden/selfhost_lexer_file_hello.tokens` 对比。
`tools/compare_selfhost_lexer.sh` 还会在本地 corpus 上把这个 M0 lexer 输出和生产 C++ Stage0 lexer 的 token kind 输出直接对比。

```sh
make -C selfhost check
```

手动编译 selfhost 源码时，需要传入 selfhost import root：

```sh
build/m0c -I selfhost/src selfhost/src/aurex/selfhost/tool/lexer_file.ax -o build/lexer_file.c
build/m0c -I selfhost/src selfhost/src/aurex/selfhost/smoke/parser_smoke.ax -o build/parser_smoke.c
```
