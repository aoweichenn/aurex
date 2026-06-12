#include <support/test_support.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::test {
namespace {

void expect_document_contains(const fs::path& path, const std::string_view text)
{
    const std::string document = read_text(source_root() / path);
    EXPECT_NE(document.find(text), std::string::npos) << path << " missing " << text;
}

} // namespace

TEST_F(AurexIntegrationTest, DocumentationLayoutIsStable)
{
    const std::vector<fs::path> required = {
        "docs/README.md",
        "docs/zh/README.md",
        "docs/zh/architecture.md",
        "docs/zh/requirements.md",
        "docs/zh/runtime-flow.md",
        "docs/zh/api.md",
        "docs/zh/language-manual.md",
        "docs/zh/regex.md",
        "docs/zh/implementation.md",
        "docs/zh/usage.md",
        "docs/zh/introduction.md",
        "docs/zh/version.md",
        "docs/zh/next-steps.md",
        "docs/zh/progress.md",
        "docs/zh/m3-roadmap.md",
        "docs/zh/m3.9-m3-release-baseline.md",
        "docs/zh/m4-roadmap.md",
        "docs/zh/m4-trait-protocol-system-design.md",
        "docs/zh/m4-release-baseline.md",
        "docs/zh/m5-roadmap.md",
        "docs/zh/m5-default-trait-methods-design.md",
        "docs/zh/m5-release-baseline.md",
        "docs/zh/m6-roadmap.md",
        "docs/zh/m6-resource-access-semantics-design.md",
        "docs/zh/m7-origin-loan-lifetime-design.md",
        "docs/zh/m7-roadmap.md",
        "docs/zh/m7b-borrow-contract-design.md",
        "docs/zh/m7b-roadmap.md",
        "docs/zh/m7c-m7d-complete-borrow-raii-design.md",
        "docs/zh/m7-hardening-performance-closure.md",
        "docs/zh/m8-dyn-trait-design.md",
        "docs/zh/m9-dyn-abi-tooling-design.md",
        "docs/zh/m9-release-baseline.md",
        "docs/zh/m10-supertrait-upcasting-design.md",
        "docs/zh/m10-release-baseline.md",
        "docs/zh/m11-advanced-dyn-design.md",
        "docs/zh/m11-release-baseline.md",
        "docs/zh/m12-release-baseline.md",
        "docs/zh/m13-advanced-dyn-design.md",
        "docs/zh/m14-borrowed-dyn-view-path-release.md",
        "docs/zh/m15-advanced-dyn-const-generic-design.md",
        "docs/zh/m16-const-generic-check-only-release.md",
        "docs/zh/m17-dyn-ownership-runtime-prep-release.md",
        "docs/zh/m18-dyn-ownership-runtime-boundary-hardening-release.md",
        "docs/zh/m19-dyn-ownership-runtime-ir-verifier-prep-release.md",
        "docs/zh/m20-owned-dyn-runtime-admission-gate-release.md",
        "docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "docs/zh/m20-owned-dyn-drop-allocator-identity-release.md",
        "docs/zh/m20-owned-dyn-runtime-lowering-abi-design-release.md",
        "docs/zh/m21-macro-system-design-gate.md",
        "docs/zh/m21b-attribute-token-tree-surface.md",
        "docs/zh/m21c-early-item-macro-expansion-plan.md",
        "docs/zh/m21d-noop-early-item-expansion-boundary.md",
        "docs/en/README.md",
        "docs/en/architecture.md",
        "docs/en/requirements.md",
        "docs/en/runtime-flow.md",
        "docs/en/api.md",
        "docs/en/implementation.md",
        "docs/en/usage.md",
        "docs/en/introduction.md",
        "docs/en/version.md",
        "docs/en/next-steps.md",
        "docs/en/progress.md",
        "docs/en/m3-roadmap.md",
        "docs/en/m3.9-m3-release-baseline.md",
        "docs/en/m4-roadmap.md",
        "docs/en/m4-trait-protocol-system-design.md",
        "docs/en/m4-release-baseline.md",
        "docs/en/m5-roadmap.md",
        "docs/en/m5-default-trait-methods-design.md",
        "docs/en/m5-release-baseline.md",
        "docs/en/m6-roadmap.md",
        "docs/en/m6-resource-access-semantics-design.md",
    };
    for (const fs::path& path : required) {
        EXPECT_TRUE(fs::exists(source_root() / path)) << path;
    }

    const std::vector<fs::path> obsolete = {
        "docs/ARCHITECTURE.zh.md",
        "docs/DESIGN.en.md",
        "docs/DESIGN.zh.md",
        "docs/SELFHOST.md",
        "docs/SEMANTICS.md",
        "docs/USAGE.en.md",
        "docs/USAGE.zh.md",
    };
    for (const fs::path& path : obsolete) {
        EXPECT_FALSE(fs::exists(source_root() / path)) << path;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(source_root() / "docs")) {
        EXPECT_FALSE(entry.path().filename().string().rfind("M0V0.1.", 0) == 0) << entry.path();
    }
}

TEST_F(AurexIntegrationTest, M4ReleaseDocumentationIsClosed)
{
    expect_document_contains("docs/en/m4-roadmap.md", "M4-WP1 through WP8 are complete");
    expect_document_contains("docs/zh/m4-roadmap.md", "M4-WP1 到 WP8 已完成");
    expect_document_contains("docs/en/m4-release-baseline.md", "M4-WP1 through M4-WP8 are complete");
    expect_document_contains("docs/zh/m4-release-baseline.md", "M4-WP1 到 M4-WP8 已全部完成");
    expect_document_contains("docs/en/progress.md", "M4 trait/protocol work has completed WP1");
    expect_document_contains("docs/zh/progress.md", "M4 trait/protocol 系统已完成 WP1");
    expect_document_contains("docs/en/next-steps.md", "M4-WP8 is complete");
    expect_document_contains("docs/zh/next-steps.md", "M4-WP8 已完成");
}

TEST_F(AurexIntegrationTest, M5ReleaseDocumentationIsClosed)
{
    expect_document_contains("docs/en/README.md", "M5 Default Trait Methods Roadmap");
    expect_document_contains("docs/zh/README.md", "M5 Default Trait Methods 路线图");
    expect_document_contains("docs/en/README.md", "M5 Default Trait Methods Release Baseline");
    expect_document_contains("docs/zh/README.md", "M5 Default Trait Methods Release Baseline");
    expect_document_contains("docs/en/progress.md", "M5 has closed as the default trait methods release baseline");
    expect_document_contains("docs/zh/progress.md", "M5 已作为 default trait methods release baseline 收口");
    expect_document_contains("docs/en/next-steps.md", "Closed Background: Post-M5 Design Selection");
    expect_document_contains("docs/zh/next-steps.md", "已收口背景：Post-M5 Design Selection");
    expect_document_contains("docs/en/m5-roadmap.md", "M5-WP1: Research And Design Baseline");
    expect_document_contains("docs/zh/m5-roadmap.md", "M5-WP1：调研和设计基线");
    expect_document_contains("docs/en/m5-roadmap.md", "M5-WP1 through M5-WP7 are complete");
    expect_document_contains("docs/zh/m5-roadmap.md", "M5-WP1 到 M5-WP7 已完成");
    expect_document_contains("docs/en/m5-roadmap.md", "Status: complete.");
    expect_document_contains("docs/zh/m5-roadmap.md", "状态：已完成。");
    expect_document_contains("docs/en/m5-default-trait-methods-design.md", "TraitMethodDispatchKind");
    expect_document_contains("docs/zh/m5-default-trait-methods-design.md", "TraitMethodDispatchKind");
    expect_document_contains("docs/en/m5-release-baseline.md", "M5-WP1 through M5-WP7 are complete");
    expect_document_contains("docs/zh/m5-release-baseline.md", "M5-WP1 到 M5-WP7 已全部完成");
    expect_document_contains("docs/en/usage.md", "method bodies inside traits");
    expect_document_contains("docs/zh/usage.md", "default method body");
}

