# 版本文档

## M24c Builtin Derive Dry-Run Negative Matrix Closure

当前版本在 M23c builtin derive parser pre-consumption verification closure 之后，完成 M24a/M24b/M24c 三段
controlled parser dry-run facts 收口。`EarlyItemExpansionResult` 的 result name 固定为
`M24c Builtin Derive Dry-Run Negative Matrix Closure`。本阶段仍不执行 dry-run、不执行用户宏、不生成 source text、
不让 parser 消费 generated token buffer、不 parse / merge generated module part、不生成用户代码，也不引入标准库或
runtime helper。

新增或固定：

- 新增 `frontend::macro::BuiltinDeriveControlledParserDryRunAdapter`。
- 新增 `frontend::macro::BuiltinDeriveDryRunRollbackDiagnosticReplay`。
- 新增 `frontend::macro::BuiltinDeriveDryRunNegativeMatrixClosure`。
- 新增 `is_valid(const BuiltinDeriveControlledParserDryRunAdapter&)`。
- 新增 `is_valid(const BuiltinDeriveDryRunRollbackDiagnosticReplay&)`。
- 新增 `is_valid(const BuiltinDeriveDryRunNegativeMatrixClosure&)`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_controlled_dry_run_adapters`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_dry_run_rollback_replays`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_dry_run_negative_matrices`。
- `EarlyItemExpansionSummary` 新增 builtin derive controlled dry-run adapter、dry-run rollback replay 和
  dry-run negative matrix 计数。
- M24a adapter 固定 `builtin_derive_controlled_parser_dry_run_adapter_v1`。
- M24a adapter query name 固定
  `m24a-builtin-derive-controlled-parser-dry-run:<module>:<part>`。
- M24a adapter 绑定 M23c `verification_closure_identity`、M23a `admission_protocol_identity` 和 M23b
  `checkpoint_protocol_identity`。
- M24a adapter 记录 token record count、diagnostic anchor count 和 `prerequisite_count=5`。
- M24b rollback replay 固定 `builtin_derive_dry_run_rollback_diagnostic_replay_v1`。
- M24b rollback replay query name 固定
  `m24b-builtin-derive-dry-run-rollback-replay:<module>:<part>`。
- M24b rollback replay 绑定 M24a `dry_run_adapter_identity`、M23b `checkpoint_protocol_identity` 和 M22f
  `rollback_gate_identity`。
- M24b rollback replay 固定 diagnostic anchor、report entry、planned replay 和 executed replay counts。
- M24c negative matrix 固定 `builtin_derive_dry_run_negative_matrix_closure_v1`。
- M24c negative matrix query name 固定
  `m24c-builtin-derive-dry-run-negative-matrix:<module>:<part>`。
- M24c negative matrix 绑定 M24a `dry_run_adapter_identity`、M24b `replay_protocol_identity` 和 M23c
  `verification_closure_identity`。
- M24c negative matrix 固定 `negative_case_count=8` 和 `parser_consumable_case_count=0`。
- validation 拒绝 M24 identity / query / count 漂移、上游 identity 串线、dry-run execution / replay execution /
  parser admission / parser consumption 被打开、generated part parse / merge / sema-visible 被打开、standard
  library/runtime/external process requirement 被打开、emit/debug/source-map/user-code flag 被打开。
- dump 会输出 `builtin_derive_controlled_dry_run_adapter`、
  `builtin_derive_dry_run_rollback_replay` 和 `builtin_derive_dry_run_negative_matrix`。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser dry-run execution。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。
- rollback execution。

## M23c Builtin Derive Parser Pre-Consumption Verification Closure

当前版本在 M22f builtin derive rollback diagnostic design gate 之后，完成 M23a/M23b/M23c 三段 parser
consumption admission 收口。`EarlyItemExpansionResult` 的 result name 固定为
`M23c Builtin Derive Parser Pre-Consumption Verification Closure`。本阶段仍不执行用户宏、不生成 source text、
不让 parser 消费 generated token buffer、不 parse / merge generated module part、不生成用户代码，也不引入标准库或
runtime helper。

新增或固定：

- 新增 `frontend::macro::BuiltinDeriveParserConsumptionAdmissionProtocol`。
- 新增 `frontend::macro::BuiltinDeriveParserConsumptionCheckpointRollbackProtocol`。
- 新增 `frontend::macro::BuiltinDeriveParserPreConsumptionVerificationClosure`。
- 新增 `is_valid(const BuiltinDeriveParserConsumptionAdmissionProtocol&)`。
- 新增 `is_valid(const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol&)`。
- 新增 `is_valid(const BuiltinDeriveParserPreConsumptionVerificationClosure&)`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_parser_consumption_admission_protocols`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_checkpoint_rollback_protocols`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_preconsumption_verification_closures`。
- `EarlyItemExpansionSummary` 新增 builtin derive parser consumption admission、checkpoint rollback 和
  preconsumption verification closure 计数。
- M23a admission protocol 固定 `builtin_derive_parser_consumption_admission_protocol_v1`。
- M23a admission query name 固定
  `m23a-builtin-derive-parser-consumption-admission:<module>:<part>`。
- M23a admission protocol 绑定 M21n `contract_identity`、M22c `release_gate_identity` 和 M22f
  `rollback_gate_identity`。
- M23a admission protocol 记录 token buffer、token record、derive candidate、empty candidate 和 blocked
  diagnostic counts。
- M23b checkpoint rollback protocol 固定 `builtin_derive_parser_checkpoint_rollback_protocol_v1`。
- M23b checkpoint rollback query name 固定
  `m23b-builtin-derive-checkpoint-rollback:<module>:<part>`。
- M23b checkpoint rollback protocol 绑定 M23a `admission_protocol_identity` 和 M22f
  `rollback_gate_identity`。
- M23b checkpoint rollback protocol 固定 `checkpoint_count=3` 和 `rollback_plan_count=3`。
- M23b checkpoint rollback protocol 固定 parser state checkpoint、token cursor checkpoint、generated part
  checkpoint、diagnostic replay 和 complete protocol prerequisites。
- M23c verification closure 固定 `builtin_derive_parser_preconsumption_verification_closure_v1`。
- M23c verification query name 固定
  `m23c-builtin-derive-preconsumption-verification:<module>:<part>`。
- M23c verification closure 绑定 M23a `admission_protocol_identity`、M23b `checkpoint_protocol_identity` 和 M22e
  `debug_dump_contract_identity`。
- M23c verification closure 要求 admission protocol、checkpoint protocol、release hardening matrix、debug dump
  contract 和 rollback gate 都可见。
- validation 拒绝 M23 identity / query / count 漂移、上游 identity 串线、parser admission / parser consumption /
  rollback execution 被打开、generated part parse / merge / sema-visible 被打开、standard library/runtime/external
  process requirement 被打开、emit/debug/source-map/user-code flag 被打开。
- dump 会输出 `builtin_derive_parser_consumption_admission_protocol`、
  `builtin_derive_checkpoint_rollback_protocol` 和
  `builtin_derive_preconsumption_verification_closure`。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。
- rollback execution。

## M22f Builtin Derive Rollback Diagnostic Design Gate

该版本在 M22c builtin derive parser release gate 之后，完成 M22d/M22e/M22f 三段 release-hardening
收口。`EarlyItemExpansionResult` 的 result name 固定为
`M22f Builtin Derive Rollback Diagnostic Design Gate`。本阶段仍不执行用户宏、不生成 source text、不让 parser
消费 generated token buffer、不 parse / merge generated module part、不生成用户代码，也不引入标准库或
runtime helper。

新增或固定：

- 新增 `frontend::macro::BuiltinDeriveReleaseHardeningMatrix`。
- 新增 `frontend::macro::BuiltinDeriveDebugDumpStabilityContract`。
- 新增 `frontend::macro::BuiltinDeriveRollbackDiagnosticDesignGate`。
- 新增 `is_valid(const BuiltinDeriveReleaseHardeningMatrix&)`。
- 新增 `is_valid(const BuiltinDeriveDebugDumpStabilityContract&)`。
- 新增 `is_valid(const BuiltinDeriveRollbackDiagnosticDesignGate&)`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_release_hardening_matrices`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_debug_dump_contracts`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_rollback_diagnostic_gates`。
- `EarlyItemExpansionSummary` 新增 builtin derive release hardening、debug dump contract 和 rollback diagnostic
  design gate 计数。
- M22d hardening matrix 固定 `builtin_derive_release_hardening_matrix_v1`。
- M22d hardening query name 固定 `m22d-builtin-derive-release-hardening:<module>:<part>`。
- M22d hardening matrix 绑定 M22c `release_gate_identity`、`admission_group_identity` 和
  `semantic_plan_group_identity`。
- M22d hardening matrix 记录 part-local admission / derive admission / semantic plan / release gate counts、
  global admission / semantic plan / generated part totals 和 cross-part admission / semantic plan totals。
- M22e debug dump contract 固定 `builtin_derive_debug_dump_stability_contract_v1`。
- M22e debug dump query name 固定 `m22e-builtin-derive-debug-dump:<module>:<part>`。
- M22e debug dump contract 绑定 M22c `release_gate_identity` 和 M22d `hardening_matrix_identity`。
- M22e debug dump contract 固定 `dump_section_count=4`、stable ordering、identity projection、summary projection、
  drift-debuggable 和 complete contract。
- M22f rollback diagnostic design gate 固定 `builtin_derive_rollback_diagnostic_design_gate_v1`。
- M22f rollback diagnostic query name 固定 `m22f-builtin-derive-rollback-diagnostic:<module>:<part>`。
- M22f rollback diagnostic design gate 绑定 M21n `contract_identity`、M22c `release_gate_identity`、M22d
  `hardening_matrix_identity` 和 M22e `debug_dump_contract_identity`。
- M22f rollback diagnostic design gate 记录 diagnostic projection、diagnostic report entry、blocked diagnostic、
  derive diagnostic、empty diagnostic 和 parser consumption contract counts。
- M22f rollback diagnostic design gate 固定 source anchor、token-tree anchor、diagnostic grouping、debug dump
  contract 和 release rollback plan prerequisites 为 available/complete。
- validation 拒绝 M22d/M22e/M22f identity 漂移、part-local/cross-part totals 漂移、debug dump stability 漂移、
  rollback diagnostic prerequisites 漂移、parser consumption / rollback execution 被打开、
  standard library/runtime/external process requirement 被打开、emit/debug/source-map/user-code flag 被打开。
- dump 会输出 `builtin_derive_release_hardening_matrix`、
  `builtin_derive_debug_dump_stability_contract` 和
  `builtin_derive_rollback_diagnostic_design_gate`。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。
- rollback execution。

## M22c Builtin Derive Parser Consumption Release Gate

当前版本在 M21o 宏展开边界 closure 之后，完成 M22a/M22b/M22c 三段 builtin derive parser consumption
释放门禁准备。`EarlyItemExpansionResult` 的 result name 固定为
`M22c Builtin Derive Parser Consumption Release Gate`。本阶段仍不执行用户宏、不生成 source text、不让 parser
消费 generated token buffer、不 parse / merge generated module part、不生成用户代码，也不引入标准库或
runtime helper。

新增或固定：

- 新增 `frontend::macro::BuiltinDeriveExpansionAdmissionGate`。
- 新增 `frontend::macro::BuiltinDeriveSemanticExpansionPlan`。
- 新增 `frontend::macro::BuiltinDeriveParserConsumptionReleaseGate`。
- 新增 `is_valid(const BuiltinDeriveExpansionAdmissionGate&)`。
- 新增 `is_valid(const BuiltinDeriveSemanticExpansionPlan&)`。
- 新增 `is_valid(const BuiltinDeriveParserConsumptionReleaseGate&)`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_expansion_admissions`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_semantic_plans`。
- `EarlyItemExpansionResult` 新增 `builtin_derive_parser_release_gates`。
- `EarlyItemExpansionSummary` 新增 builtin derive admission、semantic plan、parser release gate 和 Copy/Eq/Hash
  capability 计数。
- M22a admission gate 固定 `builtin_derive_expansion_admission_gate_v1`。
- M22a admission query name 固定
  `m22a-builtin-derive-admission:<module>:<part>:<item>:<attr>:<name>`；文档测试也保留
  `m22a-builtin-derive-admission:<module>:<part>` 作为前缀锚点。
- M22a admission kind 固定为 `builtin_derive_expansion_candidate` 或
  `non_derive_attribute_expansion_blocked`。
- M22a admission 绑定 M21i `token_buffer_identity`、M21m `preflight_identity`、M21j `parse_gate_identity`、
  M21k `diagnostic_identity` 和 M21o `closure_identity`。
- M22a 记录 `capability_candidate_count`、`unsupported_candidate_count` 和 `duplicate_candidate_count`。
- M22b semantic plan 固定 `builtin_derive_semantic_expansion_plan_v1`。
- M22b semantic model 固定为 `capability_fact_lowering_plan`。
- M22b 复用现有内建 `#[derive(Copy, Eq, Hash)]` capability path，记录 target kind、Copy/Eq/Hash capability
  count、`capability_set_identity` 和 `semantic_plan_identity`。
- M22c release gate 固定 `builtin_derive_parser_consumption_release_gate_v1`。
- M22c release query name 固定 `m22c-builtin-derive-parser-release:<module>:<part>`。
- M22c release gate 绑定 M21n `contract_identity`、M21o `closure_identity`、M22a admission group identity 和
  M22b semantic plan group identity。
- M22c release gate 汇总 admission、derive admission、semantic plan、capability total 和
  parser-consumable contract counts。
- M22c release gate 固定 rollback diagnostics、debug trace、source-map 和 hygiene prerequisites 为 available。
- validation 拒绝 M22 admission / semantic plan / release gate identity 漂移、totals 漂移、parser consumption 被打开、
  standard library/runtime/external process requirement 被打开、emit/debug/source-map/user-code flag 被打开。
- dump 会输出 `builtin_derive_expansion_admission_gate`、`builtin_derive_semantic_expansion_plan` 和
  `builtin_derive_parser_consumption_release_gate`。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21o Macro Expansion Boundary Release Closure

当前版本把 M21m generated token parser readiness preflight 和 M21n parser consumption contract gate 汇总成
M21 宏展开边界 release closure。`EarlyItemExpansionResult` 的 result name 固定为
`M21o Macro Expansion Boundary Release Closure`。本阶段仍不执行用户宏、不生成 source text、不让 parser
消费 generated token buffer、不 parse / merge generated module part、不生成用户代码，也不引入标准库或
runtime helper。

新增或固定：

- 新增 `frontend::macro::GeneratedTokenParserReadinessPreflightEntry`。
- 新增 `frontend::macro::GeneratedTokenParserConsumptionContractGate`。
- 新增 `frontend::macro::MacroExpansionBoundaryClosureReport`。
- 新增 `is_valid(const GeneratedTokenParserReadinessPreflightEntry&)`。
- 新增 `is_valid(const GeneratedTokenParserConsumptionContractGate&)`。
- 新增 `is_valid(const MacroExpansionBoundaryClosureReport&)`。
- `EarlyItemExpansionResult` 新增 `parser_readiness_preflight_entries`。
- `EarlyItemExpansionResult` 新增 `parser_consumption_contract_gates`。
- `EarlyItemExpansionResult` 新增 `macro_boundary_closure_reports`。
- `EarlyItemExpansionSummary` 新增 parser readiness preflight、parser consumption contract 和 macro boundary
  closure report 计数。
- M21m preflight 固定 `generated_token_parser_consumption_readiness_preflight_v1`。
- M21m preflight 固定 token stream shape：`derive_token_buffer_parser_input_candidate` 或
  `empty_token_stream_parser_input_blocked`。
- M21m preflight 会校验 token index continuity、delimiter balance、source-anchor coverage、parse config
  compatibility、hygiene/source-map prerequisites 和 diagnostic projection availability。
- M21n contract gate 固定 `generated_token_parser_consumption_contract_gate_v1`。
- M21n contract query name 固定 `m21n-parser-consumption-contract:<module>:<part>`。
- M21n contract gate 汇总 preflight entry totals、blocked totals、derive / empty totals、contiguous / delimiter /
  source-anchor / parse-config / diagnostic totals。
- M21o closure report 固定 `m21_macro_expansion_boundary_release_closure_v1`。
- M21o closure query name 固定 `m21o-macro-boundary-closure`。
- M21o closure report 汇总 macro input、generated part、parser admission report、preflight entry、contract gate、
  blocked contract gate 和 parser-consumable contract gate counts。
- validation 拒绝 preflight / contract / closure identity 漂移、totals 漂移、parser consumption 被打开、
  standard library/runtime/external process requirement 被打开、emit/debug/source-map/user-code flag 被打开。
- dump 会输出 `parser_readiness_preflight_entry`、`parser_consumption_contract_gate` 和
  `macro_boundary_closure_report`。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21n Parser Consumption Contract Gate

M21n 按 generated module part 汇总 M21m preflight entries，形成 parser consumption contract gate。它固定
`GeneratedTokenParserConsumptionContractGate`、`parser_consumption_contract_gates`、`contract_identity`、
`contract_grouping_identity`、`contract_anchor_identity`、`generated_token_parser_consumption_contract_gate_v1`
和 `m21n-parser-consumption-contract:<module>:<part>`。M21n 仍固定 `parser_admitted=false`、
`parse_ready=false`、`parser_consumable=false`、`generated_part_parsed=false`、`generated_part_merged=false`、
`sema_visible=false`、`emit_expanded_available=false`、`debug_trace_available=false`、
`source_map_available=false` 和 `produced_user_generated_code=false`。

## M21m Generated Token Parser Consumption Readiness Preflight

M21m 为每个 macro input 新增 `GeneratedTokenParserReadinessPreflightEntry` 和
`parser_readiness_preflight_entries`。preflight entry 绑定 M21i token buffer、M21j parser admission gate、M21k
diagnostic projection、M21l report entry 和 M21f source-map / hygiene / trace facts，并固定
`generated_token_parser_consumption_readiness_preflight_v1`、`preflight_identity`、token stream shape、
delimiter balance、source-anchor coverage 和 parser-blocked blocker。M21m 仍不让 generated token buffer
进入 parser。

## M21l Parser Admission Diagnostic Report Projection

当前版本继续沿用 `macro.expand_items` frontend pipeline boundary，把 M21k 的 per-input parser admission
diagnostic projection 汇总为 generated module part 级 report/query projection。该阶段仍不执行用户宏、不生成
source text、不让 parser 消费 generated token buffer、不 parse / merge generated module part、不生成用户代码，
也不引入标准库或 runtime helper。

新增或固定：

- 新增 `frontend::macro::ParserAdmissionDiagnosticReportEntry`。
- 新增 `frontend::macro::ParserAdmissionDiagnosticReport`。
- 新增 `is_valid(const ParserAdmissionDiagnosticReportEntry&)`。
- 新增 `is_valid(const ParserAdmissionDiagnosticReport&)`。
- `EarlyItemExpansionResult` 新增 `parser_admission_report_entries`。
- `EarlyItemExpansionResult` 新增 `parser_admission_reports`。
- `EarlyItemExpansionSummary` 新增 parser admission report entry、report、blocked entry、derive / empty entry、
  token-record-available entry、visible report、query-reusable report、unordered anchor 和 parser-consumable
  report 计数。
- 每个 M21k diagnostic projection 都有一个 deterministic report entry。
- 每个 generated module part 都有一个 deterministic parser admission report。
- report entry 固定 `report_entry_identity`，并混入 diagnostic identity、diagnostic anchor、parse gate identity、
  source anchors、category、debug projection name、query projection name、token count 和 blocked flags。
- report 固定 `report_policy = parser_admission_blocked_report_query_projection_v1`。
- report 固定 `report_query_name = m21l-parser-admission-report:<module>:<part>`。
- report 固定 `report_identity`、`report_anchor_identity` 和 `report_grouping_identity`。
- report 绑定 M21e `generated_buffer_identity` 和 `parse_config_fingerprint`。
- report 从 entries 重算 `entry_count`、`blocked_entry_count`、`derive_entry_count`、`empty_entry_count` 和
  `token_record_available_entry_count`。
