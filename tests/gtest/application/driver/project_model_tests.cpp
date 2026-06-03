#include <aurex/application/driver/compiler.hpp>
#include <aurex/application/driver/file_cache.hpp>
#include <aurex/application/driver/incremental_cache.hpp>
#include <aurex/application/driver/package_identity.hpp>
#include <aurex/application/driver/project_model.hpp>
#include <aurex/infrastructure/project/project_model.hpp>

#include <support/test_support.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <application/driver/incremental_cache/subjects/private/detail.hpp>

namespace aurex::test {
namespace {

constexpr std::string_view PROJECT_MODEL_ROOT_PACKAGE = "project.model.root";
constexpr std::string_view PROJECT_MODEL_IMPORT_A_PACKAGE = "project.model.import.a";
constexpr std::string_view PROJECT_MODEL_IMPORT_B_PACKAGE = "project.model.import.b";
constexpr std::string_view PROJECT_MODEL_BUFFER_A_TEXT = "module project_model.a;\n";
constexpr std::string_view PROJECT_MODEL_BUFFER_Z_TEXT = "module project_model.z;\n";
constexpr std::string_view PROJECT_MODEL_NO_PRUNING_FIRST_SOURCE = "module project_model_no_pruning;\n"
                                                                   "fn main() -> i32 { return 0; }\n";
constexpr std::string_view PROJECT_MODEL_NO_PRUNING_COMMENT_SOURCE =
    "module project_model_no_pruning;\n"
    "// comment-only edit still changes the coarse source fingerprint\n"
    "fn main() -> i32 { return 0; }\n";
constexpr base::u64 PROJECT_MODEL_BUFFER_A_GENERATION = 2;
constexpr base::u64 PROJECT_MODEL_BUFFER_Z_GENERATION = 1;
constexpr int PROJECT_MODEL_UNKNOWN_ENUM_VALUE = 99;

struct ProjectModelEmitCase {
    driver::EmitKind kind;
    std::string_view name;
};

struct ProjectModelOptimizationCase {
    ir::OptimizationLevel level;
    std::string_view name;
};

struct ProjectModelDiagnosticCase {
    driver::DiagnosticOutputFormat format;
    std::string_view name;
};

constexpr std::array PROJECT_MODEL_EMIT_CASES{
    ProjectModelEmitCase{driver::EmitKind::tokens, "tokens"},
    ProjectModelEmitCase{driver::EmitKind::lossless, "lossless"},
    ProjectModelEmitCase{driver::EmitKind::ast, "ast"},
    ProjectModelEmitCase{driver::EmitKind::modules, "modules"},
    ProjectModelEmitCase{driver::EmitKind::checked, "checked"},
    ProjectModelEmitCase{driver::EmitKind::typed, "typed"},
    ProjectModelEmitCase{driver::EmitKind::ir, "ir"},
    ProjectModelEmitCase{driver::EmitKind::llvm_ir, "llvm_ir"},
    ProjectModelEmitCase{driver::EmitKind::check, "check"},
    ProjectModelEmitCase{driver::EmitKind::assembly, "assembly"},
    ProjectModelEmitCase{driver::EmitKind::object, "object"},
    ProjectModelEmitCase{driver::EmitKind::executable, "executable"},
};

constexpr std::array PROJECT_MODEL_OPTIMIZATION_CASES{
    ProjectModelOptimizationCase{ir::OptimizationLevel::none, "none"},
    ProjectModelOptimizationCase{ir::OptimizationLevel::basic, "basic"},
    ProjectModelOptimizationCase{ir::OptimizationLevel::standard, "standard"},
    ProjectModelOptimizationCase{ir::OptimizationLevel::aggressive, "aggressive"},
};

constexpr std::array PROJECT_MODEL_DIAGNOSTIC_CASES{
    ProjectModelDiagnosticCase{driver::DiagnosticOutputFormat::text, "text"},
    ProjectModelDiagnosticCase{driver::DiagnosticOutputFormat::json, "json"},
};

void write_project_model_test_file(const fs::path& path, const std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out) << path;
    out << text;
}

[[nodiscard]] query::PackageKey project_model_test_package(const std::string_view identity)
{
    return driver::package_key_from_identity(identity);
}

[[nodiscard]] project::ProjectOpenBuffer project_model_open_buffer(
    const fs::path& path, const std::string_view uri, const std::string_view text, const base::u64 generation)
{
    return project::ProjectOpenBuffer{
        path,
        std::string(uri),
        std::string(PROJECT_MODEL_ROOT_PACKAGE),
        std::string(uri),
        query::SourceRole::virtual_buffer,
        project::project_open_buffer_fingerprint(text),
        static_cast<base::u64>(text.size()),
        generation,
    };
}

[[nodiscard]] driver::CompilerInvocation project_model_base_invocation(const fs::path& input)
{
    driver::CompilerInvocation invocation;
    invocation.input_path = input;
    invocation.package_identity = std::string(PROJECT_MODEL_ROOT_PACKAGE);
    invocation.emit_kind = driver::EmitKind::check;
    return invocation;
}

} // namespace

