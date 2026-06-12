# Aurex M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure

阶段：M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure
状态：已完成。

M26c 在 M26a parser dry-run admission gate 和 M26b error recovery shadow diagnostic gate 之后，为每个 generated
module part 增加 `BuiltinDeriveCursorRollbackAstMutationVerifierClosure`。它把 future cursor rollback execution guard
和 AST mutation verifier 收成一个闭环：当前只证明 rollback / recovery / diagnostic / AST mutation 都保持关闭，
并把这些关闭状态纳入 summary、dump、fingerprint 和 validation。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_cursor_rollback_ast_mutation_verifier_closures`。
- `BuiltinDeriveCursorRollbackAstMutationVerifierClosure`。
- `is_valid(const BuiltinDeriveCursorRollbackAstMutationVerifierClosure&)`。
- `verifier_closure_identity`。
- `EarlyItemExpansionSummary` 中的 cursor rollback AST mutation verifier closure 计数。

## 绑定关系

每个 verifier closure 绑定：

- generated part identity。
- M26a `admission_gate_identity`。
- M26b `recovery_shadow_identity`。
- M25b `cursor_snapshot_identity`。
- M25a `dry_run_session_identity`。
- M25c `diagnostic_shadow_closure_identity`。
- `verifier_policy = builtin_derive_cursor_rollback_ast_mutation_verifier_closure_v1`。
- query name：`m26c-builtin-derive-cursor-rollback-ast-verifier:<module>:<part>`。

group validation 会要求 M26c 与同一 generated part 上的 M26a admission gate、M26b recovery shadow gate、
M25b cursor snapshot proof、M25a session boundary 和 M25c diagnostic shadow closure 完全一致。任何跨 part
identity 串线、query 漂移、rollback proof 漂移或上游 identity 漂移都会被拒绝。

## Verifier Closure 内容

M26c 固定：

- `cursor_snapshot_count = <M25b cursor_snapshot_count>`。
- `rollback_proof_count = <M25b rollback_proof_count>`。
- `recovery_shadow_count = 1` when M26b gate is visible, otherwise `0`。
- `ast_baseline_snapshot_count = 1`。
- `ast_mutation_count = 0`。
- `cursor_commit_count = 0`。
- `session_commit_count = 0`。
- `parser_consumable_case_count = 0`。
- `dry_run_admission_gate_available=true`。
- `recovery_shadow_available=true`。
- `cursor_snapshot_proof_available=true`。
- `dry_run_session_available=true`。
- `diagnostic_shadow_closure_available=true`。
- `ast_baseline_available=true`。
- `rollback_execution_guard_available=true`。
- `ast_mutation_verifier_complete=true`。

`ast_baseline_snapshot_count=1` 表示 verifier 有一个 compiler-owned baseline snapshot identity 可用于 future
AST mutation 检查；当前不会读取、修改或提交真实 AST。

## 当前 result baseline

M26c 是 M26a-M26c 三段闭环的 release baseline。当前仓库最终 baseline 固定为：

- result name：`M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure`。
- fingerprint marker：`frontend.macro.m26c.builtin_derive_cursor_rollback_ast_mutation_verifier_closure.v1`。

## 关闭边界

M26c 明确保持：

- `rollback_execution_enabled=false`。
- `recovery_execution_enabled=false`。
- `diagnostic_emission_enabled=false`。
- `dry_run_execution_admitted=false`。
- `dry_run_executed=false`。
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
`builtin derive cursor rollback execution and AST mutation verifier remain check-only in M26c`。

## 验证和调试面

M26c facts 已接入：

- summary：`builtin_derive_cursor_rollback_ast_mutation_verifier_closures`、
  `builtin_derive_cursor_rollback_ast_mutation_verifier_visible`、
  `builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable`、
  `builtin_derive_cursor_rollback_ast_mutation_verifier_complete`、
  `builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed`、
  `builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation`、
  `builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable`。
- dump：`builtin_derive_cursor_rollback_ast_mutation_verifier_closure` section。
- fingerprint：verifier closure identity、M26a/M26b/M25a/M25b/M25c identity、policy、query、blocked reason、
  rollback/cursor/session/AST counts 或关闭边界变化都会改变 fingerprint。
- validation：拒绝 M26a/M26b/M25a/M25b/M25c identity 串线、query 漂移、verifier incomplete、rollback execution
  被打开、recovery execution 被打开、diagnostic emission 被打开、dry-run execution 被打开、session commit 被打开、
  parser cursor advance 被打开、parser admission / parser consumption 被打开、generated part parse / merge /
  sema-visible 被打开、AST mutation、标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M26c 仍不实现真实 parser dry-run、dry-run admission execution、真实 parser recovery、用户可见 diagnostic emission、
rollback execution、parser cursor advance、parser token consumption、session commit、generated source text、
generated module part parse / merge、AST mutation、sema-visible macro output、真实 source map、debug trace CLI、
`--emit-expanded`、用户自定义 derive、用户自定义 macro、external procedural macro、typed expression macro、
文本替换宏、标准库或 runtime helper。
