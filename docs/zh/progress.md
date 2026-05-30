# 当前进度文档

版本：0.1.4
阶段：M4-WP4 coherence / generic predicates 已收口；下一步 M4-WP5 trait method resolution / lowering

## 总体状态

2026-05-31：M4 trait/protocol 系统已完成 WP1、WP2、WP3 和 WP4。M4-WP1 完成调研与设计基线，正式选择
nominal static trait：语言关键字为 `trait`，`protocol` 只作为行为契约的设计术语；conformance 由显式
`impl Trait for Type` 给出；泛型约束进入 canonical trait predicate / `ParamEnv`；调用默认静态分派，单态化后降低为具体
impl method direct call。M4-WP2 已把 `trait` / `impl Trait for Type` 的 token、parser、AST payload、AST
dump、lossless syntax 和 query identity scaffold 落到可回归基线。M4-WP3 已把 trait declaration 和 impl registry
接入 query-backed sema aggregate：`CheckedModule::traits` 记录 `TraitSignature`、generic params、visibility
和结构化 requirement；trait requirement prototype 不再作为普通 top-level function/prototype 校验；`CheckedModule::trait_impls`
记录 `impl Trait for Type` fact；sema 已校验缺方法、重复 impl method、未知 impl method、签名不匹配、trait
不可见、trait target 非命名 trait、self target 非命名类型、trait generic arity 和重复 exact impl key。M4-WP4 已在
`CheckedModule` 中新增 `TraitPredicate`、`TraitObligation`、`TraitEvidence` 和 `ParamEnvInfo`，把
`where T: Trait` 降低为正式 predicate；`Sized`、`Eq`、`Ord`、`Hash` 保持旧 capability 检查并同步记录
compiler-owned builtin trait predicate；generic instantiation 已按 ParamEnv 做 user trait candidate rejection；
trait impl registry 已增加 canonical coherence fingerprint、orphan rule 和 first-pass overlap check。

M4-WP3/WP4 的测试已落到常规仓库测试，而不是临时目录：`tests/gtest/sema/trait_tests.cpp` 覆盖白盒 checked facts、dump
和正负样例，`tests/samples/positive/traits/trait_impl_registry.ax` 覆盖正样例，`tests/samples/negative/traits/*.ax`
覆盖诊断路径，`tests/samples/positive/traits/trait_predicate_where_generic.ax`、
`tests/samples/negative/traits/trait_predicate_unsatisfied_generic_arg.ax` 和
`tests/samples/negative/traits/trait_impl_orphan_external.ax` 覆盖 WP4 predicate/candidate/orphan 路径，
`tests/samples/imports/samplelib/traits.ax` 覆盖跨模块可见性。完整设计见
[Aurex M4-WP1 Trait / Protocol 系统调研与设计基线](m4-trait-protocol-system-design.md)，阶段路线见
[M4 Trait / Protocol 系统路线图](m4-roadmap.md)。下一步是 M4-WP5：Static Method Resolution And Lowering。

当前仍未把 WP4 误扩成完整 trait 系统：trait method call resolution、lowering/backend direct call、associated
types、dynamic trait object 和 RAII/resource semantics 仍分别由 WP5、WP6 或后续资源设计承接。WP4 的
`where` grammar 仍只支持单个 identifier predicate 名称；qualified where predicate、generic trait predicate
arguments 和 associated type constraints 后续再进入 solver/associated type 阶段。

当前仓库已经从 M2 language-core-no-std 基线进入 M2.5 frontend-foundation 阶段。M2 的目标不是继续修补 M1，而是重新收口语言核心：冻结并删除标准库和 M1 系统样例，把注意力放回基础语法、类型系统、模式匹配、`unsafe` 边界、IR 和 LLVM 后端。M2.5 建立在这条已收口主线之上，开始处理 query 化、lossless syntax 和 IDE-native 前端所需的结构化地基。

2026-05-25 R5 Compilation Pipeline / Driver Action core 已完成。M2.5 前端地基和收尾拆分完成后，重构主线已完成现代编译器 driver/session/pipeline 边界：`CompilerInvocation` 保持纯配置，`Compiler` 退回 public facade，一次编译的 source、diagnostics、profile、cache policy 和 backend emitter 由内部 `CompilationSession` 持有，source/frontend/sema/cache/lowering/backend 阶段由 `CompilationPipeline` 显式编排。R5.2 已把 source/token/lossless/module graph/AST dump/sema/cache write 收口到内部 `FrontendPipeline`；R5.3 继续把 checked dump、IR lowering、IR pass pipeline 和 IR dump 收口到 `LoweringPipeline`，把 LLVM IR emission、LLVM IR dump、temporary LLVM file、clang native invocation 和 native 输出路径校验收口到 `BackendPipeline`，并用 `PipelineStage` 固定 driver 主阶段的 profile/input/output/diagnostic/cache-query 契约；R5.4 已建立轻量 IR pass manager、`PassResult`、`PreservedAnalyses` 和 verifier gate；R5.5 已建立 `ModuleAnalysisManager`，惰性缓存 CFG、dominance 和 value-use analysis，并按 `PreservedAnalyses` 自动失效；R5.6 已给 IR verifier gate failure 接入稳定 `stage/profile/verifier/pass` 上下文，同时保持原始 verifier body 和 `ErrorCode` 不变，并让 `LoweringPipeline` 从 `PipelineStage` record 传递 IR pass pipeline 阶段名；R5.7 已让 profile JSON 为 driver 主阶段 phase 输出来自 `PipelineStageRecord` 的 `stage` 元数据对象；R5.8 已让 incremental-cache profile 子事件通过 `parent_stage` 挂回 `incremental_cache.lookup` 或 `incremental_cache.write`；R5.9 已让 `DiagnosticCategory` 通过 `PipelineStage` 反查候选 owner stage，lexer 诊断保留 `tokens.lex` / `module.lex` 双归属，sema 类诊断归属 `sema.analyze`，且 diagnostics text/JSON 协议保持不变；R5.10 已让 `aurex_tooling` 的 `IdeDiagnostic.owner_stages` 消费同一份 `PipelineStageRecord`，供后续 LSP/IDE 阶段视图直接复用；R5.11 已把阶段目录头文件提升为公开只读 API，并用 `PipelineStageMetadata` 统一 profile writer 和 tooling diagnostics 的 metadata 形状；R5.12 已让 profile 记录入口直接接收 `PipelineStageId` / `PipelineProfileSubeventId`，调用点不再散落阶段 profile name 字符串；R5.13 已增加 `pipeline_profile_phase_classification(...)`，profile JSON writer 和后续 viewer/LSP adapter 可以通过同一入口区分 driver 主阶段、profile 子事件和 unknown。M3 后续实现继续复用 R5 稳定下来的 driver/session/query/diagnostics/pipeline 主路径。

2026-05-28 M3.0 Phase 9A-D 已完成模块系统收口：文档已把 M3.0 contract matrix 固定为
ModuleKey/ModulePartKey、part root、package visibility、source-root topology、IDE source-part context、
selective re-export 和 query/cache 边界；IDE/tooling 对真实 `.parts/<name>.ax` buffer 可以在 owning primary
存在且显式列出该 part 时恢复 resolved `ModulePartKey`，无法证明 ownership 时继续保持 unresolved；primary-level
`pub use module.Item [as Alias]` / `pub(package) use ...` 已作为 selective item re-export 进入 parser、AST、
loader、sema、ModuleGraph、ModuleExports 和 ModulePackageExports。M3.0 仍明确拒绝 glob import/use、
part-local `pub use`、bare/private use、nested module tree、`pub(in path)`、file-private、workspace/dependency
resolver、lockfile、version solving 和 package manager。

