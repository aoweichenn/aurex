#include "nex/config.hpp"
#include "nex/scanner.hpp"
#include "nex/graph.hpp"
#include "nex/digest.hpp"
#include "nex/cache.hpp"
#include "nex/executor.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;
using namespace nex;

namespace {

void print_usage() {
    std::cout
        << "Nex — build tool for M0 projects\n"
        << "  nex [-j N] [Nexfile]\n"
        << "  defaults: ./Nexfile, -j = nproc\n";
}

struct BuildSpec {
    std::string              name;
    fs::path                 entry;
    fs::path                 output_dir;
    std::string              output_name;
    std::vector<fs::path>    import_dirs;
    std::vector<std::string> links;
    std::string              m0_flags;
    std::string              c_flags;
    DependencyGraph          graph;
};

Result<BuildSpec> prepare_target(const TargetConfig& t) {
    BuildSpec spec;
    spec.name        = t.name;
    spec.entry       = t.entry;
    spec.output_dir  = fs::path(".nex") / "build" / t.name;
    spec.output_name = t.output;
    spec.m0_flags    = t.m0_flags;
    spec.c_flags     = t.c_flags;

    for (auto& imp : t.import_paths) spec.import_dirs.push_back(imp);
    spec.links = t.link_files;

    auto result = build_graph(spec.entry, spec.import_dirs);
    if (!result) return Result<BuildSpec>::err(result.error());
    spec.graph = std::move(result.value());

    return Result<BuildSpec>::ok(std::move(spec));
}

std::string make_m0c_cmd(const std::string& entry, const std::string& output_c,
                          const std::vector<std::string>& imports, const std::string& extra) {
    std::ostringstream cmd;
    cmd << "m0c";
    for (auto& imp : imports) cmd << " -I " << imp;
    if (!extra.empty()) cmd << ' ' << extra;
    cmd << ' ' << entry << " -o " << output_c;
    return cmd.str();
}

} // namespace

int main(int argc, char** argv) {
    fs::path config_path = "Nexfile";
    int concurrency = 0;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "-h" || arg == "--help") { print_usage(); return 0; }
        if (arg == "-j" && i + 1 < argc) {
            concurrency = std::stoi(argv[++i]);
        } else if (arg.starts_with("-j") && arg.size() > 2) {
            concurrency = std::stoi(std::string(arg.substr(2)));
        } else if (!arg.empty() && arg.front() != '-') {
            config_path = arg;
        } else {
            std::cerr << "nex: unknown option: " << arg << '\n';
            return 2;
        }
    }

    auto cfg_result = parse_nexfile(config_path);
    if (!cfg_result) {
        std::cerr << "nex: " << cfg_result.error().message << '\n';
        return 1;
    }
    auto& cfg = cfg_result.value();

    // ── Prepare all targets ──
    std::vector<BuildSpec> specs;
    for (auto& t : cfg.targets) {
        auto r = prepare_target(t);
        if (!r) {
            std::cerr << "nex: target '" << t.name << "': " << r.error().message << '\n';
            return 1;
        }
        specs.push_back(std::move(r.value()));
    }

    // ── Cache ──
    fs::path cache_dir = ".nex";
    fs::create_directories(cache_dir);
    BuildCache cache(cache_dir / "cache");
    (void)cache.load();

    // ── Build jobs ──
    std::vector<BuildJob> jobs;

    for (auto& spec : specs) {
        // Collect all source file paths
        std::vector<fs::path> sources;
        for (auto& node : spec.graph.nodes) {
            sources.push_back(node.path);
        }

        // Check cache
        if (cache.all_match(sources)) {
            std::cout << "nex: [" << spec.name << "] cache hit, skipping\n";
            continue;
        }

        fs::create_directories(spec.output_dir);
        auto output_c = (spec.output_dir / (spec.output_name + ".c")).string();
        auto output_bin = (spec.output_dir / spec.output_name).string();

        std::vector<std::string> import_strs;
        for (auto& d : spec.import_dirs) import_strs.push_back(d.string());

        // m0c compile
        int m0_idx = static_cast<int>(jobs.size());
        jobs.push_back(BuildJob{
            spec.name + " (m0c)",
            make_m0c_cmd(spec.entry.string(), output_c, import_strs, spec.m0_flags),
            ""
        });

        // cc compile + link
        std::ostringstream cc_cmd;
        cc_cmd << "cc " << output_c;
        for (auto& l : spec.links) cc_cmd << ' ' << l;
        if (!spec.c_flags.empty()) cc_cmd << ' ' << spec.c_flags;
        cc_cmd << " -o " << output_bin;

        jobs.push_back(BuildJob{
            spec.name + " (cc)",
            cc_cmd.str(),
            ""
        });

        // Update cache after build
        for (auto& s : sources) {
            auto h = file_digest(s);
            if (!h.empty()) cache.set(s, std::move(h));
        }
    }

    if (jobs.empty()) {
        std::cout << "nex: everything up to date\n";
        (void)cache.save();
        return 0;
    }

    // ── Execute ──
    std::cout << "nex: " << jobs.size() << " job(s), " << concurrency << " worker(s)\n";
    Executor executor(concurrency);
    auto exec_result = executor.run(jobs);

    // Save cache
    (void)cache.save();

    if (!exec_result) {
        std::cerr << "nex: build failed: " << exec_result.error().message << '\n';
        return 1;
    }

    if (cfg.targets.size() == 1) {
        std::cout << "nex: build complete → " << specs[0].output_dir / specs[0].output_name << '\n';
    } else {
        std::cout << "nex: " << cfg.targets.size() << " target(s) built\n";
    }
    return 0;
}