TEST_F(AurexIntegrationTest, ModuleLoaderProjectModelNormalizesSortsAndSerializesWorkspaceInputs)
{
    const fs::path work = tmp_root() / "project-model-normalization";
    const fs::path package_root = work / "pkg";
    const fs::path import_a = work / "imports" / "a";
    const fs::path import_b = work / "imports" / "b";
    const fs::path buffer_a = package_root / "src" / "a.ax";
    const fs::path buffer_z = package_root / "src" / "z.ax";
    write_project_model_test_file(buffer_a, PROJECT_MODEL_BUFFER_A_TEXT);
    write_project_model_test_file(buffer_z, PROJECT_MODEL_BUFFER_Z_TEXT);
    fs::create_directories(import_a);
    fs::create_directories(import_b);

    project::ProjectModelInput input{
        project::ProjectSessionKind::tooling,
        package_root / ".",
        {},
        std::string(PROJECT_MODEL_ROOT_PACKAGE),
        project_model_test_package(PROJECT_MODEL_ROOT_PACKAGE),
        {
            project::ProjectImportRoot{
                import_b,
                std::string(PROJECT_MODEL_IMPORT_B_PACKAGE),
                project_model_test_package(PROJECT_MODEL_IMPORT_B_PACKAGE),
            },
            project::ProjectImportRoot{
                import_a,
                std::string(PROJECT_MODEL_IMPORT_A_PACKAGE),
                project_model_test_package(PROJECT_MODEL_IMPORT_A_PACKAGE),
            },
        },
        project::ProjectTargetConfig{
            "llvm_ir",
            "aggressive",
            "clang++",
            {"-stdlib=libc++", "-DAUREX_TEST=1"},
        },
        project::ProjectCommandOptions{
            "json",
            false,
        },
        {
            project_model_open_buffer(
                buffer_z, "file:///project-model/z.ax", PROJECT_MODEL_BUFFER_Z_TEXT, PROJECT_MODEL_BUFFER_Z_GENERATION),
            project_model_open_buffer(
                buffer_a, "file:///project-model/a.ax", PROJECT_MODEL_BUFFER_A_TEXT, PROJECT_MODEL_BUFFER_A_GENERATION),
        },
    };

    const project::ProjectModel model = project::build_project_model(input);
    EXPECT_EQ(model.source_root, model.package_root);
    ASSERT_EQ(model.import_roots.size(), 2U);
    EXPECT_EQ(model.import_roots.front().root, project::canonical_or_absolute_project_path(import_a));
    EXPECT_EQ(model.import_roots.back().root, project::canonical_or_absolute_project_path(import_b));
    ASSERT_EQ(model.open_buffers.size(), 2U);
    EXPECT_EQ(model.open_buffers.front().path, project::canonical_or_absolute_project_path(buffer_a));
    EXPECT_EQ(model.open_buffers.back().path, project::canonical_or_absolute_project_path(buffer_z));
    EXPECT_NE(model.global_id, 0U);
    EXPECT_EQ(model.key.identity, model.identity);

    const project::ProjectModel deterministic_model = project::build_project_model(std::move(input));
    EXPECT_EQ(deterministic_model.identity, model.identity);
    EXPECT_EQ(deterministic_model.global_id, model.global_id);

    const std::string target = project::stable_serialize(model.target);
    EXPECT_NE(target.find("-stdlib=libc++"), std::string::npos);
    EXPECT_NE(target.find("-DAUREX_TEST=1"), std::string::npos);
    EXPECT_EQ(project::stable_serialize(model.command_options), "options{diagnostics=json,query_pruning=0}");
    EXPECT_EQ(project::debug_string(model), project::stable_serialize(model));

    const std::array projects{model};
    const project::WorkspaceModel workspace =
        project::build_workspace_model(std::span<const project::ProjectModel>(projects.data(), projects.size()));
    EXPECT_EQ(workspace.projects.size(), projects.size());
    EXPECT_NE(workspace.global_id, 0U);
    EXPECT_EQ(project::debug_string(workspace), project::stable_serialize(workspace));
    EXPECT_NE(project::stable_serialize(workspace).find("projects=1"), std::string::npos);
}

