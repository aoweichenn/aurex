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

[[nodiscard]] inline std::string driver_invalid_optimization_level_message(const std::string_view level)
{
    return "invalid optimization level: " + std::string(level);
}

inline constexpr std::string_view DRIVER_MULTIPLE_INPUT_FILES_UNSUPPORTED = "multiple input files are not supported";

inline constexpr std::string_view DRIVER_ERROR_PREFIX = "aurexc: ";

} // namespace aurex::driver
