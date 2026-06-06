#pragma once

#include <aurex/frontend/sema/resource_semantics.hpp>
#include <aurex/midend/ir/lower_ast.hpp>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aurex::ir::detail {

[[nodiscard]] inline sema::TypeHandle checked_expr_type(
    const sema::CheckedModule& checked, const syntax::ExprId expr) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= checked.expr_types.size()) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return checked.expr_types[expr.value];
}

struct ActiveSideTables {
    const sema::GenericSideTables* generic = nullptr;
    const sema::SemaTypeTable* expr_types = nullptr;
    const sema::SemaOwnedUseModeTable* expr_owned_use_modes = nullptr;
    const sema::SemaIdentTable* expr_c_name_ids = nullptr;
    const sema::SemaIdentTable* pattern_c_name_ids = nullptr;
    const sema::SemaTypeTable* syntax_type_handles = nullptr;
    const sema::SemaTypeTable* stmt_local_types = nullptr;
};

struct CallTarget {
    FunctionId function = INVALID_FUNCTION_ID;
    IrTextId symbol = INVALID_IR_TEXT_ID;
};

struct PlaceAddress {
    ValueId address = INVALID_VALUE_ID;
    bool is_mutable = true;
};

enum class LocalPlaceProjectionKind : base::u8 {
    field,
    tuple_element,
};

struct LocalPlaceProjection {
    LocalPlaceProjectionKind kind = LocalPlaceProjectionKind::field;
    sema::IdentId field_name_id = sema::INVALID_IDENT_ID;
    base::u32 element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX;
    std::string_view field_name;
};

struct LocalPlacePath {
    sema::IdentId root_name_id = sema::INVALID_IDENT_ID;
    std::vector<LocalPlaceProjection> projections;
};

struct CleanupProjection {
    LocalPlaceProjectionKind kind = LocalPlaceProjectionKind::field;
    sema::IdentId field_name_id = sema::INVALID_IDENT_ID;
    base::u32 element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX;
    IrTextId field_name = INVALID_IR_TEXT_ID;
};

struct StructuredCleanupFrame {
    ValueId address = INVALID_VALUE_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    std::string flag_name;
    IrTextId name = INVALID_IR_TEXT_ID;
    std::vector<CleanupProjection> projections;
};

enum class CleanupDropMode {
    full,
    custom_destructor_only,
};

struct CleanupBinding {
    ValueId address = INVALID_VALUE_ID;
    ValueId flag = INVALID_VALUE_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    IrTextId name = INVALID_IR_TEXT_ID;
    std::vector<CleanupProjection> projections;
    CleanupDropMode drop_mode = CleanupDropMode::full;
};

struct LocalBinding {
    ValueId slot = INVALID_VALUE_ID;
    ValueId cleanup_flag = INVALID_VALUE_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    bool is_mutable = false;
    std::vector<CleanupBinding> field_cleanups;
};

struct LocalScopeFrame {
    std::unordered_map<sema::IdentId, std::optional<LocalBinding>, sema::IdentIdHash> previous_bindings;
};

struct PatternBindingSlot {
    IrTextId name = INVALID_IR_TEXT_ID;
    sema::IdentId name_id = sema::INVALID_IDENT_ID;
    ValueId slot = INVALID_VALUE_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
};

struct LoopContext {
    BlockId break_target = INVALID_BLOCK_ID;
    BlockId continue_target = INVALID_BLOCK_ID;
    base::usize cleanup_depth = 0;
};

enum class CleanupActionKind {
    drop_local,
    defer_call,
};

struct CleanupAction {
    CleanupActionKind kind = CleanupActionKind::drop_local;
    ValueId slot = INVALID_VALUE_ID;
    ValueId flag = INVALID_VALUE_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    syntax::ExprId defer_expr = syntax::INVALID_EXPR_ID;
    IrTextId name = INVALID_IR_TEXT_ID;
    CleanupDropMode drop_mode = CleanupDropMode::full;
};

