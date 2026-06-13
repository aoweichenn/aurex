# 下一步计划

## 当前最高优先级：标准库 / Owning Dyn Runtime Surface 入口评估（保留锚点）

这个标题保留为 M8-M20 dyn/runtime 文档测试和后续路线索引的稳定锚点。当前阶段只保留入口评估语义，
不实现标准库、allocator API、runtime helper、`Box<dyn Trait>`、owning dyn 用户值或 dynamic Drop runtime。

## 当前实现入口：M21-M27 宏系统主线已开启，M27d output contract admission 已收口

M27 已把 Aurex 自己风格的用户宏声明入口落到 parser / AST / query / early expansion admission gate：
`macro Name { ... }`、`macro derive Name { ... }` 和 `macro const Name { ... }` 都可被解析和索引，AST 记录
`ItemKind::macro_decl`、`MacroDeclKind::{declarative, derive, compile_time}`、macro body token tree、match clause
count 和 delimiter balance；`expand_early_item_macros_noop()` 会生成 `AurexMacroSurfaceAdmissionGate`，summary /
dump / fingerprint 中会出现 `aurex_macro_surface_source_items`、`aurex_macro_surface_admissions`、
`aurex_macro_declarative_surfaces`、`aurex_macro_user_derive_surfaces` 和
`aurex_macro_compile_time_surfaces`。Query 层新增 `m27_macro_expansion_plan_baseline()`、
`is_valid_m27_macro_expansion_plan()`、`aurex_declarative_macro_surface`、
`aurex_user_derive_macro_surface` 和 `aurex_compile_time_macro_execution_admission`。

M27 仍是 admission-only：不采用 Rust `macro_rules!` / `$matcher` 写法，不展开宏、不执行用户编译期代码、不 parse /
merge generated module part、不修改 AST、不生成 sema-visible item、不生成用户代码、不引入标准库/runtime/external process。
M27b 已新增 `AurexMacroDefinitionSiteHygieneAdmissionGate` 和 `AurexMacroTypedMatcherAdmissionGate`，当前能索引
`match expr_list(xs) -> { xs }`、`match item(target) -> { target }` 和
`match tokens(input) -> { input }`，summary/dump/fingerprint 会记录
`aurex_macro_definition_site_hygiene_gates`、`aurex_macro_typed_matcher_admissions`、definition-site mark、fresh name
scope、diagnostic anchor、matcher fingerprint 和 matcher identity；Query 层新增
`m27b_macro_expansion_plan_baseline()`、`aurex_macro_typed_matcher_admission`、
`aurex_macro_definition_site_hygiene_admission` 和 `aurex_macro_debuggable_diagnostic_anchor`。
typed matcher execution is admission-only in M27b；definition-site hygiene resolution is admission-only in M27b；
仍不展开宏/不执行用户编译期代码/不消费 parser/不修改 AST。

M27c 已新增 `ItemKind::macro_call`、`AurexMacroCallSiteAdmissionGate`、
`AurexMacroMatcherToCallBindingAdmissionGate` 和 `AurexUserDeriveTargetSchemaAdmissionGate`。当前能索引
`macro call Name { ... }` 的调用点 token tree，能判断同模块同名 macro surface 是否存在，能把 declared call-site
绑定到第一个 recognized typed matcher admission gate，并能在 `#[derive(Name)]` 匹配 `macro derive Name { ... }`
时记录目标 `struct` / `enum` schema。Summary/dump/fingerprint 会记录
`aurex_macro_call_site_source_items`、`aurex_macro_call_site_admissions`、
`aurex_macro_matcher_to_call_bindings`、`aurex_user_derive_target_schema_source_derives` 和
`aurex_user_derive_target_schemas`；Query 层新增 `m27c_macro_expansion_plan_baseline()`、
`aurex_macro_call_site_admission`、`aurex_macro_matcher_to_call_binding_admission` 和
`aurex_user_derive_target_schema_admission`。macro call-site expansion is admission-only in M27c；
matcher-to-call binding execution is admission-only in M27c；user derive target schema is admission-only in M27c。

M27d 已新增 `AurexMacroOutputContractAdmissionGate`、
`AurexMacroOutputDeclaredNamePolicyAdmissionGate` 和
`AurexMacroOutputDiagnosticProjectionAdmissionGate`。当前能为 matcher-to-call binding 和 user derive target schema
预留 compiler-owned future output token buffer identity、generated `ModulePartKey`、declared-name policy 和 diagnostic
projection；Summary/dump/fingerprint 会记录 `aurex_macro_output_contracts`、
`aurex_macro_output_declared_name_policies` 和 `aurex_macro_output_diagnostic_projections`；Query 层新增
`m27d_macro_expansion_plan_baseline()`、`aurex_macro_output_contract_admission`、
`aurex_macro_output_declared_name_policy_admission` 和 `aurex_macro_output_diagnostic_projection_admission`。
macro output parser consumption remains blocked in M27d；macro output declared names are hidden from lookup/export/sema
in M27d；macro output diagnostics are projected but parser emission remains blocked in M27d。下一步建议进入 user derive
output schema design：为 `macro derive Name` 设计 capability/output 声明、impl/item output 分类和错误定位，仍不执行
lowering、不消费 parser、不修改 AST。

## 当前实现入口：M21-M26 宏系统主线已开启，M26c builtin derive cursor rollback AST mutation verifier closure 已收口

M21a 已完成宏系统设计 gate；M21b 已把第一块 frontend 地基落到代码：`AttributeDecl` /
`AttributeTokenDecl` / `ItemNode::attributes` 保存通用 item attribute token tree，`#[derive(Copy, Eq, Hash)]`
继续兼容现有内建 derive capability。非 `derive` item attribute 现在可以被 parser/AST/dump 索引，但 sema 会明确报错
`item attribute macros are parsed but macro expansion is not implemented yet`。

M21c 已把该输入面接到 query-level early item expansion facts：`MacroExpansionPlan` 固定 attribute token-tree input、
builtin derive passthrough、early item expansion query key、`SourceRole::generated` / `ModulePartKind::generated`
no-op generated module part、expansion source map stub、unimplemented item attribute blocker 和 external procedural
macro future blocker。M21c 仍不生成用户代码，也不实现标准库、external procedural macro 或 typed expression macro。

M21d 已把 M21c plan 接到真实 frontend pipeline boundary：`FrontendPipeline::load_modules()` 会在 module loading /
AST combine 之后、sema 之前运行 `macro.expand_items`，产出 `EarlyItemExpansionResult`、每个 parsed attribute 的
query-key fingerprint、generated part placeholder 和 source-map placeholder。M21d 仍不修改 AST、不 parse / merge
generated module part、不执行 external procedural macro、不实现 typed expression macro、不引入标准库，也不生成用户代码。

M21e 已把 generated module part parse / merge stub contract 固定到同一 `macro.expand_items` boundary：每个
generated placeholder 都有 `GeneratedModulePartParseMergeStub`，并记录 deterministic `generated_buffer_identity`、
`parse_config_fingerprint`、`merge_ordering_key`、expansion origin、buffer name 和 `merge_blocked` lifecycle。
M21e 仍不修改 AST、不 parse / merge generated module part、不执行 external procedural macro、不实现 typed expression
macro、不引入标准库，也不生成用户代码。

M21f 已把 hygiene / source-map / debug trace 的结构化 stub contract 固定到同一 `macro.expand_items` boundary：
每个 macro input 都有 `ExpansionHygieneStub` 和 `ExpansionTraceStub`，并记录 deterministic `call_site_mark`、
`definition_site_mark`、`generated_fresh_mark`、`declared_name_set`、`trace_identity`、
`generated_source_map_identity`、`diagnostic_anchor`、`origin_mark_hygiene_v1` 和
`expansion_source_map_debug_trace_v1`。M21f 仍不修改 AST、不 parse / merge generated module part、不执行 external
procedural macro、不实现 typed expression macro、不引入标准库，也不生成用户代码。

M21g 已把 generated item / declared generated names 的结构化 stub contract 固定到同一 `macro.expand_items`
boundary：每个 macro input 都有 `GeneratedItemDeclarationStub` 和 `DeclaredGeneratedNameStub`，并记录 deterministic
`declaration_identity`、`generated_item_key`、`declared_name_identity`、`hygiene_mark`、internal generated item
name、`attached_item_codegen_declared_names_v1`、M21f `declared_name_set` 以及对应 M21d/M21e generated module
part。M21g 仍不 materialize tokens、不 parse / merge generated module part、不让 declared generated names 参与
lookup/export/sema、不执行 external procedural macro、不实现 typed expression macro、不引入标准库，也不生成用户代码。

M21h 已把 compiler-owned token materialization admission / empty generated token buffer 的结构化 stub contract
固定到同一 `macro.expand_items` boundary：每个 macro input 都有 `TokenMaterializationAdmissionStub` 和
`GeneratedTokenBufferStub`，并记录 deterministic `token_plan_identity`、`token_buffer_identity`、
`source_map_identity`、`trace_identity`、`hygiene_mark`、token stream name、
`compiler_owned_attached_item_token_materialization_admission_v1` 和 `compiler_owned_empty_token_stream`。M21h
仍不 materialize tokens、不生成 source text、不让 token buffer 被 parser 消费、不 parse / merge generated module
part、不执行 external procedural macro、不实现 typed expression macro、不引入标准库，也不生成用户代码。

M21i 已把 compiler-owned generated token buffer prototype 固定到同一 `macro.expand_items` boundary：`derive`
input 现在有 `compiler_owned_builtin_derive_token_stream_prototype`、`materialization_identity`、
`compiler_owned_builtin_derive_token_producer_prototype_v1` 和 `GeneratedTokenRecord` facts；record 使用
`derive_source_token_placeholder` internal spelling，保持 `parser_visible=false` 和 `produced_user_generated_code=false`。
非 `derive` item attribute 继续保持 `compiler_owned_empty_token_stream` 和
`compiler_owned_blocked_empty_token_producer_v1`，不产生 records。M21i 仍不生成 source text、不让 generated token
buffer 被 parser 消费、不 parse / merge generated module part、不执行 external procedural macro、不实现 typed
expression macro、不引入标准库，也不生成用户代码。

M21j 已把 generated token buffer parser admission gate 固定到同一 `macro.expand_items` boundary：每个 macro
input 都有 `GeneratedTokenParserAdmissionGateStub`，并绑定 M21e `generated_buffer_identity` /
`parse_config_fingerprint`、M21i `token_plan_identity` / `token_buffer_identity` /
`materialization_identity`、source-map identity、hygiene mark 和 token stream name。policy 固定为
`compiler_owned_generated_token_parser_admission_gate_v1`。`derive` gate 可以记录
`token_buffer_materialized=true` 和 `token_records_available=true`，但所有 gate 仍固定
`parser_admitted=false`、`parse_ready=false`、`parser_consumable=false`、`generated_part_parsed=false`、
`generated_part_merged=false`、`sema_visible=false` 和 `produced_user_generated_code=false`。

M21k 已把 parser admission diagnostic / dump projection 固定到同一 `macro.expand_items` boundary：每个 macro
input 都有 `ParserAdmissionDiagnosticProjectionStub`，并绑定 M21j `parse_gate_identity`、M21e
`generated_buffer_identity` / `parse_config_fingerprint`、M21i token buffer / materialization identity、source-map
identity、hygiene mark、M21f trace identity、source anchor 和 token-tree anchor。policy 固定为
`parser_admission_blocked_diagnostic_projection_v1`。projection 会区分
`empty_token_buffer_parser_admission_blocked` 和 `derive_token_buffer_parser_admission_blocked`，同时记录 token
buffer admission blocker 与 generated module part parse blocker；但所有 projection 仍固定
`parser_admitted=false`、`parse_ready=false`、`parser_consumable=false`、`generated_part_parsed=false`、
`generated_part_merged=false`、`emit_expanded_available=false`、`debug_trace_available=false`、
`source_map_available=false` 和 `produced_user_generated_code=false`。

M21l 已把 parser admission diagnostic report / query projection 固定到同一 `macro.expand_items` boundary：每个
M21k diagnostic 都有 `ParserAdmissionDiagnosticReportEntry`，每个 generated module part 都有
`ParserAdmissionDiagnosticReport`。report entry 绑定 diagnostic identity、diagnostic anchor、parse gate identity、
source anchors、category、debug projection 和 `m21l-parser-admission-report:<module>:<part>` query projection name；
report 绑定 M21e `generated_buffer_identity` / `parse_config_fingerprint`，并固定
`parser_admission_blocked_report_query_projection_v1`、`report_identity`、`report_anchor_identity`、
`report_grouping_identity`、blocked / derive / empty / token-record-available totals 和
`source_anchor_ordered=true`。M21l 仍固定 `parser_admitted=false`、`parse_ready=false`、
`parser_consumable=false`、`emit_expanded_available=false`、`debug_trace_available=false`、
`source_map_available=false` 和 `produced_user_generated_code=false`。

M21m 已把 generated token parser consumption readiness preflight 固定到同一 `macro.expand_items` boundary：
每个 macro input 都有 `GeneratedTokenParserReadinessPreflightEntry`，并绑定 M21i token buffer、M21j parser
admission gate、M21k diagnostic projection、M21l report entry 和 M21f source-map / hygiene / trace facts。preflight
固定 `generated_token_parser_consumption_readiness_preflight_v1`、`preflight_identity`、
`derive_token_buffer_parser_input_candidate` / `empty_token_stream_parser_input_blocked`、token index continuity、
delimiter balance、source-anchor coverage、parse config compatibility 和 diagnostic projection availability；
但仍固定 `parser_admitted=false`、`parse_ready=false`、`parser_consumable=false`、
`generated_part_parsed=false`、`generated_part_merged=false` 和 `produced_user_generated_code=false`。

M21n 已把 parser consumption contract gate 固定到 generated module part 级：每个 generated part 都有
`GeneratedTokenParserConsumptionContractGate`，并绑定 M21e `generated_buffer_identity` /
`parse_config_fingerprint`、M21l `report_identity`、M21m preflight group、`contract_identity`、
`contract_grouping_identity`、`contract_anchor_identity` 和
`m21n-parser-consumption-contract:<module>:<part>` query name。contract gate 仍固定
`parser_admitted=false`、`parse_ready=false`、`parser_consumable=false`、`generated_part_parsed=false`、
`generated_part_merged=false`、`sema_visible=false`、`emit_expanded_available=false`、
`debug_trace_available=false`、`source_map_available=false` 和 `produced_user_generated_code=false`。

M21o 已把 M21m/M21n 汇总为 `MacroExpansionBoundaryClosureReport`，并把当前 result name 推进到
`M21o Macro Expansion Boundary Release Closure`。closure 固定
`m21_macro_expansion_boundary_release_closure_v1`、`m21o-macro-boundary-closure`、`closure_identity` 和
`closure_grouping_identity`，并汇总 macro input、generated part、parser admission report、preflight entry、
contract gate、blocked contract gate 和 parser-consumable contract gate counts。M21o 仍不执行 external
procedural macro，不生成 source text，不 parse / merge generated module part，不打开 parser consumption，不实现
标准库或 runtime helper。

M22a-M22f 已完成 builtin derive parser release gate / release hardening 准备：M22a 新增 `BuiltinDeriveExpansionAdmissionGate` 和
`builtin_derive_expansion_admissions`，固定 `builtin_derive_expansion_admission_gate_v1`、`admission_identity`、
`m22a-builtin-derive-admission:<module>:<part>:<item>:<attr>:<name>` query name、derive/non-derive admission kind
和 capability candidate 计数；M22b 新增 `BuiltinDeriveSemanticExpansionPlan` 和
`builtin_derive_semantic_plans`，复用现有内建 `#[derive(Copy, Eq, Hash)]` capability path，固定
`builtin_derive_semantic_expansion_plan_v1`、`capability_fact_lowering_plan`、`capability_set_identity`、
`semantic_plan_identity`、target kind 和 Copy/Eq/Hash capability 计数；M22c 新增
`BuiltinDeriveParserConsumptionReleaseGate` 和 `builtin_derive_parser_release_gates`，按 generated module part 汇总
M22a/M22b，绑定 M21n `contract_identity` 与 M21o `closure_identity`，固定
`builtin_derive_parser_consumption_release_gate_v1`、`m22c-builtin-derive-parser-release:<module>:<part>`、
rollback diagnostics/debug trace/source-map/hygiene prerequisites，并把当前 result name 推进到
`M22c Builtin Derive Parser Consumption Release Gate`；M22d 新增 `BuiltinDeriveReleaseHardeningMatrix` 和
`builtin_derive_release_hardening_matrices`，固定
`builtin_derive_release_hardening_matrix_v1`、`m22d-builtin-derive-release-hardening:<module>:<part>`、part-local /
cross-part totals 和 multi-item negative matrix；M22e 新增 `BuiltinDeriveDebugDumpStabilityContract` 和
`builtin_derive_debug_dump_contracts`，固定
`builtin_derive_debug_dump_stability_contract_v1`、`m22e-builtin-derive-debug-dump:<module>:<part>`、stable ordering、
identity projection、summary projection 和 drift-debuggable contract；M22f 新增
`BuiltinDeriveRollbackDiagnosticDesignGate` 和 `builtin_derive_rollback_diagnostic_gates`，固定
`builtin_derive_rollback_diagnostic_design_gate_v1`、
`m22f-builtin-derive-rollback-diagnostic:<module>:<part>`、M21n/M22c/M22d/M22e identity 链接和 diagnostic/report
totals，并把当前 result name 推进到 `M22f Builtin Derive Rollback Diagnostic Design Gate`。M22f 仍不执行用户自定义
macro，不打开 external procedural macro，不生成 source text，不 parse / merge generated module part，不打开 parser
consumption，不实现标准库或 runtime helper。

M23a-M23c 已完成 builtin derive parser consumption admission / checkpoint / pre-consumption verification 准备：
M23a 新增 `BuiltinDeriveParserConsumptionAdmissionProtocol` 和
`builtin_derive_parser_consumption_admission_protocols`，固定
`builtin_derive_parser_consumption_admission_protocol_v1`、
`m23a-builtin-derive-parser-consumption-admission:<module>:<part>`、M21n/M22c/M22f identity 链接和 part-local token
buffer / token record / derive candidate / empty candidate / blocked diagnostic counts；M23b 新增
`BuiltinDeriveParserConsumptionCheckpointRollbackProtocol` 和 `builtin_derive_checkpoint_rollback_protocols`，固定
`builtin_derive_parser_checkpoint_rollback_protocol_v1`、
`m23b-builtin-derive-checkpoint-rollback:<module>:<part>`、`checkpoint_count=3`、`rollback_plan_count=3`、diagnostic
replay prerequisite 和 rollback execution blocker；M23c 新增
`BuiltinDeriveParserPreConsumptionVerificationClosure` 和
`builtin_derive_preconsumption_verification_closures`，固定
`builtin_derive_parser_preconsumption_verification_closure_v1`、
`m23c-builtin-derive-preconsumption-verification:<module>:<part>`、M23a/M23b/M22d/M22e/M22f 可见性闭环，并把当前
result name 推进到 `M23c Builtin Derive Parser Pre-Consumption Verification Closure`。M23c 仍不执行用户自定义
macro，不打开 external procedural macro，不生成 source text，不 parse / merge generated module part，不打开 parser
consumption，不实现标准库或 runtime helper。

M24a-M24c 已完成 controlled builtin derive parser dry-run facts 准备：M24a 新增
`BuiltinDeriveControlledParserDryRunAdapter` 和 `builtin_derive_controlled_dry_run_adapters`，固定
`builtin_derive_controlled_parser_dry_run_adapter_v1`、
`m24a-builtin-derive-controlled-parser-dry-run:<module>:<part>`、M23c/M23a/M23b identity 链接、token record /
diagnostic anchor counts、`prerequisite_count=5` 和 dry-run execution blocker；M24b 新增
`BuiltinDeriveDryRunRollbackDiagnosticReplay` 和 `builtin_derive_dry_run_rollback_replays`，固定
`builtin_derive_dry_run_rollback_diagnostic_replay_v1`、
`m24b-builtin-derive-dry-run-rollback-replay:<module>:<part>`、M24a/M23b/M22f identity 链接、planned replay /
executed replay counts 和 rollback diagnostic replay execution blocker；M24c 新增
`BuiltinDeriveDryRunNegativeMatrixClosure` 和 `builtin_derive_dry_run_negative_matrices`，固定
`builtin_derive_dry_run_negative_matrix_closure_v1`、
`m24c-builtin-derive-dry-run-negative-matrix:<module>:<part>`、M24a/M24b/M23c 可见性闭环、
`negative_case_count=8`、`parser_consumable_case_count=0`，并把当前 result name 推进到
`M24c Builtin Derive Dry-Run Negative Matrix Closure`。M24c 仍不执行 real parser dry-run、不执行用户自定义 macro、
不打开 external procedural macro、不生成 source text、不 parse / merge generated module part、不打开 parser
consumption，不实现标准库或 runtime helper。