2026-05-28 M3.1 Generics Completion 已完成 release baseline：`GenericTemplateSignature`、
`GenericInstanceSignature` 和 `GenericInstanceBody` 成为泛型权威边界；generic ABI suffix、stable id 和
incremental key 来自 `GenericInstanceKey` / canonical type identity；generic body、IR lowering、
LLVM lowering 和 native execution 消费同一份实例身份与 side table 视图；generic builtin type operand
和 value-only builtin 在 generic function 中完成 sema/IR/LLVM 闭环；method-local generics 已进入
ABI/query/diagnostics/lowering/native 路径。2026-05-29 `m3.1` 已 fast-forward 合并回 `m3`，随后
`m3.2` 完成 Query-backed Sema 设计和实现。M3.2 已把 sema 从“单次 eager analyzer 产生 checked module”
推进为 query-backed semantic authority：`ItemSignature`、`BodySyntax`、
`TypeCheckBody`、`GenericTemplateSignature`、`GenericInstanceSignature` 和 `GenericInstanceBody`
形成统一 authority 边界，`CheckedModule` 分清 durable facts、session-local caches 和 lowering-only side
tables，incremental cache / query pruning / provider-skip replay 能解释 sema 级结果复用。M3.2 执行记录已收束为
[Aurex M3.2 Query-backed Sema 设计与执行计划](m3.2-query-backed-sema-plan.md)，后续新专题应进入新的 M3.3 /
LSP adapter / 更细粒度 incremental sema 计划，不再向 M3.2 追加新范围。

2026-05-29 M3.2 WP-1/2/3 Query-backed Sema authority batch 已完成：非泛型
`ItemSignature`、`FunctionBodySyntax` 和 `TypeCheckBody` 已补齐到 M3.1 泛型 authority 的同级边界。
`ItemSignatureAuthority` 显式记录 signature incremental key、`ModulePartKey`、namespace、`DefKind`、
visibility rank、value/generic 参数数量、return/receiver/unsafe/variadic/definition flags；
`FunctionBodySyntaxAuthority` 记录 body syntax fingerprint、owner `DefKey`、`ModulePartKey`、body source
range、body slot 和 ordinal；`TypeCheckBodyAuthority` 记录 checked body fingerprint、body syntax result、
item signature result、side-table summary、coercion count、retained-side-table flag 和 diagnostics flag。
provider input 不再接收非泛型 item/body 的裸 `IncrementalKey` 或裸 body fingerprint，provider 默认实现、
provider-skip replay、incremental-cache subject ordering 和 `query_record_for_subject` 共用同一套 authority
result helper。`CheckedModule` 当前仍作为 eager sema 聚合结果，但 durable sema facts 的 materialization 输入
来自 stable id、incremental key、module id、part index、body range 和 side-table summaries；跨 session 的事实
由 query record/cache 保存，lowering-only side table 仍留在 checked aggregate 内。新增/更新 query、robustness
和 driver cache 覆盖 authority valid/invalid、语义敏感 fingerprint、依赖边、split logical module package rows
和手工 query record fixture。

2026-05-29 M3.2 WP-4/5/6 已完成并收口 Query-backed Sema 当前批次：新增
`SemanticLookupService`、`SemanticTypeService`、`SemanticGenericService` 和 `SemanticBodyCheckService`
作为 `SemanticAnalyzerCore` 的内部 service boundary；pipeline 的 generic definition、ordinary function body
和 type layout validation 已通过这些 service 进入现有 analyzer/resolver，不复制 analyzer state、不重写语义算法。
`aurex_tooling::IdeSnapshot` 现在在同一个 `query` snapshot 中暴露 module/item/signature/body/type-check
query records、dependency edges 和 `semantic_facts`；semantic facts 携带 stable `DefKey` / `MemberKey` /
`BodyKey` / `GenericInstanceKey`、source range、part index 和 checked 标记。IDE semantic module identity
已对齐 sema stable module identity，避免 tooling `ModuleKey` 与 checked stable def key 分叉。M3.2 当前
work package 已全部完成，后续若继续推进，应以新的 M3.3 / LSP adapter / 更细粒度 incremental sema 计划为入口。

2026-05-29：M3.2 已 fast-forward 合并回 `m3`，M3.3 Tooling Session And Incremental Sema 也已完成并合并回
`m3`。M3.3 收口基线包括协议无关 `ToolingSession`、versioned open-document state、`IdeSnapshot`
snapshot cache、in-place snapshot 构建入口、session-level diagnostics/hover/definition/reference wrappers、
最小 `LspServer` JSON-RPC adapter、document symbols、incremental reuse planning、workspace semantic indexing
和 quality gates。LSP 层只消费 tooling value types，不读取 parser/sema/query/driver internals。

2026-05-29：`m3.4` 已从 M3.3 收口基线切出。新的执行入口是
[Aurex M3.4 Real Incremental Sema Execution 计划](m3.4-real-incremental-sema-plan.md)。M3.4 的优先级是把
M3.3 reuse explanation 变成 executable semantic fact reuse：snapshot construction 必须接收 previous
snapshot/query context，query reuse decision 必须驱动局部重算，semantic facts 必须在 body-local /
signature / module edits 后保持稳定，workspace index 尽量按 affected fact identity 更新。完整 M3.4-M3.9
路线已写入 [M3 路线图](m3-roadmap.md)。

2026-05-29：M3.4 Real Incremental Sema Execution 已完成当前 deterministic tooling/query 边界。
`IdeIncrementalSnapshotInput` 会把 previous query snapshot 传入 `build_ide_snapshot_into(...)`；可证明
unchanged 的 file/lex/parse/diagnostics、module-surface、item-signature、generic-template-signature、
function-body-syntax 和 type-check-body records 会在 provider evaluation 前 seed 到 `QueryContext`。
`ToolingIncrementalSnapshotResult` 暴露已执行的 reuse plan、reuse-execution counters 和 workspace-index
update stats；`ToolingWorkspaceSemanticIndex` 报告 retained、replaced、removed、inserted facts，并避免对外返回
旧 document version 的 stale entries。聚焦测试覆盖 accepted/rejected previous context、重复 body-local edit
稳定性、removed-definition invalidation、generic body-edit reuse、malformed reuse 和无旧版本泄漏的 workspace
facts。该阶段已收口，后续目标已经推进到 M3.5/M3.6。

2026-05-29：M3.5 Incremental Syntax And Stable AST Identity 已完成当前 deterministic tooling/syntax 边界。
`ToolingSession` 新增 range-based edit 入口，`ToolingDocumentTextEdit` 能描述 begin / removed length / inserted
text，并在 `change_document_range_with_reuse_plan(...)` 中返回 applied edit、精确 edit impact、reuse plan 和
incremental snapshot result。`LosslessNodeStableKey` 已作为位置无关 syntax identity 落地，不使用绝对 source
range 或 token index；`compare_lossless_stable_nodes(...)` 通过 stable-key multiset 报告 reused、recomputed、
invalidated 和 collision counters；这些结果进入 `ToolingIncrementalSnapshotResult::syntax_reuse`。新增
`IdeAstNodeInfo` / `ToolingAstNode` 将 offset 投影到 AST item 或 function body，并输出稳定 `DefKey` /
`BodyKey`，使 offset-to-token、syntax-node、AST-node 和 semantic-fact projection 能在同一个 snapshot 中对齐。
M3.6 Project Graph And Persistent Query DB 已完成，下一目标是 M3.7 IDE Semantic Features。

2026-05-30：M3.6 Project Graph And Persistent Query DB 已完成当前工程级 query/cache 边界。
`ProjectModel` / `WorkspaceModel` 已作为 `aurex_project` 公共目标落地，driver invocation 和
`ToolingSession` 都消费同一套 package root、source root、import roots、target config、command options
和 open buffers 输入。query 层新增 `ProjectKey`、`QueryKind::project_graph` 和 project graph provider；
`module_graph` 现在显式依赖 project graph 和 module part queries。incremental cache schema 升到 2，
header 写入 project identity、package/source root、target config、command options 和 open buffer count；
`incremental_cache.project_inputs` profile 子事件能说明 project input 是 reuse 还是 reject，以及具体 changed
inputs。测试覆盖 project graph key/layout/provider、edge verifier、driver cache row/edge/profile 和 tooling
workspace model open/change/close 行为。详细计划见
[Aurex M3.6 Project Graph And Persistent Query DB 计划](m3.6-project-graph-persistent-query-db-plan.md)。

