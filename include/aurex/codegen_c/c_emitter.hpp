#pragma once

#include "aurex/base/result.hpp"
#include "aurex/sema/sema.hpp"
#include "aurex/syntax/ast.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace aurex::codegen_c {

struct COutput {
    std::string text;
};

class CEmitter final {
public:
    // Emits C from AST plus semantic side tables. The emitter should not resolve
    // names or guess types; that work belongs in sema/lowering.
    CEmitter(const syntax::AstModule& module, const sema::CheckedModule& checked) noexcept;

    [[nodiscard]] base::Result<COutput> emit();

private:
    void emit_prelude();
    void emit_forward_decls();
    void emit_item(const syntax::ItemNode& item);
    void emit_const(const syntax::ItemNode& item);
    void emit_struct(const syntax::ItemNode& item);
    void emit_enum(const syntax::ItemNode& item);
    void emit_function_decl(const syntax::ItemNode& item, bool with_semicolon);
    void emit_function_decl_named(const syntax::ItemNode& item, std::string_view name, bool with_semicolon);
    void emit_main_wrapper(const syntax::ItemNode& item);
    void emit_block(syntax::StmtId block);
    void emit_stmt(syntax::StmtId stmt);
    bool emit_call_statement_with_ordering(syntax::ExprId expr);
    [[nodiscard]] std::string emit_expr_ordered(syntax::ExprId expr);
    [[nodiscard]] std::string lower_expr_to_temp(syntax::ExprId expr);
    [[nodiscard]] std::string lower_logical_expr_to_temp(syntax::ExprId expr);
    [[nodiscard]] std::string lower_child_if_needed(syntax::ExprId expr);
    [[nodiscard]] std::string emit_expr(syntax::ExprId expr);
    [[nodiscard]] std::string emit_condition(syntax::ExprId expr);
    [[nodiscard]] std::string emit_type(syntax::TypeId type, std::string declarator = {});
    [[nodiscard]] std::string emit_callee(syntax::ExprId callee) const;
    [[nodiscard]] std::string emit_name(syntax::ExprId expr) const;
    [[nodiscard]] sema::TypeHandle type_handle(syntax::TypeId type) const noexcept;
    [[nodiscard]] std::string c_name(const syntax::ItemNode& item) const;
    [[nodiscard]] std::string c_type_name(const syntax::ItemNode& item) const;
    [[nodiscard]] std::string escape_c_string(std::string_view literal) const;
    [[nodiscard]] bool is_exported_main(const syntax::ItemNode& item) const noexcept;
    [[nodiscard]] bool expr_has_pointer_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] std::string declare_temp(sema::TypeHandle type, std::string value);
    void write_indent();

    const syntax::AstModule& module_;
    const sema::CheckedModule& checked_;
    std::ostringstream out_;
    int indent_ = 0;
    int temp_index_ = 0;
};

} // namespace aurex::codegen_c