- report 固定 `source_anchor_ordered=true`。
- report / entry 固定 query/debug projection 可见：`report_visible=true`、`query_reusable=true`。
- report / entry 仍固定 `parser_admitted=false`、`parse_ready=false`、`parser_consumable=false`、
  `emit_expanded_available=false`、`debug_trace_available=false`、`source_map_available=false` 和
  `produced_user_generated_code=false`。
- validation 要求 report entry 与 M21k diagnostic projection 一一对应。
- validation 要求 report 与 generated part、M21e parse / merge stub 和该 part 下 report entries 一一对应。
- validation 拒绝 identity、anchor、category totals、query name、source-anchor ordering、parser/debug/source-map/
  emit/user-code flags 漂移。
- dump 会输出 `parser_admission_report_entry`、`parser_admission_diagnostic_report`、query projection name、
  report totals、report identity、report anchor identity、report grouping identity、generated buffer identity 和
  parse config。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21k Parser Admission Diagnostic Projection Gate

当前版本继续沿用 `macro.expand_items` frontend pipeline boundary，把 M21j 的 compiler-owned parser admission
gate 扩展为稳定 diagnostic / dump projection。该阶段仍不执行用户宏、不生成 source text、不让 parser 消费
generated token buffer、不 parse / merge generated module part、不生成用户代码，也不引入标准库或 runtime helper。

新增或固定：

- 新增 `frontend::macro::ParserAdmissionDiagnosticProjectionStub`。
- 新增 `is_valid(const ParserAdmissionDiagnosticProjectionStub&)`。
- `EarlyItemExpansionResult` 新增 `parser_admission_diagnostics`。
- `EarlyItemExpansionSummary` 新增 parser admission diagnostic、blocked diagnostic、derive / empty category、
  emit-expanded projection、debug trace projection 和 source-map projection 计数。
- 每个 macro input 都有一个 deterministic parser admission diagnostic projection。
- diagnostic 固定 `diagnostic_policy = parser_admission_blocked_diagnostic_projection_v1`。
- diagnostic 固定 `diagnostic_identity`，并混入 input、generated part、M21j parse gate identity、diagnostic anchor、
  token plan、token buffer、materialization、M21e generated buffer identity、M21e parse config、source-map、
  hygiene、trace、policy、blocker category、blocker message、debug projection name、token count 和 blocked flags。
- diagnostic 固定 `diagnostic_anchor_identity`，并混入 source anchor、token-tree anchor、parse gate identity、
  trace identity 和 M21f diagnostic anchor。
- 非 `derive` input 的 diagnostic 固定
  `blocker_category = empty_token_buffer_parser_admission_blocked`。
- `derive` input 的 diagnostic 固定
  `blocker_category = derive_token_buffer_parser_admission_blocked`。
- diagnostic 同时记录 token buffer admission blocker 和 generated module part parse blocker。
- 所有 diagnostic 固定 `parser_admitted=false`、`parse_ready=false` 和 `parser_consumable=false`。
- 所有 diagnostic 固定 `generated_part_parsed=false`、`generated_part_merged=false`、
  `emit_expanded_available=false`、`debug_trace_available=false`、`source_map_available=false` 和
  `produced_user_generated_code=false`。
- validation 要求 diagnostic 与 input、source anchors、generated part placeholder、M21e parse / merge stub、M21i
  token buffer、M21j parser gate、source-map identity、hygiene mark 和 trace identity 一一对应。
- validation 拒绝 parser-admitted、parse-ready、parser-consumable、parsed / merged generated part、emit-expanded
  projection、debug trace projection、source-map projection 或 produced user code。
- dump 会输出 `parser_admission_diagnostic_projection_stub`、policy、category、source anchors、token records
  availability、parser flags、emit/debug/source-map availability、diagnostic identity、diagnostic anchor、parse
  gate identity 和 trace identity。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21j Generated Token Parser Admission Gate

当前版本继续沿用 `macro.expand_items` frontend pipeline boundary，把 M21i 的 compiler-owned generated token
buffer prototype 扩展为独立 parser admission gate。该阶段仍不执行用户宏、不生成 source text、不让 parser
消费 generated token buffer、不 parse / merge generated module part、不生成用户代码，也不引入标准库或 runtime
helper。

新增或固定：

- 新增 `frontend::macro::GeneratedTokenParserAdmissionGateStub`。
- 新增 `is_valid(const GeneratedTokenParserAdmissionGateStub&)`。
- `EarlyItemExpansionResult` 新增 `parser_admission_gates`。
- `EarlyItemExpansionSummary` 新增 parser admission gate、compiler-owned parser admission gate、token-record
  available gate、parser-blocked token buffer 和 parser-admitted token buffer 计数。
- 每个 macro input 都有一个 deterministic parser admission gate。
- gate 固定 `parser_gate_policy = compiler_owned_generated_token_parser_admission_gate_v1`。
- gate 固定 `parse_gate_identity`，并混入 input、generated part、M21e generated buffer identity、M21e parse
  config、token plan、token buffer、materialization、source-map、hygiene、token stream、policy、blocker 和 token
  count。
- 非 `derive` input 的 gate 固定 `token_buffer_materialized=false`、`token_records_available=false`。
- `derive` input 的 gate 可以记录 `token_buffer_materialized=true` 和 `token_records_available=true`。
- 所有 gate 固定 `parser_admitted=false`、`parse_ready=false` 和 `parser_consumable=false`。
- 所有 gate 固定 `generated_source_text=false`、`generated_part_parsed=false`、`generated_part_merged=false`、
  `sema_visible=false` 和 `produced_user_generated_code=false`。
- validation 要求 gate 与 input、generated part placeholder、M21e parse / merge stub、M21i token buffer、
  source-map identity 和 hygiene mark 一一对应。
- validation 拒绝 parser-admitted、parse-ready、parser-consumable、generated-source-text、parsed / merged
  generated part、sema-visible 或 produced user code。
- dump 会输出 `generated_token_parser_admission_gate_stub`、policy、token records availability、parser-admitted
  state、parse config、generated buffer identity 和 parse gate identity。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21i Compiler-Owned Generated Token Buffer Prototype

当前版本继续沿用 `macro.expand_items` frontend pipeline boundary，把 M21h 的 compiler-owned token
materialization admission / generated token buffer stub contract 扩展为内建 `derive` 输入的第一版
compiler-owned generated token buffer prototype。该阶段仍不执行用户宏、不生成 source text、不让 parser 消费
generated token buffer、不生成用户代码，也不引入标准库或 runtime helper。

新增或固定：

- 新增 `frontend::macro::GeneratedTokenRecord`。
- 新增 `is_valid(const GeneratedTokenRecord&)`。
- `EarlyItemExpansionResult` 新增 `generated_token_records`。
- `GeneratedTokenBufferStub` 新增 `materialization_identity`。
- `GeneratedTokenBufferStub` 新增 `token_producer_policy`。
- `EarlyItemExpansionSummary` 新增 compiler-owned token buffer、generated token record、compiler-owned generated
  token record 和 parser-visible generated token 计数。
- `TokenMaterializationAdmissionStub` 继续固定
  `admission_policy = compiler_owned_attached_item_token_materialization_admission_v1`。
- 非 `derive` input 继续使用 `compiler_owned_empty_token_stream` 和
  `compiler_owned_blocked_empty_token_producer_v1`，不产生 generated token records。
- `derive` input 使用 `compiler_owned_builtin_derive_token_stream_prototype`。
- `derive` input 使用 `compiler_owned_builtin_derive_token_producer_prototype_v1`。
- `derive` input 的 generated token buffer 固定 `token_count = attribute.token_tree.size() + 2`。
- `derive` input 的 generated token records 固定 begin sentinel、`derive_source_token_placeholder` source
  placeholders 和 end sentinel。
- generated token records 固定 `compiler_owned = true`、`parser_visible = false` 和
  `produced_user_generated_code = false`。
- validation 要求 admission / buffer / record 与 input、generated part、hygiene、trace、generated item declaration、
  declared generated name 和 source ranges 一一对应。
- validation 拒绝 generated source text、parse-ready / parser-consumable token buffer、parser-visible generated
  tokens、standard library required、runtime required、external process required 或 produced user code。
- dump 会输出 `generated_token_record`、token role、internal token text、token identity、token buffer identity、
  materialization identity、source-map identity、hygiene mark 和 token producer policy。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21h Token Materialization Admission Stub Contract

当前版本继续沿用 `macro.expand_items` frontend pipeline boundary，把 M21g 的 generated item / declared generated
name stub contract 扩展为 compiler-owned token materialization admission / empty generated token buffer stub
contract。该阶段仍不执行宏、不生成 tokens、不生成 source text、不生成用户代码，也不引入标准库或 runtime helper。

新增或固定：

- 新增 `frontend::macro::TokenMaterializationAdmissionStub`。
- 新增 `frontend::macro::GeneratedTokenBufferStub`。
- 新增 `is_valid(const TokenMaterializationAdmissionStub&)`。
- 新增 `is_valid(const GeneratedTokenBufferStub&)`。
- `EarlyItemExpansionResult` 新增 `token_materialization_admissions`。
- `EarlyItemExpansionResult` 新增 `generated_token_buffers`。
- `EarlyItemExpansionSummary` 新增 token materialization admission、compiler-owned admission、admitted token
  materialization、materialized token admission、generated token buffer、empty token buffer、materialized token
  buffer、generated source text 和 parse-ready token buffer 计数。
- 每个 macro input 都有一个 deterministic token materialization admission stub。
- 每个 macro input 都有一个 deterministic generated token buffer stub。
- admission 固定 `token_plan_identity`。
- admission 固定 `token_buffer_identity`。
- admission 固定 `admission_policy = compiler_owned_attached_item_token_materialization_admission_v1`。
- token buffer 固定 `token_buffer_kind = compiler_owned_empty_token_stream`。
- admission 绑定 M21g `declaration_identity`、`generated_item_key` 和 `declared_name_identity`。
- admission / buffer 绑定 M21f `source_map_identity`、`trace_identity` 和 `hygiene_mark`。
- validation 要求 admission / buffer 与 input 一一对应。
- validation 拒绝 materialized tokens、generated source text、parse-ready / parser-consumable token buffer。
- validation 拒绝 standard library required、runtime required、external process required 或 produced user code。
- dump 会输出 `token_materialization_admission_stub`、`generated_token_buffer_stub`、token stream name、token plan
  identity、token buffer identity、source-map identity、trace identity、hygiene mark 和 blocker。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- generated module part real token materialization。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21g Generated Item Declared Names Stub Contract

当前版本继续沿用 `macro.expand_items` frontend pipeline boundary，把 M21f 的 hygiene/source-map/debug-trace
stub contract 扩展为 generated item / declared generated names stub contract。该阶段仍不执行宏、不生成用户代码，
也不引入标准库或 runtime helper。

新增或固定：

- 新增 `frontend::macro::GeneratedItemDeclarationStub`。
- 新增 `frontend::macro::DeclaredGeneratedNameStub`。
- 新增 `is_valid(const GeneratedItemDeclarationStub&)`。
- 新增 `is_valid(const DeclaredGeneratedNameStub&)`。
- `EarlyItemExpansionResult` 新增 `generated_item_declarations`。
- `EarlyItemExpansionResult` 新增 `declared_generated_names`。
- `EarlyItemExpansionSummary` 新增 generated item declaration、planned generated item declaration、
  materialized generated item、declared generated name、lookup-visible declared name 和 export-visible declared
  name 计数。
- 每个 macro input 都有一个 deterministic generated item declaration stub。
- 每个 macro input 都有一个 deterministic declared generated name stub。
- generated item declaration 固定 `declaration_identity`。
- generated item declaration 固定 `generated_item_key`。
- generated item declaration 固定 `declaration_role = attached_item_codegen_declared_names_v1`。
- declared generated name 固定 `declared_name_identity`。
- declared generated name 固定 `hygiene_mark`，并绑定 M21f `generated_fresh_mark`。
- generated item declaration 和 declared generated name 都绑定 M21f `declared_name_set`。
- generated item declaration 和 declared generated name 都绑定 M21d/M21e generated module part placeholder。
- validation 要求 generated item declaration / declared generated name 与 input 一一对应。
- validation 拒绝 generated item materialized tokens、parsed、merged、sema-visible 或 produced user code。
- validation 拒绝 declared generated name lookup-visible、export-visible、sema-visible 或 produced user code。
- dump 会输出 `generated_item_declaration_stub`、`declared_generated_name_stub`、generated item name、
  declaration identity、generated item key、declared name identity、hygiene mark 和 blocker。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- generated module part token materialization。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21f Hygiene Source Map Debug Trace Stub Contract

当前版本继续沿用 `macro.expand_items` frontend pipeline boundary，把 M21e 的 generated module part parse / merge
stub contract 扩展为 hygiene/source-map/debug-trace stub contract。该阶段仍不执行宏、不生成用户代码，也不引入
标准库或 runtime helper。

新增或固定：

- 新增 `frontend::macro::ExpansionHygieneStub`。
- 新增 `frontend::macro::ExpansionTraceStub`。
- 新增 `is_valid(const ExpansionHygieneStub&)`。
- 新增 `is_valid(const ExpansionTraceStub&)`。
- `EarlyItemExpansionResult` 新增 `hygiene_stubs`。
- `EarlyItemExpansionResult` 新增 `trace_stubs`。
- `EarlyItemExpansionSummary` 新增 hygiene stub、unresolved hygiene、declared name stub、call-site capture、
  trace stub、real source map、debug trace 和 `--emit-expanded` 计数。
- 每个 macro input 都有一个 deterministic hygiene stub。
- hygiene stub 固定 `call_site_mark`。
- hygiene stub 固定 `definition_site_mark`。
- hygiene stub 固定 `generated_fresh_mark`。
- hygiene stub 固定 `declared_name_set`。
- hygiene policy 固定为 `origin_mark_hygiene_v1`。
- 每个 macro input 都有一个 deterministic trace stub。
- trace stub 固定 `trace_identity`。
- trace stub 固定 `generated_source_map_identity`。
- trace stub 固定 `diagnostic_anchor`。
- trace policy 固定为 `expansion_source_map_debug_trace_v1`。
- validation 要求 source-map placeholder、hygiene stub 和 trace stub 与 input 一一对应。
- validation 拒绝 hygiene resolved、declared names visible 或 call-site local capture。
- validation 拒绝 real source map、debug trace 或 `--emit-expanded` 可用。
- dump 会输出 `hygiene_stub`、`trace_stub`、policy、mark identity、source-map identity、diagnostic anchor 和 blocker。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## M21e Generated Module Part Parse/Merge Stub Contract

当前版本继续沿用 `macro.expand_items` frontend pipeline boundary，把 M21d 的 generated placeholder 扩展为
generated module part parse / merge stub contract。该阶段仍不执行宏、不生成用户代码，也不引入标准库或
runtime helper。

新增或固定：

- 新增 `frontend::macro::GeneratedModulePartParseMergeStub`。
- 新增 `frontend::macro::GeneratedModulePartLifecycleState`。
- 新增 `generated_module_part_lifecycle_state_name()`。
- 新增 `is_valid(GeneratedModulePartLifecycleState)`。
- 新增 `is_valid(const GeneratedModulePartParseMergeStub&)`。
- `EarlyItemExpansionResult` 新增 `generated_part_stubs`。
- `EarlyItemExpansionSummary` 新增 generated part stub、materialized buffer、parse blocked、merge blocked 和
  sema visible generated part 计数。
- 每个 generated placeholder 都有一个 deterministic parse / merge stub。
- stub 固定 `generated_buffer_identity`。
- stub 固定 `parse_config_fingerprint`。
- stub 固定 `merge_ordering_key`。
- stub 固定 expansion origin 和 generated buffer name。
- stub 当前唯一合法 lifecycle 是 `merge_blocked`。
- validation 要求 stub 与 placeholder 一一对应。
- validation 拒绝 stub parsed、merged、sema-visible 或 produced user-generated code。
- dump 会输出 `parse_merge_stub`、lifecycle、buffer identity、parse config 和 merge ordering。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- generated module part parse / merge。
- 真实 hygiene resolution。
- 真实 expansion source map。
- `--emit-expanded` 或 macro trace CLI。

## M21d No-op Early Item Macro Expansion Boundary

当前版本把 M21c 的 early item expansion plan 接入真实 frontend pipeline。该阶段仍不执行宏、不生成用户代码，
也不引入标准库或 runtime helper。

新增或固定：

- 新增 `frontend::macro::EarlyItemMacroInput`。
- 新增 `frontend::macro::GeneratedModulePartPlaceholder`。
- 新增 `frontend::macro::ExpansionSourceMapPlaceholder`。
- 新增 `frontend::macro::EarlyItemExpansionSummary`。
- 新增 `frontend::macro::EarlyItemExpansionResult`。
- 新增 `frontend::macro::EarlyItemExpansionDisposition`。
- 新增 `expand_early_item_macros_noop()`。
- 新增 `early_item_expansion_fingerprint()`。
- 新增 `summarize_early_item_expansion()` 和 `dump_early_item_expansion()`。
- 新增 `aurex_macro` target。
- 新增 driver pipeline stage `early_item_macro_expand`，profile name 为 `macro.expand_items`。
- `FrontendPipeline::load_modules()` 现在在 module loading / AST combine 之后、sema 之前运行 no-op early item expansion。
- 每个 parsed item attribute 都会生成 deterministic token-tree fingerprint 和 query-key fingerprint。
- `derive` attribute 标记为 `builtin_derive_passthrough`，继续兼容内建 derive path。
- 非 `derive` item attribute 标记为 `blocked_unimplemented_attribute`。
- 每个带 attribute 的 source module part 都有 `SourceRole::generated` / `ModulePartKind::generated` placeholder。
- 每个 macro input 都有 source-map placeholder，但 `real_source_map=false` 且 `debug_trace_available=false`。
- validation 固定 M21d no-op 边界：generated part 不能 parsed、不能 merged、不能 produced user-generated code。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- generated module part parse / merge。
- 真实 hygiene resolution。
- 真实 expansion source map。
- `--emit-expanded` 或 macro trace CLI。

## M21c Early Item Macro Expansion Plan

当前版本把 M21b 的 item attribute token-tree surface 接到 query-level early item expansion facts。该阶段仍不执行
宏、不生成用户代码，也不引入标准库。

新增或固定：

- 新增 `MacroExpansionFact`。
- 新增 `MacroExpansionSummary`。
- 新增 `MacroExpansionPlan`。
- 新增 `MacroExpansionFactKind`。
- 新增 `MacroExpansionStage`。
- 新增 `MacroExpansionPolicy`。
- 新增 `m21c_macro_expansion_plan_baseline()`。
- 新增 `is_valid_m21c_macro_expansion_plan()`。
- 新增 `macro_expansion_plan_fingerprint()`。
- 新增 `summarize_macro_expansion_plan()` 和 `dump_macro_expansion_plan()`。
- 固定 `attribute_token_tree_input` 事实，把 `ItemNode::attributes` / `AttributeTokenDecl` 作为宏输入面。
- 固定 `builtin_derive_passthrough` 事实，保持 `#[derive(Copy, Eq, Hash)]` 兼容旧 `DeriveDecl` 语义。
- 固定 `early_item_expansion_query_key` 事实，要求真实展开使用 macro definition identity、attached item stable key 和 token-tree fingerprint。
- 固定 `generated_module_part_noop` 事实，要求未来生成代码使用 `SourceRole::generated` 和 `ModulePartKind::generated`。
- 固定 `expansion_source_map_stub` 事实，为 expansion origin / debug trace 留出接口。
- 固定 `unimplemented_item_attribute_blocker` 事实，作为 sema 非 `derive` attribute blocker 的消息来源。
- 固定 `external_procedural_macro_blocked` 事实，external procedural macro 继续留到 future sandbox 阶段。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- 真实 hygiene resolution。
- 真实 expansion source map。
- 真实 generated module part parse / merge。
- `--emit-expanded` 或 macro trace CLI。

