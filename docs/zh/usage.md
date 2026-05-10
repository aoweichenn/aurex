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
build/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` 是默认模式。native 输出需要 `-o`。

## import

模块查找顺序：

1. 导入者所在目录。
2. 显式传入的 `-I path`。

本分支没有隐式标准库路径。需要共享模块时显式传入 import root：

```sh
build/bin/aurexc -I tests/samples/imports --emit=checked tests/samples/positive/modules/import_path.ax
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