struct AggregateElementInit {
    IrTextId name = INVALID_IR_TEXT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
};

struct PendingConstant {
    GlobalConstantId id = INVALID_GLOBAL_CONSTANT_ID;
    syntax::ExprId initializer = syntax::INVALID_EXPR_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    IrTextId literal_text = INVALID_IR_TEXT_ID;
    bool is_literal = false;
};

enum class TryShapeKind {
    none,
    result,
    option,
};

struct TryShape {
    TryShapeKind kind = TryShapeKind::none;
    const sema::EnumCaseInfo* success_case = nullptr;
    const sema::EnumCaseInfo* failure_case = nullptr;
};

struct EnumCaseTypeKey {
    base::u32 type = sema::TypeHandle::INVALID_VALUE;
    sema::IdentId case_name = sema::INVALID_IDENT_ID;

    [[nodiscard]] bool operator==(const EnumCaseTypeKey& other) const noexcept
    {
        return type == other.type && case_name == other.case_name;
    }
};

struct EnumCaseTypeKeyHash {
    [[nodiscard]] std::size_t operator()(const EnumCaseTypeKey& key) const noexcept
    {
        const std::size_t type_hash = std::hash<base::u32>{}(key.type);
        const std::size_t name_hash = sema::IdentIdHash{}(key.case_name);
        return type_hash ^ (name_hash + 0x9e3779b9U + (type_hash << 6U) + (type_hash >> 2U));
    }
};

class Lowerer final {
public:
    Lowerer(const syntax::AstModule& ast, const sema::CheckedModule& checked);

    [[nodiscard]] Module lower();

    struct ExprView {
        syntax::ExprKind kind = syntax::ExprKind::invalid;
        base::SourceRange range{};
        std::string_view scope_name;
        base::SourceRange scope_range{};
        std::string_view text;
        sema::IdentId scope_name_id = sema::INVALID_IDENT_ID;
        sema::IdentId text_id = sema::INVALID_IDENT_ID;
        syntax::UnaryOp unary_op = syntax::UnaryOp::logical_not;
        syntax::ExprId unary_operand = syntax::INVALID_EXPR_ID;
        syntax::ExprId try_operand = syntax::INVALID_EXPR_ID;
        syntax::BinaryOp binary_op = syntax::BinaryOp::add;
        syntax::ExprId binary_lhs = syntax::INVALID_EXPR_ID;
        syntax::ExprId binary_rhs = syntax::INVALID_EXPR_ID;
        syntax::ExprId callee = syntax::INVALID_EXPR_ID;
        std::span<const syntax::ExprId> args{};
        syntax::ExprId condition = syntax::INVALID_EXPR_ID;
        syntax::PatternId condition_pattern = syntax::INVALID_PATTERN_ID;
        syntax::ExprId then_expr = syntax::INVALID_EXPR_ID;
        syntax::ExprId else_expr = syntax::INVALID_EXPR_ID;
        syntax::StmtId block = syntax::INVALID_STMT_ID;
        syntax::ExprId block_result = syntax::INVALID_EXPR_ID;
        syntax::ExprId match_value = syntax::INVALID_EXPR_ID;
        std::span<const syntax::MatchArm> match_arms{};
        std::span<const syntax::ExprId> array_elements{};
        std::span<const syntax::ExprId> tuple_elements{};
        syntax::ExprId array_repeat_value = syntax::INVALID_EXPR_ID;
        syntax::ExprId array_repeat_count = syntax::INVALID_EXPR_ID;
        syntax::ExprId object = syntax::INVALID_EXPR_ID;
        std::string_view field_name;
        sema::IdentId field_name_id = sema::INVALID_IDENT_ID;
        syntax::ExprId index = syntax::INVALID_EXPR_ID;
        syntax::ExprId slice_start = syntax::INVALID_EXPR_ID;
        syntax::ExprId slice_end = syntax::INVALID_EXPR_ID;
        std::span<const syntax::FieldInit> field_inits{};
        syntax::TypeId cast_type = syntax::INVALID_TYPE_ID;
        syntax::ExprId cast_expr = syntax::INVALID_EXPR_ID;
    };

