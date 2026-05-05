#pragma once

#include "aurex/ir/lower_ast.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aurex::ir::detail {

[[nodiscard]] inline sema::TypeHandle checked_expr_type(
    const sema::CheckedModule& checked,
    const syntax::ExprId expr
) noexcept {
    if (!syntax::is_valid(expr) || expr.value >= checked.expr_types.size()) {
        return sema::invalid_type_handle;
    }
    return checked.expr_types[expr.value];
}

struct CallTarget {
    FunctionId function = invalid_function_id;
    std::string symbol;
};

struct PlaceAddress {
    ValueId address = invalid_value_id;
    bool is_mutable = true;
};

struct LocalBinding {
    ValueId slot = invalid_value_id;
    bool is_mutable = false;
};

struct LoopContext {
    BlockId break_target = invalid_block_id;
    BlockId continue_target = invalid_block_id;
    base::usize defer_depth = 0;
};

struct PendingConstant {
    GlobalConstantId id = invalid_global_constant_id;
    syntax::ExprId initializer = syntax::invalid_expr_id;
    sema::TypeHandle type = sema::invalid_type_handle;
    std::string literal_text;
    bool is_literal = false;
};

class Lowerer final {
public:
    Lowerer(const syntax::AstModule& ast, const sema::CheckedModule& checked);

    [[nodiscard]] Module lower();

private:
    void lower_record_layouts();
    void declare_global_constants();
    void lower_function_declarations();
    void lower_generic_function_declarations();
    void lower_global_constant_initializers();

    [[nodiscard]] std::string item_symbol(base::u32 index, const syntax::ItemNode& item) const;
    [[nodiscard]] std::string enum_case_symbol(
        base::u32 index,
        const syntax::ItemNode& item,
        const syntax::EnumCaseDecl& enum_case
    ) const;
    [[nodiscard]] sema::TypeHandle enum_case_type(const std::string& symbol) const noexcept;
    [[nodiscard]] GlobalConstantId enum_case_constant(std::string_view name) const noexcept;
    [[nodiscard]] std::string enum_case_symbol(std::string_view name) const noexcept;
    [[nodiscard]] const sema::EnumCaseInfo* enum_case_info(std::string_view name) const noexcept;

    [[nodiscard]] const syntax::PatternNode* pattern_node(syntax::PatternId id) const noexcept;
    [[nodiscard]] std::string pattern_case_symbol(syntax::PatternId id) const;
    [[nodiscard]] bool is_fallback_match_pattern(syntax::PatternId id) const noexcept;
    [[nodiscard]] ValueId append_match_pattern_condition(
        syntax::PatternId id,
        ValueId matched_tag,
        sema::TypeHandle matched_type,
        bool payload_enum
    );

    void lower_function_body(FunctionId function_id, const syntax::ItemNode& item);
    void lower_block(syntax::StmtId block_id);
    void lower_block_contents(syntax::StmtId block_id);
    void lower_stmt(syntax::StmtId stmt_id);
    void lower_if(const syntax::StmtNode& stmt);
    void lower_while(const syntax::StmtNode& stmt);

