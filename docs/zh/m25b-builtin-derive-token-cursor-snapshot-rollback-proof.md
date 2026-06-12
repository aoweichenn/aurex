# Aurex M25b Builtin Derive Token Cursor Snapshot Rollback Proof

阶段：M25b Builtin Derive Token Cursor Snapshot Rollback Proof
状态：已完成。

M25b 在 M25a parser dry-run session boundary 之后，为每个 generated module part 增加
`BuiltinDeriveTokenCursorSnapshotRollbackProof`。它把 M23b checkpoint rollback protocol 和 M24b dry-run
rollback diagnostic replay 接到 M25a session 上，证明后续即使进入 dry-run 设计面，也必须具备 token cursor /
parser state snapshot 与 rollback proof；当前阶段仍不执行 replay、不执行 rollback、不推进 parser cursor。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_token_cursor_snapshot_proofs`。
- `BuiltinDeriveTokenCursorSnapshotRollbackProof`。
- `is_valid(const BuiltinDeriveTokenCursorSnapshotRollbackProof&)`。
- `cursor_snapshot_identity`。
- `EarlyItemExpansionSummary` 中的 token cursor snapshot rollback proof 计数。

## 绑定关系

每个 cursor snapshot proof 绑定：

- generated part identity。
- M25a `dry_run_session_identity`。
- M23b `checkpoint_protocol_identity`。
- M24b `replay_protocol_identity`。
- `snapshot_policy = builtin_derive_token_cursor_snapshot_rollback_proof_v1`。
- query name：`m25b-builtin-derive-token-cursor-rollback-proof:<module>:<part>`。

group validation 要求 M25b 与同一 generated part 上的 M25a session、M23b checkpoint rollback protocol 和 M24b
rollback replay protocol 一一对应。

## Snapshot / Rollback Proof 内容

M25b 固定：

- `token_record_count = <M25a token_record_count>`。
- `checkpoint_count = <M23b checkpoint_count>`。
- `cursor_snapshot_count = <M23b checkpoint_count>`。
- `parser_state_snapshot_count = <M23b checkpoint_count>`。
- `rollback_proof_count = <M23b rollback_plan_count>`。
- `cursor_commit_count = 0`。
- `dry_run_session_available=true`。
- `checkpoint_protocol_available=true`。
- `rollback_replay_available=true`。
- `token_cursor_snapshot_available=true`。
- `parser_state_snapshot_available=true`。
- `rollback_proof_complete=true`。

当前默认 M23b checkpoint/rollback plan 为 3 组：parser state checkpoint、token cursor checkpoint、generated part
checkpoint。M25b 只证明这些边界可见并可被 query/cache/debug dump 稳定追踪，不执行任何 rollback 或 replay。

## 当前 result baseline

M25b 是 M25a-M25c 三段闭环中的第二段。当前仓库最终 baseline 已推进到：

- result name：`M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure`。
- fingerprint marker：`frontend.macro.m25c.builtin_derive_diagnostic_shadow_no_ast_mutation_closure.v1`。

## 关闭边界

M25b 明确保持：

- `replay_execution_enabled=false`。
- `rollback_execution_enabled=false`。
- `dry_run_executed=false`。
- `parser_cursor_advanced=false`。
- `session_committed=false`。
- `parser_consumption_enabled=false`。
- `parser_admitted=false`。
- `generated_part_parsed=false`。
- `generated_part_merged=false`。
- `ast_mutated=false`。
- `sema_visible=false`。
- `standard_library_required=false`。
- `runtime_required=false`。
- `external_process_required=false`。
- `produced_user_generated_code=false`。

blocker 固定为
`builtin derive token cursor snapshot rollback proof keeps parser cursor unadvanced in M25b`。

## 验证和调试面

M25b facts 已接入：

- summary：`builtin_derive_token_cursor_snapshot_proofs`、
  `builtin_derive_token_cursor_snapshot_proof_complete`、
  `builtin_derive_token_cursor_snapshot_proof_cursor_advanced`、
  `builtin_derive_token_cursor_snapshot_proof_committed`。
- dump：`builtin_derive_token_cursor_snapshot_proof` section。
- fingerprint：cursor snapshot identity、query、checkpoint count、snapshot count、rollback proof count、
  cursor commit count、policy、blocked reason 或关闭边界变化都会改变 fingerprint。
- validation：拒绝 M25a/M23b/M24b identity 串线、query 漂移、proof incomplete、replay / rollback execution 被打开、
  dry-run execution 被打开、parser cursor advance 被打开、cursor commit count 非零、session commit 被打开、
  parser admission / parser consumption 被打开、generated part parse / merge / sema-visible 被打开、AST mutation、
  标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M25b 仍不实现真实 parser dry-run、rollback execution、diagnostic replay execution、parser token consumption、
session commit、parser cursor advance、generated source text、generated module part parse / merge、AST mutation、
sema-visible macro output、用户自定义 derive、用户自定义 macro、external procedural macro、typed expression
macro、文本替换宏、标准库或 runtime helper。
