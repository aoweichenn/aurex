# Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 路线图

日期：2026-06-03

状态：实现收口。完整设计依据见
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线](m7b-borrow-contract-design.md)。

## 0. M7b 总目标

M7b 的目标是在 M7a CFG-sensitive borrow facts 之上，把内部 `BorrowSummary` 提升为可声明、可比较、可缓存、可投影的
函数边界 contract，并用新 checker 覆盖旧 `BorrowEscapeAnalyzer`。

M7b 核心产物：

- `FunctionBorrowContract` checked fact。
- `@borrow(return = [...])` 窄 surface。
- summary-vs-contract subset checker。
- trait requirement / impl borrow contract matching。
- reborrow parent/child loan model。
- method receiver access contract。
- receiver auto-borrow two-phase reservation/activation。
- `BorrowEscapeAnalyzer` parity replacement。
- diagnostics、query/cache、IDE/tooling 和 release gate 收口。

当前已落地：

- 函数装饰器式 `@borrow(return = [...])` parser/AST/sema，以及 declared/inferred
  `FunctionBorrowContract` fingerprint/dump/query 投影。
- summary-vs-contract subset enforcement，trait requirement / impl borrowed-return contract matching。
- `BodyLoan::parent_loan` reborrow parent/child model；child loan 的有效 place 会归一到 parent 底层 place，
  保留 known-disjoint field projection 放宽，并诊断 child 活跃期间的 mutable parent use。
- method / trait method call binding 记录 `receiver_access`、`receiver_auto_borrow` 和
  `receiver_two_phase_eligible`。
- receiver auto-borrow two-phase reservation/activation facts；reservation 期间允许 shared read，拒绝写、
  move、drop、cleanup、mutable borrow 和 nested mutable receiver reservation；activation 点执行 active loan conflict
  check。
- `TypeCheckBodyAuthority` 混入 borrow contract fingerprint、body loan fingerprint、reborrow/two-phase counts 和
  diagnostics-emitted 状态；IDE semantic fact detail 展示 `borrow_contract`、`body_loan_check`、reborrow/two-phase
  计数。

M7b 不做：

- full Rust-style lifetime generic surface。
- full Polonius Datalog engine。
- raw pointer alias safe proof。
- partial move / replace / take / swap 完整 place-level resource semantics。
- `dyn Trait`、async drop、generator borrow。
- unsafe optimizer alias model。

## 1. M7b-WP1：Parity Matrix 与 Contract Fact Scaffold

目标：

- 建立 `BorrowEscapeAnalyzer` 当前负例矩阵。
- 新增 `FunctionBorrowContract` 内部 fact scaffold。
- 不改变现有 diagnostics 行为。

建议实现：

- 新增 `sema_borrow_contract.cpp` / `.hpp`。
- 在 `CheckedModule` 增加 `borrow_contracts` map。
- 从 existing `FunctionBorrowSummary` 自动生成 inferred contract。
- 白盒 dump 输出 contract，但不参与 enforcement。
- 建立旧 analyzer parity 测试表：local/temporary/slice/str/raw/assignment/pattern/branch escape。

验收：

- 旧诊断行为不变。
- contract dump/fingerprint 稳定。
- parity matrix 有 shadow baseline。
- release CTest、coverage、query sanitizer 通过。

## 2. M7b-WP2：`@borrow(...)` Parser/AST/Sema Contract

目标：

- 支持窄语法 `@borrow(return = [param, self])`。
- 让 extern/prototype/trait requirement 能声明 borrowed-return contract。

建议实现：

- 复用现有 `@name(...)` 函数声明前装饰器位置，不新增 keyword。
- AST 增加结构化 `BorrowContractDecl`。
- parser 只接受 `return = [identifier | self | static | unknown, ...]` 的第一版 grammar。
- 对返回类型不能含 borrow 的函数报告冗余 contract。
- 对 public/prototype/extern borrowed-return 缺 contract 的场景给出明确诊断或 conservative unknown fact。

验收：

- AST dump / lossless parse 覆盖 `@borrow`。
- 正例：parameter/self/static contract。
- 负例：unknown name、local name、重复 selector、非 borrowed return 的 contract。
- query signature fingerprint 因 declared contract 改变而改变。

## 3. M7b-WP3：Summary-vs-Contract Enforcement

目标：

- 对有 body 的函数检查 inferred summary 是否满足 declared contract。
- 用新 checker 接管 borrowed-local return escape 的主诊断。