2026-05-30：M3.7 IDE Semantic Features 已完成当前协议无关 IDE 能力第一层。
`IdeSnapshot` / `ToolingSession` 新增 completion、rename、semantic tokens、inlay hints、code actions 和
workspace symbols value types；completion 合并 syntax context、sema scope、keyword 和 open workspace
semantic facts；rename 基于 symbol identity 生成 workspace edit plan，并检查 identifier、reserved keyword
和可见符号冲突；semantic tokens 由 syntax token kind 加 checked symbol facts 合成；inlay hints 当前覆盖缺少显式
类型标注的 checked locals；code actions 从结构化 help diagnostics 生成 lookup suggestion quick fix。
LSP adapter 只做投影，新增 `textDocument/completion`、`textDocument/rename`、
`textDocument/semanticTokens/full`、`textDocument/codeAction`、`workspace/symbol` 和
`textDocument/inlayHint`，并在文档请求边界使用 generation guard 防止 stale result 发布。详细计划见
[Aurex M3.7 IDE Semantic Features 计划与收口记录](m3.7-ide-semantic-features-plan.md)。

2026-05-30：M3.8 Query-backed Lowering / Backend Reuse 已完成当前 lowering/IR/backend reuse 边界。
`lower_function_ir` query rows 现在只在真实 lowering 和 IR pass pipeline 完成后写入 incremental cache；
普通函数体通过 `BodyKey`，generic function instance 通过 `GenericInstanceKey` 映射到 lowered IR function
symbol，并用真实 target-independent IR unit fingerprint 生成 result。新增 `ir::layout_abi_fingerprint(...)`、
`ir::function_ir_unit_fingerprints(...)` 和 `ir::llvm_emission_unit_fingerprint(...)`，把 type layout、
payload enum layout、ABI symbol、target-independent IR unit 和 LLVM emission unit 分成可观测的稳定事实边界。
`PassPipelineRunSummary` 现在记录 invalidated analyses，`ir.pass_pipeline` profile detail 输出
scheduled/executed/changed/preserved/invalidated 摘要；`llvm.emit_ir` profile detail 输出 function unit 数量和
layout/ABI fingerprint global id。`check` / `typed` / `checked` 仍不写 lower IR rows，`ir` / `llvm-ir` /
native emit 在 lowering/pass pipeline 后写真实 lower IR rows。详细收口记录见
[Aurex M3.8 Query-backed Lowering / Backend Reuse 计划与收口记录](m3.8-query-backed-lowering-backend-reuse-plan.md)。

2026-05-30：M3.9 已完成完整 M3 release baseline 收口。`m3` 分支现在包含 M3.0 到 M3.8 的全部阶段成果，
并新增最终 authority-boundary audit 和质量门基线。固定的公开边界是：source/lex 产出 source facts；
parse/syntax 持有 AST 和 stable syntax identity；module/project 持有 `ModuleRecord`、`ModulePartKey`
和 project graph facts；sema 持有 query-backed durable checked facts；tooling 通过 `IdeSnapshot` 和
`ToolingSession` 消费事实；LSP 只作为 adapter；lowering 持有 verified Aurex IR 和 IR unit fingerprints；
backend 消费 optimized Aurex IR，不回读 AST 或 sema 私有状态。详细收口记录见
[Aurex M3.9 M3 Release Baseline 与 Authority Audit](m3.9-m3-release-baseline.md)。

2026-05-28 WP-1B Generic Instance Identity Propagation 已完成：`FunctionSignature`、`EnumCaseInfo`、
`GenericEnumInstanceInfo` 和 `GenericTypeAliasInstanceInfo` 都携带结构化 `GenericInstanceKey`；
generic function / owner-generic method 的 retained 与 non-retained 路径都会把 identity 写入 checked
signature；generic type alias 实例保存 resolved target type 和对 target type 敏感的 instance signature
incremental key，但仍保持透明别名语义；incremental cache 的 generic instance signature subject 从 checked
metadata 收集并按 key 去重，invalid key 不再进入 query subject。新增白盒覆盖 function signature identity、
generic enum case identity、generic type alias instance identity、checked module copy/move 保真和 driver cache
rows。

2026-05-28 WP-2 Generic Query Authority 已完成：query provider input 现在携带
`GenericTemplateSignatureAuthority`、`GenericInstanceSignatureAuthority` 和 `GenericInstanceBodyAuthority`，
不再只把泛型 query result 绑定到裸 `IncrementalKey` 或 body fingerprint。template authority 记录 signature、
`ModulePartKey`、namespace、visibility rank、param count 和 constraint count；instance signature authority
记录 instance kind、type/const args、param-env predicate count、value/generic param counts、return/receiver、
unsafe/variadic/definition flags；body authority 记录 checked body、signature result、generic side-table layout
count、sparse fallback count 和 retained/local-dense/sparse flags。incremental cache subject 从 checked metadata
和 module records 构造这些 authority，provider、provider-skip replay、query pruning 和 `query_record_for_subject`
共用同一套 result fingerprint helper。generic struct / enum 实例的 upstream signature fingerprint 同步补强为
解析后形状敏感，struct 字段和 enum payload 在同一个 `GenericInstanceKey` 下变化时也会改变 signature result。
新增 query/sema/driver 覆盖 authority valid/invalid、fingerprint 语义敏感性、generic signature/body dependency、
fallback/cycle、generic aggregate shape、generic cache rows、query pruning reuse 和 malformed graph / identity
repair。

2026-05-28 WP-3 Generic Body And Lowering Closure 已完成：retained generic function instance 现在在
`GenericFunctionInstanceInfo` 中显式保存 `body`，`CheckedModule::generic_function_instance_body_view(...)`
把 instance、signature、side table、AST item 和 body 组成唯一的 sema-authority 视图。IR lowerer 的 generic
declaration/body lowering 改为消费该 view，`lower_ast` 在缺失 retained body view 时直接返回 internal error，
不再静默跳过或在 lowerer 侧重新解释 generic identity。incremental cache 的 generic body checked-result
fingerprint 改为读取 retained body 对应 AST block range，`--emit=typed` 只写 generic body query rows，不再写
lower generic IR rows，IR/LLVM/native emit mode 才收集 lower generic instance IR subject。新增/更新 sema、IR
whitebox、driver cache 和 sample runtime 覆盖 retained/discarded emit-mode 边界、generic body view、缺失 view
拒绝、checked module copy/move body 保真和 `generics/basic_m2.ax` native execution。

2026-05-28 WP-4 Generic Builtin Operand Closure 已完成：generic function / method instance body 的
`GenericAnalysisScope` 在 retained 与 non-retained side-table 路径都缓存 syntax type handle，`sizeof[T]` /
`alignof[T]`、`ptrat[*const T]`、`ptrcast[*const T]` 和 `bitcast[*const T]` 这类 builtin type operand
在实例化后由 sema 写入 concrete type，再由 IR lowering 消费同一份 retained side table。`ptraddr`、
`sliceptr`、`slicelen`、`strptr`、`strblen`、`strvalid`、`strfromutf8` 和 `strraw` 已通过泛型正样例覆盖
retained expression side table 路径。新增 `generics/builtins_m3_1.ax` 的 IR dump 和 native smoke 覆盖，
并新增缺 `Sized` 的 `sizeof[T]`、非法 `ptrat[T]` target 负样例。`cast[T](value)` 仍受现有 scalar-cast
规则限制，M3.1 不引入新的 `Scalar` / `Cast` capability。