M25a-M25c 已完成 controlled builtin derive parser dry-run sandbox / check-only closure：M25a 新增
`BuiltinDeriveParserDryRunSessionBoundary` 和 `builtin_derive_parser_dry_run_sessions`，固定
`builtin_derive_parser_dry_run_session_boundary_v1`、
`m25a-builtin-derive-dry-run-session:<module>:<part>`、M24a/M24c/M21e identity 链接、token buffer candidate /
parser state snapshot counts、`committed_parse_count=0` 和 check-only uncommitted session blocker；M25b 新增
`BuiltinDeriveTokenCursorSnapshotRollbackProof` 和 `builtin_derive_token_cursor_snapshot_proofs`，固定
`builtin_derive_token_cursor_snapshot_rollback_proof_v1`、
`m25b-builtin-derive-token-cursor-rollback-proof:<module>:<part>`、M25a/M23b/M24b identity 链接、token cursor
snapshot、parser state rollback proof 和 `cursor_commit_count=0`；M25c 新增
`BuiltinDeriveDiagnosticShadowNoAstMutationClosure` 和
`builtin_derive_diagnostic_shadow_no_ast_mutation_closures`，固定
`builtin_derive_diagnostic_shadow_no_ast_mutation_closure_v1`、
`m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:<module>:<part>`、diagnostic replay shadow、
`executed_shadow_count=0`、`ast_mutation_count=0`、`parser_consumable_case_count=0`，并把当前 result name 推进到
`M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure`。M25c 仍不执行 real parser dry-run、不执行 diagnostic
shadow、不执行 rollback、不提交 session、不推进 parser cursor、不执行用户自定义 macro、不打开 external
procedural macro、不生成 source text、不 parse / merge generated module part、不打开 parser consumption、不修改 AST、
不实现标准库或 runtime helper。

M26a-M26c 已完成 builtin derive parser dry-run admission / recovery shadow / rollback verifier closure：M26a 新增
`BuiltinDeriveParserDryRunAdmissionGate` 和 `builtin_derive_parser_dry_run_admission_gates`，固定
`builtin_derive_parser_dry_run_admission_gate_v1`、
`m26a-builtin-derive-parser-dry-run-admission:<module>:<part>`、M25a/M25b/M25c/M21e identity 链接、
`admission_prerequisite_count=5`、token buffer / token record counts、`dry_run_execution_admitted_count=0` 和
`builtin derive parser dry-run execution admission remains blocked in M26a` blocker；M26b 新增
`BuiltinDeriveErrorRecoveryShadowDiagnosticGate` 和
`builtin_derive_error_recovery_shadow_diagnostic_gates`，固定
`builtin_derive_error_recovery_shadow_diagnostic_gate_v1`、
`m26b-builtin-derive-error-recovery-shadow-diagnostic:<module>:<part>`、M26a/M25c/M24b/M21l identity 链接、
diagnostic shadow / parser report / planned recovery counts、`executed_recovery_count=0` 和
`emitted_diagnostic_count=0`、`builtin derive error recovery shadow diagnostics remain non-emitting in M26b`
blocker；M26c 新增
`BuiltinDeriveCursorRollbackAstMutationVerifierClosure` 和
`builtin_derive_cursor_rollback_ast_mutation_verifier_closures`，固定
`builtin_derive_cursor_rollback_ast_mutation_verifier_closure_v1`、
`m26c-builtin-derive-cursor-rollback-ast-verifier:<module>:<part>`、M26a/M26b/M25a/M25b/M25c identity 链接、
cursor snapshot / rollback proof / recovery shadow counts、`ast_baseline_snapshot_count=1`、
`ast_mutation_count=0`、`cursor_commit_count=0`、`session_commit_count=0`、`parser_consumable_case_count=0` 和
diagnostic sink isolation、`builtin derive cursor rollback execution and AST mutation verifier remain check-only in M26c`
blocker，并把
当前 result name 推进到 `M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure`。M26c 仍不执行 real
parser dry-run、不准入 dry-run execution、不执行 error recovery、不发出 shadow diagnostic、不执行 rollback、
不提交 session、不推进 parser cursor、不执行用户自定义 macro、不打开 external procedural macro、不生成 source
text、不 parse / merge generated module part、不打开 parser consumption、不修改 AST、不实现标准库或 runtime helper。

M27b typed matcher / hygiene definition-site admission 已完成：继续保持 no-parser-consumption / no-stdlib /
no-runtime / no-external-procedural-macro / no-user-generated-code，不照搬 Rust `macro_rules!`，已经把
`match expr_list(xs) -> { ... }`、`match item(target) -> { ... }` 和 `match tokens(input) -> { ... }` 从 debug hint
升级为结构化 matcher facts，并把 definition-site hygiene、fresh name scope、diagnostic anchor、body fingerprint 和
query cache identity 固定清楚。M27c 已完成 macro call-site admission、matcher-to-call binding admission 和 user
derive target schema admission：`macro call Name { ... }` 能被 AST/tooling/query 索引，调用点 target surface
状态、call token fingerprint、binding identity 和 derive target schema 都进入 stable facts。M27d 已完成 macro
output contract admission：output token ownership、hygiene mark、declared-name policy 和 diagnostic projection 已成为
stable facts，但仍不展开、不执行用户代码、不消费 parser。之后应进入 user derive output schema / lowering design 或
compile-time execution sandbox。

## 已完成入口：M20g 默认参数 / 命名参数已收口，后续继续非标准库语言特性

M20e 已完成第一批编译器内建 derive 属性：`#[derive(Copy, Eq, Hash)]` 支持 `struct` / `enum`，
`Eq` / `Hash` 进入 checked capability facts，`Copy` 继续服从资源语义和 `impl Drop` custom destructor 事实。
这不是完整 macro/proc-macro 系统，也不生成标准库方法。

M20f 已把“结构体引用字段”按现有语言模型收口：它不引入新语法，而是固定 `&record.field`、
`&mut record.field`、字段 projection borrow、字段级 conflict/lifetime diagnostics 和 IR `field_addr`
lowering 的回归覆盖。实现复用 M7 的 `PlaceInfo`、projection-aware loan facts 和 field-disjoint conflict
判断；不同已知字段可以分离，同一字段写入和整个 parent overwrite 会在 later carrier use 时诊断，local field
reference 不能逃逸函数。该阶段不实现标准库，也不实现 resource-field overwrite helper、`take`/`swap` 之类库能力。

M20g 已把“默认参数 / 命名参数”按 source-level call sugar 收口：函数和 method 参数可带默认值，普通函数 /
inherent method / 泛型函数 / 泛型 method 可省略 defaulted 参数，普通函数、inherent method、trait/static/dyn method
call 可用命名参数重排实参。sema 会生成 checked `ordered_args`；IR lowering、borrow summary、body flow graph、
body loan precheck、place-state precheck、move analysis 和 borrow escape / lambda capture 扫描都消费归一化顺序。
该阶段不实现标准库，不打开 ABI 级 default metadata，也不支持 trait requirement defaults、C ABI / variadic
defaults、function value / lambda named calls 或 enum constructor named payloads。

接下来建议继续沿非标准库语言可用性推进，优先做会影响用户写法和静态语义的 compiler-only 能力：

- P1：结构体引用字段已完成。当前可用面是 source-level field borrow、field-disjoint loan separation、
  same-field/parent invalidation diagnostics 和 `field_addr` lowering；resource field partial move/overwrite
  仍以后续资源语义阶段处理。
- P2：默认参数 / 命名参数已完成第一版。后续只建议补“默认表达式可见前序参数”“function value/lambda 参数名元数据”
  这类独立小阶段，不应和标准库或 ABI metadata 混在一起。
- P3：完整 macro / proc-macro / 用户自定义 derive。当前不建议立刻推进；现有内建 derive 已解决 capability
  marker 的主要痛点，完整宏系统需要单独的卫生、增量、query/cache、错误恢复和安全边界设计。
- P4：闭包语义深水区。M20 已有 Copy-by-value 捕获闭包核心子集；下一步若继续函数式路线，应先做
  shared / mutable / consuming capture 和 borrow/resource/dropck 接入，而不是写标准库 adapter。

## 已完成入口：M20 捕获闭包核心子集

无捕获 lambda 已作为 `fn(...) -> T` 薄函数值落地；M20 捕获闭包核心子集也已完成。当前支持
非泛型依赖、非 borrowed-view 的 Copy-by-value 捕获、内部匿名 environment record、hidden-env thunk、直接调用、
嵌套捕获、match guard 捕获使用和函数返回捕获闭包。下一步若继续函数式主线，应补闭包语义深水区，而不是直接写标准库：

- 区分 shared / mutable / consuming capture。
- 定义 closure call ability，后续再决定是否命名为 `Fn` / `FnMut` / `FnOnce` 风格能力。
- 把捕获的 borrow/resource 接入现有 dropck、place-state、loan/lifetime facts。
- 设计 generic-dependent closure environment ABI 和实例化规则。
- 设计 borrowed closure environment escape 规则。
- 保持 closure-to-function-pointer 的唯一合法转换：只有无捕获 lambda 可以转换为薄 `fn(...) -> T`。

这一阶段仍不需要实现标准库、allocator 或 runtime helper；标准库函数式 adapter 应在 closure 核心语义稳定后再做。

## 当前已完成 dyn/runtime 背景

M8 borrowed dyn runtime dispatch、M9 dyn ABI/tooling release closure、M10 supertrait upcasting release closure 和
M11a Advanced Dyn Design Baseline、M11b Principal-Set Composition Query Prototype Gate、M11c Principal-Set
Composition Frontend / Sema Check-Only、M11d Principal-Set Composition IR / Backend Runtime、M11e Principal-Set
Composition Hardening / Release Closure、M12a Direct Principal-Qualified Composition Method Dispatch、M12b
Direct Composition Dispatch Hardening / Release Closure、M13a Advanced Dyn Remaining Policy Design Baseline、
M13b Borrowed Composition-To-Supertrait Frontend / Query / Sema Check-Only、M13c Borrowed Composition-To-Supertrait
IR / Backend Runtime、M13d Borrowed Composition-To-Supertrait Hardening / Release Closure、
M14 Borrowed Dyn View Path Inference / Dispatch Release、M16 Const Generic Frontend / Query / Sema Check-Only 和
M17 Dyn Ownership Runtime Preparation、M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate、
M19 Dyn Ownership Runtime IR / Verifier Preparation、M20a Owned Dyn Runtime Admission Design Gate、
M20b Owned Dyn IR Shape Prototype Gate、M20c Drop / Allocator Identity Prerequisite Gate 和
M20d Runtime Lowering ABI Design Closure 均已完成。
当前状态入口：

- [Aurex M8 Dyn Trait、Erased View 与动态派发设计基线](m8-dyn-trait-design.md)
- [Aurex M9 Dyn ABI / Tooling 设计基线](m9-dyn-abi-tooling-design.md)
- [Aurex M9 Dyn ABI / Tooling Release Baseline](m9-release-baseline.md)
- [Aurex M10 Supertrait Upcasting 设计基线](m10-supertrait-upcasting-design.md)
- [Aurex M10 Supertrait Upcasting Release Baseline](m10-release-baseline.md)
- [Aurex M11 Advanced Dyn Design Baseline](m11-advanced-dyn-design.md)
- [Aurex M11 Principal-Set Composition Release Baseline](m11-release-baseline.md)
- [Aurex M12 Direct Composition Dispatch Release Baseline](m12-release-baseline.md)
- [Aurex M13 Advanced Dyn Remaining Policy Design Baseline](m13-advanced-dyn-design.md)
- [Aurex M14 Borrowed Dyn View Path Inference Release Baseline](m14-borrowed-dyn-view-path-release.md)
- [Aurex M15 Advanced Dyn Ownership / Const Generic Boundary Design Baseline](m15-advanced-dyn-const-generic-design.md)
- [Aurex M16 Const Generic Frontend / Query / Sema Check-Only Release Baseline](m16-const-generic-check-only-release.md)
- [Aurex M17 Dyn Ownership Runtime Preparation Release Baseline](m17-dyn-ownership-runtime-prep-release.md)
- [Aurex M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate Release Baseline](m18-dyn-ownership-runtime-boundary-hardening-release.md)
- [Aurex M19 Dyn Ownership Runtime IR / Verifier Preparation Release Baseline](m19-dyn-ownership-runtime-ir-verifier-prep-release.md)
- [Aurex M20a Owned Dyn Runtime Admission Design Gate Release Baseline](m20-owned-dyn-runtime-admission-gate-release.md)
- [Aurex M20b Owned Dyn IR Shape Prototype Gate Release Baseline](m20-owned-dyn-ir-shape-prototype-release.md)
- [Aurex M20c Drop / Allocator Identity Prerequisite Gate Release Baseline](m20-owned-dyn-drop-allocator-identity-release.md)
- [Aurex M20d Runtime Lowering ABI Design Closure Release Baseline](m20-owned-dyn-runtime-lowering-abi-design-release.md)

M10 已结束。当前能稳定使用 borrowed dyn supertrait upcast：`&dyn Child -> &dyn Parent`、
`&mut dyn Child -> &mut dyn Parent` 和 `&mut dyn Child -> &dyn Parent`；`dyn Child` receiver 上 inherited parent
method call 会通过 `trait_object_upcast` IR 和 `supertrait_vptr_metadata_v1` parent vtable projection runtime dispatch。
M10d 已补齐 query/cache/tooling projection、negative sample matrix、documentation tests、coverage gate 和 M10c
实际代码量偏差分析。

M11a 也已结束。M11a 选择 **principal-set borrowed dyn composition** 作为下一条 advanced dyn 主线，并通过
`m11a_dyn_advanced_design_gate_baseline` 固定 `principal_set_metadata_v1`、`principal_set_identity_fact`、
`composition_witness_set_fact`、`principal_method_namespace_fact`、`associated_equality_merge_fact` 和
`composition_projection_fact`。M10 supertrait upcasting 在 gate 中标记为 `completed_release_baseline`；M11a 不重开
M10 runtime。

M11b 也已结束。M11b 新增 `PrincipalSetCompositionFacts`、`PrincipalSetIdentityFact`、
`CompositionWitnessSetFact`、`PrincipalMethodNamespaceFact`、`AssociatedEqualityMergeFact` 和
`CompositionProjectionFact`，并提供 validation、`principal_set_composition_facts_fingerprint()`、
`summarize_principal_set_composition_facts()`、`dump_principal_set_composition_facts()` 和 focused query tests。
M11b 固定了 principal-set 至少两个 principal、derived composition origin、canonical order、principal-qualified method namespace、
associated equality merge、projection 保持 data pointer/origin 和 aggregate summary/fingerprint consistency。

M11c 也已结束。M11c 固定当前用户可写 borrowed composition spelling 为 `dyn (A + B)`，并接入
parser/AST/type identity、borrowed concrete-to-composition coercion check、checked vtable layout recording、
`CompositionWitnessSetFact` / `CompositionProjectionFact`、associated equality merge diagnostics、method-call
guard、checked dump/fingerprint 和 `TypeCheckBodyAuthority` principal-set counts。M11c 是 check-only gate；
M11d 已移除该 runtime guard 并接入 composition pack/project lowering。

M11d 也已结束。M11d 新增 `trait_object_composition_pack` / `trait_object_composition_project` IR value、
`PrincipalSetMetadataLayout` / `PrincipalSetMetadataWitness`、IR verifier metadata/projection invariants、
LLVM `principal_set_metadata_v1` global emission 和 native execution coverage。当前 runtime representation 是 borrowed
`{data*, metadata*}` composition view；metadata global shape 为 `{ [N x ptr] }`，每个 entry 是 canonical principal
顺序上的 single-trait vtable witness。显式 projection `let draw: &dyn Draw = combo;` 会按 principal index 从
metadata 取出 vtable，并构造普通 `{data*, vtable*}` single dyn view。Direct `combo.draw()` 仍被 M11c/M11d 的
method-call guard 拒绝，等待 principal-qualified syntax/dispatch 设计。

M11e 也已结束。M11e 把 M11d runtime projection 收口到 release-quality facts/tooling/verifier/documentation：
`FunctionDynAbiFacts` 现在投影 `principal_sets` 和 `composition_projections`，lower-IR query fingerprint 混入
principal-set metadata/projection counts，IDE semantic fact 和 hover 展示 composition runtime facts，IR verifier
负例矩阵覆盖 duplicate witness、metadata identity drift、missing pack metadata、invalid projection principal index
和 missing principal object。M11e 没有打开 direct composition method dispatch，也没有实现标准库、owning dyn、
`Box<dyn Trait>`、allocator 或 dynamic Drop dispatch。

M12a 也已结束。M12a 打开 borrowed principal-set composition receiver 上的无歧义 direct method call：
`view.draw()` 会选择唯一提供 `draw` 的 principal，记录 composition-to-principal projection，再复用 ordinary
single-trait dyn `vtable_slot` dispatch。多个 principal 暴露同名 method 仍诊断 ambiguous，缺失 method 仍给
no visible impl；shared receiver 不能调用 `&mut Self` method。M12a 不实现标准库、owning dyn、`Box<dyn Trait>`、
allocator、dynamic Drop dispatch、bare `dyn A + B` syntax 或 composition-to-supertrait 的隐式多步 direct dispatch。

M12b 也已结束。M12b 把 direct composition dispatch 收口到 release quality：checked binding 的 receiver access
按投影后的 `dispatch_receiver_type` 计算，associated equality direct dispatch 已覆盖，direct dispatch 与显式
projection 混用时去重 projection fact 和 function-level ABI descriptor，query/cache fingerprint 会响应 projection
target drift，negative matrix 覆盖 composition-to-supertrait 隐式 direct call rejection。M12b 仍不实现标准库、
owning dyn、`Box<dyn Trait>`、allocator 或 dynamic Drop dispatch。

M13a 也已结束。M13a 从 M12 后剩余 advanced dyn 候选中选择 **borrowed composition-to-supertrait explicit
projection** 作为下一条主线，并通过 `m13a_dyn_advanced_design_gate_baseline()` 固定
`borrowed_composition_supertrait_projection`、`composes_existing_metadata_policies`、
`composition_to_supertrait_projection_fact`、`principal_supertrait_path_fact`、
`composition_supertrait_ambiguity_fact` 和 `composition_supertrait_projection_abi_descriptor`。M13a 的关键边界是：
组合已有 `principal_set_metadata_v1` 与 `supertrait_vptr_metadata_v1`，不新增 runtime metadata，不让
`view.parent()` 变成隐式 composition-to-supertrait direct call，不实现标准库、owning dyn、allocator 或 dynamic
Drop dispatch。

M13b 也已结束。M13b 把 M13a 的显式 borrowed composition-to-supertrait projection 落成 check-only 用户语法：
`dynproject<SourcePrincipal, TargetSupertrait>(view)`。`view` 必须是 borrowed principal-set composition，
source principal 必须在 principal set 中，target 必须是该 source principal 的 direct/transitive supertrait。
成功后 sema 返回 borrowed single-trait target view，并记录
`CompositionProjectionFact{kind=composition_to_supertrait}`；query dump/summary/fingerprint 已包含
`supertrait_projections`。M13b 不做 IR/backend lowering，也继续拒绝隐式
`let parent: &dyn Parent = view;` 和 `view.parent()`。

