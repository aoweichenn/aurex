# 使用文档

版本：0.1.2

## 构建

```sh
cmake -S . -B build
cmake --build build -j
```

依赖项：

- CMake。
- 支持 C++20 的 C++ 编译器。
- LLVM 开发库。
- clang，用于 `--emit=asm`、`--emit=obj` 和 `--emit=exe`。

## 编译运行

```sh
build/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

预期输出：

```text
hello from Aurex M0
```

系统级示例使用共享 example 模块：

```sh
build/bin/aurexc -I examples/libs examples/system/file_journal/main.ax -o build/tests/file_journal
build/tests/file_journal
```

## 查看中间产物

```sh
build/bin/aurexc --emit=tokens examples/hello.ax
build/bin/aurexc --emit=ast examples/hello.ax
build/bin/aurexc --emit=modules tests/samples/positive/modules/module_math.ax
build/bin/aurexc --emit=checked examples/hello.ax
build/bin/aurexc --emit=ir examples/hello.ax
build/bin/aurexc --emit=llvm-ir examples/hello.ax
```

## 生成 native 输出

```sh
build/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` 是默认模式。

如果 clang 不在 PATH 中：

```sh
build/bin/aurexc --clang /path/to/clang examples/hello.ax -o build/tests/hello
```

透传 clang 参数：

```sh
build/bin/aurexc --clang-arg -g --clang-arg -O2 examples/hello.ax -o build/tests/hello
```

## 优化级别

```sh
build/bin/aurexc --emit=ir --opt-level O1 examples/hello.ax
```

`O0` 只做 IR 验证；`O1` 及以上启用当前保守 pass。

等价写法：

```sh
build/bin/aurexc --emit=ir -O1 examples/hello.ax
build/bin/aurexc --emit=ir -O O1 examples/hello.ax
```

## import 和标准库

手动 import path：

```sh
build/bin/aurexc -I tests/samples/imports tests/samples/positive/modules/import_path.ax -o build/tests/import_path
```

指定标准库：

```sh
build/bin/aurexc --stdlib /path/to/std tests/samples/positive/std/std_text.ax -o build/tests/std_text
```

关闭标准库：

```sh
build/bin/aurexc --no-stdlib examples/hello.ax -o build/tests/hello
```

选择 std backend：

```sh
build/bin/aurexc --std-backend host-c tests/samples/positive/std/std_text.ax -o build/tests/std_text
build/bin/aurexc --std-backend none examples/hello.ax -o build/tests/hello
```

通过环境变量指定标准库：

```sh
AUREX_STDLIB=/path/to/std build/bin/aurexc tests/samples/positive/std/std_text.ax -o build/tests/std_text
```

`--stdlib` 的优先级高于 `AUREX_STDLIB`。

## 测试

```sh
tools/run_tests.sh
tools/check_golden.sh
tools/bench.py
```

## 安装

```sh
cmake --install build --prefix build/install
build/install/bin/aurexc tests/samples/positive/std/std_text.ax -o build/tests/std_text.installed
```

安装后标准库位于：

```text
build/install/share/aurex/std
```

## 常见问题

- `native output requires -o`：native 输出需要显式 `-o output`。
- `failed to locate Aurex standard library`：设置 `--stdlib`、`AUREX_STDLIB`，或确认安装前缀下存在 `share/aurex/std`。
- clang 调用失败：使用 `--clang` 指定路径，或通过 `--clang-arg` 传递目标平台参数。
- 只想检查语义：使用 `--check` 或 `--emit=check`，不会生成 native 产物。