    struct FunctionBodyView {
        std::span<const syntax::ParamDecl> params{};
        syntax::StmtId body = syntax::INVALID_STMT_ID;
    };

    [[nodiscard]] ExprView expr_view(syntax::ExprId expr_id) const noexcept;

    void lower_record_layouts();
    void declare_global_constants();
    void lower_function_declarations();
    void lower_global_constant_initializers();
    void index_enum_cases();

    [[nodiscard]] std::string_view item_symbol_text(base::u32 index, const syntax::ItemNode& item) const;
    [[nodiscard]] IrTextId item_symbol(base::u32 index, const syntax::ItemNode& item);
    [[nodiscard]] IrTextId enum_case_symbol(
        base::u32 index, const syntax::ItemNode& item, const syntax::EnumCaseDecl& enum_case);
    [[nodiscard]] sema::TypeHandle enum_case_type(IrTextId symbol) const noexcept;
    [[nodiscard]] GlobalConstantId enum_case_constant(std::string_view name) noexcept;
    [[nodiscard]] IrTextId enum_case_symbol(std::string_view name) noexcept;
    [[nodiscard]] const sema::EnumCaseInfo* enum_case_info(IrTextId symbol) const noexcept;

    [[nodiscard]] const syntax::PatternNode* pattern_node(syntax::PatternId id) const;
    [[nodiscard]] IrTextId pattern_case_symbol(syntax::PatternId id);
    [[nodiscard]] bool is_irrefutable_pattern(syntax::PatternId id, sema::TypeHandle matched_type) const;
    [[nodiscard]] ValueId append_true_value();
    [[nodiscard]] ValueId append_pattern_condition(
        syntax::PatternId id, ValueId source_address, sema::TypeHandle source_type);
    [[nodiscard]] ValueId append_pattern_element_address(
        ValueId source_address, sema::TypeHandle source_type, ValueId index, sema::TypeHandle element_type);
    [[nodiscard]] ValueId append_pattern_source_length(ValueId source_address, sema::TypeHandle source_type);

    void lower_function_body(FunctionId function_id, FunctionBodyView body);
    void lower_generic_function_body(FunctionId function_id, const sema::GenericFunctionInstanceBodyView& body);
    void lower_trait_default_method_body(FunctionId function_id, const sema::TraitDefaultMethodInstanceBodyView& body);
    void lower_block(syntax::StmtId block_id);
    void lower_block_contents(syntax::StmtId block_id);
    void lower_stmt(syntax::StmtId stmt_id);
    void lower_local_pattern(
        syntax::PatternId pattern, ValueId source_address, sema::TypeHandle source_type, bool is_mutable);
    void lower_if(const syntax::StmtNode& stmt);
    void lower_for(const syntax::StmtNode& stmt);
    void lower_for_range(syntax::StmtId stmt_id, const syntax::StmtNode& stmt);
    void lower_while(const syntax::StmtNode& stmt);

    [[nodiscard]] ValueId lower_short_circuit_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_if_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_block_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_match_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_try_expr(syntax::ExprId expr_id, const ExprView& expr);

