# Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图

日期：2026-06-02

状态：M7a WP2-WP7 已完成实现收口。完整设计依据见
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)。
M7b 后续设计已经拆出到
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线](m7b-borrow-contract-design.md) 和
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 路线图](m7b-roadmap.md)。

## 0. M7 总目标

M7 的目标是在 M6 resource/cleanup/drop 基线之上，把当前保守的 borrowed-view 逃逸检查和 borrow alias
use-after-move hardening 提升为稳定的 CFG-sensitive borrow/lifetime checker。

M7a 的核心产物：

- `Place`、`Origin`、`Loan`、`Point`、`BorrowAction`、`BorrowSummary` 内部事实模型。
- CFG-sensitive loan liveness。
- projection-aware access conflict。
- borrowed-return contract。
- 函数摘要和 call effect summary。
- 与 M6 move/write/drop/reinit/cleanup action 的统一冲突检查。
- stable checked facts、diagnostics、query/cache/tooling projection。

M7a 不做：

- 完整 Rust-style lifetime surface。
- full Polonius Datalog engine。
- user destructor syntax。
- partial move / replace / take / swap 完整 place-level resource semantics。
- raw pointer alias safety。
- `dyn Trait`、async drop、generator borrow。

## 1. M7-WP1：设计基线与工程入口

目标：

- 固定 M7 设计研究和路线图。
- 把 M7 原则写入全局 compiler skill。
- 明确 M7 与 M6/M8 的边界。

产物：

- `docs/zh/m7-origin-loan-lifetime-design.md`
- `docs/zh/m7-roadmap.md`
- 全局 `compiler-engineering` skill 的 Aurex M7 指南。

验收：

- 文档入口更新到 `docs/zh/README.md`、`docs/zh/next-steps.md`、`docs/zh/progress.md`。
- `git diff --check` 通过。

## 2. M7-WP2：共享 BodyFlowGraph 与 BorrowAction Builder

当前状态：

- Phase 1 已落地 collect-only `BodyFlowGraph` facts：`CheckedModule::body_flow_graphs` 现在按
  `FunctionLookupKey` 暴露 function body 的 point、edge、place 和 action timeline。
- 新增 `src/sema/internal/sema_body_flow_graph.cpp` / `.hpp`，使用迭代式 task stack 收集 statement /
  expression entry-exit、顺序点、branch、return、call、defer cleanup、read/write/reinit/move-candidate、
  lexical cleanup-storage 和 shared/mutable borrow facts。
- `analyze_function_body_with_signature(...)` 在现有 body check、borrow escape 和 move analysis 之后收集 facts；
  当前不新增 diagnostics，不替换 `BorrowEscapeAnalyzer`，不改变 M6 move/resource/cleanup 行为。
- 白盒测试覆盖 projection-aware `&mut source.field` place、call/return/cleanup facts、稳定 dump，以及完整
  `analyze()` 路径把 collect-only graph 写入 `CheckedModule`。

目标：

- 从 `BodyMoveAnalysis` 当前 CFG/worklist 构建逻辑中抽出共享 body flow skeleton。
- 让 move analysis 和 borrow analysis 使用同一条 expression/statement action timeline。
- 把隐式 cleanup/drop 点位纳入可检查 action。

建议实现：

- 新增 `SemaBodyFlowGraph`、`SemaBodyPoint`、`SemaBodyAction` 或等价内部模块。
- 保留现有 `BodyMoveAnalysis` 行为，先做结构抽取，不改变语义。
- 先提供只读 action/point/place fact dump 或白盒访问入口，用于和现有 M6 行为做 parity 校验。
- 使用迭代式 task/worklist，不能引入递归遍历。

验收：

- Phase 1 已验证：`aurex_frontend_tests` / `aurex_tests` 构建通过，release 全量 `ctest` 12/12 通过，
  `tools/check_coverage.sh -j4` 通过。
- Phase 1 已验证：`BodyFlowAnalyzer` 白盒测试覆盖 return、defer cleanup、call、mutable borrow、
  read/move-candidate 和 projection place dump。
