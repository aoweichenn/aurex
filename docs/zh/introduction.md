# 项目介绍

Aurex 是一个 C++20 实现的系统语言编译器项目。当前工作重心是把语言核心做稳：泛型、闭包、控制流、资源语义、borrow 检查、IR lowering、LLVM backend 和样例测试必须形成一致闭环。

当前仓库不维护标准库源树，也不依赖标准库包装来验证语言能力。语言样例应自包含；需要 libc 或平台能力时，通过窄 `extern c` 声明表达。

当前优先级：

- 语法表面统一：尖括号泛型、`as` cast、`[]T` / `[]mut T` slice、C++ 风格 lambda capture-list。
- 泛型闭环：parser、AST、sema、generic side table、IR lowering、LLVM lowering 和测试保持同一套类型身份。
- 闭包和控制流：lambda capture-list、counted `range(...)`、array/slice value for-in 形成当前可用子集。
- 文档收敛：只保留中文当前项目文档，不保留旧路线和阶段流水账。
