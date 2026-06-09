# Aurex 文档

文档基线：**M11e Principal-Set Composition Hardening / Release Closure**，建立在已经收口的 M2 language-core-no-std、
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
documentation tests 已固定 release boundary。当前仍不实现标准库、owning dyn runtime、`Box<dyn Trait>`、
dynamic Drop dispatch runtime、allocator policy、bare `dyn A + B` syntax 或 direct principal-qualified composition
method dispatch；下一步是 M12 advanced dyn design，且仍不引入标准库或 owning dyn。

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
- [Aurex M7 Origin/Loan/Lifetime 设计三轮评审](../review/aurex_m7_design_three_round_review.md)
- [使用文档](usage.md)
- [版本文档](version.md)
- [下一步计划文档](next-steps.md)
