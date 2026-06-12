# Aurex M22e Builtin Derive Debug Dump Stability Contract

阶段：M22e Builtin Derive Debug Dump Stability Contract
状态：已完成。

M22e 在 M22d hardening matrix 之后，为每个 generated module part 增加
`BuiltinDeriveDebugDumpStabilityContract`。它把 builtin derive admission / semantic plan / parser release gate /
hardening matrix 的 dump 投影稳定性固化成事实，确保后续打开 parser consumption 之前，开发者能用稳定 query 和
dump 定位 release gate 漂移。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_debug_dump_contracts`。
- `BuiltinDeriveDebugDumpStabilityContract`。
- `is_valid(const BuiltinDeriveDebugDumpStabilityContract&)`。
- `debug_dump_contract_identity`。
- `EarlyItemExpansionSummary` 中的 builtin derive debug dump contract 计数。

## 聚合内容

每个 debug dump contract 绑定：

- generated part identity。
- M22c `release_gate_identity`。
- M22d `hardening_matrix_identity`。
- `debug_dump_policy = builtin_derive_debug_dump_stability_contract_v1`。
- query name：`m22e-builtin-derive-debug-dump:<module>:<part>`。

当前 `dump_section_count=4`，对应：

- M22a builtin derive expansion admission facts。
- M22b builtin derive semantic expansion plan facts。
- M22c builtin derive parser release gate facts。
- M22d builtin derive release hardening matrix facts。

## 稳定性契约

M22e 固定：

- `stable_ordering_available=true`。
- `identity_projection_available=true`。
- `summary_projection_available=true`。
- `drift_debuggable=true`。
- `debug_dump_contract_complete=true`。

这些字段只表示当前 facts 的 summary / dump / identity 投影已经稳定，不表示真实宏展开 trace、真实 source map 或
`--emit-expanded` 已经实现。

## 关闭边界

M22e 明确保持：

- `emit_expanded_available=false`。
- `debug_trace_available=false`。
- `source_map_available=false`。
- `parser_consumption_enabled=false`。
- `standard_library_required=false`。
- `runtime_required=false`。
- `external_process_required=false`。
- `produced_user_generated_code=false`。

blocker 固定为 `builtin derive debug dump stability remains facts-only and parser-blocked in M22e`。

## 当前 result baseline

M22e 只追加 debug dump stability facts；不生成用户代码，不让 parser 消费 generated token buffer。`EarlyItemExpansionResult`
最终 baseline 由 M22f 推进，M22e facts 会进入 summary、dump、fingerprint 和 validation。
