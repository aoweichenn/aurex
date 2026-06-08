# Aurex M11 Advanced Dyn Design Baseline

日期：2026-06-08

状态：**M11b Principal-Set Composition Query Prototype Gate 已完成**。本阶段仍是 compiler/query facts
baseline，不是 parser/sema、IR/backend runtime 或标准库实现阶段。

## 1. 阶段结论

M8 已完成 borrowed dyn trait runtime dispatch，M9 已完成 dyn ABI/tooling facts，M10 已完成 borrowed dyn
supertrait upcasting release baseline。M11a 的任务不是继续往 M10 追加 runtime feature，而是从 M9c
`DynAdvancedDesignGate` 剩余候选中选择下一条 advanced dyn 主线，并把后续实现必须遵守的 semantic model、
metadata policy、query facts、diagnostics、verifier/backend negative matrix 和非目标固定下来。

M11a 的选择是：

**principal-set borrowed dyn composition**。

更完整地说，M11 主线应设计为 Aurex 自己的 **origin-bound borrowed dyn principal-set view**，不是照搬
Rust trait object、Swift existential、Go interface type set 或 C++ vtable embedding。它继续复用 M7 origin/loan
事实、M8 borrowed erased view、M9 dyn ABI/tooling facts 和 M10 `supertrait_vptr_metadata_v1` upcast projection；
但它必须新增 composition 专用 metadata policy：`principal_set_metadata_v1`。

M11a 先落地 query-facing gate：

- 新增 `completed_release_baseline` stage，用来标记 M10 supertrait upcasting 已经完成，不在 M11a 重开。
- 新增 `m11a_dyn_advanced_design_gate_baseline()`，把 `multi_trait_composition` 选为后续 `ready_for_future_stage`。
- M11a baseline 仍保留 `owning_dyn`、`dynamic_drop_dispatch` 和 `allocator_policy` 的 blocked 状态。
- Gate validation 要求五个 advanced capability 各出现一次，避免候选缺失或重复时仍被误判为有效 baseline。
- Summary/dump/fingerprint 暴露 selected principal-set direction、completed M10 baseline 和 M11a stage counts。

M11b 在该选择之上落地纯 query facts 原型：`PrincipalSetCompositionFacts`、
`PrincipalSetIdentityFact`、`CompositionWitnessSetFact`、`PrincipalMethodNamespaceFact`、
`AssociatedEqualityMergeFact`、`CompositionProjectionFact`、`principal_set_composition_facts_fingerprint()`、
`summarize_principal_set_composition_facts()` 和 `dump_principal_set_composition_facts()`。

一句话：M11b 已把 **borrowed multi-principal composition** 的 facts/query 地基落到代码，但 M11b 不实现
`dyn A + B` parser syntax、不实现 composition sema、不实现 IR/backend runtime、不实现标准库。

## 2. 当前能做什么

当前仓库已经能稳定使用的 dyn 能力仍然来自 M8-M10：

- `&dyn Trait` / `&mut dyn Trait` borrowed erased view。
- `dyn Trait[Assoc = Type]` associated equality dispatch。
- Trait default method slot 进入 checked vtable。
- `&T -> &dyn Trait` / `&mut T -> &mut dyn Trait` borrowed dyn coercion。
- `{data*, vtable*}` borrowed fat view lowering。
- Vtable slot indirect dispatch。
- `trait Child: Parent` direct supertrait declaration。
- `&dyn Child -> &dyn Parent`、`&mut dyn Child -> &mut dyn Parent` 和
  `&mut dyn Child -> &dyn Parent` borrowed dyn supertrait upcast。
- `trait_object_upcast` IR 和 `supertrait_vptr_metadata_v1` parent vtable projection。
- `FunctionDynAbiFacts`、`DynUpcastAbiDescriptor`、summary/dump/fingerprint、lower-IR invalidation 和 IDE hover
  对 dyn ABI / upcast facts 的 tooling projection。

M11a/M11b 新增的是设计和 query facts 能力，而不是用户可写语言语法：

- 可以通过 `m11a_dyn_advanced_design_gate_baseline()` 查询 M11a 设计选择。
- 可以通过 `is_valid_m11a_dyn_advanced_design_gate()` 防止 M11a boundary drift。
- 可以在 `dump_dyn_advanced_design_gate()` 中看到 `principal_set_metadata_v1`、`ready_for_future_stage`、
  `completed_release_baseline`、required facts 和 non-goals。