2026-05-28 WP-5 Method-local Generics 已完成：`impl[T] Owner[T]` 的 owner 参数继续作为 method
`generic_params` 前缀，method-local 参数保持独立 `GenericParamIdentity`；sema 注册阶段不再拒绝
method-local generic，普通方法调用可从参数推断局部泛型，显式 `value.method[T](...)` / `Type.method[T](...)`
只绑定 method-local 参数。generic method instance 现在使用 `DefNamespace::member` 的 `GenericInstanceKey`
和生成 semantic key / ABI suffix，避免同一 owner 上不同 method-local 实参实例发生 lookup 或 C 符号碰撞；
实例不再写入普通 method-name lookup，调用解析始终回到 template bucket。IR lowering 已支持
`generic_apply(field(...))` 显式泛型方法 callee 并正确补 receiver 参数。新增
`generics/method_local_m3_1.ax` 正样例和 arity、无法推断、where 不满足、非泛型方法误传 type args 负样例；
原 “method-local generic unsupported” 回归测试已更新为新语义 checked dump 覆盖。

2026-05-28 WP-7 Generic Closure Audit And Release Baseline 已完成，M3.1 泛型闭环进入可验收基线：
审计确认 `generic_instance_abi_suffix` 只接收 `GenericInstanceKey`，generic struct / enum / type alias /
function / owner-generic method / method-local generic method 的 stable id、ABI suffix、incremental key 和
query subject 都从 `GenericInstanceIdentity` 或 checked metadata 中的结构化 `GenericInstanceKey` 派生。
`generic_instance_key_suffix` 仍可使用 session-local `TypeHandle.value`，但只作为本次编译的 lookup/cache fast
key；白盒测试已经验证它跨 session 可不同，而 stable instance key 和 ABI suffix 相同。checked dump、
diagnostics、IR dump 中的 display string / c_name 只作为展示输出，incremental cache generic
signature/body/lower-IR subjects 从 checked metadata 和 authority 结构收集并按 `GenericInstanceKey` 去重。
新增 `generics/method_local_identity_closure_m3_1.ax` 和 runtime smoke 覆盖同一 owner-generic 类型上的
owner-only method、method-local method，以及相同 method-local type args 跨不同 owner instance 的 native
行为。M3.1 当前 release baseline 明确不包含用户 trait、associated type、const generic、resource
capability、RAII、closure、async/generator/iterator 或标准库重建。

M1 阶段已经舍弃。主要原因不是单个功能失败，而是整体设计方向不稳：

- 标准库、host support、构建工具样例和语言核心同时扩张，导致测试结果很难判断是语言问题、库问题还是工具链问题。
- 部分能力仍缺少语言级规则，例如资源释放、容器 API 约束，以及未来标准库 `Result` / `Option` 定义和当前结构化 `?` shape 之间的正式绑定。
- 语法地基还没有冻结时就推进 selfhost/build-tool 路线，造成前端、库 API、资源模型互相牵制。
- M1 的很多样例验证的是“能跑通当前 demo”，不是验证语言核心是否稳定、可解释、可长期扩展。

因此 M2 只承认当前 C++ Stage0 编译器、Aurex IR、LLVM backend 和自包含语言样例作为有效基线。当前仓库没有 `std/` 目录，也没有 `selfhost/` 目录；相关旧路线只作为历史输入，不再代表当前进度。

## 已完成能力

当前生产编译器位于 `src/` 和 `include/`，使用 C++20 实现。

- CLI 支持 `--check`、`--dump-*`、`--emit=*`、`--opt-level`、`-I`、`-o`、`--clang` 和 `--clang-arg`。
- driver 能完成文件 IO、模块加载、编译流水线调度、IR lowering / pass pipeline、LLVM IR emission、
  LLVM IR 临时文件生成和 clang 调用；当前主路径已经拆成 `CompilationSession`、`CompilationPipeline`、
  `FrontendPipeline`、`LoweringPipeline`、`BackendPipeline` 和 `PipelineStage` 阶段记录。
- module loader 支持根模块和 import 模块合并，能检测模块名不匹配、缺失 import、循环 import、重复模块名和重复加载。
- lexer/parser 是手写实现，输出 ID-backed AST；token、AST、module、checked、IR、LLVM IR 都有 dump 路径。
- 语义分析已具备类型表、符号表、函数签名、ABI 名称、struct/enum 元数据、泛型实例化、表达式类型表和 source range 诊断；诊断分级支持 error/warning/note/help，lookup miss 已有 `did you mean` help，常见 type mismatch 输出 expected/actual note，duplicate 主路径输出 previous declaration note，parser 成对 delimiter 缺失输出 opening delimiter note。M2.5 的首项工作已完成：sema 诊断在创建时携带显式 kind/category/code，不再从 message 文本反推机器元数据。
- 当前语言样例覆盖函数、函数指针类型、`extern c`、`export c fn`、普通 `fn main`、import、可见性、const、type alias、struct、enum、opaque struct、泛型、method、指针、数组、slice、字段访问、索引、cast、内建 size/align、`if` 表达式、block 表达式、`match`、`while`、C-style `for`、基础 `for i in range(...)`、`defer` 和 `?`。
- 程序入口支持根模块普通 `fn main`，签名包括无参数或 `argc/argv`，返回 `i32` 或 `void`。
- Aurex IR 是 typed CFG/SSA-like 中间层，后端只消费 IR，不回读 AST。
- IR verifier 会检查函数、block、value、terminator、类型和引用一致性。
- pass pipeline 支持 `O0` 到 `O3` 选项；当前实际优化以保守的局部 mem2reg 和 CFG cleanup 为主。
  IR pass manager 已提供 `ModulePassManager`、`PassResult`、`PreservedAnalyses`、`VerifierGate` 和
  `run_pass_pipeline_with_summary`，旧 `run_pass_pipeline` 调用保持兼容；`ModuleAnalysisManager` 已提供 CFG、
  dominance 和 value-use 缓存与 pass 后失效；verifier gate failure 现在携带稳定 stage/profile/verifier/pass
  上下文，driver 从 `PipelineStageId::ir_pass_pipeline` record 传入阶段名，原始 verifier 错误 body 保持不变。
- `aurex-profile-v1` 的 driver 主阶段 phase 附带可选 `stage` 元数据对象，包含 stage id、input、output、
  diagnostic ownership 和 cache/query impact；原 phase name 和内部 incremental-cache query 子事件保持不变。
  incremental-cache 子事件附带可选 `parent_stage` 元数据，profile viewer 可以把 source-stage reuse
  归回 `incremental_cache.lookup`，把 query diff / plan / pruning / provider-eval 归回
  `incremental_cache.write`，但这些子事件仍不是 driver 主阶段。`PipelineStage` 同时提供
  `DiagnosticCategory` 到候选 owner stage 的反查；`IdeDiagnostic.owner_stages` 已通过公开
  `PipelineStageMetadata` 消费这些记录；profile 主阶段和 incremental-cache 子事件也通过
  `PipelineStageId` / `PipelineProfileSubeventId` 进入 profiler；profile phase 消费端通过
  `pipeline_profile_phase_classification(...)` 统一分类，避免后续 IDE/LSP 阶段视图重新维护 phase-name
  或诊断阶段表。
- LLVM backend 使用 LLVM C++ API 从 Aurex IR 生成 LLVM IR，并通过 clang 生成 asm、object 和 executable。

## 已删除能力

M2 明确删除并暂缓这些 M1 内容：

- `std/` 源树。
- host C support 源文件和自动链接。
- driver 的标准库查找、import path 自动注入和安装后 std 查找。
- CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 安装规则中的 `share/aurex/std`。
- std/M1/system/build-tool 样例。
- 依赖标准库名字的资源语义特判。
- selfhost / Stage1 / AIR snapshot 路线在当前仓库中的实现。

这些内容不是永远不能回来，而是必须等基础语法、module/package、`unsafe`、slice/string 和泛型约束稳定后再重新设计或重新评估；拥有型资源库还要等后续资源语义专题完成。

## 质量门

当前主要质量门：

```sh
tools/run_tests.sh
tools/bench.py
make perf
make perf-stress
make perf-stress-threshold
make perf-release-threshold
make perf-ast-stress
```

测试覆盖：

- lexer/parser/AST dump。
- driver/CLI/native toolchain。
- positive/negative `.ax` sample suite。
- 模块、可见性、泛型、函数、方法、pattern matching、error handling 和类型系统诊断。
- Aurex IR lowering、IR verifier、pass pipeline、LLVM backend。
- native execution 和安装后 compiler 执行。

