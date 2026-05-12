#pragma once

#include <string>
#include <string_view>

namespace aurex::backend {

inline constexpr std::string_view BACKEND_LLVM_NULL_MODULE =
    "LLVM backend received a null IR module";

[[nodiscard]] inline std::string backend_llvm_target_lookup_failed_message(
    const std::string_view triple,
    const std::string_view error
) {
    return "LLVM target lookup failed for " + std::string(triple) + ": " + std::string(error);
}

[[nodiscard]] inline std::string backend_llvm_target_machine_creation_failed_message(const std::string_view triple) {
    return "LLVM target machine creation failed for " + std::string(triple);
}

} // namespace aurex::backend
