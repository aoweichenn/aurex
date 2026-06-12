# Aurex M26b Builtin Derive Error Recovery Shadow Diagnostic Gate

阶段：M26b Builtin Derive Error Recovery Shadow Diagnostic Gate
状态：已完成。

M26b 在 M26a parser dry-run admission gate 之后，为每个 generated module part 增加
`BuiltinDeriveErrorRecoveryShadowDiagnosticGate`。它把 future parser dry-run 的 error recovery 计划和 shadow
diagnostic 输出拆成独立门禁事实：当前只记录 recovery / diagnostic 的可追踪闭环，不执行 recovery，不发出诊断，
也不允许 parser 或 AST 进入任何可变状态。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_error_recovery_shadow_diagnostic_gates`。
- `BuiltinDeriveErrorRecoveryShadowDiagnosticGate`。
- `is_valid(const BuiltinDeriveErrorRecoveryShadowDiagnosticGate&)`。
- `recovery_shadow_identity`。
- `EarlyItemExpansionSummary` 中的 error recovery shadow diagnostic gate 计数。

## 绑定关系

每个 recovery shadow diagnostic gate 绑定：

- generated part identity。
- M26a `admission_gate_identity`。
- M25c `diagnostic_shadow_closure_identity`。
- M24b `replay_protocol_identity`。
- M21l `report_identity`。
- `recovery_policy = builtin_derive_error_recovery_shadow_diagnostic_gate_v1`。
- query name：`m26b-builtin-derive-error-recovery-shadow-diagnostic:<module>:<part>`。

group validation 会要求 M26b 与同一 generated part 上的 M26a admission gate、M25c diagnostic shadow closure、
M24b rollback diagnostic replay 和 M21l parser admission report 完全一致。任何跨 part identity 串线、query 漂移、
report 漂移或上游 identity 漂移都会被拒绝。

## Shadow Diagnostic Gate 内容

M26b 固定：

- `diagnostic_shadow_count = <M25c diagnostic_shadow_count>`。
- `report_entry_count = <M21l report.entry_count>`。
- `planned_recovery_count = <M21l report.blocked_entry_count>`。
- `executed_recovery_count = 0`。
- `emitted_diagnostic_count = 0`。
- `dry_run_admission_gate_available=true`。
- `diagnostic_shadow_closure_available=true`。
- `rollback_replay_available=true`。
- `parser_report_available=true`。
- `recovery_shadow_plan_available=true`。
- `recovery_shadow_complete=true`。

这里的 `planned_recovery_count` 只描述“如果 future parser dry-run 被准入，哪些 blocked report entry 需要 recovery
策略覆盖”。它不是执行计数，也不是用户可见诊断数量。

## 当前 result baseline

M26b 是 M26a-M26c 三段闭环中的第二段。当前仓库最终 baseline 已推进到：

- result name：`M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure`。
- fingerprint marker：`frontend.macro.m26c.builtin_derive_cursor_rollback_ast_mutation_verifier_closure.v1`。

## 关闭边界

M26b 明确保持：

- `recovery_execution_enabled=false`。
- `diagnostic_emission_enabled=false`。
- `dry_run_execution_admitted=false`。
- `dry_run_executed=false`。
- `rollback_execution_enabled=false`。
- `session_committed=false`。
- `parser_cursor_advanced=false`。
- `parser_consumption_enabled=false`。
- `parser_admitted=false`。
- `generated_part_parsed=false`。
- `generated_part_merged=false`。
- `ast_mutated=false`。
- `sema_visible=false`。
- `emit_expanded_available=false`。
- `debug_trace_available=false`。
- `source_map_available=false`。
- `standard_library_required=false`。
- `runtime_required=false`。
- `external_process_required=false`。
- `produced_user_generated_code=false`。

blocker 固定为
`builtin derive error recovery shadow diagnostics remain non-emitting in M26b`。

## 验证和调试面

M26b facts 已接入：

- summary：`builtin_derive_error_recovery_shadow_diagnostic_gates`、
  `builtin_derive_error_recovery_shadow_diagnostic_gate_visible`、
  `builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable`、
  `builtin_derive_error_recovery_shadow_diagnostic_gate_complete`、
  `builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed`、
  `builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted`、
  `builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable`。
- dump：`builtin_derive_error_recovery_shadow_diagnostic_gate` section。
- fingerprint：recovery shadow identity、admission gate identity、diagnostic closure identity、rollback replay identity、
  parser report identity、policy、query、blocked reason、planned/executed/emitted counts 或关闭边界变化都会改变
  fingerprint。
- validation：拒绝 M26a/M25c/M24b/M21l identity 串线、query 漂移、recovery shadow incomplete、recovery execution
  被打开、diagnostic emission 被打开、dry-run execution 被打开、rollback execution 被打开、parser cursor advance
  被打开、parser admission / parser consumption 被打开、generated part parse / merge / sema-visible 被打开、
  AST mutation、标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M26b 仍不实现真实 parser dry-run、真实 parser recovery、用户可见 diagnostic emission、rollback execution、parser
cursor advance、parser token consumption、session commit、generated source text、generated module part parse / merge、
AST mutation、sema-visible macro output、真实 source map、debug trace CLI、`--emit-expanded`、用户自定义 derive、
用户自定义 macro、external procedural macro、typed expression macro、文本替换宏、标准库或 runtime helper。
