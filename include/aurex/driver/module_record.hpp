#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/query/query_key.hpp>
#include <aurex/syntax/ast/nodes.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace aurex::driver {

enum class ModulePartRecordKind {
    primary,
    named,
};

struct ModulePartRecord {
    std::string name;
    std::filesystem::path path;
    base::u32 stable_index = 0;
    ModulePartRecordKind kind = ModulePartRecordKind::primary;
};

struct ModuleImportRecord {
    std::string owner_part;
    std::string module_name;
    std::string alias;
    query::PackageKey module_package;
    bool owner_is_primary = false;
    syntax::Visibility visibility = syntax::Visibility::private_;
    bool is_public = false;
};

struct ModuleRecord {
    std::string name;
    std::filesystem::path path;
    query::PackageKey package;
    syntax::ModuleId id = syntax::INVALID_MODULE_ID;
    std::vector<ModulePartRecord> parts{};
    std::vector<ModuleImportRecord> imports{};
};

} // namespace aurex::driver
