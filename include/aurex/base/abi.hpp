#pragma once

#include <string>
#include <string_view>

namespace aurex::base::abi {

inline constexpr std::string_view internal_symbol_prefix = "m0";
inline constexpr std::string_view std_abi_version = "v0";
inline constexpr std::string_view std_support_namespace = "aurex_std_v0";
inline constexpr std::string_view std_support_legacy_namespace = "aurex_std";

[[nodiscard]] inline std::string std_support_symbol(const std::string_view name) {
    std::string symbol(std_support_namespace);
    symbol += "_";
    symbol += name;
    return symbol;
}

[[nodiscard]] inline std::string legacy_std_support_symbol(const std::string_view name) {
    std::string symbol(std_support_legacy_namespace);
    symbol += "_";
    symbol += name;
    return symbol;
}

} // namespace aurex::base::abi