- 可以用 documentation tests 固定 “M11a 不实现标准库，不实现 owning/runtime feature” 的阶段边界。
- 可以通过 `PrincipalSetCompositionFacts` 在 query/tooling 层表达 principal-set identity、witness set、
  principal-qualified method namespace、associated equality merge 和 composition projection。
- 可以通过 `principal_set_identity_fact()` 对 principal descriptors 做 canonical order、same-origin 和 duplicate
  validation，并生成稳定 principal-set identity。
- 可以通过 `is_valid(const PrincipalSetCompositionFacts&)` 拒绝 mixed origin、single principal、duplicate principal、
  missing identity、stale summary、stale fingerprint、flattened method namespace、associated equality conflict shape
  drift、projection 改变 data pointer/origin 和 invalid metadata policy。
- 可以通过 summary/dump/fingerprint 固定 `principal_set_metadata_v1`、`ambiguous_requires_principal`、
  `composition_to_supertrait` 等 tooling-facing 事实。

当前还不能写：

- `dyn A + B` 或任何多 principal trait object 语法。
- owning dyn、`Box<dyn Trait>`、side allocation 或 allocator-driven dyn container。
- trait-object Drop dispatch、dynamic destructor ABI、dynamic cleanup runtime call。
- 标准库容器、标准库 `Box`、标准库 allocator API。
- auto trait composition、structural conformance、Swift-style owning existential value buffer。

M11b 当前也不能做：

- parser/AST/sema 中的 principal-set composition type。
- concrete-to-composition coercion 的用户级诊断。
- principal-set method call resolution。
- IR `trait_object_composition` value、LLVM principal-set metadata global 或 runtime dispatch。
- 标准库、owning dyn、`Box<dyn Trait>`、allocator API/policy 或 dynamic Drop dispatch。

## 3. 为什么选择 principal-set composition

M9c 剩余候选中有四类大方向：multi trait composition、dynamic Drop dispatch、owning dyn / `Box<dyn Trait>`、
allocator policy。M11a 选择 multi trait composition 的原因是它最符合当前 Aurex 的阶段边界。

### 3.1 适合当前阶段的原因

principal-set borrowed dyn composition 可以继续站在已完成的 compiler/query/tooling 地基上：

- M7 已有 origin/loan/lifetime facts，可以表达 borrowed view 不延长来源、不创建 ownership。
- M8 已有 borrowed dyn object identity、checked vtable witness 和 method slot dispatch。
- M9 已有 library-independent dyn ABI DTO、fingerprint、summary/dump 和 tooling projection。
- M10 已有 supertrait edge metadata 和 borrowed dyn-to-dyn projection runtime。

因此 M11b/M11c 可以先做 parser/query/sema check-only 和 tooling facts，不需要在第一步引入标准库、allocator、
owning container、Drop dispatch 或 destructor ABI。

### 3.2 其他候选为什么后移

`owning_dyn` 和 `Box<dyn Trait>` 必须回答所有权、移动、销毁、allocator、layout、panic/unwind cleanup 和标准库 API
问题。当前阶段明确不实现标准库；因此它们继续由 `requires_standard_library_stage` 阻塞。

`allocator_policy` 也必须有标准库或 runtime ownership policy。把 allocator 信息塞进 borrowed view 会让
`&dyn Trait` 这种不拥有对象的 view 背上错误责任，所以它继续后移。

`dynamic_drop_dispatch` 需要 destructor slot、drop glue metadata、dynamic cleanup runtime ABI、dropck/tooling facts
和资源模型联动。M7 当前对 generic/associated/opaque cleanup 仍是 marker-only；M11a 不应把 destructor slot 混进
`principal_set_metadata_v1` 或 M10 的 `supertrait_vptr_metadata_v1`。

## 4. 参考模型取舍

M11a 借鉴成熟语言的动机，但不复制它们的对象模型。

Rust 的 trait object 给出重要边界：显式 `dyn`、object-callability、associated equality、upcast/coercion 边界和
metadata policy 不能混淆。Aurex 采纳 “显式 dyn + object-callability + 不隐藏 widening” 的原则，但不复制 Rust
lifetime surface、`Box<dyn Trait>` 标准库路径、auto trait set 或完整 vtable entry shape。

