#include <support/test_support.hpp>

#include <array>
#include <string>
#include <string_view>

namespace aurex::test {
namespace {

constexpr std::array<std::string_view, 22> REQUIRED_CURRENT_DOCUMENTS = {
    "docs/README.md",
    "docs/zh/README.md",
    "docs/zh/architecture.md",
    "docs/zh/requirements.md",
    "docs/zh/runtime-flow.md",
    "docs/zh/api.md",
    "docs/zh/language-manual.md",
    "docs/zh/language-feature-inventory.md",
    "docs/zh/implementation.md",
    "docs/zh/usage.md",
    "docs/zh/introduction.md",
    "docs/zh/version.md",
    "docs/zh/next-steps.md",
    "docs/zh/progress.md",
    "docs/zh/syntax-revision-optimization/README.md",
    "docs/zh/syntax-revision-optimization/01-angle-bracket-generics.md",
    "docs/zh/syntax-revision-optimization/02-builtin-surface.md",
    "docs/zh/syntax-revision-optimization/03-range-loop-surface.md",
    "docs/zh/syntax-revision-optimization/04-mut-const-access-surface.md",
    "docs/zh/syntax-revision-optimization/05-function-closure-surface.md",
    "docs/zh/syntax-revision-optimization/06-function-closure-cpp-capture-list.md",
    "docs/zh/syntax-revision-optimization/07-builtin-member-projection.md",
};

constexpr std::array<std::string_view, 7> REMOVED_DOCUMENT_PATHS = {
    "docs/en",
    "docs/ARCHITECTURE.zh.md",
    "docs/DESIGN.en.md",
    "docs/DESIGN.zh.md",
    "docs/SELFHOST.md",
    "docs/SEMANTICS.md",
    "docs/zh/m3.1-generics-plan.md",
};

void expect_document_contains(const fs::path& path, const std::string_view text)
{
    const std::string document = read_text(source_root() / path);
    EXPECT_NE(document.find(text), std::string::npos) << path << " missing " << text;
}

void expect_document_missing(const fs::path& path, const std::string_view text)
{
    const std::string document = read_text(source_root() / path);
    EXPECT_EQ(document.find(text), std::string::npos) << path << " unexpectedly contains " << text;
}

} // namespace

TEST_F(AurexIntegrationTest, CurrentChineseDocumentationLayoutIsStable)
{
    for (const std::string_view path : REQUIRED_CURRENT_DOCUMENTS) {
        EXPECT_TRUE(fs::exists(source_root() / std::string(path))) << path;
    }

    for (const std::string_view path : REMOVED_DOCUMENT_PATHS) {
        EXPECT_FALSE(fs::exists(source_root() / std::string(path))) << path;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(source_root() / "docs")) {
        EXPECT_FALSE(entry.path().filename().string().rfind("M0V0.1.", 0) == 0) << entry.path();
    }
}

TEST_F(AurexIntegrationTest, CurrentDocumentationExplainsTheActiveRoadmap)
{
    expect_document_contains("README.md", "docs/zh/README.md");
    expect_document_missing("README.md", "docs/en/README.md");
    expect_document_contains("docs/README.md", "这一版只维护中文文档");
    expect_document_contains("docs/zh/README.md", "本目录是 Aurex 当前唯一维护的文档版本");
    expect_document_contains("docs/zh/progress.md", "泛型语法和实例化闭环");
    expect_document_contains("docs/zh/next-steps.md", "先把已落地语法面的边界继续收紧");
    expect_document_contains("docs/zh/version.md", "当前文档基线：中文文档精简版");
}

TEST_F(AurexIntegrationTest, SyntaxRevisionDocumentationMatchesCurrentSurface)
{
    expect_document_contains(
        "docs/zh/syntax-revision-optimization/README.md",
        "Builtin Surface：收窄类型查询和显式转换表面");
    expect_document_contains("docs/zh/syntax-revision-optimization/02-builtin-surface.md", "x as T");
    expect_document_contains("docs/zh/syntax-revision-optimization/02-builtin-surface.md", "sizeof<T>()");
    expect_document_contains("docs/zh/syntax-revision-optimization/02-builtin-surface.md", "alignof<T>()");
    expect_document_contains(
        "docs/zh/syntax-revision-optimization/02-builtin-surface.md",
        "低层指针、bitcast、字符串 raw/UTF-8 builtin 保持现有短写法");
    expect_document_missing("docs/zh/syntax-revision-optimization/README.md", "02-builtin-intrinsic-surface.md");
    const std::string reserved_namespace = std::string{"intrinsic"} + ".";
    expect_document_missing("docs/zh/syntax-revision-optimization/02-builtin-surface.md", reserved_namespace);
}

} // namespace aurex::test
