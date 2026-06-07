# Aurex 文档

文档基线：**M8 Dyn Trait、Erased View 与动态派发 release closure**，建立在已经收口的 M2 language-core-no-std、
M2.5 frontend-foundation、M3 query-backed/module/generic、M4 trait/protocol 和 M5 default trait methods
基线、M6 资源/cleanup/drop-glue release baseline、M7a CFG-sensitive borrow facts、M7b borrow contract /
reborrow / two-phase receiver、M7c lifetime/storage escape、M7d-B struct field place-state 以及 M7d-C RAII
user surface 实现收口基线之上。M7 Hardening Performance Closure 和 M7d-K Array Repeat Resource Safety Closure
也已完成；M8 已完成 borrowed dyn trait / erased view 的 query 地基、frontend syntax/sema、borrowed dyn
coercion、checked vtable facts、IR/backend runtime dynamic dispatch、M8e hardening 和 sample / release polish
收口。M8 已正式封口；下一条主线是 M9 dyn ABI / tooling 设计阶段。

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
- [Aurex M7 Origin/Loan/Lifetime 设计三轮评审](../review/aurex_m7_design_three_round_review.md)
- [使用文档](usage.md)
- [版本文档](version.md)
- [下一步计划文档](next-steps.md)