## M21b AttributeDecl / Token Tree Surface

当前版本把 M21a 选定的 token tree / attribute surface 落到 frontend。该阶段仍不实现完整 macro/proc-macro、
不生成用户代码，也不引入标准库。

新增或固定：

- 新增 `syntax::AttributeDecl`。
- 新增 `syntax::AttributeTokenDecl`。
- 新增 `syntax::AttributeTokenTreeGroupKind`。
- 新增 `ItemNode::attributes`。
- 所有 item compact payload 保存通用 attributes。
- `ItemNodeList` copy / move / detach / materialize 保留 attributes。
- parser 接受 `#[name(...)]`、`#[name[...]]`、`#[name{...}]` 形式的 item attribute token tree。
- token tree 以 flat token stream 保存 token kind、text、range、group kind 和 depth。
- `#[derive(...)]` 同时保存为通用 `AttributeDecl` 和兼容 `DeriveDecl`。
- AST dump 显示 `#[attr <name> tokens=N ...]`，并保留旧 `#[derive(...)]` 输出。
- sema 对非 `derive` item attribute 报错，明确宏展开尚未实现。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- hygiene / origin mark。
- expansion source map / debug trace。
- generated module part。
- `--emit-expanded` 或 macro trace CLI。

## M21a Macro System Design Gate

当前版本开启宏系统主线，但只完成 design gate，不实现完整 macro/proc-macro、不生成用户代码，也不引入标准库。
M21a 用 query facts 固定后续宏系统的边界：宏输入输出必须使用 token tree / attribute surface，不做文本替换；
宏展开必须带 hygiene、source map、debug trace 和 declared generated names；宏展开必须接入 query/cache；
第一条实现路线选择 attached item codegen / derive codegen 地基。

新增或固定：

- 新增 `MacroDesignGate`。
- 新增 `MacroDesignCandidate`。
- 新增 `MacroDesignImpactSummary`。
- 新增 `MacroDesignCapability`、`MacroDesignGateStage`、`MacroDesignPolicyDecision`。
- 新增 `m21a_macro_design_gate_baseline()`。
- 新增 `is_valid_m21a_macro_design_gate()`。
- 新增 `macro_design_gate_fingerprint()`。
- 新增 `summarize_macro_design_gate()` 和 `dump_macro_design_gate()`。
- 固定 `token_tree_and_attribute_surface` 为 M21 后续实现入口。
- 固定 `hygienic_name_resolution` 为必需地基。
- 固定 `expansion_source_map_and_debug_trace` 为必需地基。
- 固定 `query_backed_incremental_expansion` 为必需地基。
- 固定 `attached_item_codegen_surface` 为第一条真实代码生成主线。
- 将 `typed_expression_macro_boundary` 标为 future stage。
- 将 `external_procedural_macro_sandbox` 标为 blocked dependency，必须等待 process sandbox / manifest /
  permission / implementation fingerprint 设计。

仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户可执行 external procedural macro。
- typed expression macro。
- macro-generated user code lowering。
- 宏生成 `module` / `import` / `pub use`。
- 宏绕过 sema、borrow checking、visibility、trait solver 或 unsafe gate。

## M20g Default And Named Call Arguments Closure

当前版本收口默认参数和命名参数。该阶段不引入标准库，也不改变 ABI 层的函数签名；默认值和命名实参是
source-level call sugar，sema 会把 call-site 参数归一化为 checked `ordered_args`，后续 lowering 和分析只消费
归一化顺序。

新增或固定：

- 普通函数参数可以声明默认值：`fn f(a: i32, b: i32 = 1) -> i32`。
- inherent method 参数可以声明默认值，receiver 仍按第一个 `self` 参数处理。
- 泛型函数和 owner generic method 的默认参数可在实例化调用中使用。
- 普通函数调用可使用命名参数：`f(a: 1, b: 2)`。
- positional 参数可以出现在 named 参数之前：`f(1, c: 3)`。
- named 参数按目标函数参数名重排，checked binding 保存 `ordered_args`。
- 缺失的默认参数由声明上的默认表达式填充，并按该参数类型做期望类型分析。
- trait method / static trait dispatch / dyn method dispatch 在有参数名元数据时支持命名参数。
- IR lowering 使用 checked `ordered_args`，因此 native execution 按参数声明顺序传参。
- borrow summary、body flow graph、body loan precheck、place-state precheck、move analysis 和 borrow escape /
  lambda capture 扫描都使用 checked `ordered_args`，命名重排不会破坏 borrow / move 语义。
- checked dump 会显示带默认值的参数和 call binding 的 `ordered_args=[...]`，便于 query/tooling 侧检查。

当前诊断边界：

- required 参数不能出现在 default 参数之后。
- named 参数之后不能再写 positional 参数。
- 未知 named 参数、重复 named 参数和缺失 required 参数会分别诊断。
- named 参数不支持 enum constructor、function value / lambda 间接调用和 variadic C call。
- 默认参数不支持 C ABI / variadic 函数。
- trait requirement 上的默认参数暂不支持；trait impl / method call 可以使用命名参数。
- 默认表达式当前不把前序参数放进作用域，`fn f(a: i32, b: i32 = a)` 仍不是本阶段能力。

仍不实现：

- 标准库。
- ABI 级 optional/default parameter metadata。
- overload resolution 或多重候选中的 named argument disambiguation。
- function value / lambda 的命名参数和默认参数调用。
- enum constructor 的命名 payload 参数。
- 默认表达式引用前序参数、comptime default evaluation 或依赖默认值。

## M20f Struct Field Reference Borrow Closure

当前版本收口结构体字段引用借用。该阶段不新增字段访问语法；`record.field` 已存在，M20f 固定的是它作为
safe reference source、borrow checker place projection 和 IR address lowering 的完整行为。

新增或固定：

- `&record.field` 可作为 shared field borrow。
- `&mut record.field` 可作为 mutable field borrow，但 base place 必须可写。
- 字段 borrow 复用 M7 `PlaceInfo` 和 projection-aware loan facts，不新增 ad hoc escape 规则。
- 不同已知 struct 字段的 active loans 可分离；借用 `pair.left` 时写入 `pair.right` 可以通过。
- 同一字段写入会和 active field borrow 冲突，并报告 loan creation、invalidating action 和 later carrier use。
- 整个 parent overwrite 会和任一 active child field borrow 冲突。
- local struct field reference 不能作为函数返回值逃逸。
- address-of field projection lowering 到 IR `field_addr`，并保留字段名投影信息。
- 新增独立集成测试文件 `tests/gtest/integration/struct_field_reference_tests.cpp`，避免继续扩大既有
  `regression_tests.cpp`。

仍不实现：

- 新字段访问语法。
- 标准库。
- resource field `take` / `swap` / partial-overwrite helper。
- partial move 的完整用户语义。
- 完整 macro / proc-macro 或用户自定义 derive。

## M20e Builtin Derive Attribute Closure

当前版本补齐第一批编译器内建 derive 属性。`#[derive(Copy, Eq, Hash)]` 可写在 `struct` 和
`enum` 声明上，parser/AST 会保留 item attribute，sema 会把满足条件的 derive 降低为 checked capability
facts，checked dump 会展示对应 `derives=...`。

新增或固定：

- 新增 `#` punctuator 和 `#[...]` item attribute 解析入口。
- 当前只支持 `derive` item attribute；未知 item attribute 会被 parser 诊断。
- 当前只支持 `Copy`、`Eq` 和 `Hash` 三个 derive capability。
- derive 目标只允许 `struct` / `enum`；写在 `fn`、`type`、`trait`、`impl` 等 item 上会被 sema 拒绝。
- 重复 derive capability 会诊断，并给出 previous note。
- `Eq` / `Hash` 派生记录 checked capability fact，可被 `where T: Eq + Hash` 消费。
- `Copy` 派生仍通过 resource semantics 判定，不能绕过非 `Copy` 字段、enum payload 或 `impl Drop` custom
  destructor。
- 泛型 struct/enum 的 derive 是条件性事实：模板声明阶段只检查属性名字和重复项；具体实例化后，组件类型满足能力时才记录该实例 derived fact。
- sema pipeline 会先验证 `impl Drop` 析构器事实，再分析 derive，保证 `#[derive(Copy)]` 能看到 custom destructor。

仍不实现：

- 完整 macro / proc-macro 系统。
- 用户自定义 derive。
- `Clone`、`Ord` 或标准库 trait 派生。
- Eq/Hash 运行时函数、operator overload 或 hash API。
- 标准库。

## M20 函数式和捕获闭包核心子集

当前版本补齐 M20 函数式和闭包核心能力：无捕获 lambda 字面量可以作为一等 `fn(...) -> T`
薄函数值；捕获闭包支持按值捕获非泛型依赖、非 borrowed-view 的 `Copy` 外层局部或参数，并 lowering 为编译器生成的匿名
environment record 和 hidden-env thunk。

新增或固定：

- `fn(x: T) -> U => expr` 表达式体 lambda。
- `fn(x: T) -> U { ... }` 块体 lambda。
- lambda 参数类型和返回类型必须显式标注。
- 无捕获 lambda lowering 为内部匿名函数，表达式值为 `function_ref`。
- 函数值间接调用、作为参数传递、作为返回值返回继续复用既有 `fn(...) -> T` 规则。
- 捕获闭包 lowering 为内部匿名环境 record，调用时把环境地址作为隐藏参数传给内部 thunk。
- 捕获闭包支持局部存储、直接调用、嵌套捕获、match guard 捕获使用，以及由函数 return inference 推断后返回。
- 捕获闭包不是 `fn(...) -> T`，不能赋给薄函数指针；只有无捕获 lambda 可作为薄 `fn` 值。
- 捕获非 `Copy` 值会报 `capturing a non-Copy value in a closure is not supported yet`。
- 捕获 borrowed-view 值会报 `capturing a borrowed-view value in a closure is not supported yet`。
- 捕获 generic-dependent 值会报 `capturing a generic-dependent value in a closure is not supported yet`。

仍不实现：

- 标准库或函数式库 adapter。
- allocator/runtime helper。
- heap/allocator closure box。
- shared / mutable / consuming capture mode。
- generic closure environment ABI。
- `Fn` / `FnMut` / `FnOnce` 能力。
- borrowed closure environment escape 求解。
- 非 `Copy` 捕获的 dropck/place-state/lifetime 规则。

## M20d Runtime Lowering ABI Design Closure

M20d 已完成 runtime lowering ABI design closure。M20d 不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop runtime，不做 backend
runtime helper call，也不做 executable runtime ABI lowering。M20d 的目标是把 M20c 的 compiler-owned drop /
allocator identity prerequisites 继续推进成 runtime lowering ABI 设计事实。

M20d 新增或固定：

- `OwnedDynRuntimeLoweringAbiGate`。
- `OwnedDynRuntimeLoweringAbiFact`。
- `OwnedDynRuntimeLoweringAbiSummary`。
- `OwnedDynRuntimeLoweringAbiFactKind`、`OwnedDynRuntimeLoweringAbiStage` 和
  `OwnedDynRuntimeLoweringAbiPolicy`。
- `m20d_owned_dyn_runtime_lowering_abi_gate_baseline()`。
- `owned_dyn_runtime_lowering_abi_gate_fingerprint()`、summary、dump 和 validation。
- `ir::owned_dyn_runtime_lowering_abi_gate(const Module&)`。
- `runtime_abi_descriptor_key` 和 `backend_helper_identity_key`。
- M20d facts 对 embedded M20c drop / allocator identity gate 的 key consistency validation。

M20d validation 明确拒绝：

- 删除 M20c gate 引用或让 embedded M20c fingerprint 漂移。
- M20d facts 的 drop identity、allocator identity、prototype identity set 或 prototype count 与 M20c gate 不一致。
- runtime ABI descriptor key 或 backend helper identity key 为空、漂移或互相重合。
- 把 backend helper 标成 callable。
- 把 executable runtime 标成 implemented。
- 把 standard library、`Box<dyn Trait>`、owning dyn user value、allocator API、runtime lowering、
  backend helper 或 dynamic Drop runtime 标记成已实现。

下一阶段可以进入标准库 / owning dyn runtime surface 的设计或实现入口评估；该阶段必须显式建立在
M20a-M20d facts 之上，不能把 M20d 误读成已经实现 runtime ABI lowering。

## M20c Drop / Allocator Identity Prerequisite Gate

M20c 已完成 drop / allocator identity prerequisite gate。M20c 不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop runtime，不做 backend
runtime helper call，也不做 runtime ABI lowering。M20c 的目标是把 M20b 的 compiler-owned owned dyn handle
prototype 继续推进成可验证的 drop / allocator identity facts。

M20c 新增或固定：

- `OwnedDynDropAllocatorIdentityGate`。
- `OwnedDynDropAllocatorIdentityFact`。
- `OwnedDynDropAllocatorIdentitySummary`。
- `OwnedDynDropAllocatorIdentityFactKind`、`OwnedDynDropAllocatorIdentityStage` 和
  `OwnedDynDropAllocatorIdentityPolicy`。
- `m20c_owned_dyn_drop_allocator_identity_gate_baseline()`。
- `owned_dyn_drop_allocator_identity_gate_fingerprint()`、summary、dump 和 validation。
- `ir::owned_dyn_drop_allocator_identity_gate(const Module&)`。
- `OwnedDynObjectLayoutPrototype::erased_drop_identity_key` 和
  `OwnedDynObjectLayoutPrototype::allocator_identity_key`。
- IR dump、layout ABI fingerprint、verifier 和 M20b shape adapter 对 drop / allocator identity key 的消费。

M20c validation 明确拒绝：

- drop identity key 为空、allocator identity key 为空，或两者相同。
- IR module 中重复 drop identity key 或重复 allocator identity key。
- erased drop runtime slot 或 allocator runtime slot 不再是 blocked sentinel。
- 把 standard library、`Box<dyn Trait>`、owning dyn user value、allocator API、runtime lowering、
  backend helper 或 dynamic Drop runtime 标记成已实现。
- 让 query summary、fingerprint 或 M20b IR shape gate 引用与当前 facts 漂移。

下一阶段已进入 M20d Runtime Lowering ABI Design Closure；M20d 仍不直接实现标准库 API。

## M20b Owned Dyn IR Shape Prototype Gate

M20b 已完成 owned dyn IR shape prototype gate。M20b 不实现标准库、不实现 `Box<dyn Trait>`、不实现 allocator
API、不实现 owning dyn 用户值、不生成 dynamic Drop runtime，不做 backend runtime helper call，也不做 runtime
ABI lowering。M20b 的目标是把 M20a admission gate 要求的 owned object layout prerequisite 落成 compiler-owned
IR 形状和 verifier/query 不变量。

M20b 新增或固定：

- `OwnedDynObjectLayoutPrototype` 和
  `OwnedDynObjectLayoutPrototypePolicy::compiler_owned_handle_metadata_v1`。
- `ir::Module::owned_dyn_object_layout_prototypes` 及其 copy/move/clone/reserve 支持。
- two-field owned dyn handle prototype：field 0 是 `*mut u8` erased payload pointer，field 1 是
  `*const u8` borrowed vtable pointer。
- blocked erased-drop slot 和 allocator slot，不提供可执行 runtime identity。
- `owned_dyn_object_layout_prototype` IR dump 和 layout ABI fingerprint。
- IR verifier 对 object key/type、pointer type、field count/index、blocked runtime slot 和 stdlib/runtime blocker
  matrix 的硬校验。
- `OwnedDynIrShapePrototypeGate`、六类 shape facts、summary/dump/fingerprint 和 validation。
- `m20b_owned_dyn_ir_shape_prototype_gate_baseline()` 和
  `ir::owned_dyn_ir_shape_prototype_gate(const Module&)`。

M20b validation 明确拒绝：

- object key/type 漂移、缺失/重复 symbol 或重复 object key。
- data pointer 不是 `*mut u8`，或 vtable pointer 不是 `*const u8`。
- two-field handle shape、field index 或 blocked runtime slot 漂移。
- 把 standard library、`Box<dyn Trait>`、owning dyn user value、allocator API、runtime lowering、
  backend helper 或 dynamic Drop runtime 标记成已实现。
- 让 query summary、fingerprint 或 M20a admission gate 引用与当前 facts 漂移。

下一阶段建议进入 M20c Drop / Allocator Identity Prerequisite Gate；M20c 仍不应直接实现标准库 API。

## M20a Owned Dyn Runtime Admission Design Gate

M20a 已完成 owned dyn runtime admission design gate。M20a 不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop dispatch，不做 backend
runtime helper call，也不做 runtime ABI lowering。M20a 的目标是把 M17/M18/M19 的 runtime facts、boundary gate
和 IR/verifier facts 汇总成后续 owned dyn runtime 的准入门禁。

M20a 新增或固定：

- `OwnedDynRuntimeAdmissionGate`。
- `OwnedDynRuntimeAdmissionFact`。
- `OwnedDynRuntimeAdmissionSummary`。
- `OwnedDynRuntimeAdmissionCapability`。
- `OwnedDynRuntimeAdmissionStage`。
- `OwnedDynRuntimeAdmissionPolicy`。
- `m20_owned_dyn_runtime_admission_gate_baseline()`。
- `owned_dyn_runtime_admission_gate_fingerprint()`、summary、dump 和 validation。
- 对 M17 runtime facts、M18 boundary gate 和 M19 IR/verifier facts fingerprint 的稳定引用。

M20a validation 明确拒绝：

- 删除 M17/M18/M19 baseline 引用或让嵌入 fingerprint 漂移。
- 把 standard library API、`Box<dyn Trait>` surface、allocator API、owning dyn user value、runtime ABI lowering、
  backend runtime helper call 或 dynamic Drop runtime 标记成已实现。
- 把 borrowed dyn ABI 改成携带 owning/drop metadata。
- 跳过 owned layout、erased drop identity 或 allocator identity，直接进入 runtime lowering 或 `Box<dyn Trait>`。

下一阶段建议进入 M20b Owned Dyn IR Shape Prototype Gate；M20b 仍不应直接实现标准库 API。

## M19 Dyn Ownership Runtime IR / Verifier Preparation

M19 已完成 dyn ownership runtime 的 IR / verifier preparation。M19 不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop dispatch，不做 backend
runtime helper call，也不做 runtime ABI lowering。M19 的目标是把 M18 lowering design gate 中记录的 future
IR/verifier prerequisites 落成 verifier-visible 的 IR 形状、query facts 和负例矩阵。

M19 新增或固定：

- `DynOwnershipRuntimeIrVerifierFact`。
- `DynOwnershipRuntimeIrVerifierSummary`。
- `FunctionDynOwnershipRuntimeIrVerifierFacts`。
- `m19_dyn_ownership_runtime_ir_verifier_baseline()`。
- `dyn_ownership_runtime_ir_verifier_facts_fingerprint()`、summary、dump 和 validation。
- `function_dyn_ownership_runtime_ir_verifier_facts()` 函数级 IR collector。
- `TraitObjectVTableLayout::destructor_slot_blocked`，进入 clone/copy、dump、layout ABI fingerprint 和 verifier。
- `CleanupAbiPolicy::dynamic_erased_drop_blocked`，作为 verifier 负例哨兵，明确不代表可执行 dynamic Drop runtime。
- IR verifier 对 borrowed vtable destructor-free drift 和 dynamic erased drop cleanup policy 的硬拒绝。

M19 validation 明确拒绝：