Swift 的 existential / protocol composition 说明 witness sets 和 composition identity 对 tooling 很有价值。但 Swift
existential 经常涉及 owning value buffer、copy/destroy/value witness 和 runtime opening；Aurex 当前不实现 owning
existential，因此只采纳 “composition witness set 可查询” 的部分。

Go 的 interface type set 强调方法集合和组合接口的可用性。但 Go 是结构化 conformance；Aurex 继续保持 nominal
trait 和 explicit `impl Trait for Type`，不把实现关系改成 structural matching。

C++ ABI 和 Itanium vtable 经验说明 metadata layout 一旦进入 ABI 就必须 versioned、verified、可 dump、可
fingerprint。Aurex 采纳 “metadata policy versioning + verifier negative matrix” 的原则，但不把 vptr 嵌入 concrete
object，也不把 borrowed dyn view 改成 C++ class object。

## 5. 语义模型

M11 的 future source surface 可以考虑 borrowed composition view，例如 `&dyn A + B`、`&dyn (A + B)` 或显式
composition type spelling。M11a 不冻结最终 parser spelling，只冻结语义模型。

### 5.1 Principal Set

composition object 的核心 identity 不是单一 principal trait，而是 canonical principal set：

- principal set 至少包含一个 principal trait。
- 每个 principal trait 必须是 nominal trait，并且必须有可见 checked impl evidence。
- principal set 中不能有重复 principal。
- canonical order 必须稳定，不能依赖源码遍历偶然顺序；建议后续使用 trait `DefKey` / canonical trait args /
  associated equality fingerprint 排序。
- 如果一个 principal 是另一个 principal 的 supertrait，仍不能简单删掉或合并；是否允许冗余 principal 要由
  `principal_set_identity_fact` 和 diagnostics 明确决定。
- `dyn A` 不是 `dyn A + B` 的同一 identity；composition 不能伪装成单 principal object。

### 5.2 Borrowed View

M11 principal-set composition 继续是 origin-bound borrowed view：

- view 不拥有 concrete object。
- view 不分配内存。
- view 不复制 concrete object。
- view 不创建 cleanup obligation。
- view 不延长 source origin。
- view 不放宽 active loan。
- shared source 不能升级成 mutable composition target。
- `&mut` source 可以产生 shared composition target，但 target 只能拥有 shared 权限。

这保持了 M8/M10 的借用权语义：composition 是 view/projection，不是 owner/container。

### 5.3 Witness Set

composition view 需要 `composition_witness_set_fact` 表达每个 principal 对应的 witness：

- concrete type 到每个 principal trait 的 checked vtable witness。
- 每个 principal 的 method slot schema。
- 每个 principal 与 supertrait projection 的 relation。
- 每个 principal 的 associated equality substitutions。
- composition metadata policy version。

该 witness set 是 query/tooling 表面，不应由 backend dump 文本反推。

### 5.4 Method Namespace

M11 不允许把多个 principal 的 methods 直接 flatten 到一个 slot namespace。必须先建立
`principal_method_namespace_fact`：

- Method lookup 首先在 source expression 的 visible composition type 上解析。
- 如果多个 principal 暴露同名 callable method，必须要求显式 disambiguation 或给出 ambiguity diagnostic。
- Slot ordinal 必须是 principal-qualified，例如 `(principal=A, slot=0)`，而不是 composition-wide `slot=0`。
- Supertrait methods 必须通过 M10 supertrait projection 进入对应 principal namespace，不能绕过
  `supertrait_vptr_metadata_v1`。

这就是 `do_not_flatten_method_slots_without_principal_namespace` non-goal 的原因。

### 5.5 Associated Equality Merge

多个 principal 同时出现时，associated equality 不能靠字符串合并。必须有
`associated_equality_merge_fact`：

- 同一 associated member 从不同 principal path 进入时，canonical type 必须一致，否则报 ambiguity/conflict。
- 未约束 associated type 仍不能进入 callable dyn slot。
- 泛型 principal args 和 associated equality substitutions 必须在 principal set identity 中稳定 fingerprint。
- supertrait path 带来的 associated equality 必须和 direct principal equality 统一检查。

