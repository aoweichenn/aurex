#include "nex/config.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace nex {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

TargetKind parse_kind(std::string_view v) {
    if (v == "static_library") return TargetKind::static_library;
    return TargetKind::executable;
}

} // namespace

Result<ProjectConfig> parse_nexfile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) return Result<ProjectConfig>::err(ErrorCode::io_error,
        "cannot open: " + path.string());

    ProjectConfig project;
    std::vector<TargetConfig> targets;
    TargetConfig* current = nullptr;
    int line_no = 0;

    for (std::string line; std::getline(in, line); ) {
        ++line_no;
        // strip comment
        auto comment = line.find('#');
        if (comment != std::string::npos) line.resize(comment);
        auto trimmed = trim(line);
        if (trimmed.empty()) continue;

        // section header: [target:name]
        if (trimmed.starts_with('[') && trimmed.ends_with(']')) {
            auto inner = trimmed.substr(1, trimmed.size() - 2);
            if (inner.starts_with("target:")) {
                std::string tname(inner.substr(7));
                targets.emplace_back();
                current = &targets.back();
                current->name = std::string(trim(tname));
            }
            continue;
        }

        // key = value
        auto eq = trimmed.find('=');
        if (eq == std::string_view::npos) {
            return Result<ProjectConfig>::err(ErrorCode::config_error,
                path.string() + ":" + std::to_string(line_no) + ": missing '='");
        }
        auto key = trim(trimmed.substr(0, eq));
        auto val = trim(trimmed.substr(eq + 1));

        auto set = [&](std::string_view k, auto& dest) {
            if (key == k) { dest = std::string(val); return true; }
            return false;
        };
        auto append = [&](std::string_view k, auto& vec) {
            if (key == k) { vec.push_back(std::string(val)); return true; }
            return false;
        };

        if (current) {
            if (set("entry", current->entry)) continue;
            if (set("output", current->output)) continue;
            if (append("import", current->import_paths)) continue;
            if (append("link", current->link_files)) continue;
            if (set("m0_flags", current->m0_flags)) continue;
            if (set("c_flags", current->c_flags)) continue;
            if (key == "kind") {
                current->kind = parse_kind(val);
                continue;
            }
        } else {
            if (set("project", project.project_name)) continue;
            // global level keys before any [target]
        }

        return Result<ProjectConfig>::err(ErrorCode::config_error,
            path.string() + ":" + std::to_string(line_no) +
            ": unknown key '" + std::string(key) + "'");
    }

    // Apply defaults
    for (auto& t : targets) {
        if (t.output.empty() && !t.entry.empty()) {
            auto name = fs::path(t.entry).filename().string();
            if (name.ends_with(".ax")) name.resize(name.size() - 3);
            t.output = name;
        }
    }

    project.targets = std::move(targets);
    return Result<ProjectConfig>::ok(std::move(project));
}

} // namespace nex