- 把 borrowed vtable 改成带 destructor slot 的 owning/drop vtable。
- 把 dynamic erased drop runtime 标记成已实现。
- 把 standard library、runtime ABI lowering、owning dyn user value、allocator API 或 `Box<dyn Trait>` 标记成已实现。
- 删除 erased drop identity prerequisite、allocator identity prerequisite 或 owned dyn object placeholder blocker。
- 让 summary、stable fingerprint 或 function-level facts 与当前 facts 漂移。

下一阶段建议进入 M20 标准库/owning dyn runtime design gate；M20 才适合开始设计 `Box<dyn Trait>`、allocator API、
owned object layout、dynamic Drop metadata 和 runtime lowering 的实际执行边界。

## M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate

M18 已完成 dyn ownership runtime boundary hardening / lowering design gate。M18 不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop dispatch，也不做 runtime ABI
lowering。M18 的目标是把 M17 facts 接到 query/cache/tooling/reuse/workspace 边界，并把 future IR/verifier/runtime
lowering prerequisites 固定为可验证的 project-level facts。

M18 新增或固定：

- `DynOwnershipRuntimeBoundaryGate`。
- `DynOwnershipRuntimeBoundaryCheckpointFact`，覆盖 `query_cache_projection`、`tooling_projection`、
  `reuse_boundary`、`ir_verifier_planning`、`borrowed_abi_guard` 和 `runtime_lowering_gate`。
- `DynOwnershipRuntimeLoweringDesignGateFact`，记录 future IR owned object placeholder、erased drop identity、
  allocator identity、borrowed-vtable destructor verifier guard、missing-erased-receiver guard 和
  stdlib-before-runtime-lowering guard。
- `DynOwnershipRuntimeBoundarySummary`，统计 M17 reference、standard-library blocker、runtime-lowering blocker、
  `Box` surface blocker、owning dyn user value blocker、allocator API blocker、dynamic-drop blocker、
  borrowed-metadata destructor-free 和 lowering design gate。
- `m18_dyn_ownership_runtime_boundary_gate_baseline()`、
  `dyn_ownership_runtime_boundary_gate_fingerprint()`、
  `dyn_ownership_runtime_boundary_gate_result_fingerprint()`、summary 和 dump。
- `QueryKind::dyn_ownership_runtime_boundary_gate`，使用 `ProjectKey` stable identity，且依赖同一个 `ProjectKey` 的
  `project_graph`。
- Incremental cache subject/order/profile/reuse、IDE semantic fact、tooling session reuse 和 workspace index 投影。

M18 validation 明确拒绝：

- 把 standard library、allocator API、`Box` surface、owning dyn user value 或 runtime ABI lowering 标记成已实现。
- 把 dynamic Drop dispatch 标记成已实现。
- 把 destructor slot 加进 borrowed vtable 或让 borrowed vtable metadata 承担 owning/drop ABI。
- 让 M18 provider 依赖非 `project_graph` query、依赖其他项目的 project graph，或带多个 project graph 依赖。
- 让 summary、stable fingerprint 或嵌入的 M17 facts 与当前 facts 漂移。

下一阶段建议进入 M19 Dyn Ownership Runtime IR / Verifier Preparation；M19 仍应先补 verifier-visible IR 形状和负例矩阵，
不应直接实现标准库 API。

## M17 Dyn Ownership Runtime Preparation

M17 已完成 dyn ownership runtime preparation 的 compiler/query/tooling 事实边界。M17 不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop dispatch，也不做 runtime ABI
lowering。M17 的目标是把 future owning dyn、erased drop glue、allocator 和 cleanup/dropck runtime boundary 先变成
稳定 facts、summary、dump、fingerprint 和 validation。

M17 新增或固定：

- `DynOwnershipRuntimeFacts`。
- `DynOwnedContainerBoundaryFact`，固定 `owning_dyn_container_v1` 和 `owning_dyn_metadata_v1`。
- `DynErasedDropGlueBoundaryFact`，固定 `dynamic_drop_metadata_v1`、`erased_drop_glue_identity_fact`、
  `dynamic_drop_slot_layout_fact` 和 `dropck_erased_receiver_fact`。
- `DynAllocatorBoundaryFact`，固定 `allocator_placement_policy_v1`、`allocator_metadata_v1`、
  `allocator_identity_fact`、`allocator_placement_policy_fact` 和 `owned_dyn_deallocation_policy_fact`。
- `DynCleanupDropckBoundaryFact`，把 cleanup/resource/dropck facts 桥接到 future erased drop boundary。
- `DynOwnershipRuntimeSummary`，统计 standard-library blocker、runtime-lowering blocker、`Box` surface blocker、
  allocator API blocker、dynamic-drop blocker、borrowed-vtable destructor-free 和 cleanup/dropck bridge。
- `dyn_ownership_runtime_facts_fingerprint()`、`summarize_dyn_ownership_runtime_facts()`、
  `dump_dyn_ownership_runtime_facts()`。
- `m17_dyn_ownership_runtime_preparation_baseline()` 和
  `is_valid_m17_dyn_ownership_runtime_preparation_baseline()`。

M17 validation 明确拒绝：

- 把 standard library、allocator API、`Box` surface、owning dyn user value 或 runtime ABI lowering 标记成已实现。
- 把 dynamic Drop dispatch 标记成已实现。
- 把 destructor slot 加进 borrowed vtable 或让 `borrowed_methods_only_v1` 承担 owning/drop ABI。
- 让 summary 或 stable fingerprint 与当前 facts 漂移。

下一阶段建议进入 M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate；M18 仍应先补 runtime-boundary
设计和 query/tooling hardening，不应直接实现标准库 API。

## M16 Const Generic Frontend / Query / Sema Check-Only

M16 已完成 const generic 的 frontend / query / sema check-only 子集。M16 不实现标准库、不实现 runtime owning
dyn、不实现 `Box<dyn Trait>`、不生成 dynamic Drop dispatch，也不把 unresolved const-param array lowering 到 runtime
ABI。M16 的范围是把 M15 选定的 typed scalar const generic 路线变成当前可写、可解析、可检查、可进入 query identity
的前端能力。

M16 新增或固定：

- `syntax::GenericParamKind::const_`。
- `syntax::GenericArgKind::{type,const_expr}`。
- `syntax::ArrayLengthKind::{literal,const_expr}`。
- `sema::ArrayLengthKind::const_param` 和 `ArrayLengthInfo`。
- mixed ordered generic args，同时保留 legacy `type_args` 兼容旧调用面。
- `GenericInstanceKey::const_args`，const argument fingerprint 会进入 generic instance identity。
- const generic parameter query key：`query::GenericParamKind::const_`。
- const parameter environment binding：函数体内 `N` 可解析为当前 const generic value。
- generic template AST span 覆盖 const parameter declared type、mixed const argument expression 和 `[N]T`
  array length expression。

当前可写示例：

```aurex
struct ArrayView[T, const N: usize] {
    value: T;
}

fn len[T, const N: usize](value: [N]T) -> usize {
    return N;
}

fn main() -> usize {
    let value: ArrayView[i32, 4] = ArrayView[i32, 4] { value: 1 };
    return len[i32, 4]([1]);
}
```

当前规则：

- const parameter 必须写成 `const Name: Type`。
- const parameter type 只接受 integer、`bool` 和 `char` 标量。
- const argument 只接受 scalar literal 或当前 generic context 中的 const parameter name；const parameter name
  转发时必须和目标 const parameter type 一致。
- type parameter 和 const parameter 按 ordered generic parameter list 检查，类型/值实参不能混用。
- `[N]T` 是 check-only array length 集成点；它可以进入 canonical identity / fingerprint，但不进入 runtime layout
  lowering。

M16 明确不支持：

- untyped const parameter。
- generic const arithmetic，例如 `N + 1`。
- user function comptime evaluation。
- const where predicate、const associated value、dyn const equality dispatch。
- runtime ABI / LLVM lowering for unresolved const-param arrays。
- 标准库 const generic API、`Box`、allocator 或 owning dyn。

## M15 Advanced Dyn Ownership / Const Generic Boundary Design Baseline

M15 已完成 advanced dyn ownership/runtime boundary 与 const generic boundary 的 design baseline。M15 只落
query design gate、validation、summary、dump、fingerprint、文档和测试；不实现标准库、不实现 owning dyn runtime、
不实现 `Box<dyn Trait>`、不生成 dynamic Drop dispatch、不打开用户可用 const generic 语法。

M15 新增或固定：

- `m15_dyn_advanced_design_gate_baseline()`。
- `m15_const_generic_design_gate_baseline()`。
- `ConstGenericCapability::{typed_const_parameter_surface, canonical_const_argument_identity,
  generic_instance_key_integration, array_length_type_integration, const_expression_evaluation_subset,
  trait_predicate_and_dyn_boundary}`。
- `ConstGenericGateStage::{research_only, design_gate, ready_for_implementation, blocked_by_dependency,
  future_stage}`。
- `ConstGenericPolicyDecision::{rejected, selected_m15_frontend_query_path, requires_comptime_engine,
  requires_trait_solver_extension, requires_runtime_or_std_boundary}`。
- `const_generic_design_gate_fingerprint()`、summary 和 dump。

Dyn ownership 侧，M15 把 M10/M11/M12/M13/M14 的 borrowed dyn 能力全部标记为 completed release baseline；
`owning_dyn`、`dynamic_drop_dispatch` 和 `allocator_policy` 进入 design gate，但仍分别受 standard library stage、
runtime stage 和 future resource surface 阻塞。M15 明确不把 destructor slot 加到 borrowed vtable，也不把 borrowed
`{data*, vtable*}` 当成 owner。

Const generic 侧，M15 选择后续实现路线为 typed scalar const generic：

```aurex
struct ArrayView[T, const N: usize] {
    data: *const T;
}
```

第一批实现目标应是 parser/AST/query/sema check-only、canonical const value identity、generic instance key const
argument、以及 `[N]T` array length 集成。M15 不支持 `N + 1` 这类 generic const arithmetic，不支持 user
function comptime evaluation，不支持 const where predicate、const associated value 或 dyn const equality dispatch。

## M14 Borrowed Dyn View Path Inference / Dispatch Release

M14 已完成 borrowed dyn view path inference / dispatch release。M14 在 M13 的 explicit
composition-to-supertrait runtime chain 上打开两个受限隐式路径：

```aurex
let parent: &dyn Parent = view;
return view.parent();
```

其中 `view` 可以是 `&dyn (Child + Debug)` 或 `&mut dyn (Child + Debug)`。成功条件是 source composition 中
只有一个 principal 能沿 checked supertrait path 到达 target supertrait；否则仍拒绝并要求用户写
`dynproject[SourcePrincipal, TargetSupertrait](view)`。

M14 新增或固定：

- `BorrowedDynViewPathUse::{explicit_projection, expected_type_projection, method_dispatch}`。
- `BorrowedDynViewPathFact`，记录 source principal、target object、composition projection fingerprint、
  supertrait edge fingerprint、borrow kind、source/projected/target view name 和 method dispatch step。
- `PrincipalSetCompositionSummary::{borrowed_view_path_count, borrowed_view_path_dispatch_count,
  borrowed_view_path_expected_projection_count}`。
- `principal_set_composition_facts_fingerprint()`、summary 和 dump 均混入 borrowed view path facts。
- direct `view.parent()` lowering 为 `trait_object_composition_project` + `trait_object_upcast` + ordinary
  `vtable_slot`。
- expected-type projection lowering 为 `trait_object_composition_project` + `trait_object_upcast`。
- ambiguous multi-principal path 继续诊断，不做隐式 source-principal 猜测。

M14 继续保持 no-std/compiler runtime core 边界：不实现标准库、不实现 owning dyn、不实现 `Box<dyn Trait>`、
不实现 allocator API/policy、不实现 dynamic Drop dispatch、不实现 trait-object destructor ABI、不实现 bare
`dyn A + B` syntax，也不新增 runtime metadata policy。

## M13d Borrowed Composition-To-Supertrait Hardening / Release Closure

M13d 已完成 borrowed composition-to-supertrait explicit projection 的 release closure。M13d 不改变用户语法：

```aurex
dynproject[SourcePrincipal, TargetSupertrait](view)
```

M13d 新增或固定：

- `FunctionDynAbiFacts::composition_supertrait_chains`，把
  `trait_object_composition_project` + `trait_object_upcast` 归纳成 query/cache/tooling 可见的 runtime chain。
- `function_dyn_abi_facts_fingerprint()` 和 `lower_function_ir_result_fingerprint()` 会响应 chain 的 target、
  source principal、borrow kind、layout 和 supertrait edge drift。
- IDE semantic fact / hover 展示 `composition_supertrait_chains=N`、source composition view、projected principal
  view、target supertrait view、`principal_set_metadata_v1` 与 `supertrait_vptr_metadata_v1`。
- IR verifier negative matrix 覆盖缺失 upcast object、错误 supertrait edge、source/target layout drift 和
  projection principal drift。
- 文档测试和 coverage gate 将 M13 收口为 release baseline。

M13d 继续保持 no-std/compiler runtime core 边界：不实现标准库、不实现 owning dyn、不实现 `Box<dyn Trait>`、
不实现 allocator API/policy、不实现 dynamic Drop dispatch、不实现 trait-object destructor ABI，也不实现 bare
`dyn A + B` syntax 或隐式 composition-to-supertrait direct call。

## M13c Borrowed Composition-To-Supertrait IR / Backend Runtime

M13c 已完成 borrowed composition-to-supertrait explicit projection 的 IR/backend runtime 子集。M13b 引入的
用户语法保持不变：

```aurex
dynproject[SourcePrincipal, TargetSupertrait](view)
```

M13c 新增或固定：

- `dynproject[...]` 在 sema 成功后不再停留在 check-only；lowering 会生成
  `trait_object_composition_project` + `trait_object_upcast`。
- `trait_object_composition_project` 先从 `&dyn (A + B)` / `&mut dyn (A + B)` 的
  `principal_set_metadata_v1` 中取出 source principal vtable。
- `trait_object_upcast` 再沿 source principal 的 `supertrait_vptr_metadata_v1` edge 取出 target supertrait vtable。
- data pointer、borrow kind 和 origin 仍保持 borrowed view 语义；不分配、不拥有对象、不引入 cleanup/drop runtime。
- Function dyn ABI facts 会同时暴露 composition projection descriptor 和 upcast descriptor；IR dump、LLVM output 和
  native execution tests 已覆盖该路径。
- 仍拒绝隐式 `let parent: &dyn Parent = view;`，也仍拒绝 `view.parent()` 直接穿过 composition 到 supertrait。

M13c 继续保持 no-std/compiler runtime core 边界：不实现标准库、不实现 owning dyn、不实现 `Box<dyn Trait>`、
不实现 allocator API/policy、不实现 dynamic Drop dispatch、不实现 trait-object destructor ABI，也不实现 bare
`dyn A + B` syntax。M13d 已完成 hardening/release closure。

## M13b Borrowed Composition-To-Supertrait Frontend / Query / Sema Check-Only

M13b 已完成 borrowed composition-to-supertrait explicit projection 的 frontend / query / sema check-only 子集。
当前用户可写：

```aurex
dynproject[SourcePrincipal, TargetSupertrait](view)
```

示例：

```aurex
trait Parent { fn parent(self: &Self) -> i32; }
trait Child: Parent { fn child(self: &Self) -> i32; }
trait Debug { fn debug(self: &Self) -> i32; }

fn score(view: &dyn (Child + Debug)) -> i32 {
    let parent: &dyn Parent = dynproject[Child, Parent](view);
    return parent.parent();
}
```

M13b 新增或固定：

- `dynproject` 是 sema 识别的上下文 intrinsic，不是新的 lexer keyword；当前只对未限定
  `dynproject[...]()` 调用形状生效。
- `dynproject[...]` 必须恰好有两个 type arguments 和一个 value argument。
- 第一个 type argument 必须解析为 source principal trait object，且该 principal 必须存在于 source
  `&dyn (A + B)` / `&mut dyn (A + B)` composition 中。
- 第二个 type argument 必须解析为 target supertrait trait object，且必须能从 source principal 的
  direct/transitive supertrait path 到达。
- 成功表达式返回 `&dyn TargetSupertrait` 或 `&mut dyn TargetSupertrait`，borrow kind 来自 source view；
  后续 contextual assignment 可把 `&mut dyn Target` 降级为 `&dyn Target`。
- 成功后记录 `CompositionProjectionFact{kind=composition_to_supertrait}`，summary/dump/fingerprint 暴露
  `supertrait_projections`。
- 负例覆盖 type-arg arity、value-arg arity、source/target 非 trait、argument 非 composition、source 不在
  composition、target 不是 source 的 supertrait、隐式 assignment 以及 direct parent method dispatch。

M13b 继续保持 no-std/compiler frontend 边界：不实现标准库、不实现 owning dyn、不实现 `Box<dyn Trait>`、
不实现 allocator API/policy、不实现 dynamic Drop dispatch、不实现 trait-object destructor ABI、不实现 bare
`dyn A + B` syntax，也不做 IR/backend runtime lowering。`let parent: &dyn Parent = view;` 和
`view.parent()` 仍被拒绝。M13c 已在下一阶段把 checked fact 降低为 runtime 路径。

## M13a Advanced Dyn Remaining Policy Design Baseline

M13a 已完成 advanced dyn 剩余能力的 policy selection 和 query design gate。M13a 选择
**borrowed composition-to-supertrait explicit projection** 作为下一条主线，但本阶段不实现用户语法、不实现
IR/backend runtime，也不改变 M12b 对 `view.parent()` 这类隐式 composition-to-supertrait direct call 的拒绝规则。
设计入口见 [Aurex M13 Advanced Dyn Remaining Policy Design Baseline](m13-advanced-dyn-design.md)。

M13a 新增或固定：

- `DynAdvancedCapability::borrowed_composition_supertrait_projection`
- `DynAdvancedPolicyDecision::composes_existing_metadata_policies`
- `m13a_dyn_advanced_design_gate_baseline()`
- `is_valid_m13a_dyn_advanced_design_gate()`
- required facts：`composition_to_supertrait_projection_fact`、`principal_supertrait_path_fact`、
  `composition_supertrait_ambiguity_fact`、`composition_supertrait_projection_abi_descriptor`
- non-goals：`standard_library_runtime_not_in_m13a`、`new_runtime_metadata_not_in_m13a`、
  `owning_dyn_runtime_not_in_m13a`、`do_not_make_composition_to_supertrait_direct_call_implicit`、
  `do_not_add_new_principal_set_metadata_policy`

M13a 的核心决策是组合已有 `principal_set_metadata_v1` 和 `supertrait_vptr_metadata_v1`，不新增
composition-supertrait metadata policy。owning dyn、`Box<dyn Trait>`、allocator policy 和 dynamic Drop dispatch
继续留到独立阶段。

## M12b Direct Composition Dispatch Hardening / Release Closure

M12b 已完成 M12 direct composition dispatch 的 hardening / release closure。M12 现在收口为 borrowed
principal-set dyn composition direct dispatch release baseline，入口见
[Aurex M12 Direct Composition Dispatch Release Baseline](m12-release-baseline.md)。

M12b 继续保持 no-std/compiler-runtime-core 边界：不实现标准库、不实现 owning dyn、不实现 `Box<dyn Trait>`、
不实现 allocator API/policy、不实现 dynamic Drop dispatch、不实现 trait-object destructor ABI、不实现 bare
`dyn A + B` syntax，也不实现 composition-to-supertrait 隐式多步 direct dispatch。

M12b 当前新增或固定的实现包括：

- Direct composition method call 的 checked binding 保留表面 `receiver_type`，但 receiver access 按投影后的
  `dispatch_receiver_type` reference 计算；这让 tooling、borrow facts 和 checked dump 看到的实际 vtable
  receiver 与 lowering 一致。
- Associated equality direct dispatch 已覆盖：`dyn (Source[Item = i32] + Debug)` 上的 `view.item()` 会选择
  `Source[Item = i32]` principal，并把 return type 替换为 `i32`。