    [[nodiscard]] ValueId append_enum_case_ref(std::string_view case_name, sema::TypeHandle enum_type);
    [[nodiscard]] ValueId append_enum_tag_literal(std::string_view case_name, sema::TypeHandle tag_type);
    [[nodiscard]] ValueId lower_enum_constructor(const sema::EnumCaseInfo& enum_case, syntax::ExprId payload_expr);
    [[nodiscard]] ValueId lower_enum_constructor_call(const sema::EnumCaseInfo& enum_case, const ExprView& expr);
    [[nodiscard]] ValueId append_enum_constructor(const sema::EnumCaseInfo& enum_case, ValueId payload_value);
    [[nodiscard]] ValueId append_enum_payload_load(ValueId enum_slot, sema::TypeHandle payload_type, IrTextId name);
    [[nodiscard]] const sema::EnumCaseInfo* enum_case_by_type_and_case(
        sema::TypeHandle enum_type, std::string_view case_name) const noexcept;
    [[nodiscard]] TryShape classify_try_shape(sema::TypeHandle enum_type) const noexcept;

    [[nodiscard]] ValueId append_temp_alloca(std::string_view name, sema::TypeHandle value_type);
    [[nodiscard]] ValueId append_integer_literal(std::string_view text, sema::TypeHandle value_type);
    [[nodiscard]] ValueId append_binary_value(BinaryOp op, sema::TypeHandle type, ValueId lhs, ValueId rhs);
    [[nodiscard]] ValueId append_for_range_condition(
        ValueId cursor_slot, ValueId end_slot, ValueId step_slot, sema::TypeHandle range_type);
    [[nodiscard]] sema::ResourceSemanticsSummary resource_summary(sema::TypeHandle type);
    [[nodiscard]] bool cleanup_required(sema::TypeHandle type);
    [[nodiscard]] sema::OwnedUseMode expr_owned_use_mode(syntax::ExprId expr) const noexcept;
    [[nodiscard]] const LocalBinding* local_binding_for_name_expr(syntax::ExprId expr_id) const noexcept;
    [[nodiscard]] std::optional<LocalPlacePath> local_place_path(syntax::ExprId expr_id) const;
    [[nodiscard]] LocalPlaceProjection local_place_projection_for_field_expr(
        const syntax::FieldExprPayload& field) const;
    [[nodiscard]] const LocalBinding* local_binding_for_place_path(const LocalPlacePath& path) const noexcept;
    [[nodiscard]] const sema::StructInfo* struct_info_for_type(sema::TypeHandle type) const noexcept;
    [[nodiscard]] ValueId append_bool_literal(bool value);
    [[nodiscard]] ValueId append_cleanup_flag(std::string_view name);
    void append_cleanup_flag_store(ValueId flag, bool initialized);
    [[nodiscard]] const sema::DestructorInfo* custom_destructor_info(sema::TypeHandle type) const noexcept;
    [[nodiscard]] CallTarget destructor_call_target(const sema::DestructorInfo& destructor);
    [[nodiscard]] bool type_may_emit_runtime_drop(sema::TypeHandle type, CleanupDropMode mode);
    [[nodiscard]] bool append_custom_destructor_call(ValueId slot, sema::TypeHandle type, IrTextId name);
    [[nodiscard]] bool append_runtime_drop_glue(
        ValueId slot, sema::TypeHandle type, IrTextId name, CleanupDropMode mode);
    void append_conditional_runtime_drop(
        ValueId condition, ValueId slot, sema::TypeHandle type, IrTextId name, CleanupDropMode mode);
    void append_cleanup_drop(
        ValueId slot, sema::TypeHandle type, IrTextId name, CleanupDropMode mode = CleanupDropMode::full);
    void append_cleanup_drop_if(
        ValueId slot, ValueId flag, sema::TypeHandle type, IrTextId name,
        CleanupDropMode mode = CleanupDropMode::full);
    void register_local_cleanup(LocalBinding& binding, std::string_view name);
    [[nodiscard]] bool register_structured_local_cleanup(LocalBinding& binding, std::string_view name);
    [[nodiscard]] bool append_structured_cleanup_bindings(LocalBinding& binding, ValueId address, sema::TypeHandle type,
        const std::vector<CleanupProjection>& projections, std::string_view name);
    [[nodiscard]] bool push_structured_cleanup_children(
        const StructuredCleanupFrame& frame, std::vector<StructuredCleanupFrame>& pending);
    void append_cleanup_binding(LocalBinding& binding, ValueId address, ValueId flag, sema::TypeHandle type,
        IrTextId name, const std::vector<CleanupProjection>& projections,
        CleanupDropMode mode = CleanupDropMode::full);
    void append_root_cleanup_flag_from_fields(const LocalBinding& binding);
    [[nodiscard]] ValueId append_field_address(
        ValueId object, IrTextId field_name, sema::TypeHandle field_type, sema::PointerMutability mutability);
    [[nodiscard]] bool cleanup_projection_matches(
        const CleanupProjection& cleanup, const LocalPlaceProjection& place) const noexcept;
    [[nodiscard]] bool cleanup_binding_has_prefix(
        const CleanupBinding& binding, std::span<const LocalPlaceProjection> projections) const noexcept;
    void append_local_cleanup_drop_if(const LocalBinding& binding);
    void append_place_cleanup_drop_if(syntax::ExprId expr_id);
    void mark_place_initialized(syntax::ExprId expr_id);
    void mark_local_moved(sema::IdentId name_id);
    void mark_expr_place_moved(syntax::ExprId expr_id);
    [[nodiscard]] ValueId append_load(ValueId address, sema::TypeHandle value_type, IrTextId name = INVALID_IR_TEXT_ID);
    [[nodiscard]] ValueId enum_field_addr(ValueId object, IrTextId field_name);
    void bind_pattern_locals(syntax::PatternId pattern, ValueId source_address, sema::TypeHandle source_type);
    void bind_pattern_locals_with_mutability(
        syntax::PatternId pattern, ValueId source_address, sema::TypeHandle source_type, bool is_mutable);
    void collect_pattern_binding_slots(syntax::PatternId pattern, sema::TypeHandle source_type, bool is_mutable,
        std::unordered_map<sema::IdentId, PatternBindingSlot, sema::IdentIdHash>& slots);
    void store_pattern_bindings(syntax::PatternId pattern, ValueId source_address, sema::TypeHandle source_type,
        const std::unordered_map<sema::IdentId, PatternBindingSlot, sema::IdentIdHash>& slots);

