# 下一步计划文档

版本：0.1.2

## 自举当前阶段

当前 selfhost 自举处在“可度量的 Stage1 前端切片 + TAC/AIR snapshot 输出”阶段，还不是能替代 Stage0 的完整自举编译器。

- Stage0 仍是 C++20 编译器，是当前生产编译器。
- selfhost 已包含 M0 编写的 lexer core、token dump、parser seed、ID-backed AST、Stage1 CLI、Stage1 TAC snapshot emitter 和初版 AIR model/lowering/verifier。
- Stage1 能读取 `.ax` 文件，解析 seed 覆盖范围内的语法，并输出 `aurex_tac v0` 文本快照；函数体内还嵌入 `air_ir v0` / `air_cfg v0` 注释快照。
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

Stage1 TAC/AIR snapshot：

- 输出 `aurex_tac v0` header。
- 输出 extern function、opaque record、export function signature。
- 输出三地址码表达式临时值。
- AIR 已拆分为 model、binding、lowering、text、verify 模块。
- AIR 函数快照包含函数头、linkage、参数表、局部表、block、value DAG、instruction 和 terminator。
- AIR value 已记录 result type、identifier/field/struct 名称范围、cast/type-op 目标类型、call/struct literal 参数。
- AIR name value 已能绑定到 param/local/item；let/var/assign 指令也记录局部绑定。
- AIR verifier 已覆盖 header、params、locals、value 引用、value args、binding、instruction 和 terminator 合法性。
- 临时值已按当前函数 block 限定，不再把全模块表达式重复输出到每个函数。
- 对 parser seed 未覆盖的模块输出 `selfhost_module ... lowering(ast_pending)` 占位，保持 selfhost bundle 可度量。

## 距离 M0 最终自举的缺口

- Parser 覆盖还没达到完整编译 selfhost 的程度，后续仍要补 module bundle 中的未覆盖语法边界和错误恢复。
- Stage1 sema 还没持久化 typed AST：当前能做基础解析/类型/符号检查，但 AIR 的 result type 仍主要来自 AST 节点携带的类型字段，后面需要统一类型注解表。
- Stage1 lowering 还没有真实可执行 AIR 后端：当前 AIR 是结构化注释快照，不是 backend handoff 格式。
- AIR 还缺 slots/alloca、record layout、global constant lowering、复杂 lvalue descriptor、phi/SSA 合流、跨模块 item/import 绑定。
- Stage1 没有完整 TAC/AIR verifier 与 backend verifier 的闭环：目前 verifier 已覆盖 AIR 结构合法性，但还没校验类型等价、call signature、control-flow dominance。
- Stage1 没有 LLVM handoff：还不能把 Stage1 产物交给现有 LLVM backend 编译成 native。
- 还没有 fixed-point：Stage1 不能完整编译自己，也不能证明 `Stage0 -> Stage1 -> Stage1'` 收敛。

## 实现计划

1. 扩展 parser 到 selfhost 编译器所需最小语法  
   优先覆盖当前 selfhost 源码真实使用的语法：普通 `fn`、`let` / `var`、block、`if`、`while`、赋值和 call。

2. 稳定 AST 结构  
   给 item、stmt、expr、type 增加缺失节点。每增加一类语法，都补 parser smoke 和 Stage1 snapshot 断言。

3. 稳定 Stage1 typed AST / AIR 类型注解  
   把 sema 推导出的类型、item 绑定、local 绑定持久化，避免 AIR lowering 继续依赖源码范围反查。

4. 让 Stage1 AIR 从 snapshot 走向真实 backend handoff  
   先覆盖 function、block、value、instruction、terminator、locals、return、call、binary/unary、`if` 和 `while` CFG。

5. 扩展 Stage1 AIR verifier
   继续检查类型等价、function 参数和返回类型一致、call 目标/参数数量、branch condition、dominance 和 block reachability。

6. 设计 LLVM handoff  
   优先让 Stage1 输出 Stage0 backend 能读取的 TAC 格式，或先在 Stage0 增加 TAC reader，把 Stage1 TAC 喂给现有 LLVM backend。

7. 建立 fixed-point bootstrap 链路  
   目标链路：

   ```text
   Stage0(C++) builds Stage1(M0)
   Stage1 compiles selfhost compiler sources to TAC
   Stage0 TAC reader + LLVM backend builds Stage1'
   Stage1' repeats compile
   compare stable outputs
   ```

   先比较结构化 IR/golden，再比较 native 行为，最后再追求 bit-for-bit。

## 下一步优先级

下一步最值得做的是把 sema 结果持久化到 typed AST/AIR 注解表，并补 AIR slot/lvalue descriptor。这是从“可读快照”推进到“后端可消费 IR”的最短路径。