TEST_F(AurexIntegrationTest, M6ResourceSemanticsWp2ThroughWp7AreClosed)
{
    expect_document_contains("docs/en/README.md", "M6 Resource, Value Lifetime, And Access Semantics Roadmap");
    expect_document_contains("docs/zh/README.md", "M6 资源、值生命周期与访问语义路线图");
    expect_document_contains("docs/en/progress.md",
        "Stage: M6-WP2 through M6-WP7 resource, cleanup, drop-glue, tooling, and release closure complete");
    expect_document_contains("docs/zh/progress.md", "M6-WP2 到 M6-WP7 已完成 M6 实现基线");
    expect_document_contains("docs/en/next-steps.md", "Current Highest Priority: M6 Resource And Access Semantics");
    expect_document_contains("docs/zh/next-steps.md", "M6-WP2 到 M6-WP7 已完成 M6 实现基线");
    expect_document_contains(
        "docs/en/version.md", "M6-WP2/WP3/WP4/WP5/WP6/WP7 Resource, Cleanup, Drop-Glue, And Tooling Baseline");
    expect_document_contains(
        "docs/zh/version.md", "M6-WP2/WP3/WP4/WP5/WP6/WP7 资源、cleanup、drop-glue 与 tooling 基线");
    expect_document_contains(
        "docs/en/requirements.md", "The active implementation baseline is M6 Resource And Access Semantics");
    expect_document_contains("docs/zh/requirements.md",
        "M6 Resource And Access Semantics 这条旧 release baseline 仍作为资源语义设计入口保留");
    expect_document_contains("docs/en/m6-roadmap.md", "M6-WP1: Three-Pass Research And Design Review");
    expect_document_contains("docs/zh/m6-roadmap.md", "M6-WP1：三轮调研和设计审视");
    expect_document_contains("docs/en/m6-roadmap.md", "Status: complete.");
    expect_document_contains("docs/zh/m6-roadmap.md", "状态：已完成。");
    expect_document_contains("docs/en/m6-roadmap.md", "M6-WP3: Owned Use Modes And Whole-Local Move Analysis");
    expect_document_contains("docs/zh/m6-roadmap.md", "M6-WP3：Owned Use Mode 和 Whole-Local Move Analysis");
    expect_document_contains("docs/en/m6-roadmap.md", "M6-WP4: Cleanup Obligations");
    expect_document_contains("docs/zh/m6-roadmap.md", "M6-WP4：Cleanup Obligations");
    expect_document_contains("docs/en/m6-roadmap.md", "Status: complete for the M6 baseline.");
    expect_document_contains("docs/zh/m6-roadmap.md", "状态：M6 基线已完成。");
    expect_document_contains("docs/en/m6-roadmap.md", "M6-WP7: Release Closure And M7 Entry");
    expect_document_contains("docs/zh/m6-roadmap.md", "M6-WP7：Release Closure 和 M7 入口");
    expect_document_contains("docs/en/next-steps.md", "The next implementation package is M7 CFG-Sensitive");
    expect_document_contains("docs/zh/next-steps.md", "M7 继续以 M6 cleanup 和 resource facts 为输入");
    expect_document_contains("docs/en/m6-resource-access-semantics-design.md", "`NeedsDrop`");
    expect_document_contains("docs/zh/m6-resource-access-semantics-design.md", "`NeedsDrop`");
    expect_document_contains("docs/en/m6-resource-access-semantics-design.md", "Third Review Pass: User Cases");
    expect_document_contains("docs/zh/m6-resource-access-semantics-design.md", "第三轮审视：用户案例");
}

TEST_F(AurexIntegrationTest, M8DynTraitDesignDocumentationIsCurrent)
{
    expect_document_contains("docs/zh/README.md", "M8 已正式封口");
    expect_document_contains("docs/zh/progress.md", "M8 release closure 已完成");
    expect_document_contains("docs/zh/progress.md", "origin-bound erased view");
    expect_document_contains("docs/zh/progress.md", "M8 封口结论");
    expect_document_contains("docs/zh/version.md", "## M8 Dyn Trait、Erased View 与动态派发 Release Closure");
    expect_document_contains(
        "docs/zh/next-steps.md",
        "当前最高优先级：标准库 / Owning Dyn Runtime Surface 入口评估");
    expect_document_contains("docs/zh/next-steps.md", "M9 Dyn ABI / Tooling Design Baseline");
    expect_document_contains("docs/zh/next-steps.md", "`CanonicalTypeKind::trait_object` 占位已移除");
    expect_document_contains("docs/zh/next-steps.md", "`TraitObjectTypeKey`、`VTableLayoutKey`、`TraitObjectCoercionKey`");
    expect_document_contains("docs/zh/progress.md", "M8a Borrowed Erased Trait View query foundation");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "origin-bound erased view");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "ObjectCallability");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "`TraitObjectTypeKey`");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "剩余阶段代码量预估");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "M8a：设计基线与 query 地基");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "M8d：IR/backend dynamic dispatch");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "M8e：hardening 和后续扩展评估");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "M8 follow-up：sample 和 release polish");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "M8 release closure");
    expect_document_contains(
        "docs/zh/m8-dyn-trait-design.md", "borrowed dyn view runtime dynamic dispatch 已完成");
    expect_document_contains("docs/zh/m8-dyn-trait-design.md", "当前阶段继续不实现标准库");
    expect_document_contains("docs/zh/usage.md", "tests/samples/positive/traits/trait_dyn_borrowed_dispatch.ax");
    expect_document_contains("docs/zh/language-manual.md", "Borrowed dyn trait view");
    expect_document_contains("docs/zh/language-manual.md", "`&dyn Trait` / `&mut dyn Trait` lowering 为 `{data*, vtable*}`");

    expect_document_contains("docs/zh/README.md", "M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线");
    expect_document_contains("docs/zh/progress.md", "M7d-F tuple / index place-state closure");
    expect_document_contains("docs/zh/version.md", "## M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线");
    expect_document_contains(
        "docs/zh/next-steps.md", "已收口基线：M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check");
    expect_document_contains("docs/zh/m7c-m7d-complete-borrow-raii-design.md", "`&[origin] T` / `&mut[origin] T`");
    expect_document_contains("docs/zh/m7c-m7d-complete-borrow-raii-design.md", "ClosureCaptureFact {");
    expect_document_contains("docs/zh/m7c-m7d-complete-borrow-raii-design.md", "ClosureEnvironmentFact {");
    expect_document_contains("docs/zh/m7c-m7d-complete-borrow-raii-design.md",
        "`src/frontend/sema/internal/` 只允许作为 private implementation root");
    expect_document_contains("docs/zh/README.md", "M7 Hardening Performance Closure");
    expect_document_contains("docs/zh/version.md", "## M7 Hardening Performance Closure");
    expect_document_contains("docs/zh/next-steps.md", "M7 hardening performance closure 也已完成");
    expect_document_contains("docs/zh/m7-hardening-performance-closure.md", "`u32/i32` 审计");
    expect_document_contains("docs/zh/m7-hardening-performance-closure.md", "tools/m7_hardening_perf.py");
}