- Phase 2/3 已继续在这些 facts 上落地 local loan checker；WP4/WP5 已补充函数 summary、call binding、
  reinit/drop/cleanup-storage conflict matrix 和词法 block local cleanup action。更完整的 `?` / break /
  continue parity matrix 仍归入后续 WP6/WP7。

## 3. M7-WP3：Place/Origin/Loan 本地检查

当前状态：

- Phase 2 diagnostic-shadow 已落地：`CheckedModule::body_loan_checks` 保存本地 `Origin` / `Loan` /
  conflict facts、shadow/enforced mode 和稳定 dump。
- Phase 3 enforced diagnostics 已接入函数体分析：active shared/mutable loan 与 write、owned-consume move、
  read、shared/mutable borrow 的本地冲突会产生 semantic diagnostic，并附 loan creation note。
- checker 使用 Phase 1 point/edge 做 deterministic worklist；直接本地 carrier loan 使用后向 liveness 支持
  last-use 后写入。
- projection matrix 已覆盖 same/prefix conflict、known field disjoint、index/slice/unknown 保守冲突；WP5
  已把整 local 赋值区分为 `reinit`，并把词法 block local cleanup 转成 `cleanup_storage` invalidation。
- `move_candidate` 只有在 M6 `OwnedUseMode::owned_consume` 时才作为 move invalidation，避免普通 read/copy
  误报。
- `BorrowEscapeAnalyzer` 仍保留；WP4 已生成 borrowed-return summary facts，但在 parity 覆盖全部旧 escape
  负例前不降级旧诊断。

目标：

- 引入 place/origin/loan ID 表。
- 支持本地 `&`、`&mut`、slice、safe `str` view 的 loan 发出与存活分析。
- 支持 shared/mutable loan 与 read/write/move/drop/reinit 的基本冲突。

建议实现：

- `Place` root 覆盖 local、parameter、temporary、global、unknown。
- `Origin` 覆盖 local、parameter、temporary、static/global、unknown。
- `Loan` 记录 issued point、kind、place、origin、carrier。
- active loans 使用 dense bitset 或 small sorted vector，按函数规模选择。
- 先分 `collect-only`、`diagnostic-shadow`、`enforced` 三步推进，不在第一批 facts 落地时直接替换 M6 现有诊断。

验收：

- 已验证：borrow last-use 后允许 write。
- 已验证：active shared loan 时 write/reinit/drop/cleanup/mutable reborrow 冲突，active mutable loan 时 read /
  shared borrow 冲突。
- 已验证：field projection disjoint 可放宽，same/prefix/unknown/temporary conservative roots 有白盒覆盖。
- 已验证：词法 block cleanup-storage 对仍存活 carrier 的本地 loan 产生 cleanup conflict。
- 待 WP6/WP7 收口：更完整的 early-exit cleanup parity、diagnostics/tooling projection，以及显式 drop 语法或
  lowering 产生的 source-level drop action。

## 4. M7-WP4：BorrowSummary 与 Borrowed-Return Contract

当前状态：

- 已新增 `src/sema/internal/sema_borrow_summary.cpp` / `.hpp`，在函数体分析、return type inference、body
  flow 和 local loan check 之后生成 `CheckedModule::borrow_summaries`。
- 已新增 `CheckedModule::function_calls` direct call binding；普通函数、泛型函数、普通方法和泛型方法调用会记录
  `FunctionLookupKey`、return type 和 receiver-argument count，函数值调用保持 conservative unknown。
- `FunctionBorrowSummary` 记录 return type、parameter/local/temporary origins、return origin dependency set、
  unknown-return 标志、local/temporary escape 标志和 stable fingerprint。
- call wrapper 会把 callee summary 的 parameter origin dependency 映射到 caller 实参；callee 缺 summary、
  raw/unchecked pointer path、callee local/temporary return 都不会被当作安全证明，而是 conservative unknown。
- generic parameter 与 associated projection 在 M7a 内部按“可能含借用”处理，保证 `fn id[T](x: T) -> T`
  这类 wrapper 在 `T = &U` 时不会漏掉 parameter-origin dependency。
- `BorrowEscapeAnalyzer` 暂时继续负责旧 borrowed-local escape 诊断；summary 同步记录 local/temporary escape
  facts，等 parity 覆盖后再替换。

