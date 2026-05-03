#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace nex {

struct SourceInfo {
    std::string module_name;
    std::vector<std::string> imports;
};

// Scan an .ax source for module declaration and import declarations.
SourceInfo scan_source(std::string_view text);

} // namespace nex
