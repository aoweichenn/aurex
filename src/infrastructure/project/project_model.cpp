#include <aurex/infrastructure/project/project_model.hpp>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>

namespace aurex::project {
namespace {

constexpr base::u64 PROJECT_MODEL_ID_MARKER = 0x415850524f4a3031ULL;
constexpr base::u64 PROJECT_WORKSPACE_ID_MARKER = 0x4158574b53503131ULL;
constexpr std::string_view PROJECT_MODEL_SERIALIZATION_TAG = "aurex-project-model-v1";
constexpr std::string_view PROJECT_WORKSPACE_SERIALIZATION_TAG = "aurex-workspace-model-v1";
constexpr std::string_view PROJECT_OPEN_BUFFER_FINGERPRINT_TAG = "project-open-buffer:v1";

[[nodiscard]] base::u64 nonzero_project_global_id(
    const base::u64 marker, const query::StableFingerprint128 fingerprint) noexcept
{
    base::u64 global_id = query::stable_mix(marker, fingerprint.primary);
    global_id = query::stable_mix(global_id, fingerprint.secondary);
    global_id = query::stable_mix(global_id, fingerprint.byte_count);
    return global_id == 0 ? marker : global_id;
}

void mix_string_vector(query::StableHashBuilder& builder, const std::vector<std::string>& values) noexcept
{
    builder.mix_u64(static_cast<base::u64>(values.size()));
    for (base::usize index = 0; index < values.size(); ++index) {
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_string(values[index]);
    }
}

void mix_target_config(query::StableHashBuilder& builder, const ProjectTargetConfig& target) noexcept
{
    builder.mix_string(target.emit_kind);
    builder.mix_string(target.optimization_level);
    builder.mix_string(target.clang_path);
    mix_string_vector(builder, target.clang_args);
}

void mix_command_options(query::StableHashBuilder& builder, const ProjectCommandOptions& options) noexcept
{
    builder.mix_string(options.diagnostic_format);
    builder.mix_bool(options.query_pruning_enabled);
}

void mix_package_key(query::StableHashBuilder& builder, const query::PackageKey package) noexcept
{
    builder.mix_fingerprint(package.identity);
    builder.mix_u64(package.global_id);
}

void mix_import_root(query::StableHashBuilder& builder, const ProjectImportRoot& root) noexcept
{
    builder.mix_string(root.root.generic_string());
    builder.mix_string(root.package_identity);
    mix_package_key(builder, root.package);
}

void mix_open_buffer(query::StableHashBuilder& builder, const ProjectOpenBuffer& buffer) noexcept
{
    builder.mix_string(buffer.path.generic_string());
    builder.mix_string(buffer.uri);
    builder.mix_string(buffer.package_identity);
    builder.mix_string(buffer.virtual_buffer_identity);
    builder.mix_u64(static_cast<base::u64>(buffer.source_role));
    builder.mix_fingerprint(buffer.text_fingerprint);
    builder.mix_u64(buffer.text_size);
    builder.mix_u64(buffer.generation);
}

[[nodiscard]] query::StableFingerprint128 project_model_fingerprint(const ProjectModelInput& input) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(PROJECT_MODEL_SERIALIZATION_TAG);
    builder.mix_u32(PROJECT_MODEL_SCHEMA_VERSION);
    builder.mix_u64(static_cast<base::u64>(input.session_kind));
    builder.mix_string(input.package_root.generic_string());
    builder.mix_string(input.source_root.generic_string());
    builder.mix_string(input.package_identity);
    mix_package_key(builder, input.package);
    builder.mix_u64(static_cast<base::u64>(input.import_roots.size()));
    for (base::usize index = 0; index < input.import_roots.size(); ++index) {
        builder.mix_u64(static_cast<base::u64>(index));
        mix_import_root(builder, input.import_roots[index]);
    }
    mix_target_config(builder, input.target);
    mix_command_options(builder, input.command_options);
    builder.mix_u64(static_cast<base::u64>(input.open_buffers.size()));
    for (base::usize index = 0; index < input.open_buffers.size(); ++index) {
        builder.mix_u64(static_cast<base::u64>(index));
        mix_open_buffer(builder, input.open_buffers[index]);
    }
    return builder.finish();
}

[[nodiscard]] bool import_root_less(const ProjectImportRoot& lhs, const ProjectImportRoot& rhs)
{
    return std::tie(lhs.root, lhs.package_identity, lhs.package.global_id)
        < std::tie(rhs.root, rhs.package_identity, rhs.package.global_id);
}

[[nodiscard]] bool open_buffer_less(const ProjectOpenBuffer& lhs, const ProjectOpenBuffer& rhs)
{
    return std::tie(lhs.path, lhs.uri, lhs.package_identity, lhs.virtual_buffer_identity, lhs.generation)
        < std::tie(rhs.path, rhs.uri, rhs.package_identity, rhs.virtual_buffer_identity, rhs.generation);
}

void normalize_project_input(ProjectModelInput& input)
{
    input.package_root = canonical_or_absolute_project_path(input.package_root);
    input.source_root =
        canonical_or_absolute_project_path(input.source_root.empty() ? input.package_root : input.source_root);
    for (ProjectImportRoot& root : input.import_roots) {
        root.root = canonical_or_absolute_project_path(root.root);
    }
    std::sort(input.import_roots.begin(), input.import_roots.end(), import_root_less);
    for (ProjectOpenBuffer& buffer : input.open_buffers) {
        buffer.path = canonical_or_absolute_project_path(buffer.path);
    }
    std::sort(input.open_buffers.begin(), input.open_buffers.end(), open_buffer_less);
}

void write_path_field(std::ostringstream& out, const std::string_view name, const std::filesystem::path& path)
{
    out << name << '=' << path.generic_string();
}

} // namespace

