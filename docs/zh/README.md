# Aurex 文档

文档基线：**M27d Aurex Macro Output Contract Admission**，建立在已经收口的 M2 language-core-no-std、
M2.5 frontend-foundation、M3 query-backed/module/generic、M4 trait/protocol 和 M5 default trait methods
基线、M6 资源/cleanup/drop-glue release baseline、M7a CFG-sensitive borrow facts、M7b borrow contract /
reborrow / two-phase receiver、M7c lifetime/storage escape、M7d-B struct field place-state 以及 M7d-C RAII
user surface 实现收口基线之上。M7 Hardening Performance Closure 和 M7d-K Array Repeat Resource Safety Closure
也已完成；M8 已完成 borrowed dyn trait / erased view 的 query 地基、frontend syntax/sema、borrowed dyn
coercion、checked vtable facts、IR/backend runtime dynamic dispatch、M8e hardening 和 sample / release polish
收口。M8 已正式封口；M9 Dyn ABI / Tooling Release Closure 也已完成，当前已经完成 borrowed dyn 的 ABI
DTO、metadata/fingerprint schema、query/cache、tooling projection、verifier/backend negative matrix、
M9c Advanced Dyn Design Gate 和 M9 release baseline。M10a 已从 M9c gate 中选择 supertrait upcasting 作为 M10
第一条 advanced dyn 主线；M10b 已把 frontend/query/sema 的 check-only 子集落到代码；M10c 已继续完成
`trait_object_upcast` IR、`supertrait_vptr_metadata_v1` vtable metadata、LLVM parent vtable projection 和
native inherited supertrait dispatch；M10d 已完成 query/cache/tooling projection、negative sample matrix、
documentation tests、coverage gate 和 **M10d Supertrait Hardening / Release Closure** release baseline。
M11a 已从剩余 advanced dyn 候选中选择
principal-set borrowed dyn composition 作为下一条主线，并通过 `m11a_dyn_advanced_design_gate_baseline` 固定
`principal_set_metadata_v1`、`principal_set_identity_fact`、`composition_witness_set_fact`、
`principal_method_namespace_fact`、`associated_equality_merge_fact` 和 `composition_projection_fact`。
M11b 已把该模型落成 `PrincipalSetCompositionFacts` query 原型，覆盖 principal-set identity、composition witness
set、principal-qualified method namespace、associated equality merge、composition projection、validation、
summary/dump/fingerprint 和 focused query tests。M11c 已把该 query 地基接到 frontend/sema check-only：
当前用户可写 borrowed composition spelling 是 `dyn (A + B)`，parser/AST/type identity/checked dump/fingerprint
已支持 principal-set trait object，`&T` / `&mut T` 可以在所有 principal impl 可见时 coercion 到
`&dyn (A + B)` / `&mut dyn (A + B)`，并记录 identity、method namespace、associated equality merge、
witness set 和 projection facts。M11d 已进一步实现 IR/backend runtime projection：`&dyn (A + B)` 可以显式
project 到 `&dyn A` / `&dyn B`，lowering 为 `trait_object_composition_pack` /
`trait_object_composition_project` IR，LLVM 使用 `principal_set_metadata_v1` metadata global 从 canonical
principal index 取出目标 vtable。M11e 已完成 release closure：`FunctionDynAbiFacts` 暴露 `principal_sets`
和 `composition_projections`，lower-IR query/IDE hover 消费 composition runtime descriptors，IR verifier 负例矩阵和
documentation tests 已固定 release boundary。M12a 已打开无歧义 direct composition method dispatch：`combo.draw()`
会选择唯一提供 `draw` 的 principal，隐式 composition projection 后复用 ordinary dyn vtable dispatch；M12b 已完成
receiver-access binding、associated equality direct dispatch、direct/explicit projection 去重、dyn ABI fingerprint 和
negative matrix hardening。当前仍不实现
标准库、owning dyn runtime、`Box<dyn Trait>`、dynamic Drop dispatch runtime、allocator policy、bare
`dyn A + B` syntax 或歧义 composition-to-supertrait 自动选择。M13a 已从剩余 advanced dyn 候选中选择
borrowed composition-to-supertrait explicit projection 作为下一条主线，并通过
`m13a_dyn_advanced_design_gate_baseline` 固定 `borrowed_composition_supertrait_projection`、
`composes_existing_metadata_policies`、`composition_to_supertrait_projection_fact` 和
`do_not_make_composition_to_supertrait_direct_call_implicit`。M13a 仍不直接引入标准库、owning dyn、allocator 或
dynamic Drop dispatch。M13b 已把该路线落成 frontend/query/sema check-only：当前可写
`dynproject[SourcePrincipal, TargetSupertrait](view)`，要求 `view` 是 borrowed principal-set composition，
source principal 在 composition 中，target 是该 source principal 的 supertrait；成功后记录
`CompositionProjectionFact{kind=composition_to_supertrait}`。M13c 已把它 lowering 为
`trait_object_composition_project` + `trait_object_upcast`，复用 `principal_set_metadata_v1` 与
`supertrait_vptr_metadata_v1`；M13d 已把该 runtime surface 收口到 query/cache/tooling/verifier release baseline：
`FunctionDynAbiFacts::composition_supertrait_chains`、IDE hover、lower-IR fingerprint 和 negative verifier matrix 都能
显式展示 / 校验 `composition_project -> upcast` chain。M14 已在唯一 source-principal path 下支持
`let parent: &dyn Parent = view;` 和 `view.parent()`，并记录 `BorrowedDynViewPathFact`；歧义 path 仍要求显式
`dynproject[...]`。M15 已新增 advanced dyn ownership/runtime boundary 和 const generic boundary 两条 query design
gate：`m15_dyn_advanced_design_gate_baseline()` 固定 owning dyn、dynamic Drop dispatch 和 allocator policy 的后续
边界；`m15_const_generic_design_gate_baseline()` 固定 typed scalar const generic、canonical const value、generic
instance const arg key 和 `[N]T` array length 集成的后续路线。M16 已把该 const generic 路线落到
parser/AST/query/sema check-only：当前可写 `struct ArrayView[T, const N: usize]`、`ArrayView[i32, 4]` 和
`[N]T`，`GenericInstanceKey::const_args` 会携带 const argument fingerprint，函数体内 `return N;` 可解析为 const
generic value。M16 仍不实现标准库、owning dyn runtime、`Box<dyn Trait>`、dynamic Drop dispatch、runtime ABI lowering
for unresolved const-param arrays、generic const arithmetic、const where predicate、associated const 或 dyn const
equality dispatch。M17 已完成 dyn ownership runtime preparation：`DynOwnershipRuntimeFacts`、
`DynOwnedContainerBoundaryFact`、`DynErasedDropGlueBoundaryFact`、`DynAllocatorBoundaryFact` 和
`DynCleanupDropckBoundaryFact` 固定 future owning dyn、erased drop glue、allocator 与 cleanup/dropck boundary；
`m17_dyn_ownership_runtime_preparation_baseline()`、summary/dump/fingerprint 和 validation 会拒绝把 M17 伪装成已经
实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user value、runtime ABI lowering 或 dynamic Drop dispatch。M18 已进一步新增 `DynOwnershipRuntimeBoundaryGate`、`DynOwnershipRuntimeBoundaryCheckpointFact`、`DynOwnershipRuntimeLoweringDesignGateFact` 和 `dyn_ownership_runtime_boundary_gate` project-level query，把 M17 facts 接入 query/cache/tooling/reuse/workspace index，并固定 future IR/verifier/runtime lowering prerequisites；M18 仍不实现任何标准库或 runtime surface。M19 已新增 `DynOwnershipRuntimeIrVerifierFact`、`FunctionDynOwnershipRuntimeIrVerifierFacts`、`function_dyn_ownership_runtime_ir_verifier_facts()`、`TraitObjectVTableLayout::destructor_slot_blocked` 和 `CleanupAbiPolicy::dynamic_erased_drop_blocked` blocked negative sentinel，把 M18 prerequisites 落成 verifier-visible IR facts 和负例矩阵；M19 仍不实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user values、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。M20a 已新增 `OwnedDynRuntimeAdmissionGate`、`OwnedDynRuntimeAdmissionFact` 和 `m20_owned_dyn_runtime_admission_gate_baseline()`，把 M17/M18/M19 三层 facts 汇总为 owned dyn runtime 准入门禁，固定 owned layout、erased drop identity、allocator identity、runtime lowering ABI 和 `Box<dyn Trait>` surface 的后续顺序；M20a 仍不实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user values、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。M20b 已新增 `OwnedDynObjectLayoutPrototype`、`OwnedDynIrShapePrototypeGate` 和 `owned_dyn_ir_shape_prototype_gate()`，把 owned dyn handle 的 compiler-owned two-field IR prototype、dump/fingerprint/verifier 不变量和 query projection 落地；M20b 仍不实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user values、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。M20c 已新增 `OwnedDynDropAllocatorIdentityGate`、`OwnedDynDropAllocatorIdentityFact`、`m20c_owned_dyn_drop_allocator_identity_gate_baseline()` 和 `ir::owned_dyn_drop_allocator_identity_gate()`，并在 `OwnedDynObjectLayoutPrototype` 中固定 compiler-owned drop / allocator identity keys；M20c 仍不实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user values、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。M20d 已新增 `OwnedDynRuntimeLoweringAbiGate`、`OwnedDynRuntimeLoweringAbiFact`、`m20d_owned_dyn_runtime_lowering_abi_gate_baseline()` 和 `ir::owned_dyn_runtime_lowering_abi_gate()`，把 runtime ABI descriptor、blocked-to-admitted transition guard、backend helper prerequisite、drop/allocator runtime bridge 和 dynamic Drop blocker 固定成 query/IR facts；M20d 仍不实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user values、executable runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。M21a 已开启 macro system 主线，但只完成 design gate：`m21a_macro_design_gate_baseline()`、`is_valid_m21a_macro_design_gate()`、`macro_design_gate_fingerprint()`、summary/dump 和 query tests 固定 token tree / attribute surface、hygiene、source map、debug trace、declared generated names、query-backed incremental expansion 和 attached item codegen 路线；M21b 已新增 `AttributeDecl`、`AttributeTokenDecl` 和 `ItemNode::attributes`，把通用 item attribute token-tree surface 落到 frontend parser/AST/dump，并保持 `#[derive(Copy, Eq, Hash)]` 兼容；M21c 已新增 `MacroExpansionPlan`、`MacroExpansionFact`、`m21c_macro_expansion_plan_baseline()`、summary/dump/fingerprint 和 validation，固定 early item expansion query key、`SourceRole::generated` / `ModulePartKind::generated` no-op generated part、source-map stub、builtin derive passthrough 和 unimplemented attribute blocker；M21d 已把该 plan 接到真实 frontend pipeline，在 module loading / AST combine 之后、sema 之前运行 `macro.expand_items`，产出 `EarlyItemExpansionResult`、attribute query-key fingerprint、generated part placeholder 和 source-map placeholder；M21e 已新增 `GeneratedModulePartParseMergeStub` 和 `GeneratedModulePartLifecycleState`，为每个 generated placeholder 固定 deterministic `generated_buffer_identity`、`parse_config_fingerprint`、`merge_ordering_key`、expansion origin、buffer name 和 `merge_blocked` lifecycle stub，但仍不修改 AST、不 parse / merge generated module part、不执行 external procedural macro、不生成用户代码；M21f 已新增 `ExpansionHygieneStub` 和 `ExpansionTraceStub`，为每个 macro input 固定 deterministic `call_site_mark`、`definition_site_mark`、`generated_fresh_mark`、`declared_name_set`、`trace_identity`、`generated_source_map_identity`、`diagnostic_anchor`、`origin_mark_hygiene_v1` 和 `expansion_source_map_debug_trace_v1`，并继续拒绝真实 hygiene resolution、真实 source map、debug trace CLI、`--emit-expanded` 和 macro-generated user code；typed expression macro 和 external procedural macro 继续后移，标准库、runtime helper、文本替换宏和真实用户宏展开仍不实现。

