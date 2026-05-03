#include "nex/graph.hpp"
#include "nex/scanner.hpp"

#include <algorithm>
#include <fstream>
#include <queue>
#include <sstream>
#include <unordered_map>

namespace nex {

Result<DependencyGraph> build_graph(
    const fs::path& entry_path,
    const std::vector<fs::path>& import_dirs)
{
    DependencyGraph g;
    std::unordered_map<std::string, int> module_index; // name -> node index

    // Read file helper
    auto read_text = [](const fs::path& p) -> std::optional<std::string> {
        std::ifstream in(p, std::ios::binary);
        if (!in) return std::nullopt;
        std::ostringstream buf;
        buf << in.rdbuf();
        return buf.str();
    };

    // DFS-based graph building
    struct Frame { std::string name; fs::path path; };
    std::vector<Frame> stack;
    auto source = read_text(entry_path);
    if (!source) return Result<DependencyGraph>::err(ErrorCode::io_error,
        "cannot read: " + entry_path.string());
    auto info = scan_source(*source);
    stack.push_back(Frame{info.name, std::move(entry_path)});

    // For cycle detection: track path being explored
    std::unordered_set<std::string> visiting;

    while (!stack.empty()) {
        auto [mod_name, file_path] = std::move(stack.back());
        stack.pop_back();

        if (mod_name.empty()) continue;
        if (module_index.contains(mod_name)) continue; // already processed

        if (visiting.contains(mod_name)) {
            return Result<DependencyGraph>::err(ErrorCode::cycle_detected,
                "circular import involving: " + mod_name);
        }
        visiting.insert(mod_name);

        auto src = read_text(file_path);
        if (!src) {
            return Result<DependencyGraph>::err(ErrorCode::io_error,
                "cannot read: " + file_path.string());
        }
        auto mod_info = scan_source(*src);

        if (mod_info.name.empty()) {
            mod_info.name = mod_name;
        }

        // Ensure node exists
        int idx = static_cast<int>(g.nodes.size());
        if (module_index.contains(mod_info.name)) {
            idx = module_index[mod_info.name];
        } else {
            g.nodes.push_back(SourceNode{mod_info.name, file_path, {}, 0});
            module_index[mod_info.name] = idx;
        }

        if (mod_info.name == mod_name && module_index.size() == 1) {
            g.entry_idx = idx;
        }

        // Process imports
        for (auto& imp : mod_info.imports) {
            auto resolved = resolve_module(imp, import_dirs);
            if (!resolved) {
                visiting.erase(mod_name);
                return Result<DependencyGraph>::err(ErrorCode::resolve_error,
                    "cannot resolve import '" + imp + "' from module '" + mod_info.name + "'");
            }

            auto imp_src = read_text(*resolved);
            if (!imp_src) {
                visiting.erase(mod_name);
                return Result<DependencyGraph>::err(ErrorCode::io_error,
                    "cannot read imported file: " + resolved->string());
            }
            auto imp_info = scan_source(*imp_src);

            int dep_idx = static_cast<int>(g.nodes.size());
            auto it = module_index.find(imp_info.name);
            if (it != module_index.end()) {
                dep_idx = it->second;
                // Add edge if not already present
                auto& deps = g.nodes[idx].deps;
                if (std::find(deps.begin(), deps.end(), dep_idx) == deps.end()) {
                    deps.push_back(dep_idx);
                    g.nodes[dep_idx].indegree++;
                }
            } else {
                g.nodes.push_back(SourceNode{imp_info.name, *resolved, {}, 0});
                module_index[imp_info.name] = dep_idx;
                g.nodes[idx].deps.push_back(dep_idx);
                g.nodes[dep_idx].indegree = 1;
                // Push to stack for recursive processing
                stack.push_back(Frame{std::move(imp_info.name), std::move(*resolved)});
            }
        }

        visiting.erase(mod_name);
    }

    if (g.nodes.empty()) return Result<DependencyGraph>::err(ErrorCode::parse_error, "no modules found");

    // Topological sort (Kahn's algorithm)
    std::queue<int> queue;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (g.nodes[i].indegree == 0) queue.push(static_cast<int>(i));
    }

    g.order.reserve(g.nodes.size());
    while (!queue.empty()) {
        int u = queue.front(); queue.pop();
        g.order.push_back(u);
        for (int v : g.nodes[u].deps) {
            if (--g.nodes[v].indegree == 0) queue.push(v);
        }
    }

    if (g.order.size() != g.nodes.size()) {
        return Result<DependencyGraph>::err(ErrorCode::cycle_detected, "circular import dependency detected");
    }

    return Result<DependencyGraph>::ok(std::move(g));
}

} // namespace nex