建议实现：

- 将 inferred return origin set 映射到 function-boundary selectors。
- local/temporary/raw/unknown 超出 declared safe contract 时报错。
- private 函数可继续完全推导；public API 推荐显式或导出 inferred safe contract。
- diagnostics 给出 declared contract、实际返回来源和 return expression。

验收：

- 返回参数派生 borrow 通过。
- 返回 local/temporary 派生 borrow 被新 checker 拒绝。
- branch/match 多 origin set 与 contract subset 正确比较。
- `BorrowEscapeAnalyzer` 进入 shadow-only 或完全被覆盖，具体取决于 parity 测试结果。

## 4. M7b-WP4：Trait / Generic Borrow Contract

目标：

- trait requirement 能声明 borrowed-return contract。
- impl method contract 必须是 requirement contract 的子集。
- generic wrapper 不丢失 borrowed dependency。

建议实现：

- `TraitMethodRequirement` 记录 borrow contract。
- impl matching 增加 contract subset check。
- inherited default body 的 inferred contract 也要满足 requirement。
- generic `T` 可能含 borrow 时继续保守追踪 parameter dependency。
- 缺 summary 的 trait/generic/extern call 不作为 safe proof。

验收：

- trait method `@borrow(return = [self]) fn view(self: &Self) -> str;` 正例。
- impl 返回其他参数/local/unknown 时拒绝。
- generic identity/wrapper summary 保持 dependency。
- associated projection 仍按可能含 borrow 处理。

## 5. M7b-WP5：Reborrow Parent/Child Loan

目标：

- 支持 safe reference reborrow。
- mutable parent 在 child 活跃期间 suspend，child last-use 后恢复。

建议实现：

- `BodyLoan` 增加 optional parent loan 或新增 `BodyReborrow` side table。
- solver 计算 child live range 后投影 parent suspended interval。
- conflict matrix 增加 parent-use-while-child-active 诊断。
- raw pointer reborrow 不进入 safe proof。

验收：

- shared-from-shared reborrow 正例。
- shared-from-mutable reborrow 期间 parent write 拒绝，child last-use 后允许。
- mutable-from-mutable reborrow 期间 parent read/write/reborrow 拒绝。
- field projection reborrow 仍遵守 known-disjoint / prefix conflict。

## 6. M7b-WP6：Method Receiver Access 与 Two-phase Borrow

目标：

- method receiver access contract 进入 call binding 和 borrow facts。
- 只为 receiver auto-borrow 支持 two-phase reservation/activation。

建议实现：

- 从 `self: &T` / `self: &mut T` / `self: T` 推导 receiver access。
- method call binding 记录 receiver_arg_count、receiver access 和 two-phase eligibility。
- reservation point 在 receiver evaluate 后，activation point 在 callee call 前。
- activation 时执行完整 mutable conflict check。

验收：

- `values.push(values.len())` 这类 receiver auto-borrow 正例。
- 显式 `&mut values` 不自动 two-phase。
- reservation 后写/move/drop receiver place 报错。
- activation conflict diagnostic 带 reservation 和 argument evaluation note。

## 7. M7b-WP7：Diagnostics、Query、Tooling、Release Closure

目标：

- contract / reborrow / two-phase facts 进入 query/cache/tooling。
- release gates 收口。

建议实现：

- `TypeCheckBodyAuthority` 混入 borrow contract fingerprint 和 reborrow/two-phase counts。
- IDE semantic facts 增加 `borrow_contract` 和可选 `reborrow` / `two_phase_borrow` detail。
- hover 展示 declared/inferred contract 的摘要。
- `dump_checked_module` 输出 `borrow_contracts` 和 reborrow summary。

验收：

- release 全量 `ctest` 通过。
- coverage gate 保持 >= 95%。
- query sanitizer 和 query graph fuzz 通过。
- perf-stress / perf-release threshold 通过。
- docs/version/progress/next-steps 全部更新。
- `BorrowEscapeAnalyzer` 删除、降级或以明确 shadow-only 状态记录在文档中。

## 8. 后续 M7c/M8 候选

M7b 完成后再评估：

- full lifetime generic surface。
- unsafe/raw alias model。
- partial move / replace / take / swap place-level resource semantics。
- `dyn Trait` 与 object lifetime bound。
- async/generator borrow。
- full Polonius-style engine。