M21g 已新增 `GeneratedItemDeclarationStub` 和 `DeclaredGeneratedNameStub`，把 generated item declaration、
declared generated name、`attached_item_codegen_declared_names_v1`、`macro_declared_name_fact` 和 M21e/M21f
generated part / hygiene facts 的绑定落成结构化 no-op contract。M21h 已新增
`TokenMaterializationAdmissionStub` 和 `GeneratedTokenBufferStub`，把
`compiler_owned_attached_item_token_materialization_admission_v1`、`compiler_owned_empty_token_stream`、
`macro_token_materialization_fact` 和 `macro_generated_token_buffer_fact` 的 no-op stub contract 接到同一
`macro.expand_items` 边界。M21i 已新增 `GeneratedTokenRecord` 和 generated token buffer
`materialization_identity` / `token_producer_policy`，为内建 `derive` 输入生成 compiler-owned
`compiler_owned_builtin_derive_token_stream_prototype` token buffer prototype，并用
`compiler_owned_builtin_derive_token_producer_prototype_v1` 和 `derive_source_token_placeholder` 固定第一版
parser-blocked generated token record facts；非 `derive` item attribute 仍保持 empty generated token buffer。M21j
已新增 `GeneratedTokenParserAdmissionGateStub` 和 `parser_admission_gates`，把 generated token buffer 是否允许进入
parser 从 token materialization 层单独拆出来，并绑定 M21e `generated_buffer_identity` /
`parse_config_fingerprint`、M21i token buffer identity / materialization identity、source-map identity 和 hygiene mark。
当前 gate 固定为 `compiler_owned_generated_token_parser_admission_gate_v1`，`derive` gate 可记录
`token_records_available=true`，但所有 gate 仍保持 `parser_admitted=false`、`parse_ready=false`、
`parser_consumable=false`、`generated_part_parsed=false`、`generated_part_merged=false` 和 `sema_visible=false`。
M21k 已新增 `ParserAdmissionDiagnosticProjectionStub` 和 `parser_admission_diagnostics`，把 M21j parser
admission gate 转成稳定 diagnostic / dump projection，并固定
`parser_admission_blocked_diagnostic_projection_v1`、`diagnostic_identity`、`diagnostic_anchor_identity`、
`empty_token_buffer_parser_admission_blocked`、`derive_token_buffer_parser_admission_blocked`、source anchor、
token-tree anchor、token buffer admission blocker、generated module part parse blocker、debug projection name 和
future `--emit-expanded` / debug trace / source-map projection unavailable states。M21k 仍保持
`parser_admitted=false`、`parse_ready=false`、`parser_consumable=false`、`generated_part_parsed=false`、
`generated_part_merged=false`、`emit_expanded_available=false`、`debug_trace_available=false`、
`source_map_available=false` 和 `produced_user_generated_code=false`。typed expression macro 和 external procedural
macro 继续后移，标准库、runtime helper、文本替换宏、parser-consumable generated token buffer、真实 generated
source text 和真实用户宏展开仍不实现。M21l 已新增 `ParserAdmissionDiagnosticReportEntry`、
`ParserAdmissionDiagnosticReport`、`parser_admission_report_entries` 和 `parser_admission_reports`，把 M21k 的
per-input diagnostic projection 汇总为 generated module part 级 report/query projection，并固定
`parser_admission_blocked_report_query_projection_v1`、`report_entry_identity`、`report_identity`、
`report_anchor_identity`、`report_grouping_identity`、`m21l-parser-admission-report:<module>:<part>` query name、
category totals、source anchor ordering 和 report-level blocked flags。M21l 仍保持 `parser_admitted=false`、
`parse_ready=false`、`parser_consumable=false`、`emit_expanded_available=false`、
`debug_trace_available=false`、`source_map_available=false` 和 `produced_user_generated_code=false`。M21m 已新增
`GeneratedTokenParserReadinessPreflightEntry` 和 `parser_readiness_preflight_entries`，把每个 generated token
buffer 的 token stream shape、token index continuity、delimiter balance、source-anchor coverage、parse config
compatibility、hygiene/source-map prerequisites 和 diagnostic projection availability 固定为 parser consumption
之前的 preflight facts。M21n 已新增 `GeneratedTokenParserConsumptionContractGate` 和
`parser_consumption_contract_gates`，按 generated module part 汇总 preflight entries，并固定
`generated_token_parser_consumption_contract_gate_v1`、`contract_identity`、`contract_grouping_identity`、
`contract_anchor_identity` 和 `m21n-parser-consumption-contract:<module>:<part>` query name。M21o 已新增
`MacroExpansionBoundaryClosureReport` 和 `macro_boundary_closure_reports`，把 M21m/M21n 汇总成
`m21_macro_expansion_boundary_release_closure_v1` closure report，并把 result name 推进为
`M21o Macro Expansion Boundary Release Closure`。M21o 仍保持 parser consumption、generated source text、
generated module part parse/merge、AST mutation、emit-expanded、debug/source-map projection、标准库、runtime helper
和 macro-generated user code 全部关闭。M22a 已新增 `BuiltinDeriveExpansionAdmissionGate` 和
`builtin_derive_expansion_admissions`，为每个 macro input 固定
`builtin_derive_expansion_admission_gate_v1`、`admission_identity`、
`m22a-builtin-derive-admission:<module>:<part>:<item>:<attr>:<name>` query name、derive/non-derive admission kind
以及 capability candidate 计数；M22b 已新增 `BuiltinDeriveSemanticExpansionPlan` 和
`builtin_derive_semantic_plans`，把现有内建 `#[derive(Copy, Eq, Hash)]` capability path 投影为
`builtin_derive_semantic_expansion_plan_v1`、`capability_fact_lowering_plan`、`capability_set_identity` 和
`semantic_plan_identity`；M22c 已新增 `BuiltinDeriveParserConsumptionReleaseGate` 和
`builtin_derive_parser_release_gates`，按 generated part 汇总 admission/semantic plan，固定
`builtin_derive_parser_consumption_release_gate_v1`、`m22c-builtin-derive-parser-release:<module>:<part>`、
rollback diagnostics/debug trace/source-map/hygiene prerequisites，并把 result name 推进为
`M22c Builtin Derive Parser Consumption Release Gate`。M22d 已新增
`BuiltinDeriveReleaseHardeningMatrix` 和 `builtin_derive_release_hardening_matrices`，固定
`builtin_derive_release_hardening_matrix_v1`、`m22d-builtin-derive-release-hardening:<module>:<part>`、part-local /
cross-part admission/semantic-plan totals、multi-item negative matrix 和 release-blocked 状态；M22e 已新增
`BuiltinDeriveDebugDumpStabilityContract` 和 `builtin_derive_debug_dump_contracts`，固定
`builtin_derive_debug_dump_stability_contract_v1`、`m22e-builtin-derive-debug-dump:<module>:<part>`、dump section
count、stable ordering、identity projection、summary projection 和 drift-debuggable contract；M22f 已新增
`BuiltinDeriveRollbackDiagnosticDesignGate` 和 `builtin_derive_rollback_diagnostic_gates`，固定
`builtin_derive_rollback_diagnostic_design_gate_v1`、`m22f-builtin-derive-rollback-diagnostic:<module>:<part>`、
M21n/M22c/M22d/M22e identity 链接、diagnostic/report totals、source/token-tree anchor prerequisite 和 design-only
rollback gate，并把 result name 推进为 `M22f Builtin Derive Rollback Diagnostic Design Gate`。M22a-M22f 仍保持
parser consumption、generated source text、generated module part parse/merge、AST mutation、emit-expanded、
debug/source-map projection、标准库、runtime helper、external procedural macro 和 macro-generated user code 全部关闭。
M23a 已新增 `BuiltinDeriveParserConsumptionAdmissionProtocol` 和
`builtin_derive_parser_consumption_admission_protocols`，固定
`builtin_derive_parser_consumption_admission_protocol_v1`、
`m23a-builtin-derive-parser-consumption-admission:<module>:<part>`、M21n/M22c/M22f identity 链接、token buffer /
token record / derive candidate / empty candidate / blocked diagnostic counts 和 no-parser-consumption admission
blocker；M23b 已新增 `BuiltinDeriveParserConsumptionCheckpointRollbackProtocol` 和
`builtin_derive_checkpoint_rollback_protocols`，固定
`builtin_derive_parser_checkpoint_rollback_protocol_v1`、
`m23b-builtin-derive-checkpoint-rollback:<module>:<part>`、`checkpoint_count=3`、`rollback_plan_count=3`、
diagnostic replay prerequisite 和 parser-blocked rollback protocol；M23c 已新增
`BuiltinDeriveParserPreConsumptionVerificationClosure` 和
`builtin_derive_preconsumption_verification_closures`，固定
`builtin_derive_parser_preconsumption_verification_closure_v1`、
`m23c-builtin-derive-preconsumption-verification:<module>:<part>`、M23a/M23b/M22d/M22e/M22f 可见性闭环，并把
result name 推进为 `M23c Builtin Derive Parser Pre-Consumption Verification Closure`。M23a-M23c 仍保持 parser
consumption、generated source text、generated module part parse/merge、AST mutation、sema-visible generated part、
emit-expanded、debug/source-map projection、标准库、runtime helper、external procedural macro、用户自定义 macro 和
macro-generated user code 全部关闭。
M24a 已新增 `BuiltinDeriveControlledParserDryRunAdapter` 和
`builtin_derive_controlled_dry_run_adapters`，固定
`builtin_derive_controlled_parser_dry_run_adapter_v1`、
`m24a-builtin-derive-controlled-parser-dry-run:<module>:<part>`、M23c/M23a/M23b identity 链接、token record /
diagnostic anchor counts、`prerequisite_count=5` 和 execution-blocked dry-run adapter；M24b 已新增
`BuiltinDeriveDryRunRollbackDiagnosticReplay` 和 `builtin_derive_dry_run_rollback_replays`，固定
`builtin_derive_dry_run_rollback_diagnostic_replay_v1`、
`m24b-builtin-derive-dry-run-rollback-replay:<module>:<part>`、M24a/M23b/M22f identity 链接、planned replay /
executed replay counts 和 replay execution blocker；M24c 已新增 `BuiltinDeriveDryRunNegativeMatrixClosure` 和
`builtin_derive_dry_run_negative_matrices`，固定
`builtin_derive_dry_run_negative_matrix_closure_v1`、
`m24c-builtin-derive-dry-run-negative-matrix:<module>:<part>`、M24a/M24b/M23c 可见性闭环、
`negative_case_count=8`、`parser_consumable_case_count=0`，并把 result name 推进为
`M24c Builtin Derive Dry-Run Negative Matrix Closure`。M24a-M24c 仍保持 dry-run execution、parser consumption、
generated source text、generated module part parse/merge、AST mutation、sema-visible generated part、
emit-expanded、debug/source-map projection、标准库、runtime helper、external procedural macro、用户自定义 macro 和
macro-generated user code 全部关闭。M25a 已新增 `BuiltinDeriveParserDryRunSessionBoundary` 和
`builtin_derive_parser_dry_run_sessions`，固定
`builtin_derive_parser_dry_run_session_boundary_v1`、
`m25a-builtin-derive-dry-run-session:<module>:<part>`、M24a/M24c/M21e identity 链接、compiler-owned token stream
candidate、parser state snapshot、`committed_parse_count=0` 和 check-only uncommitted session blocker；M25b 已新增
`BuiltinDeriveTokenCursorSnapshotRollbackProof` 和 `builtin_derive_token_cursor_snapshot_proofs`，固定
`builtin_derive_token_cursor_snapshot_rollback_proof_v1`、
`m25b-builtin-derive-token-cursor-rollback-proof:<module>:<part>`、M25a/M23b/M24b identity 链接、cursor snapshot /
parser state snapshot / rollback proof counts 和 `cursor_commit_count=0`；M25c 已新增
`BuiltinDeriveDiagnosticShadowNoAstMutationClosure` 和
`builtin_derive_diagnostic_shadow_no_ast_mutation_closures`，固定
`builtin_derive_diagnostic_shadow_no_ast_mutation_closure_v1`、
`m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:<module>:<part>`、M25a/M25b/M24b/M24c 可见性闭环、
diagnostic shadow count、`executed_shadow_count=0`、`ast_mutation_count=0`、`parser_consumable_case_count=0`，并把
result name 推进为 `M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure`。M25a-M25c 仍保持 real parser
dry-run、dry-run execution、diagnostic shadow execution、rollback execution、parser cursor advance、parser
consumption、generated source text、generated module part parse/merge、AST mutation、sema-visible generated part、
emit-expanded、debug/source-map projection、标准库、runtime helper、external procedural macro、用户自定义 macro 和
macro-generated user code 全部关闭。M26a 已新增 `BuiltinDeriveParserDryRunAdmissionGate` 和
`builtin_derive_parser_dry_run_admission_gates`，固定
`builtin_derive_parser_dry_run_admission_gate_v1`、
`m26a-builtin-derive-parser-dry-run-admission:<module>:<part>`、M25a/M25b/M25c/M21e identity 链接、
`admission_prerequisite_count=5`、token buffer / token record counts、`dry_run_execution_admitted_count=0` 和
`builtin derive parser dry-run execution admission remains blocked in M26a` blocker；M26b 已新增
`BuiltinDeriveErrorRecoveryShadowDiagnosticGate` 和
`builtin_derive_error_recovery_shadow_diagnostic_gates`，固定
`builtin_derive_error_recovery_shadow_diagnostic_gate_v1`、
`m26b-builtin-derive-error-recovery-shadow-diagnostic:<module>:<part>`、M26a/M25c/M24b/M21l identity 链接、
diagnostic shadow / parser report / planned recovery counts、`executed_recovery_count=0` 和
`emitted_diagnostic_count=0`、`builtin derive error recovery shadow diagnostics remain non-emitting in M26b`
blocker；M26c 已新增
`BuiltinDeriveCursorRollbackAstMutationVerifierClosure` 和
`builtin_derive_cursor_rollback_ast_mutation_verifier_closures`，固定
`builtin_derive_cursor_rollback_ast_mutation_verifier_closure_v1`、
`m26c-builtin-derive-cursor-rollback-ast-verifier:<module>:<part>`、M26a/M26b/M25a/M25b/M25c identity 链接、
`ast_baseline_snapshot_count=1`、`ast_mutation_count=0`、`cursor_commit_count=0`、`session_commit_count=0`、
`parser_consumable_case_count=0`、`builtin derive cursor rollback execution and AST mutation verifier remain check-only in M26c`
blocker，并把 result name 推进为
`M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure`。M26a-M26c 仍保持 real parser dry-run、
dry-run execution admission/execution、error recovery execution、diagnostic emission、rollback execution、
parser cursor advance、session commit、parser consumption、generated source text、generated module part
parse/merge、AST mutation、sema-visible generated part、emit-expanded、debug/source-map projection、标准库、
runtime helper、external procedural macro、用户自定义 macro 和 macro-generated user code 全部关闭。

