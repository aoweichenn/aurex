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

TEST_F(AurexIntegrationTest, M5DefaultTraitMethodsDesignIsPlanned)
{
    expect_document_contains("docs/en/README.md", "M5 Default Trait Methods Roadmap");
    expect_document_contains("docs/zh/README.md", "M5 Default Trait Methods 路线图");
    expect_document_contains("docs/en/progress.md", "Stage: M5 default trait methods design baseline active");
    expect_document_contains("docs/zh/progress.md", "阶段：M5 default trait methods design baseline 已启动");
    expect_document_contains("docs/en/next-steps.md", "Current Highest Priority: M5 Default Trait Methods Design");
    expect_document_contains("docs/zh/next-steps.md", "当前最高优先级：M5 Default Trait Methods 设计");
    expect_document_contains("docs/en/m5-roadmap.md", "M5-WP1: Research And Design Baseline");
    expect_document_contains("docs/zh/m5-roadmap.md", "M5-WP1：调研和设计基线");
    expect_document_contains("docs/en/m5-default-trait-methods-design.md", "TraitMethodDispatchKind");
    expect_document_contains("docs/zh/m5-default-trait-methods-design.md", "TraitMethodDispatchKind");
}

} // namespace aurex::test