M10 已支持 generic parent args substitution，但完整 associated equality edge mapping / ambiguity solver 仍是后续项；
M11 composition 不能假装这部分已经完成。

### 5.6 Projection

composition view 必须有 `composition_projection_fact`：

- concrete borrowed reference 可以 coercion 到 principal-set composition view。
- composition view 可以投影到其中一个 principal borrowed dyn view。
- composition view 可以通过 M10 edge 投影到某个 principal 的 supertrait view。
- projection 不改变 data pointer，不改变 origin，不改变 borrow permission。
- projection 必须参与 dyn ABI summary/dump/fingerprint 和 lower-IR invalidation。

## 6. ABI 与 query policy

M11a 固定新 metadata policy 名称：`principal_set_metadata_v1`。

这个 policy 的边界：

- 它不是 `borrowed_methods_only_v1` 的隐式扩展。
- 它不是 `supertrait_vptr_metadata_v1` 的替代品。
- 它不包含 drop/size/align/type metadata。
- 它不包含 allocator 或 owner container metadata。
- 它不包含 destructor slot。
- 它描述 principal-set composition metadata 和 principal-qualified witness/projection。

M11a gate 中要求的事实：

- `principal_set_identity_fact`
- `composition_witness_set_fact`
- `principal_method_namespace_fact`
- `associated_equality_merge_fact`
- `composition_projection_fact`

M11a gate 中固定的 non-goals：

- `standard_library_runtime_not_in_m11a`
- `runtime_dispatch_not_in_m11a`
- `owning_dyn_runtime_not_in_m11a`
- `do_not_encode_principal_set_as_single_trait_object`
- `do_not_flatten_method_slots_without_principal_namespace`
- `do_not_reopen_m10_supertrait_runtime_in_m11a`
- `do_not_add_destructor_slot_to_principal_set_metadata_v1`
- `do_not_allocate_for_principal_set_dyn_view`

## 7. Gate baseline

`m11a_dyn_advanced_design_gate_baseline()` 的五个 candidates：

| Capability | Stage | Decision | Policy / Blocker |
| --- | --- | --- | --- |
| `supertrait_upcasting` | `completed_release_baseline` | `requires_new_metadata_policy` | M10 已完成，metadata 为 `supertrait_vptr_metadata_v1` |
| `multi_trait_composition` | `ready_for_future_stage` | `requires_new_metadata_policy` | 选择为 M11 主线，metadata 为 `principal_set_metadata_v1` |
| `owning_dyn` | `prototype_blocked` | `requires_standard_library_stage` | 标准库/owner/runtime 阻塞 |
| `dynamic_drop_dispatch` | `prototype_blocked` | `requires_runtime_stage` | runtime/destructor ABI 阻塞 |
| `allocator_policy` | `prototype_blocked` | `requires_standard_library_stage` | 标准库/allocator API 阻塞 |

Validation 要求：

- Gate name 必须是 `M11a Advanced Dyn Design Baseline`。
- 五个 advanced capability 必须各出现一次。
- 每个 candidate 的 required policy、impact summary、decision、stage、required facts 和 non-goals 必须匹配 M11a
  baseline。
- M11a composition candidate 不能把 metadata policy 改回 `borrowed_methods_only_v1`。
- M11a composition candidate 不能要求 runtime。
- M11a 不能重开 M10 supertrait runtime。

## 8. M11b Query Prototype Gate

M11b 已完成 principal-set composition 的 query facts 原型，代码入口是：

- `include/aurex/infrastructure/query/principal_set_composition_facts.hpp`
- `src/infrastructure/query/principal_set_composition_facts.cpp`
- `tests/gtest/infrastructure/query/principal_set_composition_facts_tests.cpp`

M11b 新增的事实 DTO：

- `PrincipalSetCompositionFacts`：单个 subject 下的 aggregate facts，包含 summary 和 fingerprint。
- `PrincipalSetIdentityFact`：principal-set identity、object origin、metadata policy 和 canonical principal list。
- `PrincipalSetPrincipalDescriptor`：单个 principal 的 `TraitObjectTypeKey` 与展示名。
- `CompositionWitnessSetFact`：principal-set 对应的 checked witness set。
- `CompositionWitnessDescriptor`：principal object、`VTableLayoutKey`、witness fingerprint 和 concrete type 名称。
- `PrincipalMethodNamespaceFact` / `PrincipalMethodNamespaceEntry`：principal-qualified method namespace。
- `AssociatedEqualityMergeFact`：associated type equality merge、conflict 或 unconstrained 状态。
- `CompositionProjectionFact`：concrete-to-composition、composition-to-principal 和 composition-to-supertrait projection。
- `PrincipalSetCompositionSummary`：principal/witness/method/equality/projection 计数。

