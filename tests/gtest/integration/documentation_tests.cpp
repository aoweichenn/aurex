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

TEST_F(AurexIntegrationTest, M6ResourceSemanticsWp2ThroughWp4AreClosed)
{
    expect_document_contains("docs/en/README.md", "M6 Resource, Value Lifetime, And Access Semantics Roadmap");
    expect_document_contains("docs/zh/README.md", "M6 资源、值生命周期与访问语义路线图");
    expect_document_contains("docs/en/progress.md",
        "Stage: M6-WP2/WP3/WP4 resource classification, move analysis, and cleanup lowering complete");
    expect_document_contains(
        "docs/zh/progress.md", "阶段：M6-WP2/WP3/WP4 资源分类、whole-local move analysis 和 cleanup lowering 已完成");
    expect_document_contains("docs/en/next-steps.md", "Current Highest Priority: M6 Resource And Access Semantics");
    expect_document_contains("docs/zh/next-steps.md", "当前最高优先级：M6 Resource And Access Semantics");
    expect_document_contains(
        "docs/en/version.md", "M6-WP2/WP3/WP4 Resource Classification, Move Analysis, And Cleanup Baseline");
    expect_document_contains("docs/zh/version.md", "M6-WP2/WP3/WP4 资源分类、move analysis 与 cleanup 基线");
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
    expect_document_contains("docs/en/next-steps.md", "The next implementation package is M6-WP5 Destructor Protocol");
    expect_document_contains("docs/zh/next-steps.md", "下一实现包是 M6-WP5 Destructor Protocol");
    expect_document_contains("docs/en/m6-resource-access-semantics-design.md", "`NeedsDrop`");
    expect_document_contains("docs/zh/m6-resource-access-semantics-design.md", "`NeedsDrop`");
    expect_document_contains("docs/en/m6-resource-access-semantics-design.md", "Third Review Pass: User Cases");
    expect_document_contains("docs/zh/m6-resource-access-semantics-design.md", "第三轮审视：用户案例");
}

} // namespace aurex::test
