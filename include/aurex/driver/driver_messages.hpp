#pragma once

#include <string>
#include <string_view>

namespace aurex::driver {

inline constexpr std::string_view DRIVER_OUTPUT_OPEN_FAILED = "failed to open output file";

inline constexpr std::string_view DRIVER_OUTPUT_WRITE_FAILED = "failed to write output file";

inline constexpr std::string_view DRIVER_NATIVE_OUTPUT_REQUIRES_OUTPUT_PATH = "native output requires -o";

inline constexpr std::string_view DRIVER_UNSUPPORTED_EMISSION_MODE = "unsupported emission mode";

inline constexpr std::string_view DRIVER_LLVM_BACKEND_UNAVAILABLE =
    "LLVM backend is unavailable for this compiler build";

inline constexpr std::string_view DRIVER_INPUT_OPEN_FAILED = "failed to open input file";

inline constexpr std::string_view DRIVER_INPUT_READ_FAILED = "failed to read input file";

inline constexpr std::string_view DRIVER_NATIVE_SUPPORT_SOURCES_EXECUTABLE_ONLY =
    "native support sources are only supported for executable output";

inline constexpr std::string_view DRIVER_CLANG_FORK_FAILED = "failed to fork clang process";

inline constexpr std::string_view DRIVER_CLANG_WAIT_FAILED = "failed to wait for clang process";

inline constexpr std::string_view DRIVER_CLANG_INVOCATION_FAILED = "clang invocation failed";

inline constexpr std::string_view DRIVER_MODULE_LOADING_FAILED = "module loading failed";

inline constexpr std::string_view DRIVER_IMPORT_DEPTH_EXCEEDED = "maximum import depth exceeded";

inline constexpr std::string_view DRIVER_IMPORTABLE_MODULE_DECL_REQUIRED =
    "module declaration is required for importable files";

[[nodiscard]] inline std::string driver_native_output_directory_failed_message(const std::string_view path)
{
    return "failed to create native output directory: " + std::string(path);
}

[[nodiscard]] inline std::string driver_cyclic_import_message(const std::string_view path)
{
    return "cyclic import involving: " + std::string(path);
}

[[nodiscard]] inline std::string driver_module_import_mismatch_message(
    const std::string_view declared, const std::string_view expected)
{
    return "module declaration '" + std::string(declared) + "' does not match import '" + std::string(expected) + "'";
}

[[nodiscard]] inline std::string driver_module_source_root_outside_message(
    const std::string_view file_path, const std::string_view source_root)
{
    return "module file " + std::string(file_path) + " is outside package source-root " + std::string(source_root);
}

[[nodiscard]] inline std::string driver_module_source_root_mismatch_message(
    const std::string_view declared, const std::string_view file_path, const std::string_view expected_path)
{
    return "module declaration '" + std::string(declared) + "' expects source path " + std::string(expected_path)
        + " but file is " + std::string(file_path);
}

[[nodiscard]] inline std::string driver_duplicate_module_name_message(
    const std::string_view module_name, const std::string_view loaded_path)
{
    return "duplicate module name '" + std::string(module_name) + "' already loaded from " + std::string(loaded_path);
}

[[nodiscard]] inline std::string driver_import_resolve_failed_message(
    const std::string_view module_name, const std::string_view candidates)
{
    return "failed to resolve import: " + std::string(module_name) + " (searched: " + std::string(candidates) + ")";
}

[[nodiscard]] inline std::string driver_import_ambiguous_message(
    const std::string_view module_name, const std::string_view candidates)
{
    return "ambiguous import: " + std::string(module_name) + " (candidates: " + std::string(candidates) + ")";
}

[[nodiscard]] inline std::string driver_missing_module_part_message(
    const std::string_view module_name, const std::string_view part_name, const std::string_view expected_path)
{
    return "module '" + std::string(module_name) + "' declares missing part '" + std::string(part_name)
        + "' (expected: " + std::string(expected_path) + ")";
}

[[nodiscard]] inline std::string driver_duplicate_module_part_message(
    const std::string_view module_name, const std::string_view part_name)
{
    return "duplicate module part '" + std::string(part_name) + "' in module '" + std::string(module_name) + "'";
}

[[nodiscard]] inline std::string driver_module_part_case_collision_message(
    const std::string_view module_name, const std::string_view first_part, const std::string_view second_part)
{
    return "module parts '" + std::string(first_part) + "' and '" + std::string(second_part) + "' in module '"
        + std::string(module_name) + "' differ only by case";
}

[[nodiscard]] inline std::string driver_module_part_imported_message(
    const std::string_view module_name, const std::string_view part_name)
{
    return "module part '" + std::string(part_name) + "' of module '" + std::string(module_name)
        + "' is not an importable module";
}

[[nodiscard]] inline std::string driver_module_part_header_mismatch_message(const std::string_view expected_module,
    const std::string_view expected_part, const std::string_view declared_module, const std::string_view declared_part)
{
    return "module part declaration '" + std::string(declared_module) + " part " + std::string(declared_part)
        + "' does not match expected '" + std::string(expected_module) + " part " + std::string(expected_part) + "'";
}

[[nodiscard]] inline std::string driver_module_part_root_owner_missing_message(
    const std::string_view part_name, const std::string_view expected_primary)
{
    return "module part root '" + std::string(part_name) + "' has no owning primary module at "
        + std::string(expected_primary);
}

[[nodiscard]] inline std::string driver_module_part_unlisted_root_message(
    const std::string_view part_name, const std::string_view primary)
{
    return "module part file '" + std::string(part_name) + "' is not listed by primary module " + std::string(primary);
}

[[nodiscard]] inline std::string driver_module_part_artifact_root_message(
    const std::string_view part_name, const std::string_view primary)
{
    return "cannot emit artifact from module part '" + std::string(part_name) + "'; compile owning primary module "
        + std::string(primary);
}

[[nodiscard]] inline std::string driver_invalid_optimization_level_message(const std::string_view level)
{
    return "invalid optimization level: " + std::string(level);
}

inline constexpr std::string_view DRIVER_MULTIPLE_INPUT_FILES_UNSUPPORTED = "multiple input files are not supported";

inline constexpr std::string_view DRIVER_ERROR_PREFIX = "aurexc: ";

} // namespace aurex::driver