- Direct dispatch 与显式 projection 混用时，composition-to-principal projection fact 和
  `FunctionDynAbiFacts::composition_projections` descriptor 都保持去重。
- Query/cache fingerprint 已固定：projection borrow kind、target principal、target vtable layout 或 target
  reference type 改变时，`function_dyn_abi_facts_fingerprint()` 和
  `lower_function_ir_result_fingerprint()` 都会变化。
- Negative matrix 已补齐 composition-to-supertrait 隐式 direct call rejection；当前 inherited supertrait method
  仍需要显式投影或后续设计，不会从 `&dyn (Child + Debug)` 自动穿过 `Child -> Parent` dispatch。

## M12a Direct Principal-Qualified Composition Method Dispatch

M12a 已完成 borrowed principal-set dyn composition 的 direct dispatch 子集。当前 `view: &dyn (Draw + Debug)`
可以直接调用 `view.draw()` 或 `view.debug()`，前提是该 method 名称在 principal set 中只由一个 principal
提供。该语法不是 composition-wide flattened slot dispatch；它等价于先隐式 projection 到唯一 principal 的
borrowed single-trait view，再复用已有 `vtable_slot` dyn dispatch：

```aurex
fn score(view: &dyn (Draw + Debug)) -> i32 {
    return view.draw() + view.debug();
}
```

M12a 当前新增或固定的实现包括：

- Sema 在 `&dyn (A + B)` / `&mut dyn (A + B)` receiver 上按 principal method namespace 解析 direct method call。
  恰好一个 principal 提供该 method 时，checked binding 的 `dispatch_receiver_type` 指向该 principal object。
- Direct dispatch 会记录 composition-to-principal borrowed projection fact，并在 lowering 中生成
  `trait_object_composition_project` 后继续执行普通 `vtable_slot` dispatch。
- 多个 principal 暴露同名 method 仍被拒绝，诊断为
  `dyn trait composition method `name` is ambiguous across multiple principal traits`；缺失 method 仍走普通
  no visible impl 诊断。
- Receiver mutability 不会被 direct dispatch 绕过：`&dyn (A + B)` 不能调用要求 `&mut Self` 的 principal method；
  `&mut dyn (A + B)` 会保留 mutable borrow kind。
- Focused frontend、IR lowering、IDE/query hover 和 native execution tests 已覆盖 shared/mut direct dispatch、
  ambiguous method、missing method、mutable receiver rejection、composition projection fact、IR project/vtable slot
  lowering 和多 concrete runtime dispatch。

M12a 继续保持 no-std/compiler-runtime-core 边界：不实现标准库、不实现 owning dyn、不实现 `Box<dyn Trait>`、
不实现 allocator API/policy、不实现 dynamic Drop dispatch、不实现 trait-object destructor ABI、不实现 bare
`dyn A + B` syntax，也不实现 chaining composition projection through supertrait upcast 的隐式多步 direct dispatch。
`view.draw()` 只在唯一 principal 自身提供 `draw` 时成立；需要跨 supertrait 的组合调用仍应先显式投影或等待后续设计。

## M11e Principal-Set Composition Hardening / Release Closure

M11e 已完成 principal-set borrowed dyn composition 的 hardening / release closure。M11 现在完整收口为
origin-bound borrowed principal-set dyn composition release baseline：M11a 设计、M11b query facts、M11c
frontend/sema、M11d IR/backend runtime、M11e query/cache/tooling/verifier/docs closure 均已完成。新增 release
入口见 [Aurex M11 Principal-Set Composition Release Baseline](m11-release-baseline.md)。

M11e 继续保持 no-std/compiler-runtime-core 边界：不实现标准库、不实现 owning dyn、不实现 `Box<dyn Trait>`、
不实现 allocator API/policy、不实现 dynamic Drop dispatch、不实现 trait-object destructor ABI、不实现 bare
`dyn A + B` syntax，也不把 composition method call 直接打开为 `combo.method()`。

M11d 已经新增或固定的 runtime 实现包括：

- Sema 支持显式 composition-to-principal projection：`&dyn (A + B) -> &dyn A` 和
  `&mut dyn (A + B) -> &mut dyn A`；shared source 仍不能升级为 mutable target。
- Direct method call on composition 仍被拒绝。当前可执行写法是先投影到 single-principal borrowed dyn view：
  `let draw: &dyn Draw = combo; return draw.draw();`。
- IR 新增 `trait_object_composition_pack` 和 `trait_object_composition_project` value kind，并新增
  `PrincipalSetMetadataLayout` / `PrincipalSetMetadataWitness`，由 verifier 固定 principal-set metadata policy、
  concrete type、composition object type、principal witness coverage 和 projection target。
- Lowering 会把 concrete pointer/reference 到 `&dyn (A + B)` 的 coercion 降成
  `dyn.composition.pack`，把 `&dyn (A + B)` 到 `&dyn A` 的显式 projection 降成
  `dyn.composition.project`。
- LLVM backend 生成 `principal_set_metadata_v1` metadata global，shape 为 `{ [N x ptr] }`；borrowed composition
  runtime view 仍是两个指针 `{data*, metadata*}`，不携带 owner、allocator、drop、size 或 align metadata。
- Runtime projection 从 composition metadata 中按 canonical principal index 加载目标 principal vtable，再构造普通
  `{data*, vtable*}` single dyn view，因此后续 dispatch 复用 M8/M10 已有 vtable slot indirect call 路径。
- Metadata layout 以 `(principal_set_identity, concrete_type)` 为键；同一个 principal set 对不同 concrete type
  生成不同 metadata global。Verifier 还要求 witness index 与 principal-set 类型的 canonical principal 顺序一致。
- Focused frontend、IR lowering、LLVM whitebox 和 native execution tests 已覆盖 shared/mut projection、多个 concrete
  witness layout、metadata pack/project、principal vtable load 和显式 projection 后的 native dispatch。

M11e 当前新增或固定的 hardening 实现包括：

- `FunctionDynAbiFacts` 新增 `principal_sets` 和 `composition_projections`，并通过
  `DynPrincipalSetMetadataAbiDescriptor`、`DynPrincipalSetWitnessAbiDescriptor` 和
  `DynCompositionProjectionAbiDescriptor` 投影 composition runtime facts。
- `DynMetadataPolicy::principal_set_metadata_v1` 进入 dyn ABI facts validation、summary、dump 和 fingerprint。
- lower-function IR query result fingerprint 混入 principal-set metadata/projection counts，避免 cache 复用过期的
  composition runtime facts。
- IDE semantic fact 和 hover 展示 `principal_sets=N`、`composition_projections=N`、首条 composition projection、
  principal index、borrow kind 和 `composition_metadata=principal_set_metadata_v1`。
- IR dyn ABI facts adapter 区分 concrete pack metadata 与 callee-only projection facts：只接收 `&dyn (A+B)` 的函数
  不伪造 concrete-specific principal-set metadata。
- IR verifier 现在要求 `trait_object_composition_project` 携带有效且匹配 target principal 的 `principal_object`。
- Focused query、IR、IDE 和 verifier negative tests 已覆盖 descriptor validation、fingerprint drift、lower-IR
  invalidation、tooling surface、metadata identity drift、duplicate witness index、missing pack metadata、invalid
  projection principal index 和 missing principal object。

M11d/M11e 实际实现比早期 “principal-qualified slot dispatch” 设想更保守：本阶段不新增 direct composition receiver
dispatch syntax，而是先交付显式 projection runtime 和 release-quality facts/tooling closure。这样可以复用
single-trait dyn dispatch，避免在没有稳定 principal-qualified method syntax 前提前 flatten method namespace。

M11 已结束。M12a/M12b 已在此基础上打开并收口 unambiguous direct principal composition dispatch，但仍继续不实现
标准库、owning dyn、`Box<dyn Trait>`、allocator 和 dynamic Drop dispatch；这些能力仍进入独立后续阶段。

## M11c Principal-Set Composition Frontend / Sema Check-Only

M11c 已完成 principal-set borrowed dyn composition 的 frontend/sema check-only 子集。当前用户可写
composition spelling 是 `dyn (A + B)`；它只用于 borrowed dyn composition view，例如
`&dyn (Draw + Debug)` 和 `&mut dyn (Draw + Debug)`。M11c 继续保持 compiler-only/check-only 边界，不实现
标准库、不实现 owning dyn、不实现 `Box<dyn Trait>`、不实现 allocator API/policy、不实现 dynamic Drop
dispatch、不实现 principal-qualified dispatch、不实现 bare `dyn A + B` syntax、不实现 IR/backend runtime。

M11c 当前新增或固定的实现包括：

- Parser/AST 支持 `dyn (A + B)` principal-set composition，单 trait `dyn Trait` 和
  `dyn Trait[Assoc = Type]` 语法保持不变；`dyn ()`、缺少 `+` 和 trailing `+` 都有 focused recovery。
- Type model 新增 principal-set trait object identity 和 canonical principal type list；display 使用
  `dyn (Draw + Debug)`，不会显示成 `dyn (dyn Draw + dyn Debug)`。
- Sema 要求 composition 至少两个 principal、每个 principal 必须是 single dyn trait object、duplicate
  principal 会诊断；associated equality merge 按 associated type 名称合并，冲突会给用户诊断。
- Borrowed concrete reference 可以在所有 principal impl 可见时 coercion 到 borrowed composition view：
  `&T -> &dyn (A + B)` 和 `&mut T -> &mut dyn (A + B)`；shared-to-mut 仍被现有 assignment/coercion 规则拒绝。
- Coercion 会记录每个 principal 的 checked vtable layout、`CompositionWitnessSetFact`、
  `CompositionProjectionFact` 和 generic `CoercionRecord`，但不会把 composition 伪装成 single-trait
  `TraitObjectCoercionFact`。
- Composition method call 在本阶段显式拒绝。存在匹配 principal method 时诊断
  `principal-qualified dispatch is not part of this stage`；缺失 method 时保留普通 no visible impl 诊断。
- `CheckedModule`、checked dump、stable fingerprint 和 `TypeCheckBodyAuthority` 已纳入
  `PrincipalSetCompositionFacts`。
- `ir::lower_ast` 曾在 M11c 对包含 principal-set composition type 的 checked module 返回明确 codegen error，
  固定 check-only 边界，避免后端把 composition 当成普通 `{data*, vtable*}` single-trait dyn runtime；该 guard
  已由 M11d 的 composition pack/project runtime lowering 取代。
- Focused parser、AST、sema、query 和 IR negative tests 已覆盖 positive spelling、invalid principal set、
  canonical order、associated equality merge/conflict、method-call guard、borrowed witness/projection facts 和
  M11c 历史 runtime guard 边界。

M11c 实际改动低于早期 1,800-3,200 行预估的可能原因是 M11b 已提前铺好 `PrincipalSetCompositionFacts`
DTO、validation、summary/dump/fingerprint，M8/M10 已有 single-trait dyn 和 supertrait upcast 的 checked
vtable/borrowed coercion 地基；但 M11c 仍额外补了 parser/AST/type/sema/check-only coercion、测试拆分和 IR
guard。最终代码量以提交 diffstat 为准。

M11c 之后的 **M11d Principal-Set Composition IR / Backend Runtime** 已完成显式 runtime projection 子集：
IR composition/projection value、verifier、LLVM principal-set metadata layout 和 native runtime tests 已落地。
Direct composition receiver dispatch / principal-qualified method syntax 仍留给后续设计；M11e hardening/release
closure 也已完成。标准库、owning dyn、allocator 和 dynamic Drop dispatch 仍进入独立后续阶段。

## M11b Principal-Set Composition Query Prototype Gate

M11b 已完成 principal-set composition 的 query facts 原型。该阶段继续保持 compiler/query facts 边界，不实现
标准库、不实现 `dyn A + B` parser syntax、不实现 parser/AST/sema、不实现 IR/backend runtime、不实现 owning dyn、
不实现 `Box<dyn Trait>`、不实现 allocator API/policy、不实现 dynamic Drop dispatch。

M11b 当前新增或固定的实现包括：

- 新增 `PrincipalSetCompositionFacts` aggregate DTO，记录 subject、identity facts、witness sets、method
  namespaces、associated equality merges、projections、summary 和 fingerprint。
- 新增 `PrincipalSetIdentityFact` / `PrincipalSetPrincipalDescriptor`，由 `principal_set_identity_fact()` 对
  principal descriptors 做 canonical order、duplicate validation、derived composition origin 和稳定 identity 生成。
- 新增 `CompositionWitnessSetFact` / `CompositionWitnessDescriptor`，把 principal object、`VTableLayoutKey` 和
  witness fingerprint 组成 composition witness set。
- 新增 `PrincipalMethodNamespaceFact` / `PrincipalMethodNamespaceEntry`，明确同名 method 冲突必须使用
  `ambiguous_requires_principal`，不能 flatten 到未命名 slot namespace。
- 新增 `AssociatedEqualityMergeFact`，记录 `satisfied`、`conflict` 和 `unconstrained` associated equality merge 状态。
- 新增 `CompositionProjectionFact`，记录 `concrete_to_composition`、`composition_to_principal` 和
  `composition_to_supertrait` projection，并要求 data pointer 与 origin preserved。
- 新增 `principal_set_composition_facts_fingerprint()`、`summarize_principal_set_composition_facts()` 和
  `dump_principal_set_composition_facts()`。
- Focused query tests 覆盖 enum fallback、identity canonicalization、boundary drift、flattened namespace rejection、
  summary/dump/fingerprint 和 stale fact rejection。

M11b 之后的下一步是 **M11c Principal-Set Composition Frontend / Sema Check-Only**：在不实现标准库和 runtime 的
前提下，选择 source spelling，接入 parser/AST、type identity、coercion check、method namespace diagnostics、
associated equality merge check、checked dump/fingerprint 和 negative samples。M11c 预计 1,800-3,200 行；
M11d IR/backend runtime 预计 1,600-2,800 行；M11e hardening/release 预计 700-1,300 行。标准库、owning dyn、
allocator 和 dynamic Drop dispatch 仍进入独立后续阶段。

## M11a Advanced Dyn Design Baseline

M11a 已完成 advanced dyn 后续主线选择。M11 现在进入 principal-set borrowed dyn composition 设计流：该路线继续
复用 M8 origin-bound erased view、M9 dyn ABI/tooling facts 和 M10 `supertrait_vptr_metadata_v1` supertrait
projection，但必须新增 composition 专用 metadata policy `principal_set_metadata_v1`。新增设计入口见
[Aurex M11 Advanced Dyn Design Baseline](m11-advanced-dyn-design.md)。

M11a 当前新增或固定的实现包括：

- Query 层新增 `completed_release_baseline` stage，用来表示 M10 supertrait upcasting 已完成 release baseline。
- 新增 `m11a_dyn_advanced_design_gate_baseline()`，把 `multi_trait_composition` 标记为
  `ready_for_future_stage`，并选择 `principal_set_metadata_v1`。
- 新增 `is_valid_m11a_dyn_advanced_design_gate()`，固定 M11a gate name、candidate shape、capability-specific
  policy、stage、decision、impact summary 和 non-goals。
- Gate validation 要求五个 advanced capability 各出现一次，拒绝 capability 缺失或重复。
- Summary/dump 现在暴露 `ready_for_future_stage=N` 和 `completed_release=N`，documentation tests 固定
  `principal_set_identity_fact`、`composition_witness_set_fact`、`principal_method_namespace_fact`、
  `associated_equality_merge_fact` 和 `composition_projection_fact`。

M11a 仍不实现标准库、不实现 `Box<dyn Trait>`、不实现 owning dyn、不实现 dynamic Drop dispatch、不实现
allocator policy、不实现 `dyn A + B` parser syntax、不实现 principal-set sema coercion、不实现 IR/backend runtime
dispatch。M11a 也不把 composition 偷偷编码成单 trait object，不把 method slots flatten 到未命名 namespace，
不往 `principal_set_metadata_v1` 塞 destructor slot。

M11a 之后的下一步是 **M11b Principal-Set Composition Query Prototype Gate**：把 principal-set identity、
composition witness set、principal-qualified method namespace、associated equality merge 和 composition projection
做成稳定 query DTO / fingerprint / summary / dump / tooling facts。M11b 预计 800-1,400 行；M11c frontend/sema
check-only 预计 1,800-3,200 行；M11d IR/backend runtime 预计 1,600-2,800 行；M11e hardening/release 预计
700-1,300 行。标准库、owning dyn、allocator 和 dynamic Drop dispatch 进入独立后续阶段。

## M10d Supertrait Hardening / Release Closure

M10d 已完成 supertrait upcasting 的 hardening / release closure。M10 现在完整收口为 borrowed dyn supertrait
upcasting release baseline：M10a 设计、M10b frontend/query/sema facts、M10c IR/backend runtime、M10d
query/cache/tooling/sample/docs/coverage closure 均已完成。新增 release 入口见
[Aurex M10 Supertrait Upcasting Release Baseline](m10-release-baseline.md)。

M10d 当前新增或固定的实现包括：

- `FunctionDynAbiFacts::upcasts` 会作为 dyn ABI surface 参与 lower-IR query、semantic fact、summary、dump 和
  fingerprint。
- IDE function hover 对 supertrait upcast 展示 `metadata=supertrait_vptr_metadata_v1`、`upcasts=N`、首条
  source/target upcast、borrow kind 和 upcast metadata。
- Query focused tests 固定 upcast edge / borrow kind 变化会改变 `FunctionDynAbiFacts` fingerprint 和 lower-IR
  result fingerprint。
- 常规 negative sample suite 新增非 supertrait target、shared-to-mut borrow upgrade 和 missing parent evidence；
  IR/verifier focused tests 保持 layout/edge mismatch 覆盖。
- README、progress、version、next-steps、requirements、architecture、usage、language manual、language feature
  inventory 和 documentation tests 已统一到 M10d release baseline。
- M10c 实际代码量偏差已记录：实际 `37 files changed, 1316 insertions(+), 255 deletions(-)`，低于
  1,600-2,800 预估，原因是 M10b 已铺好 checked/query DTO，M8/M9 已有 borrowed dyn ABI/tooling 基线，LLVM
  lowering 复用 fat-view/indirect-call 路径，且标准库/owning dyn/Drop dispatch/allocator/multi trait composition
  均未进入 M10。

M10d 继续不实现标准库、不实现 `Box<dyn Trait>`、不实现 owning dyn、不实现 dynamic Drop dispatch、不实现
allocator policy、不实现 multi trait composition，也不新增 trait-object Drop metadata。Associated equality edge
mapping / ambiguity solver 仍是后续项，当前稳定覆盖的是 generic parent args substitution。

M10 已结束。下一步是 **M11 Advanced Dyn Design Baseline**：从 M9c gate 剩余候选中选择后续主线，先设计
policy/schema 和 query gate，而不是直接实现标准库或 owning dyn。

## M10c Supertrait IR / Backend Runtime Implementation

M10c 已完成 supertrait upcasting 的 IR/backend runtime implementation。该阶段继续不实现标准库、不实现
`Box<dyn Trait>`、不实现 owning dyn、不实现 dynamic Drop dispatch、不实现 allocator policy、不实现
multi trait composition，也不新增 trait-object Drop metadata。M10c 的范围只覆盖 borrowed dyn-to-dyn
supertrait upcast：保留 `{data*, vtable*}` borrowed erased view，data pointer 不变，通过
`supertrait_vptr_metadata_v1` vtable metadata 投影 parent vtable pointer。

当前新增实现包括：

- Sema 支持 inherited parent method dispatch on borrowed dyn child receiver：`child.parent()` 会把
  dispatch receiver 绑定为 target `dyn Parent`，并记录 `TraitObjectUpcastCoercionFact`；后续 concrete-to-child
  coercion 出现时会回填 source/target vtable layout 和 method binding layout。