M27 已新增 Aurex 自己风格的宏声明表面 admission：parser / AST 当前可识别 `macro Name { ... }`、
`macro derive Name { ... }` 和 `macro const Name { ... }`，记录 `ItemKind::macro_decl`、
`MacroDeclKind::{declarative, derive, compile_time}`、宏体 token tree、`match` clause 计数和 delimiter balance；
`expand_early_item_macros_noop()` 会为每个 macro item 生成 `AurexMacroSurfaceAdmissionGate`，并在 summary /
dump / fingerprint 中记录 `aurex_macro_surface_source_items`、`aurex_macro_surface_admissions`、
`aurex_macro_declarative_surfaces`、`aurex_macro_user_derive_surfaces` 和
`aurex_macro_compile_time_surfaces`。Query 层新增
`m27_macro_expansion_plan_baseline()`、`is_valid_m27_macro_expansion_plan()`、`aurex_declarative_macro_surface`、
`aurex_user_derive_macro_surface` 和 `aurex_compile_time_macro_execution_admission`。M27 明确不采用
`macro_rules!` 或 `$matcher` 写法，仍不展开宏、不执行用户编译期代码、不 parse / merge generated module part、不修改
AST、不生成 sema-visible item、不生成用户代码、不需要标准库/runtime/external process。M27b 已在该表面上新增
`AurexMacroDefinitionSiteHygieneAdmissionGate`、`AurexMacroTypedMatcherAdmissionGate`、
`aurex_macro_definition_site_hygiene_gates` 和 `aurex_macro_typed_matcher_admission_gates`，当前能识别顶层
`match expr_list(xs) -> { xs }`、`match item(target) -> { target }` 和
`match tokens(input) -> { input }`，并把 `aurex_macro_typed_matcher_admissions`、
`aurex_macro_definition_site_hygiene_gates`、definition-site mark、fresh name scope、diagnostic anchor、matcher
fingerprint 和 matcher identity 固定为可验证 facts。Query 层新增
`m27b_macro_expansion_plan_baseline()`、`is_valid_m27b_macro_expansion_plan()`、
`aurex_macro_typed_matcher_admission`、`aurex_macro_definition_site_hygiene_admission` 和
`aurex_macro_debuggable_diagnostic_anchor`。typed matcher execution is admission-only in M27b；definition-site
hygiene resolution is admission-only in M27b；仍不展开宏/不执行用户编译期代码/不消费 parser/不修改 AST。