TEST_F(AurexIntegrationTest, M9DynAbiToolingDesignDocumentationIsCurrent)
{
    expect_document_contains("docs/zh/README.md", "M9 Dyn ABI / Tooling Release Closure");
    expect_document_contains("docs/zh/README.md", "Aurex M9 Dyn ABI / Tooling 设计基线");
    expect_document_contains("docs/zh/README.md", "Aurex M9 Dyn ABI / Tooling Release Baseline");
    expect_document_contains("docs/zh/README.md", "M9c Advanced Dyn Design Gate");
    expect_document_contains("docs/zh/progress.md", "M9 Dyn ABI / Tooling release closure 已完成");
    expect_document_contains("docs/zh/progress.md", "FunctionDynAbiFacts");
    expect_document_contains("docs/zh/progress.md", "DynAdvancedDesignGate");
    expect_document_contains("docs/zh/progress.md", "library-independent dyn ABI DTO");
    expect_document_contains("docs/zh/version.md", "## M9 Dyn ABI / Tooling Release Closure");
    expect_document_contains("docs/zh/next-steps.md", "M9b ABI/tooling implementation");
    expect_document_contains("docs/zh/next-steps.md", "M10a / post-M9 advanced dyn policy selection 已完成");
    expect_document_contains("docs/zh/next-steps.md", "M9d / M9 release closure 已完成");
    expect_document_contains("docs/zh/next-steps.md", "M9c Advanced Dyn Design Gate");
    expect_document_contains("docs/zh/next-steps.md", "borrowed_methods_only_v1");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "facts-first dyn ABI DTO");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "TraitObjectTypeKey");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "VTableLayoutKey");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "TraitObjectCoercionKey");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "FunctionDynAbiFacts");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "DynAdvancedDesignGate");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "requires_standard_library_stage");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "requires_runtime_stage");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "standard_library_runtime_not_in_m9c");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "borrowed_methods_only_v1");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "dispatch=vtable_slot slot=N");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "Cross-Module Invalidation Matrix");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "Verifier And Backend Negative Matrix");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "不实现标准库");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "不实现 owning dyn");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "不实现 dynamic Drop dispatch");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "`trait_object_pack`");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "`trait_object_data`");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "`trait_object_vtable`");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "`vtable_slot`");
    expect_document_contains("docs/zh/m9-dyn-abi-tooling-design.md", "{data*, vtable*}");
    expect_document_contains("docs/zh/m9-release-baseline.md", "M9 release closure 已完成");
    expect_document_contains("docs/zh/m9-release-baseline.md", "FunctionDynAbiFacts");
    expect_document_contains("docs/zh/m9-release-baseline.md", "DynAdvancedDesignGate");
    expect_document_contains("docs/zh/m9-release-baseline.md", "`borrowed_view_v1`");
    expect_document_contains("docs/zh/m9-release-baseline.md", "`borrowed_methods_only_v1`");
    expect_document_contains("docs/zh/m9-release-baseline.md", "`standard_library_runtime_not_in_m9c`");
    expect_document_contains("docs/zh/m9-release-baseline.md", "不新增标准库");
    expect_document_contains("docs/zh/m9-release-baseline.md", "不新增 owning dyn runtime");
    expect_document_contains("docs/zh/m9-release-baseline.md", "不新增 dynamic Drop dispatch runtime");
    expect_document_contains("docs/zh/m9-release-baseline.md", "不新增 supertrait upcasting runtime");
    expect_document_contains("docs/zh/m9-release-baseline.md", "M10 planning / Post-M9 Advanced Dyn Policy Selection");
    expect_document_contains("docs/zh/language-feature-inventory.md", "M9 dyn ABI / tooling release closure 已完成");
}

TEST_F(AurexIntegrationTest, M10SupertraitUpcastingDesignDocumentationIsCurrent)
{
    expect_document_contains("docs/zh/README.md", "M10d Supertrait Hardening / Release Closure");
    expect_document_contains("docs/zh/README.md", "Aurex M10 Supertrait Upcasting 设计基线");
    expect_document_contains("docs/zh/README.md", "Aurex M10 Supertrait Upcasting Release Baseline");
    expect_document_contains("docs/zh/progress.md", "M10d Supertrait Hardening / Release Closure 已完成");
    expect_document_contains("docs/zh/progress.md", "borrowed dyn-to-dyn coercion");
    expect_document_contains("docs/zh/progress.md", "supertrait_vptr_metadata_v1");
    expect_document_contains("docs/zh/progress.md", "37 files changed, 1316 insertions(+), 255 deletions(-)");
    expect_document_contains("docs/zh/version.md", "## M10d Supertrait Hardening / Release Closure");
    expect_document_contains("docs/zh/version.md", "M11 Advanced Dyn Design Baseline");
    expect_document_contains(
        "docs/zh/next-steps.md",
        "当前最高优先级：标准库 / Owning Dyn Runtime Surface 入口评估");
    expect_document_contains("docs/zh/next-steps.md", "M10 已结束");
    expect_document_contains("docs/zh/next-steps.md", "M11a 也已结束");
    expect_document_contains("docs/zh/language-feature-inventory.md", "M10d 已完成 hardening/release closure");

    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md",
        "M10d Supertrait Hardening / Release Closure");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "supertrait_vptr_metadata_v1");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "TraitSupertraitEdgeFact");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "TraitObjectUpcastCoercionKey");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "DynUpcastAbiDescriptor");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "VTableSupertraitEdgeDescriptor");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "&dyn Child -> &dyn Parent");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "&mut dyn Child -> &mut dyn Parent");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "borrowed dyn-to-dyn coercion");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "不是普通子类型");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "不实现标准库");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "不实现 owning dyn");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "不实现 dynamic Drop dispatch");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "不实现 multi trait composition");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "borrowed_methods_only_v1");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "borrowed_view_v1");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "trait Child: Parent");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "trait_object_upcast");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "native inherited parent dispatch");
    expect_document_contains("docs/zh/m10-supertrait-upcasting-design.md", "M10c 实际低于预估");
    expect_document_contains("docs/zh/m10-release-baseline.md", "M10d 的 hardening/release closure 已完成");
    expect_document_contains("docs/zh/m10-release-baseline.md", "FunctionDynAbiFacts::upcasts");
    expect_document_contains("docs/zh/m10-release-baseline.md", "37 files changed, 1316 insertions(+), 255 deletions(-)");
    expect_document_contains("docs/zh/m10-release-baseline.md", "M11 Advanced Dyn Design Baseline");
    expect_document_contains("docs/zh/language-manual.md", "Borrowed dyn supertrait upcast");
}