- IR 新增 `trait_object_upcast` value，`TraitObjectVTableLayout` 新增 `supertrait_edges`，dump/fingerprint/
  clone/pass pipeline/dyn ABI facts 均纳入 upcast 和 supertrait edge。
- IR verifier 固定 runtime invariants：upcast source/target 必须是 trait object reference，source/target layout
  必须匹配 supertrait edge，edge index 必须唯一且有效，borrow kind、upcast key、object type 和
  `supertrait_vptr_metadata_v1` metadata policy 必须一致。
- Lowering 从 M10b checked upcast fact 生成 runtime `trait_object_upcast`，并保证每个 concrete `dyn Child`
  vtable 都有同序 parent edge；同一个 `score(child: &dyn Child)` 可以 late bind 到不同 concrete parent vtable。
- LLVM backend 将 vtable global 从 method pointer array 扩展为 `{ [methods x ptr], [supertraits x ptr] }`；
  `trait_object_upcast` 复用 data pointer，从 source vtable 的 supertrait table 加载 target parent vtable pointer，
  再构造 target borrowed dyn view。
- Native execution 已覆盖 inherited parent dispatch 和多 concrete child vtable 的 runtime parent projection。

M10c 的重要边界：

- Upcast 仍是 borrowed dyn-to-dyn coercion，**不是普通子类型**。
- Upcast 不创建 ownership、不复制对象、不延长 origin、不放宽 loan、不把 shared borrow 升级成 mutable borrow。
- 当前 runtime support 覆盖 `&dyn Child -> &dyn Parent`、`&mut dyn Child -> &mut dyn Parent` 和
  `&mut dyn Child -> &dyn Parent`，以及 `dyn Child` receiver 上 inherited parent method 的 runtime dispatch。
- Associated equality edge mapping 仍是后续项；当前稳定覆盖的是泛型 parent args substitution。
- 标准库、owning dyn、`Box<dyn Trait>`、allocator、dynamic Drop dispatch 和 multi trait composition 仍不属于 M10。

M10c 的下一步已由 **M10d Supertrait Hardening / Release Closure** 完成。标准库仍不属于 M10。

## M10b Supertrait Frontend / Query / Sema Implementation

M10b 已完成 supertrait upcasting 的 frontend/query/sema check-only 实现。该阶段继续不实现标准库、不实现
`Box<dyn Trait>`、不实现 owning dyn、不实现 dynamic Drop dispatch、不实现 multi trait composition，也不做
LLVM runtime lowering；它把 M10a 设计基线中的语言表面、checked facts、query key、ABI facts 和 sema proof
链路先稳定下来。

当前新增实现包括：

- Parser/AST 接受 `trait Child: Parent, Other where ... { ... }`，并通过 `TraitSupertraitDecl` /
  `ItemNode::trait_supertraits` 保存 direct supertrait list、source range 和 ordinal；AST dump、module store/load、
  copy/move 均已接入。
- Checked module 新增 `TraitSupertraitInfo`、`TraitSignature::supertraits`、`TraitSupertraitEdgeFact` 和
  `TraitObjectUpcastCoercionFact`；clone/copy/move/swap、stable fingerprint、checked dump、text rebind 和
  `TypeCheckBodyAuthority` 已纳入 edge/upcast 事实。
- Sema 注册 direct/transitive supertrait graph，拒绝 duplicate direct parent、cycle、public/private visibility leak，
  并把 `impl Child for T` 的 parent trait obligation 纳入 evidence checking。
- 泛型 parent trait args 会按 child edge 和 impl 实参替换，例如 `trait Child[T]: Parent[T]` 下
  `impl Child[i32] for File` 会要求 `impl Parent[i32] for File`。
- Borrowed dyn-to-dyn coercion 已进入 contextual coercion：`&dyn Child -> &dyn Parent`、
  `&mut dyn Child -> &mut dyn Parent` 和 `&mut dyn Child -> &dyn Parent` 可以记录 checked upcast fact；
  `&dyn Child -> &mut dyn Parent`、非 supertrait target 和泛型 target mismatch 会被拒绝。
- Query 层新增 `TraitObjectMetadataPolicyKey::supertrait_vptr_metadata_v1`、
  `TraitObjectUpcastCoercionKey`、`DynMetadataPolicy::supertrait_vptr_metadata_v1` 和 `DynUpcastAbiDescriptor`；
  `FunctionDynAbiFacts` 现在可以 summary/fingerprint/dump upcast descriptors。
- Focused tests 已按模块拆分到 parser、sema facts、dyn upcast 和 query key 文件，不继续膨胀旧的大型测试文件。

M10b 的重要边界已由 M10c runtime implementation 承接：

- Upcast 是 borrowed dyn-to-dyn coercion，**不是普通子类型**。
- Upcast 不创建 ownership、不复制对象、不延长 origin、不放宽 loan、不把 shared borrow 升级成 mutable borrow。
- M10b 只记录 checked facts / query DTO / ABI descriptor；`trait_object_upcast` IR、
  `supertrait_vptr_metadata_v1` LLVM vtable global 和 inherited parent dispatch runtime 已由 M10c 完成。
- Associated equality edge mapping 仍是后续项；M10b 已支持泛型 parent args substitution，但没有完整实现
  supertrait associated equality mapping / ambiguity solver。

M10b 的下一步已由 **M10d Supertrait Hardening / Release Closure** 完成。标准库仍不属于 M10c/M10d。

## M10a Supertrait Upcasting Design Baseline

M10 已从 `m10` 分支开启。M9 release closure 已完成；当前版本新增
[Aurex M10 Supertrait Upcasting 设计基线](m10-supertrait-upcasting-design.md)，把 M9c
`DynAdvancedDesignGate` 中的 `supertrait_upcasting` 候选正式选为 M10 第一条 advanced dyn 主线。M10a 是设计基线，
不实现 parser/sema/IR lowering/LLVM backend runtime 或标准库代码。

当前新增设计内容包括：

- 固定 M10a 语义：只设计 borrowed dyn-to-dyn coercion，即 `&dyn Child -> &dyn Parent` 和
  `&mut dyn Child -> &mut dyn Parent`。
- 明确 upcast 是 coercion，**不是普通子类型**；它不创建 ownership、不延长 origin、不放宽 loan、不把 shared
  borrow 升级成 mutable borrow。
- 保留 Aurex 的 origin-bound erased view：`borrowed_view_v1` 继续表达 `{data*, vtable*}` borrowed fat view，
  data pointer 不变，只替换或投影 parent vtable pointer。
- 明确 `borrowed_methods_only_v1` 不能承载 supertrait edge metadata；后续实现必须新增
  `supertrait_vptr_metadata_v1`。
- 固定后续 query/checked/ABI facts：`TraitSupertraitEdgeFact`、`TraitObjectUpcastCoercionKey`、
  `DynUpcastAbiDescriptor` 和 `VTableSupertraitEdgeDescriptor`。
- 固定 source surface 方向：`trait Child: Parent { ... }` 是 declaration graph edge；`where Self: Parent`
  继续只是 generic predicate，不作为 trait inheritance metadata。
- 固定 M10a 非目标：不实现标准库、`Box<dyn Trait>`、owning dyn、allocator、dynamic Drop dispatch、
  multi trait composition、`dyn A + B` 或 runtime upcast lowering。
- 给出 M10b frontend/query/sema、M10c IR/backend runtime、M10d hardening/release 的后续实现计划和代码量预估。

M10a 的下一步已经由 M10b Supertrait Frontend / Query / Sema Implementation 承接；M10b 已实现
supertrait syntax/AST、`TraitSupertraitEdgeFact`、cycle/visibility diagnostics 和 `TraitObjectUpcastCoercionKey`。
M10c 已继续完成 IR/backend runtime；标准库仍不属于本阶段。

## M9 Dyn ABI / Tooling Release Closure

M9 已从 `m9` 分支开启。M8 release closure 已完成；当前版本在
[Aurex M9 Dyn ABI / Tooling 设计基线](m9-dyn-abi-tooling-design.md) 和
[Aurex M9 Dyn ABI / Tooling Release Baseline](m9-release-baseline.md) 之上完成 M9 release closure。M9b 已把
M8 已经能运行的 borrowed dyn runtime dispatch 固化为可查询、可 fingerprint、可 dump、可投影到 IDE/tooling 的
ABI facts；M9c 则把 supertrait upcasting、owning dyn、dynamic Drop dispatch、allocator policy 和 multi trait
composition 的后续准入条件固化为 query-facing design gate；M9d 完成 release 文档、状态入口、documentation tests
和 release gate 收口，而不是继续增加 M8/M9 语言语义。

当前新增设计内容包括：

- 将 M9a 明确为 design baseline：不改变 `&dyn Trait` / `&mut dyn Trait` 当前语义，不实现新 runtime feature。
- 将 M9b 明确为 implementation baseline：实现 query-facing dyn ABI facts DTO、checked adapter、IR adapter、
  lower-function-IR query/cache invalidation 和 IDE semantic fact / hover projection。
- 将 M9c 明确为 advanced dyn design gate baseline：实现 `DynAdvancedDesignGate` /
  `DynAdvancedDesignCandidate` DTO、validation、stable fingerprint、summary、dump 和 focused query tests。
- 将 M9d 明确为 release closure：新增 M9 release baseline，更新 README/progress/version/next-steps，并用
  documentation tests 固定 release contract。
- 固定 M9 第一包非目标：不实现标准库、`Box<dyn Trait>`、owning dyn、allocator、dynamic Drop dispatch、
  supertrait upcasting 或多 trait object composition。
- 基于现有代码事实整理 M9 输入：`TraitObjectTypeKey`、`VTableLayoutKey`、`TraitObjectCoercionKey`、checked
  vtable facts、IR `trait_object_pack/data/vtable/vtable_slot`、IR verifier invariants 和 LLVM `{data*, vtable*}`
  borrowed view lowering。
- 实现 library-independent dyn ABI DTO：object descriptor、vtable descriptor、slot descriptor、coercion
  descriptor 和 dispatch descriptor；DTO 不依赖 LLVM 类型，不扫描 dump 文本，不复活无语义的 canonical
  trait-object kind tag。
- 固定 metadata schema：当前只承认 `borrowed_methods_only_v1`，明确 absent metadata 包括 drop/size/align、
  supertrait vptr、type metadata 和 allocator。
- 固定 fingerprint schema：object descriptor、vtable descriptor、coercion descriptor、dispatch descriptor 和
  tooling projection 分层 fingerprint，服务 query/cache 与 lower-IR invalidation。
- 固定 tooling projection 方向：IDE semantic facts / hover 可展示 dyn ABI policy、metadata policy、vtable layout、
  slot ordinal、dispatch kind 和 coercion relation。
- 固定 M9b verifier/backend negative matrix：layout duplicate、slot mismatch、receiver ABI mismatch、fat view type
  mismatch、missing vtable layout 和 invalid slot pointer type 等。
- 固定 M9c advanced dyn gate：`supertrait_upcasting` 和 `multi_trait_composition` 要求新 metadata policy；
  `owning_dyn` 和 `allocator_policy` 要求后续标准库阶段；`dynamic_drop_dispatch` 要求后续 runtime 阶段。
- 固定 policy 分离规则：advanced dyn candidate 不能把 `borrowed_view_v1` 或 `borrowed_methods_only_v1` 当成
  required policy 复用。

M9 release 后续已经由 M10a 承接：supertrait upcasting 已被选为 M10 第一条 advanced dyn 主线，并已先固定
policy/schema/query/ABI 设计。M9 release 本身仍不实现标准库或 advanced runtime。

## M8 Dyn Trait、Erased View 与动态派发 Release Closure

M8 主线已完成 release closure。M8a-M8e 已完成 dyn trait / erased view 调研设计、query 地基修正、frontend
syntax/sema、borrowed dyn coercion、checked vtable facts、IR/backend runtime dispatch 和 hardening closure；
M8 follow-up sample / release polish 也已完成。
M8 不把 dyn trait 当成“照抄 Rust trait object”或
“补一个 parser 分支”；新的设计基线选择 Aurex 自己的 origin-bound erased view：第一版只做 borrowed dyn view，
复用 M7 origin / loan / lifetime facts，以 checked vtable witness 描述动态派发。

当前新增实现包括：

- 新增 [Aurex M8 Dyn Trait、Erased View 与动态派发设计基线](m8-dyn-trait-design.md)，整理 Rust、Swift、
  Go、C++ ABI 的取舍，并固定 Aurex 的非照抄路线。
- 移除 query 层无语义形状的 `CanonicalTypeKind::trait_object` 占位。该旧设计没有 principal trait、
  associated equality、object origin/lifetime 或 vtable layout identity，却被 decoder 和 tests 当作有效
  canonical type key；M8 后续不能在这个错误稳定 key 上继续扩展。
- 新增结构化 M8a query identity：`TraitObjectTypeKey`、`VTableLayoutKey`、`TraitObjectCoercionKey`。三者分别表达
  borrowed erased view 类型、checked vtable witness 和 borrow-to-dyn coercion，不再把 canonical type、vtable
  layout、conformance evidence 混在一个 kind tag 或 children list 里。
- 更新 stable key decoder 与 query tests：当前 canonical type key 只承认可由现有语言/语义实际产生的类型形状。
  M8a decoder 还会验证 trait object / vtable / coercion key 的 schema、policy、principal trait、associated
  type member 和嵌套 canonical type 形状，并拒绝三类 key 布局混用。
- 新增 M8b frontend surface：`dyn Trait`、qualified dyn trait、trait args 和 associated equality 可 parse、
  AST dump 和 sema resolve；bare `dyn Trait` 仍不能作为普通 storage type，只允许经 `&dyn Trait` /
  `&mut dyn Trait` 等 reference pointee 使用。object-callability 会诊断缺少 self receiver、非法 receiver、
  缺失/未知/重复 associated equality 和 unconstrained `Self`。
- 新增 M8c borrowed dyn coercion：`&T -> &dyn Trait`、`&mut T -> &mut dyn Trait` 会检查可见 nominal impl 与
  associated equality，成功时记录 checked vtable layout、method slot、callability 和 coercion facts；dyn receiver
  method call 绑定为 `TraitMethodDispatchKind::vtable_slot`。
- 新增 M8d IR/backend runtime dispatch：IR 显式表达 `trait_object_pack`、`trait_object_data`、
  `trait_object_vtable` 和 `vtable_slot`；verifier 检查 vtable layout、slot schema 和 erased receiver ABI；
  LLVM backend 生成 `{data*, vtable*}` fat view、internal vtable global 和 indirect call。
- 新增 M8e hardening：default method slot、associated equality dispatch、checked/IR fingerprint、lower-IR unit
  invalidation、native execution 和文档收口都已补强。
- 新增 M8 follow-up sample / release polish：borrowed dyn runtime dispatch 进入常规 positive runtime sample
  suite，缺失 associated equality 和缺失 nominal impl coercion 进入常规 negative sample suite。
- 更新 `next-steps` 与中文文档入口，把 M8a-M8e 路线明确为：query 地基、syntax/sema、borrowed dyn coercion、
  IR/backend dispatch、hardening/后续扩展评估。

当前仍保守的边界：M8 只完成 borrowed dyn view 和 runtime method dispatch，不实现 owning dyn、
`Box<dyn Trait>`、allocator、标准库、dynamic Drop dispatch、supertrait upcasting 或多 trait object composition。
后续 M9 已完成 dyn ABI / tooling release closure；当前下一条主线是 M10 planning / post-M9 advanced dyn policy
selection，标准库或 owning dyn 必须独立设计、独立估算。

## M7d-K Array Repeat Resource Safety Closure

M7d-K 已完成 compiler-only array repeat resource safety 收口。本阶段继续不实现标准库，也不引入 `Clone`、
owned resource wrapper、generic Drop surface 或任何库级资源 API；新增能力只发生在 Sema array repeat
规则、ownership/borrow/place-state 运行期遍历语义、IR lowering 防御和测试覆盖内部。核心语义是：`[expr; N]`
不能在没有复制/克隆构造语义时隐式复制非 `Copy` 资源。

当前新增实现包括：

- Sema 对 repeat array literal 增加资源规则：`[expr; 0]` 与 `[expr; 1]` 不要求 `expr` 的元素类型是
  `Copy`；`[expr; N]` 且 `N > 1` 时，元素类型必须满足 compiler-owned `Copy` capability，否则诊断
  `array repeat value must be Copy when repeated more than once`。
- `[expr; 0]` 仍会按元素类型上下文对 `expr` 做类型检查，避免无效表达式从零长度 repeat 中漏过；但运行期
  ownership/move/borrow/place-state/flow traversal 不把 repeat value 当作会求值或会 consume 的表达式。
- `[expr; 1]` 按一次普通 owned value transfer 处理，允许非 `Copy` 资源进入长度为 1 的数组。
- move analysis、place-state precheck、body-flow return scan、loan checker origin traversal、borrow summary 和
  storage escape origin traversal 都使用同一个 checked array repeat runtime helper，避免不同分析阶段对
  zero-repeat value 的运行期语义分叉。
- IR lowering 对 repeat literal 保持防御：长度为 0 时只生成空 aggregate；若非法的非 `Copy` 多元素 repeat
  绕过 Sema 到达 lowering，只生成空 aggregate placeholder，不复制同一个 owned `ValueId`。
- 覆盖新增 positive samples：单元素非 `Copy` 资源 repeat、零长度非 `Copy` 资源 repeat；新增 negative sample：
  非 `Copy` 资源多元素 repeat。白盒测试覆盖 Sema copy 需求、零长度运行期跳过、IR lowering 防御和 sample
  suite 诊断。

当前能做的事情：

- `let files: [0]File = [make_file(); 0];` 可以通过：`make_file()` 类型检查为 `File`，但运行期不被求值，
  不产生 move/borrow/cleanup 消费。
- `let files: [1]File = [make_file(); 1];` 可以通过：`make_file()` 被求值一次，数组持有一个 `File`。
- `let values: [4]i32 = [0; 4];` 继续按 `Copy` 标量 repeat 生成数组值。
- `let files: [2]File = [make_file(); 2];` 会在 Sema 阶段拒绝，避免在没有 Clone/fill constructor/rollback
  构造协议时隐式复制同一个资源值。

当前仍保守的边界：本阶段不实现标准库 API，不实现 `Clone`、array fill constructor、非 `Copy` 多元素 repeat
构造、用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、dynamic destructor ABI、
async/unwind-aware drop 或 panic cleanup ABI；也不实现 consuming pattern payload transfer、non-`Copy` `?`
payload transfer、indexed move-out、array/slice/index 精确 disjoint proof 或 replace/take/swap primitive。

## M7d-J Cleanup Marker Query / Tooling Consumption Closure

M7d-J 已完成 compiler-only cleanup marker query / tooling 消费面收口。本阶段继续不实现标准库，也不引入任何
库级 owned resource wrapper；新增能力只发生在 IR cleanup marker facts、query/cache result、driver incremental
cache subject、IDE/tooling semantic fact 和测试覆盖内部。语言行为不放宽：generic / associated / opaque /
unknown cleanup 仍保持 marker-only，consuming pattern payload、non-`Copy` `?` payload transfer 和 indexed
move-out 仍按当前规则拒绝。

当前新增实现包括：

- query 层新增稳定 DTO：`CleanupMarkerKind`、`CleanupMarkerPolicy`、`CleanupMarkerFact`、
  `CleanupMarkerSummary` 和 `FunctionCleanupMarkerFacts`。这些 facts 有稳定 fingerprint、summary 和 dump，
  工具链可以消费 facts 而不依赖 IR 内部结构。
- IR 层新增 cleanup marker facts 提取器：`function_cleanup_marker_facts(...)`、
  `function_cleanup_marker_facts_by_symbol(...)` 和 `query_cleanup_marker_policy(...)`。提取器按函数 value
  closure 收集 `drop` / `drop_if` marker，记录 value id、object、condition、target type 和 cleanup ABI policy。
