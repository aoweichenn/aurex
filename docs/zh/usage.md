# 使用说明

## 构建

```sh
cmake -S . -B build/full-llvm
cmake --build build/full-llvm -j
```

仓库中常见本地构建目录：

- `build/frontend-tests`
- `build/full-llvm-fedora`
- `build/full-llvm`

## 编译程序

```sh
build/full-llvm/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

`--emit=exe` 是默认模式。也可以显式选择输出：

```sh
build/full-llvm/bin/aurexc --emit=tokens examples/hello.ax
build/full-llvm/bin/aurexc --emit=ast examples/hello.ax
build/full-llvm/bin/aurexc --emit=checked examples/hello.ax
build/full-llvm/bin/aurexc --emit=ir examples/hello.ax
build/full-llvm/bin/aurexc --emit=llvm-ir examples/hello.ax
build/full-llvm/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/full-llvm/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

native 输出需要 clang。需要 libc 函数时，在 Aurex 源码里声明窄 `extern c` 边界。

## 模块和 import

模块文件以 `module path;` 开头。import 查找只使用导入者目录和显式 `-I`：

```sh
build/full-llvm/bin/aurexc -I examples/libs examples/app.ax -o build/tests/app
```

没有标准库根、环境变量查找或安装前缀推导。

## 当前语法示例

泛型使用尖括号：

```aurex
fn id<T>(value: T) -> T {
    return value;
}

let value: i32 = id<i32>(1);
```

slice 使用 `[]T` / `[]mut T`：

```aurex
fn sum(values: []i32) -> i32 {
    var total: i32 = 0;
    for value in values {
        total += value;
    }
    return total;
}
```

lambda 使用 C++ 风格 capture-list：

```aurex
fn main() -> i32 {
    let base: i32 = 2;
    let add = [=](value: i32) -> i32 => value + base;
    return add(40);
}
```

低层 builtin 当前保持短名字，不使用新的 `intrinsic.*` namespace：

```aurex
let addr: usize = ptraddr(&value);
let ptr: *const i32 = unsafe { ptrat<*const i32>(addr) };
```

## 测试

常用命令：

```sh
cmake --build build/frontend-tests --target aurex_frontend_tests -j20
build/frontend-tests/bin/aurex_frontend_tests

cmake --build build/full-llvm-fedora --target aurexc aurex_tests -j20
ctest --test-dir build/full-llvm-fedora --parallel 8 --output-on-failure
```

推荐用 `tools/run_tests.sh` 跑完整测试。脚本会自动配置、构建并按 CPU 数量并发调度 CTest；需要手动限制并发时设置 `AUREX_CTEST_JOBS=4`。

提交前至少运行触及面的 focused tests、完整 frontend tests、完整 integration/gtest suite 和 `git diff --check`。
