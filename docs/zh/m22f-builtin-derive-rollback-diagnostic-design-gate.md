# Aurex M22f Builtin Derive Rollback Diagnostic Design Gate

阶段：M22f Builtin Derive Rollback Diagnostic Design Gate
状态：已完成。

M22f 在 M22e debug dump stability contract 之后，为每个 generated module part 增加
`BuiltinDeriveRollbackDiagnosticDesignGate`。它把未来打开 parser consumption 时需要的失败回滚诊断设计门禁固化为
事实，但仍不执行回滚、不 parse generated part、不 merge generated part。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_rollback_diagnostic_gates`。
- `BuiltinDeriveRollbackDiagnosticDesignGate`。
- `is_valid(const BuiltinDeriveRollbackDiagnosticDesignGate&)`。
- `rollback_gate_identity`。
- `EarlyItemExpansionSummary` 中的 builtin derive rollback diagnostic gate 计数。

## 聚合内容

每个 rollback diagnostic design gate 绑定：

- generated part identity。
- M21n `parser_consumption_contract_identity`。
- M22c `release_gate_identity`。
- M22d `hardening_matrix_identity`。
- M22e `debug_dump_contract_identity`。
- `rollback_policy = builtin_derive_rollback_diagnostic_design_gate_v1`。
- query name：`m22f-builtin-derive-rollback-diagnostic:<module>:<part>`。

gate 会按 generated part 记录：

- `diagnostic_projection_count`。
- `diagnostic_report_entry_count`。
- `blocked_diagnostic_count`。
- `derive_diagnostic_count`。
- `empty_diagnostic_count`。
- `parser_consumption_contract_count`，当前必须为 1。

这些计数直接绑定 M21k diagnostic projection、M21l report entry 和 M21n contract gate，用于保证将来 parser
consumption 如果打开，失败路径可以精确回滚到对应 source anchor / token-tree anchor / debug dump contract。

## 设计门禁

M22f 固定：

- `rollback_diagnostic_design_available=true`。
- `diagnostic_grouping_available=true`。
- `source_anchor_available=true`。
- `token_tree_anchor_available=true`。
- `debug_dump_contract_available=true`。
- `release_rollback_plan_complete=true`。

这些字段只表示设计门禁已存在，不表示 rollback execution 已实现。

## 关闭边界

M22f 明确保持：

- `rollback_execution_enabled=false`。
- `parser_consumption_enabled=false`。
- `generated_part_parsed=false`。
- `generated_part_merged=false`。
- `emit_expanded_available=false`。
- `debug_trace_available=false`。
- `source_map_available=false`。
- `standard_library_required=false`。
- `runtime_required=false`。
- `external_process_required=false`。
- `produced_user_generated_code=false`。

blocker 固定为 `builtin derive rollback diagnostics remain design-only and parser-blocked in M22f`。

## 当前 result baseline

M22f 把 `EarlyItemExpansionResult::name` 推进为
`M22f Builtin Derive Rollback Diagnostic Design Gate`，并把 fingerprint marker 推进为
`frontend.macro.m22f.builtin_derive_rollback_diagnostic_design_gate.v1`。M22a-M22e facts 仍保留在结果中，作为
M22f rollback diagnostic design gate 的上游事实。
