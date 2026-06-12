# Aurex M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure

阶段：M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure
状态：已完成。

M25c 在 M25a parser dry-run session boundary 和 M25b token cursor snapshot rollback proof 之后，为每个
generated module part 增加 `BuiltinDeriveDiagnosticShadowNoAstMutationClosure`。它把 M24b rollback diagnostic
replay、M24c negative matrix、M25a session 和 M25b cursor proof 汇总成最后一道 check-only dry-run sandbox
关闭门：diagnostic shadow 可以被索引和 dump，但不会执行 shadow replay，不会让 parser 消费 token，不会修改 AST，
也不会产生 sema-visible macro output。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_diagnostic_shadow_no_ast_mutation_closures`。
- `BuiltinDeriveDiagnosticShadowNoAstMutationClosure`。
- `is_valid(const BuiltinDeriveDiagnosticShadowNoAstMutationClosure&)`。
- `closure_identity`。
- `EarlyItemExpansionSummary` 中的 diagnostic shadow no-AST-mutation closure 计数。
- `EarlyItemExpansionSummary::ast_mutation_count`。

## 绑定关系

每个 diagnostic shadow closure 绑定：

- generated part identity。
- M25a `dry_run_session_identity`。
- M25b `cursor_snapshot_identity`。
- M24b `replay_protocol_identity`。
- M24c `negative_matrix_identity`。
- `closure_policy = builtin_derive_diagnostic_shadow_no_ast_mutation_closure_v1`。
- query name：`m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:<module>:<part>`。

group validation 要求 M25c 与同一 generated part 上的 M25a session、M25b cursor proof、M24b rollback replay 和
M24c negative matrix 一一对应。

## Diagnostic Shadow / No-AST-Mutation 内容

M25c 固定：

- `dry_run_session_count = 1`。
- `cursor_snapshot_proof_count = 1`。
- `rollback_replay_count = 1`。
- `negative_matrix_count = 1`。
- `diagnostic_shadow_count = <M24b planned_replay_count>`。
- `executed_shadow_count = 0`。
- `ast_mutation_count = 0`。
- `parser_consumable_case_count = 0`。
- `dry_run_session_available=true`。
- `cursor_snapshot_proof_available=true`。
- `rollback_replay_available=true`。
- `negative_matrix_available=true`。
- `diagnostic_shadow_available=true`。
- `no_ast_mutation_verified=true`。
- `closure_complete=true`。

这表示 diagnostic replay 的 shadow projection 已经能稳定绑定 dry-run sandbox，但它只作为 debug/query 事实存在；
当前不会执行 shadow replay，也不会把任何生成结果交给 parser、AST、sema 或 backend。

## 当前 result baseline

M25c 当前 baseline：

- result name：`M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure`。
- fingerprint marker：`frontend.macro.m25c.builtin_derive_diagnostic_shadow_no_ast_mutation_closure.v1`。

M21/M22/M23/M24/M25 facts 都保留在 `EarlyItemExpansionResult` 中；M25c 只是关闭 check-only parser dry-run
sandbox 进入真实 parser consumption / AST mutation 之前的最后一层事实闭环，不会删除已有 facts。

## 关闭边界

M25c 明确保持：

- `dry_run_executed=false`。
- `replay_execution_enabled=false`。
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
`builtin derive diagnostic shadow replay remains non-executing and no-AST-mutation in M25c`。

## 验证和调试面

M25c facts 已接入：

- summary：`builtin_derive_diagnostic_shadow_no_ast_mutation_closures`、
  `builtin_derive_diagnostic_shadow_no_ast_mutation_complete`、
  `builtin_derive_diagnostic_shadow_no_ast_mutation_executed`、
  `builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation`、
  `builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable` 和 `ast_mutations`。
- dump：`builtin_derive_diagnostic_shadow_no_ast_mutation_closure` section。
- fingerprint：closure identity、query、diagnostic shadow count、executed shadow count、AST mutation count、
  parser consumable case count、policy、blocked reason 或关闭边界变化都会改变 fingerprint。
- validation：拒绝 M25a/M25b/M24b/M24c identity 串线、query 漂移、closure incomplete、diagnostic shadow
  execution 被打开、dry-run execution 被打开、session commit 被打开、parser cursor advance 被打开、parser admission /
  parser consumption 被打开、parser-consumable case 出现、generated part parse / merge / sema-visible 被打开、
  AST mutation、标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M25c 仍不实现真实 parser dry-run、rollback execution、diagnostic replay execution、diagnostic shadow execution、
parser token consumption、session commit、parser cursor advance、generated source text、generated module part parse /
merge、AST mutation、sema-visible macro output、真实 source map、debug trace CLI、`--emit-expanded`、用户自定义
derive、用户自定义 macro、external procedural macro、typed expression macro、文本替换宏、标准库或 runtime helper。
