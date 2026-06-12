# Aurex M23b Builtin Derive Checkpoint Rollback Protocol

阶段：M23b Builtin Derive Parser Checkpoint Rollback Protocol
状态：已完成。

M23b 在 M23a admission protocol 之后，为每个 generated module part 增加
`BuiltinDeriveParserConsumptionCheckpointRollbackProtocol`。它把未来 parser consumption 尝试前需要的
checkpoint / rollback 设计协议固定成 facts，但仍不执行 parser、不执行 rollback、不修改 AST。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_checkpoint_rollback_protocols`。
- `BuiltinDeriveParserConsumptionCheckpointRollbackProtocol`。
- `is_valid(const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol&)`。
- `checkpoint_protocol_identity`。
- `EarlyItemExpansionSummary` 中的 builtin derive checkpoint rollback protocol 计数。

## 绑定关系

每个 checkpoint rollback protocol 绑定：

- generated part identity。
- M23a `admission_protocol_identity`。
- M22f `rollback_gate_identity`。
- `checkpoint_policy = builtin_derive_parser_checkpoint_rollback_protocol_v1`。
- query name：`m23b-builtin-derive-checkpoint-rollback:<module>:<part>`。

这样 M23b 只承认通过 M23a admission protocol 形成的 part-local candidate，不允许绕过 M22f rollback diagnostic
gate。

## 协议内容

M23b 固定：

- `checkpoint_count = 3`。
- `rollback_plan_count = 3`。
- `token_record_count = M23a token_record_count`。
- `diagnostic_anchor_count = M23a blocked_diagnostic_count`。
- `parser_state_checkpoint_available=true`。
- `token_cursor_checkpoint_available=true`。
- `generated_part_checkpoint_available=true`。
- `diagnostic_replay_available=true`。
- `rollback_protocol_complete=true`。

三类 checkpoint 表示 parser state、token cursor 和 generated module part 边界都必须具备回滚点；diagnostic replay
表示失败路径必须能回放到 M21k/M21l 的诊断锚点。

## 关闭边界

M23b 明确保持：

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

blocker 固定为
`builtin derive checkpoint rollback protocol remains design-only and parser-blocked in M23b`。

## 验证和调试面

M23b facts 已接入 summary、dump、fingerprint 和 `EarlyItemExpansionResult` validation。validation 会拒绝
checkpoint identity / query / count 漂移、M23a admission identity 串线、M22f rollback identity 串线、diagnostic
replay 被关闭、rollback execution 被打开、parser consumption 被打开、generated part parse / merge 被打开，或标准库 /
runtime / external process / user code flag 被打开。

## 仍不实现

M23b 仍不实现真实 parser checkpoint 对象、token cursor 回滚执行、diagnostic replay 执行、generated module part parse /
merge、用户宏执行、external procedural macro、标准库、runtime helper 或 generated source text。
