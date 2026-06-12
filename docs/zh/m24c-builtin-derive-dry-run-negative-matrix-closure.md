# Aurex M24c Builtin Derive Dry-Run Negative Matrix Closure

阶段：M24c Builtin Derive Dry-Run Negative Matrix Closure
状态：已完成。

M24c 在 M24a controlled dry-run adapter 和 M24b rollback diagnostic replay 之后，为每个 generated module part
增加 `BuiltinDeriveDryRunNegativeMatrixClosure`。它把 builtin derive dry-run 的负面矩阵收口成可验证 facts，
确保当前阶段没有任何路径会把 dry-run、parser consumption、generated part parsing、sema visibility、标准库、
runtime 或用户代码提前打开。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_dry_run_negative_matrices`。
- `BuiltinDeriveDryRunNegativeMatrixClosure`。
- `is_valid(const BuiltinDeriveDryRunNegativeMatrixClosure&)`。
- `negative_matrix_identity`。
- `EarlyItemExpansionSummary` 中的 dry-run negative matrix 计数。

## 绑定关系

每个 negative matrix 绑定：

- generated part identity。
- M24a `dry_run_adapter_identity`。
- M24b `replay_protocol_identity`。
- M23c `verification_closure_identity`。
- `matrix_policy = builtin_derive_dry_run_negative_matrix_closure_v1`。
- query name：`m24c-builtin-derive-dry-run-negative-matrix:<module>:<part>`。

group validation 要求 M24c 与同一 generated part 上的 M24a adapter、M24b replay protocol 和 M23c verification
closure 一一对应。

## Negative Matrix 内容

M24c 固定：

- `dry_run_adapter_count = 1`。
- `rollback_replay_count = 1`。
- `verification_closure_count = 1`。
- `negative_case_count = 8`。
- `parser_consumable_case_count = 0`。
- `dry_run_adapter_available=true`。
- `rollback_replay_available=true`。
- `verification_closure_available=true`。
- `negative_matrix_complete=true`。

这 8 类负面用例覆盖当前阶段最危险的越界面：dry-run execution、parser consumption、parser admission、generated
part parse/merge、sema visibility、debug/source-map/emit projection、stdlib/runtime/external dependency 和 user
generated code。

## 当前 result baseline

M24c 当前 baseline：

- result name：`M24c Builtin Derive Dry-Run Negative Matrix Closure`。
- fingerprint marker：`frontend.macro.m24c.builtin_derive_dry_run_negative_matrix_closure.v1`。

M21/M22/M23/M24 facts 都保留在 `EarlyItemExpansionResult` 中；M24c 只是关闭 dry-run 进入 parser consumption
之前的负面矩阵闭环，不会删除已有 facts。

## 关闭边界

M24c 明确保持：

- `dry_run_executed=false`。
- `parser_consumption_enabled=false`。
- `parser_admitted=false`。
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
`builtin derive dry-run negative matrix keeps parser consumption blocked in M24c`。

## 验证和调试面

M24c facts 已接入：

- summary：`builtin_derive_dry_run_negative_matrices`、
  `builtin_derive_dry_run_negative_matrix_complete`、
  `builtin_derive_dry_run_negative_matrix_parser_consumable`。
- dump：`builtin_derive_dry_run_negative_matrix` section。
- fingerprint：negative matrix identity、query、case count、parser consumable case count、policy、blocked reason 或
  关闭边界变化都会改变 fingerprint。
- validation：拒绝 M24a/M24b/M23c identity 串线、query 漂移、negative matrix incomplete、parser consumable
  case 出现、dry-run execution 被打开、parser admission / parser consumption 被打开、generated part parse /
  merge / sema-visible 被打开、标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M24c 仍不实现真实 parser dry-run、parser token consumption、generated source text、generated module part parse /
merge、AST mutation、sema-visible macro output、真实 source map、debug trace CLI、`--emit-expanded`、用户自定义
derive、external procedural macro、typed expression macro、标准库或 runtime helper。