- `lower_function_ir` / generic lower IR query 的 provider input/output 现在携带 `FunctionCleanupMarkerFacts`；
  query result fingerprint 使用 raw lowered IR fingerprint 加 cleanup marker facts fingerprint 和 summary count
  共同生成，避免 cleanup ABI 事实变化被 cache 误复用。
- driver incremental cache 的 lower-IR subject 分离 raw IR input fingerprint、final query result fingerprint 和
  cleanup marker facts；调度 provider 时传入 facts，而不是把 final result 当 raw IR input 二次混入。
- tooling full build 下 `aurex_tooling` 可选择性链接 `aurex_ir` 并启用 `AUREX_TOOLING_ENABLE_IR_FACTS`，
  IDE snapshot 会在 sema 成功后临时 lower IR 并填充 `snapshot.cleanup_marker_facts`；frontend-only build 不链接
  IR target，facts 为空且行为保持兼容。
- IDE semantic facts 新增 `cleanup_marker_facts` kind，并把该 fact 关联到 `lower_function_ir` query；
  workspace index 和 session reuse invalidation 识别该 fact 为 body-local / indexable；函数 hover 在存在 facts
  时显示 `cleanup_markers=count=...`。
- 覆盖率补齐到新增代码自身：`cleanup_marker_facts.cpp` region 100%，`lower_function_ir_query.cpp` region 100%，
  `ir_cleanup_marker_facts.cpp` region 95.89%；全量 coverage gate 的 source lines/functions/regions 均达到 95% 门槛。

当前能做的事情：

- 编译器能把 M7d-G 之后 IR `drop` / `drop_if` marker 上的 cleanup ABI policy 稳定投影给 query/cache/IDE，
  不需要 IDE/LSP 重新扫描 IR dump 字符串或重新推导 lowering 行为。
- lower-IR query cache 能感知 cleanup marker policy、数量和分类变化；generic/opaque/associated/unknown marker-only
  事实改变时，query result 会正确失效。
- tooling full build 能在函数 hover、semantic facts、workspace index 和 reuse plan 中消费 cleanup marker facts；
  frontend-only build 仍保持轻量，不强制引入 IR 依赖。
- 这为后续 dynamic Drop ABI、generic cleanup runtime ABI 或 payload transfer 语义提供事实面，但当前不生成任何未知
  runtime destructor 调用。

当前仍保守的边界：本阶段不实现标准库 API，不实现标准库拥有型资源封装，不实现用户可写 `Drop` bound、generic
Drop impl、trait-object Drop dispatch、dynamic destructor ABI、async/unwind-aware drop 或 panic cleanup ABI；
也不实现 consuming pattern payload transfer、non-`Copy` `?` payload transfer、indexed move-out、array/slice/index
精确 disjoint proof、array repeat resource rollback 或 replace/take/swap primitive。

## M7d-I Move Rejection Facts Closure

M7d-I 已完成 compiler-only move rejection facts 收口。本阶段继续不实现标准库，也不引入任何库级 owned
resource wrapper；新增能力只发生在 sema move analysis、checked facts、query/cache authority、IDE/tooling
投影和白盒测试内部。语言行为不放宽：consuming pattern payload、non-`Copy` `?` payload transfer 和
indexed move-out 仍按当前规则拒绝。

当前新增实现包括：

- `CheckedModule::move_rejection_facts` 按函数记录当前 move analysis 实际发出的三类 unsupported 事实：
  `pattern_payload`、`try_payload` 和 `indexed_element`。事实包含关联 `expr`、`stmt`、`pattern`、当前用于资源判定的
  `tracked_type`、resource fingerprint、诊断是否已发出和 source range。
- move analysis 只在 reachable action 真实发出 unsupported diagnostic 时记录事实；不可达路径不会凭空产生
  checked fact，避免 query/tooling 与用户可见诊断不一致。
- `FunctionMoveRejectionFacts` 增加稳定 fingerprint、dump 和 checked-module copy/move 支持；checked dump 新增
  `move_rejection_facts` 段，便于后续调试 consuming pattern / try payload 的事实链。
- `TypeCheckBodyAuthority` 混入 move rejection fingerprint、总数、三类分类计数和 emitted-diagnostic 状态位；
  type-check body query result 会随这些 compiler facts 改变而失效。
- IDE semantic facts 新增 `move_rejection_facts` kind，workspace index / reuse invalidation 认识该 fact；
  函数 hover 在存在 facts 时显示 `move_rejections=count=.../first=...`。
- 新增 `move_rejection_facts_tests.cpp`，避免继续扩大既有资源/dropck 巨型测试文件；测试覆盖 match arm payload、
  struct pattern payload、if / if-expr / while condition payload、`?` payload 和 indexed move-out 三类拒绝事实。

当前能做的事情：

- 编译器能把“为什么当前拒绝这个 non-`Copy` pattern / `?` / index move”的事实稳定暴露给 checked dump、query
  authority 和 IDE/tooling，而不是只留下字符串诊断。
- 后续如果真正实现 payload transfer，可以用同一事实面验证从“拒绝”到“接受”的语义迁移，不需要 IDE/LSP 重新扫描
  AST 或重新推导 move analysis。
- 当前 diagnostic 与 fact 一致：只有已发出的 unsupported diagnostic 会进入 `move_rejection_facts`。

当前仍保守的边界：本阶段不实现 consuming pattern payload 的真实 move/reinit/drop 语义，不实现 non-`Copy`
`?` ok/some payload transfer，不实现 indexed move-out 或 array/slice/index 精确 per-element ownership proof；
标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、dynamic destructor
ABI、async/unwind-aware drop、panic cleanup ABI、array repeat resource rollback 和 replace/take/swap primitive
仍是后续独立工作。

## M7d-H Index / Slice Place-State Conservative Closure

M7d-H 已完成 compiler-only index / slice place-state 保守闭包。本阶段继续不实现标准库，也不引入任何库级
owned resource wrapper；新增能力只发生在 BodyFlow place identity、place-state facts、borrow checker 回归测试和
IR lowering cleanup place path 内部。

当前新增实现包括：

- place-state 的 semantic place identity 不再把本地 root 的 AST `ExprId` 当成不同 storage；同一个 local /
  parameter 的不同语法出现点会归并到同一个 root place。temporary root 仍保留 `root_expr`，避免不同临时值被误合并。
- field projection identity 只使用 `field_name_id`，tuple projection identity 只使用 `element_index`；index、slice
  和 dereference projection 不使用具体表达式 id 做精确区分，符合当前 M7d 对 array/slice/index 的 conservative
  may-alias 策略。
- place-state facts fingerprint schema 升级到 `sema.place_state.facts.v3`，避免旧缓存把 expr-id-sensitive facts
  当成新的 conservative facts 复用。
- BodyLoan checker 原有 projection conflict 规则继续保持：same/prefix 冲突、已知 struct field / tuple element
  disjoint 可放宽，index/slice/dereference/unknown projection 保守冲突；白盒测试显式覆盖不同 index expr 仍冲突。
- BodyFlow 的 return cleanup 链现在按已注册前缀构造：`return` 只清理执行到该点前已经进入作用域的 local/defer，
  return 后未执行的 local 声明和 defer 注册不会被误加入该 return path。
- IR lowering 的 `LocalPlaceProjectionKind` 增加 `index` 和 `slice`，`local_place_path` 现在能识别
  `local[index]`、`local[start:end]` 以及 `local[index].field` 这类本地 place path。cleanup prefix matching
  对 index/slice 只做同类保守匹配，不尝试证明不同下标或不同 slice range disjoint。

当前能做的事情：

- 本地 struct field 和 tuple element 的 partial move / reinit / cleanup drop flag 仍按 M7d-B/M7d-F 的精确规则工作。
- 本地 array/slice/index projection 在 borrow、place-state 和 lowering cleanup path 中不会被错误拆成互不相干的
  精确子 place；`a[i]` 与 `a[j]` 在当前阶段按 may-alias 处理。
- tracked resource 的 indexed move-out 仍由 move analysis 保守拒绝；resource index assignment 仍由 sema 报
  unsupported，避免在没有 per-element ownership/drop proof 时泄漏或双 drop。

当前仍保守的边界：标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop
dispatch、dynamic destructor ABI、async/unwind-aware drop、panic cleanup ABI、non-`Copy` `?` payload transfer、
indexed move-out、array/slice/index 精确 disjoint proof、consuming pattern payload、array repeat resource rollback
和 replace/take/swap primitive 仍是后续独立工作。

## M7d-G Generic / Opaque Cleanup Marker ABI Closure

M7d-G 已完成 compiler-only cleanup marker ABI policy 正式化。本阶段继续不实现标准库，也不引入任何库级
owned resource wrapper；新增能力只发生在 sema drop-glue planner、IR cleanup marker、IR verifier、dump、
fingerprint 和 lowering 内部。

当前新增实现包括：

- sema drop-glue step 增加 `DropGlueAbiPolicy`，把 structural static cleanup、generic marker-only、
  associated-projection marker-only、opaque marker-only、unknown marker-only 和 static custom destructor
  明确区分。
- missing structural metadata 和 recursive drop-glue cycle 不再伪装成 `opaque_value`，而是记录为
  `unknown_value` + `unknown_marker_only`，避免后续 ABI 设计把未知结构误判为真正 opaque type。
- IR `drop` / `drop_if` marker 增加 `CleanupAbiPolicy` 字段；所有 lowering 生成的 cleanup marker 都会携带
  policy，IR dump 输出 `abi(...)`，IR fingerprint 混入 policy，clone/copy 会保留该事实。
- IR verifier 拒绝没有 cleanup ABI policy 的 `drop` / `drop_if`，拒绝非 drop value 携带 cleanup policy，
  并校验 marker policy 与 target type kind 匹配：generic、associated projection、opaque 和 structural/static
  marker 不再只能靠名字或注释区分。
- lowering 仍只为静态可解析 custom destructor 生成普通 direct `call`；generic、associated projection、opaque
  和 unknown cleanup 当前均保持 marker-only，不生成未知 runtime ABI 调用。
- LLVM backend 行为不变：`drop` / `drop_if` marker 本身仍是 no-op，真实析构副作用继续来自 M7d-D 已有的
  direct call lowering。

当前仍保守的边界：标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop
dispatch、dynamic destructor ABI、async/unwind-aware drop、panic cleanup ABI、non-`Copy` `?` payload transfer、
indexed move-out、array/slice/index 精确 disjoint proof、consuming pattern payload、array repeat resource rollback
和 replace/take/swap primitive 仍是后续独立工作。

## M7d-F Tuple / Index Place-State Closure

M7d-F 已完成 compiler-only tuple numeric field 与 tuple element place-state closure。本阶段继续不实现标准库，
也不引入任何库级 owned resource wrapper；新增能力只发生在 parser、sema、borrow/place/dropck facts 和 IR
lowering 内部。

当前新增实现包括：

- parser 接受匿名 tuple 数字字段访问：`value.0` 和带空格的 `value . 0` 都会进入普通 field expression；
  `value.0f32` 仍保持为 suffixed float 边界，不被误当成 tuple field。
- sema 对 tuple field 做数字索引检查：`.0` / `.1` 等合法范围内字段得到对应元素类型，并作为可写 place
  参与 assignment；`.first` 这类非数字字段报 `tuple field access requires a numeric field`，
  越界数字字段报 `tuple field index is out of range`。
- BodyFlow place projection 增加 `tuple_element`，borrow conflict matrix 能区分同一 tuple root 下的不同已知
  tuple 元素；`pair.0` 和 `pair.1` 不再被当成同一个字段冲突。
- place-state、dropck 和 move analysis 能处理本地 owned tuple 元素的 partial move、reinit 和 cleanup facts；
  单元素 tuple `.0` consume 仍按 whole-object consume 处理，多元素 tuple 元素 move 作为 partial move。
- IR lowering 为 droppable tuple 元素建立元素级 cleanup/drop flag，tuple 元素 move-out 会关闭对应 flag，
  reinit 后重新打开，scope cleanup 只 drop 仍 initialized 的 tuple 元素；嵌套 tuple/struct droppable leaves
  使用同一套结构化 cleanup 展开。

当前仍保守的边界：标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop
dispatch、generic/opaque Drop ABI、async/unwind-aware drop、panic cleanup ABI、non-`Copy` `?` payload transfer、
indexed move-out、array/slice/index 精确 disjoint proof、consuming pattern payload、array repeat resource rollback
和 replace/take/swap primitive 仍是后续独立工作。

## M7d-E Aggregate Rollback Codegen Closure

M7d-E 已完成 compiler-only aggregate rollback lowering 收口。本阶段不实现标准库，也不引入任何库级
owned resource wrapper；新增能力只发生在 IR lowering：当函数体中构造含 droppable 元素的 aggregate 时，
lowerer 会为部分初始化状态生成临时 storage 和 rollback cleanup action，保证后续元素求值提前终止时已经初始化
成功的元素会被清理。

当前新增实现包括：

- struct literal、tuple literal、array literal 和多 payload enum synthetic record payload 会在需要 rollback 时
  走 staged aggregate lowering。
- 每个 droppable 元素先完成表达式求值；只有当前 block 未被 `return` 等 terminator 关闭后，lowerer 才创建
  rollback drop flag、注册临时 cleanup action、store 元素值并把 flag 置为 initialized。
- 后续元素求值如果触发 early-exit，已有 cleanup stack 会发出 `drop_if` marker；M7d-D 已接入的 runtime
  drop lowering 会在 marker 旁生成静态 custom destructor direct call。
- 成功构造完整 aggregate 后，rollback cleanup action 会从当前 cleanup scope 中撤销，避免正常路径和后续
  local cleanup 重复 drop 临时元素。
- global/constant initializer、无 droppable 元素的 aggregate 和普通 scalar aggregate 仍保持 lightweight
  `ValueKind::aggregate` 路径，不引入临时 storage。

当前仍保守的边界：标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop
dispatch、generic/opaque Drop ABI、async/unwind-aware drop、panic cleanup ABI、non-`Copy` `?` payload transfer、
indexed move-out、array/slice/index 精确 disjoint proof、array repeat resource rollback 和 replace/take/swap
primitive 仍是后续独立工作；tuple `.0` / `.1` source surface 与本地 tuple 元素 partial move 已由 M7d-F 补上。

## M7d-D RAII Runtime Lowering 与 Execution Closure

M7d-D 已完成 RAII runtime lowering 的第一版闭环。当前实现沿用 M7d-C 的窄 `Drop` surface 和
`CheckedModule::destructors` facts，在 IR lowering 阶段把可静态解析的 custom destructor 降低为普通
direct `call`，LLVM backend 通过现有 call emission 生成真实调用。

当前新增实现包括：

- cleanup lowering 解析 `DestructorInfo::function_key` 对应的 `FunctionSignature::c_name`，复用普通函数声明阶段
  建好的 IR symbol -> `FunctionId` 表，不再额外发明 destructor mangling。
- `drop(self: deinit T)` 按 by-value ABI 调用：lowerer 从 cleanup slot load 出 `T`，再把该值作为唯一实参传给
  destructor；不会把 slot 地址传给 `self: deinit T`。
- `drop` / `drop_if` 继续作为 target-independent cleanup marker 保留，便于 verifier、dump 和后续优化观察 cleanup；
  真实运行时副作用由 marker 旁路生成的 direct call 承担。
- 条件 cleanup 会读取 drop flag，并在独立 then block 中执行 custom destructor call；join 后清空 drop flag。
- 带 custom destructor 且含 droppable 字段的 struct local 会同时注册根 cleanup flag 和字段级 cleanup flag：
  scope exit、overwrite、early-exit 和 defer unwind cleanup 会先执行根 custom destructor，再按字段 cleanup
  反向处理仍 initialized 的字段，避免字段拆分吞掉根 destructor，也避免根 full glue 双 drop 字段。
- `self: deinit T` 参数不再注册普通 lexical cleanup，避免 destructor body 退出时递归/重复 drop 自己。
- 结构化 runtime drop 会继续沿 struct、tuple、array 和 active enum payload 展开已知 custom destructor 子对象；
  generic/associated/opaque cleanup 仍只保留 marker，不凭空生成未知 ABI 调用。

当前仍保守的边界：`Drop` bound、generic Drop impl、trait-object Drop dispatch、async/unwind-aware drop、
panic cleanup ABI 和标准库拥有型资源封装仍是后续设计项；通用 aggregate rollback codegen 已由 M7d-E 补上
compiler-only 子集。LLVM backend 中
`drop` / `drop_if` marker 本身仍是 no-op；M7d-D 的 runtime 行为来自 lowering 阶段额外生成的普通 call。

## M7d-C RAII User Surface 与 Release Closure

M7d-C 的窄 RAII user surface 已完成实现收口。当前实现建立在 M7d-B place-state、M7d-A dropck facts、
M7c lifetime facts、M7b borrow contract 和 M6 resource/drop-glue 基线之上，开放 compiler-owned
`impl Drop for T { fn drop(self: deinit T) -> void { ... } }` 语义表面。

当前新增实现包括：

- parser/AST 接受参数冒号后的 contextual `deinit` 修饰符，AST dump 会输出
  `param self : deinit T`。
- `Drop` 是 reserved destructor surface，不再作为普通用户 trait 注册；`trait Drop`、`impl pkg.Drop for T`、
  `impl Drop[i32] for T`、generic Drop impl、associated type、额外方法、borrow contract、unsafe/extern/export/
  variadic Drop method 等非法 surface 都有专门诊断。
- 合法 destructor 必须精确写成 `impl Drop for T`，目标为 named `struct` / `enum` / `opaque struct`，
  方法必须且只能为 `fn drop(self: deinit T) -> void { ... }`，并带函数体。
- sema 在 `CheckedModule::destructors` 记录 `DestructorInfo`，并把对应 `FunctionSignature` 标记为
  `is_destructor`；checked dump、clone/copy 和 stable fingerprint 都覆盖该事实。
- resource classifier 将带 custom destructor 的类型归为 conservative owned resource；
  drop-glue plan 先生成 `custom_destructor` step，再继续展开结构字段、tuple、array、enum payload、generic
  或 opaque cleanup。
- dropck action 使用 destructor info fingerprint 作为 custom destructor key，并在 drop facts 中记录
  destructor function；`TypeCheckBodyAuthority` 混入 destructor count/fingerprint；IDE hover 显示
  `destructor=custom`。
- IR verifier 已补充 `drop` / `drop_if` target mutable 约束，拒绝对 `*const T` / `&T` target 发出 drop。

M7d-C 本身只声明 semantic/checking/tooling closure；M7d-D 已在其后补上可静态解析 custom destructor 的
runtime direct-call lowering。用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、
async/unwind-aware drop、panic cleanup ABI 和标准库拥有型资源封装继续作为后续设计项。

## M7d-B Struct Field Place-State 与字段级 Drop Flag 子集

M7d-B 的 struct field 子集已完成实现收口。当前实现建立在 M7d-A dropck facts、M7c lifetime facts、M7b
borrow contract 和 M6 cleanup/drop lowering 之上，把本地 owned struct 字段纳入 place-level resource state。

当前新增实现包括：

- body-flow assignment 对直接 local 和 field projection 生成 `reinit`；block cleanup 对 droppable struct
  fields 生成 projected `cleanup_storage`。
- generic template body-flow 会读取 generic side table 的 expr/local 类型，保证 `Box[T]` 这类模板内字段 cleanup
  能看到字段类型。
- sema 允许本地 owned struct 字段 move-out 和字段 reinit；`current.left = replacement` 这类本地字段覆盖进入
  place-state，而通过 `&mut Box[T]` 的 `box.value = replacement` 仍保守拒绝。
- place-state facts 记录 struct field partial move、reinit、cleanup 和 drop flag 状态；owned generic return
  summary 不再被 place-state 误解释为对 moved 参数的借用访问。
