#include <aurex/application/driver/package_identity.hpp>
#include <aurex/application/driver/project_model.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::driver {
namespace {

[[nodiscard]] std::string emit_kind_name(const EmitKind emit_kind)
{
    switch (emit_kind) {
        case EmitKind::tokens:
            return "tokens";
        case EmitKind::lossless:
            return "lossless";
        case EmitKind::ast:
            return "ast";
        case EmitKind::modules:
            return "modules";
        case EmitKind::checked:
            return "checked";
        case EmitKind::typed:
            return "typed";
        case EmitKind::ir:
            return "ir";
        case EmitKind::llvm_ir:
            return "llvm_ir";
        case EmitKind::check:
            return "check";
        case EmitKind::assembly:
            return "assembly";
        case EmitKind::object:
            return "object";
        case EmitKind::executable:
            return "executable";
    }
    return "unknown";
}

[[nodiscard]] std::string optimization_level_name(const ir::OptimizationLevel level)
{
    switch (level) {
        case ir::OptimizationLevel::none:
            return "none";
        case ir::OptimizationLevel::basic:
            return "basic";
        case ir::OptimizationLevel::standard:
            return "standard";
        case ir::OptimizationLevel::aggressive:
            return "aggressive";
    }
    return "unknown";
}

[[nodiscard]] std::string diagnostic_format_name(const DiagnosticOutputFormat format)
{
    switch (format) {
        case DiagnosticOutputFormat::text:
            return "text";
        case DiagnosticOutputFormat::json:
            return "json";
    }
    return "unknown";
}

[[nodiscard]] std::filesystem::path package_root_for_invocation(const CompilerInvocation& invocation)
{
    const std::optional<PackageManifestInfo> manifest = package_manifest_info_for_path(invocation.input_path);
    if (manifest.has_value()) {
        return manifest->manifest_root;
    }
    return project::canonical_or_absolute_project_path(invocation.input_path).parent_path();
}

[[nodiscard]] std::filesystem::path source_root_for_invocation(const CompilerInvocation& invocation)
{
    if (const std::optional<std::filesystem::path> source_root = package_source_root_for_invocation(invocation)) {
        return *source_root;
    }
    return package_root_for_invocation(invocation);
}

[[nodiscard]] project::ProjectTargetConfig project_target_config_from_invocation(const CompilerInvocation& invocation)
{
    return project::ProjectTargetConfig{
        emit_kind_name(invocation.emit_kind),
        optimization_level_name(invocation.optimization_level),
        invocation.clang_path,
        invocation.clang_args,
    };
}

[[nodiscard]] project::ProjectCommandOptions project_command_options_from_invocation(
    const CompilerInvocation& invocation)
{
    return project::ProjectCommandOptions{
        diagnostic_format_name(invocation.diagnostic_format),
        invocation.query_pruning_enabled,
    };
}

[[nodiscard]] std::vector<project::ProjectImportRoot> project_import_roots_from_invocation(
    const CompilerInvocation& invocation)
{
    std::vector<project::ProjectImportRoot> roots;
    roots.reserve(invocation.import_paths.size());
    for (const std::filesystem::path& import_path : invocation.import_paths) {
        const std::filesystem::path canonical_root = project::canonical_or_absolute_project_path(import_path);
        const std::filesystem::path source_root =
            package_source_root_for_import_root(canonical_root.string()).value_or(canonical_root);
        const std::string identity = package_identity_for_import_root(canonical_root.string());
        roots.push_back(project::ProjectImportRoot{
            source_root,
            identity,
            package_key_from_identity(identity),
        });
    }
    return roots;
}

} // namespace

project::ProjectModel project_model_from_invocation(const CompilerInvocation& invocation)
{
    const std::string package_identity = package_identity_for_invocation(invocation);
    return project::build_project_model(project::ProjectModelInput{
        project::ProjectSessionKind::cli,
        package_root_for_invocation(invocation),
        source_root_for_invocation(invocation),
        package_identity,
        package_key_from_identity(package_identity),
        project_import_roots_from_invocation(invocation),
        project_target_config_from_invocation(invocation),
        project_command_options_from_invocation(invocation),
        {},
    });
}

project::WorkspaceModel workspace_model_from_invocation(const CompilerInvocation& invocation)
{
    const project::ProjectModel model = project_model_from_invocation(invocation);
    const std::array projects{model};
    return project::build_workspace_model(std::span<const project::ProjectModel>(projects.data(), projects.size()));
}

} // namespace aurex::driver
