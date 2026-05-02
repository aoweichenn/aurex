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
    TypeHandle return_type = invalid_type_handle;
    std::vector<TypeHandle> param_types;
    base::SourceRange range {};
    bool is_extern_c = false;
    bool is_export_c = false;
};

struct StructFieldInfo {
    std::string name;
    TypeHandle type = invalid_type_handle;
    base::SourceRange range {};
};

struct StructInfo {
    std::string name;
    TypeHandle type = invalid_type_handle;
    std::vector<StructFieldInfo> fields;
    bool is_opaque = false;
};

struct EnumCaseInfo {
    std::string name;
    TypeHandle type = invalid_type_handle;
    std::string value_text;
    base::SourceRange range {};
};

struct CheckedModule {
    // CheckedModule is the bridge between syntax and codegen. It is deliberately
    // side-table based so AST nodes remain parse-only data.
    TypeTable types;
    std::vector<TypeHandle> expr_types;
    std::unordered_map<std::string, FunctionSignature> functions;
    std::unordered_map<std::string, StructInfo> structs;
    std::unordered_map<std::string, EnumCaseInfo> enum_cases;
};

class SemanticAnalyzer final {
public:
    SemanticAnalyzer(const syntax::AstModule& module, base::DiagnosticSink& diagnostics) noexcept;

    [[nodiscard]] base::Result<CheckedModule> analyze();

private:
    void register_type_names();
    void register_value_names();
    void analyze_struct_properties();
    void analyze_const_decls();
    void analyze_function_body(const syntax::ItemNode& function);
    void analyze_block(syntax::StmtId block, TypeHandle expected_return);
    void analyze_stmt(syntax::StmtId stmt, TypeHandle expected_return);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr);
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type);
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type, bool opaque_allowed_as_pointee);
    [[nodiscard]] bool can_assign(TypeHandle dst, TypeHandle src, syntax::ExprId value) const noexcept;
    [[nodiscard]] bool is_integer_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_null_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_writable_place(syntax::ExprId expr);
    [[nodiscard]] bool is_copy_forbidden_value(TypeHandle type) const noexcept;
    [[nodiscard]] const StructInfo* find_struct(TypeHandle type) const noexcept;
    [[nodiscard]] TypeHandle record_expr_type(syntax::ExprId expr, TypeHandle type) noexcept;
    void report(base::SourceRange range, std::string message);

    const syntax::AstModule& module_;
    base::DiagnosticSink& diagnostics_;
    CheckedModule checked_;
    SymbolTable symbols_;
    std::unordered_map<std::string, TypeHandle> named_types_;
    int loop_depth_ = 0;
};

[[nodiscard]] std::string dump_checked_module(const CheckedModule& checked);

} // namespace aurex::sema
