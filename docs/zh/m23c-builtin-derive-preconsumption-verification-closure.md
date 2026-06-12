# Aurex M23c Builtin Derive Parser Pre-Consumption Verification Closure

阶段：M23c Builtin Derive Parser Pre-Consumption Verification Closure
状态：已完成。

M23c 在 M23a/M23b 之后，为每个 generated module part 增加
`BuiltinDeriveParserPreConsumptionVerificationClosure`。它把 builtin derive parser consumption 之前的最后一层
verification closure 固定为 facts，并把 `EarlyItemExpansionResult::name` 推进到
`M23c Builtin Derive Parser Pre-Consumption Verification Closure`。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_preconsumption_verification_closures`。
- `BuiltinDeriveParserPreConsumptionVerificationClosure`。
- `is_valid(const BuiltinDeriveParserPreConsumptionVerificationClosure&)`。
- `verification_closure_identity`。
- `EarlyItemExpansionSummary` 中的 builtin derive preconsumption verification closure 计数。

## 绑定关系

每个 verification closure 绑定：

- generated part identity。
- M23a `admission_protocol_identity`。
- M23b `checkpoint_protocol_identity`。
- M22e `debug_dump_contract_identity`。
- `verification_policy = builtin_derive_parser_preconsumption_verification_closure_v1`。
- query name：`m23c-builtin-derive-preconsumption-verification:<module>:<part>`。

此外，group validation 会要求同一 generated part 上的 M22d release hardening matrix、M22e debug dump contract、
M22f rollback diagnostic gate、M23a admission protocol 和 M23b checkpoint protocol 都可见。

## Closure 内容

M23c 固定：

- `admission_protocol_count = 1`。
- `checkpoint_protocol_count = 1`。
- `hardening_matrix_count = 1`。
- `debug_dump_contract_count = 1`。
- `rollback_gate_count = 1`。
- `admission_protocol_available=true`。
- `checkpoint_protocol_available=true`。
- `release_hardening_available=true`。
- `debug_dump_contract_available=true`。
- `rollback_gate_available=true`。
- `verification_closure_complete=true`。

这表示 builtin derive parser consumption 之前的 admission、checkpoint/rollback、release hardening、debug dump 和
rollback diagnostic 设计门禁已经能被同一个 deterministic query boundary 查询、dump、fingerprint 和校验。

## 当前 result baseline

M23c 当前 baseline：

- result name：`M23c Builtin Derive Parser Pre-Consumption Verification Closure`。
- fingerprint marker：`frontend.macro.m23c.builtin_derive_preconsumption_verification_closure.v1`。

M21/M22/M23 facts 都保留在 `EarlyItemExpansionResult` 中；M23c 只是关闭 parser consumption 前的验证闭环，不会删除
已有 M21/M22 事实。

## 关闭边界

M23c 明确保持：

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
`builtin derive pre-consumption verification closure keeps parser consumption blocked in M23c`。

## 验证和调试面

M23c facts 已接入：

- summary：`builtin_derive_preconsumption_verification_closures`、
  `builtin_derive_preconsumption_verification_complete`、
  `builtin_derive_preconsumption_verification_parser_consumable`。
- dump：`builtin_derive_preconsumption_verification_closure` section。
- fingerprint：任何 closure identity、query、count、policy 或关闭边界变化都会改变 fingerprint。
- validation：拒绝 M23a/M23b/M22e identity 串线、closure count 漂移、parser consumption 被打开、sema visible 被打开、
  generated part parse / merge 被打开、标准库 / runtime / external process / user code flag 被打开。

## 仍不实现

M23c 仍不实现 parser consumption、generated source text、generated module part parse / merge、AST mutation、真实
hygiene resolution、真实 source map、debug trace CLI、`--emit-expanded`、用户自定义 derive、external procedural
macro、typed expression macro、标准库或 runtime helper。
