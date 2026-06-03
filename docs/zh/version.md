# 版本文档

## M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线

M7c/M7d 设计基线已固定，文档入口为
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。

该基线建立在 M6 resource/drop facts、M7a `BodyFlowGraph` / `BodyLoanCheckResult` / `BorrowSummary` 和 M7b
`FunctionBorrowContract` / reborrow / two-phase receiver 之上。后续 M7c 目标是完整 safe borrow 与 lifetime core：
contextual `origin` 参数、`&[origin] T` / `&mut[origin] T`、origin union、region solver、type-outlives、
trait/generic lifetime predicates 和 `BorrowEscapeAnalyzer` 退场。M7d 目标是 RAII/dropck 地基：dropck facts、
destructor body safety、generic drop glue type-outlives、place-level resource state、partial move/reinit/drop flag
以及 RAII user surface。

语法决策：不采用 Rust apostrophe lifetime，也不新增 `ref[...] T` 作为 source surface。显式 origin 绑定到现有引用前缀：
`&[data] T` 表示 shared borrowed view，`&mut[data] T` 表示 mutable borrowed view，`&[left | right] T`
表示 origin union。函数边界继续优先使用 `@borrow(return = [...])`。
该语法只在 type context 中解析，不占用未来 lambda/closure 的表达式语法空间。完整 closure/lambda capture 后续独立设计；
M7c/M7d 只预留 `ClosureCaptureFact` / `ClosureEnvironmentFact` 与 dropck/place-state 复用路线。

工程决策：`src/sema/internal/` 以后只作为 private implementation root，不再直接新增文件；M7c/M7d 新代码必须按
`borrow/`、`lifetime/`、`dropck/`、`place/`、`diagnostics/`、`pipeline/` 等职责拆子目录。其他 compiler stage
同样遵守该解耦规则。全局 `compiler-engineering` 与 `cpp-project-standards` skill 已同步该约束。

## M7b Borrow Contract、Reborrow 与 Lifetime Surface 实现收口

M7b WP1-WP7 已完成实现收口，文档入口为
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线](m7b-borrow-contract-design.md) 和
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 路线图](m7b-roadmap.md)。

M7b 已把 M7a 的 `BorrowSummary` / `BodyLoanCheckResult` 内部 facts 提升为函数边界
`FunctionBorrowContract`，引入函数声明前装饰器式 `@borrow(return = [param, self])`，补齐 trait/generic
borrowed-return contract、summary-vs-contract enforcement、reborrow parent/child loan、method receiver access、
receiver auto-borrow two-phase reservation/activation，以及 query/cache/tooling 投影。

当前实现包括：

- `CheckedModule::borrow_contracts`、`FunctionBorrowContract` fingerprint/dump/query 投影。
- `BodyLoan::parent_loan` reborrow model，child effective place 归一到 parent 底层 place，并保留 known-disjoint
  field projection 放宽。
- `FunctionCallBinding` / `TraitMethodCallBinding` 的 `receiver_access`、`receiver_auto_borrow` 和
  `receiver_two_phase_eligible`。
- `BodyTwoPhaseBorrow` reserve/activate facts，reservation conflict 与 activation conflict diagnostics。
- `TypeCheckBodyAuthority` 的 borrow contract/body loan fingerprint、reborrow/two-phase counts 和
  diagnostics-emitted 状态位。
- IDE semantic fact detail 中的 `borrow_contract` 与 `body_loan_check loans/reborrows/two_phase/conflicts`。

M7b 明确不做 full Rust-style lifetime generics、full Polonius Datalog、raw pointer alias safe proof、partial
move / replace / take / swap 完整 place-level resource semantics、`dyn Trait`、async drop 或 generator borrow。

## M7a CFG-sensitive borrow facts release closure

当前实现阶段是 M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking。M7 设计基线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)，
执行路线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图](m7-roadmap.md)。

M7-WP2 Phase 1 已完成 collect-only BodyFlowGraph facts：`CheckedModule::body_flow_graphs` 按
`FunctionLookupKey` 保存函数体 point、edge、place 和 action timeline；内部收集器覆盖 statement/expression
entry-exit、顺序点、branch、return、call、defer cleanup、read/write/move-candidate 以及 shared/mutable
borrow action，并提供稳定 dump。该阶段只生产事实，不新增 diagnostics，不替换 `BorrowEscapeAnalyzer`，不改变
M6 move/resource/cleanup 行为。