M27c 已新增 item-level macro call-site 和 user derive target schema admission：parser / AST 当前可识别
`macro call Name { ... }`，记录 `ItemKind::macro_call`、调用点 token tree 和 delimiter balance；
`expand_early_item_macros_noop()` 会生成 `AurexMacroCallSiteAdmissionGate`，并在同模块同名 macro surface 存在时生成
`AurexMacroMatcherToCallBindingAdmissionGate`。`#[derive(Name)]` 匹配同模块 `macro derive Name { ... }` 时，会生成
`AurexUserDeriveTargetSchemaAdmissionGate`，记录目标 `struct` / `enum` schema、字段数、enum case 数和 enum payload
数。Summary / dump / fingerprint 记录 `aurex_macro_call_site_source_items`、
`aurex_macro_call_site_admissions`、`aurex_macro_matcher_to_call_bindings`、
`aurex_user_derive_target_schema_source_derives` 和 `aurex_user_derive_target_schemas`。Query 层新增
`m27c_macro_expansion_plan_baseline()`、`is_valid_m27c_macro_expansion_plan()`、
`aurex_macro_call_site_admission`、`aurex_macro_matcher_to_call_binding_admission` 和
`aurex_user_derive_target_schema_admission`。macro call-site expansion is admission-only in M27c；
matcher-to-call binding execution is admission-only in M27c；user derive target schema is admission-only in M27c；
仍不展开宏/不执行用户编译期代码/不消费 parser/不修改 AST。

