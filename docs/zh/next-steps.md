# 下一步计划文档

版本：0.1.2

## 自举当前阶段

当前 selfhost 自举处在“可度量的 Stage1 前端切片 + AIR snapshot 输出”阶段，还不是能替代 Stage0 的完整自举编译器。

- Stage0 仍是 C++20 编译器，是当前生产编译器。
- selfhost 已包含 M0 编写的 lexer core、token dump、parser seed、ID-backed AST、Stage1 CLI 和 Stage1 AIR snapshot emitter。
- Stage1 能读取 `.ax` 文件，解析 seed 覆盖范围内的语法，并输出 `aurex_ir v0` 文本快照。
- bootstrap 链路覆盖 selfhost lexer golden 对比、parser smoke、Stage1 snapshot 输出和 selfhost bundle 可见性。

## 当前已具备能力

M0 lexer：

- token 化本地语料。
- 输出 token kind，用于和 Stage0 对比。
- 通过 ranges、smoke 和 golden 测试。

M0 parser seed：

- `module` / `import`。
- 一个 `extern c` block。
- `opaque struct` 和 extern function signature。
- 多个 `export c fn`。
- 函数参数、返回类型、primitive/named/pointer type。
- 表达式语句和 `return`。
- integer、identifier、string、c string、bool、null、unary/binary/call 表达式。
- 基本运算符优先级和调用参数。

Stage1 AIR snapshot：

- 输出 `aurex_ir v0` header。
- 输出 extern function、opaque record、export function signature。
- 输出 expression value snapshot。
- expression value 已按当前函数 block 限定，不再把全模块表达式重复输出到每个函数。
- 对 parser seed 未覆盖的模块输出 `selfhost_module ... lowering(ast_pending)` 占位，保持 selfhost bundle 可度量。

## 距离 M0 最终自举的缺口

- Parser 覆盖不完整：普通 `fn`、`struct`、`enum`、`const`、`let` / `var`、赋值、block、`if`、`while`、field access、index、cast、struct literal、array 等还未完整覆盖。
- Stage1 没有完整 sema：类型解析、符号解析、作用域、重复定义检查、函数签名校验、调用参数校验、返回类型校验、ABI/mangling 规则还需要实现。
- Stage1 没有真正 IR verifier：目前主要是 snapshot 输出，不是完整可执行 IR 合法性验证。
- Stage1 lowering 不完整：还缺 locals/slots、CFG、branch、loop、phi、record layout、global constant lowering 等。
- Stage1 没有 LLVM handoff：还不能把 Stage1 产物交给现有 LLVM backend 编译成 native。
- 还没有 fixed-point：Stage1 不能完整编译自己，也不能证明 `Stage0 -> Stage1 -> Stage1'` 收敛。

## 实现计划

1. 扩展 parser 到 selfhost 编译器所需最小语法  
   优先覆盖当前 selfhost 源码真实使用的语法：普通 `fn`、`let` / `var`、block、`if`、`while`、赋值和 call。

2. 稳定 AST 结构  
   给 item、stmt、expr、type 增加缺失节点。每增加一类语法，都补 parser smoke 和 Stage1 snapshot 断言。

3. 实现 Stage1 minimal sema  
   先做 module item 表、函数表、local symbol table、expression type、return/call/assign 类型检查。不急着做完整错误恢复。

4. 让 Stage1 lowering 从 snapshot 走向真实 AIR 子集  
   先覆盖 function、block、value、terminator、locals、return、call、binary/unary、`if` 和 `while` CFG。

5. 实现 Stage1 IR verifier  
   先检查 block/value 引用范围、terminator 存在、function 参数和返回类型一致、call 目标和参数数量一致。

6. 设计 LLVM handoff  
   优先让 Stage1 输出 Stage0 backend 能读取的 AIR 格式，或先在 Stage0 增加 AIR reader，把 Stage1 AIR 喂给现有 LLVM backend。

7. 建立 fixed-point bootstrap 链路  
   目标链路：

   ```text
   Stage0(C++) builds Stage1(M0)
   Stage1 compiles selfhost compiler sources to AIR
   Stage0 AIR reader + LLVM backend builds Stage1'
   Stage1' repeats compile
   compare stable outputs
   ```

   先比较结构化 IR/golden，再比较 native 行为，最后再追求 bit-for-bit。

## 下一步优先级

下一步最值得做的是 `普通 fn + let/var + if/while` 的 parser/AST 覆盖。这是通向编译 selfhost 自身的最短路径，比先做完整 sema 或 LLVM 接入更稳。