M11b 新增的 policy/status/kind：

- `PrincipalSetMetadataPolicy::principal_set_metadata_v1`
- `PrincipalMethodNamespaceStatus::unique_principal_method`
- `PrincipalMethodNamespaceStatus::ambiguous_requires_principal`
- `PrincipalAssociatedEqualityMergeStatus::satisfied`
- `PrincipalAssociatedEqualityMergeStatus::conflict`
- `PrincipalAssociatedEqualityMergeStatus::unconstrained`
- `PrincipalSetProjectionKind::concrete_to_composition`
- `PrincipalSetProjectionKind::composition_to_principal`
- `PrincipalSetProjectionKind::composition_to_supertrait`

M11b 新增的查询 API：

- `principal_set_identity_fact(std::span<const PrincipalSetPrincipalDescriptor>)`
- `record_principal_set_identity_fact`
- `record_composition_witness_set_fact`
- `record_principal_method_namespace_fact`
- `record_associated_equality_merge_fact`
- `record_composition_projection_fact`
- `summarize_principal_set_composition_counts`
- `principal_set_composition_facts_fingerprint`
- `summarize_principal_set_composition_facts`
- `dump_principal_set_composition_facts`

M11b validation 固定以下边界：

- principal set 至少包含两个 principal，且 principal 必须是有效 `TraitObjectTypeKey`。
- principal set 中所有 principal 必须共享同一个 `object_origin`。
- principal descriptor 必须 canonical ordered 且 duplicate principal 会被拒绝。
- composition metadata policy 只接受 `principal_set_metadata_v1`。
- witness descriptor 必须把 principal object 与 vtable layout 的 object type 对齐。
- witness set 必须 canonical sorted、unique，且至少覆盖两个 principal。
- method namespace 不允许把多个 principal 的同名 method flatten 成 `unique_principal_method`；冲突必须标成
  `ambiguous_requires_principal`。
- associated equality merge 必须使用 associated-type `MemberKey`；`unconstrained` 不能带 merged type；
  contributor principals 必须 valid、sorted、unique。
- projection 必须保持 data pointer 和 origin，不允许 invalid borrow kind，不允许 composition-to-supertrait 投影回同一 principal。
- aggregate facts 必须引用已记录的 identity，summary 必须与 facts 重新计算结果一致，非空 fingerprint 必须与当前 facts
  重新计算结果一致。

M11b focused tests 覆盖 enum name、invalid fallback、principal-set identity canonicalization、single/duplicate/
mixed-origin principal rejection、metadata policy drift、witness layout mismatch、associated equality shape drift、
projection boundary drift、flattened method namespace rejection、summary/dump/fingerprint 稳定性、unknown identity、
stale summary 和 stale fingerprint rejection。

M11b 仍明确不实现标准库、不实现 `dyn A + B` parser syntax、不实现 parser/AST/sema、不实现 IR/backend runtime、
不实现 owning dyn、不实现 `Box<dyn Trait>`、不实现 allocator API/policy、不实现 dynamic Drop dispatch。

## 9. Diagnostics 计划

M11c/M11d 实现时，用户级 diagnostics 需要至少覆盖：

- duplicate principal in composition。
- principal trait 没有 visible nominal impl。
- composition 中多个 principal 暴露同名 method 且无法自动决定调用目标。
- associated equality conflict。
- 未约束 associated type 出现在 callable slot。
- shared-to-mut composition coercion。
- composition projection target 不在 principal set 或 supertrait closure 中。
- attempted owning dyn / `Box<dyn Trait>` / allocator / dynamic Drop dispatch in M11 compiler-only stage。

Diagnostics 不能只说 “invalid dyn composition”。它应指出 principal set、冲突 method/equality、source coercion site、
目标 principal 或被禁止的 owner/runtime boundary。

## 10. Verifier 与 backend negative matrix

