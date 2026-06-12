# Aurex M24a Builtin Derive Controlled Parser Dry-Run Adapter

阶段：M24a Builtin Derive Controlled Parser Dry-Run Adapter
状态：已完成。

M24a 在 M23c pre-consumption verification closure 之后，为每个 generated module part 增加
`BuiltinDeriveControlledParserDryRunAdapter`。它把“未来可以做 builtin derive parser dry-run”的输入边界固化成
facts，但仍不真正调用 parser、不推进 token cursor、不 parse generated module part，也不把结果暴露给 sema。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_controlled_dry_run_adapters`。
- `BuiltinDeriveControlledParserDryRunAdapter`。
- `is_valid(const BuiltinDeriveControlledParserDryRunAdapter&)`。
- `dry_run_adapter_identity`。
- `EarlyItemExpansionSummary` 中的 controlled dry-run adapter 计数。

## 绑定关系

每个 adapter 绑定：

- generated part identity。
- M23c `verification_closure_identity`。
- M23a `admission_protocol_identity`。
- M23b `checkpoint_protocol_identity`。
- `adapter_policy = builtin_derive_controlled_parser_dry_run_adapter_v1`。
- query name：`m24a-builtin-derive-controlled-parser-dry-run:<module>:<part>`。

group validation 会要求 adapter 与同一 generated part 上的 M23a admission protocol、M23b checkpoint rollback
protocol 和 M23c verification closure 完全一致，任何跨 part identity 串线都会被拒绝。

## Adapter 内容

M24a 固定：

- `token_record_count = <M23b token_record_count>`。
- `diagnostic_anchor_count = <M23b diagnostic_anchor_count>`。
- `prerequisite_count = 5`。
- `verification_closure_available=true`。
- `admission_protocol_available=true`。
- `checkpoint_protocol_available=true`。
- `compiler_owned_tokens_available=true`。
- `diagnostic_replay_available=true`。
- `dry_run_adapter_complete=true`。

注意：非 derive / builder-only part 可以没有 generated token records，因此 `token_record_count=0` 是合法的；但
diagnostic anchor 仍必须存在，用于后续 dry-run 失败 replay 的稳定归档。

## 当前 result baseline

M24a 是 M24a-M24c 三段闭环中的第一段。当前仓库最终 baseline 已推进到：

- result name：`M24c Builtin Derive Dry-Run Negative Matrix Closure`。
- fingerprint marker：`frontend.macro.m24c.builtin_derive_dry_run_negative_matrix_closure.v1`。

## 关闭边界

M24a 明确保持：

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
`builtin derive controlled parser dry-run adapter remains execution-blocked in M24a`。

## 验证和调试面

M24a facts 已接入：

- summary：`builtin_derive_controlled_dry_run_adapters`、
  `builtin_derive_controlled_dry_run_adapter_complete`、
  `builtin_derive_controlled_dry_run_adapter_executed`。
- dump：`builtin_derive_controlled_dry_run_adapter` section。
- fingerprint：adapter identity、query、count、policy、blocked reason 或关闭边界变化都会改变 fingerprint。
- validation：拒绝 M23 identity 串线、query 漂移、adapter incomplete、dry-run execution 被打开、parser admission /
  parser consumption 被打开、generated part parse / merge / sema-visible 被打开、标准库 / runtime / external
  process / user code flag 被打开。

## 仍不实现

M24a 仍不实现真实 parser dry-run、parser token consumption、generated source text、generated module part parse /
merge、AST mutation、sema-visible macro output、真实 source map、debug trace CLI、`--emit-expanded`、用户自定义
derive、external procedural macro、typed expression macro、标准库或 runtime helper。
