# Aurex M24b Builtin Derive Dry-Run Rollback Diagnostic Replay

阶段：M24b Builtin Derive Dry-Run Rollback Diagnostic Replay
状态：已完成。

M24b 在 M24a controlled dry-run adapter 之后，为每个 generated module part 增加
`BuiltinDeriveDryRunRollbackDiagnosticReplay`。它描述 dry-run 失败时应如何复用 M22f/M23b 已建立的 diagnostic
projection、checkpoint 和 rollback plan，但仍不执行 replay、不执行 rollback，也不调用 parser。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_dry_run_rollback_replays`。
- `BuiltinDeriveDryRunRollbackDiagnosticReplay`。
- `is_valid(const BuiltinDeriveDryRunRollbackDiagnosticReplay&)`。
- `replay_protocol_identity`。
- `EarlyItemExpansionSummary` 中的 dry-run rollback replay 计数。

## 绑定关系

每个 replay protocol 绑定：

- generated part identity。
- M24a `dry_run_adapter_identity`。
- M23b `checkpoint_protocol_identity`。
- M22f `rollback_gate_identity`。
- `replay_policy = builtin_derive_dry_run_rollback_diagnostic_replay_v1`。
- query name：`m24b-builtin-derive-dry-run-rollback-replay:<module>:<part>`。

group validation 要求 M24b 与同一 generated part 上的 M24a adapter、M23b checkpoint rollback protocol 和 M22f
rollback diagnostic design gate 一一对应。

## Replay 内容

M24b 固定：

- `diagnostic_anchor_count = <M24a diagnostic_anchor_count>`。
- `report_entry_count = <M22f diagnostic_report_entry_count>`。
- `planned_replay_count = diagnostic_anchor_count`。
- `executed_replay_count = 0`。
- `dry_run_adapter_available=true`。
- `checkpoint_protocol_available=true`。
- `rollback_gate_available=true`。
- `diagnostic_replay_plan_available=true`。
- `replay_protocol_complete=true`。

这表示 dry-run 失败诊断已经有确定的 query projection 和 rollback replay plan，但执行仍被硬关闭。

## 当前 result baseline

M24b 是 M24a-M24c 三段闭环中的第二段。当前仓库最终 baseline 已推进到：

- result name：`M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure`。
- fingerprint marker：`frontend.macro.m25c.builtin_derive_diagnostic_shadow_no_ast_mutation_closure.v1`。

## 关闭边界

M24b 明确保持：

- `replay_execution_enabled=false`。
- `dry_run_executed=false`。
- `parser_consumption_enabled=false`。
- `generated_part_parsed=false`。
- `generated_part_merged=false`。
- `sema_visible=false`。
- `emit_expanded_available=false`。
- `debug_trace_available=false`。
- `source_map_available=false`。
- `standard_library_required=false`。
- `runtime_required=false`。
- `external_process_required=false`。
- `produced_user_generated_code=false`。

blocker 固定为
`builtin derive dry-run rollback diagnostic replay remains execution-blocked in M24b`。

## 验证和调试面

M24b facts 已接入：

- summary：`builtin_derive_dry_run_rollback_replays`、
  `builtin_derive_dry_run_rollback_replay_complete`、
  `builtin_derive_dry_run_rollback_replay_executed`。
- dump：`builtin_derive_dry_run_rollback_replay` section。
- fingerprint：replay identity、query、planned / executed replay count、policy、blocked reason 或关闭边界变化都会
  改变 fingerprint。
- validation：拒绝 M24a/M23b/M22f identity 串线、query 漂移、protocol incomplete、replay execution 被打开、
  dry-run execution 被打开、parser consumption 被打开、generated part parse / merge / sema-visible 被打开、
  标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M24b 仍不实现真实 parser dry-run、rollback execution、diagnostic replay execution、parser token consumption、
generated source text、generated module part parse / merge、AST mutation、sema-visible macro output、用户自定义
derive、external procedural macro、typed expression macro、标准库或 runtime helper。