    [[nodiscard]] ValueId lower_short_circuit_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_if_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_block_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_match_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_try_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);

    [[nodiscard]] ValueId append_enum_case_ref(std::string_view case_name, sema::TypeHandle enum_type);
    [[nodiscard]] ValueId append_enum_tag_literal(std::string_view case_name, sema::TypeHandle tag_type);
    [[nodiscard]] ValueId lower_enum_constructor(const sema::EnumCaseInfo& enum_case, syntax::ExprId payload_expr);
    [[nodiscard]] ValueId append_enum_constructor(const sema::EnumCaseInfo& enum_case, ValueId payload_value);
    [[nodiscard]] ValueId append_enum_payload_load(ValueId enum_slot, sema::TypeHandle payload_type, const std::string& name);
    [[nodiscard]] const sema::EnumCaseInfo* enum_case_by_type_and_case(
        sema::TypeHandle enum_type,
        std::string_view case_name
    ) const noexcept;

    [[nodiscard]] ValueId append_temp_alloca(const std::string& name, sema::TypeHandle value_type);
    [[nodiscard]] ValueId append_load(
        ValueId address,
        sema::TypeHandle value_type,
        const std::string& name = {}
    );
    [[nodiscard]] ValueId enum_field_addr(ValueId object, const std::string& field_name);
    void bind_payload_arm(const syntax::PatternNode& pattern, const sema::EnumCaseInfo& info, ValueId matched_slot);
    void bind_match_payload(
        const syntax::PatternNode* pattern,
        syntax::PatternId pattern_id,
        bool payload_enum,
        ValueId matched_slot
    );

    [[nodiscard]] ValueId lower_expr(syntax::ExprId expr_id);
    [[nodiscard]] ValueId lower_expr(syntax::ExprId expr_id, sema::TypeHandle expected_type);
    [[nodiscard]] ValueId lower_name(syntax::ExprId expr_id, const syntax::ExprNode& expr);

    void emit_deferred_scopes(base::usize keep_depth);
    [[nodiscard]] ValueId lower_place_addr(syntax::ExprId expr_id);
    [[nodiscard]] PlaceAddress lower_place_address(syntax::ExprId expr_id);
    [[nodiscard]] PlaceAddress lower_object_place_or_value(syntax::ExprId expr_id);

    [[nodiscard]] bool is_local_slot_type(sema::TypeHandle type) const noexcept;
    [[nodiscard]] bool pointee_is_mutable(syntax::ExprId expr_id) const noexcept;

    [[nodiscard]] CallTarget call_target(syntax::ExprId callee) const;
    [[nodiscard]] std::string call_symbol(syntax::ExprId callee) const;
    [[nodiscard]] std::string value_symbol(syntax::ExprId expr_id, const syntax::ExprNode& expr) const;
    [[nodiscard]] sema::TypeHandle call_param_type(FunctionId function_id, base::usize index) const noexcept;
    [[nodiscard]] sema::TypeHandle variadic_argument_type(sema::TypeHandle source_type) const noexcept;
    [[nodiscard]] sema::TypeHandle expr_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] sema::TypeHandle syntax_type(syntax::TypeId type) const noexcept;
    [[nodiscard]] sema::TypeHandle function_return_type(base::u32 index, const syntax::ItemNode& item) const noexcept;
    [[nodiscard]] sema::TypeHandle stmt_local_type(syntax::StmtId stmt) const noexcept;
    [[nodiscard]] sema::TypeHandle aggregate_field_type(
        sema::TypeHandle aggregate_type,
        std::string_view name
    ) const noexcept;
    [[nodiscard]] sema::TypeHandle local_load_type(ValueId slot) const noexcept;

    [[nodiscard]] ValueId coerce_value(ValueId value_id, sema::TypeHandle target_type);
    [[nodiscard]] ValueId append_value(const Value& value);
    void append_store(ValueId target, ValueId source);
    void append_branch_if_open(BlockId target);
    [[nodiscard]] bool has_terminator(BlockId block) const;
    void set_terminator(BlockId block, const Terminator& terminator);

    const syntax::AstModule& ast_;
    const sema::CheckedModule& checked_;
    Module module_;
    Function* current_function_ = nullptr;
    const sema::GenericFunctionInstanceInfo* current_generic_function_instance_ = nullptr;
    BlockId current_block_ = invalid_block_id;
    std::unordered_map<std::string, LocalBinding> locals_;
    std::unordered_map<std::string, FunctionId> function_symbols_;
    std::unordered_map<std::string, GlobalConstantId> constant_symbols_;
    std::vector<PendingConstant> pending_constants_;
    std::vector<FunctionId> item_functions_;
    std::vector<FunctionId> generic_function_instance_functions_;
    std::vector<LoopContext> loop_contexts_;
    std::vector<std::vector<syntax::ExprId>> defer_scopes_;
};

} // namespace aurex::ir::detail
