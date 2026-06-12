# Aurex M22c Builtin Derive Parser Consumption Release Gate

阶段：M22c Builtin Derive Parser Consumption Release Gate
状态：已完成。

M22c 在 M22a admission facts 和 M22b semantic plan facts 之后，为每个 generated module part 增加
`BuiltinDeriveParserConsumptionReleaseGate`，把 builtin derive parser consumption 的释放条件收束到 part 级
release gate。当前 release gate 仍保持关闭，不让 parser 消费 generated token buffer。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_parser_release_gates`。
- `BuiltinDeriveParserConsumptionReleaseGate`。
- `is_valid(const BuiltinDeriveParserConsumptionReleaseGate&)`。
- `admission_group_identity`。
- `semantic_plan_group_identity`。
- `release_gate_identity`。
- `EarlyItemExpansionSummary` 中的 parser release gate 计数。

## 聚合内容

每个 release gate 绑定：

- generated part identity。
- M21n `contract_identity`。
- M21o `closure_identity`。
- M22a admission group identity。
- M22b semantic plan group identity。
- `release_policy = builtin_derive_parser_consumption_release_gate_v1`。
- query name：`m22c-builtin-derive-parser-release:<module>:<part>`。

release gate 汇总：

- `admission_count`。
- `derive_admission_count`。
- `semantic_plan_count`。
- `capability_total_count`。
- `parser_consumable_contract_count`，当前必须为 0。

## 释放前置条件

M22c 已把未来打开 parser consumption 前必须可审计的前置条件显式化：

- `rollback_diagnostics_available=true`。
- `debug_trace_prerequisite_available=true`。
- `source_map_prerequisite_available=true`。
- `hygiene_prerequisite_available=true`。

这些字段只表示 release gate 已经知道后续必须检查这些前置条件，不表示真实 debug trace、source map 或 hygiene
resolution 已经实现。

## 关闭边界

M22c 明确保持：

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

blocker 固定为 `builtin derive parser consumption release remains blocked in M22c`。

## 当前 result baseline

M22c 把 `EarlyItemExpansionResult::name` 推进为
`M22c Builtin Derive Parser Consumption Release Gate`，并把 fingerprint marker 推进为
`frontend.macro.m22c.builtin_derive_parser_consumption_release_gate.v1`。M21o boundary closure 仍保留在结果中，
作为 M22a/M22b/M22c 的上游事实。
