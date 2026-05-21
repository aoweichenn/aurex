#pragma once

#include <filesystem>
#include <string>

namespace aurex::driver {

struct ModuleRecord {
    std::string name;
    std::filesystem::path path;
};

} // namespace aurex::driver