std::filesystem::path canonical_or_absolute_project_path(const std::filesystem::path& path)
{
    if (path.empty()) {
        return {};
    }

    std::error_code canonical_error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonical_error);
    if (!canonical_error) {
        return canonical.lexically_normal();
    }

    std::error_code absolute_error;
    std::filesystem::path absolute = std::filesystem::absolute(path, absolute_error);
    return absolute_error ? path.lexically_normal() : absolute.lexically_normal();
}

ProjectModel build_project_model(ProjectModelInput input)
{
    normalize_project_input(input);
    const query::StableFingerprint128 identity = project_model_fingerprint(input);
    return ProjectModel{
        input.session_kind,
        std::move(input.package_root),
        std::move(input.source_root),
        std::move(input.package_identity),
        input.package,
        std::move(input.import_roots),
        std::move(input.target),
        std::move(input.command_options),
        std::move(input.open_buffers),
        query::project_key(identity),
        identity,
        nonzero_project_global_id(PROJECT_MODEL_ID_MARKER, identity),
    };
}

WorkspaceModel build_workspace_model(const std::span<const ProjectModel> projects)
{
    query::StableHashBuilder builder;
    builder.mix_string(PROJECT_WORKSPACE_SERIALIZATION_TAG);
    builder.mix_u32(PROJECT_MODEL_SCHEMA_VERSION);
    builder.mix_u64(static_cast<base::u64>(projects.size()));
    for (base::usize index = 0; index < projects.size(); ++index) {
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_fingerprint(projects[index].identity);
        builder.mix_u64(projects[index].global_id);
    }
    const query::StableFingerprint128 identity = builder.finish();
    return WorkspaceModel{
        std::vector<ProjectModel>(projects.begin(), projects.end()),
        identity,
        nonzero_project_global_id(PROJECT_WORKSPACE_ID_MARKER, identity),
    };
}

query::StableFingerprint128 project_open_buffer_fingerprint(const std::string_view text) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(PROJECT_OPEN_BUFFER_FINGERPRINT_TAG);
    builder.mix_string(text);
    return builder.finish();
}

std::string stable_serialize(const ProjectTargetConfig& target)
{
    std::ostringstream out;
    out << "target{emit=" << target.emit_kind << ",opt=" << target.optimization_level << ",clang=" << target.clang_path
        << ",clang_args=" << target.clang_args.size();
    for (const std::string& arg : target.clang_args) {
        out << ',' << arg;
    }
    out << '}';
    return out.str();
}

std::string stable_serialize(const ProjectCommandOptions& options)
{
    std::ostringstream out;
    out << "options{diagnostics=" << options.diagnostic_format
        << ",query_pruning=" << (options.query_pruning_enabled ? 1 : 0) << '}';
    return out.str();
}

std::string stable_serialize(const ProjectModel& model)
{
    std::ostringstream out;
    out << PROJECT_MODEL_SERIALIZATION_TAG << "{schema=" << PROJECT_MODEL_SCHEMA_VERSION
        << ",session=" << static_cast<base::u32>(model.session_kind) << ',';
    write_path_field(out, "package_root", model.package_root);
    out << ',';
    write_path_field(out, "source_root", model.source_root);
    out << ",package=" << model.package_identity << ",project_key=" << model.key.global_id
        << ",imports=" << model.import_roots.size() << ",open_buffers=" << model.open_buffers.size()
        << ",identity=" << query::debug_string(model.identity) << ",global=" << model.global_id << '}';
    return out.str();
}

std::string stable_serialize(const WorkspaceModel& model)
{
    std::ostringstream out;
    out << PROJECT_WORKSPACE_SERIALIZATION_TAG << "{schema=" << PROJECT_MODEL_SCHEMA_VERSION
        << ",projects=" << model.projects.size() << ",identity=" << query::debug_string(model.identity)
        << ",global=" << model.global_id << '}';
    return out.str();
}

std::string debug_string(const ProjectModel& model)
{
    return stable_serialize(model);
}

std::string debug_string(const WorkspaceModel& model)
{
    return stable_serialize(model);
}

} // namespace aurex::project
