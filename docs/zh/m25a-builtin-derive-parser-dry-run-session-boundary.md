# Aurex M25a Builtin Derive Parser Dry-Run Session Boundary

阶段：M25a Builtin Derive Parser Dry-Run Session Boundary
状态：已完成。

M25a 在 M24a controlled parser dry-run adapter 和 M24c negative matrix closure 之后，为每个 generated module
part 增加 `BuiltinDeriveParserDryRunSessionBoundary`。它把“可以建立一个 compiler-owned parser dry-run session”
这件事落成可验证 facts，但 session 仍是 check-only、不可提交、不可推进 token cursor，也不会 parse 或 merge
generated module part。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_parser_dry_run_sessions`。
- `BuiltinDeriveParserDryRunSessionBoundary`。
- `is_valid(const BuiltinDeriveParserDryRunSessionBoundary&)`。
- `dry_run_session_identity`。
- `EarlyItemExpansionSummary` 中的 parser dry-run session 计数。

## 绑定关系

每个 session boundary 绑定：

- generated part identity。
- M24a `dry_run_adapter_identity`。
- M24c `negative_matrix_identity`。
- M21e `generated_buffer_identity`。
- M21e `parse_config_fingerprint`。
- `session_policy = builtin_derive_parser_dry_run_session_boundary_v1`。
- query name：`m25a-builtin-derive-dry-run-session:<module>:<part>`。

group validation 会要求 M25a 与同一 generated part 上的 M24a adapter、M24c negative matrix 和 M21e parse /
merge stub 完全一致。任何跨 part identity 串线、query 漂移或 parse config 漂移都会被拒绝。

## Session Boundary 内容

M25a 固定：

- `token_buffer_candidate_count = 1` when M24a token records are available, otherwise `0`。
- `token_record_count = <M24a token_record_count>`。
- `diagnostic_anchor_count = <M24a diagnostic_anchor_count>`。
- `parser_state_snapshot_count = 1`。
- `committed_parse_count = 0`。
- `dry_run_adapter_available=true`。
- `negative_matrix_available=true`。
- `compiler_owned_token_stream_available=true`。
- `sandbox_available=true`。
- `check_only=true`。
- `dry_run_session_complete=true`。

非 derive / builder-only part 可以没有 token records，因此 `token_buffer_candidate_count=0` 仍是合法事实；但
session boundary 仍会存在，方便后续 query/cache/tooling 对所有 generated part 做稳定索引。

## 当前 result baseline

M25a 是 M25a-M25c 三段闭环中的第一段。当前仓库最终 baseline 已推进到：

- result name：`M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure`。
- fingerprint marker：`frontend.macro.m25c.builtin_derive_diagnostic_shadow_no_ast_mutation_closure.v1`。

## 关闭边界

M25a 明确保持：

- `dry_run_executed=false`。
- `session_committed=false`。
- `parser_consumption_enabled=false`。
- `parser_admitted=false`。
- `parser_cursor_advanced=false`。
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
`builtin derive parser dry-run session remains check-only and uncommitted in M25a`。

## 验证和调试面

M25a facts 已接入：

- summary：`builtin_derive_parser_dry_run_sessions`、
  `builtin_derive_parser_dry_run_session_complete`、
  `builtin_derive_parser_dry_run_session_executed`、
  `builtin_derive_parser_dry_run_session_committed`。
- dump：`builtin_derive_parser_dry_run_session` section。
- fingerprint：session identity、query、token/diagnostic counts、parser state snapshot count、policy、blocked
  reason 或关闭边界变化都会改变 fingerprint。
- validation：拒绝 M24a/M24c/M21e identity 串线、query 漂移、session incomplete、dry-run execution 被打开、
  session commit 被打开、parser cursor advance 被打开、parser admission / parser consumption 被打开、generated
  part parse / merge / sema-visible 被打开、AST mutation、标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M25a 仍不实现真实 parser dry-run、parser token consumption、session commit、parser cursor advance、generated source
text、generated module part parse / merge、AST mutation、sema-visible macro output、真实 source map、debug trace
CLI、`--emit-expanded`、用户自定义 derive、用户自定义 macro、external procedural macro、typed expression macro、
文本替换宏、标准库或 runtime helper。