M27d 已新增 macro output contract admission：`expand_early_item_macros_noop()` 会为每个
`AurexMacroMatcherToCallBindingAdmissionGate` 和 `AurexUserDeriveTargetSchemaAdmissionGate` 生成
`AurexMacroOutputContractAdmissionGate`，并同步生成 `AurexMacroOutputDeclaredNamePolicyAdmissionGate` 与
`AurexMacroOutputDiagnosticProjectionAdmissionGate`。Summary / dump / fingerprint 记录
`aurex_macro_output_contracts`、`aurex_macro_output_declared_name_policies` 和
`aurex_macro_output_diagnostic_projections`；Query 层新增 `m27d_macro_expansion_plan_baseline()`、
`is_valid_m27d_macro_expansion_plan()`、`aurex_macro_output_contract_admission`、
`aurex_macro_output_declared_name_policy_admission` 和
`aurex_macro_output_diagnostic_projection_admission`。macro output parser consumption remains blocked in M27d；
macro output declared names are hidden from lookup/export/sema in M27d；macro output diagnostics are projected but
parser emission remains blocked in M27d；仍不展开宏/不执行用户编译期代码/不消费 parser/不修改 AST。

本目录提供中文文档集。文档按主题组织，不再按 `0.1.0`、`0.1.1` 等小版本拆分零散变更说明。