目标：

- 替代 `BorrowEscapeAnalyzer` 的 return escape 逻辑。
- 生成函数级 `BorrowSummary`。
- 支持参数派生 borrowed return 和 local/temporary 派生 borrowed return 拒绝。

建议实现：

- `FunctionBorrowSummary` 记录 parameter origin、return origin dependency set、receiver/argument access requirement。
- 当前模块内函数由 checker 生成 summary。
- extern/native/prototype 无 summary 且返回类型可含 borrow 时，走 conservative unknown。
- generic/trait method 暂按 checked body 或 trait requirement summary；缺 summary 时拒绝 risky borrowed return。
- inferred return 函数先收集 return carrier facts，再在 return type inference finalized 后固化 summary，或直接用 return
  expression/carrier type 生成 summary。

验收：

- 已验证：返回参数派生 `&T` 记录 parameter dependency。
- 已验证：call wrapper 能通过 direct call binding 传播 callee parameter dependency。
- 已验证：if branch 多参数来源返回记录 origin set，不被压成单一 origin。
- 已验证：raw pointer 派生 `str_from_bytes_unchecked` 只记录 unknown，不进入 safe borrowed proof。
- 已验证：返回 local 派生 borrow 时旧诊断仍触发，summary 记录 local escape fact。
- 已验证：summary fingerprint 和 stable dump 有白盒测试。

## 5. M7-WP5：Projection-Aware Conflict

当前状态：

- WP3 已支持 same/prefix conflict、known field disjoint 和 index/slice/unknown conservative roots。
- WP5 已补充 `BodyFlowActionKind::reinit`、`drop`、`cleanup_storage` 和对应 `BodyLoanConflictKind`。
- 整 local assignment 生成 `reinit`，field/index/deref assignment 继续生成 `write`，避免 reinit 与 projection write
  混淆。
- 词法 block exit 会为直接 local/pattern binding 生成 `cleanup_storage` action；loan checker 用 carrier liveness
  判断 cleanup 点是否仍有活跃 loan。
- `drop` 作为 checker matrix action 已支持；当前语言还没有显式 user drop syntax，因此 source-level drop emission
  仍属于后续语法/lowering 工作。

目标：

- 不再把所有 projection 都退化成 whole-local。
- 对 known disjoint struct/tuple field 放宽。
- 对 index/slice 保持保守，避免错判 disjoint。

建议实现：

- same place conflict。
- prefix place conflict。
- known struct/tuple field disjoint 可不冲突。
- array/slice/index 先同 root 保守冲突。
- safe reference deref 追踪 borrowed origin；raw pointer 不进入 safe proof。

验收：

- 已验证：`p.x` 与 `p.y` 可区分。
- 已验证：`p.x` 与 `p`、unknown/temporary conservative roots 冲突。
- 已验证：`a[i]` / slice / deref M7a 保守冲突，除非后续版本证明常量不同。
- 已验证：reinit/drop/cleanup action 与 active shared loan 的 conflict matrix。
- 已验证：raw pointer 派生 safe view 不进入 `BorrowSummary` 的 safe proof。

## 6. M7-WP6：Diagnostics、Query、Tooling

当前状态：

- 已把 `BorrowSummary` 与 `BodyLoanCheckResult` 纳入 `TypeCheckBodyAuthority`，type-check-body result
  fingerprint 现在混入 summary fingerprint、loan-check fingerprint、origin/dependency/loan/conflict count、
  unknown/local-escape/diagnostic-emitted 等状态位。
- CLI incremental-cache subject collection 和 IDE snapshot query collection 使用同一份 checked facts，不让
  tooling / LSP 重新跑 borrow sema。
- IDE semantic facts 新增 `borrow_summary` 与 `body_loan_check`，都挂在对应 `type_check_body` query 上；
  fact detail 暴露 dependency count、unknown/local_escape、loan/conflict count、diagnostic mode、conflict reason
  和 stable fingerprint。
- 函数 hover 在已有签名信息后追加 `borrow_summary=deps/unknown/local_escape` 摘要；仍不暴露 Rust-style
  lifetime surface，也不把 raw pointer aliasing 当成 safe proof。
