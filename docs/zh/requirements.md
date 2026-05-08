# 需求分析文档

## 当前分支目标

`language-core-no-std` 分支的需求是隔离语言核心验证：

- 编译器必须能在没有标准库源树的情况下构建、安装和运行。
- import 只能来自导入者目录和显式 `-I`。
- native 输出不能隐式链接任何 std support 源文件。
- 样例和测试必须能区分语言特性耗时与外部脚本/标准库加载耗时。
- 语言核心样例应自包含；`Result` / `Option` 等用于 `?` 验证时在样例内定义。

## 保留能力

- 手写 lexer/parser。
- 模块、import、可见性、re-export。
- 基础类型、struct、enum、generic struct/enum/function/method。
- pattern matching、guard、or-pattern。
- `if` expression、block expression、`while`、`for`、`break`、`continue`。
- `defer`、`move`、noncopy、`?`。
- Aurex IR、pass pipeline、LLVM IR、clang native 输出。

## 暂缓能力

- 标准库 API、容器、文件、目录、进程、console。
- M1 frontend/build-tool 样例。
- std host support 和安装后 std 查找。

这些能力等语言级 ownership、borrow/drop、trait/where/capability 规则稳定后再恢复。
