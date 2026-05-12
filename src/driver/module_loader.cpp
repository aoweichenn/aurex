#include <aurex/driver/module_loader.hpp>

#include <aurex/driver/driver_messages.hpp>

#include <aurex/base/config.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/syntax/module.hpp>

#include <filesystem>
#include <optional>
#include <sstream>
#include <utility>

namespace aurex::driver {

namespace {

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
    const auto exists = [](const std::filesystem::path& candidate) {
        std::error_code error;
        return std::filesystem::exists(candidate, error) && !error;
    };

    const std::filesystem::path importer_candidate = importer_dir / relative;
    if (exists(importer_candidate)) {
        return importer_candidate;
    }
    for (const std::filesystem::path& import_path : import_paths) {
        const std::filesystem::path candidate = import_path / relative;
        if (exists(candidate)) {
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
    candidates.reserve(import_paths.size() + 1);
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
    std::vector<syntax::PatternId> patterns;
    std::vector<syntax::StmtId> stmts;
    std::vector<syntax::ItemId> items;
};

[[nodiscard]] syntax::TypeId remap_type(const syntax::TypeId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.types.size() ? map.types[id.value] : syntax::INVALID_TYPE_ID;
}

[[nodiscard]] syntax::ExprId remap_expr(const syntax::ExprId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.exprs.size() ? map.exprs[id.value] : syntax::INVALID_EXPR_ID;
}

[[nodiscard]] syntax::StmtId remap_stmt(const syntax::StmtId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.stmts.size() ? map.stmts[id.value] : syntax::INVALID_STMT_ID;
}

[[nodiscard]] syntax::PatternId remap_pattern(const syntax::PatternId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.patterns.size() ? map.patterns[id.value] : syntax::INVALID_PATTERN_ID;
}

[[nodiscard]] syntax::ItemId remap_item(const syntax::ItemId id, const IdMap& map) {
    return syntax::is_valid(id) && id.value < map.items.size() ? map.items[id.value] : syntax::INVALID_ITEM_ID;
}

void remap_type_node(syntax::TypeNode& node, const IdMap& map) {
    for (syntax::TypeId& arg : node.type_args) {
        arg = remap_type(arg, map);
    }
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
    node.condition = remap_expr(node.condition, map);
    node.then_expr = remap_expr(node.then_expr, map);
    node.else_expr = remap_expr(node.else_expr, map);
    node.block = remap_stmt(node.block, map);
    node.block_result = remap_expr(node.block_result, map);
    node.match_value = remap_expr(node.match_value, map);
    for (syntax::MatchArm& arm : node.match_arms) {
        arm.pattern = remap_pattern(arm.pattern, map);
        arm.guard = remap_expr(arm.guard, map);
        arm.value = remap_expr(arm.value, map);
    }
    for (syntax::ExprId& element : node.array_elements) {
        element = remap_expr(element, map);
    }
    node.array_repeat_value = remap_expr(node.array_repeat_value, map);
    node.array_repeat_count = remap_expr(node.array_repeat_count, map);
    node.object = remap_expr(node.object, map);
    node.index = remap_expr(node.index, map);
    for (syntax::FieldInit& init : node.field_inits) {
        init.value = remap_expr(init.value, map);
    }
    for (syntax::TypeId& arg : node.type_args) {
        arg = remap_type(arg, map);
    }
    node.cast_type = remap_type(node.cast_type, map);
    node.cast_expr = remap_expr(node.cast_expr, map);
}

void remap_pattern_node(syntax::PatternNode& node, const IdMap& map) {
    for (syntax::PatternId& alternative : node.alternatives) {
        alternative = remap_pattern(alternative, map);
    }
}

void remap_stmt_node(syntax::StmtNode& node, const IdMap& map) {
    node.declared_type = remap_type(node.declared_type, map);
    node.init = remap_expr(node.init, map);
    node.lhs = remap_expr(node.lhs, map);
    node.rhs = remap_expr(node.rhs, map);
    node.condition = remap_expr(node.condition, map);
    node.range_start = remap_expr(node.range_start, map);
    node.range_end = remap_expr(node.range_end, map);
    node.range_step = remap_expr(node.range_step, map);
    node.then_block = remap_stmt(node.then_block, map);
    node.else_block = remap_stmt(node.else_block, map);
    node.else_if = remap_stmt(node.else_if, map);
    node.body = remap_stmt(node.body, map);
    node.for_init = remap_stmt(node.for_init, map);
    node.for_update = remap_stmt(node.for_update, map);
    node.return_value = remap_expr(node.return_value, map);
    for (syntax::StmtId& stmt : node.statements) {
        stmt = remap_stmt(stmt, map);
    }
}

void remap_item_node(syntax::ItemNode& node, const IdMap& map) {
    node.const_type = remap_type(node.const_type, map);
    node.const_value = remap_expr(node.const_value, map);
    node.alias_type = remap_type(node.alias_type, map);
    for (syntax::FieldDecl& field : node.fields) {
        field.type = remap_type(field.type, map);
    }
    node.enum_base_type = remap_type(node.enum_base_type, map);
    for (syntax::EnumCaseDecl& enum_case : node.enum_cases) {
        enum_case.payload_type = remap_type(enum_case.payload_type, map);
        for (syntax::TypeId& payload_type : enum_case.payload_types) {
            payload_type = remap_type(payload_type, map);
        }
    }
    for (syntax::ParamDecl& param : node.params) {
        param.type = remap_type(param.type, map);
    }
    node.return_type = remap_type(node.return_type, map);
    node.body = remap_stmt(node.body, map);
    node.impl_type = remap_type(node.impl_type, map);
    for (syntax::ItemId& item : node.extern_items) {
        item = remap_item(item, map);
    }
    for (syntax::ItemId& item : node.impl_items) {
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
    map.patterns.reserve(source.patterns.size());
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
    for (const syntax::PatternNode& node : source.patterns) {
        map.patterns.push_back(syntax::PatternId {static_cast<base::u32>(destination.patterns.size())});
        destination.patterns.push_back(node);
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
    const base::usize pattern_begin = destination.patterns.size() - source.patterns.size();
    for (base::usize i = pattern_begin; i < destination.patterns.size(); ++i) {
        remap_pattern_node(destination.patterns[i], map);
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
      import_paths_(invocation.import_paths) {}

base::Result<syntax::AstModule> ModuleLoader::load_root() {
    syntax::AstModule combined;
    auto result = this->load_file(canonical_or_absolute(this->invocation_.input_path), combined, 0, true, nullptr);
    if (!result) {
        return base::Result<syntax::AstModule>::fail(result.error());
    }
    return base::Result<syntax::AstModule>::ok(std::move(combined));
}

std::span<const ModuleRecord> ModuleLoader::modules() const noexcept {
    return this->modules_;
}

base::Result<syntax::ModuleId> ModuleLoader::load_file(
    const std::filesystem::path& path,
    syntax::AstModule& combined,
    const base::usize depth,
    const bool is_root,
    const syntax::ModulePath* expected_module
) {
    if (depth > base::config::AUREX_MAX_INCLUDE_DEPTH) {
        return base::Result<syntax::ModuleId>::fail({
            base::ErrorCode::invalid_source,
            std::string(DRIVER_IMPORT_DEPTH_EXCEEDED)
        });
    }

    const std::filesystem::path canonical = canonical_or_absolute(path);
    const std::string key = canonical.string();
    if (this->loading_files_.contains(key)) {
        push_error(
            this->diagnostics_,
            expected_module != nullptr ? expected_module->range : base::SourceRange {},
            driver_cyclic_import_message(key)
        );
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
    }
    if (const auto loaded = this->loaded_file_modules_.find(key); loaded != this->loaded_file_modules_.end()) {
        const syntax::ModuleId module_id = loaded->second;
        if (expected_module != nullptr &&
            syntax::is_valid(module_id) &&
            module_id.value < combined.modules.size() &&
            !syntax::module_paths_equal(combined.modules[module_id.value].path, *expected_module)) {
            push_error(
                this->diagnostics_,
                expected_module->range,
                driver_module_import_mismatch_message(
                    syntax::module_path_to_string(combined.modules[module_id.value].path),
                    syntax::module_path_to_string(*expected_module)
                )
            );
            return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
        }
        return base::Result<syntax::ModuleId>::ok(loaded->second);
    }
    this->loading_files_.insert(key);

    auto source_result = read_text_file(canonical);
    if (!source_result) {
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(source_result.error());
    }

    const base::SourceId source_id = this->sources_.add_source(canonical.string(), std::move(source_result.value()));
    lex::Lexer lexer(source_id, this->sources_.text(source_id), this->diagnostics_);
    auto token_result = lexer.tokenize();
    if (!token_result) {
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(token_result.error());
    }
    parse::Parser parser(token_result.value(), this->diagnostics_);
    auto ast_result = parser.parse_module();
    if (!ast_result) {
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail(ast_result.error());
    }

    syntax::AstModule module = std::move(ast_result.value());
    if (module.module_path.parts.empty()) {
        push_error(
            this->diagnostics_,
            base::SourceRange {source_id, 0, 0},
            std::string(DRIVER_IMPORTABLE_MODULE_DECL_REQUIRED)
        );
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
    }
    if (expected_module != nullptr && !syntax::module_paths_equal(module.module_path, *expected_module)) {
        push_error(
            this->diagnostics_,
            module.module_path.range,
            driver_module_import_mismatch_message(
                syntax::module_path_to_string(module.module_path),
                syntax::module_path_to_string(*expected_module)
            )
        );
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
    }
    const std::string module_name = syntax::module_path_to_string(module.module_path);
    const auto module_inserted = this->loaded_modules_.emplace(module_name, LoadedModule {canonical, syntax::INVALID_MODULE_ID, module.module_path.range});
    if (!module_inserted.second && module_inserted.first->second.path != canonical) {
        push_error(
            this->diagnostics_,
            module.module_path.range,
            driver_duplicate_module_name_message(module_name, module_inserted.first->second.path.string())
        );
        this->loading_files_.erase(key);
        return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
    }

    syntax::ModuleId module_id = module_inserted.first->second.id;
    if (module_inserted.second) {
        module_id = syntax::ModuleId {static_cast<base::u32>(combined.modules.size())};
        module_inserted.first->second.id = module_id;
        combined.modules.push_back(syntax::ModuleInfo {module.module_path, {}});
        this->modules_.push_back(ModuleRecord {module_name, canonical});
    }
    if (is_root) {
        combined.module_path = module.module_path;
    }

    const std::vector<syntax::ImportDecl> imports = module.imports;
    std::vector<syntax::ResolvedImport> direct_imports;
    direct_imports.reserve(imports.size());
    for (const syntax::ImportDecl& import : imports) {
        const auto import_file = find_import_file(import.path, canonical.parent_path(), this->import_paths_);
        if (!import_file) {
            const std::vector<std::filesystem::path> candidates = import_candidates(import.path, canonical.parent_path(), this->import_paths_);
            push_error(
                this->diagnostics_,
                import.path.range,
                driver_import_resolve_failed_message(
                    syntax::module_path_to_string(import.path),
                    format_import_candidates(candidates)
                )
            );
            this->loading_files_.erase(key);
            return base::Result<syntax::ModuleId>::fail({base::ErrorCode::io_error, std::string(DRIVER_MODULE_LOADING_FAILED)});
        }
        auto import_result = this->load_file(canonical_or_absolute(*import_file), combined, depth + 1, false, &import.path);
        if (!import_result) {
            this->loading_files_.erase(key);
            return import_result;
        }
        direct_imports.push_back(syntax::ResolvedImport {
            import_result.value(),
            import.alias,
            import.alias_range,
            import.visibility,
        });
    }
    if (syntax::is_valid(module_id) && module_id.value < combined.modules.size()) {
        combined.modules[module_id.value].imports = std::move(direct_imports);
    }

    append_module_into(combined, std::move(module), is_root, module_id);
    this->loading_files_.erase(key);
    this->loaded_file_modules_.emplace(key, module_id);
    return base::Result<syntax::ModuleId>::ok(module_id);
}

} // namespace aurex::driver
