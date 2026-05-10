#include <aurex/syntax/module.hpp>

#include <aurex/base/abi.hpp>

#include <sstream>

namespace aurex::syntax {

std::filesystem::path module_path_to_relative_file(const ModulePath& path) {
    std::filesystem::path result;
    for (std::string_view part : path.parts) {
        result /= std::string(part);
    }
    result += ".ax";
    return result;
}

std::string module_path_to_string(const ModulePath& path) {
    std::ostringstream out;
    for (base::usize i = 0; i < path.parts.size(); ++i) {
        if (i != 0) {
            out << ".";
        }
        out << path.parts[i];
    }
    return out.str();
}

bool module_paths_equal(const ModulePath& lhs, const ModulePath& rhs) noexcept {
    if (lhs.parts.size() != rhs.parts.size()) {
        return false;
    }
    for (base::usize i = 0; i < lhs.parts.size(); ++i) {
        if (lhs.parts[i] != rhs.parts[i]) {
            return false;
        }
    }
    return true;
}

std::string mangle_c_symbol(const ModulePath& module, const std::string_view name) {
    std::string result(aurex::base::abi::AUREX_INTERNAL_SYMBOL_PREFIX);
    for (std::string_view part : module.parts) {
        result += "_";
        for (const char c : part) {
            const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
            result += alnum ? c : '_';
        }
    }
    result += "_";
    for (const char c : name) {
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        result += alnum ? c : '_';
    }
    return result;
}

} // namespace aurex::syntax