M7-WP3 Phase 2/3 已完成本地 local loan checker：`CheckedModule::body_loan_checks` 现在保存
`Origin` / `Loan` / conflict facts、shadow/enforced diagnostic mode、projection-aware conflict 结果和稳定
dump。checker 复用 Phase 1 `BodyFlowGraph`，用 carrier-local liveness 支持直接本地 borrow 的 last-use 后写入，
对 active shared/mutable loan 与 write、owned-consume move、read、shared/mutable borrow 的冲突进行 shadow 记录；
语义管线已启用 enforced diagnostics，并给出冲突 primary diagnostic 和 loan 创建 note。`move_candidate` 只有在
M6 `OwnedUseMode::owned_consume` 时才作为 move 冲突，普通 read/copy 不误报。

M7-WP4 已完成 `BorrowSummary` 与 borrowed-return facts：`CheckedModule::function_calls` 记录普通函数、泛型函数、
普通方法和泛型方法的 direct call binding；`CheckedModule::borrow_summaries` 按 `FunctionLookupKey` 保存函数级
`FunctionBorrowSummary`，包含 parameter/local/temporary origins、return origin dependency set、unknown-return
标志、local/temporary escape 标志和 stable fingerprint。call wrapper 会把 callee summary 的 parameter dependency
映射到 caller 实参；generic parameter 与 associated projection 在 summary 内按可能含借用处理，保证 `T = &U`
实例不会漏掉 parameter-origin dependency；函数值调用、callee 缺 summary、raw/unchecked pointer path 和 callee
local/temporary return 不会被当作 safe proof，而是 conservative unknown。

M7-WP5 已补齐 projection/drop/reinit/cleanup matrix：整 local assignment 生成 `reinit`，field/index/deref
assignment 保持 `write`，词法 block local cleanup 生成 `cleanup_storage` invalidation；loan checker 对
write/reinit/drop/cleanup/move/read/shared-borrow/mutable-borrow 使用同一套 projection-aware conflict policy。当前语言
仍没有显式 user drop syntax，所以 `drop` 已作为 checker action 支持，source-level drop emission 仍等待后续语法或
lowering 接入。

M7-WP6 已完成 diagnostics、query 与 tooling projection：`TypeCheckBodyAuthority` 混入 borrow summary / body
loan check fingerprint、origin/dependency/loan/conflict count 和 unknown/local-escape/diagnostic-emitted 状态位；
CLI incremental-cache subject collection 与 IDE snapshot query collection 都消费同一份 `CheckedModule`
facts。IDE semantic facts 新增 `borrow_summary` 与 `body_loan_check`，函数 hover 可展示 summary dependency；
enforced diagnostics 现在包含 primary conflict、loan creation、invalidating action 和可定位时的 later carrier use
note，同时保持按冲突点/range 的 cascade suppression。`dump_checked_module` 输出 `body_loan_checks` summary 与
stable fingerprint。

W7a release closure 已完成性能/内存边界收口：普通 `--check` 路径保留 `BodyLoanCheckResult` 与
`FunctionBorrowSummary` 等稳定 checked facts，不长期保留 full `BodyFlowGraph`；checked/typed 输出和 IDE/tooling
仍可保留 CFG facts。非借用返回函数的 summary 构建走 fast path，不扫描完整函数体；direct/trait call binding
由 `CheckedModule` 维护 expr-id index。release 全量测试、coverage、query sanitizer、perf/stress gate 已验证通过。

M7-WP7 已完成 release closure 文档边界：当前仍不移除 `BorrowEscapeAnalyzer`。WP4/WP6 summary 已记录
borrowed-return facts，但旧 borrowed-local escape 诊断继续由现有 analyzer 负责；只有在 parity 覆盖当前
borrowed-view 逃逸矩阵后才讨论降级/移除。M7a 继续明确不做完整 Rust-style lifetime surface、full Polonius
Datalog engine、raw pointer alias safe proof、user destructor syntax、partial move / replace / take / swap
完整 place-level resource semantics、`dyn Trait`、async drop 或 generator borrow。

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