M11a 不实现 IR/backend runtime，但后续必须提前保留 negative matrix：

- principal set metadata policy mismatch。
- duplicate principal descriptor。
- missing principal witness。
- principal-qualified slot ordinal 越界。
- flattened slot namespace 被错误使用。
- composition projection target 不属于 principal set。
- supertrait projection 绕过 M10 `supertrait_vptr_metadata_v1`。
- composition view 改变 data pointer。
- composition projection 改变 borrow kind 或 origin。
- dynamic Drop slot 被错误塞入 `principal_set_metadata_v1`。
- allocator/owner metadata 被错误塞入 borrowed composition view。

这些 verifier/backend tests 应在 M11d runtime package 前就通过 query/sema/IR focused negative tests 分批铺好。

## 11. 测试和覆盖率要求

覆盖率门槛按当前仓库要求为 90%，但不能为了覆盖率降低测试质量。

M11a 已覆盖：

- enum name / invalid fallback。
- M9c gate 兼容。
- M11a baseline selection。
- boundary drift rejection。
- duplicate capability rejection。
- summary/dump/fingerprint exposure。
- documentation tests 固定 M11a 设计和非目标。

M11c/M11d 应继续按功能模块拆测试，不把所有 dyn 测试塞回一个巨大文件：

- query DTO / validation tests。
- parser syntax tests，等最终 spelling 选定后再加。
- sema coercion / method lookup / associated equality tests。
- checked dump / fingerprint tests。
- IDE/tooling projection tests。
- negative samples。
- IR/verifier focused tests。
- backend/native tests 只在 runtime package 开始后加入。

## 12. 明确非目标

M11b 明确不做：

- 不实现标准库。
- 不实现 `Box<dyn Trait>`。
- 不实现 owning dyn。
- 不实现 dynamic Drop dispatch。
- 不实现 allocator API。
- 不实现 trait-object destructor ABI。
- 不实现 `dyn A + B` parser syntax。
- 不实现 principal-set sema coercion。
- 不实现 IR/backend runtime dispatch。
- 不实现 side allocation、owner container 或 value buffer。
- 不实现 structural conformance。
- 不把 composition 偷偷塞进 `borrowed_methods_only_v1`。
- 不把 dynamic Drop slot 偷偷塞进 `principal_set_metadata_v1`。

这些能力不是永久禁止，而是必须在后续阶段单独设计、估算、测试和 release。

## 13. 后续阶段和代码量预估

M11b 预估是 800-1,400 行。实际完成后应以 commit diffstat 为准；若偏离预估，主要原因通常是文档入口、
documentation tests、query validation 强度和 focused tests 数量。当前 M11b 实现仍保持 compiler/query facts
边界，因此没有 parser/sema/IR/backend/runtime 代码量。

M11 剩余阶段建议：

| 阶段 | 内容 | 预计新增/修改代码量 |
| --- | --- | ---: |
| M11b Principal-Set Composition Query Prototype Gate | 已完成。新增 principal-set composition query DTO、stable fingerprint、summary/dump、validation 和 focused query tests；仍不实现 parser/sema/runtime | 实际以本次 diffstat 为准 |
| M11c Principal-Set Composition Frontend / Sema Check-Only | 最终选择 source spelling，parser/AST、type identity、coercion check、method namespace diagnostics、associated equality merge check、checked dump/fingerprint、negative samples | 1,800-3,200 行 |
| M11d Principal-Set Composition IR / Backend Runtime | IR value/layout/projection、verifier、LLVM metadata layout、principal-qualified slot dispatch、native runtime tests；仍不实现 owning dyn/stdlib | 1,600-2,800 行 |
| M11e Hardening / Release Closure | query/cache/tooling polish、IDE hover/workspace index、stress/perf gates、docs/tests release closure、代码量偏差分析 | 700-1,300 行 |
| 后续标准库/owning/drop 阶段 | `Box`、owning dyn container、allocator API、dynamic Drop dispatch、destructor ABI、standard-library resource wrappers | 独立设计后估算 |

M11c 是 M11b 之后最合适的下一步，因为 query facts 已经固定，下一步应在不实现标准库和 runtime 的前提下把
source spelling、type identity、coercion check、method namespace diagnostics 和 associated equality merge check 做成
frontend/sema check-only 子集。
