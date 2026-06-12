# Aurex M22d Builtin Derive Release Hardening Matrix

阶段：M22d Builtin Derive Release Hardening Matrix
状态：已完成。

M22d 在 M22c `BuiltinDeriveParserConsumptionReleaseGate` 之后，为每个 generated module part 增加
`BuiltinDeriveReleaseHardeningMatrix`。它不是打开 parser consumption，而是把 release gate 在跨 part、多 item、
derive / non-derive 混合场景下必须保持 part-local 的负例矩阵固化为可验证事实。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_release_hardening_matrices`。
- `BuiltinDeriveReleaseHardeningMatrix`。
- `is_valid(const BuiltinDeriveReleaseHardeningMatrix&)`。
- `hardening_matrix_identity`。
- `EarlyItemExpansionSummary` 中的 builtin derive release hardening 计数。

## 聚合内容

每个 hardening matrix 绑定：

- generated part identity。
- M22c `release_gate_identity`。
- M22c `admission_group_identity`。
- M22c `semantic_plan_group_identity`。
- `hardening_policy = builtin_derive_release_hardening_matrix_v1`。
- query name：`m22d-builtin-derive-release-hardening:<module>:<part>`。

matrix 会同时记录：

- `part_local_admission_count`。
- `part_local_derive_admission_count`。
- `part_local_semantic_plan_count`。
- `part_local_release_gate_count`，当前必须为 1。
- `global_admission_count`。
- `global_semantic_plan_count`。
- `global_generated_part_count`。
- `cross_part_admission_count`。
- `cross_part_semantic_plan_count`。

这些计数用于捕捉错误实现：例如 part 0 的 release hardening fact 不得把 part 1 的 admission / semantic plan 当成
part-local 输入；跨 part 输入只能反映在 cross-part totals 中。

## 关闭边界

M22d 明确保持：

- `part_locality_preserved=true`。
- `multi_item_matrix_available=true`。
- `negative_matrix_complete=true`。
- `release_remains_blocked=true`。
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

blocker 固定为 `builtin derive release hardening matrix keeps parser consumption blocked in M22d`。

## 当前 result baseline

M22d 自身不改变 M22c release gate 的语义；它只追加 part-level release hardening facts。`EarlyItemExpansionResult`
最终 baseline 由 M22f 推进，M22d facts 会进入 summary、dump、fingerprint 和 validation。
