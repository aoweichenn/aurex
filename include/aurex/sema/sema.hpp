#pragma once

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/result.hpp"
#include "aurex/sema/symbol.hpp"
#include "aurex/sema/type.hpp"
#include "aurex/syntax/ast.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace aurex::sema {

struct FunctionSignature {
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::invalid_module_id;
    TypeHandle return_type = invalid_type_handle;
    std::vector<TypeHandle> param_types;
    base::SourceRange range {};
    bool is_extern_c = false;
    bool is_export_c = false;
};

struct StructFieldInfo {
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::invalid_module_id;
    TypeHandle type = invalid_type_handle;
    base::SourceRange range {};
};

struct StructInfo {
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::invalid_module_id;
    TypeHandle type = invalid_type_handle;
    std::vector<StructFieldInfo> fields;
    bool is_opaque = false;
};

struct EnumCaseInfo {
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::invalid_module_id;
    TypeHandle type = invalid_type_handle;
    std::string value_text;
    base::SourceRange range {};
};

struct TypeAliasInfo {
    std::string name;
    syntax::ModuleId module = syntax::invalid_module_id;
    syntax::TypeId target = syntax::invalid_type_id;
    base::SourceRange range {};
};

struct CheckedModule {
    // CheckedModule is the bridge between syntax and codegen. It is deliberately
    // side-table based so AST nodes remain parse-only data.
    TypeTable types;
    std::vector<TypeHandle> expr_types;
    std::vector<std::string> expr_c_names;
    std::vector<TypeHandle> syntax_type_handles;
    std::vector<std::string> item_c_names;
    std::unordered_map<std::string, FunctionSignature> functions;
    std::unordered_map<std::string, StructInfo> structs;
    std::unordered_map<std::string, EnumCaseInfo> enum_cases;
    std::unordered_map<std::string, TypeAliasInfo> type_aliases;
};

class SemanticAnalyzer final {
public:
    SemanticAnalyzer(const syntax::AstModule& module, base::DiagnosticSink& diagnostics) noexcept;

    [[nodiscard]] base::Result<CheckedModule> analyze();

private:
    void register_type_names();
    void register_value_names();
    void analyze_entry_points();
    void resolve_type_alias_decls();
    void analyze_struct_properties();
    void analyze_const_decls();
    void analyze_function_body(const syntax::ItemNode& function);
    void analyze_block(syntax::StmtId block, TypeHandle expected_return);
    void analyze_stmt(syntax::StmtId stmt, TypeHandle expected_return);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr);
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type);
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type, bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle resolve_type_alias(const TypeAliasInfo& alias, bool opaque_allowed_as_pointee);
    [[nodiscard]] bool can_assign(TypeHandle dst, TypeHandle src, syntax::ExprId value) const noexcept;
    [[nodiscard]] bool is_valid_storage_type(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_valid_cast(syntax::ExprKind kind, TypeHandle dst, TypeHandle src) const noexcept;
    [[nodiscard]] base::u64 abi_size(TypeHandle type) const noexcept;
    [[nodiscard]] base::u64 abi_align(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_integer_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_null_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_place_expr(syntax::ExprId expr);
    [[nodiscard]] bool is_writable_place(syntax::ExprId expr);
    [[nodiscard]] bool is_copy_forbidden_value(TypeHandle type) const noexcept;
    [[nodiscard]] const StructInfo* find_struct(TypeHandle type) const noexcept;
    [[nodiscard]] syntax::ModuleId item_module(const syntax::ItemNode& item) const noexcept;
    [[nodiscard]] std::vector<syntax::ModuleId> visible_modules(syntax::ModuleId module) const;
    [[nodiscard]] std::string module_name(syntax::ModuleId module) const;
    [[nodiscard]] std::string qualified_name(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string c_symbol_name(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string module_key(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] TypeHandle find_type_in_visible_modules(std::string_view name, base::SourceRange range, bool opaque_allowed_as_pointee);
    [[nodiscard]] const FunctionSignature* find_function_in_visible_modules(std::string_view name, base::SourceRange range);
    [[nodiscard]] const Symbol* find_symbol(std::string_view name, base::SourceRange range);
    [[nodiscard]] TypeHandle record_expr_type(syntax::ExprId expr, TypeHandle type) noexcept;
    void report(base::SourceRange range, std::string message);

    const syntax::AstModule& module_;
    base::DiagnosticSink& diagnostics_;
    CheckedModule checked_;
    SymbolTable symbols_;
    std::unordered_map<std::string, TypeHandle> named_types_;
    std::unordered_map<std::string, TypeHandle> resolved_type_aliases_;
    std::vector<std::string> resolving_type_aliases_;
    std::unordered_map<std::string, Symbol> global_values_;
    syntax::ModuleId current_module_ = syntax::invalid_module_id;
    int loop_depth_ = 0;
};

[[nodiscard]] std::string dump_checked_module(const CheckedModule& checked);

} // namespace aurex::sema
