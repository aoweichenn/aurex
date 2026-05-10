#pragma once

#include <aurex/ir/lower_ast.hpp>

#include <functional>
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
        return sema::INVALID_TYPE_HANDLE;
    }
    return checked.expr_types[expr.value];
}

struct CallTarget {
    FunctionId function = INVALID_FUNCTION_ID;
    std::string symbol;
};

struct PlaceAddress {
    ValueId address = INVALID_VALUE_ID;
    bool is_mutable = true;
};

struct LocalBinding {
    ValueId slot = INVALID_VALUE_ID;
    bool is_mutable = false;
};

struct LoopContext {
    BlockId break_target = INVALID_BLOCK_ID;
    BlockId continue_target = INVALID_BLOCK_ID;
    base::usize defer_depth = 0;
};

struct PendingConstant {
    GlobalConstantId id = INVALID_GLOBAL_CONSTANT_ID;
    syntax::ExprId initializer = syntax::INVALID_EXPR_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    std::string literal_text;
    bool is_literal = false;
};

struct EnumCaseTypeKey {
    base::u32 type = sema::TypeHandle::INVALID_VALUE;
    std::string_view case_name;

    [[nodiscard]] bool operator==(const EnumCaseTypeKey& other) const noexcept {
        return type == other.type && case_name == other.case_name;
    }
};

struct EnumCaseTypeKeyHash {
    [[nodiscard]] std::size_t operator()(const EnumCaseTypeKey& key) const noexcept {
        const std::size_t type_hash = std::hash<base::u32> {}(key.type);
        const std::size_t name_hash = std::hash<std::string_view> {}(key.case_name);
        return type_hash ^ (name_hash + 0x9e3779b9U + (type_hash << 6U) + (type_hash >> 2U));
    }
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
    void index_enum_cases();

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
    void lower_for(const syntax::StmtNode& stmt);
    void lower_for_range(syntax::StmtId stmt_id, const syntax::StmtNode& stmt);
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
    [[nodiscard]] ValueId append_integer_literal(std::string_view text, sema::TypeHandle value_type);
    [[nodiscard]] ValueId append_binary_value(BinaryOp op, sema::TypeHandle type, ValueId lhs, ValueId rhs);
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
    [[nodiscard]] ValueId lower_literal_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, sema::TypeHandle expected_type);
    [[nodiscard]] ValueId lower_name(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_unary_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_binary_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_call_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_load_expr(syntax::ExprId expr_id);
    [[nodiscard]] ValueId lower_struct_literal_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_cast_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_size_or_align_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_str_projection_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] ValueId lower_str_from_bytes_unchecked_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);

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
    BlockId current_block_ = INVALID_BLOCK_ID;
    bool lowering_constant_initializer_ = false;
    std::unordered_map<std::string, LocalBinding> locals_;
    std::unordered_map<std::string, FunctionId> function_symbols_;
    std::unordered_map<std::string, GlobalConstantId> constant_symbols_;
    std::unordered_map<std::string_view, const sema::EnumCaseInfo*> enum_cases_by_name_;
    std::unordered_map<std::string_view, const sema::EnumCaseInfo*> enum_cases_by_c_name_;
    std::unordered_map<EnumCaseTypeKey, const sema::EnumCaseInfo*, EnumCaseTypeKeyHash> enum_cases_by_type_and_case_;
    std::vector<PendingConstant> pending_constants_;
    std::vector<FunctionId> item_functions_;
    std::vector<FunctionId> generic_function_instance_functions_;
    std::vector<LoopContext> loop_contexts_;
    std::vector<std::vector<syntax::ExprId>> defer_scopes_;
};

} // namespace aurex::ir::detail