TEST_F(AurexIntegrationTest, ModuleLoaderProjectModelMapsManifestSourceRootAndTargetOptions)
{
    const fs::path work = tmp_root() / "project-model-manifest";
    const fs::path root = work / "root";
    const fs::path source_root = root / "src";
    const fs::path source = source_root / "app" / "main.ax";
    const fs::path import_root = work / "imported";
    const fs::path import_source_root = import_root / "lib";
    write_project_model_test_file(root / "aurex.toml",
        "[package]\n"
        "name = \"root-project-model\"\n"
        "version = \"1.2.3\"\n"
        "source-root = \"src\"\n");
    write_project_model_test_file(source, "module app.main;\n");
    write_project_model_test_file(import_root / "aurex.toml",
        "[package]\n"
        "name = \"import-project-model\"\n"
        "version = \"9.8.7\"\n"
        "source-root = \"lib\"\n");
    fs::create_directories(import_source_root);

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.import_paths.push_back(import_root);
    invocation.emit_kind = driver::EmitKind::llvm_ir;
    invocation.optimization_level = ir::OptimizationLevel::aggressive;
    invocation.diagnostic_format = driver::DiagnosticOutputFormat::json;
    invocation.query_pruning_enabled = false;
    invocation.clang_path = "clang++";
    invocation.clang_args = {"-fuse-ld=lld", "-Wl,--fatal-warnings"};

    const project::ProjectModel model = driver::project_model_from_invocation(invocation);
    EXPECT_EQ(model.session_kind, project::ProjectSessionKind::cli);
    EXPECT_EQ(model.package_root, project::canonical_or_absolute_project_path(root));
    EXPECT_EQ(model.source_root, project::canonical_or_absolute_project_path(source_root));
    EXPECT_FALSE(model.package_identity.empty());
    EXPECT_EQ(model.target.emit_kind, "llvm_ir");
    EXPECT_EQ(model.target.optimization_level, "aggressive");
    EXPECT_EQ(model.target.clang_path, "clang++");
    ASSERT_EQ(model.target.clang_args.size(), 2U);
    EXPECT_EQ(model.command_options.diagnostic_format, "json");
    EXPECT_FALSE(model.command_options.query_pruning_enabled);
    ASSERT_EQ(model.import_roots.size(), 1U);
    EXPECT_EQ(model.import_roots.front().root, project::canonical_or_absolute_project_path(import_source_root));
    EXPECT_TRUE(model.open_buffers.empty());

    const project::WorkspaceModel workspace = driver::workspace_model_from_invocation(invocation);
    ASSERT_EQ(workspace.projects.size(), 1U);
    EXPECT_EQ(workspace.projects.front().identity, model.identity);
    EXPECT_NE(workspace.global_id, 0U);
}