`tools/bench.py` 使用 Release `build/perf` 构建目录，并用 Google Benchmark
测量 frontend 热路径。`make perf` 输出基于 JSON 的 Aurex frontend baseline，
覆盖 lexer、lookup-heavy sema、generic-instantiation-heavy sema 和 AST bulk sema 路径，并运行
Google Benchmark 的进程级现代前端对比通道，对可用的 `clang++`、`g++`、`rustc`
做 frontend/check 模式基线；`make perf-compare` 只运行跨前端对比通道。
`make perf-stress` 运行 `tools/generic_stress.py`、`tools/ast_stress.py` 和
`tools/diagnostic_stress.py`，生成 mixed-feature 泛型、AST bulk 和 diagnostic
源码，并记录 `aurexc --check` elapsed time + peak RSS baseline；`make perf-ast-stress`
只运行 AST bulk RSS/time lane。三条 stress lane 默认使用 `--shape=mixed`：generic lane 覆盖
generic struct/enum/type alias、where constraint、impl method、pointer alias、tuple/pair、slice
和 pattern matching；AST lane 覆盖 extern、type alias、struct/enum、impl/method、generic
constraint、tuple、array/slice、match/or-pattern、`let else`、`if is`、`try`、`defer`、
for/while/range、compound assignment、unsafe pointer/string builtin，并在 2M bulk 中抽样保留复杂
feature block；diagnostic lane 循环覆盖 unknown name、type mismatch、call arity/type、field/index、
struct literal、enum payload、builtin、generic apply、array/void、operator 和 match-arm 类型错误。
`tools/generic_stress.py` / `tools/ast_stress.py` / `tools/diagnostic_stress.py` 现在支持
`--max-elapsed-ms`、`--max-rss-mib`、`--threshold-profile` 和 `--threshold-scale`；三条 lane 共用
`tools/perf_thresholds.py`，把 raw thresholds、profile、scale、machine info、effective thresholds、
进程级 wall/user/sys/RSS/page fault 指标、以及 `aurexc --profile-output` 产出的
`aurex-profile-v1` 阶段 profile 写入 JSON。`make perf-stress-threshold` 默认跑 100/200 generic +
1000/5000 AST bulk + 100/500 errors 的轻量阈值门，GitHub Actions `stress-thresholds` job 固定
`AUREX_PERF_THRESHOLD_PROFILE=github-ubuntu-24.04-fedora`；`make perf-release-threshold` 现在默认用
独立 `build/perf-lto` + `AUREX_STRESS_ENABLE_LTO=ON` 跑 5000 generic、2M AST 和 5000 errors 的
Release+LTO 发布阈值门。`make perf-release-lto-threshold` 和 `make perf-release-all-threshold`
保留为同一发布阈值门的兼容别名，不再重复跑普通 Release 与 Release+LTO 两套逻辑；发布 AST RSS 阈值为
8192 MiB，用来保留高复杂 mixed 源码而不是降级成过渡态 toy case。
generic function instance 签名、
generic struct/enum `TypeInfo` 和 checked enum case display 已经把内部 semantic key / TypeHandle
args 和展示名分离，`--check` 热路径不再为了 checked signature 或泛型类型实例生成
`id[i32]`、`Box[i32]`、`Maybe[i32]_some` 这类展示字符串，dump、IR lowering 和诊断需要时再延迟格式化。
`--check` 模式不再保留泛型实例 side table；`--emit=typed` 会保留 typed generic body 但不做 lowering，
用于把 retained-side-table 内存和 IR/codegen 成本分开压测。`tools/generic_stress.py --shape=templates` 覆盖 2000/5000+ 不同泛型模板场景；IR/native 输出模式继续保留 lowering 所需表，保证 codegen 行为不退化。
AST 主路径也已按 P0-Perf-4 收口：driver 持有 parser/module AST 并把 mutable 引用传给 sema 和 IR lowering，
`SemanticAnalyzer(const AstModule&)` 被删除以避免隐式整树复制，`CheckedModule::normalized_ast` 已改成轻量
normalization overlay，彻底不拥有 `AstModule` snapshot，sema 构造期不再对 `exprs/types` 做 `size+4096`
reserve，postfix suffix 创建不再按值复制胖 `ExprNode` / `TypeNode`。节点级 C symbol side table 也已从
`std::vector<std::string>` 改为 `expr_c_name_ids` / `pattern_c_name_ids` / `item_c_name_ids` 的 `IdentId`
表，实际文本只进入 checked module 的 C-name interner 去重，不再每个节点分配一个 `std::string`。
2026-05-15 的 compact AST 主线已继续落地：`TypeNode`、`ExprNode`、
`PatternNode`、`StmtNode` 和 `ItemNode` 的主存储从胖节点 vector 改成 32B compact header +
per-kind payload arena，module loader 合并模块时按 compact payload remap/append，不再依赖
fat-vector 地址或 `.data()` 指针反推 ID；sema 的 item owner 查询也改为显式 `ItemId`，不再靠
`items.data()` 地址反推。2026-05-16 继续把 sema、IR lowering 和 AST dump 的 `ExprNode` 热路径迁到 compact
view / payload 直读，移除 `ExprNode` 兼容 wrapper 和 literal 分析里的胖节点重建；同日继续落地
global bump allocator + syntax 层 `IdentifierInterner`，AST 的 type/expr/pattern/stmt/item/module/import
名字字段携带原生 `IdentId`，parser、module loader、postfix suffix 创建和 sema 入口都会把节点/metadata
重新收口到当前 `AstModule` 的 identifier arena，sema typed lookup key 不再维护第二套私有 interner；函数、类型、值、
generic template、enum case、struct field、method/member 和局部 scope lookup 都使用 `IdentId` typed 索引，
checked-module map 不再保留并行 string-key lookup 路径；生成的 ABI/display/dump 文本进入 bump-backed
`IdentifierInterner`，`FunctionSignature`、`Symbol`、`StructInfo`、`EnumCaseInfo`、`TypeAliasInfo` 和 `TypeInfo`
只保存 `InternedText` / typed id，不再持有堆分配 `std::string` payload。2026-05-16
性能线随后删除旧胖 `ExprNode` 生产类型，parser 创建、`AstModule` 存储、module loader append 和 postfix
suffix 都直接写 compact expression header + per-kind payload。随后又把 parser 表达式创建 API
收紧到字段级 append/set：name、unary/binary、call、if/block/match、array、field/index/slice、
struct literal、generic apply、try 和 cast-like 节点都不再先构造 payload 大临时对象；postfix suffix
的 call/field/index/slice/generic apply/struct literal/try 路径也直接写 compact payload。2026-05-16 后续 bump pass 又把
`TypeNodeList` / `ExprNodeList` / `PatternNodeList` / `StmtNodeList` / `ItemNodeList` 的 header vector 和
per-kind payload vector 接到 `BumpAllocatorAdapter`，`IdentifierInterner` 的 text vector、hash bucket/node
也进入同一个 bump arena；parser 会根据 token 形态估算 statement/item/type/pattern/identifier 规模，并在 AST
模块创建初期按 token 形态对 expression header 和每类 payload 计算容量上界，直接从 bump arena 取好 vector backing storage 并对表达式 arena page pre-touch；parser 创建表达式节点时只在这些已分配区间内顺序 emplace，不再依赖 vector 热路径自动扩容，也避免解析过程中逐节点首次触达新页。后续 lexer/sema bump pass 已把 lexer token 输出改为 `TokenBuffer`，token vector backing storage 由 bump arena 持有并一次性 reserve；token 容量不再被 262144 上限截断，也不再预触永远不会写入的估算余量页，避免 bump vector 扩容留下旧 backing storage。sema 的 `CheckedModule`、`GenericSideTables`、`PatternCaseNameTable`、`TypeTable`、`SymbolTable`、analyzer lookup/cache 主表、`FunctionSignature` 参数/generic args、`StructInfo` 字段、`EnumCaseInfo` payload 列表、`TypeInfo` tuple/function/generic args、generic template 参数列表和 generic constraint bucket 也改为 bump-backed storage；sema 持久 name / c_name / value_text / generic key 字段改为 `InternedText`，复制 `CheckedModule` / `TypeTable` / `SymbolTable` 时会重新 intern 到目标 arena，不再复制字符串 buffer。generic function instance 列表使用 bump-backed deque 保持元素地址稳定；retained generic instance 使用函数体局部 NodeSpan side table，只有非连续节点 ID 映射才共享 module-level sparse layout，不再按实例复制 sparse ID mapping vector；generic method / enum-case / visible-module cache bucket 不再由 `operator[]` 创建普通 heap vector；IR lowering 的源码 local lookup 和 IR verifier 的 symbol 去重也改为 interned typed id，不再保留持久 string-key map。
2026-05-16 后续补齐了 source-name 去重：`FunctionSignature.name`、`Symbol.name`、`StructInfo.name`、
`StructFieldInfo.name`、`TypeAliasInfo.name`、`EnumCaseInfo.name/case_name/enum_name`
等源代码已有 identifier 的字段在普通 driver 路径直接借用 `AstModule::identifiers`，不再把函数名、字段名、
局部名再复制进 checked C-name interner；`CheckedModule` move 只重绑原本属于自身 `c_names` 的
`InternedText`，不会把 borrowed AST name 错绑到 checked interner，显式 copy 才重新 intern 到目标模块。
ABI 校验也从临时 `IdentifierInterner` 改为 `std::string_view` key，避免校验阶段把所有 C symbol 再复制一遍。
benchmark 和 stress profile 是机器相关 RSS/耗时数据的唯一来源；主文档只保留 gate 形态和 profile/scale 校准机制，
不再把本机样例数字写成可移植 baseline。
generic side table 生命周期已在主路径收口：sema-only expected-type 和 pattern-case cache 进入可释放 arena 并在分析后丢弃；retained instance 只保留 lowering 需要的表；非连续 NodeSpan sparse ID mapping 按模板共享；小型 per-instance side table arena 从默认 64 KiB floor 降到 1 KiB block，覆盖 2000/5000+ 不同泛型模板的固定开销。跨模块 stable hash、parallel global ID、轻量 generic/AST/diagnostic 阈值门、默认 Release+LTO 的 5000 generic / 2M 高复杂 AST / 5000 errors 发布阈值门、8 GiB AST RSS 阈值、阶段级 profile，以及 profile/scale 形式的跨机器阈值校准机制已接入；后续只保留新增机器 profile 数据和 query 级增量复用，不再缺身份或 release gate 主路径。
2026-05-17 M2.5 起步后，诊断协议先做了结构化收口：语义诊断现在由显式 semantic kind 映射到稳定 category/code，message 只保留展示职责；CLI 文本、JSON 输出和后续 diagnostics query / LSP 适配将共享同一事件语义。随后 M2.5 主线完成第一批 query key 与依赖跟踪，下一阶段转向 lossless syntax 和 IDE-native 入口，而不是继续扩张语言面。
2026-05-21 M2.5 收尾重构继续推进：`src/driver/diagnostic_renderer.cpp` 已经是唯一的诊断输出层，
`src/driver/module_loader_remap.cpp` 已把 AST remap / append 从 `module_loader.cpp` 中拆出；
incremental cache 也已经从单个巨型 `incremental_cache.cpp` 拆成 `src/driver/incremental_cache/`
内部模块，分别承担 format、io、query orchestration、subjects、reuse、source-stage reuse、
schedule、profile 和 query stats。`subjects` 又继续拆出 source、module、semantic、ordering
四个内部域；随后按聚合度要求把过细的 incremental cache `io/fingerprint.cpp`、`io/reader.cpp`
和 `io/writer.cpp` 回收进单个 `io.cpp`，让路径规范化、source fingerprint、cache parser /
validator、row writer 和原子发布共享同一个 wire-format 权威入口。`module_loader.cpp` 现在只做文件读取、import 解析、模块校验和递归编排；
`incremental_cache.cpp` 则降为 public facade，不再持有协议、I/O、query subject 生成、reuse plan
或 profile 明细。driver / backend public header 也已开始按 facade 边界瘦身：
`EmitKind`、`DiagnosticOutputFormat`、`ModuleRecord`、`LlvmIrEmitter` 和
`OptimizationLevel` 拆为窄接口头；`compiler.hpp` 不再包含完整 `ir.hpp`，
`incremental_cache.hpp` 不再把 module loader、checked module 和 AST 头传递给所有调用者，
`diagnostic_renderer.hpp` / `native_toolchain.hpp` 也只依赖各自需要的 enum。query 层新增
`query_graph.hpp` / `query_graph.cpp`，让 graph node / edge / dependency-kind 规则从
provider-heavy 的 `query_context.hpp` 中分离出来，`query_edge_verifier.hpp`、`query_replay.hpp`
和 `query_reuse.hpp` 只在实现文件里回到 context。随后 query 层继续新增
`query_provider_set.hpp` / `query_provider_set.cpp`，把 provider 类型别名、默认 provider wiring
和自定义 provider 回退策略收口为独立 Strategy 集合，`QueryContext` 不再直接持有十五个
provider 成员，而是专注 query 生命周期、依赖图维护、缓存/失效协调和 provider set 转发；
后续又把 `QueryProviderSet` / `QueryContext` 的 15 参数 provider 构造入口收束为
`QueryProviderOverrides` 聚合对象，短构造和 setter 继续保留，但新增或扩展 query provider
不再需要把所有 provider 作为并列参数穿透 public facade；
`query_executor.hpp` 也改为前置声明 `QueryContext`，避免 batch request API 被动包含完整
context 实现。
module loader 继续收口但保持聚合度：`module_loader_support.cpp` 作为私有 support 聚合点，
承接路径/canonicalization、import 候选、文件读取、lex/parse、模块诊断和身份校验；
`module_loader_remap.cpp` 继续只负责 AST remap / append 独立算法域；`module_loader.cpp`
保留加载状态机、模块登记、import 递归和 append 编排。加载中集合清理改为局部 RAII scope，
错误分支不再手动重复 erase。后续新增或重构函数参数数以 6 个为上限，超过时必须用具名
context/value object 或重新切分职责，避免低聚合度 helper 继续扩散。parser 的
`parser_source_ranges.cpp` 也已并回 `parser_part_ranges.cpp`，让 source-range composition 和
AST range lookup helpers 留在同一个 range reader 聚合点。
2026-05-22 继续把 parser postfix 的 `[]` 判定收成中等粒度分类层：
新增 `src/parse/bracket_suffix_classifier.hpp` / `src/parse/bracket_suffix_classifier.cpp`，
统一拥有空 `[]`、slice、type-only 参数、generic call/literal continuation、selector continuation
和嵌套 generic continuation 的分类规则；`parser_postfix.cpp` 仍保留参数解析、错误恢复和 AST
节点构造，不把 index/slice/generic apply 继续拆成低聚合度小文件。
2026-05-22 sema 开始按“外部不散，内部不耦合”的路线做第一阶段 facade/core 收口：
`include/aurex/sema/sema.hpp` 已从约 800 行瘦到 39 行，只保留 `SemanticOptions`
和 `SemanticAnalyzer` 稳定入口；旧 analyzer 的内部类型、lookup cache、泛型模板信息、
表达式 view、pattern/statement helper 和 analyzer 状态先移动到 `src/sema/internal/sema_core.hpp`，
由 `src/sema/sema_facade.cpp` 通过私有 `Impl` 持有。生产调用方继续只依赖 public facade，
sema 实现和白盒测试才显式包含 `<sema/internal/sema_core.hpp>`；白盒测试改为
`AUREX_SEMA_WHITEBOX_TESTS` opt-in 内部访问，不再用 `#define private public` 打开 public
`sema.hpp`。这一步只建立稳定外部入口和内部 core 边界，不改变 sema 算法；后续在 `SemaState`
稳定后再引入 NameResolver / TypeResolver / GenericEngine / FunctionChecker /
ExprChecker / StmtChecker / PatternChecker 等中等粒度 domain service。同步把
`FunctionBodyContextScope` 的 7 参数构造器收束为具名 `Config`，继续执行新增或重构函数参数不超过
6 个的规则。新增 public facade 测试覆盖 borrowed AST 和 owned AST 两个入口，`src/sema/sema_facade.cpp`
lines/functions/regions 均达到 100.00%。
同日继续完成第一块状态 owner 聚合：`current_module`、当前函数返回类型/推导、当前泛型上下文、
当前泛型 side table、loop depth、unsafe depth 和 const initializer 标记收进
`SemanticAnalyzerCore::FlowState`，旧的散落 mutable 字段不再直接挂在 analyzer core 上。
这一步仍然不新增深层目录，也不拆算法文件，只先把“当前分析上下文/控制流状态”确认为唯一 owner；
Name/Type/Generic/Function/Module 等更大的 owner 随后继续收进状态层。
随后继续把剩余 sema mutable table 收进中等粒度 owner：`NameState` 持有本地 symbol table、
typed lookup index、函数/值/enum-case 名称索引；`TypeState` 持有 named type、type alias
resolution stack/cache 和 struct side table；`GenericState` 持有泛型模板表、实例缓存、
param query key 和 placeholder function；`FunctionState` 持有 global value、definition item
和 body state；`ModuleState` 持有 visible/export module cache。构造期也改为按这些 owner
接入 bump arena，不再在 analyzer 构造器上平铺几十个 map 初始化。算法文件暂不继续切碎，
下一阶段才引入 NameResolver / TypeResolver / GenericEngine 等 domain service。
随后把 `CheckedModule`、bump arena 和这组 owner 统一包进 `SemaState`，`SemanticAnalyzerCore`
现在只直接持有 `state_` 一个可变语义状态入口；生产代码和白盒测试都通过 `state_.checked`、
`state_.names`、`state_.types`、`state_.generics`、`state_.functions`、`state_.modules`
和 `state_.flow` 访问对应 owner，避免 analyzer core 继续暴露一排平铺 mutable table。
随后补齐 context 层：`SemaContext` 统一持有 AST module、diagnostic sink 和 semantic options，
`SemanticAnalyzerCore` 不再直接平铺 `module_`、`diagnostics_`、`options_` 三个外部会话字段。
这让后续中等粒度 service 可以拿 `SemaContext&` 加必要 state owner，而不是继续依赖完整 analyzer。
`SemanticAnalyzerCore::analyze()` 也收成 phase pipeline：prepare、reserve、declaration、body、
validation 和 finish 六个阶段方法留在 `sema.cpp` 聚合点内，只表达阶段顺序，不为每个小阶段新建
目录或文件。
同轮把两个历史长参数接口继续收口：函数注册改为 `FunctionRegistrationRequest`，
generic side table 本地布局改为 `GenericSideTableLocalLayoutView`，不再让 item/owner/key/type/
incremental identity 或 expr/pattern/type/stmt sparse node id 以 8 到 10 个并列参数在调用链上传递。
随后 sema 的第一块中等粒度 domain service 也已落地：新增
`src/sema/internal/name_resolution.hpp` / `src/sema/internal/name_resolution.cpp`，由
`ModuleVisibilityResolver` 统一承接 import alias 解析、visible/export module cache、
模块路径匹配、public re-export 遍历和 module display name。`SemanticAnalyzerCore` 保留原有
方法作为薄 facade，`sema_lookup.cpp` 继续聚合 typed lookup 和 did-you-mean 逻辑，没有把
表达式或 lookup case 拆成碎片文件；resolver 只通过 `ModuleState` 的 mutable cache 写入可见性结果，
public header 仍只暴露稳定 sema facade。
相关 cache/query/incremental gtest、完整 ctest、改动 C++ 文件格式检查和
`tools/check_coverage.sh` 已经重新跑过，source totals lines/functions/regions 覆盖率分别为
95.27% / 97.79% / 95.20%，新的 `src/sema/internal/name_resolution.cpp`
lines/functions/regions 为 100.00% / 100.00% / 97.47%，新的聚合
`src/driver/incremental_cache/io.cpp` lines/functions/regions 为 95.30% / 100.00% / 96.11%，`module_loader_support.cpp`
三项为 99.38% / 100.00% / 98.65%，`aurexc` entrypoint 三项为 100.00%。
M2.5 第一批 query-key 主路径已经闭环：`--incremental-cache` 默认使用 query-key pruning，
只有显式 `--no-query-pruning` 才退回 coarse source-fingerprint 兼容路径。当前 query
基础设施覆盖 stable key、canonical type / generic instance identity、`QueryContext`
row/edge 落盘与 replay、source-stage green reuse、red-green provider-skip、profile
事件、query graph fuzz、sanitizer 和 release/coverage 门禁；后续 lossless syntax、
IDE-native 入口和高级语言特性都必须复用这条主路径。
lossless syntax 专项已经完成当前 M2.5 验收边界：lexer 增加 opt-in trivia token emission，默认
编译路径仍跳过 trivia；`LosslessSyntaxTree` 保存完整 token 序列，提供结构化
node/element/token-leaf API，记录 parent 和连续 token span，能校验 tree invariant，支持按 offset
查最深节点，并能重建整文件或任意 node 子树源码。dump 形态已经从原始 token stream 前进为：
`source_file` 下挂 `module_decl`、`import_decl`、`function_decl` 等顶层声明节点、直接 trivia/eof
token leaves，以及 `block` / `paren_group` / `bracket_group` / `brace_group` 分隔符组节点；
`token_stream` 只保留为非单调手造 token span 的保守兜底。parser 层新增 lossless CST -> AST
lowering façade，过滤 trivia 后走现有 parser，并用 AST dump parity 覆盖正常 semantic token 路径。
query-key 侧已经确保 retain-trivia 的 `LexFileKey` fingerprint 使用 trivia lexer，build-lossless
parse result fingerprint 混入 CST 结构，`lossless_tooling` 的 parse provider 依赖对应 retain-trivia
lex query。完整局部重解析仍属于后续更深优化，不再是 lossless syntax 基线缺口。
IDE-native 工程入口已经完成当前验收：新增 `aurex_tooling` 目标和
`include/aurex/tooling/ide.hpp`。`IdeSnapshot` 面向内存 buffer，一次构建统一产出
source manager、lossless tree、AST、checked module、结构化 diagnostics，以及
file/lex/parse/diagnostics query records 和 dependency edges；offset token、hover、顶层
definition、同名 identifier references、checked-backed 的全局查找、AST fallback 的局部参数 /
`let` 绑定查询，以及编辑影响 node 选择都通过这层 API 暴露。diagnostics 在 CLI 渲染和
query fingerprint 之前会先转成结构化 event stream。该入口不绑定 LSP protocol，后续 LSP
adapter 只消费 snapshot 数据，不再旁路 parser/sema/query 主路径。
2026-05-17 正则性能/测试线继续把 `RegexSet` exact-literal prefix trie 推进为持久标量 Aho-Corasick fast path：纯字面量 set 构建共享 trie 后补 failure/output link，`matches_set`、`find_set`、first-match scan、all-span/overlap scan 和 vectored flatten 后入口都用同一份自动机线性扫描；database 升级为 v3，序列化 node/terminal/max-literal 元数据，roundtrip 后不退回 VM active-list。测试侧补上 Unicode byte span、suffix failure output、重复 literal、database fast path workspace 和 deterministic RegexSet property corpus；`tools/regex_differential.py` 现在同时生成固定 + property Python `re` 差分、RegexSet exact-literal property cases、Unicode 17.0 full case-fold 与 UAX #29 `\X` conformance 程序，并作为 opt-in CTest slow conformance 入口，需通过 `-DAUREX_ENABLE_REGEX_CONFORMANCE=ON` 显式注册。
2026-05-16 后续表达式 P0 语义线把 expression type cache 从 final-only 记录拆为三层：
`expr_intrinsic_types` 保存表达式自身类型，`expr_types` 继续保存当前语义使用的 contextual final type，
`expr_expected_types` 作为 final cache key，`CoercionRecord` 记录 contextual integer/float literal、`null`
到 pointer、slice mutability 等调整。主模块和 generic side table 都有对应 intrinsic/final 存储，local dense
和 sparse fallback 行为一致；integer/float/null、unary/binary、slice、array/tuple literal、if/block/match
在 expected type 下会保留 intrinsic type，不再把 expected type 污染成表达式自身类型。IR lowering 仍读取
`expr_types` final table，coercion/intrinsic 只作为 checked 语义 overlay。
同日后续工程质量线把 `analyze_expr` 从单一大 dispatch 收口为一条明确主路径：
`analyze_expr(expr, expected)` 只负责 final cache lookup / expected key 记录，`analyze_expr(expr, view, expected)`
只做表达式类别调度；literal、value/name/call、control、aggregate、projection、operator、builtin 分别进入小型
helper。二元表达式内部也拆成 operand contextual typing、类型不匹配诊断、整数字面量 hazard 检查和 operator
result 记录，没有保留并行的新旧 analyzer 路径。
2026-05-16 后续 match 性能/正确性线又把结构化穷尽检查从“枚举 bool / enum 叶子笛卡尔积”替换为
pattern matrix / usefulness witness search：bool、enum payload、tuple、struct、4096 列显式 M2.1 边界内的 fixed array、open integer literal
和 dynamic slice 通过 constructor specialization、default matrix 和 slice 代表长度特化判定覆盖和 unreachable arm；超过 4096 元素的 fixed array 不再隐式穿透实现上限，而是要求不可反驳 arm；
无 guard 和字面量 true guard 计入覆盖，字面量 false 和动态 guard 不计入覆盖。动态 slice 不再只能靠 `[..]`
兜底，`[]` + `[_, ..]`、bool head partitions 等有限代表长度覆盖可被证明；开放整数域 literal 也进入
usefulness constructor，重复 literal arm 会被判为 unreachable，缺少剩余整数域时输出 open-domain wildcard 诊断。