- [介绍文档](introduction.md)
- [需求分析文档](requirements.md)
- [架构设计文档](architecture.md)
- [运行流程文档](runtime-flow.md)
- [API 接口文档](api.md)
- [Aurex 语言参考手册](language-manual.md)
- [Aurex 正则库语法和实现说明](regex.md)
- [Aurex 正则库工业化现状报告](regex-industrial-report.md)
- [字符串基础类型设计草案](string-primitive-design.md)
- [Aurex 当前语法与特性清单](language-feature-inventory.md)
- [Aurex 基础语法地基评估](basic-syntax-foundation-review.md)
- [Aurex 语法现状与设计评估](language-syntax-and-design-review.md)
- [Aurex M27c 语法易用性与审美问题清单](m27c-syntax-ergonomics-review.md)
- [类、trait 与组合背景说明](class-trait-composition-background.md)
- [Aurex M3 模块系统设计稿](aurex-module-system-m3-design.md)
- [Aurex M3 模块系统使用案例](aurex-module-system-m3-example.md)
- [Aurex 模块系统 V2 设计草案](module-system-v2-design.md)
- [设计实现文档](implementation.md)
- [M2.5 收尾重构详细设计](m2.5-refactor-cleanup-design.md)
- [当前进度文档](progress.md)
- [M2.5 路线图](m2.5-roadmap.md)
- [M2.5 Query Key 设计](m2.5-query-key-design.md)
- [M3 路线图](m3-roadmap.md)
- [M4 Trait / Protocol 系统路线图](m4-roadmap.md)
- [Aurex M4-WP1 Trait / Protocol 系统调研与设计基线](m4-trait-protocol-system-design.md)
- [Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md)
- [M5 Default Trait Methods 路线图](m5-roadmap.md)
- [Aurex M5 Default Trait Methods 调研与设计基线](m5-default-trait-methods-design.md)
- [Aurex M5 Default Trait Methods Release Baseline](m5-release-baseline.md)
- [M6 资源、值生命周期与访问语义路线图](m6-roadmap.md)
- [Aurex M6 资源、值生命周期与访问语义调研和三轮设计审视基线](m6-resource-access-semantics-design.md)
- [Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)
- [Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图](m7-roadmap.md)
- [Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线](m7b-borrow-contract-design.md)
- [Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 路线图](m7b-roadmap.md)
- [Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)
- [M7 Hardening Performance Closure](m7-hardening-performance-closure.md)
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
- [Aurex M21a Macro System Design Gate](m21-macro-system-design-gate.md)
- [Aurex M21b AttributeDecl / Token Tree Surface](m21b-attribute-token-tree-surface.md)
- [Aurex M21c Early Item Macro Expansion Plan](m21c-early-item-macro-expansion-plan.md)
- [Aurex M21d No-op Early Item Macro Expansion Boundary](m21d-noop-early-item-expansion-boundary.md)
- [Aurex M21e Generated Module Part Parse/Merge Stub Contract](m21e-generated-module-part-parse-merge-stub-contract.md)
- [Aurex M21f Hygiene Source Map Debug Trace Stub Contract](m21f-hygiene-source-map-debug-trace-stub-contract.md)
- [Aurex M21g Generated Item Declared Names Stub Contract](m21g-generated-item-declared-names-stub-contract.md)
- [Aurex M21h Token Materialization Admission Stub Contract](m21h-token-materialization-admission-stub-contract.md)
- [Aurex M21i Compiler-Owned Generated Token Buffer Prototype](m21i-compiler-owned-generated-token-buffer-prototype.md)
- [Aurex M21j Generated Token Parser Admission Gate](m21j-generated-token-parser-admission-gate.md)
- [Aurex M21k Parser Admission Diagnostic Projection Gate](m21k-parser-admission-diagnostic-projection-gate.md)
- [Aurex M21l Parser Admission Diagnostic Report Projection](m21l-parser-admission-diagnostic-report-projection.md)
- [Aurex M21m Generated Token Parser Consumption Readiness Preflight](m21m-generated-token-parser-readiness-preflight.md)
- [Aurex M21n Parser Consumption Contract Gate](m21n-parser-consumption-contract-gate.md)
- [Aurex M21o Macro Expansion Boundary Release Closure](m21o-macro-expansion-boundary-release-closure.md)
- [Aurex M22a Builtin Derive Expansion Admission Gate](m22a-builtin-derive-expansion-admission-gate.md)
- [Aurex M22b Builtin Derive Semantic Expansion Plan](m22b-builtin-derive-semantic-expansion-plan.md)
- [Aurex M22c Builtin Derive Parser Consumption Release Gate](m22c-builtin-derive-parser-consumption-release-gate.md)
- [Aurex M22d Builtin Derive Release Hardening Matrix](m22d-builtin-derive-release-hardening-matrix.md)
- [Aurex M22e Builtin Derive Debug Dump Stability Contract](m22e-builtin-derive-debug-dump-stability-contract.md)
- [Aurex M22f Builtin Derive Rollback Diagnostic Design Gate](m22f-builtin-derive-rollback-diagnostic-design-gate.md)
- [Aurex M23a Builtin Derive Parser Consumption Admission Protocol](m23a-builtin-derive-parser-consumption-admission-protocol.md)
- [Aurex M23b Builtin Derive Checkpoint Rollback Protocol](m23b-builtin-derive-checkpoint-rollback-protocol.md)
- [Aurex M23c Builtin Derive Parser Pre-Consumption Verification Closure](m23c-builtin-derive-preconsumption-verification-closure.md)
- [Aurex M24a Builtin Derive Controlled Parser Dry-Run Adapter](m24a-builtin-derive-controlled-parser-dry-run-adapter.md)
- [Aurex M24b Builtin Derive Dry-Run Rollback Diagnostic Replay](m24b-builtin-derive-dry-run-rollback-diagnostic-replay.md)
- [Aurex M24c Builtin Derive Dry-Run Negative Matrix Closure](m24c-builtin-derive-dry-run-negative-matrix-closure.md)
- [Aurex M25a Builtin Derive Parser Dry-Run Session Boundary](m25a-builtin-derive-parser-dry-run-session-boundary.md)
- [Aurex M25b Builtin Derive Token Cursor Snapshot Rollback Proof](m25b-builtin-derive-token-cursor-snapshot-rollback-proof.md)
- [Aurex M25c Builtin Derive Diagnostic Shadow No-AST-Mutation Closure](m25c-builtin-derive-diagnostic-shadow-no-ast-mutation-closure.md)
- [Aurex M26a Builtin Derive Parser Dry-Run Admission Gate](m26a-builtin-derive-parser-dry-run-admission-gate.md)
- [Aurex M26b Builtin Derive Error Recovery Shadow Diagnostic Gate](m26b-builtin-derive-error-recovery-shadow-diagnostic-gate.md)
- [Aurex M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure](m26c-builtin-derive-cursor-rollback-ast-mutation-verifier-closure.md)
- [Aurex M27 Aurex Macro Surface Admission](m27-aurex-macro-surface-admission.md)
- [Aurex M7 Origin/Loan/Lifetime 设计三轮评审](../review/aurex_m7_design_three_round_review.md)
- [使用文档](usage.md)
- [版本文档](version.md)
- [下一步计划文档](next-steps.md)