TEST_F(AurexIntegrationTest, ModuleLoaderProjectModelCoversDriverEnumMappings)
{
    const fs::path work = tmp_root() / "project-model-enum-mapping";
    const fs::path source = work / "main.ax";
    write_project_model_test_file(source, "module project_model_enum;\n");

    for (const ProjectModelEmitCase& emit_case : PROJECT_MODEL_EMIT_CASES) {
        driver::CompilerInvocation invocation = project_model_base_invocation(source);
        invocation.emit_kind = emit_case.kind;
        const project::ProjectModel model = driver::project_model_from_invocation(invocation);
        EXPECT_EQ(model.target.emit_kind, std::string(emit_case.name));
    }

    for (const ProjectModelOptimizationCase& optimization_case : PROJECT_MODEL_OPTIMIZATION_CASES) {
        driver::CompilerInvocation invocation = project_model_base_invocation(source);
        invocation.optimization_level = optimization_case.level;
        const project::ProjectModel model = driver::project_model_from_invocation(invocation);
        EXPECT_EQ(model.target.optimization_level, std::string(optimization_case.name));
    }

    for (const ProjectModelDiagnosticCase& diagnostic_case : PROJECT_MODEL_DIAGNOSTIC_CASES) {
        driver::CompilerInvocation invocation = project_model_base_invocation(source);
        invocation.diagnostic_format = diagnostic_case.format;
        const project::ProjectModel model = driver::project_model_from_invocation(invocation);
        EXPECT_EQ(model.command_options.diagnostic_format, std::string(diagnostic_case.name));
    }

    driver::CompilerInvocation unknown_emit = project_model_base_invocation(source);
    unknown_emit.emit_kind = static_cast<driver::EmitKind>(PROJECT_MODEL_UNKNOWN_ENUM_VALUE);
    EXPECT_EQ(driver::project_model_from_invocation(unknown_emit).target.emit_kind, "unknown");

    driver::CompilerInvocation unknown_optimization = project_model_base_invocation(source);
    unknown_optimization.optimization_level = static_cast<ir::OptimizationLevel>(PROJECT_MODEL_UNKNOWN_ENUM_VALUE);
    EXPECT_EQ(driver::project_model_from_invocation(unknown_optimization).target.optimization_level, "unknown");

    driver::CompilerInvocation unknown_diagnostic = project_model_base_invocation(source);
    unknown_diagnostic.diagnostic_format =
        static_cast<driver::DiagnosticOutputFormat>(PROJECT_MODEL_UNKNOWN_ENUM_VALUE);
    EXPECT_EQ(driver::project_model_from_invocation(unknown_diagnostic).command_options.diagnostic_format, "unknown");
}

TEST_F(AurexIntegrationTest, ModuleLoaderProjectModelSkipsInvalidProjectGraphSubject)
{
    namespace cache_detail = driver::incremental_cache_detail;

    cache_detail::QuerySubjectCollection collection;
    collection.module_graphs.push_back(cache_detail::ModuleGraphQuerySubject{});
    const project::ProjectModel invalid_model;
    const std::array invalid_modules{driver::ModuleRecord{}};
    const std::span<const driver::ModuleRecord> invalid_module_span{invalid_modules.data(), invalid_modules.size()};

    cache_detail::collect_project_graph_query_subjects(collection, invalid_model, invalid_module_span);
    EXPECT_TRUE(collection.project_graphs.empty());
    ASSERT_EQ(collection.module_graphs.size(), 1U);
    EXPECT_FALSE(query::is_valid(collection.module_graphs.front().project));

    const sema::CheckedModule checked;
    EXPECT_FALSE(cache_detail::collect_module_graph_query_subjects(invalid_module_span).empty());
    EXPECT_TRUE(cache_detail::collect_module_part_query_subjects(invalid_module_span).empty());
    EXPECT_FALSE(cache_detail::collect_module_exports_query_subjects(invalid_module_span, checked, nullptr).empty());
    EXPECT_TRUE(
        cache_detail::collect_module_package_exports_query_subjects(invalid_module_span, checked, nullptr).empty());
    EXPECT_FALSE(cache_detail::collect_item_list_query_subjects(invalid_module_span, checked, nullptr).empty());
}

TEST_F(AurexIntegrationTest, ModuleLoaderProjectModelNoPruningCacheRejectsChangedSourceAfterProjectInputsMatch)
{
    driver::clear_file_cache();

    const fs::path work = tmp_root() / "project-model-no-pruning-cache";
    const fs::path source = work / "main.ax";
    const fs::path cache = work / "main.axic";
    write_project_model_test_file(source, PROJECT_MODEL_NO_PRUNING_FIRST_SOURCE);

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.query_pruning_enabled = false;

    driver::Compiler compiler;
    const auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    ASSERT_TRUE(fs::exists(cache));

    write_project_model_test_file(source, PROJECT_MODEL_NO_PRUNING_COMMENT_SOURCE);
    driver::clear_file_cache();
    const auto reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(reuse) << reuse.error().message;
    EXPECT_FALSE(reuse.value());

    driver::clear_file_cache();
}

} // namespace aurex::test
