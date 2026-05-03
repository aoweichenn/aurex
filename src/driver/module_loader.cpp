#include "aurex/driver/module_loader.hpp"

#include "aurex/base/config.hpp"
#include "aurex/lex/lexer.hpp"
#include "aurex/parse/parser.hpp"
#include "aurex/driver/standard_library.hpp"
#include "aurex/syntax/module.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <utility>

namespace aurex::driver {

namespace {

[[nodiscard]] base::Result<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return base::Result<std::string>::fail({base::ErrorCode::io_error, "failed to open input file"});
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return base::Result<std::string>::ok(buffer.str());
}

[[nodiscard]] std::filesystem::path canonical_or_absolute(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path);
}

void push_error(base::DiagnosticSink& diagnostics, base::SourceRange range, std::string message) {
    diagnostics.push(base::Diagnostic {
        base::Severity::error,
        range,
        std::move(message),
    });
}

[[nodiscard]] std::optional<std::filesystem::path> find_import_file(
    const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir,
    const std::vector<std::filesystem::path>& import_paths
) {
    const std::filesystem::path relative = syntax::module_path_to_relative_file(path);
    std::vector<std::filesystem::path> roots;
    roots.push_back(importer_dir);
    for (const std::filesystem::path& import_path : import_paths) {
        roots.push_back(import_path);
    }

    for (const std::filesystem::path& root : roots) {
        const std::filesystem::path candidate = root / relative;
        std::error_code error;
        if (std::filesystem::exists(candidate, error) && !error) {
            return candidate;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<std::filesystem::path> import_candidates(
    const syntax::ModulePath& path,
    const std::filesystem::path& importer_dir,
    const std::vector<std::filesystem::path>& import_paths
) {
    const std::filesystem::path relative = syntax::module_path_to_relative_file(path);
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(importer_dir / relative);
    for (const std::filesystem::path& import_path : import_paths) {
        candidates.push_back(import_path / relative);
    }
    return candidates;
}

[[nodiscard]] std::string format_import_candidates(const std::span<const std::filesystem::path> candidates) {
    std::ostringstream out;
    for (base::usize i = 0; i < candidates.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << candidates[i].string();
    }
    return out.str();
}

struct IdMap {
    std::vector<syntax::TypeId> types;
    std::vector<syntax::ExprId> exprs;
    std::vector<syntax::StmtId> stmts;
    std::vector<syntax::ItemId> items;
};

[[nodiscard]] syntax::TypeId remap_type(const syntax::TypeId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.types.size() ? map.types[id.value] : syntax::invalid_type_id;
}

[[nodiscard]] syntax::ExprId remap_expr(const syntax::ExprId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.exprs.size() ? map.exprs[id.value] : syntax::invalid_expr_id;
}

[[nodiscard]] syntax::StmtId remap_stmt(const syntax::StmtId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.stmts.size() ? map.stmts[id.value] : syntax::invalid_stmt_id;
}

[[nodiscard]] syntax::ItemId remap_item(const syntax::ItemId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.items.size() ? map.items[id.value] : syntax::invalid_item_id;
}

void remap_type_node(syntax::TypeNode& node, const IdMap& map) {
    node.pointee = remap_type(node.pointee, map);
    node.array_element = remap_type(node.array_element, map);
}

void remap_expr_node(syntax::ExprNode& node, const IdMap& map) {
    node.unary_operand = remap_expr(node.unary_operand, map);
    node.binary_lhs = remap_expr(node.binary_lhs, map);
    node.binary_rhs = remap_expr(node.binary_rhs, map);
    node.callee = remap_expr(node.callee, map);
    for (syntax::ExprId& arg : node.args) {
        arg = remap_expr(arg, map);
    }
    node.object = remap_expr(node.object, map);
    node.index = remap_expr(node.index, map);
    for (syntax::FieldInit& init : node.field_inits) {
        init.value = remap_expr(init.value, map);
    }
    node.cast_type = remap_type(node.cast_type, map);
    node.cast_expr = remap_expr(node.cast_expr, map);
}

void remap_stmt_node(syntax::StmtNode& node, const IdMap& map) {
    node.declared_type = remap_type(node.declared_type, map);
    node.init = remap_expr(node.init, map);
    node.lhs = remap_expr(node.lhs, map);
    node.rhs = remap_expr(node.rhs, map);
    node.condition = remap_expr(node.condition, map);
    node.then_block = remap_stmt(node.then_block, map);
    node.else_block = remap_stmt(node.else_block, map);
    node.else_if = remap_stmt(node.else_if, map);
    node.body = remap_stmt(node.body, map);
    node.return_value = remap_expr(node.return_value, map);
    for (syntax::StmtId& stmt : node.statements) {
        stmt = remap_stmt(stmt, map);
    }
}

void remap_item_node(syntax::ItemNode& node, const IdMap& map) {
    node.const_type = remap_type(node.const_type, map);
    node.const_value = remap_expr(node.const_value, map);
    for (syntax::FieldDecl& field : node.fields) {
        field.type = remap_type(field.type, map);
    }
    node.enum_base_type = remap_type(node.enum_base_type, map);
    for (syntax::ParamDecl& param : node.params) {
        param.type = remap_type(param.type, map);
    }
    node.return_type = remap_type(node.return_type, map);
    node.body = remap_stmt(node.body, map);
    for (syntax::ItemId& item : node.extern_items) {
        item = remap_item(item, map);
    }
}

void append_module_into(
    syntax::AstModule& destination,
    syntax::AstModule&& source,
    const bool keep_imports,
    const syntax::ModuleId owner_module
) {
    IdMap map;
    map.types.reserve(source.types.size());
    map.exprs.reserve(source.exprs.size());
    map.stmts.reserve(source.stmts.size());
    map.items.reserve(source.items.size());

    for (const syntax::TypeNode& node : source.types) {
        map.types.push_back(syntax::TypeId {static_cast<base::u32>(destination.types.size())});
        destination.types.push_back(node);
    }
    for (const syntax::ExprNode& node : source.exprs) {
        map.exprs.push_back(syntax::ExprId {static_cast<base::u32>(destination.exprs.size())});
        destination.exprs.push_back(node);
    }
    for (const syntax::StmtNode& node : source.stmts) {
        map.stmts.push_back(syntax::StmtId {static_cast<base::u32>(destination.stmts.size())});
        destination.stmts.push_back(node);
    }
    for (const syntax::ItemNode& node : source.items) {
        map.items.push_back(syntax::ItemId {static_cast<base::u32>(destination.items.size())});
        destination.items.push_back(node);
        destination.item_modules.push_back(owner_module);
    }

    const base::usize type_begin = destination.types.size() - source.types.size();
    const base::usize expr_begin = destination.exprs.size() - source.exprs.size();
    const base::usize stmt_begin = destination.stmts.size() - source.stmts.size();
    const base::usize item_begin = destination.items.size() - source.items.size();

    for (base::usize i = type_begin; i < destination.types.size(); ++i) {
        remap_type_node(destination.types[i], map);
    }
    for (base::usize i = expr_begin; i < destination.exprs.size(); ++i) {
        remap_expr_node(destination.exprs[i], map);
    }
    for (base::usize i = stmt_begin; i < destination.stmts.size(); ++i) {
        remap_stmt_node(destination.stmts[i], map);
    }
    for (base::usize i = item_begin; i < destination.items.size(); ++i) {
        remap_item_node(destination.items[i], map);
    }

    if (keep_imports) {
        destination.imports.insert(destination.imports.end(), source.imports.begin(), source.imports.end());
    }
}

} // namespace

ModuleLoader::ModuleLoader(
    const CompilerInvocation& invocation,
    base::SourceManager& sources,
    base::DiagnosticSink& diagnostics
) noexcept
    : invocation_(invocation),
      sources_(sources),
      diagnostics_(diagnostics),
      import_paths_(standard_library_import_paths(invocation)) {}

base::Result<syntax::AstModule> ModuleLoader::load_root() {
    syntax::AstModule combined;
    auto result = load_file(canonical_or_absolute(invocation_.input_path), combined, 0, true, nullptr);
    if (!result) {
        return base::Result<syntax::AstModule>::fail(result.error());
    }
    return base::Result<syntax::AstModule>::ok(std::move(combined));
}

std::span<const ModuleRecord> ModuleLoader::modules() const noexcept {
    return modules_;
}

base::Result<syntax::ModuleId> ModuleLoader::load_file(
    const std::filesystem::path& path,
    syntax::AstModule& combined,
    const base::usize depth,
    const bool is_root,
    const syntax::ModulePath* expected_module
) {
    if (depth > base::config::max_include_depth) {
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::invalid_source, "maximum import depth exceeded"});
    }

    const std::filesystem::path canonical = canonical_or_absolute(path);
    const std::string key = canonical.string();
    if (loading_files_.contains(key)) {
        push_error(diagnostics_, expected_module != nullptr ? expected_module->range : base::SourceRange {}, "cyclic import involving: " + key);
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, "module loading failed"});
    }
    if (const auto loaded = loaded_file_modules_.find(key); loaded != loaded_file_modules_.end()) {
        return base::Result<syntax::ModuleId>::ok(loaded->second);
    }
    loading_files_.insert(key);

    auto source_result = read_file(canonical);
    if (!source_result) {
        loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(source_result.error());
    }

    const base::SourceId source_id = sources_.add_source(canonical.string(), std::move(source_result.value()));
    lex::Lexer lexer(source_id, sources_.text(source_id), diagnostics_);
    auto token_result = lexer.tokenize();
    if (!token_result) {
        loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(token_result.error());
    }
    parse::Parser parser(token_result.value(), diagnostics_);
    auto ast_result = parser.parse_module();
    if (!ast_result) {
        loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(ast_result.error());
    }

    syntax::AstModule module = std::move(ast_result.value());
    if (module.module_path.parts.empty()) {
        push_error(diagnostics_, base::SourceRange {source_id, 0, 0}, "module declaration is required for importable files");
        loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, "module loading failed"});
    }
    if (expected_module != nullptr && !syntax::module_paths_equal(module.module_path, *expected_module)) {
        push_error(
            diagnostics_,
            module.module_path.range,
            "module declaration '" + syntax::module_path_to_string(module.module_path) +
                "' does not match import '" + syntax::module_path_to_string(*expected_module) + "'"
        );
        loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, "module loading failed"});
    }
    const std::string module_name = syntax::module_path_to_string(module.module_path);
    const auto module_inserted = loaded_modules_.emplace(module_name, LoadedModule {canonical, syntax::invalid_module_id, module.module_path.range});
    if (!module_inserted.second && module_inserted.first->second.path != canonical) {
        push_error(
            diagnostics_,
            module.module_path.range,
            "duplicate module name '" + module_name + "' already loaded from " + module_inserted.first->second.path.string()
        );
        loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, "module loading failed"});
    }

    syntax::ModuleId module_id = module_inserted.first->second.id;
    if (module_inserted.second) {
        module_id = syntax::ModuleId {static_cast<base::u32>(combined.modules.size())};
        module_inserted.first->second.id = module_id;
        combined.modules.push_back(syntax::ModuleInfo {module.module_path, {}});
        modules_.push_back(ModuleRecord {module_name, canonical});
    }
    if (is_root) {
        combined.module_path = module.module_path;
    }

    const std::vector<syntax::ModulePath> imports = module.imports;
    std::vector<syntax::ModuleId> direct_imports;
    direct_imports.reserve(imports.size());
    for (const syntax::ModulePath& import : imports) {
        const auto import_file = find_import_file(import, canonical.parent_path(), import_paths_);
        if (!import_file) {
            const std::vector<std::filesystem::path> candidates = import_candidates(import, canonical.parent_path(), import_paths_);
            push_error(
                diagnostics_,
                import.range,
                "failed to resolve import: " + syntax::module_path_to_string(import) +
                    " (searched: " + format_import_candidates(candidates) + ")"
            );
            loading_files_.erase(key);
            return base::Result<syntax::ModuleId>::fail({base::ErrorCode::io_error, "module loading failed"});
        }
        auto import_result = load_file(canonical_or_absolute(*import_file), combined, depth + 1, false, &import);
        if (!import_result) {
            loading_files_.erase(key);
            return import_result;
        }
        direct_imports.push_back(import_result.value());
    }
    if (syntax::is_valid(module_id) && module_id.value < combined.modules.size()) {
        combined.modules[module_id.value].imports = std::move(direct_imports);
    }

    append_module_into(combined, std::move(module), is_root, module_id);
    loading_files_.erase(key);
    loaded_file_modules_.emplace(key, module_id);
    return base::Result<syntax::ModuleId>::ok(module_id);
}

} // namespace aurex::driver
