#pragma once

#include "nex/error.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nex {

namespace fs = std::filesystem;

// Represents a source file node in the dependency graph
struct SourceNode {
    std::string name;                    // module name
    fs::path    path;                    // file path on disk
    std::vector<int> deps;              // indices into nodes[] for imports
    int         indegree = 0;
};

// The full dependency graph for one compilation target
struct DependencyGraph {
    std::vector<SourceNode> nodes;       // all source nodes
    std::vector<int>        order;       // topologically sorted indices
    int                     entry_idx = -1;

    bool empty() const noexcept { return nodes.empty(); }
};

// Build a dependency graph starting from an entry file.
// Recursively scans all transitive imports.
Result<DependencyGraph> build_graph(
    const fs::path& entry_path,
    const std::vector<fs::path>& import_dirs);

} // namespace nex
