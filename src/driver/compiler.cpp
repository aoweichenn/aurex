#include "aurex/driver/compiler.hpp"

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/source.hpp"
#include "aurex/base/text.hpp"
#include "aurex/codegen_c/c_emitter.hpp"
#include "aurex/lex/lexer.hpp"
#include "aurex/parse/parser.hpp"
#include "aurex/sema/sema.hpp"
#include "aurex/syntax/ast_dump.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

[[nodiscard]] base::Result<void> write_file(const std::filesystem::path& path, const std::string_view text) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return base::Result<void>::fail({base::ErrorCode::io_error, "failed to open output file"});
    }
    output << text;
    if (!output) {
        return base::Result<void>::fail({base::ErrorCode::io_error, "failed to write output file"});
    }
    return base::Result<void>::ok();
}

void print_diagnostics(const base::SourceManager& sources, const base::DiagnosticSink& diagnostics) {
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        const base::SourceFile& file = sources.get(diagnostic.range.source);
        const base::LineColumn location = base::line_column(file.text(), diagnostic.range.begin);
        std::cerr << file.path() << ":" << location.line << ":" << location.column << ": "
                  << base::severity_name(diagnostic.severity) << ": "
                  << diagnostic.message << "\n";

        const std::string_view text = file.text();
        base::usize line_begin = diagnostic.range.begin;
        while (line_begin > 0 && text[line_begin - 1] != '\n') {
            --line_begin;
        }
        base::usize line_end = diagnostic.range.begin;
        while (line_end < text.size() && text[line_end] != '\n') {
            ++line_end;
        }
        const std::string_view source_line = text.substr(line_begin, line_end - line_begin);
        if (!source_line.empty()) {
            std::cerr << "  " << source_line << "\n";
            std::cerr << "  ";
            const base::usize caret_column = diagnostic.range.begin - line_begin;
            for (base::usize i = 0; i < caret_column; ++i) {
                std::cerr << (source_line[i] == '\t' ? '\t' : ' ');
            }
            const base::usize caret_count = diagnostic.range.empty() ? 1 : diagnostic.range.length();
            for (base::usize i = 0; i < caret_count && caret_column + i < source_line.size(); ++i) {
                std::cerr << '^';
            }
            std::cerr << "\n";
        }
    }
}

[[nodiscard]] std::filesystem::path canonical_or_absolute(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path);
}

[[nodiscard]] std::filesystem::path module_path_to_relative_file(const syntax::ModulePath& path) {
    std::filesystem::path result;
    for (std::string_view part : path.parts) {
        result /= std::string(part);
    }
    result += ".ax";
    return result;
}

[[nodiscard]] std::string module_path_to_string(const syntax::ModulePath& path) {
    std::ostringstream out;
    for (base::usize i = 0; i < path.parts.size(); ++i) {
        if (i != 0) {
            out << ".";
        }
        out << path.parts[i];
    }
    return out.str();
}

