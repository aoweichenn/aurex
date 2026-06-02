# Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图

日期：2026-06-02

状态：M7 设计路线图。完整设计依据见
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)。

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

目标：

- 从 `BodyMoveAnalysis` 当前 CFG/worklist 构建逻辑中抽出共享 body flow skeleton。
- 让 move analysis 和 borrow analysis 使用同一条 expression/statement action timeline。
- 把隐式 cleanup/drop 点位纳入可检查 action。

建议实现：

- 新增 `SemaBodyFlowGraph`、`SemaBodyPoint`、`SemaBodyAction` 或等价内部模块。
- 保留现有 `BodyMoveAnalysis` 行为，先做结构抽取，不改变语义。
- 使用迭代式 task/worklist，不能引入递归遍历。

验收：

- 现有 move/resource/cleanup tests 全绿。
- `expr_owned_use_modes` side table 行为不变。
- CFG action dump 或白盒测试能证明 return/break/continue/`?`/defer cleanup 顺序稳定。

## 3. M7-WP3：Place/Origin/Loan 本地检查

目标：

- 引入 place/origin/loan ID 表。
- 支持本地 `&`、`&mut`、slice、safe `str` view 的 loan 发出与存活分析。
- 支持 shared/mutable loan 与 read/write/move/drop/reinit 的基本冲突。

建议实现：

- `Place` root 覆盖 local、parameter、temporary、global、unknown。
- `Origin` 覆盖 local、parameter、temporary、static/global、unknown。
- `Loan` 记录 issued point、kind、place、origin、carrier。
- active loans 使用 dense bitset 或 small sorted vector，按函数规模选择。

验收：

- 正例：borrow last-use 后允许 write/move。
- 负例：active shared loan 时 write/move/drop，active mutable loan 时任何冲突访问。
- cleanup/drop 隐式 action 能触发 borrow conflict。
- 不依赖 `BorrowEscapeAnalyzer` 新增特判。

## 4. M7-WP4：BorrowSummary 与 Borrowed-Return Contract

目标：

- 替代 `BorrowEscapeAnalyzer` 的 return escape 逻辑。
- 生成函数级 `BorrowSummary`。
- 支持参数派生 borrowed return 和 local/temporary 派生 borrowed return 拒绝。

建议实现：

- `FunctionBorrowSummary` 记录 parameter origin、return origin dependency、receiver/argument access requirement。
- 当前模块内函数由 checker 生成 summary。
- extern/native/prototype 无 summary 且返回类型可含 borrow 时，走 conservative unknown。
- generic/trait method 暂按 checked body 或 trait requirement summary；缺 summary 时拒绝 risky borrowed return。

验收：

- 正例：返回参数派生 `&T` / slice / `str`。
- 负例：返回 local/temporary 派生 `&T` / slice / `str`。
- call wrapper、method receiver、block/if/match return 的 origin 传播稳定。
- summary fingerprint/query key 有白盒测试。

## 5. M7-WP5：Projection-Aware Conflict

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

- `p.x` 与 `p.y` 可区分。
- `p.x` 与 `p`、`p.x.y` 与 `p.x` 冲突。
- `a[i]` 与 `a[j]` M7a 保守冲突，除非后续版本证明常量不同。
- raw pointer 派生 safe view 绕过必须拒绝。

## 6. M7-WP6：Diagnostics、Query、Tooling

目标：

- borrow checker 的结果进入 checked facts。
- IDE/LSP 消费同一份 facts，不重新跑语义。
- diagnostics 有 primary conflict、loan creation、later carrier use、invalidating action notes。

建议实现：

- 新增 borrow facts side table 或 checked module 子结构。
- 将 summary 纳入 incremental cache/query。
- IDE hover/diagnostic 可展示 borrow kind、origin 和 conflict reason。

验收：

- 负例 diagnostics 不级联刷屏。
- stale source/range 不崩溃。
- LSP projection 有 targeted tests。
- query/cache 复用和失效测试覆盖 summary change。

## 7. M7-WP7：Release Closure

目标：

- 文档、样例、测试、coverage、query/perf/stress gates 收口。

验收：

- `ctest` 全量通过。
- coverage gate 通过。
- query sanitizer / incremental cache tests 通过。
- release docs 写清 M7 已支持和仍暂缓的能力。

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
