# 使用文档

本文描述 **M2 language-core-no-std** 阶段的用法。本阶段冻结并移除了标准库，所有示例和测试都应围绕语言语法、语义、IR 和后端本身展开。

## 构建

```sh
cmake -S . -B build
cmake --build build -j
```

## 运行 hello

```sh
build/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

期望输出：

```text
hello from Aurex M2
```

## 输出模式

```sh
build/bin/aurexc --emit=tokens examples/hello.ax
build/bin/aurexc --emit=ast examples/hello.ax
build/bin/aurexc --emit=checked examples/hello.ax
build/bin/aurexc --emit=ir examples/hello.ax
build/bin/aurexc --emit=llvm-ir examples/hello.ax
build/bin/aurexc -S examples/hello.ax -o build/tests/hello.s
build/bin/aurexc -c examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` 是默认模式。`--emit=asm`、`--emit=obj` 和 `--emit=exe` 形式的
native 输出需要 `-o`。driver 风格的 `-S` 和 `-c` 在省略 `-o` 时会推导
`input.s` 和 `input.o`。

命令语法仍保持 clang 风格的扁平 flags，但 `--help` 会按一级动作选项和二级
修饰选项分组。`--clang`、`--clang-arg` 这类 native backend 修饰选项只适用
于 native 输出模式；如果和 `--emit=ir` 或 `--check` 这类 frontend-only 模式
组合，会作为参数错误拒绝。

## import

模块查找顺序：

1. 导入者所在目录。
2. 显式传入的 `-I path`。

本分支没有隐式标准库路径。需要共享模块时显式传入 import root：

```sh
build/bin/aurexc -I tests/samples/imports --emit=checked tests/samples/positive/modules/import_path.ax
build/bin/aurexc --import-path tests/samples/imports --emit checked tests/samples/positive/modules/import_path.ax
```

## C FFI

本分支不再提供 std C FFI 包装。语言核心测试如需 C 函数，应在样例中声明最小 `extern c` 边界：

```aurex
extern c {
    fn puts(text: *const u8) -> i32 @name("puts");
}
```

## 测试

```sh
tools/run_tests.sh
```

测试覆盖 lexer/parser、模块、可见性、泛型、sum type、pattern matching、`?`、`defer`、`for`、普通值语义、IR、LLVM lowering、native execution 和安装后 compiler 执行。标准库 API、std host support、M1 system/build-tool 样例不在本分支验证范围内。