[[nodiscard]] bool module_paths_equal(const syntax::ModulePath& lhs, const syntax::ModulePath& rhs) noexcept {
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
    const std::filesystem::path relative = module_path_to_relative_file(path);
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
    const std::filesystem::path relative = module_path_to_relative_file(path);
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

class ModuleLoader final {
public:
    struct ModuleRecord {
        std::string name;
        std::filesystem::path path;
    };

    ModuleLoader(
        const CompilerInvocation& invocation,
        base::SourceManager& sources,
        base::DiagnosticSink& diagnostics
    ) noexcept
        : invocation_(invocation), sources_(sources), diagnostics_(diagnostics) {}

    [[nodiscard]] base::Result<syntax::AstModule> load_root() {
        syntax::AstModule combined;
        auto result = load_file(canonical_or_absolute(invocation_.input_path), combined, 0, true, nullptr);
        if (!result) {
            return base::Result<syntax::AstModule>::fail(result.error());
        }
        return base::Result<syntax::AstModule>::ok(std::move(combined));
    }

    [[nodiscard]] std::span<const ModuleRecord> modules() const noexcept {
        return modules_;
    }

private:
    struct LoadedModule {
        std::filesystem::path path;
        syntax::ModuleId id = syntax::invalid_module_id;
        base::SourceRange range {};
    };

    [[nodiscard]] base::Result<syntax::ModuleId> load_file(
        const std::filesystem::path& path,
        syntax::AstModule& combined,
        base::usize depth,
        bool is_root,
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
        if (expected_module != nullptr && !module_paths_equal(module.module_path, *expected_module)) {
            push_error(
                diagnostics_,
                module.module_path.range,
                "module declaration '" + module_path_to_string(module.module_path) +
                    "' does not match import '" + module_path_to_string(*expected_module) + "'"
            );
            loading_files_.erase(key);
            return base::Result<syntax::ModuleId>::fail({base::ErrorCode::parse_error, "module loading failed"});
        }
        const std::string module_name = module_path_to_string(module.module_path);
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
            const auto import_file = find_import_file(import, canonical.parent_path(), invocation_.import_paths);
            if (!import_file) {
                const std::vector<std::filesystem::path> candidates = import_candidates(import, canonical.parent_path(), invocation_.import_paths);
                push_error(
                    diagnostics_,
                    import.range,
                    "failed to resolve import: " + module_path_to_string(import) +
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

    const CompilerInvocation& invocation_;
    base::SourceManager& sources_;
    base::DiagnosticSink& diagnostics_;
    std::unordered_set<std::string> loading_files_;
    std::unordered_map<std::string, syntax::ModuleId> loaded_file_modules_;
    std::unordered_map<std::string, LoadedModule> loaded_modules_;
    std::vector<ModuleRecord> modules_;
};

} // namespace

base::Result<void> Compiler::run(const CompilerInvocation& invocation) {
    base::SourceManager sources;
    base::DiagnosticSink diagnostics;

    if (invocation.emit_kind == EmitKind::tokens) {
        auto source_result = read_file(invocation.input_path);
        if (!source_result) {
            return base::Result<void>::fail(source_result.error());
        }
        const base::SourceId source_id = sources.add_source(invocation.input_path.string(), std::move(source_result.value()));
        lex::Lexer lexer(source_id, sources.text(source_id), diagnostics);
        auto token_result = lexer.tokenize();
        if (!token_result) {
            print_diagnostics(sources, diagnostics);
            return base::Result<void>::fail(token_result.error());
        }
        std::cout << syntax::dump_tokens(token_result.value());
        return base::Result<void>::ok();
    }

    ModuleLoader loader(invocation, sources, diagnostics);
    auto ast_result = loader.load_root();

    if (!ast_result) {
        print_diagnostics(sources, diagnostics);
        return base::Result<void>::fail(ast_result.error());
    }

    if (invocation.emit_kind == EmitKind::ast) {
        std::cout << syntax::dump_ast(ast_result.value());
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::modules) {
        std::cout << "modules\n";
        for (const ModuleLoader::ModuleRecord& record : loader.modules()) {
            std::cout << "  " << record.name << " " << record.path.string() << "\n";
        }
        return base::Result<void>::ok();
    }

    sema::SemanticAnalyzer analyzer(ast_result.value(), diagnostics);
    auto checked_result = analyzer.analyze();
    if (!checked_result) {
        print_diagnostics(sources, diagnostics);
        return base::Result<void>::fail(checked_result.error());
    }

    if (invocation.emit_kind == EmitKind::check) {
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::checked) {
        std::cout << sema::dump_checked_module(checked_result.value());
        return base::Result<void>::ok();
    }

    codegen_c::CEmitter emitter(ast_result.value(), checked_result.value());
    auto c_result = emitter.emit();
    if (!c_result) {
        print_diagnostics(sources, diagnostics);
        return base::Result<void>::fail(c_result.error());
    }

    if (invocation.output_path.empty()) {
        std::cout << c_result.value().text;
        return base::Result<void>::ok();
    }

    auto write_result = write_file(invocation.output_path, c_result.value().text);
    if (!write_result) {
        return write_result;
    }
    return base::Result<void>::ok();
}

} // namespace aurex::driver
