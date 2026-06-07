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
    expect_document_contains("docs/zh/requirements.md", "当前实现基线是 M6 Resource And Access Semantics");
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
    expect_document_contains("docs/zh/README.md", "M8 Dyn Trait、Erased View 与动态派发设计基线");
    expect_document_contains("docs/zh/progress.md", "阶段：M8 Dyn Trait、Erased View 与动态派发设计基线");
    expect_document_contains("docs/zh/progress.md", "origin-bound erased view");
    expect_document_contains("docs/zh/version.md", "## M8 Dyn Trait、Erased View 与动态派发设计基线");
    expect_document_contains("docs/zh/next-steps.md", "当前最高优先级：M8 Dyn Trait、Erased View 与动态派发");
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

} // namespace aurex::test