M13c 也已结束。M13c 把 `dynproject<SourcePrincipal, TargetSupertrait>(view)` 降低为已有 runtime IR 组合：
先用 `trait_object_composition_project` 从 `&dyn (A + B)` / `&mut dyn (A + B)` 取出 source principal 的
single-trait dyn view，再用 `trait_object_upcast` 沿 checked supertrait edge 取出 target supertrait vtable。
M13c 没有新增 runtime metadata policy；composition-supertrait projection 复用 `principal_set_metadata_v1` 和
`supertrait_vptr_metadata_v1`。Function dyn ABI facts 会同时暴露 composition projection 和 upcast descriptor；
IR/LLVM/native tests 已覆盖单 concrete、多 concrete 和 backend execution。隐式
`let parent: &dyn Parent = view;` 与 `view.parent()` 仍继续拒绝。

M13d 也已结束。M13d 把 M13c runtime surface 收口到 release quality：新增
`FunctionDynAbiFacts::composition_supertrait_chains` 派生 descriptor，summary/dump/fingerprint、
`lower_function_ir_result_fingerprint()`、IDE semantic fact 和 hover 都能显示
`composition_project -> upcast` 的完整 runtime chain。Verifier negative matrix 已覆盖缺失 upcast object、
错误 supertrait edge、source/target layout drift 和 projection principal drift。M13d 没有实现标准库、owning dyn、
`Box<dyn Trait>`、allocator、dynamic Drop dispatch、bare `dyn A + B` 或隐式 composition-to-supertrait direct call。

M14 也已结束。M14 在 M13 的 explicit runtime chain 上打开受限的 borrowed dyn view path inference：
`let parent: &dyn Parent = view;` 和 `view.parent()` 会在唯一 source-principal path 下解析为
`&dyn (Child + Debug) -> &dyn Child -> &dyn Parent`，并 lowering 为既有
`trait_object_composition_project` + `trait_object_upcast` + ordinary `vtable_slot`。M14 新增
`BorrowedDynViewPathFact`、`BorrowedDynViewPathUse`、borrowed view path summary counters、query fingerprint
mixing、summary/dump、sema/IR/native coverage 和 ambiguity negative case。M14 没有实现标准库、owning dyn、
`Box<dyn Trait>`、allocator、dynamic Drop dispatch、trait-object destructor ABI、bare `dyn A + B` 或新 runtime
metadata policy。

M15 也已结束。M15 没有实现标准库、owning dyn runtime、`Box<dyn Trait>`、dynamic Drop dispatch 或用户可写
const generic；它把 advanced dyn ownership/runtime boundary 和 const generic boundary 固定成两个 query design
gate。`m15_dyn_advanced_design_gate_baseline()` 把 M10/M11/M12/M13/M14 borrowed dyn 路径标为
`completed_release_baseline`，并把 owning dyn、dynamic Drop dispatch 和 allocator policy 留在 future standard
library/runtime/resource surface。`m15_const_generic_design_gate_baseline()` 选择 typed scalar const parameter、
canonical const value key、generic instance const arg key 和 `[N]T` array length integration 作为下一步路线；
const expression evaluation subset 仍受 comptime engine 阻塞，trait/dyn const predicate 仍受 trait solver extension
阻塞。

M17 也已结束。M17 没有实现标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering 或
dynamic Drop dispatch；它新增 `DynOwnershipRuntimeFacts`、`DynOwnedContainerBoundaryFact`、
`DynErasedDropGlueBoundaryFact`、`DynAllocatorBoundaryFact`、`DynCleanupDropckBoundaryFact` 和
`DynOwnershipRuntimeSummary`，把 future owning dyn、erased drop glue、allocator、cleanup/dropck boundary 固定成
query/tooling 可验证事实。`m17_dyn_ownership_runtime_preparation_baseline()` 和
`is_valid_m17_dyn_ownership_runtime_preparation_baseline()` 会拒绝 standard-library blocker、runtime-lowering blocker、
`Box` surface blocker、allocator API blocker、dynamic-drop blocker 或 borrowed-vtable destructor-free 事实漂移。

已完成基线摘要：

- M8 query foundation 已完成，`CanonicalTypeKind::trait_object` 占位已移除；当前结构化 identity 是
  `TraitObjectTypeKey`、`VTableLayoutKey`、`TraitObjectCoercionKey`。
- M9 Dyn ABI / Tooling Design Baseline、M9b ABI/tooling implementation、M9c Advanced Dyn Design Gate 和
  M9d / M9 release closure 已完成；M9 固定 `borrowed_view_v1` / `borrowed_methods_only_v1`，不实现标准库或 runtime
  advanced dyn。
- M10a / post-M9 advanced dyn policy selection 已完成；M10 选择 supertrait upcasting 并最终收口到 M10d release
  baseline。
- M11a Advanced Dyn Design Baseline 已完成；M11 选择 origin-bound borrowed dyn principal-set view，不选择 owning
  dyn、dynamic Drop dispatch 或 allocator policy 作为当前阶段主线。
- M11b Principal-Set Composition Query Prototype Gate 已完成；query facts 地基已固定。
- M11c Principal-Set Composition Frontend / Sema Check-Only 已完成；`dyn (A + B)` borrowed annotation/coercion
  和 checked facts 可用。
- M11d Principal-Set Composition IR / Backend Runtime 已完成；显式 composition-to-principal projection 可降低到
  IR/LLVM 并 native execution。
- M11e Principal-Set Composition Hardening / Release Closure 已完成；query/cache/tooling/verifier/docs release
  baseline 已固定。
- M12a Direct Principal-Qualified Composition Method Dispatch 已完成；无歧义 `combo.method()` 可降低为
  composition projection + ordinary dyn vtable dispatch。
- M12b Direct Composition Dispatch Hardening / Release Closure 已完成；receiver-access binding、associated
  equality direct dispatch、projection/ABI descriptor 去重、query fingerprint drift 和 negative matrix 已固定。
- M13a Advanced Dyn Remaining Policy Design Baseline 已完成；下一条主线选择 explicit borrowed
  composition-to-supertrait projection，并固定 query gate 与非目标。
- M13b Borrowed Composition-To-Supertrait Frontend / Query / Sema Check-Only 已完成；`dynproject<...>` 显式
  projection、typing、diagnostics、`composition_to_supertrait` fact、checked dump/fingerprint 和 query summary 已固定。
- M13c Borrowed Composition-To-Supertrait IR / Backend Runtime 已完成；`dynproject<...>` 会 lowering 为
  `trait_object_composition_project` + `trait_object_upcast`，并复用既有 LLVM metadata/runtime。
- M13d Borrowed Composition-To-Supertrait Hardening / Release Closure 已完成；query/cache/tooling hover、
  `composition_supertrait_chains`、verifier negative matrix、docs/tests/coverage release closure 已固定。
- M14 Borrowed Dyn View Path Inference / Dispatch Release 已完成；唯一 path 的 expected-type projection 和
  direct supertrait method dispatch 已固定，`BorrowedDynViewPathFact` 已进入 query/tooling facts。
- M15 Advanced Dyn Ownership / Const Generic Boundary Design Baseline 已完成；owning dyn / dynamic Drop /
  allocator 仍只是 future boundary，const generic typed scalar / canonical value / instance key / `[N]T`
  integration 路线已固定。
- M16 Const Generic Frontend / Query / Sema Check-Only 已完成；当前已支持用户可写
  `const N: usize` typed scalar const param、mixed generic args、`GenericInstanceKey::const_args`、const param env
  binding、函数体 `return N;` 和 `[N]T` check-only array length。M16 不做 generic const arithmetic、
  user function comptime evaluation、const where predicate、const associated value、dyn const equality dispatch、
  unresolved const-param array runtime ABI 或标准库 API。
- M17 Dyn Ownership Runtime Preparation 已完成；`DynOwnershipRuntimeFacts`、owned container / erased drop glue /
  allocator / cleanup-dropck boundary facts、summary/dump/fingerprint 和 validation 已固定。M17 不做标准库、
  `Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering 或 dynamic Drop dispatch。
- M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate 已完成；`DynOwnershipRuntimeBoundaryGate`、
  checkpoint facts、lowering design gate facts、project-level query/cache/tooling/reuse/workspace index 和 provider dependency
  validation 已固定。M18 仍不做标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering 或
  dynamic Drop dispatch。
- M19 Dyn Ownership Runtime IR / Verifier Preparation 已完成；`DynOwnershipRuntimeIrVerifierFact`、
  `FunctionDynOwnershipRuntimeIrVerifierFacts`、borrowed vtable destructor-free guard、dynamic erased drop blocked
  sentinel 和 verifier negative matrix 已固定。M19 仍不做标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、
  runtime ABI lowering 或 dynamic Drop runtime。
- M20a Owned Dyn Runtime Admission Design Gate 已完成；`OwnedDynRuntimeAdmissionGate`、admission facts、
  summary/dump/fingerprint 和 validation 已固定。M20a 只决定 M20b/M20c/M20d/M21 的进入顺序，仍不做标准库、
  `Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering、backend runtime helper call 或
  dynamic Drop runtime。
