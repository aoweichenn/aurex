# Aurex 文档

文档基线：**M19 Dyn Ownership Runtime IR / Verifier Preparation**，建立在已经收口的 M2 language-core-no-std、
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
实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user value、runtime ABI lowering 或 dynamic Drop dispatch。M18 已进一步新增 `DynOwnershipRuntimeBoundaryGate`、`DynOwnershipRuntimeBoundaryCheckpointFact`、`DynOwnershipRuntimeLoweringDesignGateFact` 和 `dyn_ownership_runtime_boundary_gate` project-level query，把 M17 facts 接入 query/cache/tooling/reuse/workspace index，并固定 future IR/verifier/runtime lowering prerequisites；M18 仍不实现任何标准库或 runtime surface。M19 已新增 `DynOwnershipRuntimeIrVerifierFact`、`FunctionDynOwnershipRuntimeIrVerifierFacts`、`function_dyn_ownership_runtime_ir_verifier_facts()`、`TraitObjectVTableLayout::destructor_slot_blocked` 和 `CleanupAbiPolicy::dynamic_erased_drop_blocked` blocked negative sentinel，把 M18 prerequisites 落成 verifier-visible IR facts 和负例矩阵；M19 仍不实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user values、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。

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
- [Aurex M7 Origin/Loan/Lifetime 设计三轮评审](../review/aurex_m7_design_three_round_review.md)
- [使用文档](usage.md)
- [版本文档](version.md)
- [下一步计划文档](next-steps.md)