TEST_F(AurexIntegrationTest, M11AdvancedDynDesignDocumentationIsCurrent)
{
    expect_document_contains(
        "docs/zh/README.md", "文档基线：**M21f Hygiene Source Map Debug Trace Stub Contract**");
    expect_document_contains("docs/zh/README.md", "Aurex M11 Advanced Dyn Design Baseline");
    expect_document_contains("docs/zh/README.md", "Aurex M11 Principal-Set Composition Release Baseline");
    expect_document_contains("docs/zh/README.md", "Aurex M12 Direct Composition Dispatch Release Baseline");
    expect_document_contains("docs/zh/README.md", "Aurex M13 Advanced Dyn Remaining Policy Design Baseline");
    expect_document_contains("docs/zh/README.md", "Aurex M14 Borrowed Dyn View Path Inference Release Baseline");
    expect_document_contains("docs/zh/README.md",
        "Aurex M16 Const Generic Frontend / Query / Sema Check-Only");
    expect_document_contains("docs/zh/README.md",
        "Aurex M16 Const Generic Frontend / Query / Sema Check-Only Release Baseline");
    expect_document_contains("docs/zh/README.md",
        "Aurex M17 Dyn Ownership Runtime Preparation Release Baseline");
    expect_document_contains("docs/zh/README.md",
        "Aurex M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate Release Baseline");
    expect_document_contains("docs/zh/README.md",
        "Aurex M19 Dyn Ownership Runtime IR / Verifier Preparation Release Baseline");
    expect_document_contains("docs/zh/README.md",
        "Aurex M20a Owned Dyn Runtime Admission Design Gate Release Baseline");
    expect_document_contains("docs/zh/README.md", "principal_set_metadata_v1");
    expect_document_contains("docs/zh/README.md", "dyn (A + B)");
    expect_document_contains("docs/zh/README.md", "FunctionDynAbiFacts");
    expect_document_contains("docs/zh/README.md", "composition_projections");
    expect_document_contains("docs/zh/README.md", "combo.draw()");
    expect_document_contains("docs/zh/README.md", "BorrowedDynViewPathFact");
    expect_document_contains("docs/zh/README.md", "m15_dyn_advanced_design_gate_baseline()");
    expect_document_contains("docs/zh/README.md", "m15_const_generic_design_gate_baseline()");
    expect_document_contains("docs/zh/README.md", "typed scalar const generic");
    expect_document_contains("docs/zh/README.md", "GenericInstanceKey::const_args");
    expect_document_contains("docs/zh/README.md", "Aurex M21a Macro System Design Gate");
    expect_document_contains("docs/zh/README.md", "Aurex M21b AttributeDecl / Token Tree Surface");
    expect_document_contains("docs/zh/README.md", "Aurex M21c Early Item Macro Expansion Plan");
    expect_document_contains("docs/zh/README.md", "Aurex M21d No-op Early Item Macro Expansion Boundary");
    expect_document_contains("docs/zh/README.md",
        "Aurex M21e Generated Module Part Parse/Merge Stub Contract");
    expect_document_contains("docs/zh/README.md",
        "Aurex M21f Hygiene Source Map Debug Trace Stub Contract");
    expect_document_contains("docs/zh/progress.md",
        "阶段：M21f Hygiene Source Map Debug Trace Stub Contract");
    expect_document_contains("docs/zh/progress.md", "EarlyItemExpansionResult");
    expect_document_contains("docs/zh/progress.md", "ExpansionHygieneStub");
    expect_document_contains("docs/zh/progress.md", "ExpansionTraceStub");
    expect_document_contains("docs/zh/progress.md", "call_site_mark");
    expect_document_contains("docs/zh/progress.md", "definition_site_mark");
    expect_document_contains("docs/zh/progress.md", "generated_fresh_mark");
    expect_document_contains("docs/zh/progress.md", "declared_name_set");
    expect_document_contains("docs/zh/progress.md", "trace_identity");
    expect_document_contains("docs/zh/progress.md", "generated_source_map_identity");
    expect_document_contains("docs/zh/progress.md", "diagnostic_anchor");
    expect_document_contains("docs/zh/progress.md", "origin_mark_hygiene_v1");
    expect_document_contains("docs/zh/progress.md", "expansion_source_map_debug_trace_v1");
    expect_document_contains("docs/zh/progress.md", "GeneratedModulePartParseMergeStub");
    expect_document_contains("docs/zh/progress.md", "GeneratedModulePartLifecycleState");
    expect_document_contains("docs/zh/progress.md", "generated_buffer_identity");
    expect_document_contains("docs/zh/progress.md", "parse_config_fingerprint");
    expect_document_contains("docs/zh/progress.md", "merge_ordering_key");
    expect_document_contains("docs/zh/progress.md", "expand_early_item_macros_noop()");
    expect_document_contains("docs/zh/progress.md", "macro.expand_items");
    expect_document_contains("docs/zh/progress.md", "MacroExpansionPlan");
    expect_document_contains("docs/zh/progress.md", "m21c_macro_expansion_plan_baseline()");
    expect_document_contains("docs/zh/progress.md", "SourceRole::generated");
    expect_document_contains("docs/zh/progress.md", "ModulePartKind::generated");
    expect_document_contains("docs/zh/progress.md", "AttributeTokenDecl");
    expect_document_contains("docs/zh/progress.md", "item attribute macros are parsed but macro expansion is not implemented yet");
    expect_document_contains("docs/zh/progress.md", "m21a_macro_design_gate_baseline()");
    expect_document_contains("docs/zh/progress.md", "query-backed incremental expansion");
    expect_document_contains("docs/zh/progress.md", "attached item codegen");
    expect_document_contains("docs/zh/version.md",
        "## M21f Hygiene Source Map Debug Trace Stub Contract");
    expect_document_contains("docs/zh/version.md", "ExpansionHygieneStub");
    expect_document_contains("docs/zh/version.md", "ExpansionTraceStub");
    expect_document_contains("docs/zh/version.md", "origin_mark_hygiene_v1");
    expect_document_contains("docs/zh/version.md", "expansion_source_map_debug_trace_v1");
    expect_document_contains("docs/zh/version.md",
        "## M21e Generated Module Part Parse/Merge Stub Contract");
    expect_document_contains("docs/zh/version.md", "GeneratedModulePartParseMergeStub");
    expect_document_contains("docs/zh/version.md", "GeneratedModulePartLifecycleState");
    expect_document_contains("docs/zh/version.md", "generated_buffer_identity");
    expect_document_contains("docs/zh/version.md", "parse_config_fingerprint");
    expect_document_contains("docs/zh/version.md", "merge_ordering_key");
    expect_document_contains("docs/zh/version.md", "## M21d No-op Early Item Macro Expansion Boundary");
    expect_document_contains("docs/zh/version.md", "EarlyItemExpansionResult");
    expect_document_contains("docs/zh/version.md", "macro.expand_items");
    expect_document_contains("docs/zh/version.md", "不生成用户代码");
    expect_document_contains("docs/zh/version.md", "## M21c Early Item Macro Expansion Plan");
    expect_document_contains("docs/zh/version.md", "MacroExpansionFact");
    expect_document_contains("docs/zh/version.md", "generated_module_part_noop");
    expect_document_contains("docs/zh/version.md", "## M21b AttributeDecl / Token Tree Surface");
    expect_document_contains("docs/zh/version.md", "ItemNode::attributes");
    expect_document_contains("docs/zh/version.md", "## M21a Macro System Design Gate");
    expect_document_contains("docs/zh/version.md", "macro_design_gate_fingerprint()");
    expect_document_contains("docs/zh/m21-macro-system-design-gate.md", "do_not_support_textual_macros");
    expect_document_contains("docs/zh/m21-macro-system-design-gate.md", "external_procedural_macro_sandbox");
    expect_document_contains("docs/zh/m21-macro-system-design-gate.md", "m21a_macro_design_gate_baseline()");
    expect_document_contains("docs/zh/m21-macro-system-design-gate.md", "attached item codegen");
    expect_document_contains("docs/zh/m21b-attribute-token-tree-surface.md", "AttributeDecl");
    expect_document_contains("docs/zh/m21b-attribute-token-tree-surface.md", "AttributeTokenDecl");
    expect_document_contains("docs/zh/m21b-attribute-token-tree-surface.md", "item attribute macros are parsed but macro expansion is not implemented yet");
    expect_document_contains("docs/zh/m21c-early-item-macro-expansion-plan.md", "MacroExpansionPlan");
    expect_document_contains("docs/zh/m21c-early-item-macro-expansion-plan.md", "m21c_macro_expansion_plan_baseline()");
    expect_document_contains("docs/zh/m21c-early-item-macro-expansion-plan.md", "SourceRole::generated");
    expect_document_contains("docs/zh/m21c-early-item-macro-expansion-plan.md", "ModulePartKind::generated");
    expect_document_contains("docs/zh/m21c-early-item-macro-expansion-plan.md", "external procedural macro 仍是 future stage");
    expect_document_contains("docs/zh/m21d-noop-early-item-expansion-boundary.md",
        "Aurex M21d No-op Early Item Macro Expansion Boundary");
    expect_document_contains("docs/zh/m21d-noop-early-item-expansion-boundary.md",
        "阶段：M21d No-op Early Item Macro Expansion Boundary");
    expect_document_contains("docs/zh/m21d-noop-early-item-expansion-boundary.md", "EarlyItemExpansionResult");
    expect_document_contains("docs/zh/m21d-noop-early-item-expansion-boundary.md", "expand_early_item_macros_noop");
    expect_document_contains("docs/zh/m21d-noop-early-item-expansion-boundary.md", "macro.expand_items");
    expect_document_contains("docs/zh/m21d-noop-early-item-expansion-boundary.md", "SourceRole::generated");
    expect_document_contains("docs/zh/m21d-noop-early-item-expansion-boundary.md", "ModulePartKind::generated");
    expect_document_contains("docs/zh/m21d-noop-early-item-expansion-boundary.md", "不生成用户代码");
    expect_document_contains("docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md",
        "Aurex M21e Generated Module Part Parse/Merge Stub Contract");
    expect_document_contains("docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md",
        "阶段：M21e Generated Module Part Parse/Merge Stub Contract");
    expect_document_contains(
        "docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md", "GeneratedModulePartParseMergeStub");
    expect_document_contains(
        "docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md", "GeneratedModulePartLifecycleState");
    expect_document_contains(
        "docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md", "generated_buffer_identity");
    expect_document_contains(
        "docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md", "parse_config_fingerprint");
    expect_document_contains(
        "docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md", "merge_ordering_key");
    expect_document_contains("docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md",
        "macro.expand_items");
    expect_document_contains("docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md",
        "SourceRole::generated");
    expect_document_contains("docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md",
        "ModulePartKind::generated");
    expect_document_contains("docs/zh/m21e-generated-module-part-parse-merge-stub-contract.md",
        "仍不生成用户代码");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "Aurex M21f Hygiene Source Map Debug Trace Stub Contract");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "阶段：M21f Hygiene Source Map Debug Trace Stub Contract");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "ExpansionHygieneStub");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "ExpansionTraceStub");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "origin_mark_hygiene_v1");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "expansion_source_map_debug_trace_v1");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "macro_hygiene_mark_fact");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "macro_expansion_trace_fact");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "macro_generated_source_map_fact");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "macro.expand_items");
    expect_document_contains("docs/zh/m21f-hygiene-source-map-debug-trace-stub-contract.md",
        "M21f 仍不生成用户代码");
    expect_document_contains(
        "docs/zh/progress.md", "M20a Owned Dyn Runtime Admission Design Gate 已完成");
    expect_document_contains(
        "docs/zh/progress.md", "M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate 已完成");
    expect_document_contains(
        "docs/zh/progress.md", "M17 Dyn Ownership Runtime Preparation 已完成");
    expect_document_contains(
        "docs/zh/progress.md", "M16 Const Generic Frontend / Query / Sema Check-Only 已完成");
    expect_document_contains(
        "docs/zh/progress.md", "M14 Borrowed Dyn View Path Inference / Dispatch Release 已完成");
    expect_document_contains("docs/zh/progress.md", "m15_const_generic_design_gate_baseline()");
    expect_document_contains("docs/zh/progress.md", "typed_const_param_v1");
    expect_document_contains("docs/zh/progress.md", "syntax::GenericParamKind::const_");
    expect_document_contains("docs/zh/progress.md", "GenericInstanceKey::const_args");
    expect_document_contains("docs/zh/progress.md", "Aurex M16 Const Generic Frontend / Query / Sema Check-Only Release Baseline");
    expect_document_contains("docs/zh/progress.md", "DynOwnershipRuntimeFacts");
    expect_document_contains("docs/zh/progress.md", "DynOwnershipRuntimeBoundaryGate");
    expect_document_contains("docs/zh/progress.md", "DynOwnershipRuntimeIrVerifierFact");
    expect_document_contains("docs/zh/progress.md", "OwnedDynRuntimeAdmissionGate");
    expect_document_contains("docs/zh/progress.md", "OwnedDynRuntimeLoweringAbiGate");
    expect_document_contains("docs/zh/progress.md", "m17_dyn_ownership_runtime_preparation_baseline()");
    expect_document_contains("docs/zh/progress.md", "m18_dyn_ownership_runtime_boundary_gate_baseline()");
    expect_document_contains("docs/zh/progress.md", "m19_dyn_ownership_runtime_ir_verifier_baseline()");
    expect_document_contains("docs/zh/progress.md", "m20_owned_dyn_runtime_admission_gate_baseline()");
    expect_document_contains("docs/zh/progress.md", "m20d_owned_dyn_runtime_lowering_abi_gate_baseline()");
    expect_document_contains("docs/zh/progress.md", "BorrowedDynViewPathFact");
    expect_document_contains("docs/zh/progress.md", "borrowed_view_path_dispatch_count");
    expect_document_contains("docs/zh/progress.md", "view.parent()");
    expect_document_contains("docs/zh/progress.md", "FunctionDynAbiFacts::composition_supertrait_chains");
    expect_document_contains(
        "docs/zh/progress.md", "M13d Borrowed Composition-To-Supertrait Hardening / Release Closure 已完成");
    expect_document_contains(
        "docs/zh/progress.md", "M13c Borrowed Composition-To-Supertrait IR / Backend Runtime 已完成");
    expect_document_contains(
        "docs/zh/progress.md", "M13b Borrowed Composition-To-Supertrait Frontend / Query / Sema Check-Only 已完成");
    expect_document_contains("docs/zh/progress.md", "dynproject[SourcePrincipal, TargetSupertrait](view)");
    expect_document_contains("docs/zh/progress.md", "CompositionProjectionFact{kind=composition_to_supertrait}");
    expect_document_contains("docs/zh/progress.md", "M13a Advanced Dyn Remaining Policy Design Baseline 已完成");
    expect_document_contains("docs/zh/progress.md", "m13a_dyn_advanced_design_gate_baseline()");
    expect_document_contains("docs/zh/progress.md", "M12b Direct Composition Dispatch Hardening / Release Closure 已完成");
    expect_document_contains("docs/zh/progress.md", "M12a Direct Principal-Qualified Composition Method Dispatch 已完成");
    expect_document_contains("docs/zh/progress.md", "principal-set borrowed dyn composition");
    expect_document_contains("docs/zh/progress.md", "m11a_dyn_advanced_design_gate_baseline");
    expect_document_contains("docs/zh/progress.md", "M11e Principal-Set Composition Hardening / Release Closure");
    expect_document_contains("docs/zh/progress.md", "trait_object_composition_pack");
    expect_document_contains("docs/zh/progress.md", "FunctionDynAbiFacts::principal_sets");
    expect_document_contains("docs/zh/progress.md", "DynCompositionProjectionAbiDescriptor");
    expect_document_contains("docs/zh/version.md", "## M11c Principal-Set Composition Frontend / Sema Check-Only");
    expect_document_contains("docs/zh/version.md", "## M11e Principal-Set Composition Hardening / Release Closure");
    expect_document_contains("docs/zh/version.md", "## M13a Advanced Dyn Remaining Policy Design Baseline");
    expect_document_contains("docs/zh/version.md", "## M12b Direct Composition Dispatch Hardening / Release Closure");
    expect_document_contains("docs/zh/version.md", "DynPrincipalSetMetadataAbiDescriptor");
    expect_document_contains("docs/zh/version.md", "DynCompositionProjectionAbiDescriptor");
    expect_document_contains("docs/zh/version.md", "## M11a Advanced Dyn Design Baseline");
    expect_document_contains("docs/zh/version.md", "## M16 Const Generic Frontend / Query / Sema Check-Only");
    expect_document_contains("docs/zh/version.md", "## M17 Dyn Ownership Runtime Preparation");
    expect_document_contains("docs/zh/version.md",
        "## M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate");
    expect_document_contains("docs/zh/version.md",
        "## M19 Dyn Ownership Runtime IR / Verifier Preparation");
    expect_document_contains("docs/zh/version.md",
        "## M20a Owned Dyn Runtime Admission Design Gate");
    expect_document_contains("docs/zh/version.md",
        "## M20b Owned Dyn IR Shape Prototype Gate");
    expect_document_contains("docs/zh/version.md",
        "## M20c Drop / Allocator Identity Prerequisite Gate");
    expect_document_contains("docs/zh/version.md",
        "## M20d Runtime Lowering ABI Design Closure");
    expect_document_contains("docs/zh/version.md", "DynOwnedContainerBoundaryFact");
    expect_document_contains("docs/zh/version.md", "borrowed-vtable destructor-free");
    expect_document_contains("docs/zh/version.md", "syntax::GenericArgKind::{type,const_expr}");
    expect_document_contains("docs/zh/version.md", "GenericInstanceKey::const_args");
    expect_document_contains("docs/zh/version.md", "转发时必须和目标 const parameter type 一致");
    expect_document_contains("docs/zh/version.md", "completed_release_baseline");
    expect_document_contains("docs/zh/version.md", "ready_for_future_stage");
    expect_document_contains(
        "docs/zh/next-steps.md",
        "当前最高优先级：标准库 / Owning Dyn Runtime Surface 入口评估");
    expect_document_contains("docs/zh/next-steps.md",
        "M11a Advanced Dyn Design Baseline、M11b Principal-Set Composition Query Prototype Gate、M11c Principal-Set");
    expect_document_contains("docs/zh/next-steps.md", "M11b 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M11c 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M11d 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M11e 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M12a 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M12b 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M13a 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M13b 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M13c 也已结束");
    expect_document_contains("docs/zh/next-steps.md", "M12b hardening/release");
    expect_document_contains("docs/zh/next-steps.md", "M13a Advanced Dyn Remaining Policy Design Baseline");
    expect_document_contains("docs/zh/next-steps.md",
        "M13b Borrowed Composition-To-Supertrait Frontend / Query / Sema Check-Only");
    expect_document_contains(
        "docs/zh/next-steps.md", "M13c Borrowed Composition-To-Supertrait IR / Backend Runtime");
    expect_document_contains(
        "docs/zh/next-steps.md", "M13d Borrowed Composition-To-Supertrait Hardening / Release Closure");
    expect_document_contains(
        "docs/zh/next-steps.md", "M14 Borrowed Dyn View Path Inference / Dispatch Release");
    expect_document_contains(
        "docs/zh/next-steps.md", "M16 Const Generic Frontend / Query / Sema Check-Only");
    expect_document_contains(
        "docs/zh/next-steps.md", "M17 Dyn Ownership Runtime Preparation");
    expect_document_contains(
        "docs/zh/next-steps.md", "M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate");
    expect_document_contains(
        "docs/zh/next-steps.md", "M19 Dyn Ownership Runtime IR / Verifier Preparation");
    expect_document_contains(
        "docs/zh/next-steps.md", "M20a Owned Dyn Runtime Admission Design Gate");
    expect_document_contains("docs/zh/next-steps.md", "M16 const generic check-only");
    expect_document_contains("docs/zh/next-steps.md", "M17 dyn ownership runtime prep");
    expect_document_contains("docs/zh/next-steps.md", "M18 dyn ownership runtime boundary hardening");
    expect_document_contains("docs/zh/next-steps.md", "M19 dyn ownership runtime IR / verifier preparation");
    expect_document_contains("docs/zh/next-steps.md", "M20a owned dyn runtime admission design gate");
    expect_document_contains("docs/zh/next-steps.md", "M20b owned dyn IR shape prototype gate");
    expect_document_contains("docs/zh/next-steps.md", "M20c drop / allocator identity prerequisite gate");
    expect_document_contains("docs/zh/next-steps.md", "M20d Runtime Lowering ABI Design Closure 已完成");
    expect_document_contains("docs/zh/next-steps.md", "BorrowedDynViewPathFact");
    expect_document_contains("docs/zh/next-steps.md", "composition_supertrait_chains");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M11a 已选择 principal-set borrowed dyn composition design/query gate");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M11e 已完成 composition runtime facts/query/tooling/verifier hardening");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M12a 已支持唯一 principal method 的 direct composition dispatch");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M12b 已完成 receiver-access binding");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M13a 已选择 borrowed composition-to-supertrait explicit projection");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M13c/M13d 已支持并收口该显式投影的 IR/backend runtime lowering");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M14 已支持唯一 path 的 expected-type projection");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M16 已支持 typed scalar const generic check-only");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M17 已新增 `DynOwnershipRuntimeFacts`");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M18 已新增 `DynOwnershipRuntimeBoundaryGate`");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M19 已新增 `DynOwnershipRuntimeIrVerifierFact`");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M20a 已新增 `OwnedDynRuntimeAdmissionGate`");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M20b 已新增 `OwnedDynObjectLayoutPrototype`");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M20c 已新增 `OwnedDynDropAllocatorIdentityGate`");
    expect_document_contains("docs/zh/language-feature-inventory.md",
        "M20d 已新增 `OwnedDynRuntimeLoweringAbiGate`");
    expect_document_contains("docs/zh/language-manual.md", "M13a Advanced Dyn Remaining Policy Design Baseline");
    expect_document_contains(
        "docs/zh/language-manual.md",
        "M13b Borrowed Composition-To-Supertrait Frontend / Query / Sema Check-Only");
    expect_document_contains("docs/zh/language-manual.md", "m13a_dyn_advanced_design_gate_baseline");
    expect_document_contains("docs/zh/language-manual.md", "composes_existing_metadata_policies");
    expect_document_contains("docs/zh/language-manual.md", "dynproject[SourcePrincipal, TargetSupertrait](view)");
    expect_document_contains("docs/zh/language-manual.md", "dynproject[Child, Parent](view)");
    expect_document_contains("docs/zh/language-manual.md", "BorrowedDynViewPathFact");
    expect_document_contains("docs/zh/language-manual.md", "let parent: &dyn Parent = view");
    expect_document_contains("docs/zh/language-manual.md", "view.parent()");
    expect_document_contains("docs/zh/language-manual.md", "M16 const generic check-only 子集当前可写");
    expect_document_contains("docs/zh/language-manual.md", "查询 M17 dyn ownership runtime preparation facts");
    expect_document_contains("docs/zh/language-manual.md", "查询 M18 dyn ownership runtime boundary gate");
    expect_document_contains("docs/zh/language-manual.md", "查询 M19 dyn ownership runtime IR / verifier facts");
    expect_document_contains("docs/zh/language-manual.md", "查询 M20a owned dyn runtime admission gate");
    expect_document_contains("docs/zh/language-manual.md",
        "查询 M20b owned dyn IR shape prototype gate");
    expect_document_contains("docs/zh/language-manual.md",
        "查询 M20c drop / allocator identity prerequisite gate");
    expect_document_contains("docs/zh/language-manual.md",
        "查询 M20d runtime lowering ABI design closure gate");
    expect_document_contains(
        "docs/zh/language-manual.md", "GenericParam  = Identifier | \"const\" Identifier \":\" Type | \"origin\" Identifier");
    expect_document_contains("docs/zh/language-manual.md", "ArrayView[i32, 4]");
    expect_document_contains("docs/zh/language-manual.md", "转发时必须和目标 const parameter type 一致");
    expect_document_contains("docs/zh/language-manual.md", "M12b direct composition dispatch release closure");
    expect_document_contains("docs/zh/language-manual.md", "dyn (Draw + Debug)");
    expect_document_contains("docs/zh/language-manual.md", "M11e borrowed principal-set composition release closure");
    expect_document_contains("docs/zh/language-manual.md", "composition_projections");
    expect_document_contains("docs/zh/language-manual.md", "view.draw()");
    expect_document_contains("docs/zh/usage.md", "M13c");
    expect_document_contains("docs/zh/usage.md",
        "`trait_object_composition_project` + `trait_object_upcast` runtime");
    expect_document_contains("docs/zh/usage.md", "M14 后");
    expect_document_contains("docs/zh/usage.md", "Const Generic 状态");
    expect_document_contains("docs/zh/usage.md", "M16 已打开用户可写 const generic");
    expect_document_contains("docs/zh/usage.md", "M17 新增 `m17_dyn_ownership_runtime_preparation_baseline()`");
    expect_document_contains("docs/zh/usage.md", "M18 新增 `m18_dyn_ownership_runtime_boundary_gate_baseline()`");
    expect_document_contains("docs/zh/usage.md", "M19 新增 `m19_dyn_ownership_runtime_ir_verifier_baseline()`");
    expect_document_contains("docs/zh/usage.md", "M20a 新增 `m20_owned_dyn_runtime_admission_gate_baseline()`");
    expect_document_contains("docs/zh/usage.md",
        "M20b 新增 `m20b_owned_dyn_ir_shape_prototype_gate_baseline()`");
    expect_document_contains("docs/zh/usage.md",
        "M20c 新增 `m20c_owned_dyn_drop_allocator_identity_gate_baseline()`");
    expect_document_contains("docs/zh/usage.md",
        "M20d 新增 `m20d_owned_dyn_runtime_lowering_abi_gate_baseline()`");
    expect_document_contains("docs/zh/usage.md", "ArrayView[i32, 4]");
    expect_document_contains("docs/zh/usage.md", "转发时必须和目标 const parameter type 一致");
    expect_document_contains("docs/zh/usage.md", "BorrowedDynViewPathFact{use=method_dispatch}");
    expect_document_contains("docs/zh/usage.md", "m13a_dyn_advanced_design_gate_baseline");
    expect_document_contains("docs/zh/usage.md", "score_direct");
    expect_document_contains("docs/zh/usage.md", "score_supertrait");
    expect_document_contains("docs/zh/usage.md", "dynproject[Render, Draw](view)");
    expect_document_contains("docs/zh/usage.md", "composition_metadata=principal_set_metadata_v1");

    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "M11a Advanced Dyn Design Baseline");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "M11e Principal-Set Composition Hardening / Release Closure 已完成");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "principal-set borrowed dyn composition");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "dyn (A + B)");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "principal_set_metadata_v1");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "principal_set_identity_fact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "composition_witness_set_fact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "principal_method_namespace_fact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "associated_equality_merge_fact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "composition_projection_fact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "standard_library_runtime_not_in_m11a");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "runtime_dispatch_not_in_m11a");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "owning_dyn_runtime_not_in_m11a");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "do_not_encode_principal_set_as_single_trait_object");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "do_not_flatten_method_slots_without_principal_namespace");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "m11a_dyn_advanced_design_gate_baseline");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "completed_release_baseline");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "ready_for_future_stage");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "不实现标准库");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "不实现 owning dyn");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "不实现 dynamic Drop dispatch");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "M11b Principal-Set Composition Query Prototype Gate");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "PrincipalSetCompositionFacts");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "PrincipalSetIdentityFact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "CompositionWitnessSetFact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "PrincipalMethodNamespaceFact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "AssociatedEqualityMergeFact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "CompositionProjectionFact");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "principal_set_composition_facts_fingerprint");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "summarize_principal_set_composition_facts");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "dump_principal_set_composition_facts");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "ambiguous_requires_principal");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "composition_to_supertrait");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "trait_object_composition_project");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md",
        "不实现 direct principal-qualified composition method dispatch");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "不实现标准库");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "M11e Hardening / Release Closure");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "M12 Advanced Dyn Design Baseline");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "FunctionDynAbiFacts");
    expect_document_contains("docs/zh/m11-advanced-dyn-design.md", "missing principal object");
    expect_document_contains("docs/zh/m11-release-baseline.md",
        "M11a、M11b、M11c、M11d 和 M11e 已完成");
    expect_document_contains("docs/zh/m11-release-baseline.md", "FunctionDynAbiFacts::principal_sets");
    expect_document_contains("docs/zh/m11-release-baseline.md", "FunctionDynAbiFacts::composition_projections");
    expect_document_contains("docs/zh/m11-release-baseline.md", "DynPrincipalSetMetadataAbiDescriptor");
    expect_document_contains("docs/zh/m11-release-baseline.md", "DynCompositionProjectionAbiDescriptor");
    expect_document_contains("docs/zh/m11-release-baseline.md", "lower_function_ir_result_fingerprint");
    expect_document_contains("docs/zh/m11-release-baseline.md", "缺失或不匹配 principal object");
    expect_document_contains("docs/zh/m11-release-baseline.md", "M12 Advanced Dyn Design Baseline");
    expect_document_contains("docs/zh/version.md", "## M11b Principal-Set Composition Query Prototype Gate");
    expect_document_contains(
        "docs/zh/version.md", "## M12a Direct Principal-Qualified Composition Method Dispatch");
    expect_document_contains(
        "docs/zh/m12-release-baseline.md", "M12a/M12b 已完成");
    expect_document_contains(
        "docs/zh/m12-release-baseline.md", "dispatch_receiver_type` 计算 receiver access");
    expect_document_contains(
        "docs/zh/m12-release-baseline.md", "Associated equality direct dispatch");
    expect_document_contains(
        "docs/zh/m12-release-baseline.md", "function_dyn_abi_facts_fingerprint");
    expect_document_contains(
        "docs/zh/m12-release-baseline.md", "composition-to-supertrait 隐式 direct call");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "M13a design/query gate 已完成");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "borrowed composition-to-supertrait explicit projection");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "m13a_dyn_advanced_design_gate_baseline()");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "DynAdvancedCapability::borrowed_composition_supertrait_projection");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "DynAdvancedPolicyDecision::composes_existing_metadata_policies");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "composition_to_supertrait_projection_fact");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "do_not_make_composition_to_supertrait_direct_call_implicit");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "do_not_add_new_principal_set_metadata_policy");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "M13b frontend/query/sema check-only");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "M13c IR/backend runtime 已完成");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "M13d 已完成 query/cache/tooling");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md", "composition_supertrait_chains");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "dynproject[SourcePrincipal, TargetSupertrait](view)");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "trait_object_composition_project` +");
    expect_document_contains("docs/zh/m13-advanced-dyn-design.md",
        "trait_object_upcast");
    expect_document_contains("docs/zh/m14-borrowed-dyn-view-path-release.md",
        "M14 已完成 borrowed dyn view path inference");
    expect_document_contains("docs/zh/m14-borrowed-dyn-view-path-release.md",
        "BorrowedDynViewPathFact");
    expect_document_contains("docs/zh/m14-borrowed-dyn-view-path-release.md",
        "expected_type_projection");
    expect_document_contains("docs/zh/m14-borrowed-dyn-view-path-release.md",
        "method_dispatch");
    expect_document_contains("docs/zh/m14-borrowed-dyn-view-path-release.md",
        "trait_object_composition_project");
    expect_document_contains("docs/zh/m14-borrowed-dyn-view-path-release.md",
        "trait_object_upcast");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "m15_dyn_advanced_design_gate_baseline()");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "m15_const_generic_design_gate_baseline()");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "Const Generic Boundary");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "typed_const_parameter_surface");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "canonical_const_argument_identity");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "generic_instance_key_integration");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "array_length_type_integration");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "typed_const_param_v1");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "generic_instance_const_arg_key_v1");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "array_length_const_param_v1");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "standard_library_runtime_not_in_m15");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "do_not_implement_box_dyn_trait_in_m15");
    expect_document_contains("docs/zh/m15-advanced-dyn-const-generic-design.md",
        "do_not_support_generic_arithmetic_array_lengths_in_m15");
    expect_document_contains("docs/zh/m16-const-generic-check-only-release.md",
        "M16 已完成 const generic 的 parser / AST / query identity / sema check-only 子集");
    expect_document_contains("docs/zh/m16-const-generic-check-only-release.md",
        "syntax::GenericParamKind::const_");
    expect_document_contains("docs/zh/m16-const-generic-check-only-release.md",
        "GenericInstanceKey::const_args");
    expect_document_contains("docs/zh/m16-const-generic-check-only-release.md",
        "struct ArrayView[T, const N: usize]");
    expect_document_contains("docs/zh/m16-const-generic-check-only-release.md",
        "转发时必须和目标 const parameter type 一致");
    expect_document_contains("docs/zh/m16-const-generic-check-only-release.md",
        "M17 Dyn Ownership Runtime Preparation");
    expect_document_contains("docs/zh/m17-dyn-ownership-runtime-prep-release.md",
        "M17 已完成 dyn ownership runtime preparation 的 compiler/query/tooling 边界事实");
    expect_document_contains("docs/zh/m17-dyn-ownership-runtime-prep-release.md",
        "DynOwnershipRuntimeFacts");
    expect_document_contains("docs/zh/m17-dyn-ownership-runtime-prep-release.md",
        "DynErasedDropGlueBoundaryFact");
    expect_document_contains("docs/zh/m17-dyn-ownership-runtime-prep-release.md",
        "m17_dyn_ownership_runtime_preparation_baseline()");
    expect_document_contains("docs/zh/m17-dyn-ownership-runtime-prep-release.md",
        "borrowed_vtable_destructor_free_count=1");
    expect_document_contains("docs/zh/m17-dyn-ownership-runtime-prep-release.md",
        "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/m17-dyn-ownership-runtime-prep-release.md",
        "M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate");
    expect_document_contains("docs/zh/m18-dyn-ownership-runtime-boundary-hardening-release.md",
        "M18 已完成 dyn ownership runtime boundary hardening / lowering design gate");
    expect_document_contains("docs/zh/m18-dyn-ownership-runtime-boundary-hardening-release.md",
        "dyn_ownership_runtime_boundary_gate");
    expect_document_contains("docs/zh/m18-dyn-ownership-runtime-boundary-hardening-release.md",
        "不实现标准库");
    expect_document_contains("docs/zh/m18-dyn-ownership-runtime-boundary-hardening-release.md",
        "lowering_runtime_implemented=false");
    expect_document_contains("docs/zh/m18-dyn-ownership-runtime-boundary-hardening-release.md",
        "M19 Dyn Ownership Runtime IR / Verifier Preparation");
    expect_document_contains("docs/zh/m19-dyn-ownership-runtime-ir-verifier-prep-release.md",
        "M19 已完成 dyn ownership runtime 的 IR / verifier preparation");
    expect_document_contains("docs/zh/m19-dyn-ownership-runtime-ir-verifier-prep-release.md",
        "FunctionDynOwnershipRuntimeIrVerifierFacts");
    expect_document_contains("docs/zh/m19-dyn-ownership-runtime-ir-verifier-prep-release.md",
        "TraitObjectVTableLayout::destructor_slot_blocked");
    expect_document_contains("docs/zh/m19-dyn-ownership-runtime-ir-verifier-prep-release.md",
        "CleanupAbiPolicy::dynamic_erased_drop_blocked");
    expect_document_contains("docs/zh/m19-dyn-ownership-runtime-ir-verifier-prep-release.md",
        "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/m19-dyn-ownership-runtime-ir-verifier-prep-release.md",
        "M20 标准库/owning dyn runtime design gate");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-admission-gate-release.md",
        "M20a 已完成 owned dyn runtime admission design gate");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-admission-gate-release.md",
        "OwnedDynRuntimeAdmissionGate");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-admission-gate-release.md",
        "owned_dyn_runtime_admission_gate_fingerprint()");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-admission-gate-release.md",
        "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-admission-gate-release.md",
        "M20b Owned Dyn IR Shape Prototype Gate");
    expect_document_contains("docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "M20b 已完成 owned dyn IR shape prototype gate");
    expect_document_contains("docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "OwnedDynObjectLayoutPrototype");
    expect_document_contains("docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "OwnedDynIrShapePrototypeGate");
    expect_document_contains("docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "m20b_owned_dyn_ir_shape_prototype_gate_baseline()");
    expect_document_contains("docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "owned_dyn_object_layout_prototype");
    expect_document_contains("docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "IR_OWNED_DYN_OBJECT_RUNTIME_SLOT_BLOCKED");
    expect_document_contains("docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/m20-owned-dyn-ir-shape-prototype-release.md",
        "M20c Drop / Allocator Identity Prerequisite Gate");
    expect_document_contains("docs/zh/m20-owned-dyn-drop-allocator-identity-release.md",
        "M20c 已完成 drop / allocator identity prerequisite gate");
    expect_document_contains("docs/zh/m20-owned-dyn-drop-allocator-identity-release.md",
        "OwnedDynDropAllocatorIdentityGate");
    expect_document_contains("docs/zh/m20-owned-dyn-drop-allocator-identity-release.md",
        "m20c_owned_dyn_drop_allocator_identity_gate_baseline()");
    expect_document_contains("docs/zh/m20-owned-dyn-drop-allocator-identity-release.md",
        "ir::owned_dyn_drop_allocator_identity_gate");
    expect_document_contains("docs/zh/m20-owned-dyn-drop-allocator-identity-release.md",
        "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/m20-owned-dyn-drop-allocator-identity-release.md",
        "M20d Runtime Lowering ABI Design Closure");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-lowering-abi-design-release.md",
        "M20d 已完成 runtime lowering ABI design closure");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-lowering-abi-design-release.md",
        "OwnedDynRuntimeLoweringAbiGate");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-lowering-abi-design-release.md",
        "m20d_owned_dyn_runtime_lowering_abi_gate_baseline()");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-lowering-abi-design-release.md",
        "ir::owned_dyn_runtime_lowering_abi_gate");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-lowering-abi-design-release.md",
        "backend_helper_callable=false");
    expect_document_contains("docs/zh/m20-owned-dyn-runtime-lowering-abi-design-release.md",
        "不实现 `Box<dyn Trait>`");
    expect_document_contains("docs/zh/version.md",
        "## M13b Borrowed Composition-To-Supertrait Frontend / Query / Sema Check-Only");
    expect_document_contains("docs/zh/version.md",
        "## M13c Borrowed Composition-To-Supertrait IR / Backend Runtime");
    expect_document_contains("docs/zh/version.md",
        "## M13d Borrowed Composition-To-Supertrait Hardening / Release Closure");
    expect_document_contains("docs/zh/version.md",
        "## M14 Borrowed Dyn View Path Inference / Dispatch Release");
    expect_document_contains("docs/zh/version.md", "BorrowedDynViewPathUse");
    expect_document_contains("docs/zh/version.md", "borrowed_view_path_expected_projection_count");
    expect_document_contains("docs/zh/version.md", "composition_supertrait_chains");
    expect_document_contains("docs/zh/version.md", "supertrait_projections");
    expect_document_contains("docs/zh/version.md", "PrincipalSetCompositionFacts");
    expect_document_contains("docs/zh/next-steps.md",
        "M11e Principal-Set Composition Hardening / Release Closure");
    expect_document_contains("docs/zh/language-manual.md", "M11b principal-set composition facts");
    expect_document_contains("docs/zh/language-manual.md", "M11e borrowed principal-set composition release closure");
}

} // namespace aurex::test
