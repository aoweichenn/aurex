# 版本文档

## M7-WP2 Phase 1 collect-only BodyFlowGraph facts

当前实现阶段是 M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking。M7 设计基线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)，
执行路线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图](m7-roadmap.md)。

M7-WP2 Phase 1 已完成 collect-only BodyFlowGraph facts：`CheckedModule::body_flow_graphs` 按
`FunctionLookupKey` 保存函数体 point、edge、place 和 action timeline；内部收集器覆盖 statement/expression
entry-exit、顺序点、branch、return、call、defer cleanup、read/write/move-candidate 以及 shared/mutable
borrow action，并提供稳定 dump。该阶段只生产事实，不新增 diagnostics，不替换 `BorrowEscapeAnalyzer`，不改变
M6 move/resource/cleanup 行为。

下一实现包是 diagnostic-shadow local loan checker：在这些 facts 上引入本地 `Place` / `Origin` / `Loan`
ID 表、loan liveness、projection conflict matrix 和 would-diagnose 记录。只有在 shadow parity 覆盖当前
borrowed-view 逃逸矩阵后，才进入 enforced diagnostics 和 `BorrowEscapeAnalyzer` 降级/移除讨论。

## M6-WP2/WP3/WP4/WP5/WP6/WP7 资源、cleanup、drop-glue 与 tooling 基线

M6 Resource And Access Semantics 已作为 M7 输入基线收口。M6-WP1 已完成三轮设计审视，完整设计基线记录在
[Aurex M6 资源、值生命周期与访问语义调研和三轮设计审视基线](m6-resource-access-semantics-design.md)，
执行路线记录在 [M6 资源、值生命周期与访问语义路线图](m6-roadmap.md)。

M6-WP2/WP3/WP4/WP5/WP6/WP7 已完成 M6 实现基线：compiler-owned `Copy`、内部 `Discard` / `NeedsDrop` /
ownership resource summary、结构化类型分类、stable resource fingerprint、checked dump resource summary、
expression owned-use side table、whole-local move analysis、move 后重新初始化、consume-origin diagnostics、
lexical cleanup-action stack lowering、`defer` 组合、drop flag，以及正式 IR `drop` / `drop_if` cleanup 节点。
收口内容还包括 `BodySlotKind::destructor_drop` destructor body identity、stable drop-glue key、
target-independent drop-glue planner、IDE resource hover projection、generic parameter hover fallback 和
`aurex-lsp` stdio 入口。partial field move、indexed move-out、consuming pattern payload 和 non-`Copy` `?`
payload transfer 仍然拒绝；用户 destructor syntax 与 custom destructor lowering 继续后移。

下一实现包是 M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking。完整 borrow checker、lifetime surface、
partial move、`dyn Trait`、region、async drop、全量 array ABI 解禁和标准库重建继续后移。

## M5 default trait methods release baseline

当前文档基线是 M5 default trait methods release。M5 建立在已经收口的 M4 trait/protocol release baseline
之上，已经收口一个聚焦的 M4 后设计流：nominal static trait 上的 default method body。

M5 设计基线记录在
[Aurex M5 Default Trait Methods 调研与设计基线](m5-default-trait-methods-design.md)，阶段路线见
[M5 Default Trait Methods 路线图](m5-roadmap.md)，发布契约记录在
[Aurex M5 Default Trait Methods Release Baseline](m5-release-baseline.md)。M5 范围是 trait-owned default
bodies、显式 method origin、impl override vs inherited-default completeness、单态化后的 static direct-call
lowering，以及 tooling / diagnostics / query projection。

M5 不包含 dynamic trait object、object safety、vtable ABI、specialization、associated constants、default
associated types、generic associated types、blanket impl、package-level coherence expansion、class-like sugar 或
resource semantics。

## M4 trait/protocol release baseline

当前已实现的 trait 基线是 M4。M4 建立在已经收口的 M2 language-core-no-std、M2.5 frontend/query foundation
和 M3 module/generic/query-backed compiler architecture 之上，完成 nominal static trait、显式 trait impl、
generic trait predicate、static trait method dispatch、associated type，以及 IDE/tooling/diagnostics 投影。

发布契约记录在 [Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md)。M5 从该基线出发，不重新打开
M4 WP1-WP8。

## M2 language-core-no-std

M2 从 M1 的失败经验中收缩出来：停止继续修补 M1 的标准库、自举和系统样例路线，转而冻结标准库、删除干扰项，并重新聚焦语言核心设计。

M1 已经舍弃。它的问题是扩张面过大：标准库、host support、构建工具样例、自举实验和语言语义同时推进，导致核心语法与类型规则没有稳定下来。M2 不再把 M1 的 std/selfhost/build-tool 产物当作当前基线。

已完成：

- 删除 `std/` 源树和 host-c support。
- 删除 driver 标准库查找、import path 注入、support source 链接和相关头/源文件。
- 删除 CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 删除安装规则中的 `share/aurex/std`。
- 删除 std/M1/system 样例和 std 专项测试。
- 将 `Result` / `Option` 相关语言样例改为自包含定义。
- 将 defer/variadic 样例改为局部 `extern c`，不依赖 std 包装。
- 移除语义层 `std.core.vec/map/result` 专用 ownership 约束。
- 移除 M1 的语言级 `move(...)`、`noncopy struct` 和 use-after-move 追踪。
- 更新测试清单，使 sample suite 回到语言核心。

当前有效基线：

- C++20 Stage0 编译器。
- 手写 lexer/parser 和 ID-backed AST。
- sema、Aurex IR、IR verifier、pass pipeline。
- LLVM backend 和 clang native 输出。
- 自包含 positive/negative `.ax` 语言样例。

风险控制：

- 保留 match payload、`?`、`for`、`defer` 和基础类型系统的语言级正/负例。
- 保留 native hello、IR/LLVM lowering、安装后 compiler 执行验证。
- 后续资源约束不再通过标准库 hardcode 添加，应作为独立资源语义专题设计。

明确暂缓：

- 重新设计库层；不是复活旧 `std`。
- 重新评估 selfhost/Stage1。
- 重新设计 M1 build tool / system examples。
- 重新评估 host support 自动链接。

这些内容必须等 M2 基础语法、值语义、`unsafe`、slice/string 和泛型约束稳定后再重新评估；拥有型资源库还需要资源语义专题完成。