当前 `build` 目录可能不是完整测试配置；可信验证应以 `tools/run_tests.sh` 重新 configure/build/ctest 为准。

## M2 当前短板

M2 的核心短板集中在语言地基，不在标准库规模：

- block statement 和 block expression 主体规则已统一；expression block 可完整承载普通 statement，并额外要求 final expression。
- const initializer 已补齐纯标量运算；当前仍没有函数调用、控制流表达式或完整 comptime。
- compound assignment 已补齐；`++` / `--` 自增自减语法已从 M2 基础语法移除，统一使用 `+= 1` / `-= 1`。
- 基础 range-for 已补齐为 `for i in range(end)` / `for i in range(start, end)` / `for i in range(start, end, step)`；当前仍没有容器迭代、slice 迭代或 iterator protocol。
- trailing separator 策略已冻结：圆括号/方括号列表允许 trailing comma，comma 分隔花括号列表允许但不强制最后一个 comma。
- 现代基础语法继续收口：ADT-first enum、enum multi-payload destructuring、array literal / repeat literal、slice type/expression、tuple/destructuring、function type / function pointer 和基础字面量体系已完成。
- default private 已完成：顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`；`export c fn` 仍强制 public。
- `pub fn` 返回类型已收紧为必须显式；private helper 仍可推导。
- lexer 已支持嵌套 `/* ... */` 块注释。
- enum 已支持 ADT-first 形态：普通 enum 可省略 base type 和 discriminant，tag 自动分配；多字段 payload 可用 `.case(a, b)` pattern 按字段解构；显式 `enum Status: u8 { ok = 0, err = 1 }` 仍作为 C-like/repr enum 形态保留。generic enum 已进入 M2 基线，`Option[T]` / `Result[T, E]` 这类类型参数 ADT 可被实例化和匹配。
- 数组、slice、tuple、函数指针和字面量基础语法已闭合：固定数组支持 `[1, 2, 3]` 和 `[0; 128]`，borrowed slice 支持 `[]const T` / `[]mut T` 以及 `a[l:r]`、`a[:r]`、`a[l:]`、`a[:]`；tuple 支持 `(A, B)` / `(A,)` 类型、`(a, b)` / `(a,)` 字面量、局部 `let (a, _) = value;` 解构和 tuple pattern；匿名 tuple 不支持直接字段访问，需要字段访问时使用 named struct；函数类型支持 `fn(i32) -> i32`、`extern c fn(*const u8, ...) -> i32`、函数名作为值、局部/参数/字段函数指针间接调用；字面量支持普通字符串、C 字符串、raw/multiline raw string、byte string、byte literal、Unicode scalar `char` 和整数/浮点类型后缀。
- 最小 `unsafe` 已落地：`unsafe { ... }` 可作为 statement 或 expression，`unsafe fn` 和 unsafe 函数指针类型会把调用限制在 unsafe context 内；raw pointer 解引用、`ptrcast`、`bitcast`、`ptrat`、`strraw` 都必须在 unsafe context 中使用。
- 泛型已支持最小非资源类 `where` capability predicate：`Sized`、`Eq`、`Ord`、`Hash`。`Copy` / `Drop` 等资源类约束、用户自定义 trait、associated type、const generic 和 trait object 仍暂缓。
- 泛型边界已扩展到 generic struct / function / enum / type alias，以及 `impl[T] Box[T]` 这类 owner generic impl；method-local generic parameter 仍不属于 M2。
- M1 的语言级 `noncopy` / `move` MVP 已从 M2 基线删除。当前先保留普通值语义和必要的数组/含数组类型限制；copy/drop/borrow/ownership 暂缓为后续资源语义专题。
- 最小 safe reference 已落地：`&T` / `&mut T`、`&place` / `&mut place`、reference 安全解引用、`&mut` 可写 place 检查、`&mut T` 到 `&T` 的只读化赋值，以及按 pointer-sized ABI lowering。raw pointer 解引用仍必须在 `unsafe` 中；borrow checker、lifetime、borrowed return、alias/resource 语义继续暂缓。
- `str` 已有语言级雏形，普通数组/slice 地基已落地，`strraw` 已纳入 unsafe；M2 no-std checked UTF-8 边界已冻结为 `strvalid(bytes) -> bool`、`strfromutf8(bytes) -> str` 和 `text[l:r]` checked slicing。`strfromutf8` 失败或 `str` 切片越界/落在 UTF-8 continuation byte 上时返回空 `str`，不会把无效输入包装成 `str`。更完整的 text API、Unicode scalar/grapheme 迭代和拥有型 `String` 仍后置到库层重建。

当前完整语法库存、已支持高级能力、未完成特性和基础语法优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

## 当前结论

M2 的正确目标是先把基础语法和核心语义做窄做稳，再谈标准库、自举和构建工具。当前编译器已经能支撑语言核心实验和 native 输出，但不应把 M1 的 std/selfhost 经验继续当作有效路线推进。

下一步最重要的是继续冻结 M2 语法基线。ADT-first enum、generic enum、enum multi-payload destructuring、数组值语法、slice type/expression、tuple/destructuring、function pointer / function type、字面量体系、最小 unsafe 边界、最小 safe reference、M2 pattern ergonomics、最小非资源类 `where` capability 和 `str` checked UTF-8 边界已经落地；pattern 当前支持 tuple match pattern、slice pattern、struct pattern、nested enum payload destructuring、局部 struct/slice/enum destructuring、binding or-pattern alternatives、`let ... else`、`if value is pattern` / `while value is pattern`，以及 if 表达式 pattern condition。结构化 match 穷尽检查已使用 pattern matrix / usefulness witness search 覆盖 bool、enum payload、tuple、struct、fixed array、open integer literal 和 dynamic slice，不再枚举笛卡尔积；guard 已区分无 guard、字面量 true/false 和动态表达式，动态 slice 长度和开放域 witness 已进入当前 M2.1 主线。