- M20b Owned Dyn IR Shape Prototype Gate 已完成；`OwnedDynObjectLayoutPrototype`、
  `OwnedDynIrShapePrototypeGate`、IR adapter、Module copy/move、dump、layout ABI fingerprint、verifier 和
  negative matrix 已固定 compiler-owned two-field owned dyn handle prototype。M20b 仍不做标准库、
  `Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering、backend runtime helper call 或
  dynamic Drop runtime。
- M20c Drop / Allocator Identity Prerequisite Gate 已完成；`OwnedDynDropAllocatorIdentityGate`、
  `OwnedDynDropAllocatorIdentityFact`、IR adapter、`erased_drop_identity_key` / `allocator_identity_key`、
  dump、layout ABI fingerprint、verifier 和 negative matrix 已固定 compiler-owned drop / allocator identity
  prerequisites。M20c 仍不做标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering、
  backend runtime helper call 或 dynamic Drop runtime。
- M20d Runtime Lowering ABI Design Closure 已完成；`OwnedDynRuntimeLoweringAbiGate`、
  `OwnedDynRuntimeLoweringAbiFact`、IR adapter、`runtime_abi_descriptor_key`、
  `backend_helper_identity_key`、summary/dump/fingerprint、M20c key-consistency validation 和 negative matrix 已固定
  compiler-owned runtime ABI design facts。M20d 仍不做标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、
  executable runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。

M18 也已结束。M18 新增 `DynOwnershipRuntimeBoundaryGate` project-level query，把 M17 facts 接入
query/cache/tooling/reuse/workspace index，并固定 future IR/verifier/runtime lowering prerequisites。M18 仍没有实现
标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering 或 dynamic Drop dispatch。

M19 也已结束。M19 新增 `DynOwnershipRuntimeIrVerifierFact`、
`FunctionDynOwnershipRuntimeIrVerifierFacts`、`function_dyn_ownership_runtime_ir_verifier_facts()`、
`TraitObjectVTableLayout::destructor_slot_blocked` 和
`CleanupAbiPolicy::dynamic_erased_drop_blocked` blocked negative sentinel，把 M18 的 future prerequisites 落成
verifier-visible IR facts、dump/fingerprint、collector 和 negative matrix。M19 仍没有实现标准库、`Box<dyn Trait>`、
allocator API、owning dyn 用户值、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。

M20a 也已结束。M20a 新增 `OwnedDynRuntimeAdmissionGate`、
`OwnedDynRuntimeAdmissionFact`、`OwnedDynRuntimeAdmissionCapability`、`OwnedDynRuntimeAdmissionStage`、
`OwnedDynRuntimeAdmissionPolicy` 和 `m20_owned_dyn_runtime_admission_gate_baseline()`，把 M17/M18/M19 facts
组合成 admission design gate，并固定 owned object layout、erased drop identity、allocator identity、runtime lowering
ABI、`Box<dyn Trait>` surface 和 borrowed dyn ABI separation 的后续顺序。M20a 仍没有实现标准库、`Box<dyn Trait>`、
allocator API、owning dyn 用户值、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。

M20b 也已结束。M20b 新增 `OwnedDynObjectLayoutPrototype` 和
`OwnedDynIrShapePrototypeGate`，把 owned dyn handle 固定为 compiler-owned `{*mut u8 data, *const u8 vtable}`
two-field prototype；drop/allocator runtime slot 继续是 blocked sentinel，IR dump/fingerprint/verifier 和 query adapter
均已覆盖。M20b 没有实现标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering、
backend runtime helper call 或 dynamic Drop runtime。

M20g 也已结束。M20g 新增默认参数和命名参数的 parser/AST/sema/lowering/borrow-place 分析闭环。
checked call binding 现在保存 `ordered_args`，默认值和命名参数在 sema 阶段归一化；IR lowering、borrow summary、
body flow graph、body loan precheck、place-state precheck、move analysis 和 borrow escape / lambda capture 扫描都
读取 normalized call arguments。M20g 不实现标准库、ABI 级 optional metadata、trait requirement defaults、C ABI /
variadic defaults、function value / lambda named calls 或 enum constructor named payloads。

Dyn/runtime 方向的下一步可以进入 **标准库 / owning dyn runtime surface 入口评估**，但这不是当前非标准库语言特性
阶段的直接实现任务。M20a-M20d 已把 admission order、
compiler-owned two-field handle shape、drop / allocator identity prerequisites、runtime ABI descriptor、
blocked-to-admitted transition checks、backend helper prerequisite facts、dump/fingerprint 和 verifier negative
matrix 固定下来。下一阶段如果开始标准库或 owning dyn surface，必须显式引用 M20a-M20d facts，并继续把
`Box<dyn Trait>`、allocator API、runtime ABI lowering、backend helper call 和 dynamic Drop runtime 分成可验证的小步，
不能把 M20d 误读成已经实现 runtime execution。

M12 后续候选不应混在同一阶段一次性实现：

- borrowed composition-to-supertrait policy：已由 M13/M14 收口；唯一 path 可以 direct/expected 推断，歧义 path
  仍要求显式 `dynproject<...>`。
- owning dyn / `Box<dyn Trait>`：需要 ownership、move/drop、allocator、layout 和标准库 API 设计。
- dynamic Drop dispatch：需要 destructor slot、drop glue metadata、dropck/tooling facts 和 runtime cleanup ABI。
- allocator policy：需要标准库或 runtime ownership policy，不能塞进 borrowed view metadata。
- auto trait / marker trait composition：需要 trait solver、object identity 和 diagnostics 的独立设计。

近期代码量粗估：

| 阶段 | 内容 | 预计新增/修改代码量 |
| --- | --- | ---: |
| M10d hardening/release | 已完成。query/cache/tooling projection、negative samples、docs/tests/coverage、M10c diffstat 分析 | 实际以本次 diffstat 为准 |
| M11a design baseline | 已完成。advanced dyn 后续选择、`principal_set_metadata_v1` policy/schema 设计、非目标、documentation tests 和测试计划 | 实际以本次 diffstat 为准 |
| M11b query/prototype gate | 已完成。principal-set composition DTO、validation、fingerprint、summary/dump、focused query tests | 实际以本次 diffstat 为准 |
| M11c frontend/sema check-only | 已完成。`dyn (A + B)` parser/AST spelling、type identity、coercion check、method-call guard、associated equality merge check、checked dump/fingerprint、negative samples | 实际以本次 diffstat 为准 |
| M11d IR/backend runtime | 已完成。IR composition/projection value、verifier、LLVM metadata layout、显式 principal projection、native runtime tests；direct principal-qualified method dispatch 未打开 | 实际以本次 diffstat 为准 |
| M11e hardening/release | 已完成。`FunctionDynAbiFacts` composition descriptors、lower-IR invalidation、IDE hover、verifier negative matrix、docs/tests release closure | 实际以本次 diffstat 为准 |
| M12a direct composition dispatch | 已完成。唯一 principal method direct dispatch、projection fact、IR project/vtable slot、native execution、receiver mutability negative cases | 实际以本次 diffstat 为准 |
| M12b hardening/release | 已完成。receiver-access binding、associated equality direct dispatch、projection/ABI descriptor 去重、query fingerprint drift、broader negative matrix、docs/tests release closure | 实际以本次 diffstat 为准 |
| M13a design baseline | 已完成。剩余 advanced dyn 候选 policy selection，选择 borrowed composition-to-supertrait explicit projection，新增 query gate、docs/tests | 实际以本次 diffstat 为准 |
| M13b frontend/query/sema | 已完成。`dynproject<...>` explicit composition-to-supertrait projection syntax/typing/facts/diagnostics/checked dump；不做 runtime release closure | 实际以本次 diffstat 为准 |
| M13c IR/backend runtime | 已完成。composition project + supertrait upcast lowering、ABI descriptor、LLVM/native tests | 实际以本次 diffstat 为准 |
| M13d hardening/release | 已完成。`composition_supertrait_chains`、query/cache/tooling hover、negative verifier matrix、docs/tests/coverage closure、代码量偏差分析 | 实际以本次 diffstat 为准 |
| M14 borrowed view path release | 已完成。`BorrowedDynViewPathFact`、expected-type projection、direct supertrait dispatch、IR/native coverage、docs/tests release closure | 实际以本次 diffstat 为准 |
| M15 advanced dyn / const generic design baseline | 已完成。owning dyn / Drop dispatch / allocator boundary、typed scalar const generic route、query gates、docs/tests；不实现标准库、不打开 const generic 用户语法 | 实际以本次 diffstat 为准；高于原 600-1,000 行时主要因为同时新增 const generic gate、文档和 documentation tests |
| M16 const generic check-only | 已完成。`syntax::GenericParamKind::const_`、typed const param parser/AST、canonical const value key、generic instance const arg key、const param env binding、`[N]T` check-only、negative diagnostics、docs/tests | 预计 1,200-2,000 行；实际以本次 diffstat 为准，若高于预估主要因为同时更新 parser/AST/sema/query identity、文档和 documentation tests |
| M17 dyn ownership runtime prep | 已完成。`DynOwnershipRuntimeFacts`、owned container / erased drop glue / allocator / cleanup-dropck boundary facts、summary/dump/fingerprint、negative boundary tests；仍不做标准库 API | 实际以本次 diffstat 为准 |
| M18 dyn ownership runtime boundary hardening / lowering design gate | 已完成。将 M17 facts 接入 query/cache/tooling/reuse/workspace index，补 runtime lowering 所需 IR/verifier design gate、negative matrix 和 release docs；仍不实现标准库、`Box` 或 allocator API | 实际以本次 diffstat 为准 |
| M19 dyn ownership runtime IR / verifier preparation | 已完成。`DynOwnershipRuntimeIrVerifierFact`、function-level IR collector、borrowed vtable destructor-free guard、dynamic erased drop blocked sentinel、verifier negative matrix 和 release docs；仍不实现标准库、`Box`、allocator API、owning dyn user values 或 dynamic Drop runtime | 实际以本次 diffstat 为准 |
| M20a owned dyn runtime admission design gate | 已完成。`OwnedDynRuntimeAdmissionGate`、admission facts、summary/dump/fingerprint、validation、release docs 和 documentation tests；仍不实现标准库、`Box`、allocator API、owning dyn user values 或 dynamic Drop runtime | 实际以本次 diffstat 为准 |
| M20b owned dyn IR shape prototype gate | 已完成。`OwnedDynObjectLayoutPrototype`、two-field handle、blocked drop/allocator slots、IR dump/fingerprint/verifier、`OwnedDynIrShapePrototypeGate`、adapter、negative matrix、release docs 和 documentation tests；不做标准库或 executable runtime helper | 实际以本次 diffstat 为准；原预估 1,100-1,900 行，偏差分析见 M20b release 文档 |
| M20c drop / allocator identity prerequisite gate | 已完成。`OwnedDynDropAllocatorIdentityGate`、drop/allocator identity keys、cleanup/dropck bridge facts、IR dump/fingerprint/verifier、adapter、negative matrix、release docs 和 documentation tests；仍不打开标准库 API | 实际以本次 diffstat 为准；原预估 1,200-2,200 行，偏差分析见 M20c release 文档 |
| M20d runtime lowering ABI design closure | 已完成。`OwnedDynRuntimeLoweringAbiGate`、runtime ABI descriptor、blocked-to-admitted transition checks、backend helper prerequisite facts、dump/fingerprint、M20c key consistency validation 和 verifier negative matrix；仍不实现标准库 surface | 实际以本次 diffstat 为准；原预估 900-1,700 行，偏差分析见 M20d release 文档 |
| M20g default / named call arguments | 已完成。参数默认值、call-site label、checked `ordered_args`、sema diagnostics、IR lowering、borrow/place/move/lambda capture ordered traversal、parser/sema/native tests 和 release docs；仍不实现标准库或 ABI metadata | 实际以本次 diffstat 为准；本轮横跨 parser/AST/sema/lowering/analysis/test/doc，代码量高于单纯 parser feature 属正常 |
| 后续非标准库函数式/闭包语义 | shared/mutable/consuming capture、closure call ability、borrow/resource/dropck facts、generic closure environment ABI、borrowed closure environment escape；不做标准库 adapter | 1,800-3,200 行，取决于是否同时接入 borrow/dropck facts 和 native execution |
| 标准库 / owning dyn runtime surface 入口评估 | `Box`、拥有型容器、resource wrapper、allocator API、标准库 Drop helper、owning dyn 用户值和 runtime lowering 分阶段切分；必须显式建立在 M20a-M20d facts 上 | 1,200-2,400 行设计/入口实现；实际范围取决于是否只做 design gate 还是打开首个库层 API |

## 已收口基线：M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check

M7 设计研究基线已完成，记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)，
执行路线记录在 [Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图](m7-roadmap.md)。
M7b 设计基线已固定在
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线](m7b-borrow-contract-design.md)，
执行路线记录在 [Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 路线图](m7b-roadmap.md)。
M7c/M7d 新设计基线已固定在
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。

M7c/M7d 不照抄 Rust。新基线选择 Rust 级安全底线、Mojo/Hylo 式低噪声 origin surface、C++/Swift 式 RAII 工程性和
Aurex facts-first 工具链模型。显式 origin 语法采用 `&[origin] T` / `&mut[origin] T`，不采用 Rust apostrophe lifetime；
函数边界继续优先使用 `@borrow(return = [...])`。该写法沿用现有 `&T` / `&mut T`，避免新增 `ref` 关键字，且与
`Name[T]` 泛型、`[]T` slice、`[16]T` 数组没有解析歧义。
该语法只在 type context 中解析，不占用未来 lambda/closure 的表达式语法空间；M7c/M7d 只预留
`ClosureCaptureFact` / `ClosureEnvironmentFact` 事实形状，不在本阶段实现完整 closure。

M7b 当前实现状态：M7b WP1-WP7 已完成实现收口。函数边界 `FunctionBorrowContract`、装饰器式
`@borrow(return = [param, self])`、summary-vs-contract enforcement、trait/generic borrowed-return contract、
reborrow parent/child loan、method receiver access facts、receiver auto-borrow two-phase reservation/activation、
query/cache/tooling 投影均已落地。M7b 仍不做 full Rust-style lifetime generics、full Polonius Datalog、
raw pointer alias safe proof、partial move / replace / take / swap、`dyn Trait`、async drop 或 generator borrow。

M7a WP2-WP7 已完成实现收口。M7-WP2 Phase 1 已落地 collect-only `BodyFlowGraph` facts，
M7-WP3 Phase 2/3 已落地 diagnostic-shadow + enforced local loan checker，M7-WP4 已落地
`BorrowSummary` 与 direct call binding，M7-WP5 已补齐 projection/drop/reinit/cleanup conflict matrix，M7-WP6
已完成 diagnostics / query / tooling projection，M7-WP7 已完成 release closure。`CheckedModule::body_flow_graphs` 现在按
`FunctionLookupKey` 暴露函数体 point、edge、place 和 action timeline；`CheckedModule::body_loan_checks`
保存本地 `Origin` / `Loan` / conflict facts、shadow/enforced mode 和稳定 dump；`CheckedModule::function_calls`
记录普通函数/泛型函数/方法 direct call binding；`CheckedModule::borrow_summaries` 记录函数级 return origin
dependency set、unknown/local escape 标志和 stable fingerprint。checker 使用 carrier-local liveness 支持直接本地
borrow 的 last-use 后写入；projection matrix 支持 same/prefix 冲突、known field disjoint 放宽，index/slice/unknown
保持保守；整 local assignment 生成 `reinit`，词法 block local cleanup 生成 `cleanup_storage` invalidation。
`TypeCheckBodyAuthority` 现在混入 borrow summary / body loan check fingerprint、origin/dependency/loan/conflict count
和 unknown/local-escape/diagnostic-emitted 状态位；CLI incremental-cache 和 IDE snapshot query 都消费同一份 checked
facts。IDE semantic facts 新增 `borrow_summary` 与 `body_loan_check`，函数 hover 展示 summary dependency；
enforced diagnostics 现在包含 primary conflict、loan creation、invalidating action 和可定位时的 later carrier use note。
W7a release 性能收口已完成：普通 `--check` 不长期保留 full body-flow graph，非借用返回函数不扫描完整函数体构建
borrow summary，direct/trait call binding 使用 expr-id index 查找；release/coverage/query/perf/stress gates 已通过。

M7c-A / M7c-B 当前实现状态：checked lifetime facts、type/generic lifetime predicates、deterministic outlives
solver、body-flow live-range facts、elision ambiguity、return-origin subset、type-outlives 和 local/temporary escape
enforcement 已落地。`BorrowSummaryBuilder` 会追踪 `strraw` raw pointer alias 的本地来源，raw-derived local return
由新 lifetime checker 诊断；unsafe raw pointer parameter helper 仍保守记录 unknown return fact，不把所有 unknown
proof 当成错误。`BorrowEscapeAnalyzer` 已从 return escape 主路径降级为 storage-only parity guard，继续覆盖
assignment into escaping struct field 等尚未迁移的存储逃逸矩阵。

M7c-C 当前实现状态：核心 storage escape 迁移和性能收口已完成。`FunctionBorrowSummary::storage_escapes`
记录 non-name storage assignment 中逃逸到外部 storage 的 borrowed origin，lifetime collector/enforcer 将其统一映射到
`local_escape` / `unknown_escape` 主诊断；query/cache、checked dump、IDE semantic fact 和 hover 均暴露
`storage_escapes` / `local_escapes` / `unknown_escapes`。旧 `BorrowEscapeAnalyzer` 只在 summary 缺失或无
storage escape 且 body 有候选 non-name assignment 时作为窄 fallback guard 运行。M7c 热点性能已实测收口：
storage escape `sema.analyze` 3-run median 500/1000/2000/4000 条为 50.701 / 100.202 / 204.232 / 419.745 ms；
`gprof` 确认旧的 `BodyLoanSolver::expr_result_contains_loan` 二次热点已消失。

M7d-D 当前实现状态：RAII runtime lowering 已完成第一版闭环。`impl Drop for T { fn drop(self: deinit T) -> void { ... } }`
进入 parser/AST、reserved sema surface、`CheckedModule::destructors`、checked dump、resource classifier、
drop-glue、dropck、query authority、IR verifier 和 IDE hover；lowerer 会把静态可解析 custom destructor 降成
普通 direct `call`，LLVM backend 通过现有 call emission 发出真实调用。`drop` / `drop_if` marker 仍保留为
target-independent cleanup marker，marker 本身在 LLVM backend 仍是 no-op。

M7d-E 当前实现状态：aggregate rollback codegen 已完成 compiler-only 子集。函数体中的 struct literal、tuple
literal、array literal 和多 payload enum synthetic record payload 在含 droppable 元素时会进入 staged aggregate
lowering；后续元素表达式提前 `return` 时，已成功初始化的元素会通过 rollback drop flag、`drop_if` marker 和
静态 custom destructor direct call 被清理。constant/global initializer、无 droppable 元素的 aggregate 和 scalar
aggregate 仍保持 lightweight lowering。本阶段明确不实现标准库级拥有型资源封装。

M7d-F 当前实现状态：tuple numeric field / tuple element place-state closure 已完成 compiler-only 子集。
parser 接受 `pair.0` 和 `pair . 0`，sema 对 tuple 数字字段做类型、可写性和越界检查；body-flow 增加
`tuple_element` projection，borrow checker 能把同一 tuple root 下不同已知元素视为 disjoint；place-state/dropck/
move analysis 和 IR lowering 已接入本地 tuple 元素 partial move/reinit、cleanup facts 和元素级 drop flag。
本阶段仍不实现任何标准库，indexed move-out、array/slice/index 精确 disjoint proof、replace/take/swap primitive、
generic/opaque cleanup runtime ABI 和标准库拥有型资源封装继续后移。

M7d-G 当前实现状态：generic / associated / opaque cleanup marker ABI policy 已完成 compiler-only 收口。
sema drop-glue step 现在记录 `DropGlueAbiPolicy`，IR `drop` / `drop_if` marker 记录 `CleanupAbiPolicy` 并进入
dump、clone/copy、fingerprint 和 verifier；verifier 拒绝缺失 policy 的 cleanup marker、拒绝非 drop value 携带
cleanup policy，并校验 generic、associated projection、opaque、structural/static 和 unknown marker policy 与
target type kind 的匹配关系。missing structural metadata 和 recursive drop-glue cycle 统一记录为
`unknown_value` / `unknown_marker_only`，不再混同真正 opaque type。generic、associated projection、opaque 和
unknown cleanup 当前仍保持 marker-only，不生成未知 runtime ABI call；LLVM backend 中 `drop` / `drop_if`
marker 本身仍是 no-op，静态 custom destructor direct call lowering 沿用 M7d-D。

M7d-H 当前实现状态：index / slice place-state conservative closure 已完成 compiler-only 收口。place-state 的
semantic place identity 不再用本地 root AST `ExprId` 或 index/slice projection expr id 区分同一 storage；
本地 root 归并到 name，field 归并到 `field_name_id`，tuple 归并到 `element_index`，index/slice/deref 保守归并。
place-state facts schema 已升级到 `sema.place_state.facts.v3`；borrow checker 明确测试了不同 index expr 的同 root
冲突；IR lowering 的 local cleanup place path 已能识别 index/slice projection，并对 index/slice 只做同类保守
prefix matching。当前仍不实现 indexed move-out、array/slice/index 精确 disjoint proof 或任何标准库资源封装。

M7d-I 当前实现状态：move rejection facts closure 已完成 compiler-only 收口。`CheckedModule::move_rejection_facts`
记录 move analysis 已实际发出 unsupported diagnostic 的 `pattern_payload`、`try_payload` 和 `indexed_element`
拒绝事实，并保存关联 expr/stmt/pattern、tracked type、resource fingerprint、诊断状态和稳定 fingerprint。
`TypeCheckBodyAuthority`、checked dump、IDE semantic fact、hover、workspace index 和 reuse invalidation 都已接入该
事实链；白盒测试覆盖 match arm payload、struct pattern payload、if / if-expr / while condition payload、`?`
payload 和 indexed move-out。当前语言行为不放宽：consuming pattern payload、non-`Copy` `?` payload transfer
和 indexed move-out 仍拒绝，本阶段仍不实现任何标准库资源封装。

M7d-J 当前实现状态：cleanup marker query / tooling consumption closure 已完成 compiler-only 收口。
IR `drop` / `drop_if` cleanup marker 上的 `CleanupAbiPolicy` 已通过稳定 query DTO 投影为
`FunctionCleanupMarkerFacts`，并接入 lower-IR query result fingerprint、driver incremental cache subject、
IDE snapshot、semantic fact、hover、workspace index 和 session reuse invalidation。full tooling build 会在 sema
成功后临时 lower IR 以填充 cleanup marker facts；frontend-only build 不链接 IR target，facts 为空且保持兼容。
当前语言行为仍不放宽：generic / associated / opaque / unknown cleanup 仍是 marker-only，不生成未知 runtime
destructor ABI call；consuming pattern payload、non-`Copy` `?` payload transfer、indexed move-out 和标准库资源
封装仍不属于本阶段。

M7d-K 当前实现状态：array repeat resource safety closure 已完成 compiler-only 收口。`[expr; 0]` 仍会类型检查
repeat value，但运行期 ownership/move/borrow/place-state/flow traversal 与 IR lowering 不把 value 当作会求值或会
consume 的表达式；`[expr; 1]` 允许非 `Copy` 资源被转移一次；`[expr; N]` 且 `N > 1` 要求元素类型满足
compiler-owned `Copy` capability，否则 Sema 报 `array repeat value must be Copy when repeated more than once`。
IR lowering 对非法非 `Copy` 多元素 repeat 保留防御性 empty aggregate placeholder，避免同一个 owned `ValueId`
被重复写入 aggregate。当前不实现 `Clone`、array fill constructor、非 `Copy` 多元素 repeat 构造、标准库资源
封装或任何标准库 API。

M7 hardening performance closure 也已完成，记录在
[M7 Hardening Performance Closure](m7-hardening-performance-closure.md)。当前新增 statement control-flow query
cache、body-loan precheck 单次表达式遍历、`NormalizedAstOverlay` `u64` 计数和 `tools/m7_hardening_perf.py`。
同机同命令 Release benchmark 对照基线提交 `eef0c25b`，broad frontend case 无可见回退；`SemaAstBulk/4096`
当前 CPU time 约 `20.352 ms`，4x statement 规模约 `4.35x`。剩余 `u32` 主要是 handle、index、stable key schema
或 bounded 小域，后续若宽化必须作为独立 schema migration。

M7c/M7d 后续实现按以下大块推进：

1. M7c-A：已完成。parser/AST/type system 增加 contextual `origin` 参数、`&[origin] T` / `&mut[origin] T`、
   origin union 和 checked lifetime facts；dump/fingerprint/whitebox/query/tooling 已覆盖。
2. M7c-B：已完成。deterministic outlives solver、body-flow live-range facts、elision/ambiguity diagnostics、
   type-outlives、return-origin subset enforcement 和 return local/raw-derived escape 主诊断已落地；旧 analyzer
   降级为 storage-only parity guard。
3. M7c-C：核心已完成。后续只保留 analyzer 删除/debug-only 化的更大 parity 清理，以及 public/prototype/extern/trait
   lifetime policy 的发布文档化，不再阻塞 storage escape 主事实链。
4. M7d-A：已完成当前事实链。`DropCheckFact` / `DropActionFact`、dropck solver、destructor body safety 和泛型
   drop glue type-outlives constraints 已落地。
5. M7d-B：struct field 子集已完成。本地 owned struct field partial move/reinit、字段级 cleanup/drop flag、
   generic side-table body-flow 类型读取和 lowering 字段 drop flag 已落地；tuple 元素子集已由 M7d-F 补上。
   indexed move-out、array/slice/index 精确 disjoint proof、borrowed/reference field overwrite、replace/take/swap
   compiler-known primitives 仍是后续项。
6. M7d-C：已完成。窄 `Drop` / `deinit` semantic surface、IDE/tooling projection、IR verifier 和 release gates
   已接入。
7. M7d-D：已完成。静态 custom destructor runtime direct-call lowering、drop flag guarded call、根 custom
   destructor 与字段 cleanup 顺序、`self: deinit` 非递归 cleanup 和 LLVM call emission 闭环已接入。
8. M7d-E：已完成。struct/tuple/array/multi-payload enum aggregate 构造中的 droppable 元素 rollback lowering
   已接入；成功路径撤销临时 rollback cleanup，early-exit 路径清理已初始化元素；标准库资源封装不在本阶段。
9. M7d-F：已完成。tuple 数字字段访问、tuple element projection、不同 tuple element borrow disjoint proof、
   本地 tuple element partial move/reinit/drop flag 和嵌套 tuple/struct cleanup leaves 已接入；标准库资源封装不在本阶段。
10. M7d-G：已完成。generic / associated / opaque / unknown cleanup marker ABI policy 已正式化，IR
    cleanup marker 的 policy 已接入 verifier、dump、clone/copy、fingerprint 和 lowering；marker-only unknown
    ABI 仍不生成 runtime call，标准库资源封装不在本阶段。
11. M7d-H：已完成。index / slice place-state conservative closure 已正式化；本地 place identity 和 index/slice
    projection 不再因 AST expr id 被误拆分，borrow/place-state/lowering cleanup path 均保持 same-root conservative
    may-alias，标准库资源封装不在本阶段。
12. M7d-I：已完成。move rejection facts 已正式化；pattern payload、try payload 和 indexed element move
    rejection 现在进入 checked facts、query authority、dump 和 IDE/tooling。该步骤只记录当前拒绝事实，不实现
    consuming pattern payload、non-`Copy` `?` payload transfer 或 indexed move-out。
13. M7d-J：已完成。cleanup marker facts 已正式化；IR cleanup marker 的 ABI policy 现在进入 stable query
    DTO、lower-IR query result、incremental cache、IDE semantic fact、hover、workspace index 和 reuse plan。
    该步骤只完成 compiler facts/query/tooling 消费面，不实现 dynamic Drop ABI、generic cleanup runtime ABI、payload
    transfer 或标准库资源 API。
14. M7d-K：已完成。array repeat resource safety 已正式化；零长度 repeat 的 value 只做类型检查、不做运行期
    求值/consume；单元素 repeat 允许非 `Copy` 资源转移一次；多元素 repeat 要求 `Copy` 并拒绝非 `Copy`
    resource。该步骤只完成当前 repeat 语义安全闭包，不实现 `Clone`、array fill constructor、非 `Copy` 多元素
    构造或标准库资源 API。

实现架构必须低耦合：lifetime fact collector、region solver、enforcer、dropck facts、place-state analyzer、RAII surface
checker 和 tooling adapter 分模块维护；`src/sema/internal/` 只能作为 private implementation root，下面不再直接新增文件，
必须按 `borrow/`、`lifetime/`、`dropck/`、`place/`、`diagnostics/`、`pipeline/` 等职责拆子目录。其他 compiler stage
也遵守同一规则。public header 只暴露 stable checked facts。Strategy/Builder/Facade/Adapter 只在隔离策略、稳定 pass
入口或 DTO 投影时使用，不引入大型 inheritance hierarchy、service locator、global mutable state 或字符串 DSL。

M7c/M7d 仍不做完整 lambda/closure capture、HRTB、full variance、`dyn Trait` object lifetime bound、async/generator borrow、full Polonius
Datalog runtime、Stacked-Borrows-level unsafe alias semantics、interior mutability proof、并发 data-race capability、
future `MustConsume`、consuming pattern payload transfer、non-`Copy` `?` payload transfer、indexed move-out、
array/slice 精确 disjoint proof 或 self-referential/pinning/address-stability。

M6-WP1 已完成三轮设计审视：

1. 跨语言和研究证据矩阵：C++、Rust、Swift、Mojo、Move、Zig、Go、Hylo、Pony、Verona、Cyclone、
   Lean、Koka、Roc、Linear Haskell、Idris 2、Austral、Carbon、Clang 和相关论文。
2. Aurex 语义选择：拆开 `Copy`、`Discard`、`NeedsDrop` 和 future `MustConsume`；第一版只做
   whole-local move、CFG-sensitive initialized state、确定性 cleanup 和 generic drop glue。
3. 用户案例压力测试：regex 手工 `destroy`、owned string/vector、文件、锁、FFI、覆盖赋值、分支、循环、
   `?`、pattern、partial initialization、自引用、shared ownership cycle 和未来 `dyn Trait`。

M6-WP2 到 M6-WP7 已完成 M6 实现基线：compiler-owned `Copy`、内部 `Discard` / `NeedsDrop` / ownership
resource summary、结构化类型分类、stable fingerprint、checked dump resource summaries、expression
owned-use side table、whole-local move analysis、move 后重新初始化、consume-origin diagnostics、lexical
cleanup-action stack lowering、`defer` 组合、drop flag，以及正式 IR `drop` / `drop_if` cleanup 节点。该基线还包括
destructor body identity、stable drop-glue key、target-independent drop-glue planner、IDE resource hover projection、
generic parameter hover fallback、LSP stdio server 入口和 release documentation closure。

M7 继续以 M6 cleanup 和 resource facts 为输入，增加 loan origin、projection-aware access conflict、
borrowed-return contract 和 lifetime surface。M7d-C 已补上窄 `impl Drop` / `deinit` semantic surface；M7d-D
已补上静态 custom destructor call lowering；M7d-E 已补上 compiler-only aggregate rollback codegen。标准库级
拥有型资源封装、trait-object Drop dispatch、async/unwind-aware drop 和完整 Rust-style lifetime surface 仍是后续
独立工作。

M7d-K 之后本阶段不要把标准库作为工程入口。generic/associated/opaque cleanup marker ABI 策略、index/slice
place-state 保守闭包、move rejection facts、cleanup marker query/tooling 消费面和 array repeat resource safety
都已正式化；M7 剩余工作应优先做 release/documentation/coverage final gate、已实现 facts 的一致性检查和
post-M7 规划。未来如果要支持非 `Copy` 多元素 repeat，必须先设计 `Clone`/array fill constructor/逐元素构造与
rollback 语义，不能在当前阶段通过标准库 wrapper 绕过。为未来 consuming pattern / non-`Copy` `?` payload 真正转移
语义设计更精确 payload fact 可以作为 post-M7 compiler-only 议题，但当前阶段仍不做标准库入口。
`Drop` bound、generic Drop surface、trait-object dispatch 和标准库资源 API 都应等 capability、dynamic ABI 与
标准库阶段边界稳定后再作为独立阶段推进。

## 已收口背景：Post-M5 Design Selection

M5 default trait methods 已经收口为 release baseline。M5-WP1 已固定
[Aurex M5 Default Trait Methods 调研与设计基线](m5-default-trait-methods-design.md)，阶段路线记录在
[M5 Default Trait Methods 路线图](m5-roadmap.md)，完整发布契约记录在
[Aurex M5 Default Trait Methods Release Baseline](m5-release-baseline.md)。M5-WP2 到 M5-WP6 已完成
syntax / AST / body identity、trait-context default body checking、impl completeness 与 method-origin facts、
static lowering、tooling / diagnostics 和 incremental-cache projection。M5-WP7 已收口 usage notes、version notes、
unsupported matrix、常规仓库 samples、文档测试、full build/test/coverage gates、query/cache gates 和 stress gates。

已经收口的 M5 目标刻意保持窄范围：

- 允许 trait method requirement 携带 default body。
- 继续使用 M4 的显式 `impl Trait for Type`、nominal identity、coherence、associated types 和 static dispatch。
- impl 中签名匹配的方法是 explicit override。
- 省略 defaulted requirement 的 impl 继承 inherited default。
- 省略 non-default requirement 仍然报错。
- method origin 显式记录为 `impl_override`、`trait_default` 或 `param_env`。
- default body 在 trait context 中 type-check；单态化后 selected default call 降低到 direct trait-owned default
  body instance。
- 在 IDE/tooling 和 incremental-cache query records 中暴露 selected default origin，但不引入 runtime dispatch。

M5 不包含 `dyn Trait`、object safety、vtable ABI、specialization、associated constants、default associated types、
GAT、blanket impl、RAII/resource semantics、Swift-style protocol extensions、Scala/Kotlin mixins 或 runtime
interface dispatch。这些继续作为独立未来设计流。

下一阶段应该从新的设计决策开始。最强候选是 resource semantics、dynamic trait object、specialization、
default associated type、associated const、minimal implementation annotation、package-level coherence 或更强
trait solver。这些都不应该重新打开 M5 static default-method baseline。

M3 release baseline 已收口，M4 trait/protocol 系统已完成 WP1、WP2、WP3、WP4、WP5、WP6、WP7 和 WP8。M4-WP1 固定
[Aurex M4-WP1 Trait / Protocol 系统调研与设计基线](m4-trait-protocol-system-design.md)，阶段路线见
[M4 Trait / Protocol 系统路线图](m4-roadmap.md)。M4-WP2 已完成 token、parser、AST、AST dump、lossless
syntax 和 query identity scaffold。M4-WP3 已完成 trait declaration 和 impl registry 的 query-backed sema
接入。M4-WP4 已完成 coherence / generic predicates：`CheckedModule` 记录 `TraitPredicate`、
`TraitObligation`、`TraitEvidence` 和 `ParamEnvInfo`；`where T: Trait` 降低为 predicate；builtin
capability 同步进入 compiler-owned builtin trait predicate；generic instantiation 做 candidate rejection；
trait impl registry 具备 canonical coherence fingerprint、orphan rule 和 first-pass overlap check。M4-WP5
已完成 static trait method resolution and lowering：inherent method 优先，trait impl method 不污染普通
method lookup，generic body trait call 通过 `ParamEnv` 绑定为 `param_env` call fact，concrete receiver
通过 visible trait + impl registry 绑定为 `impl` direct call，LLVM IR 单态化后直接调用具体 impl method。
M4-WP6 已完成 associated type model：trait declaration 可声明 `type Item;`，trait impl 可赋值
`type Item = Type;`，`Self.Item` / generic projection 有 canonical associated-projection type，
`Trait[Item = Type]` 降低为 trait predicate + equality fact，impl requirement matching 会替换 associated
type output，并且 diagnostics 覆盖 ambiguity、cycle、缺 bound、重复/缺失/未知 associated type、builtin equality
误用和 equality unsatisfied。
M4-WP7 已完成 trait fact 的第一层 IDE/tooling 和 diagnostics 投影：`where T:` 后的 completion、trait /
trait method / impl method / associated type 的 hover/definition、semantic-token 分类、workspace member
indexing、基于 `DefKey` / `MemberKey` 的 rename identity、LSP adapter 投影且不让 LSP DTO 泄漏进 compiler
internals，以及 candidate impl、rejected candidate、associated-type equality mismatch、orphan check 和
overlap 位置的 diagnostic notes。
M4-WP8 已完成，并在
[Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md) 中记录 release contract：docs、语言表面说明、
unsupported matrix、常规仓库测试、coverage、query/cache/profile stress gates 和后续入口已经对齐到同一个
M4 边界。

当前真实能力：Aurex 使用带 default method body 的 nominal static trait，语言关键字为 `trait`，conformance 通过显式
`impl Trait for Type` 给出。`CheckedModule::traits` 记录 `TraitSignature`、generic params、visibility 和结构化
requirement；`CheckedModule::trait_impls` 记录 exact impl facts；sema 已覆盖 requirement matching、`Self`
替换、trait generic 参数替换、qualified trait reference、可见性、trait generic arity、缺方法、重复方法、未知方法、签名不匹配、
非 trait impl target、非 named self target、重复 exact impl、orphan rule、first-pass overlap 和 generic candidate
rejection。associated type surface 已覆盖 declaration、impl assignment、projection normalization、equality
predicate、method requirement substitution，以及 trait signature / impl / predicate 的 checked fact。default method
surface 已覆盖 trait-owned body、inherited default、explicit override、static default instance lowering、IDE origin
projection 和 incremental-cache rows。相关测试全部位于常规仓库测试目录：
`tests/gtest/sema/trait_tests.cpp`、`tests/samples/positive/traits/trait_impl_registry.ax`、
`tests/samples/positive/traits/trait_predicate_where_generic.ax`、
`tests/samples/positive/traits/trait_method_static_dispatch.ax`、
`tests/samples/positive/traits/trait_method_associated_static_dispatch.ax`、
`tests/samples/positive/traits/trait_method_inherent_precedence.ax`、
`tests/samples/positive/traits/trait_method_function_field_precedence.ax`、
`tests/samples/positive/traits/trait_associated_type_basic.ax`、
`tests/samples/positive/traits/trait_associated_type_where_equality.ax`、
`tests/samples/positive/traits/trait_default_method_*.ax`、`tests/samples/negative/traits/*.ax` 和
`tests/samples/imports/samplelib/traits.ax`。WP7 tooling 覆盖位于
`tests/gtest/tooling/ide_tooling_tests.cpp` 和
`tests/gtest/tooling/session_lsp_tooling_tests.cpp`。M5 release documentation coverage 位于
`tests/gtest/integration/documentation_tests.cpp`。

下一步不再是 M5 实现。resource semantics、dynamic trait object、package-level coherence、specialization、
class-like sugar、default associated type、minimal implementation annotation 和更强 trait solver 都是 M5 后候选，
需要独立设计后再进入代码修改。

M5 不做 dynamic trait object 或 RAII/resource semantics。dynamic trait object、vtable ABI/object safety、
associated const、default associated type、specialization、generic associated type、minimal implementation annotation
和资源系统继续保持 M5 当前非目标。M5 surface 已支持 identifier trait predicate 上的 associated-type equality
constraint、static default methods 和 tooling projection；qualified where predicate 和 generic trait predicate arguments
仍留给后续 solver 阶段。

## M3 收口背景

R5 Compilation Pipeline / Driver Action 重构 core 已收口：`CompilerInvocation`、`Compiler`
facade、`CompilationSession`、`CompilationPipeline`、`FrontendPipeline`、`LoweringPipeline`、
`BackendPipeline`、`PipelineStage`、IR pass manager、analysis manager、profile metadata、diagnostic
stage owner 和 tooling/profile consumer contract 都已经进入主路径，并保持原有 CLI、diagnostics JSON、
profile JSON、incremental cache 和 emit mode 行为。

M3.0 模块系统、M3.1 泛型闭环、M3.2 Query-backed Sema 和 M3.3 Tooling Session And Incremental
Sema 都已经合并回 `m3`。M3.4 Real Incremental Sema Execution 已在 `m3.4` 收口，核心是把 M3.3 的
reuse explanation 推进为真实 incremental sema execution：

- 通过 `ToolingSession` 传递 previous snapshot / query context。
- 把 query reuse decision 变成可执行 semantic fact reuse。
- 保持 body-local、signature-local、generic 和 module-surface invalidation 边界。
- workspace semantic index 尽量按 affected fact identity 更新。
- CLI、diagnostics JSON、profile JSON、incremental-cache 和 LSP protocol 行为保持兼容，除非明确 work package
  改动它们。
- tests、coverage、query pruning、fuzz 和 stress gates 保持 green。

已收口的 M3.4 执行入口是
[Aurex M3.4 Real Incremental Sema Execution 计划](m3.4-real-incremental-sema-plan.md)。
更完整的 M3.4-M3.9 路线记录在 [M3 路线图](m3-roadmap.md)。M3.5 已在
[Aurex M3.5 Incremental Syntax And Stable AST Identity 计划](m3.5-incremental-syntax-stable-ast-plan.md)
中收口。M3.6 已在
[Aurex M3.6 Project Graph And Persistent Query DB 计划](m3.6-project-graph-persistent-query-db-plan.md)
中收口。M3.7 已在
[Aurex M3.7 IDE Semantic Features 计划与收口记录](m3.7-ide-semantic-features-plan.md)
中完成 IDE 语义能力第一层：completion、rename、semantic tokens、inlay hints、code actions、
workspace symbols 和 LSP projection。M3.8 已在
[Aurex M3.8 Query-backed Lowering / Backend Reuse 计划与收口记录](m3.8-query-backed-lowering-backend-reuse-plan.md)
中收口；M3.9 已在
[Aurex M3.9 M3 Release Baseline 与 Authority Audit](m3.9-m3-release-baseline.md)
中完成最终收口。下一步工作应作为 M3 后的新设计流推进，而不是继续往 M3 追加功能切片。

R5.1 已完成 `Compiler` facade 和内部 `CompilationPipeline` 拆分；R5.2 已完成前端阶段拆分；
R5.3 已完成 `LoweringPipeline`、`BackendPipeline` 和 `PipelineStage` 记录。当前 driver 总控已经只保留
阶段顺序、emit mode 停止点和 error/profile finish，checked dump、IR lowering、IR pass pipeline、IR dump、
LLVM IR emission、LLVM IR dump、temporary LLVM file 和 clang native invocation 都已经由对应子 pipeline
持有。R5.4 已完成 IR verifier / pass manager 第一层：`ModulePassManager`、`ModulePass`、
`PassResult`、`PreservedAnalyses`、`VerifierGate` 和 `run_pass_pipeline_with_summary` 已进入主路径，
旧 `run_pass_pipeline` API 保持兼容。R5.5 已完成 IR analysis cache / invalidation 第一层：
`ModuleAnalysisManager` 已能惰性构建并缓存 CFG、dominance 和 value-use analysis，pass 回调可以访问
analysis manager，changed pass 会按 `PreservedAnalyses` 自动失效缓存。R5.6 已完成 IR verifier
diagnostics 上下文第一层：input、after-pass 和 output verifier failure 现在携带稳定
`stage/profile/verifier/pass` 上下文，原始 verifier body 和 `ErrorCode` 保持不变；`LoweringPipeline`
从 `PipelineStageId::ir_pass_pipeline` record 读取 stage/profile 并传给 IR pass pipeline。下一步继续把
`PipelineStage` 作为 profile、cache/query、diagnostics owner 和 IDE/LSP 阶段可视化的唯一阶段目录维护。
R5.7 已完成 profile JSON 阶段元数据第一层：`aurex-profile-v1` 的 driver 主阶段 phase 现在附带可选
`stage` 对象，直接来自 `PipelineStageRecord`，包含 stage id、input、output、diagnostic ownership
和 cache/query impact；原 phase `name` 和内部 incremental-cache query 子事件保持不变。
R5.8 已完成 cache/query profile 子事件父阶段映射第一层：`PipelineStage` 现在记录
`PipelineProfileSubeventRecord`，`incremental_cache.source_stage_reuse` 会通过 `parent_stage`
挂回 `incremental_cache.lookup`，query diff / plan / pruning / provider-eval 会挂回
`incremental_cache.write`；子事件继续不携带 `stage`，不会被误认为 driver 主阶段。
R5.9 已完成 diagnostics owner 阶段目录第一层：`PipelineStage` 现在能从 `DiagnosticCategory`
反查候选 owner stage，lexer 诊断保留 `tokens.lex` / `module.lex` 双归属，parser/module/sema
类诊断分别归入 `module.parse`、`module.append` 和 `sema.analyze`；diagnostics text/JSON
协议保持不变。
R5.10 已完成 IDE tooling diagnostics 消费阶段目录第一层：`pipeline_stage.cpp` 拆成轻量
`aurex_pipeline_stage` 目标，driver 和 `aurex_tooling` 共同消费同一份 `PipelineStageRecord`；
`IdeDiagnostic.owner_stages` 现在直接携带阶段 id/profile/input/output/diagnostic/cache-query metadata，
供后续 LSP/IDE 视图使用。
R5.11 已完成阶段目录公开 API 和 metadata 投影收口：`pipeline_stage.hpp` 已进入
`include/aurex/infrastructure/pipeline/stage.hpp`，`PipelineStageMetadata` 成为 profile writer、tooling
diagnostics 和后续 profile viewer/LSP adapter 可复用的只读阶段 metadata 形状；`aurex_tooling`
不再依赖 private `src` include root，profile JSON 和 diagnostics JSON 协议保持不变。
R5.12 已完成 profile 记录入口枚举化：`CompilationProfiler::record` 和 `ScopedCompilationPhase`
现在能直接接收 `PipelineStageId`，incremental-cache 子事件通过 `PipelineProfileSubeventId` 进入
profiler；调用点不再散落 stage profile name 字符串，`aurex-profile-v1` 字段保持不变。
R5.13 已完成 profile/tooling 消费者分类契约：`pipeline_profile_phase_classification(...)`
把 profile phase name 统一分类为 driver 主阶段、profile 子事件或 unknown；profile JSON writer
已通过这个入口输出原有 `stage` / `parent_stage` metadata，协议字段保持不变。后续 profile viewer
和 LSP/IDE 阶段视图必须复用这个分类 API，不再维护独立 phase-name 映射表。

M3.5 的当前完成面：

1. range-based document edit：`ToolingDocumentTextEdit`、`change_document_range(...)` 和
   `change_document_range_with_reuse_plan(...)` 已完成。
2. syntax stable identity：`LosslessNodeStableKey` 已完成，未变化 subtree 不再依赖绝对 range/token index 匹配。
3. syntax reuse stats：`compare_lossless_stable_nodes(...)` 和
   `ToolingIncrementalSnapshotResult::syntax_reuse` 已完成。
4. AST projection：`IdeAstNodeInfo` / `ToolingAstNode` 已完成，offset 可投影到 AST item/function body 的稳定
   `DefKey` / `BodyKey`。
5. 测试覆盖：prefix edit stable syntax key、range edit syntax reuse、stable AST body key 和 plain range edit 已完成。

M3.6 的当前完成面：

1. project/workspace model：`ProjectModel` / `WorkspaceModel`、`ProjectKey` 和 stable identity 已完成。
2. shared driver/tooling input：CLI invocation 和 `ToolingSession` 共享 package root、source root、import roots、
   target config、command options 和 open buffers 输入。
3. project graph query：`QueryKind::project_graph`、provider、executor/context/provider set 和 stable key decoder
   已完成。
4. persistent query DB：incremental cache schema v2 写入 project inputs，并持久化 `project_graph` rows/edges。
5. invalidation/profile：`incremental_cache.project_inputs` 能解释 reuse/reject 和 changed inputs。
6. 测试与文档：query、driver cache、tooling workspace model 和 M3.6 文档已收口。

M3.7 的当前完成面：

1. 协议无关 completion：syntax context、sema scope、workspace facts 和 keyword 合并。
2. 协议无关 rename：symbol identity、identifier/keyword/conflict 检查和 workspace edit plan。
3. semantic tokens / inlay hints：syntax token kind 与 checked semantic facts 合成。
4. code actions：从结构化 help diagnostic 生成 lookup suggestion quick fix。
5. workspace symbols 与 LSP projection：workspace index materialization、stale-generation guard 和
   completion/rename/semanticTokens/codeAction/workspaceSymbol/inlayHint provider。

M3.8 已完成：

1. WP-1：function body lowering query authority。
2. WP-2：generic instance lowering query authority。
3. WP-3：type layout / enum layout / ABI symbol query facts。
4. WP-4：IR pass analysis preservation 与 query invalidation 接入。
5. WP-5：LLVM emission unit 与 target-independent IR unit 边界。

M3.9 已完成：

1. M3.0-M3.8 文档已对齐到最终 baseline。
2. public API authority boundary 已写入 release-baseline audit。
3. 剩余 unsupported/resource/trait/package 专题已分类为 M3 后非目标，不再作为 M3 活跃任务。
4. full tests、coverage、query gates、generic stress、format 和 diff checks 固定为最终 M3 质量基线。

M4 后工作应先做设计，再进入实现，可选设计流包括：

1. resource semantics、ownership/drop timing 和 ABI 影响。
2. dynamic trait object、object safety 和 vtable ABI。
3. package/dependency resolver 与超出当前 project/query cache 边界的 workspace database。
4. default trait methods、specialization 和更强 trait solver。
5. backend codegen-unit scheduling 与 multi-target reuse policy。

2026-05-28 收口更新：原 M3.1 work packages 已通过 WP-7 Generic Closure Audit And Release Baseline 统一复审。
当前泛型 release baseline 固定为：generic struct / enum / type alias / function / owner-generic method /
method-local generic method 都以 `GenericInstanceKey` / `GenericInstanceIdentity` 作为 stable id、ABI suffix、
incremental key、query subject 和 checked metadata 的权威身份；`TypeHandle.value` 只允许留在本 session 的
lookup/cache fast key；display string、checked dump、diagnostics、IR dump 和 c_name 都只作为输出，不反向作为
语义 identity。后续若继续深化泛型 query provider、trait/resource/const generic 设计或 LSP/IDE 消费，必须以
这个 M3.1 baseline 为输入，不再重新打开已经收口的身份和 ABI 路径。

2026-05-29 M3.4 规划更新：M3.3 已 fast-forward 合并回 `m3`，`m3.4` 已创建，M3.4-M3.9 路线已固定为：
M3.4 real incremental sema execution、M3.5 incremental syntax / stable AST identity、M3.6 project graph /
persistent query DB、M3.7 IDE semantic features、M3.8 query-backed lowering / backend reuse、M3.9 release
closure。

2026-05-29 M3.4 收口更新：`ToolingSession` 现在会在 document change 时保留 previous materialized
snapshot、精确 edit impact 和 pending workspace facts；`IdeIncrementalSnapshotInput` 把 previous query
records 接入 snapshot construction；`QueryContext` 对 unchanged file/module/signature/body query records 执行
green reuse；`ToolingIncrementalSnapshotResult` 暴露已执行的 reuse plan、reuse counters 和 workspace-index
update stats。聚焦测试覆盖 accepted/rejected previous context、body-local reuse、removed-definition
invalidation、重复 stable-fact edits、generic body-edit reuse、malformed reuse plan 和无旧版本泄漏的 workspace
index update。该阶段已收口，M3.6 project graph 和 persistent query DB 也已收口，下一实现目标已经推进到
M3.7 IDE semantic features。

2026-05-29 M3.5 收口更新：`ToolingSession` 已支持 range-based text edit；`LosslessNodeStableKey` 和
`compare_lossless_stable_nodes(...)` 已把 syntax subtree reuse 变成可报告的 stable-key multiset 统计；
`ToolingIncrementalSnapshotResult::syntax_reuse` 暴露 syntax reused/recomputed/invalidated counters；
`IdeAstNodeInfo` / `ToolingAstNode` 已将 offset 投影到 AST item/function body，并输出稳定 `DefKey` / `BodyKey`。
M3.6 project graph 和 persistent query DB 已收口。下一实现目标：M3.7 IDE semantic features。

## 当前分支原则

旧标准库已冻结并从 M2 当前树删除。下一阶段不要继续扩张 std，也不要用 std 样例证明语言能力。所有新能力先用自包含 `.ax` 样例验证；库层必须等基础语法、类型系统和模块边界稳定后重新设计，不复用旧 std 路线。

## M2.5 当前路线

M2.1 的 Semantic + Performance + Security Closure 已完成，当前进入
M2.5 frontend-foundation。M2.5 不继续扩张 std 或高层语言面，先把前端变成
query-safe、lossless-syntax-ready、IDE-native-ready 的结构化系统：

1. 显式语义诊断元数据：已完成。sema 在创建诊断时写入稳定 kind/category/code，
   不再按 message 文本反推分类。
2. Query key：第一批已完成。Stable Semantic Query Key、Session Fast Handle、
   CanonicalTypeKey、GenericInstanceKey 和 diagnostics query 边界已经按
   [M2.5 Query Key 设计](m2.5-query-key-design.md) 进入默认增量缓存主路径。
3. QueryContext / dependency graph：第一批已完成。file、module、def、body、
   generic、diagnostics 的 query row/edge、result fingerprint、red-green 失效边界、
   provider-skip replay 和门禁覆盖已经闭环。
4. Lossless syntax：当前专项已完成。`--dump-lossless` 现在能输出 `source_file`
   root、顶层声明节点、直接 trivia/eof token leaves、`block` / 分隔符组节点组成的
   lossless syntax tree，保留 whitespace、line comment 和 block comment；tree 支持
   parent/children/token-span 遍历、稳定 node key、结构校验、offset lookup、子树源码重建和
   lossless CST -> AST parser lowering；query retain-trivia lex fingerprint 与 build-lossless
   parse fingerprint 已接入。
5. IDE-native 工程入口：当前验收已完成。`aurex_tooling` 提供面向内存 buffer 的
   `IdeSnapshot`，统一产出 lossless tree、AST、checked module、结构化 diagnostics、
   file/lex/parse/diagnostics query records 和 dependency edges；offset token、hover、
   顶层 definition、同名 identifier references 和编辑影响 node 选择都通过这层 API 暴露。
   后续 LSP adapter 只应消费该层数据，不再旁路 parser/sema/query。
6. M3 准备边界：M2.5 不再承载 regex 审计暴露出的 trait、resource、closure、
   iterator 等表达力专题；它只保留 `CanonicalTypeKey`、`GenericInstanceKey`、
   `ModuleKey`、`ModulePartKey` 和 diagnostics/query 边界。模块 fragments、
   package visibility、泛型后端闭环和 method-local generics 进入 M3。

独立设计见 [M2.5 路线图](m2.5-roadmap.md) 和
[M2.5 Query Key 设计](m2.5-query-key-design.md)。下一阶段入口见
[M3 路线图](m3-roadmap.md)。下面的 M2.1 章节保留为已经完成的收口基线和后续回归约束。

## M2 优化目标（已完成基线）

本节根据 `docs/review/AUREX_M2_完整报告.md` 制定，优先级高于下面的“优先路线”。在完成本节定义的 M2.1 Semantic + Performance + Security Closure 之前，不继续扩张 std、trait/interface/protocol、borrow checker、macro、async、package manager、通用 iterator protocol、selfhost 或 M1 build-tool 路线。后续新增能力必须先证明不会扩大当前 review 已经指出的语义漏洞、性能爆点和攻击面。

### 总目标

1. 语义闭包优先于新特性：safe surface 不允许隐式穿过 raw pointer，contextual typing 不能被旧缓存污染，泛型和 capability 必须用结构化身份表达，`[]` 的泛型/索引/slice 语义必须在 AST 或 checked 层变成可验证的显式节点。
2. 性能和抗攻击能力进入质量门：泛型实例化、大表达式、错误输入、二进制输入、深层括号、长操作符链和海量诊断都必须有可测上限，不能依赖“正常源码不会这么写”的假设。
3. 诊断从“能报错”升级到“可定位、可解释、可修复”：保留当前 `^` 精确定位优势，补齐 expected/actual 类型、previous declaration note、`did you mean`、配对 token 位置和诊断数量上限。
4. 所有修复必须落到测试矩阵：每个 P0/P1 语义修复至少有 negative sample 或 gtest；每个 P0 性能/安全修复至少有 stress/perf 用例和 baseline 记录；轻量本地/CI 阈值先作为质量门，2M AST / 5000 泛型等重型阈值在跨机器基线稳定后继续收紧；修复完成后再更新语法、unsupported 和 progress 文档。

### 硬性验收指标

| 维度 | 当前 review 暴露的问题 | M2.1 目标 |
|:-----|:----------------------|:----------|
| raw pointer safety | `p.x` / `p[i]` 可在 safe context 访问 raw pointer pointee | 所有 raw pointer dereference、field projection、index projection、write projection 都必须要求 unsafe context；safe reference projection 保持 safe |
| `&expr` 语义 | `&x` 根据 expected type 可隐式变成 raw pointer | `&x` / `&mut x` 只产生 safe reference；raw pointer 地址必须走显式 builtin 或显式 unsafe 边界 |
| contextual typing | expression type cache 忽略 `expected_type` | literal、`null`、reference、generic call、struct/array/tuple literal 不再被跨上下文缓存污染 |
| capability | `Eq` / `Ord` / `Hash` 是字符串 predicate，和实际 operator 不一致 | 改为 `CapabilityKind` 与结构化规则；能力判断和 operator/type checker 使用同一套 predicate |
| `[]` 后缀 | generic/index/slice 混在同一语法节点后由 sema 猜 | checked 层必须能区分 generic apply、index、slice、type apply；错误信息指出用户写的是哪一类 |
| 泛型实例化 | 2000 实例曾暴露高 RSS 增长 | side table 局部化或稀疏化后，增长曲线接近线性，具体阈值由 stress JSON 和 profile/scale 校准 |
| match exhaustiveness | 结构化 pattern 笛卡尔积可指数爆炸 | 用 pattern matrix / usefulness witness search 构造反例，不枚举全空间；guard 字面量 truth、open integer literal 和 dynamic slice 代表长度 witness 已建模 |
| parser 深度 | 3000 token `+` 链和 2000 层括号可栈溢出 | 左结合操作链循环化；嵌套分组有深度预算和诊断，不崩溃 |
| lexer 错误输入 | 10KB 全 null / 0xFF / 0x80 曾暴露长耗时 | 无效字节聚合诊断和 error budget；10KB 二进制拒绝进入阈值门 |
| diagnostics | 诊断质量约 80/100，缺 note/help | 已补齐低成本高收益诊断信息，并在 M2.5 把 sema 元数据收口为显式 kind/category/code |
| 综合性能 | 部分场景落后 Clang/Zig 5-17 倍 | 完成核心 6 项后，综合差距目标收敛到 2 倍以内或给出不可达原因 |

### Review 覆盖映射

| Review 章节 | 必须进入 M2.1 的处理结果 |
|:------------|:--------------------------|
| 总体评价 | 把“语义靠上下文和后期 sema 猜”的路径改成显式 typed identity、显式 unsafe boundary、显式 checked node |
| P0 语义缺陷 | `expected_type` cache、`&expr` 双语义、raw pointer projection、`[]` 多义、capability 字符串 predicate 全部作为 Phase 1 阻塞项 |
| P1 语义缺陷 | generic lookup、generic param identity、mangling key、`?` 形状识别、M2 unsupported 分类全部作为 Phase 2 阻塞项 |
| P2 工程质量 | reference slice index、syntax cache 禁读、Parser→Sema 契约、`analyze_expr` 拆分、record/enum/member index、frontend-only CMake、lookup reserve、error budget 全部纳入 Phase 2-5；当前主线已关闭 `analyze_expr` 拆分，其余按状态表管理 |
| P0-Perf | generic side table、match 笛卡尔积、identifier 临时 string、AST 胖节点/复制全部作为 Phase 3 阻塞项 |
| P1-Perf | Lowerer scope、module export 缓存、generic key、diagnostic line table、member lookup、unordered_map reserve 全部纳入 Phase 3 |
| 性能实测和极限压力 | 保留 500/1000/2000/5000 泛型实例、大表达式、冷启动、千函数/万函数等基线，作为后续跨编译器对比和阈值候选 |
| 工业级攻击测试 | parser 栈溢出、binary lexer 挂起、错误恢复退化全部必须有 budget、stress case 和“不崩溃”验收 |
| 诊断质量测试 | 数组常量越界、`did you mean`、expected/actual、previous declaration note、paired-token note、warning/note/help、颜色/多行 span 已进入当前主线；后续只保留结构化诊断协议 |
| M2.1 收口清单 | 21 个建议测试必须进入测试矩阵或被等价测试覆盖 |
| 最终总结和性能目标 | Semantic + Performance + Security Closure 完成前，不推进更高层语言/库路线；综合性能目标按“5-17× 收敛到 2× 内”管理 |

### 当前 P0 收口状态（2026-05-15）

当前 `m2` 分支已经优先关闭 review 中会破坏语义安全、造成指数/超线性资源消耗或形成 DoS 入口的 P0 项。下面状态是实现事实，不是长期架构终点；凡是标为“阶段性关闭”的项，表示 M2.1 先移除了当前爆点，并保留更大规模架构优化为后续任务。

| 项目 | 当前状态 | 已落地边界 | 后续保留 |
|:-----|:---------|:-----------|:---------|
| raw pointer projection unsafe | 已关闭 | `*p`、`p.field`、`p.field = v`、`p[i]`、`p[i] = v` 统一要求 unsafe；reference projection 保持 safe；method receiver 不允许把 raw pointer 静默当作 `&T` / `&mut T`，也不允许把 safe reference 静默当作 raw pointer self | 后续 borrow/lifetime 设计不属于 M2.1 |
| `&expr` 双语义 | 已关闭 | `&place` / `&mut place` 只产生 safe reference；raw pointer 地址必须显式走 `ptraddr` / `ptrat` 等边界 | 后续可设计更完整的 address-of/raw-pointer conversion API |
| contextual expression cache | 已关闭 | checked/generic side table 已拆出 `expr_intrinsic_types`、`expr_types` final table、`expr_expected_types` final-cache key 和 `CoercionRecord` overlay；integer/float/null、unary/binary、slice、array/tuple literal、if/block/match 在 expected type 下会保留 intrinsic type，并把 contextual final type 与 coercion/adjustment 单独记录；`analyze_expr` 入口已拆成缓存入口、分类调度和 literal/value/control/aggregate/projection/operator/builtin helper，binary expression 内部也拆为 operand contextual typing、类型不匹配诊断、常量 hazard 检查和 operator result 记录 | 后续只保留更丰富 coercion kind，不再是 P0 缓存污染问题 |
| `[]` 多义 | 已关闭在 parser AST 层 | parser 直接按保守语法 guardrail 生成 `generic_apply`、`index`、`slice`、`field`、`call`、`struct_literal`、`try_expr` 等显式 compact 节点；旧 raw postfix 链路和 sema 二次 lowering 路径已删除；M2.1 明确 type-shaped selector 契约，`Type[T].case` 仍走 generic selector，`items[index].field` 和 lowercase `name[index].field` 保持 value index | 后续只保留更细的诊断和跨模块语义消歧增强，不再保留第二套 postfix lowering |
| capability 字符串 predicate | 已关闭 | capability 使用结构化 `CapabilityKind`，where clause 和 generic instantiation 使用同一套能力规则；operator availability 和 capability satisfaction 明确分层：`f32` / `f64` 支持直接比较 operator，但不满足 `Eq` / `Ord`；`Hash` 当前只是 marker-only admission predicate，支持 `bool` / `char` / integer / pointer，不提供 hash operator、稳定 hash ABI 或用户自定义实现 | 用户自定义 trait/protocol、associated type 和资源 capability 继续后置 |
| 数组常量索引越界 | 已关闭 | 固定数组的整数常量 index 在 sema 报错，覆盖正数越界和负数字面量越界 | 变量 index 和完整运行时 bounds check 仍按当前 M2 边界处理 |
| 泛型 side table 全模块分配 | 已关闭当前爆点 | generic instance side table 改为函数体 NodeSpan 局部表；retained instance 只在非连续节点 ID 映射时共享 module-level sparse layout，不再按实例复制 sparse ID mapping；sema-only expected-type / pattern-case cache 使用可释放 arena 并在分析后丢弃；不同泛型模板的 per-instance arena floor 降到 1 KiB；IR lower 支持 local dense + sparse fallback 读取 | 后续集中在 2000/5000+ RSS/耗时 baseline、阈值和 CI perf lane |
| match exhaustiveness 笛卡尔积 | 已关闭正解主线 | `src/sema/match.cpp` 已替换为 pattern matrix / usefulness witness search：按 constructor specialization、default matrix 和 dynamic slice 代表长度特化检查 bool、enum payload、tuple、struct、4096 列以内 fixed array、open integer literal 和 slice pattern 的覆盖性和 unreachable arm；无 guard 和字面量 true guard 计入覆盖，字面量 false 和动态 guard 不计入；`[]` + `[_, ..]`、bool head partitions 等动态 slice 覆盖可被证明，开放整数域重复 literal 会被判为 unreachable；不再枚举笛卡尔积字符串，超过 4096 元素 fixed array 现在显式要求不可反驳 arm | 后续只保留更丰富 witness 文本和 IDE/LSP 展示，不再是 M2.1 隐式边界 |
| parser 栈溢出攻击面 | 已关闭 M2.1 崩溃点 | 左结合二元链使用迭代 operator/operand 栈；深 grouped expression、type nesting、generic type arguments 和 pattern nesting 均有 M2 深度预算并给稳定 parser diagnostic；新增 3000 项二元链、深括号、深 type、深泛型 type args 和深 pattern stress 测试 | 后续只保留长期 parser 架构去递归化，不再是 M2.1 缺口 |
| lexer 二进制输入 DoS | 已关闭 | 连续 invalid byte 聚合为单个 range 诊断；lexer error diagnostics capped at 128 + summary；全局 diagnostic 输出上限和 line table 已落地；`Diagnostic` 已携带 `DiagnosticCategory` / `DiagnosticCode`，lexer/parser/sema/module loader 主诊断入口已写入结构化分类和稳定编号 | 后续只保留 IDE/LSP protocol 输出层 |
| identifier 临时 string | 已关闭当前主热路径 | syntax 层 `IdentifierInterner` 使用 global bump allocator 保存稳定文本；AST name-bearing 字段携带原生 `IdentId`；`SymbolTable` scope lookup、函数/类型/值/generic/enum case/method/member lookup 均使用 typed `IdentId` key；sema typed lookup key 复用 `AstModule` identifier arena，不再维护第二套私有 interner，也不再在 lookup miss 时回扫 string map；`FunctionSignature`、`Symbol`、`StructInfo`、`StructFieldInfo`、`EnumCaseInfo`、`TypeAliasInfo`、`TypeInfo` 和 generic template 的持久文本字段改为 `InternedText`，普通 source name 直接借用 AST identifier interner，生成的 ABI/display/dump 名进入 checked bump-backed interner，复制时重新 intern 到目标 arena，不再复制堆 `std::string` buffer；ABI symbol 校验使用 `std::string_view` key，不再临时复制一整套 C symbol interner；跨模块 `StableModuleId` / `StableDefId` / `StableMemberKey` / `IncrementalKey` 已使用 128-bit fingerprint + kind + disambiguator + global id，写入 checked sema 元数据 | delayed display string 只保留长期 polish，不再阻塞当前性能/语义收口 |
| AST 多次整树复制 / 胖节点 | 已关闭当前主爆点 | module loader append 按 compact payload remap 并重新 intern 到目标 `AstModule`，无 import 的 root module 直接 move 进 combined AST，不再在 release AST bulk 路径峰值持有两棵大树；driver 持有 parser/module AST，并把 mutable 引用交给 sema 和 IR lowering；`SemanticAnalyzer(const AstModule&)` 已删除，避免隐式整树复制；`CheckedModule::normalized_ast` 已改成轻量 normalization overlay，彻底不拥有 `AstModule` snapshot；节点级 C symbol side table 已从 `std::vector<std::string>` 改为 `expr_c_name_ids` / `pattern_c_name_ids` / `item_c_name_ids` 的 `IdentId` 表，文本只进入 checked module C-name interner 去重；sema 构造期不再对 `exprs/types` 做 `size+4096` reserve；postfix suffix 创建不再按值复制胖 `ExprNode` / `TypeNode`；`TypeNode` / `ExprNode` / `PatternNode` / `StmtNode` / `ItemNode` 主存储已改成 32B compact header + per-kind payload arena；旧胖 `ExprNode` 生产类型已删除，parser 创建、`AstModule` 存储、module loader append 和 postfix suffix 都直接写 compact expression header + per-kind payload；parser 表达式创建和 postfix suffix 进一步改为字段级 append/set，不再先构造 name/call/cast/array 等 payload 大临时对象；parser 启动按 token 形态估算 expression header 和每类 payload 的容量上界，直接从 bump arena 取好 vector backing storage 并对表达式 arena page pre-touch；解析中表达式创建只顺序 emplace，不再触发 vector 自动扩容或逐节点首次触页；lexer token 输出改为 bump-backed `TokenBuffer`，用轻量 source scan 估算 token 数后一次性 reserve，不再用 `source_size / 2` 对大文件过量预留，也不再被 262144 token 上限截断或预触不会写入的估算余量页；`syntax::Token` 已压缩为 source pointer + range 计算 text，不再持有 `std::string_view` 大字段；`ModuleLoader` 的 lex+parse 局部化，parse 完成后立即释放 token arena；sema 的 `CheckedModule`、generic side table、pattern case table、type/symbol table、analyzer lookup/cache 主表、函数签名参数/泛型实参、struct 字段、enum payload、`TypeInfo` tuple/function/generic args、generic constraint bucket 和持久 sema 文本字段均使用 bump-backed storage / interner；generic function instance 列表改为 bump-backed deque 保持嵌套实例化期间 side table 地址稳定；generic method / enum-case / visible-module cache bucket 不再由默认 heap vector 生成；sema、IR lowering 和 AST dump 的 `ExprNode` 热路径使用 compact view / payload 直读，literal 分析不再重建胖节点；sema item owner 查询改为显式 `ItemId`，不再靠 `items.data()` 反推；module/import/path 元数据 finalize 后同样进入 AST identifier arena；`perf-release-threshold` 现在默认以 `build/perf-lto` Release+LTO 跑 5000 generic、2M 高复杂 mixed AST statements 和 5000 diagnostic errors，并记录进程级 wall/user/sys/RSS/page fault 与 compiler phase profile；发布 AST RSS 阈值提升到 8192 MiB，避免把 AST 用例降级成过渡态低复杂度源码；轻量 CI stress gate 覆盖 generic/AST/diagnostic lane | 跨机器阈值数值可继续按硬件校准，但不再缺默认 Release+LTO 发布 gate、8 GiB AST 阈值、阶段 profile 或 CI perf lane |

### 当前 Phase 2-5 收口状态（2026-05-16）

Phase 2-5 已按 M2.1 的“先关闭当前语义漏洞、性能爆点和构建阻塞，再保留大架构重写”的边界推进。下面项目已落地到代码、样例或单测：

| 项目 | 当前状态 | 已落地边界 | 后续保留 |
|:-----|:---------|:-----------|:---------|
| generic lookup / identity / key | 已关闭当前语义问题，热路径继续收紧 | generic lookup 复用普通模块可见性/import resolver；generic param identity 已改为 `GenericParamIdentity` 数值身份，由 template module/name、param index、param name 和 decl source range 做稳定 hash，不再把 identity key 存成 interned/string payload；实例 key 和 ABI suffix 基于 canonical type id，用预分配拼接和 `to_chars` 生成，不再靠 display string；generic function instance、generic struct/enum `TypeInfo` 和 checked enum case display 都保存 base name + semantic key/type args，checked dump / IR lowering / 诊断边界才延迟生成 `id[i32]`、`Box[i32]`、`Maybe[i32]_some` 这类展示名；当前 typed lookup key 已复用 AST 原生 `IdentId`；source generic template、实例化 function/struct 和 checked metadata 已挂 `StableDefId` / incremental fingerprint；`--incremental-cache` 已落盘 source/import fingerprint、module row 和 checked stable/incremental def row，并在 `--check` 命中时安全跳过 parse/sema | 后续只保留 query 级复用、细粒度失效和 IDE-native 增量架构 |
| `?` try shape | 已关闭 name-based magic | Result/Option 通过 enum identity、完整 case 集和 payload 形状识别；额外同名 case 或 malformed payload 不被误识别；return enum 的错误/none 路径也做结构化检查 | 未来标准库可提供正式 Result/Option 定义，但不在 M2 绑定 std 名字 |
| M2 unsupported policy | 已关闭主要边界 | generic C ABI/prototype、method-local generics、resource capability、foreign/pointer impl target 等均以 M2 unsupported 或明确 semantic diagnostic 拒绝，并有回归测试覆盖；unsupported 类诊断已写入 `DiagnosticCategory::unsupported` / `SEM0300` | 后续只保留更细 IDE/LSP 展示 |
| reference slice index | 已关闭 | `&[]T` / `&[]mut T` safe deref 后按 slice index 规则检查和 lowering；新增 positive sample 覆盖 IR/LLVM IR | 运行时 bounds check 仍按当前 M2 边界处理 |
| Parser→Sema 契约 | 已关闭 | parser-only AST 自动规范化 root module；带 modules 但 `item_modules` 缺失/非法时 sema 入口诊断失败 | 后续可把契约转成更强 typed AST builder API |
| syntax cache 禁读 | 已关闭 | generic/contextual 分析关闭 syntax type cache 时禁止读旧 cache，也禁止写入污染 | 长期仍应拆 intrinsic/contextual/coercion side table |
| record / enum case lookup 索引 | 已关闭当前退化路径 | struct/enum lookup 命中后做 type identity 校验；enum case 索引 miss 不再静默全表扫描通过；method/member、enum case 和 associated function 名字查找已统一走 `IdentId` typed key；struct field、enum case、method/function、const/type alias 均写入 stable member/def/incremental key；跨进程增量 cache 文件格式和 read/write/reuse 路径已接入 driver | 后续只保留 query 级复用和更细 module/export 失效模型 |
| lookup / registry reserve | 已关闭当前 rehash 热点 | sema 全局表、generic 实例表、module visibility/export cache、函数/enum/const/ABI 临时表和局部 scope 按已知规模 reserve；struct field 注册避免逐字段重复查表；identifier reserve 进入 `AstModule::identifiers`，避免 sema 再开一套 arena；stable id/fingerprint 只做一次性 metadata 生成，不重新引入 string lookup 热路径 | 后续只保留增量编译缓存落盘 |
| Identifier interner / typed lookup key | 当前主线已落地 | syntax 层 `IdentifierInterner` 使用 global bump allocator；AST 原生 `IdentId` 覆盖 type/expr/pattern/stmt/item/module/import 名字字段；parser push、module loader append、postfix set 和 sema 入口都会 finalize/re-intern identifier 元数据；type/function/value/generic template/enum case/struct field/local scope/method/member lookup 使用 `{ModuleId, IdentId}` 或 `{ModuleId, TypeHandle, IdentId}` typed 索引；生产查找函数不再提供 string overload / string-map fallback；checked-module map 不再保留并行 string-key lookup 路径，诊断/dump/ABI 只保留 display string 或 semantic payload；IR lowering 源码 local lookup 也使用 interned typed id；跨模块 stable hash / parallel global ID / incremental fingerprint 已接入 sema metadata；增量 cache 文件已持久化这些 checked stable/incremental 身份 | 后续只保留 query engine、lossless syntax 和 IDE-native 级别的增量复用 |
| lowerer scope / module export cache | 已关闭 | lowerer 使用 scope stack + shadow log，不再每个 scope 复制整个 locals map；module export modules 做缓存 | 后续可加更细粒度失效模型给增量编译 |
| diagnostics line table / 输出上限 / 高价值 note-help / category-code | 已关闭当前主线 | `SourceFile` 建 line starts 表，诊断位置查询不再线性扫描；driver 输出最多 128 条诊断并打印 summary；`Diagnostic::Severity` 支持 error/warning/note/help；lookup miss 对变量/函数/类型/module alias/struct field/enum case 给 `did you mean` help；let/const/assignment/return/call/enum payload/array/tuple/struct literal、if expression branch 和 match arm mismatch 输出 expected/actual note；duplicate function/type/const/local/field/enum case 以及 ABI symbol 冲突输出 previous declaration note；parser 对表达式/type/pattern/call/index/slice 等成对 delimiter 缺失输出 opening delimiter note；driver 诊断渲染支持多行 span 和 `AUREX_COLOR_DIAGNOSTICS=always|never|auto` / `NO_COLOR` 控制的颜色输出；`DiagnosticCategory` / `DiagnosticCode` 已覆盖 lexer、parser、sema type/name/visibility/pattern/safety/unsupported/capability、module 和 internal contract；CLI 文本保持兼容，`--diagnostics=json` 输出 `aurex-diagnostics-v1` 结构化诊断 JSON | 后续只保留 IDE/LSP protocol 适配层 |
| frontend-only CMake / tests | 已关闭 | 增加 `AUREX_FRONTEND_ONLY` 配置和 `aurex_frontend_tests` 目标，可在无 LLVM/IR/driver/CLI 情况下构建运行前端测试；CI stress-thresholds 已覆盖 generic/AST/diagnostic 三条轻量 lane | 后续只保留更细 CI 分层 |
| perf baseline benchmark harness / stress gate | 已接入 baseline、轻量 CI gate、默认 Release+LTO 发布 gate、阶段 profile 和跨机器校准机制 | 增加基于 Google Benchmark 的 `aurex_frontend_bench`、`aurex_frontend_compare_bench`、`tools/perf_report.py` / `make perf`，并通过 `tools/generic_stress.py`、`tools/ast_stress.py`、`tools/diagnostic_stress.py` 和 `tools/perf_thresholds.py` 统一记录 raw/effective thresholds、profile、scale、machine info、Release/LTO build options、进程级 wall/user/sys/RSS/page fault 指标和 `aurexc --profile-output` 阶段 profile；`make perf-stress-threshold` 固化 mixed-feature 轻量阈值质量门，GitHub Actions `stress-thresholds` job 覆盖三条 lane；`make perf-release-threshold` 默认使用独立 `build/perf-lto` Release+LTO 跑 5000 generic、2M 高复杂 mixed AST statements、5000 errors 的发布 lane，`perf-release-lto-threshold` / `perf-release-all-threshold` 为兼容别名；三条 stress 生成器默认 `--shape=mixed` 并覆盖 generic constraints、impl、pointer alias、tuple/slice/pattern、extern、type alias、struct/enum、try/defer/unsafe/loop 和多类 sema 错误；表达式 arena、lexer token arena、compact AST、AST 原生 `IdentId`、bump-backed AST/header/payload vectors、sema 持久 value-payload list 和 generic constraint bucket 已接入当前主路径；`--check` 路径不再保留泛型实例 side table，`--emit=typed` 可单独压测 retained side table。具体 RSS/耗时数字以生成的 stress JSON、profile/scale 校准和 benchmark 输出为准 | 后续只保留新增机器 profile 数据，不再缺 2M/5000/5000 默认 Release+LTO 发布 gate、diagnostic stress、阶段 profile 或校准机制 |

当前增量编译缓存文件格式和 driver read/write/reuse 路径已完成：`--incremental-cache <path>` 会写入 schema/version、root/import path、source content fingerprint、module row 和 checked stable/incremental def row；`--check` 在 root、import path 和所有 source fingerprint 全部匹配时直接命中返回，不再重新 parse/sema。当前 frontend-foundation 的 query/lossless/IDE snapshot 验收入口已完成；仍不应声称已经完成的是独立 LSP protocol adapter 和完整局部 subtree reparse。跨阶段 stable identifier hash / parallel global ID / incremental fingerprint、module alias/field/enum case suggestion、多行 span / 颜色输出、CLI JSON diagnostics、跨机器 perf threshold profile/scale 校准机制，以及默认 Release+LTO 的 2M 高复杂 AST、5000 generic、5000 errors 发布 gate 与阶段 profile 已落地。

### Phase 0：基线冻结和报告建立

1. 复现 review 中的关键数据，并把命令、机器环境和 baseline 写入性能记录：
   - `--check` 冷启动、小文件、千函数、万函数、1000/2000/5000 泛型实例。
   - 大表达式二元树、长左结合操作链、深层括号、深层 block。
   - 10KB 二进制输入、全 null、全 0xFF、全 0x80、分号风暴、海量错误诊断。
   - `tools/frontend_compare.py` 记录可用现代前端版本和 lookup/generic frontend/check 对比；rustc 不可用时记录 missing，不把缺失工具当成 Aurex 性能失败。
2. 把 stress case 固化为可重复工具或测试，不要求每次普通单测都跑最重版本，但必须能在 release 前手动或 CI perf lane 复测。
3. 建立 M2.1 禁止线：
   - 不允许新增会绕过 unsafe 的 raw pointer 入口。
   - 不允许新增依赖 display string 的语义身份。
   - 不允许新增整模块按实例复制的 side table。
   - 不允许新增无上限递归、无上限错误恢复或无上限诊断输出。

### Phase 1：P0 语义闭包

1. raw pointer projection 统一 unsafe 检查。
   - 引入统一的 place/projection 表示，例如 `PlaceInfo { is_place, is_writable, crosses_raw_pointer }`。
   - `*p`、`p.field`、`p.field = v`、`p[i]`、`p[i] = v` 只要 projection 链上穿过 raw pointer，就必须调用同一套 unsafe context 检查。
   - `&T` / `&mut T` reference 的 field/index projection 保持 safe，但 mutability、writable place 和 const pointee 规则必须继续生效。
   - 新增 negative：`raw_pointer_field_requires_unsafe`、`raw_pointer_field_write_requires_unsafe`、`raw_pointer_index_requires_unsafe`、`raw_pointer_index_write_requires_unsafe`。
   - 新增 positive：同样操作包在 `unsafe { ... }` 内可通过，并验证 write lowering 不生成无效 IR。
2. 拆除 `&expr` 的 raw pointer 兼容语义。
   - `&place` 永远产生 `&T`，`&mut place` 永远产生 `&mut T`。
   - 需要 raw pointer 时使用显式 `ptraddr(...)` / `ptrat<T>(...)` 这类 builtin，并按 unsafe/ABI 边界诊断。
   - 所有函数调用、变量初始化、return、struct literal 字段填充都不能因为 expected type 是 `*const T` / `*mut T` 而把 `&x` 改判为 raw pointer。
   - 新增 negative：`ref_ptr_diverge`、`implicit_address_to_raw_pointer_rejected`。
3. 修复 expression type cache 的 contextual typing 污染。
   - 已拆成 intrinsic type、contextual final type 和 coercion/adjustment 三层：`expr_intrinsic_types` 记录表达式自身类型，`expr_types` 记录当前语义使用的 final type，`expr_expected_types` 是 final cache key，`CoercionRecord` 记录实际调整。
   - integer/float literal、`null`、reference/unary、binary、generic call、array literal、struct literal、tuple literal、block/match/if expression 的 final type 不能跨 expected type 复用；array/tuple/if/block/match 的 expected-type 场景已有白盒覆盖。
   - generic body checking 期间，`cache_syntax_types` 关闭时必须既禁写也禁读，避免实例化读到模板上下文残留类型。
   - 已覆盖：`contextual_type_cache_attack`、`null_context_attack`、`generic_context_cache_no_read` 等样例/白盒等价测试。
4. 拆清 `[]` 的语义节点。
   - type context 中的 `Name[Args]` 进入 type apply；expression context 中含 `:` 的后缀进入 slice；确定是 generic call/value index 后 checked 层必须记录为不同节点。
   - 诊断必须区分：非泛型类型被 type apply、非泛型函数被 explicit generic call、非 indexable 值被 index、slice bound 非整数、slice 对象不是 array/slice/str。
   - 后端和 IR lower 不再通过“看起来像 bracket postfix”猜语义。
5. Capability 改成结构化能力。
   - 用 `enum class CapabilityKind { Sized, Eq, Ord, Hash }` 或等价结构替代字符串 predicate。
   - capability parser、where clause、generic instantiation 和诊断共用一套 capability 表；直接 operator 支持集和 capability satisfaction 必须分别写清楚。
   - `Eq` / `Ord` 是代数能力 predicate，不等同于 `==` / `<` 等 operator availability。当前 `f32` / `f64` 支持直接比较 operator，但由于 NaN/序关系语义不会满足 generic `Eq` / `Ord` capability；reference 也不满足这些非资源能力。
   - `Hash` 当前只是 marker-only admission predicate：`bool` / `char` / integer / pointer 满足 `Hash`，但 M2.1 不提供 hash operator、稳定 hash ABI 或用户自定义 hash 实现。
   - 新增 negative：`float_eq_rejected`、`float_ord_rejected`、`reference_does_not_satisfy_eq`、`reference_hash_rejected`、`enum_hash_rejected`、`where_unknown_capability_structured`；具体 `f32` / `f64` 仍支持 `==` / `<` 等直接 operator，但不会满足 generic `Eq` / `Ord` capability。
6. 补齐数组常量索引边界检查。
   - 对固定数组和可静态求值的整数 index，在 sema 阶段检查 `0 <= index < length`。
   - 变量 index 仍按当前 M2 运行时/后端边界处理，但不能把常量越界留到 LLVM 或 native crash。
   - 新增 negative：`array_constant_index_out_of_bounds`。
7. 收紧 `return null` 和复合表达式 null 推导。
   - `return null`、if/block/match tail `null`、`?` 错误路径中的 `null` 必须按函数返回类型或 expected type 进行 contextual typing。
   - 无指针 expected type 时拒绝 `null`，有指针 expected type 时保留正确 pointee/mutability，不把 `null` 降成 invalid type 后污染外层表达式。
   - 新增 positive/negative：`return_null_infer`、`null_if_expr_expected_pointer`、`null_match_expr_expected_pointer`、`null_without_pointer_context_rejected`。

### Phase 2：P1 语义一致性和可扩展身份

1. 统一 generic lookup 与普通 lookup。
   - generic struct/enum/type alias/function/impl lookup 必须走和普通类型/值同源的模块可见性与 import resolver。
   - 修复 visible modules、qualified import、ambiguous import 和 private generic item 的不一致。
   - 新增 mixed：`imported_generic_lookup`、`private_generic_import_rejected`。
2. generic parameter identity 改为结构化身份。
   - 已改为 `GenericParamIdentity` 数值身份；`T` 不再只按名字成为全局等价类型，身份混入 template module/name、parameter index、parameter name 和 decl source range，并缓存到模板信息 / `TypeInfo`。
   - 嵌套 generic、generic alias、generic impl、同名类型参数 shadowing 必须可区分并可诊断。
3. mangling 和实例化 key 不能依赖 display string。
   - 使用 module id、symbol id、generic args type ids、ABI namespace 组成结构化 key。
   - display name 只用于诊断和 dump，不能决定链接名唯一性。
   - 新增 negative：`generic_mangle_collision`、`module_mangle_display_collision`。
4. `?` / try-like 语义摆脱纯 name-based magic。
   - 已按本项要求把 `Result` / `Option` shape 规则收紧为结构化识别：enum identity、case identity、payload 形状、错误类型转换规则。
   - 用户定义同名 `ok` / `err` / `some` / `none` 不能被误识别。
   - 新增 mixed：`option_result_magic_name_collision`。
5. 区分 M2 unsupported 与 semantic error。
   - array by-value 参数/返回、array-containing assignment、generic C ABI/prototype、foreign impl、method-local generics 等应给出 `M2Unsupported` 或等价诊断类别。
   - 不再把后端限制伪装成语言语义错误；文档和测试应能说明这是阶段限制还是永久规则。
6. reference slice index 一致性。
   - `&[]T` / `&[]mut T` 在 safe deref 后应按 slice 规则 index，或者明确诊断当前 M2 不支持 reference-to-slice index。
   - 新增 positive：`reference_slice_index`。
7. Parser 到 Sema 的隐藏契约显式化。
   - `item_modules`、source ranges、module ownership 等必须在 sema 入口做 assert/diagnostic，而不是默认由某条 driver 路径填好。
   - 新增 unit：`parser_ast_requires_item_modules`。
8. record/member 查询做二次验证。
   - `find_record` / member map 命中后验证 type identity，防止哈希或 display key 退化造成错绑。
   - `find_enum_case` fallback 全表扫描只能作为 debug/assert 辅助，不应掩盖索引失效。
9. 拆分 `analyze_expr` 的职责。
   - 已关闭当前工程质量问题：`analyze_expr(expr, expected)` 只保留 final-type cache lookup 和 expected-type key 记录，`analyze_expr(expr, view, expected)` 只按表达式类别调度。
   - literal、value/name/call、control、aggregate、projection、operator、builtin 分别进入明确 helper；这些 helper 通过统一契约声明是否可递归调用 `analyze_expr`、是否必须记录当前 expr result，以及 intrinsic/final/coercion 记录边界。
   - binary expression 已进一步拆成 operand contextual typing、类型不匹配诊断、整数字面量 hazard 检查和 operator result 记录；没有保留第二套 analyzer 或新旧并行路径。
10. foreign impl 和 unsupported policy 定死。
   - `impl` target、pointer impl、generic extern/prototype、method-local generic 等边界要么实现，要么统一用 M2 unsupported 诊断拒绝。
   - 新增 negative：`foreign_impl_policy`、`generic_extern_unsupported_category`。

### Phase 3：P0 性能和安全收口

1. 泛型 side table 局部化或稀疏化。
   - 当前按全模块 `exprs.size()` / `patterns.size()` 为每个实例分配 side table，是 2000 实例内存爆炸的直接原因。
   - M2.1 先改为函数 body 节点区间、实例相关节点切片或 sparse map；只有实际分析到的 expr/pattern 才占内存。
   - 实例结束后释放临时上下文；跨实例共享只保留 canonical type、checked signature 和必要 memo。
   - 验收：200、500、1000、2000 实例 RSS 接近线性；2000 实例不再超过 200MB，目标约 150MB。
2. match exhaustiveness 防指数爆炸。
   - 已替换为 pattern matrix / usefulness witness search，不再生成结构化叶子笛卡尔积字符串；4096 fixed-array 列上限现在是显式 M2.1 语义边界，超过时要求不可反驳 arm。
   - bool、enum payload、tuple、struct 和 4096 列以内 fixed array 通过 constructor specialization / default matrix 检查覆盖性；`.some(true)` / `.some(false)` 这类 enum payload 拆分可正确证明穷尽。
   - 无 guard 和字面量 true guard 计入穷尽覆盖；字面量 false 和动态 guard 不计入；open integer literal、dynamic slice 长度和 slice element pattern 已进入 witness/usefulness 主路径。
   - 验收：13 bool 字段结构体的 partial-field 穷尽正例通过；13 bool 字段全字段少量 case 的非穷尽负例报错；enum bool payload split 正例通过、缺 payload witness 负例报错。
3. parser 左结合操作链循环化，深层分组加预算。
   - `a+b+c+...`、长比较/逻辑链、postfix 链采用 loop/Pratt climb，不用线性递归堆栈。
   - 括号、泛型参数、pattern 嵌套、block 嵌套设置明确 recursion/depth budget，超限给诊断。
   - 验收：`token_chain_3000` 不崩溃；深括号输入给出单个清晰诊断。
4. lexer error budget 和无效字节聚合。
   - 连续 invalid byte 作为 range 聚合诊断，不逐字节做昂贵恢复。
   - 单文件诊断数量、无效字符扫描、错误恢复 token 消耗都应有 budget。
   - 验收：10KB 全 null / 0xFF / 0x80 在 < 50ms 内拒绝；诊断输出不超过上限。
5. diagnostic line table 和输出上限。
   - SourceFile 建立 line starts，line/column 查询从线性扫描改为 O(log lines) 或 O(1) amortized。
   - 大量错误默认截断并输出“too many errors” summary；保留开关给调试模式输出更多。
   - 验收：5000 errors 场景进入 diagnostic stress 阈值门，输出大小受控；具体耗时由 stress JSON/profile 记录。
6. 延迟 LLVM 初始化。
   - `--check`、parser/sema-only、dump tokens/ast 等路径不得初始化 LLVM backend 或 clang/toolchain。
   - 验收：小文件 `--check` 不初始化 LLVM；`--emit=ir` / native 路径只在需要时付 LLVM 成本。具体耗时由 benchmark/profile 输出记录。
7. Identifier interner 和 typed lookup key。
   - identifier、module name、qualified name、field name、case name 改为 `IdentId` / `QualifiedNameKey` 或等价结构。
   - hot path 不再反复构造 `std::string`；lookup map 建表前 reserve。
   - display string 延迟生成，只用于诊断/dump。
   - 已完成当前主线：syntax 层 `IdentifierInterner` 使用 global bump allocator；AST 名字字段携带原生 `IdentId`；type/function/value/generic template/enum case/struct field/local scope/method/member lookup 使用 AST module interner + typed lookup key；生产查找函数不再提供 string overload / string-map fallback；`CheckedModule` map 不再保留并行 string-key lookup 路径；source name 持久字段改为借用 AST identifier interner 的 `InternedText`，生成的 ABI/dump/display 文本才进入 checked bump-backed interner；ABI symbol 校验借用 `std::string_view` key，不再复制第二套 C symbol interner；IR lowering 源码 local lookup 也使用 interned typed id；`StableModuleId` / `StableDefId` / `StableMemberKey` / `IncrementalKey` 已覆盖 checked sema metadata。
8. Sema 不再复制整棵 AST。
   - Sema 读取 parser/module AST 引用；normalized AST 使用 overlay 或 per-node adjustment，不复制胖节点。
   - 大表达式和大模块场景优先减少 3-4 次整树复制。
   - 已完成当前主线：driver 持有 AST 并向 sema/IR 传引用，`CheckedModule::normalized_ast` 是轻量 overlay 且不拥有 AST，`SemanticAnalyzer(const AstModule&)` 被删除以禁止隐式整树复制，节点级 C symbol side table 使用 `IdentId` 而不是 per-node `std::string`，sema 构造期不再触发 `exprs/types` 整体 reserve，postfix suffix 创建移除 `ExprNode`/`TypeNode` 按值复制。
   - 验收：默认 Release+LTO 发布 gate 覆盖 2M 高复杂 mixed AST statements；parser token buffer 已压缩为 pointer + range，ModuleLoader parse 完成后释放 token arena，阶段 profile 可直接看到 module.read/lex/parse 和 sema.analyze 的耗时/RSS-after。具体 RSS/耗时以生成的 stress JSON 和 profile/scale 校准数据为准。若继续收紧阈值，后续优先评估 streaming parser 或进一步减少 source/token/AST 同峰持有。
9. Lowerer scope stack 和 module export 缓存。
   - Lowerer 不在每个 scope 复制整个 `locals_` map，改为 scope stack + shadow log。
   - `module_export_modules()` / qualified lookup 结果缓存，失效点明确。
10. 成员索引、enum case 索引和 map reserve。
   - struct field、method、enum case、module export、function/type/value registry 建表前统一 reserve。
   - 成员查找优先使用 `(type_id, ident_id)` typed key；线性扫描只作为小集合 fallback 或 debug 验证。
   - enum case 索引 miss 时不再静默全表扫描通过；debug 模式可以 assert 索引一致性，release 模式给内部错误或保守诊断。
11. Generic key 结构化并延迟 display。
   - 泛型实例化 key 已使用 template key + canonical type handle args，不使用 `display_name()`；generic param identity 已缓存，实例 key / ABI suffix 使用预分配拼接和 `to_chars`，减少热路径临时字符串。
   - generic function instance 的 checked signature 已改为 base name + semantic key + type args；`function_display_name()` 只在 checked dump / IR lowering 需要输出名时生成展示字符串，`--check` 热路径不再为函数实例生成 `id[i32]` 展示名。
   - generic struct / enum 的 `TypeInfo.name` 和 `StructInfo` / `EnumCaseInfo` 用户展示名已改为 base name + generic args 延迟格式化；实例化热路径不再为 `Box[i32]`、`Maybe[i32]`、`Maybe[i32]_some` 生成展示字符串，checked dump、IR record/constant name 和诊断需要时再格式化。
   - `SemanticOptions.retain_generic_side_tables` 将 check、typed retained-body 和 IR/codegen 模式分开；`--check` 路径不保留泛型实例 side table，只保留签名、语义 key 和必要类型信息；`--emit=typed` 保留 typed generic body 但不做 lowering；IR/native 输出路径继续保留 lowering 所需 side table。
12. Compact AST 和 bump allocator 作为 M2.1 后半段优化。
   - 当前已经消除 sema/checked 主路径整树复制和 side table 爆炸；`TypeNode`、`ExprNode`、`PatternNode`、`StmtNode` 和 `ItemNode` 已落为 32B header + per-kind payload arena；旧胖 `ExprNode` 生产类型已删除，parser 创建、`AstModule` 存储、module loader append 和 postfix suffix 都直接写 compact expression header + per-kind payload；sema、IR lowering 和 AST dump 的 `ExprNode` 热路径已迁到 compact view / payload 直读；global bump allocator + AST 原生 `IdentId` 已覆盖当前 parser/module/sema 主路径；AST list header/payload vectors 和 identifier interner text/hash 存储已接入 bump，并由 parser token-shape estimate 做源规模 reserve；lexer token 输出和 sema 持久 side table / lookup bucket 也已接入 bump arena，lexer token reserve 不再有大文件截断或未写入页预触，generic function instance 存储保留 deque 稳定地址语义；retained instance 使用函数体局部 NodeSpan table，并只为非连续 NodeSpan 共享 module-level sparse layout；sema-only expected-type / pattern-case cache 已进入 release 路径。
   - benchmark 和 stress profile 是机器相关 RSS/耗时数据的唯一来源；主文档只保留 gate 形态和 profile/scale 校准机制，不再把本机样例数字写成可移植 baseline。
   - release gate 已覆盖 2M 高复杂 AST statements、5000 generic 和 5000 diagnostic errors，默认使用独立 `build/perf-lto` Release+LTO；`AUREX_PERF_THRESHOLD_PROFILE` / `AUREX_PERF_THRESHOLD_SCALE` 已作为跨机器阈值校准入口，并随 stress JSON 记录 raw/effective thresholds、机器信息、build options、进程级指标和 compiler phase profile。

### Phase 4：诊断信息升级

1. `did you mean` 建议已接入主 lookup。
   - 变量/local/global value、函数和类型 lookup miss 使用同 scope 候选和编辑距离；候选受模块/可见性/命名空间限制，不建议 private 或错误 namespace 的名字。
   - module alias、field、enum case 的 suggestion 已接入同一 `did you mean` help 路径。
2. expected/actual 类型显示已覆盖高频 mismatch。
   - let/const initializer、call argument、enum payload argument、return、assignment、struct field init、array element、tuple element、array repeat value 都显示 expected 与 actual。
   - if expression branch 和 match arm 也已接入同一 helper。
   - 类型显示使用 canonical display，避免 mangled/internal name 泄漏给用户。
3. previous declaration note 已覆盖主 duplicate 路径。
   - duplicate function/type/const/local/field/enum case、函数 prototype/definition 冲突和局部 shadowing restriction 都标记上一处声明。
   - ABI collision 的 previous note 可继续补细。
4. parser paired-token note 已覆盖主要 delimiter 场景。
   - 表达式分组、array literal、call、index/slice、named type generic args、tuple type、tuple/slice/struct/payload pattern 等缺 `)` / `]` / `}` 时输出 opening token 位置。
5. warning 和 note/help 级别已接入。
   - Diagnostic 支持 error/warning/note/help；M2.1 默认仍以 error 为主，lookup suggestion 使用 help，type mismatch / previous declaration / paired token 使用 note。
6. 多行 span 和颜色输出已接入 driver。
   - 多行 source span 会逐行显示 underline；颜色由 `AUREX_COLOR_DIAGNOSTICS=always|never|auto` 和 `NO_COLOR` 控制，非 TTY 默认不污染测试输出。

### Phase 5：测试矩阵和完成定义

1. 补 frontend-only 构建和测试模式。
   - CMake 增加不依赖 LLVM backend 的 frontend-only 配置或 target，至少能跑 lexer/parser/sema/diagnostic/golden 前端测试。
   - 普通 frontend 变更不应被 LLVM 安装、链接或 backend 初始化问题阻塞。
   - CI 或本地质量门区分 frontend-only、IR、LLVM backend、native execution 四层，便于快速定位回归来源。
2. 建立 stress/perf 测试分层。
   - 常规单测只跑轻量 baseline case；`make perf-stress-threshold` / CI `stress-thresholds` 跑 100/200 mixed 泛型实例、1000/5000 mixed AST bulk 和 100/500 mixed diagnostic errors 的阈值门；`make perf-release-threshold` 默认用 Release+LTO 跑 5000 mixed 泛型实例、2M 高复杂 mixed AST statements 和 5000 mixed errors；`make perf-release-lto-threshold` / `make perf-release-all-threshold` 是同一发布 gate 的兼容别名。
   - 每个 perf case 记录基线、目标、允许波动、profile/scale 和机器说明。

M2.1 至少新增或更新以下测试类别：

| 类别 | 必须覆盖 |
|:-----|:---------|
| unsafe/raw pointer | raw pointer field/index/read/write 需要 unsafe；raw pointer field slice 需要 unsafe；raw pointer / safe reference method receiver 不互相隐式满足；reference field/index 不误报；unsafe block 内通过 |
| contextual typing | literal/null/reference/generic call/aggregate literal 在不同 expected type 下不读旧 final cache |
| capability/generic | reference capability 拒绝；Hash marker-only 边界；float 支持直接比较 operator 但不满足 `Eq` / `Ord`；imported generic lookup；generic param identity；mangling collision |
| `[]` 语义 | generic apply、type apply、index、slice 的正反例和诊断区分 |
| array bounds | 常量 index 越界拒绝；变量 index 保持当前策略 |
| parser/type stress | 3000 token 操作链、深括号、深泛型、深 pattern、deep type nesting 不崩溃 |
| lexer stress | 二进制、全 null、全 0xFF、控制字符风暴快速拒绝 |
| match stress | 小结构精确 exhaustiveness；大结构 witness search 不枚举全空间；guard literal truth、open integer duplicate/uncovered domain 和 dynamic slice 代表长度 witness 已建模 |
| diagnostics | expected/actual、did-you-mean、previous declaration note、too many errors |
| perf | 500/1000/2000/5000 mixed 泛型实例 RSS/耗时；10000/50000/100000 mixed AST bulk RSS/耗时；2M 高复杂 mixed AST 节点 RSS/耗时；5000 mixed errors RSS/耗时；默认 Release+LTO 发布 gate；进程级 wall/user/sys/RSS/page fault 和 compiler phase profile |

完成定义：

1. `tools/run_tests.sh` 通过，新增 negative/positive samples 全部纳入 suite。
2. P0 stress/perf case 不能崩溃、不能超出预算、不能产生无上限诊断。
3. `docs/spec/m2_grammar.md`、`docs/spec/m2_syntax_matrix.md`、`docs/spec/m2_unsupported.md` 和 `docs/zh/progress.md` 与实际实现同步。
4. review 中 P0 语义项和 P0-Perf 项全部关闭；未关闭的 P1/P2 必须有 issue、测试或文档说明。
5. 在完成上述闭包前，下面的语言特性路线只允许做文档澄清、测试补强和 bug fix，不作为新能力扩张入口。

## 优先路线

1. 现代基础语法第一优先级

   当前阶段先完善基础语法，而不是扩张高级特性或重建库层。对照范围不只限 Rust、Go、Zig、Kotlin、C++，还要参考 Swift、C#、TypeScript/JavaScript、Python、Dart、Scala、OCaml/F#、Haskell、Julia、Nim、D、V 等现代语言，以及 ML/ADT、pattern matching exhaustiveness、null safety、type soundness 和 unsafe boundary 相关研究。结论是：Aurex 的大语法骨架已经具备，第一优先级应收口现代语言共同证明过的基础表达能力，而不是先做 trait、borrow checker、macro、async、std 或通用 iterator protocol。

   第一批 P0 基础语法按这个顺序推进：

   - 内建操作拼写已经先规范为 `sizeof<T>`、`alignof<T>`、`cast<T>(x)`、`ptrcast<T>(p)`、`bitcast<T>(x)`、`ptraddr(p)`、`ptrat<T>(addr)`、`text.ptr`、`text.len`、`strvalid(bytes)`、`strfromutf8(bytes)`、`strraw(data, len)`；旧的函数式拼写和旧 `[]` 泛型写法不再作为源码语法。
   - 最小 `unsafe` block / `unsafe fn` 已完成：raw pointer dereference、`ptrcast`、`bitcast`、`ptrat`、`strraw` 这类破坏不变量的操作已经不能留在普通安全表达式表面。
   - ADT-first enum 已完成非泛型 M2 形态：普通 `enum OptionI32 { some(i32), none }` / `enum Token { span(usize, usize), eof }` 成为主力写法；保留 `enum Status: u8 { ok = 0, err = 1 }` 作为显式 C-like/repr enum。
   - array literal / repeat literal 已完成：`[1, 2, 3]` 和 `[0; 128]` 现在能构造固定长度数组值。

   已完成的第一优先级基础项：default private、ADT-first enum、enum multi-payload destructuring、array literal / repeat literal、slice type/expression、tuple/destructuring、function type / function pointer、最小 unsafe 和 M2 pattern ergonomics。顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`；`export c fn` 仍强制 public，`impl` / `extern` block 不能显式 `priv`。slice 当前是 `ptr + len` 的 borrowed fat value，支持 `[]T` / `[]mut T` 和 `a[l:r]`、`a[:r]`、`a[l:]`、`a[:]`，不包含容器迭代或运行时 bounds check。tuple 当前支持 `(A, B)` / `(A,)` 类型、`(a, b)` / `(a,)` 字面量、数字字段访问 `value.0` / `value . 1`、局部 `let (a, _) = value;` 解构和 `match pair { (a, b) => ... }`；匿名 tuple 不支持 `.first` / `.second` 这类命名字段；空 tuple 不属于 M2。函数类型当前是非捕获函数指针，支持 `fn(...) -> T`、`unsafe fn(...) -> T`、`extern c fn(...) -> T`、`unsafe extern c fn(...) -> T`、函数名作为值以及局部/参数/字段函数指针间接调用；完整 closure 捕获仍暂缓。

   第二批 P1 基础语法中 raw/multiline raw string、byte string、Unicode scalar `char`、数值后缀、tuple/destructuring、struct pattern、slice pattern、nested enum payload pattern、binding or-pattern alternatives、`let ... else`、`if value is pattern`、`while value is pattern` 和 if 表达式 pattern condition 已补齐。容器迭代、完整 closure 捕获、trait/interface/protocol、package manager、macro、async 继续暂缓。完整库存和优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

2. unsafe 与 `str` 安全边界

   最小 unsafe 已落地：`unsafe { ... }` 建立 unsafe context，带 tail expression 时作为表达式求值，没有 tail expression 时类型为 `void`；`unsafe fn` 和 unsafe 函数指针调用必须发生在 unsafe context。raw pointer 解引用、`ptrcast`、`bitcast`、`ptrat`、`strraw` 已经是 unsafe-only。M2 这里不包含 borrow checker、lifetime、unsafe trait、unsafe impl、unsafe extern block 或资源/所有权模型。`str` 的无 std checked UTF-8 边界已冻结为 `strvalid(bytes) -> bool` 和 `strfromutf8(bytes) -> str`；失败时返回空 `str`，不会把无效输入包装成 UTF-8 文本。

3. enum ADT 与 pattern 地基

   enum ADT-first 已落地到非泛型 M2 基线：base type / discriminant 可省略，tag 自动分配，多字段 payload 可构造，并且 pattern 侧支持 `.case(a, b)`、`.case((a, b))` 等按字段/嵌套解构。局部 tuple/struct/slice/enum destructuring、match tuple/slice/struct pattern、binding or-pattern alternatives、`let ... else`、`if value is pattern`、`while value is pattern` 和 if 表达式 pattern condition 已落地。

4. 数组、slice、字符串与函数类型基础语法

   Aurex 已有数组类型和值语法、borrowed slice、tuple、`str`、C string、raw/multiline raw string、byte string、byte literal、Unicode scalar `char`、函数声明、函数指针类型、C FFI、最小 safe reference、最小非资源类 `where` capability 和结构化 match 覆盖检查。字面量体系、tuple 基础、pattern ergonomics、`str` checked UTF-8 构造/切片、`&T` / `&mut T`、generic enum / generic type alias / owner generic impl 已经补齐到 M2 基线；下一步更适合继续收窄剩余诊断边界，而不是重建库层。

5. 值语义边界

   M2 当前已经删除 M1 的 `move(...)` / `noncopy struct`，避免把失败的 move-only MVP 继续当作语言地基。当前阶段不继续推进资源模型；只写清普通值传递、struct/enum payload、match payload、`?` 和数组/含数组类型限制的统一规则。

6. 资源语义暂缓

   `Copy` / `Drop` / destructor / borrow / move-out 不作为当前 M2 近期任务。等 `unsafe`、ADT、array/slice/string、function type 和 pattern 地基稳定后，再重新开资源语义专题。

7. safe reference

   最小 `&T` / `&mut T` 已进入 M2 核心：reference 和 raw pointer 是不同类型，`&mut` 要求 writable place，reference 解引用是 safe，raw pointer 解引用仍是 unsafe-only。borrow checker、lifetime、borrowed return、alias model 和资源语义继续暂缓。

8. Capability / trait / where

   最小语言机制已落地：`where T: Eq + Hash` 支持内建非资源能力 `Sized`、`Eq`、`Ord`、`Hash`，并在泛型实例化、泛型函数体检查和 capability 约束上给出诊断。`Eq` / `Ord` 和直接比较 operator 支持集不是同一个概念；`Hash` 目前也只是 marker-only capability，不提供运行时 hash 操作。资源相关能力 `Copy` / `Drop`、用户自定义 trait、associated type、const generic、trait object 和完整 protocol 仍等后续专题再定。

9. 字符串基础类型

   保留 `str` 作为语言级 borrowed UTF-8 slice 的设计方向，但不要复活旧 std 的 `String`/`Bytes` 实现。`str` 的类型、ABI、字面量、unchecked `strraw`、checked `strvalid` / `strfromutf8` 构造边界，以及 `text[l:r]` checked byte-offset slicing 已落地；未来库层 text API、Unicode scalar/grapheme 迭代和拥有型 `String` 继续后置。

10. 测试性能继续收口

   保持测试 harness 直接调用 C++ driver，避免每个用例启动脚本。新增用例时区分 check/IR/native 三类，只有必须验证运行时行为的样例才生成并执行二进制。

## 明确暂缓

- std 容器、文件、目录、进程、console。
- M1 frontend / axbuild 样例；M1 阶段已经舍弃，不能作为当前路线继续推进。
- host support C shim。
- 安装后 std 查找。

重新设计或重新评估这些内容的前置条件是：基础语法、模块边界、`unsafe`、ADT、slice/string 和最小泛型约束已有稳定语言级设计和测试矩阵；拥有型资源库还需要后续资源语义专题完成。
