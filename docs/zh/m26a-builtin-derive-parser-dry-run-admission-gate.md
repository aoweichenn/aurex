# Aurex M26a Builtin Derive Parser Dry-Run Admission Gate

阶段：M26a Builtin Derive Parser Dry-Run Admission Gate
状态：已完成。

M26a 在 M25c diagnostic shadow no-AST-mutation closure 之后，为每个 generated module part 增加
`BuiltinDeriveParserDryRunAdmissionGate`。它把“真实 parser dry-run execution 是否可以被准入”拆成独立、
可查询、可 fingerprint、可验证的门禁事实。当前门禁只做 admission proof，不执行 dry-run，不提交 session，
不推进 parser cursor，也不允许 parser 消费 generated token buffer。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_parser_dry_run_admission_gates`。
- `BuiltinDeriveParserDryRunAdmissionGate`。
- `is_valid(const BuiltinDeriveParserDryRunAdmissionGate&)`。
- `admission_gate_identity`。
- `EarlyItemExpansionSummary` 中的 parser dry-run admission gate 计数。

## 绑定关系

每个 admission gate 绑定：

- generated part identity。
- M25a `dry_run_session_identity`。
- M25b `cursor_snapshot_identity`。
- M25c `diagnostic_shadow_closure_identity`。
- M21e `generated_buffer_identity`。
- M21e `parse_config_fingerprint`。
- `admission_policy = builtin_derive_parser_dry_run_admission_gate_v1`。
- query name：`m26a-builtin-derive-parser-dry-run-admission:<module>:<part>`。

group validation 会要求 M26a 与同一 generated part 上的 M25a session boundary、M25b cursor snapshot proof、
M25c diagnostic shadow closure 和 M21e parse / merge stub 完全一致。任何跨 part identity 串线、query 漂移、
parse config 漂移或上游 identity 漂移都会被拒绝。

## Admission Gate 内容

M26a 固定：

- `dry_run_session_count = 1` when M25a session is visible, otherwise `0`。
- `cursor_snapshot_proof_count = 1` when M25b proof is visible, otherwise `0`。
- `diagnostic_shadow_closure_count = 1` when M25c closure is visible, otherwise `0`。
- `admission_prerequisite_count = 5`。
- `token_buffer_candidate_count = <M25a token_buffer_candidate_count>`。
- `token_record_count = <M25a token_record_count>`。
- `dry_run_execution_admitted_count = 0`。
- `parser_consumable_case_count = 0`。
- `dry_run_session_available=true`。
- `cursor_snapshot_proof_available=true`。
- `diagnostic_shadow_closure_available=true`。
- `generated_buffer_available=true`。
- `parse_config_available=true`。
- `admission_gate_complete=true`。

非 derive / builder-only part 可以没有 token records，因此 `token_record_count=0` 仍是合法事实；admission gate
仍会存在，方便 query/cache/tooling 对所有 generated part 做稳定索引。

## 当前 result baseline

M26a 是 M26a-M26c 三段闭环中的第一段。当前仓库最终 baseline 已推进到：

- result name：`M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure`。
- fingerprint marker：`frontend.macro.m26c.builtin_derive_cursor_rollback_ast_mutation_verifier_closure.v1`。

## 关闭边界

M26a 明确保持：

- `dry_run_execution_admitted=false`。
- `dry_run_executed=false`。
- `diagnostic_shadow_executed=false`。
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
`builtin derive parser dry-run execution admission remains blocked in M26a`。

## 验证和调试面

M26a facts 已接入：

- summary：`builtin_derive_parser_dry_run_admission_gates`、
  `builtin_derive_parser_dry_run_admission_gate_visible`、
  `builtin_derive_parser_dry_run_admission_gate_query_reusable`、
  `builtin_derive_parser_dry_run_admission_gate_complete`、
  `builtin_derive_parser_dry_run_admission_gate_execution_admitted`、
  `builtin_derive_parser_dry_run_admission_gate_executed`、
  `builtin_derive_parser_dry_run_admission_gate_parser_consumable`。
- dump：`builtin_derive_parser_dry_run_admission_gate` section。
- fingerprint：admission identity、上游 identity、query、policy、blocked reason、token counts、prerequisite counts
  或关闭边界变化都会改变 fingerprint。
- validation：拒绝 M25a/M25b/M25c/M21e identity 串线、query 漂移、admission incomplete、
  dry-run execution admitted / executed 被打开、diagnostic shadow execution 被打开、rollback execution 被打开、
  session commit 被打开、parser cursor advance 被打开、parser admission / parser consumption 被打开、generated
  part parse / merge / sema-visible 被打开、AST mutation、标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M26a 仍不实现真实 parser dry-run、dry-run admission execution、parser token consumption、session commit、parser
cursor advance、generated source text、generated module part parse / merge、AST mutation、sema-visible macro
output、真实 source map、debug trace CLI、`--emit-expanded`、用户自定义 derive、用户自定义 macro、external
procedural macro、typed expression macro、文本替换宏、标准库或 runtime helper。
