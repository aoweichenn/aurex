# 需求分析文档

## 当前分支目标

M2 `language-core-no-std` 阶段的需求是隔离语言核心验证：

- 编译器必须能在没有标准库源树的情况下构建、安装和运行。
- import 只能来自导入者目录和显式 `-I`。
- native 输出不能隐式链接任何 std support 源文件。
- 样例和测试必须能区分语言特性耗时与外部脚本/标准库加载耗时。
- 语言核心样例应自包含；`Result` / `Option` 等用于 `?` 验证时在样例内定义。

## 保留能力

- 手写 lexer/parser。
- 模块、import、可见性、re-export。
- 基础类型、struct、enum、generic struct/function。
- pattern matching、guard、or-pattern。
- `if` expression、block expression、`while`、`for`、`break`、`continue`。
- `defer` 和 `?`。
- 普通值语义；数组和含数组类型不能作为函数 by-value 参数/返回、赋值目标或 enum payload 的限制暂时保留。
- Aurex IR、pass pipeline、LLVM IR、clang native 输出。

## 暂缓能力

- 标准库 API、容器、文件、目录、进程、console。
- M1 frontend/build-tool 样例。
- std host support 和安装后 std 查找。

M1 的语言级 `move(...)` 和 `noncopy struct` 不再属于 M2 当前需求；资源语义等基础语法、`unsafe`、slice/string、safe reference 方向和非资源类 trait/where 规则稳定后再单独重新设计。