- enforced borrow diagnostics 现在包含 primary conflict、loan creation note、invalidating action note，并在能从
  CFG/liveness 找到 carrier 后续使用时补充 later carrier use note；冲突点/range 级 cascade suppression 保持。
- `dump_checked_module` 现在输出 `body_loan_checks` summary 与 stable fingerprint，和专用
  `dump_body_loan_check_result(...)` 共用同一份 body-loan fingerprint。

目标：

- borrow checker 的结果进入 checked facts。
- IDE/LSP 消费同一份 facts，不重新跑语义。
- diagnostics 有 primary conflict、loan creation、later carrier use、invalidating action notes。

建议实现：

- 新增 borrow facts side table 或 checked module 子结构。
- 将 summary 纳入 incremental cache/query。
- IDE hover/diagnostic 可展示 borrow kind、origin 和 conflict reason。

验收：

- 已验证：负例 diagnostics 保持按冲突点/range 抑制级联，同时补齐 creation / invalidation / later-use notes。
- 已验证：`body_loan_check_fingerprint(...)` 对 conflict reason、later-use point 等语义事实敏感，
  但不混入绝对 source range，避免非语义注释/布局变更破坏 type-check-body query 复用；checked dump 仍暴露
  body loan facts 和诊断 range。
- 已验证：`TypeCheckBodyAuthority` 的 summary / loan fingerprint 改变会改变 type-check-body result fingerprint。
- 已验证：IDE semantic facts 暴露 `borrow_summary` / `body_loan_check`，函数 hover 能展示 summary dependency。
- 已验证：reuse / workspace index 认识新增 fact kind，并把它们归类为 body-local facts。

## 7. M7-WP7：Release Closure

当前状态：

- M7a release documentation 已写清 WP2-WP7 已支持的 internal fact model、local loan checking、
  borrowed-return summary、projection/drop/reinit/cleanup matrix、query/cache/tooling projection 和 diagnostics。
- `BorrowEscapeAnalyzer` 保留。M7a summary 已记录 borrowed-return facts，但在完整 borrowed-view escape parity
  覆盖前不移除或降级旧 analyzer。
- M7a 仍明确暂缓完整 Rust-style lifetime surface、full Polonius Datalog engine、raw pointer alias safe proof、
  user destructor syntax、partial move / replace / take / swap 完整 place-level resource semantics、`dyn Trait`、
  async drop 和 generator borrow。
- W7a release performance closure 已完成：普通 `--check` 不再长期保留 full body-flow graph；只有 checked/typed
  输出和 IDE/tooling 需要时保留 CFG facts。非借用返回函数的 `BorrowSummary` 只生成稳定空 return-dependency
  summary，不扫描完整函数体；call binding 查找由 `CheckedModule` 维护 expr-id index，避免每个函数重扫全局
  call binding list。

目标：

- 文档、样例、测试、coverage、query/perf/stress gates 收口。

验收：

- 已验证：release 全量 `ctest`、coverage gate、query sanitizer、perf/stress gates、format/diff gate 均通过。
- 已验证：默认 Release+LTO `perf-release-threshold` 通过；2M mixed AST lane 的 `sema.analyze` 保持秒级，
  不再出现 W7a body-flow/summary 引入后的 200s 级退化。
- release docs 已写清 M7a 已支持和仍暂缓的能力。
- 只有在新 checker parity 覆盖现有 borrowed-view 逃逸负例后，才移除或降级 `BorrowEscapeAnalyzer`。

## 8. M7 并行工程优化流

这些优化可以跟 M7 同期做，但必须和 borrow/lifetime 主线解耦：

- TargetInfo/DataLayout 贯通到 Sema、IR lowering、LLVM backend matrix。
- typed query route table。
- CompileBudget。
- ModuleIdentity。
- LSP protocol parser hardening。
- full TargetInfo/ModuleIdentity/CompileBudget matrix。

原则：

- 不把这些优化伪装成 borrow checker 的一部分。
- 先补白盒/边界测试，再改架构。
- 如果影响 checked facts/query identity，必须和 M7 summary key 一起审视。