    [[nodiscard]] ValueId lower_expr(syntax::ExprId expr_id);
    [[nodiscard]] ValueId lower_expr(syntax::ExprId expr_id, sema::TypeHandle expected_type);
    [[nodiscard]] ValueId lower_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, sema::TypeHandle expected_type);
    [[nodiscard]] ValueId lower_name(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_bound_value_ref(syntax::ExprId expr_id, IrTextId symbol);
    [[nodiscard]] ValueId lower_unary_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_binary_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_call_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_indirect_call_expr(
        syntax::ExprId expr_id, const ExprView& expr, sema::TypeHandle callee_type);
    [[nodiscard]] ValueId lower_array_literal_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_tuple_literal_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_record_aggregate_with_rollback(
        sema::TypeHandle aggregate_type, std::span<const AggregateElementInit> elements, std::string_view name);
    [[nodiscard]] ValueId lower_array_aggregate_with_rollback(
        sema::TypeHandle aggregate_type, std::span<const AggregateElementInit> elements, std::string_view name);
    [[nodiscard]] bool aggregate_needs_rollback(
        sema::TypeHandle aggregate_type, std::span<const AggregateElementInit> elements);
    [[nodiscard]] ValueId lower_slice_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_str_slice_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_load_expr(syntax::ExprId expr_id);
    [[nodiscard]] ValueId lower_struct_literal_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_cast_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_size_or_align_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_slice_projection_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_str_projection_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_str_utf8_slice_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] ValueId lower_str_from_bytes_unchecked_expr(syntax::ExprId expr_id, const ExprView& expr);

    void emit_cleanup_scopes(base::usize keep_depth);
    void push_local_scope();
    void pop_local_scope();
    void bind_local(sema::IdentId name_id, LocalBinding binding);
    [[nodiscard]] ValueId lower_place_addr(syntax::ExprId expr_id);
    [[nodiscard]] PlaceAddress lower_place_address(syntax::ExprId expr_id);
    [[nodiscard]] PlaceAddress lower_object_place_or_value(syntax::ExprId expr_id);

    [[nodiscard]] bool is_local_slot_type(sema::TypeHandle type) const noexcept;
    [[nodiscard]] bool pointee_is_mutable(syntax::ExprId expr_id) const noexcept;
    [[nodiscard]] ValueId append_slice_data(
        ValueId slice_value, sema::PointerMutability mutability, sema::TypeHandle element_type);
    [[nodiscard]] ValueId append_slice_len(ValueId slice_value);

    [[nodiscard]] CallTarget call_target(syntax::ExprId callee);
    [[nodiscard]] IrTextId call_symbol(syntax::ExprId callee);
    [[nodiscard]] IrTextId value_symbol(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] sema::TypeHandle call_param_type(FunctionId function_id, base::usize index) const noexcept;
    [[nodiscard]] sema::TypeHandle variadic_argument_type(sema::TypeHandle source_type) const noexcept;
    [[nodiscard]] sema::TypeHandle expr_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] sema::TypeHandle syntax_type(syntax::TypeId type) const noexcept;
    [[nodiscard]] sema::TypeHandle function_return_type(base::u32 index, const syntax::ItemNode& item) noexcept;
    [[nodiscard]] sema::TypeHandle stmt_local_type(syntax::StmtId stmt) const noexcept;
    [[nodiscard]] sema::TypeHandle aggregate_field_type(
        sema::TypeHandle aggregate_type, std::string_view name) const noexcept;
    [[nodiscard]] sema::TypeHandle local_load_type(ValueId slot) const noexcept;

    [[nodiscard]] ValueId coerce_value(ValueId value_id, sema::TypeHandle target_type);
    [[nodiscard]] ValueId append_value(const Value& value);
    void append_store(ValueId target, ValueId source);
    void append_branch_if_open(BlockId target);
    [[nodiscard]] bool has_terminator(BlockId block) const;
    void set_terminator(BlockId block, const Terminator& terminator) const;

    const syntax::AstModule& ast_;
    const sema::CheckedModule& checked_;
    ActiveSideTables active_side_tables_;
    Module module_;
    Function* current_function_ = nullptr;
    BlockId current_block_ = INVALID_BLOCK_ID;
    bool lowering_constant_initializer_ = false;
    std::unordered_map<sema::IdentId, LocalBinding, sema::IdentIdHash> locals_;
    std::vector<LocalScopeFrame> local_scopes_;
    std::unordered_map<IrTextId, FunctionId, sema::IdentIdHash> function_symbols_;
    std::unordered_map<IrTextId, GlobalConstantId, sema::IdentIdHash> constant_symbols_;
    std::unordered_map<sema::IdentId, const sema::EnumCaseInfo*, sema::IdentIdHash> enum_cases_by_name_;
    std::unordered_map<IrTextId, const sema::EnumCaseInfo*, sema::IdentIdHash> enum_cases_by_c_name_;
    std::unordered_map<EnumCaseTypeKey, const sema::EnumCaseInfo*, EnumCaseTypeKeyHash> enum_cases_by_type_and_case_;
    std::vector<PendingConstant> pending_constants_;
    std::vector<FunctionId> item_functions_;
    std::vector<FunctionId> generic_instance_functions_;
    std::vector<FunctionId> trait_default_instance_functions_;
    std::vector<LoopContext> loop_contexts_;
    sema::ResourceSemanticsClassifier resources_;
    std::unordered_map<base::u32, sema::ResourceSemanticsSummary> resource_cache_;
    std::vector<std::vector<CleanupAction>> cleanup_scopes_;
};

} // namespace aurex::ir::detail
