# Aurex M21o Macro Expansion Boundary Release Closure

阶段：M21o Macro Expansion Boundary Release Closure

## 目标

M21o 是当前 M21 宏系统主线的边界收口阶段。它把 M21m generated token parser readiness preflight 和
M21n parser consumption contract gate 汇总到一个 release closure report，并把 `EarlyItemExpansionResult`
的对外阶段名推进为：

```text
M21o Macro Expansion Boundary Release Closure
```

M21o 的核心目标不是打开宏执行，而是把当前能够安全承诺的 frontend macro boundary 固定下来：

- attribute token-tree surface 已进入 parser/AST。
- early item expansion 已进入真实 frontend pipeline。
- generated module part placeholder / parse-merge stub 已有稳定 identities。
- hygiene/source-map/debug trace 仍是 stub，但已经可查询、可校验。
- generated item declared names 仍不参与 lookup/sema/export，但已有 deterministic facts。
- compiler-owned generated token buffer prototype 已可被记录、审计、dump。
- parser admission gate / diagnostic / report / readiness preflight / consumption contract 已全链路可追踪。
- parser consumption、AST mutation、generated source text 和 user codegen 仍明确关闭。

## 新增 API

`EarlyItemExpansionResult` 新增：

```cpp
std::vector<MacroExpansionBoundaryClosureReport> macro_boundary_closure_reports;
```

当前 result 会生成一个 `MacroExpansionBoundaryClosureReport`。report 绑定：

- `closure_identity`。
- `closure_grouping_identity`。
- `closure_policy = m21_macro_expansion_boundary_release_closure_v1`。
- `closure_query_name = m21o-macro-boundary-closure`。
- `blocked_reason = M21 macro expansion boundary remains parser-blocked after M21o closure`。

## Closure 内容

M21o closure report 汇总：

- `macro_input_count`。
- `generated_part_count`。
- `parser_admission_report_count`。
- `parser_readiness_preflight_entry_count`。
- `parser_consumption_contract_gate_count`。
- `blocked_contract_gate_count`。
- `parser_consumable_contract_gate_count`。

并固定：

- `m21m_preflight_available = true`。
- `m21n_contract_available = true`。
- `release_closure_complete = true`。
- `query_reusable = true`。
- `closure_visible = true`。

## 当前能做什么

M21o 当前能做到：

- 对所有 parsed item attributes 建立 no-op early macro expansion facts。
- 对 builtin `derive` 输入生成 compiler-owned token record prototype。
- 对非 `derive` item attribute 保持 empty generated token buffer，并明确阻塞。
- 对 parser admission blocker 生成 diagnostic、report、preflight 和 contract gate。
- 在 summary / dump / fingerprint / validation 中暴露完整阶段状态。
- 拒绝任何把当前阶段伪装成已生成用户代码、已 parse generated part、已打开 parser consumption、已支持
  source map / debug trace / emit-expanded 的漂移。

## 仍不实现

M21o 仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

这些不是遗漏，而是 M21 release closure 的边界。M21 当前只承诺 frontend macro boundary 的事实收集、
query/cache/debug surface 和 negative validation matrix。

## Validation

M21o validation 会拒绝：

- 缺失 closure report。
- `closure_identity` 或 `closure_grouping_identity` 漂移。
- closure counts 与 result 内 facts 不一致。
- `m21m_preflight_available`、`m21n_contract_available` 或 `release_closure_complete` 被关闭。
- `parser_consumption_enabled` 被打开。
- `standard_library_required`、`runtime_required`、`external_process_required` 被打开。
- `emit_expanded_available`、`debug_trace_available`、`source_map_available` 被打开。
- `produced_user_generated_code` 被打开。

## 测试

当前测试覆盖：

- M21o result name、summary、dump 和 fingerprint。
- `MacroExpansionBoundaryClosureReport` positive facts。
- closure identity、counts、release-complete、parser-consumption-enabled、standard-library/runtime/external-process
  和 user-code negative matrix。
- 与 M21m/M21n 的联动：preflight / contract gate 的 identity 或 totals 漂移会同时使 result validation 失败。

## 后续方向

M21o 结束后，下一条宏系统主线应该进入“真实宏执行 admission 设计”，但仍应分层推进：

- 先设计 builtin derive expansion admission。
- 再决定 generated token parser consumption 的最小可打开条件。
- 再设计 hygiene/source-map/debug trace 的真实实现。
- 最后再考虑 external procedural macro、用户自定义 derive、`--emit-expanded` 和更广义的 codegen。

标准库和 runtime helper 仍不属于当前阶段。
