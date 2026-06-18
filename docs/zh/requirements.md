# 需求分析

## 当前目标

Aurex 当前版本要提供一个可持续演进的小型系统语言核心：

- 编译器可在无标准库源树的环境中构建、检查、lowering、生成 LLVM IR 并输出 native 目标。
- 语言样例自包含，避免标准库加载或 host support 掩盖编译器语义问题。
- 语法表面优先收敛，不保留旧写法兼容分支。
- 新语法必须贯通 parser、AST dump、sema、IR lowering、样例、负例和文档。

## 必须保留的能力

- 手写 lexer/parser、AST、模块加载、import、可见性和 re-export。
- 标量、raw pointer、safe reference、array、slice、tuple、`str`、function type、struct、opaque struct、ADT enum、type alias。
- 函数、extern C、export C、默认参数、命名参数、method、泛型函数/类型/impl、const generic check-only 子集。
- `where` capability、nominal static trait、显式 trait impl、associated type、associated-type equality、trait default method。
- borrowed dyn trait view、borrowed dyn composition、supertrait upcast 和 checked vtable dispatch。
- `let` / `var`、pattern、let-else、block expression、if/match expression、`while`、C-style `for`、`for i in range(...)`、array/slice value for-in、`break`、`continue`、`defer`、`?`。
- compiler-owned `Copy` capability、move/reinit 检查、cleanup/drop flag、borrow summary、local loan checking 和 lifetime/origin diagnostics。
- Aurex IR、IR verifier/pass pipeline、LLVM backend、clang native 输出。

## 暂缓能力

- 标准库 API、拥有型容器、文件/目录/进程/console 包装。
- 完整 iterator protocol、range value、mutable/reference item iteration、str iteration、generic iterable capability。
- owning dyn、`Box<dyn Trait>`、allocator API、dynamic Drop dispatch、trait-object Drop runtime。
- 完整宏展开、proc-macro、用户 derive lowering、generated token parser consumption。
- closure init-capture、move/consuming capture、closure trait、escaping closure lifetime。
- generic associated type、associated const、specialization、generic const arithmetic、const where predicate。
- 完整 Rust-style lifetime surface、raw pointer alias safe proof、语言级并发/atomic memory model。
