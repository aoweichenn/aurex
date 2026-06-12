# Aurex M23a Builtin Derive Parser Consumption Admission Protocol

阶段：M23a Builtin Derive Parser Consumption Admission Protocol
状态：已完成。

M23a 在 M22f rollback diagnostic design gate 之后，为每个 generated module part 增加
`BuiltinDeriveParserConsumptionAdmissionProtocol`。它不是 parser consumption 的实现，而是把未来允许 builtin
derive generated token buffer 进入 parser 前必须满足的 admission 协议固定为可验证 facts。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_parser_consumption_admission_protocols`。
- `BuiltinDeriveParserConsumptionAdmissionProtocol`。
- `is_valid(const BuiltinDeriveParserConsumptionAdmissionProtocol&)`。
- `admission_protocol_identity`。
- `EarlyItemExpansionSummary` 中的 builtin derive parser consumption admission protocol 计数。

## 绑定关系

每个 admission protocol 绑定同一个 generated module part 上的：

- generated part identity。
- M21n `parser_consumption_contract_identity`。
- M22c `release_gate_identity`。
- M22f `rollback_gate_identity`。
- `admission_policy = builtin_derive_parser_consumption_admission_protocol_v1`。
- query name：`m23a-builtin-derive-parser-consumption-admission:<module>:<part>`。

这些 identity 让后续阶段不能跳过 M21 parser consumption contract、M22 parser release gate 或 M22f rollback
diagnostic design gate。

## 记录内容

M23a 按 generated module part 记录：

- `token_buffer_count`。
- `token_record_count`。
- `derive_candidate_count`。
- `empty_candidate_count`。
- `blocked_diagnostic_count`。

当前不变量要求：

- `token_buffer_count == derive_candidate_count + empty_candidate_count`。
- `blocked_diagnostic_count == token_buffer_count`。
- release gate、rollback gate、parser contract、deterministic ordering 和 generated token checkpoint 都必须可见。

## 关闭边界

M23a 明确保持：

- `parser_consumption_enabled=false`。
- `parser_admitted=false`。
- `generated_part_parsed=false`。
- `generated_part_merged=false`。
- `emit_expanded_available=false`。
- `debug_trace_available=false`。
- `source_map_available=false`。
- `standard_library_required=false`。
- `runtime_required=false`。
- `external_process_required=false`。
- `produced_user_generated_code=false`。

blocker 固定为
`builtin derive parser consumption admission protocol remains no-parser-consumption in M23a`。

## 验证和调试面

M23a facts 已接入：

- `summarize_early_item_expansion_counts()`。
- `summarize_early_item_expansion()`。
- `dump_early_item_expansion()`。
- `early_item_expansion_fingerprint()`。
- `is_valid(const EarlyItemExpansionResult&)`。

validation 会拒绝 admission identity / query / count 漂移、上游 identity 串线、parser admission 被打开、
parser consumption 被打开、generated part parse / merge 被打开、标准库 / runtime / external process / user code
flag 被打开。

## 仍不实现

M23a 仍不实现 parser consumption、generated source text、generated module part parse / merge、真实 generated item
materialization、用户自定义 derive、external procedural macro、typed expression macro、标准库、runtime helper、
debug trace CLI、真实 source map 或 `--emit-expanded`。