- IR lowering 为 droppable struct fields 建立字段级 cleanup/drop flag，在字段 move-out 后关闭对应 flag，在字段
  reinit 后重新打开，cleanup 只 drop 仍 initialized 的字段。
- 样例中 `move_partial_field` 从负例移为正例，并新增字段 reinit 正例。

当前仍未完成的 M7d-B 设计项后来已有部分收口：tuple `.0` / `.1` source surface、本地 tuple 元素 partial
move/reinit/drop flag 已由 M7d-F 补上。indexed move-out、consuming pattern payload、non-`Copy` `?` payload
transfer 仍保守拒绝；`replace` / `take` /
`swap` 还没有 compiler-known primitive 或标准库 intrinsic；borrowed/reference 字段 resource overwrite 仍因缺少本地
drop flag proof 而拒绝。

## M7 Hardening Performance Closure

M7c/M7d 进入下一阶段前的硬化闭环已完成，记录在
[M7 Hardening Performance Closure](m7-hardening-performance-closure.md)。

本轮收口内容包括：

- 剩余 `u32/i32/usize` 审计：query authority 与 checked semantic count 保持/扩展到 `u64`；
  `NormalizedAstOverlay` 计数从 `usize` 改为 `u64`；AST/IR/sema handle、stable key schema、lifetime/body-flow
  index 和 bounded 小域 index 保持 `u32`，作为未来独立 schema migration 处理。
- statement control-flow query 结果按 `StmtId` 四路缓存，并缓存子语句/子块结果；body-loan local-check precheck
  从两次表达式子树扫描合并为一次扫描。
- 新增 `tools/m7_hardening_perf.py`，统一 Google Benchmark、hyperfine 和 `/usr/bin/time -v` 输出。
- 同机同命令 Release benchmark 对照基线提交 `eef0c25b`，broad frontend case 均在当前容器噪声范围内，无可见回退；
  `SemaAstBulk/4096` 当前 CPU time 为约 `20.352 ms`，4x statement 规模约 `4.35x`。
- `perf` 已安装但当前容器 `perf stat` 不可用，因此本轮不把 PMU counter 作为 gate；hyperfine 和
  `/usr/bin/time -v` 已写入 `build/m7_hardening_perf/summary.md`。

## M7c-C Storage Escape 与性能收口

M7c-C 的核心 storage escape 迁移和性能收口已完成，文档入口仍为
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。

当前新增实现包括：

- `FunctionBorrowSummary::storage_escapes`，用于记录 non-name storage assignment 中逃逸到外部 storage 的
  borrowed origin。
- lifetime collector 将 borrow summary storage escape 映射为 `local_escape` / `unknown_escape` violation，
  由 lifetime enforcer 统一发出主诊断。
- `TypeCheckBodyAuthority`、query fingerprint、checked dump、borrow summary dump、IDE semantic fact 和 hover
  暴露 storage escape / local escape / unknown escape 状态。
- 旧 `BorrowEscapeAnalyzer` 只在 summary 缺失或 summary 无 storage escape 且 body 有候选 non-name assignment
  时作为窄 fallback guard 运行，避免 summary/lifetime 主路径与旧 analyzer 重复报错。
- borrow summary、lifetime 和 body loan hot path 的 O(n²) 扫描已改为 hash index、sparse graph/cache 或
  per-action/per-carrier queue：origin 去重、lifetime duplicate facts、lifetime violation、outlives reachability、
  body-loan carrier binding、reborrow parent 绑定和 two-phase activation matching 均已收口。

本地实际性能数据使用 `aurexc --profile-output`、`cmake -E time` wall-time spot-check 和 `gprof` 采集。
`build/full-llvm-fedora` 当前配置下，storage escape 压测 `sema.analyze` 3-run median 为：500 条
50.701 ms，1000 条 100.202 ms，2000 条 204.232 ms，4000 条 419.745 ms。优化前同类 500/1000/2000
条为
323.151 / 1007.543 / 4821.837 ms。更新后的 `gprof` 2000 条报告确认旧热点
`BodyLoanSolver::expr_result_contains_loan` 已不再出现在热点调用列表，`BodyLoanSolver::type_contains_reference`
调用数为 4000 次，随输入规模线性增长。

## M7c-A / M7c-B Lifetime Facts 与 Region Enforcement 实现收口

M7c-A / M7c-B 已完成实现收口，文档入口仍为
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。

当前实现包括：

- `CheckedModule::lifetime_facts`、`type_lifetime_infos`、`generic_lifetime_predicates`，以及对应 dump、clone、
  interner rebind 和 stable fingerprint。
- `FunctionLifetimeFacts` 中的 region、outlives constraint、type-outlives constraint、return region、violation 和
  body-flow point live-range facts。
- lifetime collector / solver / enforcer 分层：signature/reference origin facts、`@borrow(...)` 与 inferred
  `BorrowSummary` 统一映射到 lifetime facts；solver 做确定性 outlives fixed point 与 live-range projection；
  enforcer 处理 elision ambiguity、return-origin subset、type-outlives 和 local/temporary escape 主诊断。
- `TypeCheckBodyAuthority` 和 IDE `lifetime_facts` detail 混入 lifetime live-range、type/generic predicate count、
  local/unknown escape 状态和 lifetime fingerprint。
- `BorrowSummaryBuilder` 追踪 `strraw` raw pointer alias 的本地来源；raw-derived local return 由新 lifetime checker
  诊断，unsafe raw pointer parameter helper 仍保守记录 unknown return fact。
- 旧 `BorrowEscapeAnalyzer` 已从 return escape 主路径降级为 storage-only parity guard，继续覆盖尚未迁移的
  assignment into escaping field 等存储逃逸场景。

M7c-A/B 仍不声明 M7c-C 范围：public/prototype/extern/trait lifetime release policy、trait impl contract subset
细化、旧 analyzer 删除、closure capture placeholder 落地和更完整的 borrowed-view escape parity matrix 留给下一阶段。

## M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线

M7c/M7d 设计基线已固定，文档入口为
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。

该基线建立在 M6 resource/drop facts、M7a `BodyFlowGraph` / `BodyLoanCheckResult` / `BorrowSummary` 和 M7b
`FunctionBorrowContract` / reborrow / two-phase receiver 之上。后续 M7c 目标是完整 safe borrow 与 lifetime core：
contextual `origin` 参数、`&[origin] T` / `&mut[origin] T`、origin union、region solver、type-outlives、
trait/generic lifetime predicates 和 `BorrowEscapeAnalyzer` 退场。M7d 目标是 RAII/dropck 地基：dropck facts、
destructor body safety、generic drop glue type-outlives、place-level resource state、partial move/reinit/drop flag
以及 RAII user surface。

语法决策：不采用 Rust apostrophe lifetime，也不新增 `ref[...] T` 作为 source surface。显式 origin 绑定到现有引用前缀：
`&[data] T` 表示 shared borrowed view，`&mut[data] T` 表示 mutable borrowed view，`&[left | right] T`
表示 origin union。函数边界继续优先使用 `@borrow(return = [...])`。
该语法只在 type context 中解析，不占用未来 lambda/closure 的表达式语法空间。完整 closure/lambda capture 后续独立设计；
M7c/M7d 只预留 `ClosureCaptureFact` / `ClosureEnvironmentFact` 与 dropck/place-state 复用路线。

工程决策：`src/sema/internal/` 以后只作为 private implementation root，不再直接新增文件；M7c/M7d 新代码必须按
`borrow/`、`lifetime/`、`dropck/`、`place/`、`diagnostics/`、`pipeline/` 等职责拆子目录。其他 compiler stage
同样遵守该解耦规则。全局 `compiler-engineering` 与 `cpp-project-standards` skill 已同步该约束。

## M7b Borrow Contract、Reborrow 与 Lifetime Surface 实现收口

M7b WP1-WP7 已完成实现收口，文档入口为
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线](m7b-borrow-contract-design.md) 和
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 路线图](m7b-roadmap.md)。

M7b 已把 M7a 的 `BorrowSummary` / `BodyLoanCheckResult` 内部 facts 提升为函数边界
`FunctionBorrowContract`，引入函数声明前装饰器式 `@borrow(return = [param, self])`，补齐 trait/generic
borrowed-return contract、summary-vs-contract enforcement、reborrow parent/child loan、method receiver access、
receiver auto-borrow two-phase reservation/activation，以及 query/cache/tooling 投影。

当前实现包括：

- `CheckedModule::borrow_contracts`、`FunctionBorrowContract` fingerprint/dump/query 投影。
- `BodyLoan::parent_loan` reborrow model，child effective place 归一到 parent 底层 place，并保留 known-disjoint
  field projection 放宽。
- `FunctionCallBinding` / `TraitMethodCallBinding` 的 `receiver_access`、`receiver_auto_borrow` 和
  `receiver_two_phase_eligible`。
- `BodyTwoPhaseBorrow` reserve/activate facts，reservation conflict 与 activation conflict diagnostics。
- `TypeCheckBodyAuthority` 的 borrow contract/body loan fingerprint、reborrow/two-phase counts 和
  diagnostics-emitted 状态位。
- IDE semantic fact detail 中的 `borrow_contract` 与 `body_loan_check loans/reborrows/two_phase/conflicts`。

M7b 明确不做 full Rust-style lifetime generics、full Polonius Datalog、raw pointer alias safe proof、partial
move / replace / take / swap 完整 place-level resource semantics、`dyn Trait`、async drop、generator borrow
或用户级析构器语法。M7d-C 后续已补上窄 `impl Drop` / `deinit` semantic surface；M7d-D 后续已补上静态
custom destructor call lowering。
M7d-B 后续已补上本地 owned struct field partial move/reinit/drop flag 子集，M7d-F 后续已补上本地 tuple
element partial move/reinit/drop flag 子集；indexed move-out 和 replace/take/swap 仍不属于当前已完成范围。

## M7a CFG-sensitive borrow facts release closure

当前实现阶段是 M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking。M7 设计基线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)，
执行路线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图](m7-roadmap.md)。

M7-WP2 Phase 1 已完成 collect-only BodyFlowGraph facts：`CheckedModule::body_flow_graphs` 按
`FunctionLookupKey` 保存函数体 point、edge、place 和 action timeline；内部收集器覆盖 statement/expression
entry-exit、顺序点、branch、return、call、defer cleanup、read/write/move-candidate 以及 shared/mutable
borrow action，并提供稳定 dump。该阶段只生产事实，不新增 diagnostics，不替换 `BorrowEscapeAnalyzer`，不改变
M6 move/resource/cleanup 行为。

M7-WP3 Phase 2/3 已完成本地 local loan checker：`CheckedModule::body_loan_checks` 现在保存
`Origin` / `Loan` / conflict facts、shadow/enforced diagnostic mode、projection-aware conflict 结果和稳定
dump。checker 复用 Phase 1 `BodyFlowGraph`，用 carrier-local liveness 支持直接本地 borrow 的 last-use 后写入，
对 active shared/mutable loan 与 write、owned-consume move、read、shared/mutable borrow 的冲突进行 shadow 记录；
语义管线已启用 enforced diagnostics，并给出冲突 primary diagnostic 和 loan 创建 note。`move_candidate` 只有在
M6 `OwnedUseMode::owned_consume` 时才作为 move 冲突，普通 read/copy 不误报。

M7-WP4 已完成 `BorrowSummary` 与 borrowed-return facts：`CheckedModule::function_calls` 记录普通函数、泛型函数、
普通方法和泛型方法的 direct call binding；`CheckedModule::borrow_summaries` 按 `FunctionLookupKey` 保存函数级
`FunctionBorrowSummary`，包含 parameter/local/temporary origins、return origin dependency set、unknown-return
标志、local/temporary escape 标志和 stable fingerprint。call wrapper 会把 callee summary 的 parameter dependency
映射到 caller 实参；generic parameter 与 associated projection 在 summary 内按可能含借用处理，保证 `T = &U`
实例不会漏掉 parameter-origin dependency；函数值调用、callee 缺 summary、raw/unchecked pointer path 和 callee
local/temporary return 不会被当作 safe proof，而是 conservative unknown。

M7-WP5 已补齐 projection/drop/reinit/cleanup matrix：整 local assignment 生成 `reinit`，field/index/deref
assignment 保持 `write`，词法 block local cleanup 生成 `cleanup_storage` invalidation；loan checker 对
write/reinit/drop/cleanup/move/read/shared-borrow/mutable-borrow 使用同一套 projection-aware conflict policy。当前语言
仍没有显式 user drop syntax，所以 `drop` 已作为 checker action 支持，source-level drop emission 仍等待后续语法或
lowering 接入。

M7-WP6 已完成 diagnostics、query 与 tooling projection：`TypeCheckBodyAuthority` 混入 borrow summary / body
loan check fingerprint、origin/dependency/loan/conflict count 和 unknown/local-escape/diagnostic-emitted 状态位；
CLI incremental-cache subject collection 与 IDE snapshot query collection 都消费同一份 `CheckedModule`
facts。IDE semantic facts 新增 `borrow_summary` 与 `body_loan_check`，函数 hover 可展示 summary dependency；
enforced diagnostics 现在包含 primary conflict、loan creation、invalidating action 和可定位时的 later carrier use
note，同时保持按冲突点/range 的 cascade suppression。`dump_checked_module` 输出 `body_loan_checks` summary 与
stable fingerprint。

W7a release closure 已完成性能/内存边界收口：普通 `--check` 路径保留 `BodyLoanCheckResult` 与
`FunctionBorrowSummary` 等稳定 checked facts，不长期保留 full `BodyFlowGraph`；checked/typed 输出和 IDE/tooling
仍可保留 CFG facts。非借用返回函数的 summary 构建走 fast path，不扫描完整函数体；direct/trait call binding
由 `CheckedModule` 维护 expr-id index。release 全量测试、coverage、query sanitizer、perf/stress gate 已验证通过。

M7-WP7 已完成 release closure 文档边界：当前仍不移除 `BorrowEscapeAnalyzer`。WP4/WP6 summary 已记录
borrowed-return facts，但旧 borrowed-local escape 诊断继续由现有 analyzer 负责；只有在 parity 覆盖当前
borrowed-view 逃逸矩阵后才讨论降级/移除。M7a 继续明确不做完整 Rust-style lifetime surface、full Polonius
Datalog engine、raw pointer alias safe proof、用户级析构器语法、partial move / replace / take / swap
完整 place-level resource semantics、`dyn Trait`、async drop 或 generator borrow。M7d-C 后续已补上窄
`impl Drop` / `deinit` semantic surface；M7d-D 后续已补上静态 custom destructor call lowering。

## M6-WP2/WP3/WP4/WP5/WP6/WP7 资源、cleanup、drop-glue 与 tooling 基线

M6 Resource And Access Semantics 已作为 M7 输入基线收口。M6-WP1 已完成三轮设计审视，完整设计基线记录在
[Aurex M6 资源、值生命周期与访问语义调研和三轮设计审视基线](m6-resource-access-semantics-design.md)，
执行路线记录在 [M6 资源、值生命周期与访问语义路线图](m6-roadmap.md)。

M6-WP2/WP3/WP4/WP5/WP6/WP7 已完成 M6 实现基线：compiler-owned `Copy`、内部 `Discard` / `NeedsDrop` /
ownership resource summary、结构化类型分类、stable resource fingerprint、checked dump resource summary、
expression owned-use side table、whole-local move analysis、move 后重新初始化、consume-origin diagnostics、
lexical cleanup-action stack lowering、`defer` 组合、drop flag，以及正式 IR `drop` / `drop_if` cleanup 节点。
收口内容还包括 `BodySlotKind::destructor_drop` destructor body identity、stable drop-glue key、
target-independent drop-glue planner、IDE resource hover projection、generic parameter hover fallback 和
`aurex-lsp` stdio 入口。M7d-B 后续已开放本地 owned struct field partial move/reinit，并在 IR lowering 中使用
字段级 drop flag；M7d-C 后续已开放窄 `impl Drop` / `deinit` semantic surface；M7d-F 后续已开放 tuple
数字字段访问和本地 tuple 元素 partial move/reinit/drop flag。indexed move-out、consuming pattern payload 和
non-`Copy` `?` payload transfer 仍然拒绝；backend custom destructor call lowering 已由 M7d-D 补上。

下一实现包是 M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking。完整 borrow checker、lifetime surface、
partial move、`dyn Trait`、region、async drop、全量 array ABI 解禁和标准库重建继续后移。

## M5 default trait methods release baseline

当前文档基线是 M5 default trait methods release。M5 建立在已经收口的 M4 trait/protocol release baseline
之上，已经收口一个聚焦的 M4 后设计流：nominal static trait 上的 default method body。

M5 设计基线记录在
[Aurex M5 Default Trait Methods 调研与设计基线](m5-default-trait-methods-design.md)，阶段路线见
[M5 Default Trait Methods 路线图](m5-roadmap.md)，发布契约记录在
[Aurex M5 Default Trait Methods Release Baseline](m5-release-baseline.md)。M5 范围是 trait-owned default
bodies、显式 method origin、impl override vs inherited-default completeness、单态化后的 static direct-call
lowering，以及 tooling / diagnostics / query projection。

M5 不包含 dynamic trait object、object safety、vtable ABI、specialization、associated constants、default
associated types、generic associated types、blanket impl、package-level coherence expansion、class-like sugar 或
resource semantics。

## M4 trait/protocol release baseline

当前已实现的 trait 基线是 M4。M4 建立在已经收口的 M2 language-core-no-std、M2.5 frontend/query foundation
和 M3 module/generic/query-backed compiler architecture 之上，完成 nominal static trait、显式 trait impl、
generic trait predicate、static trait method dispatch、associated type，以及 IDE/tooling/diagnostics 投影。

发布契约记录在 [Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md)。M5 从该基线出发，不重新打开
M4 WP1-WP8。

## M2 language-core-no-std

M2 从 M1 的失败经验中收缩出来：停止继续修补 M1 的标准库、自举和系统样例路线，转而冻结标准库、删除干扰项，并重新聚焦语言核心设计。

M1 已经舍弃。它的问题是扩张面过大：标准库、host support、构建工具样例、自举实验和语言语义同时推进，导致核心语法与类型规则没有稳定下来。M2 不再把 M1 的 std/selfhost/build-tool 产物当作当前基线。

已完成：

- 删除 `std/` 源树和 host-c support。
- 删除 driver 标准库查找、import path 注入、support source 链接和相关头/源文件。
- 删除 CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 删除安装规则中的 `share/aurex/std`。
- 删除 std/M1/system 样例和 std 专项测试。
- 将 `Result` / `Option` 相关语言样例改为自包含定义。
- 将 defer/variadic 样例改为局部 `extern c`，不依赖 std 包装。
- 移除语义层 `std.core.vec/map/result` 专用 ownership 约束。
- 移除 M1 的语言级 `move(...)`、`noncopy struct` 和 use-after-move 追踪。
- 更新测试清单，使 sample suite 回到语言核心。

当前有效基线：

- C++20 Stage0 编译器。
- 手写 lexer/parser 和 ID-backed AST。
- sema、Aurex IR、IR verifier、pass pipeline。
- LLVM backend 和 clang native 输出。
- 自包含 positive/negative `.ax` 语言样例。

风险控制：

- 保留 match payload、`?`、`for`、`defer` 和基础类型系统的语言级正/负例。
- 保留 native hello、IR/LLVM lowering、安装后 compiler 执行验证。
- 后续资源约束不再通过标准库 hardcode 添加，应作为独立资源语义专题设计。

明确暂缓：

- 重新设计库层；不是复活旧 `std`。
- 重新评估 selfhost/Stage1。
- 重新设计 M1 build tool / system examples。
- 重新评估 host support 自动链接。

这些内容必须等 M2 基础语法、值语义、`unsafe`、slice/string 和泛型约束稳定后再重新评估；拥有型资源库还需要资源语义专题完成。
