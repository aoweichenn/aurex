#include <aurex/infrastructure/base/string_literal.hpp>
#include <aurex/midend/ir/enum_layout.hpp>

#include <algorithm>

#include <aurex/frontend/sema/call_arguments.hpp>
#include <midend/ir/lowering/private/lower_ast_internal.hpp>

namespace aurex::ir::detail {

namespace {

constexpr base::usize IR_METHOD_RECEIVER_PARAM_COUNT = 1;
constexpr base::usize IR_STR_FROM_BYTES_UNCHECKED_ARG_COUNT = 2;
constexpr base::usize IR_DYNPROJECT_INTRINSIC_TYPE_ARG_COUNT = 2;
constexpr base::usize IR_DYNPROJECT_INTRINSIC_ARG_COUNT = 1;
constexpr std::string_view IR_AGGREGATE_ROLLBACK_FLAG_NAME = "aggregate.rollback";
constexpr std::string_view IR_AGGREGATE_ROLLBACK_LOAD_NAME = "aggregate.rollback.result";
constexpr std::string_view IR_AGGREGATE_ROLLBACK_ARRAY_FIELD_PREFIX = "aggregate.rollback.";
constexpr std::string_view IR_DYNPROJECT_INTRINSIC_NAME = "dynproject";

[[nodiscard]] bool lambda_capture_kind_is_reference(const syntax::LambdaCaptureKind kind) noexcept
{
    return kind == syntax::LambdaCaptureKind::shared_reference
        || kind == syntax::LambdaCaptureKind::mutable_reference;
}

struct RollbackCleanupScope {
    std::vector<std::vector<CleanupAction>>* scopes = nullptr;
    base::usize index = 0;
    base::usize base = 0;

    ~RollbackCleanupScope()
    {
        if (this->scopes != nullptr && this->index < this->scopes->size()) {
            (*this->scopes)[this->index].resize(this->base);
        }
    }

    [[nodiscard]] std::vector<CleanupAction>* current() const noexcept
    {
        if (this->scopes == nullptr || this->index >= this->scopes->size()) {
            return nullptr;
        }
        return &(*this->scopes)[this->index];
    }
};

template <typename T, typename Allocator>
[[nodiscard]] std::span<const T> readonly_span(const std::vector<T, Allocator>& values) noexcept
{
    return {values.data(), values.size()};
}

[[nodiscard]] UnaryOp map_unary(const syntax::UnaryOp op) noexcept
{
    switch (op) {
        case syntax::UnaryOp::logical_not:
            return UnaryOp::logical_not;
        case syntax::UnaryOp::numeric_negate:
            return UnaryOp::numeric_negate;
        case syntax::UnaryOp::bitwise_not:
            return UnaryOp::bitwise_not;
        case syntax::UnaryOp::address_of:
            return UnaryOp::address_of;
        case syntax::UnaryOp::address_of_mut:
            return UnaryOp::address_of;
        case syntax::UnaryOp::dereference:
            return UnaryOp::dereference;
    }
    return UnaryOp::logical_not;
}

[[nodiscard]] BinaryOp map_binary(const syntax::BinaryOp op) noexcept
{
    switch (op) {
        case syntax::BinaryOp::add:
            return BinaryOp::add;
        case syntax::BinaryOp::sub:
            return BinaryOp::sub;
        case syntax::BinaryOp::mul:
            return BinaryOp::mul;
        case syntax::BinaryOp::div:
            return BinaryOp::div;
        case syntax::BinaryOp::mod:
            return BinaryOp::mod;
        case syntax::BinaryOp::shl:
            return BinaryOp::shl;
        case syntax::BinaryOp::shr:
            return BinaryOp::shr;
        case syntax::BinaryOp::less:
            return BinaryOp::less;
        case syntax::BinaryOp::less_equal:
            return BinaryOp::less_equal;
        case syntax::BinaryOp::greater:
            return BinaryOp::greater;
        case syntax::BinaryOp::greater_equal:
            return BinaryOp::greater_equal;
        case syntax::BinaryOp::equal:
            return BinaryOp::equal;
        case syntax::BinaryOp::not_equal:
            return BinaryOp::not_equal;
        case syntax::BinaryOp::bit_and:
            return BinaryOp::bit_and;
        case syntax::BinaryOp::bit_xor:
            return BinaryOp::bit_xor;
        case syntax::BinaryOp::bit_or:
            return BinaryOp::bit_or;
        case syntax::BinaryOp::logical_and:
            return BinaryOp::logical_and;
        case syntax::BinaryOp::logical_or:
            return BinaryOp::logical_or;
    }
    return BinaryOp::add;
}

} // namespace

Lowerer::ExprView Lowerer::expr_view(const syntax::ExprId expr_id) const noexcept
{
    ExprView view{};
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ast_.exprs.size()) {
        return view;
    }

    view.kind = this->ast_.exprs.kind(expr_id.value);
    view.range = this->ast_.exprs.range(expr_id.value);
    if (const syntax::LiteralExprPayload* const literal = this->ast_.exprs.literal_payload(expr_id.value);
        literal != nullptr) {
        view.text = literal->text;
        return view;
    }
    if (const syntax::CastExprPayload* const cast = this->ast_.exprs.cast_payload(expr_id.value); cast != nullptr) {
        view.cast_type = cast->type;
        view.cast_expr = cast->expr;
        return view;
    }

    switch (view.kind) {
        case syntax::ExprKind::name: {
            const syntax::NameExprPayload& payload = *this->ast_.exprs.name_payload(expr_id.value);
            view.scope_name = payload.scope_name;
            view.scope_range = payload.scope_range;
            view.text = payload.text;
            view.scope_name_id = payload.scope_name_id;
            view.text_id = payload.text_id;
            break;
        }
        case syntax::ExprKind::unary: {
            const syntax::UnaryExprPayload& payload = *this->ast_.exprs.unary_payload(expr_id.value);
            view.unary_op = payload.op;
            view.unary_operand = payload.operand;
            break;
        }
        case syntax::ExprKind::try_expr: {
            const syntax::TryExprPayload& payload = *this->ast_.exprs.try_payload(expr_id.value);
            view.try_operand = payload.operand;
            break;
        }
        case syntax::ExprKind::binary: {
            const syntax::BinaryExprPayload& payload = *this->ast_.exprs.binary_payload(expr_id.value);
            view.binary_op = payload.op;
            view.binary_lhs = payload.lhs;
            view.binary_rhs = payload.rhs;
            break;
        }
        case syntax::ExprKind::call:
        case syntax::ExprKind::str_from_bytes_unchecked: {
            const syntax::CallExprPayload& payload = *this->ast_.exprs.call_payload(expr_id.value);
            view.callee = payload.callee;
            view.args = readonly_span(payload.args);
            break;
        }
        case syntax::ExprKind::lambda: {
            const syntax::LambdaExprPayload& payload = *this->ast_.exprs.lambda_payload(expr_id.value);
            view.lambda_params = readonly_span(payload.params);
            view.lambda_body = payload.body;
            break;
        }
        case syntax::ExprKind::if_expr: {
            const syntax::IfExprPayload& payload = *this->ast_.exprs.if_payload(expr_id.value);
            view.condition = payload.condition;
            view.condition_pattern = payload.condition_pattern;
            view.then_expr = payload.then_expr;
            view.else_expr = payload.else_expr;
            break;
        }
        case syntax::ExprKind::block_expr:
        case syntax::ExprKind::unsafe_block: {
            const syntax::BlockExprPayload& payload = *this->ast_.exprs.block_payload(expr_id.value);
            view.block = payload.block;
            view.block_result = payload.result;
            break;
        }
        case syntax::ExprKind::match_expr: {
            const syntax::MatchExprPayload& payload = *this->ast_.exprs.match_payload(expr_id.value);
            view.match_value = payload.value;
            view.match_arms = readonly_span(payload.arms);
            break;
        }
        case syntax::ExprKind::array_literal: {
            const syntax::ArrayExprPayload& payload = *this->ast_.exprs.array_payload(expr_id.value);
            view.array_elements = readonly_span(payload.elements);
            view.array_repeat_value = payload.repeat_value;
            view.array_repeat_count = payload.repeat_count;
            break;
        }
        case syntax::ExprKind::tuple_literal: {
            const syntax::AstArenaVector<syntax::ExprId>& payload = *this->ast_.exprs.tuple_elements(expr_id.value);
            view.tuple_elements = readonly_span(payload);
            break;
        }
        case syntax::ExprKind::field: {
            const syntax::FieldExprPayload& payload = *this->ast_.exprs.field_payload(expr_id.value);
            view.object = payload.object;
            view.field_name = payload.field_name;
            view.field_name_id = payload.field_name_id;
            break;
        }
        case syntax::ExprKind::index: {
            const syntax::IndexExprPayload& payload = *this->ast_.exprs.index_payload(expr_id.value);
            view.object = payload.object;
            view.index = payload.index;
            break;
        }
        case syntax::ExprKind::slice: {
            const syntax::SliceExprPayload& payload = *this->ast_.exprs.slice_payload(expr_id.value);
            view.object = payload.object;
            view.slice_start = payload.start;
            view.slice_end = payload.end;
            break;
        }
        case syntax::ExprKind::struct_literal: {
            const syntax::StructLiteralExprPayload& payload = *this->ast_.exprs.struct_literal_payload(expr_id.value);
            view.object = payload.object;
            view.scope_name = payload.scope_name;
            view.scope_range = payload.scope_range;
            view.text = payload.name;
            view.scope_name_id = payload.scope_name_id;
            view.text_id = payload.name_id;
            view.field_inits = readonly_span(payload.field_inits);
            break;
        }
        case syntax::ExprKind::invalid:
        case syntax::ExprKind::generic_apply:
        case syntax::ExprKind::integer_literal:
        case syntax::ExprKind::float_literal:
        case syntax::ExprKind::bool_literal:
        case syntax::ExprKind::null_literal:
        case syntax::ExprKind::string_literal:
        case syntax::ExprKind::c_string_literal:
        case syntax::ExprKind::raw_string_literal:
        case syntax::ExprKind::byte_string_literal:
        case syntax::ExprKind::byte_literal:
        case syntax::ExprKind::char_literal:
        case syntax::ExprKind::cast:
        case syntax::ExprKind::pcast:
        case syntax::ExprKind::bcast:
        case syntax::ExprKind::size_of:
        case syntax::ExprKind::align_of:
        case syntax::ExprKind::ptr_addr:
        case syntax::ExprKind::paddr:
        case syntax::ExprKind::slice_data:
        case syntax::ExprKind::slice_len:
        case syntax::ExprKind::str_data:
        case syntax::ExprKind::str_byte_len:
        case syntax::ExprKind::str_is_valid_utf8:
        case syntax::ExprKind::str_from_utf8_checked:
            break;
    }
    return view;
}

ValueId Lowerer::lower_short_circuit_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    const ValueId lhs = this->lower_expr(expr.binary_lhs);
    const BlockId lhs_block = this->current_block_;

    const BlockId rhs_block =
        add_block(this->module_, *this->current_function_,
            "logical.rhs" + std::to_string(this->current_function_->blocks.size()));
    const BlockId exit_block =
        add_block(this->module_, *this->current_function_,
            "logical.exit" + std::to_string(this->current_function_->blocks.size()));

    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = lhs;
    if (expr.binary_op == syntax::BinaryOp::logical_and) {
        cond.then_target = rhs_block;
        cond.else_target = exit_block;
    } else {
        cond.then_target = exit_block;
        cond.else_target = rhs_block;
    }
    this->set_terminator(this->current_block_, cond);

    this->current_block_ = rhs_block;
    const ValueId rhs = this->lower_expr(expr.binary_rhs);
    const BlockId rhs_tail_block = this->current_block_;
    this->append_branch_if_open(exit_block);

    this->current_block_ = exit_block;
    Value result = this->module_.make_value();
    result.kind = ValueKind::phi;
    result.type = this->expr_type(expr_id);
    result.incoming.push_back(PhiInput{lhs_block, lhs});
    result.incoming.push_back(PhiInput{rhs_tail_block, rhs});
    return this->append_value(result);
}

ValueId Lowerer::lower_if_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    if (this->current_function_ == nullptr || !is_valid(this->current_block_)) {
        return INVALID_VALUE_ID;
    }
    ValueId condition = INVALID_VALUE_ID;
    ValueId condition_slot = INVALID_VALUE_ID;
    const sema::TypeHandle condition_type = this->expr_type(expr.condition);
    if (syntax::is_valid(expr.condition_pattern)) {
        condition_slot = this->append_temp_alloca("if.pattern", condition_type);
        this->append_store(condition_slot, this->lower_expr(expr.condition));
        condition = this->append_pattern_condition(expr.condition_pattern, condition_slot, condition_type);
    } else {
        condition = this->lower_expr(expr.condition);
    }
    const BlockId then_block =
        add_block(this->module_, *this->current_function_,
            "if.expr.then" + std::to_string(this->current_function_->blocks.size()));
    const BlockId else_block =
        add_block(this->module_, *this->current_function_,
            "if.expr.else" + std::to_string(this->current_function_->blocks.size()));
    const BlockId join_block =
        add_block(this->module_, *this->current_function_,
            "if.expr.join" + std::to_string(this->current_function_->blocks.size()));

    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = then_block;
    cond.else_target = else_block;
    this->set_terminator(this->current_block_, cond);

    this->current_block_ = then_block;
    this->push_local_scope();
    if (syntax::is_valid(expr.condition_pattern)) {
        this->bind_pattern_locals(expr.condition_pattern, condition_slot, condition_type);
    }
    const ValueId then_value = this->lower_expr(expr.then_expr, this->expr_type(expr_id));
    this->pop_local_scope();
    const BlockId then_tail_block = this->current_block_;
    this->append_branch_if_open(join_block);

    this->current_block_ = else_block;
    const ValueId else_value = this->lower_expr(expr.else_expr, this->expr_type(expr_id));
    const BlockId else_tail_block = this->current_block_;
    this->append_branch_if_open(join_block);

    this->current_block_ = join_block;
    Value result = this->module_.make_value();
    result.kind = ValueKind::phi;
    result.type = this->expr_type(expr_id);
    result.incoming.push_back(PhiInput{then_tail_block, then_value});
    result.incoming.push_back(PhiInput{else_tail_block, else_value});
    return this->append_value(result);
}

ValueId Lowerer::lower_block_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    this->push_local_scope();
    const base::usize scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});
    this->lower_block_contents(expr.block);
    ValueId result = INVALID_VALUE_ID;
    if (syntax::is_valid(expr.block_result)) {
        result = this->lower_expr(expr.block_result, this->expr_type(expr_id));
    }
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(scope_depth);
    }
    this->cleanup_scopes_.resize(scope_depth);
    this->pop_local_scope();
    return result;
}

ValueId Lowerer::lower_expr(const syntax::ExprId expr_id)
{
    return this->lower_expr(expr_id, sema::INVALID_TYPE_HANDLE);
}

ValueId Lowerer::lower_expr(const syntax::ExprId expr_id, const sema::TypeHandle expected_type)
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ast_.exprs.size()) {
        return INVALID_VALUE_ID;
    }
    const ExprView expr = this->expr_view(expr_id);
    switch (expr.kind) {
        case syntax::ExprKind::integer_literal:
        case syntax::ExprKind::float_literal:
        case syntax::ExprKind::bool_literal:
        case syntax::ExprKind::byte_literal:
        case syntax::ExprKind::char_literal:
        case syntax::ExprKind::null_literal:
            return this->lower_literal_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::string_literal:
        case syntax::ExprKind::raw_string_literal:
        case syntax::ExprKind::c_string_literal:
        case syntax::ExprKind::byte_string_literal:
            return this->lower_literal_expr(expr_id, expr, expected_type);
        case syntax::ExprKind::name:
            return this->lower_name(expr_id, expr);
        case syntax::ExprKind::generic_apply:
            return INVALID_VALUE_ID;
        case syntax::ExprKind::unary:
            return this->lower_unary_expr(expr_id, expr);
        case syntax::ExprKind::binary:
            return this->lower_binary_expr(expr_id, expr);
        case syntax::ExprKind::call:
            return this->lower_call_expr(expr_id, expr);
        case syntax::ExprKind::lambda:
            return this->lower_lambda_expr(expr_id);
        case syntax::ExprKind::try_expr:
            return this->lower_try_expr(expr_id, expr);
        case syntax::ExprKind::if_expr:
            return this->lower_if_expr(expr_id, expr);
        case syntax::ExprKind::block_expr:
            return this->lower_block_expr(expr_id, expr);
        case syntax::ExprKind::unsafe_block:
            return this->lower_block_expr(expr_id, expr);
        case syntax::ExprKind::match_expr:
            return this->lower_match_expr(expr_id, expr);
        case syntax::ExprKind::array_literal:
            return this->lower_array_literal_expr(expr_id, expr);
        case syntax::ExprKind::tuple_literal:
            return this->lower_tuple_literal_expr(expr_id, expr);
        case syntax::ExprKind::slice:
            return this->lower_slice_expr(expr_id, expr);
        case syntax::ExprKind::field:
            if (const ValueId value = this->lower_bound_value_ref(expr_id, this->value_symbol(expr_id, expr));
                is_valid(value)) {
                return value;
            }
            [[fallthrough]];
        case syntax::ExprKind::index:
            return this->lower_load_expr(expr_id);
        case syntax::ExprKind::struct_literal:
            return this->lower_struct_literal_expr(expr_id, expr);
        case syntax::ExprKind::cast:
        case syntax::ExprKind::pcast:
        case syntax::ExprKind::bcast:
        case syntax::ExprKind::ptr_addr:
        case syntax::ExprKind::paddr:
            return this->lower_cast_expr(expr_id, expr);
        case syntax::ExprKind::size_of:
        case syntax::ExprKind::align_of:
            return this->lower_size_or_align_expr(expr_id, expr);
        case syntax::ExprKind::slice_data:
        case syntax::ExprKind::slice_len:
            return this->lower_slice_projection_expr(expr_id, expr);
        case syntax::ExprKind::str_data:
        case syntax::ExprKind::str_byte_len:
            return this->lower_str_projection_expr(expr_id, expr);
        case syntax::ExprKind::str_is_valid_utf8:
        case syntax::ExprKind::str_from_utf8_checked:
            return this->lower_str_utf8_slice_expr(expr_id, expr);
        case syntax::ExprKind::str_from_bytes_unchecked:
            return this->lower_str_from_bytes_unchecked_expr(expr_id, expr);
        case syntax::ExprKind::invalid:
            return INVALID_VALUE_ID;
    }
    return INVALID_VALUE_ID;
}

ValueId Lowerer::lower_lambda_expr(const syntax::ExprId expr_id)
{
    for (base::usize index = 0; index < this->checked_.lambdas.size(); ++index) {
        const sema::CheckedLambdaInfo& lambda = this->checked_.lambdas[index];
        if (lambda.expr.value != expr_id.value || index >= this->lambda_functions_.size()) {
            continue;
        }
        if (!lambda.captures.empty()) {
            Value value = this->module_.make_value();
            value.kind = ValueKind::aggregate;
            value.type = this->expr_type(expr_id);
            value.fields.reserve(lambda.captures.size());
            for (const sema::CheckedLambdaInfo::Capture& capture : lambda.captures) {
                if (syntax::is_valid(capture.initializer)) {
                    const ValueId captured_value = lambda_capture_kind_is_reference(capture.kind)
                        ? this->lower_place_addr(capture.initializer)
                        : this->coerce_value(this->lower_expr(capture.initializer, capture.type), capture.type);
                    value.fields.push_back(FieldValue{
                        this->module_.intern(capture.field_name.view()),
                        captured_value,
                    });
                    continue;
                }
                const auto local = this->locals_.find(capture.name_id);
                if (local == this->locals_.end()) {
                    return INVALID_VALUE_ID;
                }
                ValueId captured_value = local->second.slot;
                if (!lambda_capture_kind_is_reference(capture.kind)) {
                    captured_value = this->append_load(
                        local->second.slot, capture.type, this->module_.intern(capture.name.view()));
                }
                value.fields.push_back(FieldValue{
                    this->module_.intern(capture.field_name.view()),
                    captured_value,
                });
            }
            return this->append_value(value);
        }
        const FunctionId function_id = this->lambda_functions_[index];
        if (!is_valid(function_id) || function_id.value >= this->module_.functions.size()) {
            return INVALID_VALUE_ID;
        }
        Value value = this->module_.make_value();
        value.kind = ValueKind::function_ref;
        value.name = this->module_.functions[function_id.value].symbol;
        value.call_target = function_id;
        value.type = this->expr_type(expr_id);
        return this->append_value(value);
    }
    return INVALID_VALUE_ID;
}

ValueId Lowerer::lower_literal_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const sema::TypeHandle expected_type)
{
    if (expr.kind == syntax::ExprKind::byte_string_literal) {
        const base::StringLiteralDecode decoded =
            base::decode_string_literal(expr.text, base::StringLiteralKind::byte_string);
        Value aggregate = this->module_.make_value();
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = this->expr_type(expr_id);
        aggregate.elements.reserve(decoded.decoded.size());
        const sema::TypeHandle byte_type = this->module_.types.builtin(sema::BuiltinType::u8);
        for (const char byte : decoded.decoded) {
            Value element = this->module_.make_value();
            element.kind = ValueKind::integer_literal;
            element.type = byte_type;
            element.text =
                this->module_.intern(std::to_string(static_cast<unsigned>(static_cast<unsigned char>(byte))));
            aggregate.elements.push_back(this->append_value(element));
        }
        return this->append_value(aggregate);
    }

    Value value = this->module_.make_value();
    if (expr.kind == syntax::ExprKind::byte_literal) {
        value.kind = ValueKind::byte_literal;
    } else if (expr.kind == syntax::ExprKind::char_literal) {
        value.kind = ValueKind::char_literal;
    } else if (expr.kind == syntax::ExprKind::bool_literal) {
        value.kind = ValueKind::bool_literal;
    } else if (expr.kind == syntax::ExprKind::float_literal) {
        value.kind = ValueKind::float_literal;
    } else if (expr.kind == syntax::ExprKind::null_literal) {
        value.kind = ValueKind::null_literal;
        value.type = sema::is_valid(expected_type) ? expected_type : this->expr_type(expr_id);
        return this->append_value(value);
    } else if (expr.kind == syntax::ExprKind::string_literal) {
        value.kind = ValueKind::string_literal;
    } else if (expr.kind == syntax::ExprKind::raw_string_literal) {
        value.kind = ValueKind::raw_string_literal;
    } else if (expr.kind == syntax::ExprKind::c_string_literal) {
        value.kind = ValueKind::c_string_literal;
    } else {
        value.kind = ValueKind::integer_literal;
    }
    value.type = this->expr_type(expr_id);
    value.text = this->module_.intern(expr.text);
    return this->append_value(value);
}

ValueId Lowerer::lower_bound_value_ref(const syntax::ExprId expr_id, const IrTextId symbol)
{
    if (const sema::EnumCaseInfo* enum_case = this->enum_case_info(symbol); enum_case != nullptr
        && !sema::is_valid(enum_case->payload_type) && is_payload_enum(this->module_.types, enum_case->type)) {
        return this->lower_enum_constructor(*enum_case, syntax::INVALID_EXPR_ID);
    }
    if (const auto constant = this->constant_symbols_.find(symbol); constant != this->constant_symbols_.end()) {
        Value value = this->module_.make_value();
        value.kind = ValueKind::constant_ref;
        value.name = symbol;
        value.constant = constant->second;
        value.type = this->module_.constants[constant->second.value].type;
        return this->append_value(value);
    }
    if (const auto function = this->function_symbols_.find(symbol); function != this->function_symbols_.end()) {
        Value value = this->module_.make_value();
        value.kind = ValueKind::function_ref;
        value.name = symbol;
        value.call_target = function->second;
        value.type = this->expr_type(expr_id);
        return this->append_value(value);
    }
    return INVALID_VALUE_ID;
}

ValueId Lowerer::lower_name(const syntax::ExprId expr_id, const ExprView& expr)
{
    const auto local = expr.scope_name.empty() ? this->locals_.find(expr.text_id) : this->locals_.end();
    if (local != this->locals_.end()) {
        Value value = this->module_.make_value();
        value.kind = ValueKind::load;
        value.name = this->module_.intern(expr.text);
        value.type = this->local_load_type(local->second.slot);
        value.object = local->second.slot;
        const ValueId result = this->append_value(value);
        if (this->expr_owned_use_mode(expr_id) == sema::OwnedUseMode::owned_consume) {
            this->mark_local_moved(expr.text_id);
        }
        return result;
    }
    const IrTextId symbol = this->value_symbol(expr_id, expr);
    if (const ValueId value = this->lower_bound_value_ref(expr_id, symbol); is_valid(value)) {
        return value;
    }
    Value value = this->module_.make_value();
    value.kind = ValueKind::load;
    value.name = this->module_.intern(expr.text.empty() ? std::string_view{"<global>"} : expr.text);
    value.type = sema::INVALID_TYPE_HANDLE;
    return this->append_value(value);
}

ValueId Lowerer::lower_unary_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    if (expr.unary_op == syntax::UnaryOp::address_of || expr.unary_op == syntax::UnaryOp::address_of_mut) {
        return this->coerce_value(this->lower_place_addr(expr.unary_operand), this->expr_type(expr_id));
    }
    if (expr.unary_op == syntax::UnaryOp::dereference) {
        Value value = this->module_.make_value();
        value.kind = ValueKind::load;
        value.type = this->expr_type(expr_id);
        value.object = this->lower_expr(expr.unary_operand);
        return this->append_value(value);
    }
    Value value = this->module_.make_value();
    value.kind = ValueKind::unary;
    value.type = this->expr_type(expr_id);
    value.unary_op = map_unary(expr.unary_op);
    value.lhs = this->lower_expr(expr.unary_operand);
    return this->append_value(value);
}

ValueId Lowerer::lower_binary_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    if (!this->lowering_constant_initializer_
        && (expr.binary_op == syntax::BinaryOp::logical_and || expr.binary_op == syntax::BinaryOp::logical_or)) {
        return this->lower_short_circuit_expr(expr_id, expr);
    }
    Value value = this->module_.make_value();
    value.kind = ValueKind::binary;
    value.type = this->expr_type(expr_id);
    value.binary_op = map_binary(expr.binary_op);
    const sema::TypeHandle lhs_type = this->expr_type(expr.binary_lhs);
    const sema::TypeHandle rhs_type = this->expr_type(expr.binary_rhs);
    const sema::TypeHandle lhs_expected =
        !sema::is_valid(lhs_type) && this->module_.types.is_pointer(rhs_type) ? rhs_type : sema::INVALID_TYPE_HANDLE;
    const sema::TypeHandle rhs_expected =
        !sema::is_valid(rhs_type) && this->module_.types.is_pointer(lhs_type) ? lhs_type : sema::INVALID_TYPE_HANDLE;
    value.lhs = this->lower_expr(expr.binary_lhs, lhs_expected);
    value.rhs = this->lower_expr(expr.binary_rhs, rhs_expected);
    return this->append_value(value);
}

ValueId Lowerer::lower_call_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    if (const ValueId dynproject = this->lower_dynproject_intrinsic_expr(expr_id, expr); is_valid(dynproject)) {
        return dynproject;
    }
    const IrTextId symbol = this->call_symbol(expr.callee);
    if (const sema::EnumCaseInfo* enum_case = this->enum_case_info(symbol);
        enum_case != nullptr && !enum_case->payload_types.empty()) {
        return this->lower_enum_constructor_call(*enum_case, expr);
    }
    const sema::TraitMethodCallBinding* binding = this->checked_.trait_method_call_binding_for_call_expr(expr_id);
    if (binding == nullptr) {
        binding = this->checked_.trait_method_call_binding_for_callee_expr(expr.callee);
    }
    if (binding != nullptr && binding->dispatch == sema::TraitMethodDispatchKind::vtable_slot) {
        return this->lower_dyn_trait_call_expr(expr_id, expr, *binding);
    }
    Value value = this->module_.make_value();
    value.kind = ValueKind::call;
    value.type = this->expr_type(expr_id);
    const CallTarget target = this->call_target(expr.callee);
    const sema::TypeHandle callee_type = this->expr_type(expr.callee);
    if (const sema::CheckedLambdaInfo* const closure = this->lambda_for_environment_type(callee_type);
        closure != nullptr) {
        return this->lower_closure_call_expr(expr_id, expr, *closure);
    }
    if (!is_valid(target.function) && sema::is_valid(callee_type) && this->module_.types.is_function(callee_type)) {
        return this->lower_indirect_call_expr(expr_id, expr, callee_type);
    }
    value.name = target.symbol;
    value.call_target = target.function;
    const sema::FunctionCallBinding* const direct_binding =
        this->checked_.function_call_binding_for_expr(expr_id);
    const std::span<const syntax::ExprId> call_args = direct_binding == nullptr
        ? expr.args
        : sema::ordered_call_args_or_source(direct_binding->ordered_args, expr);
    base::usize param_offset = 0;
    syntax::ExprId receiver_callee = expr.callee;
    if (syntax::is_valid(receiver_callee) && receiver_callee.value < this->ast_.exprs.size()
        && this->ast_.exprs.kind(receiver_callee.value) == syntax::ExprKind::generic_apply) {
        if (const syntax::GenericApplyExprPayload* const apply =
                this->ast_.exprs.generic_apply_payload(receiver_callee.value);
            apply != nullptr) {
            receiver_callee = apply->callee;
        }
    }
    const syntax::FieldExprPayload* const callee_field =
        syntax::is_valid(receiver_callee) && receiver_callee.value < this->ast_.exprs.size()
        ? this->ast_.exprs.field_payload(receiver_callee.value)
        : nullptr;
    if (callee_field != nullptr && is_valid(target.function) && target.function.value < this->module_.functions.size()
        && this->module_.functions[target.function.value].signature_params.size()
            == call_args.size() + IR_METHOD_RECEIVER_PARAM_COUNT) {
        const sema::TypeHandle receiver_type = this->expr_type(callee_field->object);
        const sema::TypeHandle param_type = this->call_param_type(target.function, 0);
        ValueId receiver = INVALID_VALUE_ID;
        const bool param_is_indirect = sema::is_valid(param_type)
            && (this->module_.types.is_pointer(param_type) || this->module_.types.is_reference(param_type));
        const bool receiver_is_indirect = sema::is_valid(receiver_type)
            && (this->module_.types.is_pointer(receiver_type) || this->module_.types.is_reference(receiver_type));
        if (param_is_indirect && !receiver_is_indirect) {
            const sema::TypeInfo& param_info = this->module_.types.get(param_type);
            receiver = param_info.pointer_mutability == sema::PointerMutability::mut
                ? this->lower_place_addr(callee_field->object)
                : this->lower_object_place_or_value(callee_field->object).address;
        } else {
            receiver = this->lower_expr(callee_field->object, param_type);
        }
        value.args.push_back(this->coerce_value(receiver, param_type));
        param_offset = IR_METHOD_RECEIVER_PARAM_COUNT;
    }
    const bool variadic_call = is_valid(target.function) && target.function.value < this->module_.functions.size()
        && this->module_.functions[target.function.value].is_variadic;
    for (base::usize i = 0; i < call_args.size(); ++i) {
        sema::TypeHandle param_type = this->call_param_type(target.function, i + param_offset);
        const ValueId arg = this->lower_expr(call_args[i], param_type);
        if (variadic_call && !sema::is_valid(param_type) && is_valid(arg) && arg.value < this->module_.values.size()) {
            param_type = this->variadic_argument_type(this->module_.values[arg.value].type);
        }
        value.args.push_back(this->coerce_value(arg, param_type));
    }
    return this->append_value(value);
}

ValueId Lowerer::lower_closure_call_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const sema::CheckedLambdaInfo& lambda)
{
    const FunctionId function_id = this->lambda_function_for_expr(lambda.expr);
    if (!is_valid(function_id) || !sema::is_valid(lambda.environment_type)) {
        return INVALID_VALUE_ID;
    }
    Value value = this->module_.make_value();
    value.kind = ValueKind::call;
    value.type = this->expr_type(expr_id);
    value.call_target = function_id;
    value.name = this->module_.intern(lambda.c_name.view());

    const ValueId environment_value = this->lower_expr(expr.callee, lambda.environment_type);
    const ValueId environment_slot = this->append_temp_alloca("closure.env", lambda.environment_type);
    this->append_store(environment_slot, environment_value);
    value.args.push_back(environment_slot);
    for (base::usize i = 0; i < expr.args.size(); ++i) {
        const sema::TypeHandle param_type =
            i < lambda.param_types.size() ? lambda.param_types[i] : sema::INVALID_TYPE_HANDLE;
        const ValueId arg = this->lower_expr(expr.args[i], param_type);
        value.args.push_back(this->coerce_value(arg, param_type));
    }
    return this->append_value(value);
}

ValueId Lowerer::lower_dynproject_intrinsic_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    if (expr.args.size() != IR_DYNPROJECT_INTRINSIC_ARG_COUNT || !syntax::is_valid(expr.callee)
        || expr.callee.value >= this->ast_.exprs.size()
        || this->ast_.exprs.kind(expr.callee.value) != syntax::ExprKind::generic_apply) {
        return INVALID_VALUE_ID;
    }
    const syntax::GenericApplyExprPayload* const apply = this->ast_.exprs.generic_apply_payload(expr.callee.value);
    if (apply == nullptr || apply->type_args.size() != IR_DYNPROJECT_INTRINSIC_TYPE_ARG_COUNT
        || !syntax::is_valid(apply->callee) || apply->callee.value >= this->ast_.exprs.size()
        || this->ast_.exprs.kind(apply->callee.value) != syntax::ExprKind::name) {
        return INVALID_VALUE_ID;
    }
    const syntax::NameExprPayload* const name = this->ast_.exprs.name_payload(apply->callee.value);
    if (name == nullptr || !name->scope_name.empty() || name->text != IR_DYNPROJECT_INTRINSIC_NAME) {
        return INVALID_VALUE_ID;
    }

    const sema::TypeHandle target_reference_type = this->expr_type(expr_id);
    const sema::TypeHandle argument_type = this->expr_type(expr.args.front());
    const sema::TraitObjectUpcastCoercionFact* const upcast =
        this->dynproject_upcast_coercion(expr.args.front(), target_reference_type);
    if (upcast == nullptr || !this->module_.types.is_reference(argument_type)) {
        Value invalid = this->module_.make_value();
        invalid.kind = ValueKind::undef;
        invalid.type = target_reference_type;
        return this->append_value(invalid);
    }

    const ValueId composition = this->lower_expr(expr.args.front(), argument_type);
    const ValueId projected_principal = this->coerce_value(composition, upcast->source_reference_type);
    return this->coerce_value(projected_principal, target_reference_type);
}

ValueId Lowerer::lower_dyn_trait_call_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const sema::TraitMethodCallBinding& binding)
{
    syntax::ExprId receiver_callee = expr.callee;
    if (syntax::is_valid(receiver_callee) && receiver_callee.value < this->ast_.exprs.size()
        && this->ast_.exprs.kind(receiver_callee.value) == syntax::ExprKind::generic_apply) {
        if (const syntax::GenericApplyExprPayload* const apply =
                this->ast_.exprs.generic_apply_payload(receiver_callee.value);
            apply != nullptr) {
            receiver_callee = apply->callee;
        }
    }
    const syntax::FieldExprPayload* const callee_field =
        syntax::is_valid(receiver_callee) && receiver_callee.value < this->ast_.exprs.size()
        ? this->ast_.exprs.field_payload(receiver_callee.value)
        : nullptr;
    const sema::TypeHandle dyn_receiver_type = this->dispatch_trait_object_receiver_type(binding);
    const sema::TypeHandle dyn_object_type =
        sema::is_valid(dyn_receiver_type) && this->module_.types.is_reference(dyn_receiver_type)
        ? this->module_.types.get(dyn_receiver_type).pointee
        : sema::INVALID_TYPE_HANDLE;
    const TraitObjectVTableLayout* layout = this->trait_object_vtable_layout(binding.vtable_layout);
    if (layout == nullptr) {
        layout = this->trait_object_vtable_layout_for_object(dyn_object_type);
    }
    const TraitObjectVTableMethodSlot* slot = nullptr;
    if (layout != nullptr) {
        for (const TraitObjectVTableMethodSlot& method_slot : layout->method_slots) {
            if (method_slot.slot == binding.vtable_slot) {
                slot = &method_slot;
                break;
            }
        }
    }
    if (slot == nullptr) {
        slot = this->trait_object_vtable_method_slot_for_object(dyn_object_type, binding.vtable_slot);
    }
    if (callee_field == nullptr || slot == nullptr || layout == nullptr) {
        Value invalid = this->module_.make_value();
        invalid.kind = ValueKind::undef;
        invalid.type = this->expr_type(expr_id);
        return this->append_value(invalid);
    }

    const sema::TypeInfo& function = this->module_.types.get(slot->function_type);
    const ValueId receiver =
        this->coerce_value(this->lower_expr(callee_field->object, dyn_receiver_type), dyn_receiver_type);
    Value data = this->module_.make_value();
    data.kind = ValueKind::trait_object_data;
    data.type = function.function_params.empty() ? slot->receiver_type : function.function_params.front();
    data.object = receiver;
    data.vtable_layout = layout->layout_key;
    const ValueId receiver_data = this->append_value(data);

    Value vtable = this->module_.make_value();
    vtable.kind = ValueKind::trait_object_vtable;
    vtable.type = this->vtable_pointer_type();
    vtable.object = receiver;
    vtable.vtable_layout = layout->layout_key;
    const ValueId vtable_id = this->append_value(vtable);

    Value slot_value = this->module_.make_value();
    slot_value.kind = ValueKind::vtable_slot;
    slot_value.type = slot->function_type;
    slot_value.object = vtable_id;
    slot_value.vtable_layout = layout->layout_key;
    slot_value.vtable_slot = binding.vtable_slot;
    const ValueId callee = this->append_value(slot_value);

    Value call = this->module_.make_value();
    call.kind = ValueKind::call;
    call.type = this->expr_type(expr_id);
    call.object = callee;
    call.name = this->module_.intern(binding.method_name.empty() ? std::string_view{"dyn.call"}
                                                                 : binding.method_name.view());
    if (function.function_params.empty()) {
        return this->append_value(call);
    }
    call.args.push_back(this->coerce_value(receiver_data, function.function_params.front()));
    const std::span<const syntax::ExprId> call_args = sema::ordered_call_args_or_source(binding.ordered_args, expr);
    for (base::usize i = 0; i < call_args.size(); ++i) {
        const base::usize param_index = i + IR_METHOD_RECEIVER_PARAM_COUNT;
        sema::TypeHandle param_type = param_index < function.function_params.size()
            ? function.function_params[param_index]
            : sema::INVALID_TYPE_HANDLE;
        const ValueId arg = this->lower_expr(call_args[i], param_type);
        if (function.function_is_variadic && !sema::is_valid(param_type) && is_valid(arg)
            && arg.value < this->module_.values.size()) {
            param_type = this->variadic_argument_type(this->module_.values[arg.value].type);
        }
        call.args.push_back(this->coerce_value(arg, param_type));
    }
    return this->append_value(call);
}

ValueId Lowerer::lower_indirect_call_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const sema::TypeHandle callee_type)
{
    const sema::TypeInfo& function = this->module_.types.get(callee_type);
    Value value = this->module_.make_value();
    value.kind = ValueKind::call;
    value.type = this->expr_type(expr_id);
    value.object = this->lower_expr(expr.callee);
    value.name = this->module_.intern("<indirect>");
    const bool variadic_call = function.function_is_variadic;
    for (base::usize i = 0; i < expr.args.size(); ++i) {
        sema::TypeHandle param_type =
            i < function.function_params.size() ? function.function_params[i] : sema::INVALID_TYPE_HANDLE;
        const ValueId arg = this->lower_expr(expr.args[i], param_type);
        if (variadic_call && !sema::is_valid(param_type) && is_valid(arg) && arg.value < this->module_.values.size()) {
            param_type = this->variadic_argument_type(this->module_.values[arg.value].type);
        }
        value.args.push_back(this->coerce_value(arg, param_type));
    }
    return this->append_value(value);
}

ValueId Lowerer::lower_array_literal_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::aggregate;
    value.type = this->expr_type(expr_id);
    if (!sema::is_valid(value.type) || !this->module_.types.is_array(value.type)) {
        return this->append_value(value);
    }

    const sema::TypeInfo& array = this->module_.types.get(value.type);
    if (syntax::is_valid(expr.array_repeat_value)) {
        if (array.array_count == 0) {
            return this->append_value(value);
        }
        if (array.array_count > 1 && !sema::resource_is_copy(this->resource_summary(array.array_element))) {
            return this->append_value(value);
        }
        const ValueId repeated =
            this->coerce_value(this->lower_expr(expr.array_repeat_value, array.array_element), array.array_element);
        value.elements.reserve(static_cast<base::usize>(array.array_count));
        for (base::u64 index = 0; index < array.array_count; ++index) {
            value.elements.push_back(repeated);
        }
        return this->append_value(value);
    }

    std::vector<AggregateElementInit> elements;
    elements.reserve(expr.array_elements.size());
    for (const syntax::ExprId element : expr.array_elements) {
        elements.push_back(AggregateElementInit{
            INVALID_IR_TEXT_ID,
            element,
            array.array_element,
        });
    }
    if (this->aggregate_needs_rollback(value.type, elements)) {
        return this->lower_array_aggregate_with_rollback(value.type, elements, "array.literal");
    }

    value.elements.reserve(expr.array_elements.size());
    for (const syntax::ExprId element : expr.array_elements) {
        value.elements.push_back(
            this->coerce_value(this->lower_expr(element, array.array_element), array.array_element));
    }
    return this->append_value(value);
}

ValueId Lowerer::lower_tuple_literal_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::aggregate;
    value.type = this->expr_type(expr_id);
    if (!sema::is_valid(value.type) || !this->module_.types.is_tuple(value.type)) {
        return this->append_value(value);
    }

    const sema::TypeInfo& tuple = this->module_.types.get(value.type);
    const base::usize count = std::min(tuple.tuple_elements.size(), expr.tuple_elements.size());
    std::vector<AggregateElementInit> elements;
    elements.reserve(count);
    for (base::usize i = 0; i < count; ++i) {
        elements.push_back(AggregateElementInit{
            this->module_.intern(std::to_string(i)),
            expr.tuple_elements[i],
            tuple.tuple_elements[i],
        });
    }
    if (this->aggregate_needs_rollback(value.type, elements)) {
        return this->lower_record_aggregate_with_rollback(value.type, elements, "tuple.literal");
    }

    value.fields.reserve(count);
    for (base::usize i = 0; i < count; ++i) {
        const sema::TypeHandle element_type = tuple.tuple_elements[i];
        value.fields.push_back(FieldValue{
            this->module_.intern(std::to_string(i)),
            this->coerce_value(this->lower_expr(expr.tuple_elements[i], element_type), element_type),
        });
    }
    return this->append_value(value);
}

ValueId Lowerer::lower_slice_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    const sema::TypeHandle result_type = this->expr_type(expr_id);
    if (!sema::is_valid(result_type)) {
        Value invalid = this->module_.make_value();
        invalid.kind = ValueKind::undef;
        invalid.type = result_type;
        return this->append_value(invalid);
    }
    if (this->module_.types.is_str(result_type)) {
        return this->lower_str_slice_expr(expr_id, expr);
    }
    if (!this->module_.types.is_slice(result_type)) {
        Value invalid = this->module_.make_value();
        invalid.kind = ValueKind::undef;
        invalid.type = result_type;
        return this->append_value(invalid);
    }

    const sema::TypeInfo& result_slice = this->module_.types.get(result_type);
    const sema::TypeHandle usize_type = this->module_.types.builtin(sema::BuiltinType::usize);
    const ValueId zero = this->append_integer_literal("0", usize_type);
    const ValueId start = syntax::is_valid(expr.slice_start)
        ? this->coerce_value(this->lower_expr(expr.slice_start, usize_type), usize_type)
        : zero;

    ValueId base_data = INVALID_VALUE_ID;
    ValueId end_bound = INVALID_VALUE_ID;
    const sema::TypeHandle object_type = this->expr_type(expr.object);
    if (sema::is_valid(object_type) && this->module_.types.is_array(object_type)) {
        const sema::TypeInfo& array = this->module_.types.get(object_type);
        const PlaceAddress object = this->lower_object_place_or_value(expr.object);
        Value data = this->module_.make_value();
        data.kind = ValueKind::index_addr;
        data.type = this->module_.types.pointer(result_slice.slice_mutability, result_slice.slice_element);
        data.object = object.address;
        data.index = start;
        base_data = this->append_value(data);
        end_bound = syntax::is_valid(expr.slice_end)
            ? this->coerce_value(this->lower_expr(expr.slice_end, usize_type), usize_type)
            : this->append_integer_literal(std::to_string(array.array_count), usize_type);
    } else if (sema::is_valid(object_type) && this->module_.types.is_slice(object_type)) {
        const ValueId source = this->lower_expr(expr.object);
        const ValueId source_data =
            this->append_slice_data(source, result_slice.slice_mutability, result_slice.slice_element);
        Value data = this->module_.make_value();
        data.kind = ValueKind::index_addr;
        data.type = this->module_.types.pointer(result_slice.slice_mutability, result_slice.slice_element);
        data.object = source_data;
        data.index = start;
        base_data = this->append_value(data);
        end_bound = syntax::is_valid(expr.slice_end)
            ? this->coerce_value(this->lower_expr(expr.slice_end, usize_type), usize_type)
            : this->append_slice_len(source);
    }

    const ValueId length = this->append_binary_value(BinaryOp::sub, usize_type, end_bound, start);
    Value value = this->module_.make_value();
    value.kind = ValueKind::slice;
    value.type = result_type;
    value.lhs = base_data;
    value.rhs = length;
    return this->append_value(value);
}

ValueId Lowerer::lower_str_slice_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    const sema::TypeHandle result_type = this->expr_type(expr_id);
    const sema::TypeHandle object_type = this->expr_type(expr.object);
    if (!sema::is_valid(object_type) || !this->module_.types.is_str(object_type)) {
        Value invalid = this->module_.make_value();
        invalid.kind = ValueKind::undef;
        invalid.type = result_type;
        return this->append_value(invalid);
    }

    const sema::TypeHandle usize_type = this->module_.types.builtin(sema::BuiltinType::usize);
    const ValueId source = this->lower_expr(expr.object, object_type);
    const ValueId zero = this->append_integer_literal("0", usize_type);
    const ValueId start = syntax::is_valid(expr.slice_start)
        ? this->coerce_value(this->lower_expr(expr.slice_start, usize_type), usize_type)
        : zero;

    ValueId end = INVALID_VALUE_ID;
    if (syntax::is_valid(expr.slice_end)) {
        end = this->coerce_value(this->lower_expr(expr.slice_end, usize_type), usize_type);
    } else {
        Value length = this->module_.make_value();
        length.kind = ValueKind::str_byte_len;
        length.type = usize_type;
        length.object = source;
        end = this->append_value(length);
    }

    Value value = this->module_.make_value();
    value.kind = ValueKind::str_slice_checked;
    value.type = result_type;
    value.object = source;
    value.lhs = start;
    value.rhs = end;
    return this->append_value(value);
}

ValueId Lowerer::lower_load_expr(const syntax::ExprId expr_id)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::load;
    value.type = this->expr_type(expr_id);
    value.object = this->lower_place_address(expr_id).address;
    const ValueId result = this->append_value(value);
    this->mark_expr_place_moved(expr_id);
    return result;
}

ValueId Lowerer::lower_struct_literal_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::aggregate;
    value.type = this->expr_type(expr_id);
    std::vector<AggregateElementInit> elements;
    elements.reserve(expr.field_inits.size());
    for (const syntax::FieldInit& init : expr.field_inits) {
        elements.push_back(AggregateElementInit{
            this->module_.intern(init.name),
            init.value,
            this->aggregate_field_type(value.type, init.name),
        });
    }
    if (this->aggregate_needs_rollback(value.type, elements)) {
        return this->lower_record_aggregate_with_rollback(
            value.type, elements, expr.text.empty() ? "struct.literal" : expr.text);
    }
    for (const AggregateElementInit& init : elements) {
        value.fields.push_back(FieldValue{
            init.name,
            this->coerce_value(this->lower_expr(init.expr, init.type), init.type),
        });
    }
    return this->append_value(value);
}

bool Lowerer::aggregate_needs_rollback(
    const sema::TypeHandle aggregate_type, const std::span<const AggregateElementInit> elements)
{
    if (this->lowering_constant_initializer_ || this->has_terminator(this->current_block_)
        || !sema::is_valid(aggregate_type) || elements.empty()) {
        return false;
    }
    for (const AggregateElementInit& element : elements) {
        if (this->cleanup_required(element.type)) {
            return true;
        }
    }
    return false;
}

ValueId Lowerer::lower_record_aggregate_with_rollback(const sema::TypeHandle aggregate_type,
    const std::span<const AggregateElementInit> elements, const std::string_view name)
{
    if (!sema::is_valid(aggregate_type)) {
        Value invalid = this->module_.make_value();
        invalid.kind = ValueKind::aggregate;
        invalid.type = aggregate_type;
        return this->append_value(invalid);
    }
    if (this->has_terminator(this->current_block_)) {
        return INVALID_VALUE_ID;
    }

    const ValueId aggregate_slot = this->append_temp_alloca(name, aggregate_type);
    RollbackCleanupScope rollback{
        this->cleanup_scopes_.empty() ? nullptr : &this->cleanup_scopes_,
        this->cleanup_scopes_.empty() ? 0U : this->cleanup_scopes_.size() - 1U,
        this->cleanup_scopes_.empty() ? 0U : this->cleanup_scopes_.back().size(),
    };
    for (const AggregateElementInit& element : elements) {
        const ValueId field_address =
            this->append_field_address(aggregate_slot, element.name, element.type, sema::PointerMutability::mut);
        std::vector<CleanupAction>* const rollback_scope = rollback.current();
        if (this->cleanup_required(element.type) && rollback_scope != nullptr) {
            const ValueId lowered =
                this->coerce_value(this->lower_expr(element.expr, element.type), element.type);
            if (this->has_terminator(this->current_block_)) {
                return INVALID_VALUE_ID;
            }
            const ValueId flag = this->append_cleanup_flag(IR_AGGREGATE_ROLLBACK_FLAG_NAME);
            this->append_cleanup_flag_store(flag, false);
            rollback_scope->push_back(CleanupAction{
                CleanupActionKind::drop_local,
                field_address,
                flag,
                element.type,
                syntax::INVALID_EXPR_ID,
                element.name,
                CleanupDropMode::full,
            });
            this->append_store(field_address, lowered);
            this->append_cleanup_flag_store(flag, true);
            continue;
        }
        const ValueId lowered = this->coerce_value(this->lower_expr(element.expr, element.type), element.type);
        if (this->has_terminator(this->current_block_)) {
            return INVALID_VALUE_ID;
        }
        this->append_store(field_address, lowered);
    }
    return this->append_load(aggregate_slot, aggregate_type, this->module_.intern(IR_AGGREGATE_ROLLBACK_LOAD_NAME));
}

ValueId Lowerer::lower_array_aggregate_with_rollback(const sema::TypeHandle aggregate_type,
    const std::span<const AggregateElementInit> elements, const std::string_view name)
{
    if (!sema::is_valid(aggregate_type) || !this->module_.types.is_array(aggregate_type)) {
        return this->lower_record_aggregate_with_rollback(aggregate_type, elements, name);
    }
    if (this->has_terminator(this->current_block_)) {
        return INVALID_VALUE_ID;
    }
    const sema::TypeInfo& array = this->module_.types.get(aggregate_type);
    const ValueId aggregate_slot = this->append_temp_alloca(name, aggregate_type);
    RollbackCleanupScope rollback{
        this->cleanup_scopes_.empty() ? nullptr : &this->cleanup_scopes_,
        this->cleanup_scopes_.empty() ? 0U : this->cleanup_scopes_.size() - 1U,
        this->cleanup_scopes_.empty() ? 0U : this->cleanup_scopes_.back().size(),
    };
    const base::usize count = std::min(static_cast<base::usize>(array.array_count), elements.size());
    for (base::usize index = 0; index < count; ++index) {
        const AggregateElementInit& element = elements[index];
        const ValueId index_value = this->append_integer_literal(
            std::to_string(index), this->module_.types.builtin(sema::BuiltinType::usize));
        Value element_address = this->module_.make_value();
        element_address.kind = ValueKind::index_addr;
        element_address.type = this->module_.types.pointer(sema::PointerMutability::mut, array.array_element);
        element_address.name =
            this->module_.intern(std::string(IR_AGGREGATE_ROLLBACK_ARRAY_FIELD_PREFIX) + std::to_string(index));
        element_address.object = aggregate_slot;
        element_address.index = index_value;
        const ValueId address = this->append_value(element_address);
        std::vector<CleanupAction>* const rollback_scope = rollback.current();
        if (this->cleanup_required(element.type) && rollback_scope != nullptr) {
            const ValueId lowered =
                this->coerce_value(this->lower_expr(element.expr, element.type), element.type);
            if (this->has_terminator(this->current_block_)) {
                return INVALID_VALUE_ID;
            }
            const ValueId flag = this->append_cleanup_flag(IR_AGGREGATE_ROLLBACK_FLAG_NAME);
            this->append_cleanup_flag_store(flag, false);
            rollback_scope->push_back(CleanupAction{
                CleanupActionKind::drop_local,
                address,
                flag,
                element.type,
                syntax::INVALID_EXPR_ID,
                element_address.name,
                CleanupDropMode::full,
            });
            this->append_store(address, lowered);
            this->append_cleanup_flag_store(flag, true);
            continue;
        }
        const ValueId lowered = this->coerce_value(this->lower_expr(element.expr, element.type), element.type);
        if (this->has_terminator(this->current_block_)) {
            return INVALID_VALUE_ID;
        }
        this->append_store(address, lowered);
    }
    return this->append_load(aggregate_slot, aggregate_type, this->module_.intern(IR_AGGREGATE_ROLLBACK_LOAD_NAME));
}

ValueId Lowerer::lower_cast_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::cast;
    value.type = this->expr_type(expr_id);
    value.target_type = this->expr_type(expr_id);
    value.lhs = this->lower_expr(expr.cast_expr, this->expr_type(expr.cast_expr));
    if (expr.kind == syntax::ExprKind::pcast) {
        value.cast_kind = CastKind::pointer;
    } else if (expr.kind == syntax::ExprKind::bcast) {
        value.cast_kind = CastKind::bcast;
    } else if (expr.kind == syntax::ExprKind::ptr_addr) {
        value.cast_kind = CastKind::ptr_addr;
    } else if (expr.kind == syntax::ExprKind::paddr) {
        value.cast_kind = CastKind::paddr;
    }
    return this->append_value(value);
}

ValueId Lowerer::lower_size_or_align_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    Value value = this->module_.make_value();
    value.kind = expr.kind == syntax::ExprKind::size_of ? ValueKind::size_of : ValueKind::align_of;
    value.type = this->expr_type(expr_id);
    value.target_type = this->syntax_type(expr.cast_type);
    return this->append_value(value);
}

ValueId Lowerer::lower_str_projection_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    Value value = this->module_.make_value();
    value.kind = expr.kind == syntax::ExprKind::str_data ? ValueKind::str_data : ValueKind::str_byte_len;
    value.type = this->expr_type(expr_id);
    value.object = this->lower_expr(expr.cast_expr, this->expr_type(expr.cast_expr));
    return this->append_value(value);
}

ValueId Lowerer::lower_slice_projection_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    const ValueId slice = this->lower_expr(expr.cast_expr, this->expr_type(expr.cast_expr));
    if (expr.kind == syntax::ExprKind::slice_data) {
        const sema::TypeHandle result_type = this->expr_type(expr_id);
        const sema::TypeInfo& pointer = this->module_.types.get(result_type);
        return this->append_slice_data(slice, pointer.pointer_mutability, pointer.pointee);
    }
    return this->append_slice_len(slice);
}

ValueId Lowerer::lower_str_utf8_slice_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    Value value = this->module_.make_value();
    value.kind = expr.kind == syntax::ExprKind::str_is_valid_utf8 ? ValueKind::str_is_valid_utf8
                                                                  : ValueKind::str_from_utf8_checked;
    value.type = this->expr_type(expr_id);
    value.object = this->lower_expr(expr.cast_expr, this->expr_type(expr.cast_expr));
    return this->append_value(value);
}

ValueId Lowerer::lower_str_from_bytes_unchecked_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::str_from_bytes_unchecked;
    value.type = this->expr_type(expr_id);
    if (expr.args.size() == IR_STR_FROM_BYTES_UNCHECKED_ARG_COUNT) {
        value.args.push_back(this->lower_expr(expr.args[0], this->expr_type(expr.args[0])));
        value.args.push_back(this->lower_expr(expr.args[1], this->expr_type(expr.args[1])));
    }
    return this->append_value(value);
}

ValueId Lowerer::lower_place_addr(const syntax::ExprId expr_id)
{
    return this->lower_place_address(expr_id).address;
}

PlaceAddress Lowerer::lower_place_address(const syntax::ExprId expr_id)
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ast_.exprs.size()) {
        return {};
    }
    const ExprView expr = this->expr_view(expr_id);
    if (expr.kind == syntax::ExprKind::name && expr.scope_name.empty()) {
        const auto found = this->locals_.find(expr.text_id);
        if (found == this->locals_.end()) {
            return {};
        }
        return PlaceAddress{found->second.slot, found->second.is_mutable};
    }
    if (expr.kind == syntax::ExprKind::unary && expr.unary_op == syntax::UnaryOp::dereference) {
        return PlaceAddress{this->lower_expr(expr.unary_operand), this->pointee_is_mutable(expr.unary_operand)};
    }
    if (expr.kind == syntax::ExprKind::field) {
        const PlaceAddress object = this->lower_object_place_or_value(expr.object);
        const sema::PointerMutability mutability =
            object.is_mutable ? sema::PointerMutability::mut : sema::PointerMutability::const_;
        return PlaceAddress{
            this->append_field_address(
                object.address, this->module_.intern(expr.field_name), this->expr_type(expr_id), mutability),
            object.is_mutable,
        };
    }
    if (expr.kind == syntax::ExprKind::index) {
        const sema::TypeHandle object_type = this->expr_type(expr.object);
        if (sema::is_valid(object_type) && this->module_.types.is_reference(object_type)) {
            const sema::TypeHandle pointee = this->module_.types.get(object_type).pointee;
            if (sema::is_valid(pointee) && this->module_.types.is_slice(pointee)) {
                const sema::TypeInfo& slice = this->module_.types.get(pointee);
                const ValueId slice_address = this->lower_expr(expr.object);
                const ValueId slice_value =
                    this->append_load(slice_address, pointee, this->module_.intern("slice.ref"));
                const ValueId data_pointer =
                    this->append_slice_data(slice_value, slice.slice_mutability, slice.slice_element);
                Value value = this->module_.make_value();
                value.kind = ValueKind::index_addr;
                value.type = this->module_.types.pointer(slice.slice_mutability, this->expr_type(expr_id));
                value.object = data_pointer;
                value.index = this->lower_expr(expr.index);
                return PlaceAddress{this->append_value(value), slice.slice_mutability == sema::PointerMutability::mut};
            }
        }
        if (sema::is_valid(object_type) && this->module_.types.is_slice(object_type)) {
            const sema::TypeInfo& slice = this->module_.types.get(object_type);
            const ValueId slice_value = this->lower_expr(expr.object);
            const ValueId data_pointer =
                this->append_slice_data(slice_value, slice.slice_mutability, slice.slice_element);
            Value value = this->module_.make_value();
            value.kind = ValueKind::index_addr;
            value.type = this->module_.types.pointer(slice.slice_mutability, this->expr_type(expr_id));
            value.object = data_pointer;
            value.index = this->lower_expr(expr.index);
            return PlaceAddress{this->append_value(value), slice.slice_mutability == sema::PointerMutability::mut};
        }
        const PlaceAddress object = this->lower_object_place_or_value(expr.object);
        const sema::PointerMutability mutability =
            object.is_mutable ? sema::PointerMutability::mut : sema::PointerMutability::const_;
        Value value = this->module_.make_value();
        value.kind = ValueKind::index_addr;
        value.type = this->module_.types.pointer(mutability, this->expr_type(expr_id));
        value.object = object.address;
        value.index = this->lower_expr(expr.index);
        return PlaceAddress{this->append_value(value), object.is_mutable};
    }
    return {};
}

PlaceAddress Lowerer::lower_object_place_or_value(const syntax::ExprId expr_id)
{
    const sema::TypeHandle type = this->expr_type(expr_id);
    if (sema::is_valid(type) && (this->module_.types.is_pointer(type) || this->module_.types.is_reference(type))) {
        return PlaceAddress{this->lower_expr(expr_id),
            this->module_.types.get(type).pointer_mutability == sema::PointerMutability::mut};
    }
    const PlaceAddress place = this->lower_place_address(expr_id);
    if (is_valid(place.address)) {
        return place;
    }
    if (!sema::is_valid(type) || this->module_.types.is_void(type)) {
        return {};
    }
    const ValueId value = this->lower_expr(expr_id);
    if (!is_valid(value)) {
        return {};
    }
    const ValueId slot = this->append_temp_alloca("field.object", type);
    this->append_store(slot, value);
    return PlaceAddress{slot, false};
}

bool Lowerer::is_local_slot_type(const sema::TypeHandle type) const noexcept
{
    return sema::is_valid(type) && this->module_.types.is_pointer(type)
        && this->module_.types.get(type).pointer_mutability == sema::PointerMutability::mut;
}

bool Lowerer::pointee_is_mutable(const syntax::ExprId expr_id) const noexcept
{
    const sema::TypeHandle type = this->expr_type(expr_id);
    return sema::is_valid(type) && (this->module_.types.is_pointer(type) || this->module_.types.is_reference(type))
        && this->module_.types.get(type).pointer_mutability == sema::PointerMutability::mut;
}

ValueId Lowerer::append_slice_data(
    const ValueId slice_value, const sema::PointerMutability mutability, const sema::TypeHandle element_type)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::slice_data;
    value.type = this->module_.types.pointer(mutability, element_type);
    value.object = slice_value;
    return this->append_value(value);
}

ValueId Lowerer::append_slice_len(const ValueId slice_value)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::slice_len;
    value.type = this->module_.types.builtin(sema::BuiltinType::usize);
    value.object = slice_value;
    return this->append_value(value);
}

CallTarget Lowerer::call_target(const syntax::ExprId callee)
{
    const IrTextId symbol = this->call_symbol(callee);
    const auto found = this->function_symbols_.find(symbol);
    if (found != function_symbols_.end()) {
        return CallTarget{found->second, symbol};
    }
    return CallTarget{INVALID_FUNCTION_ID, symbol};
}

FunctionId Lowerer::lambda_function_for_expr(const syntax::ExprId expr) const noexcept
{
    if (!syntax::is_valid(expr)) {
        return INVALID_FUNCTION_ID;
    }
    for (base::usize index = 0; index < this->checked_.lambdas.size(); ++index) {
        if (this->checked_.lambdas[index].expr.value != expr.value || index >= this->lambda_functions_.size()) {
            continue;
        }
        return this->lambda_functions_[index];
    }
    return INVALID_FUNCTION_ID;
}

const sema::CheckedLambdaInfo* Lowerer::lambda_for_environment_type(const sema::TypeHandle type) const noexcept
{
    if (!sema::is_valid(type)) {
        return nullptr;
    }
    for (const sema::CheckedLambdaInfo& lambda : this->checked_.lambdas) {
        if (!lambda.has_unsupported_capture && lambda.environment_type.value == type.value
            && !lambda.captures.empty()) {
            return &lambda;
        }
    }
    return nullptr;
}

IrTextId Lowerer::call_symbol(const syntax::ExprId callee)
{
    if (syntax::is_valid(callee) && this->active_side_tables_.generic != nullptr
        && this->active_side_tables_.generic->sparse) {
        const base::usize local = this->active_side_tables_.generic->local_expr_index(callee);
        if (local != sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX && this->active_side_tables_.expr_c_name_ids != nullptr
            && local < this->active_side_tables_.expr_c_name_ids->size()) {
            if (const std::string_view c_name =
                    this->checked_.c_name_text((*this->active_side_tables_.expr_c_name_ids)[local]);
                !c_name.empty()) {
                return this->module_.intern(c_name);
            }
        }
        const auto found = this->active_side_tables_.generic->sparse_expr_c_name_ids.find(callee.value);
        if (found != this->active_side_tables_.generic->sparse_expr_c_name_ids.end()) {
            if (const std::string_view c_name = this->checked_.c_name_text(found->second); !c_name.empty()) {
                return this->module_.intern(c_name);
            }
        }
    }
    if (syntax::is_valid(callee) && this->active_side_tables_.expr_c_name_ids != nullptr
        && callee.value < this->active_side_tables_.expr_c_name_ids->size()) {
        const std::string_view c_name =
            this->checked_.c_name_text((*this->active_side_tables_.expr_c_name_ids)[callee.value]);
        if (!c_name.empty()) {
            return this->module_.intern(c_name);
        }
    }
    if (syntax::is_valid(callee) && callee.value < this->ast_.exprs.size()) {
        return this->module_.intern(this->expr_view(callee).text);
    }
    return this->module_.intern("<invalid>");
}

IrTextId Lowerer::value_symbol(const syntax::ExprId expr_id, const ExprView& expr)
{
    if (syntax::is_valid(expr_id) && this->active_side_tables_.generic != nullptr
        && this->active_side_tables_.generic->sparse) {
        const base::usize local = this->active_side_tables_.generic->local_expr_index(expr_id);
        if (local != sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX && this->active_side_tables_.expr_c_name_ids != nullptr
            && local < this->active_side_tables_.expr_c_name_ids->size()) {
            if (const std::string_view c_name =
                    this->checked_.c_name_text((*this->active_side_tables_.expr_c_name_ids)[local]);
                !c_name.empty()) {
                return this->module_.intern(c_name);
            }
        }
        const auto found = this->active_side_tables_.generic->sparse_expr_c_name_ids.find(expr_id.value);
        if (found != this->active_side_tables_.generic->sparse_expr_c_name_ids.end()) {
            if (const std::string_view c_name = this->checked_.c_name_text(found->second); !c_name.empty()) {
                return this->module_.intern(c_name);
            }
        }
    }
    if (syntax::is_valid(expr_id) && this->active_side_tables_.expr_c_name_ids != nullptr
        && expr_id.value < this->active_side_tables_.expr_c_name_ids->size()) {
        const std::string_view c_name =
            this->checked_.c_name_text((*this->active_side_tables_.expr_c_name_ids)[expr_id.value]);
        if (!c_name.empty()) {
            return this->module_.intern(c_name);
        }
    }
    if (expr.kind == syntax::ExprKind::field) {
        return this->module_.intern(expr.field_name);
    }
    return this->module_.intern(expr.text);
}

sema::TypeHandle Lowerer::call_param_type(const FunctionId function_id, const base::usize index) const noexcept
{
    if (!is_valid(function_id) || function_id.value >= this->module_.functions.size()) {
        return sema::INVALID_TYPE_HANDLE;
    }
    const Function& function = this->module_.functions[function_id.value];
    if (index >= function.signature_params.size()) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return function.signature_params[index].type;
}

sema::TypeHandle Lowerer::variadic_argument_type(const sema::TypeHandle source_type) const noexcept
{
    if (!sema::is_valid(source_type)) {
        return source_type;
    }
    const sema::TypeInfo& info = this->module_.types.get(source_type);
    if (info.kind != sema::TypeKind::builtin) {
        return source_type;
    }
    switch (info.builtin) {
        case sema::BuiltinType::bool_:
        case sema::BuiltinType::i8:
        case sema::BuiltinType::u8:
        case sema::BuiltinType::i16:
        case sema::BuiltinType::u16:
            return this->module_.types.builtin(sema::BuiltinType::i32);
        case sema::BuiltinType::f32:
            return this->module_.types.builtin(sema::BuiltinType::f64);
        default:
            return source_type;
    }
}

sema::TypeHandle Lowerer::expr_type(const syntax::ExprId expr) const noexcept
{
    if (syntax::is_valid(expr) && this->active_side_tables_.generic != nullptr
        && this->active_side_tables_.generic->sparse) {
        const base::usize local = this->active_side_tables_.generic->local_expr_index(expr);
        if (local != sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX && this->active_side_tables_.expr_types != nullptr
            && local < this->active_side_tables_.expr_types->size()) {
            const sema::TypeHandle type = (*this->active_side_tables_.expr_types)[local];
            if (sema::is_valid(type)) {
                return type;
            }
        }
        const auto found = this->active_side_tables_.generic->sparse_expr_types.find(expr.value);
        if (found != this->active_side_tables_.generic->sparse_expr_types.end()) {
            return found->second;
        }
    }
    if (!syntax::is_valid(expr) || this->active_side_tables_.expr_types == nullptr
        || expr.value >= this->active_side_tables_.expr_types->size()) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return (*this->active_side_tables_.expr_types)[expr.value];
}

sema::OwnedUseMode Lowerer::expr_owned_use_mode(const syntax::ExprId expr) const noexcept
{
    if (syntax::is_valid(expr) && this->active_side_tables_.generic != nullptr
        && this->active_side_tables_.generic->sparse) {
        const base::usize local = this->active_side_tables_.generic->local_expr_index(expr);
        if (local != sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
            && this->active_side_tables_.expr_owned_use_modes != nullptr
            && local < this->active_side_tables_.expr_owned_use_modes->size()) {
            return (*this->active_side_tables_.expr_owned_use_modes)[local];
        }
        const auto found = this->active_side_tables_.generic->sparse_expr_owned_use_modes.find(expr.value);
        if (found != this->active_side_tables_.generic->sparse_expr_owned_use_modes.end()) {
            return found->second;
        }
    }
    if (!syntax::is_valid(expr) || this->active_side_tables_.expr_owned_use_modes == nullptr
        || expr.value >= this->active_side_tables_.expr_owned_use_modes->size()) {
        return sema::OwnedUseMode::none;
    }
    return (*this->active_side_tables_.expr_owned_use_modes)[expr.value];
}

sema::TypeHandle Lowerer::syntax_type(const syntax::TypeId type) const noexcept
{
    if (syntax::is_valid(type) && this->active_side_tables_.generic != nullptr
        && this->active_side_tables_.generic->sparse) {
        const base::usize local = this->active_side_tables_.generic->local_type_index(type);
        if (local != sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
            && this->active_side_tables_.syntax_type_handles != nullptr
            && local < this->active_side_tables_.syntax_type_handles->size()) {
            const sema::TypeHandle resolved = (*this->active_side_tables_.syntax_type_handles)[local];
            if (sema::is_valid(resolved)) {
                return resolved;
            }
        }
        const auto found = this->active_side_tables_.generic->sparse_syntax_type_handles.find(type.value);
        if (found != this->active_side_tables_.generic->sparse_syntax_type_handles.end()) {
            return found->second;
        }
    }
    if (!syntax::is_valid(type) || this->active_side_tables_.syntax_type_handles == nullptr
        || type.value >= this->active_side_tables_.syntax_type_handles->size()) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return (*this->active_side_tables_.syntax_type_handles)[type.value];
}

sema::TypeHandle Lowerer::stmt_local_type(const syntax::StmtId stmt) const noexcept
{
    if (syntax::is_valid(stmt) && this->active_side_tables_.generic != nullptr
        && this->active_side_tables_.generic->sparse) {
        const base::usize local = this->active_side_tables_.generic->local_stmt_index(stmt);
        if (local != sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
            && this->active_side_tables_.stmt_local_types != nullptr
            && local < this->active_side_tables_.stmt_local_types->size()) {
            const sema::TypeHandle type = (*this->active_side_tables_.stmt_local_types)[local];
            if (sema::is_valid(type)) {
                return type;
            }
        }
        const auto found = this->active_side_tables_.generic->sparse_stmt_local_types.find(stmt.value);
        if (found != this->active_side_tables_.generic->sparse_stmt_local_types.end()) {
            return found->second;
        }
    }
    if (!syntax::is_valid(stmt) || this->active_side_tables_.stmt_local_types == nullptr
        || stmt.value >= this->active_side_tables_.stmt_local_types->size()) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return (*this->active_side_tables_.stmt_local_types)[stmt.value];
}

sema::TypeHandle Lowerer::aggregate_field_type(
    const sema::TypeHandle aggregate_type, const std::string_view name) const noexcept
{
    const RecordField* field = find_record_field(this->module_, aggregate_type, this->module_.find_text(name));
    return field == nullptr ? sema::INVALID_TYPE_HANDLE : field->type;
}

sema::TypeHandle Lowerer::local_load_type(const ValueId slot) const noexcept
{
    if (!is_valid(slot) || slot.value >= this->module_.values.size()) {
        return sema::INVALID_TYPE_HANDLE;
    }
    const sema::TypeHandle slot_type = this->module_.values[slot.value].type;
    if (!sema::is_valid(slot_type)
        || (!this->module_.types.is_pointer(slot_type) && !this->module_.types.is_reference(slot_type))) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return this->module_.types.get(slot_type).pointee;
}

const TraitObjectVTableLayout* Lowerer::trait_object_vtable_layout(
    const query::VTableLayoutKey& layout_key) const noexcept
{
    for (const TraitObjectVTableLayout& layout : this->module_.trait_object_vtables) {
        if (layout.layout_key == layout_key) {
            return &layout;
        }
    }
    return nullptr;
}

const TraitObjectVTableLayout* Lowerer::trait_object_vtable_layout_for_object(
    const sema::TypeHandle object_type) const noexcept
{
    if (!sema::is_valid(object_type)) {
        return nullptr;
    }
    for (const TraitObjectVTableLayout& layout : this->module_.trait_object_vtables) {
        if (this->module_.types.same(layout.object_type, object_type)) {
            return &layout;
        }
    }
    return nullptr;
}

const TraitObjectVTableMethodSlot* Lowerer::trait_object_vtable_method_slot(
    const query::VTableLayoutKey& layout_key, const base::u32 slot) const noexcept
{
    const TraitObjectVTableLayout* const layout = this->trait_object_vtable_layout(layout_key);
    if (layout == nullptr) {
        return nullptr;
    }
    for (const TraitObjectVTableMethodSlot& method_slot : layout->method_slots) {
        if (method_slot.slot == slot) {
            return &method_slot;
        }
    }
    return nullptr;
}

const TraitObjectVTableMethodSlot* Lowerer::trait_object_vtable_method_slot_for_object(
    const sema::TypeHandle object_type, const base::u32 slot) const noexcept
{
    const TraitObjectVTableLayout* const layout = this->trait_object_vtable_layout_for_object(object_type);
    if (layout == nullptr) {
        return nullptr;
    }
    for (const TraitObjectVTableMethodSlot& method_slot : layout->method_slots) {
        if (method_slot.slot == slot) {
            return &method_slot;
        }
    }
    return nullptr;
}

const sema::TraitObjectCoercionFact* Lowerer::trait_object_coercion(
    const sema::TypeHandle source_type, const sema::TypeHandle target_type) const noexcept
{
    if (!sema::is_valid(source_type) || !sema::is_valid(target_type)) {
        return nullptr;
    }
    for (const sema::TraitObjectCoercionFact& fact : this->checked_.trait_object_coercions) {
        if (this->module_.types.same(fact.source_reference_type, source_type)
            && this->module_.types.same(fact.target_reference_type, target_type)
            && this->trait_object_vtable_layout(fact.vtable_layout) != nullptr) {
            return &fact;
        }
    }
    return nullptr;
}

const sema::TraitObjectUpcastCoercionFact* Lowerer::trait_object_upcast_coercion(
    const sema::TypeHandle source_type, const sema::TypeHandle target_type) const noexcept
{
    if (!sema::is_valid(source_type) || !sema::is_valid(target_type)) {
        return nullptr;
    }
    for (const sema::TraitObjectUpcastCoercionFact& fact : this->checked_.trait_object_upcast_coercions) {
        if (this->module_.types.same(fact.source_reference_type, source_type)
            && this->module_.types.same(fact.target_reference_type, target_type)
            && this->trait_object_vtable_supertrait_edge(
                   fact.source_vtable_layout, fact.target_vtable_layout, fact.upcast_key,
                   fact.source_reference_type, fact.target_reference_type)
                != nullptr) {
            return &fact;
        }
    }
    return nullptr;
}

const sema::TraitObjectUpcastCoercionFact* Lowerer::dynproject_upcast_coercion(
    const syntax::ExprId argument_expr,
    const sema::TypeHandle target_type) const noexcept
{
    if (!syntax::is_valid(argument_expr) || !sema::is_valid(target_type)) {
        return nullptr;
    }
    for (const sema::TraitObjectUpcastCoercionFact& fact : this->checked_.trait_object_upcast_coercions) {
        if (fact.expr.value == argument_expr.value
            && this->module_.types.same(fact.target_reference_type, target_type)
            && this->trait_object_vtable_supertrait_edge(
                   fact.source_vtable_layout, fact.target_vtable_layout, fact.upcast_key,
                   fact.source_reference_type, fact.target_reference_type)
                != nullptr) {
            return &fact;
        }
    }
    return nullptr;
}

const sema::TraitObjectUpcastCoercionFact* Lowerer::composition_supertrait_upcast_coercion(
    const sema::TypeHandle source_type,
    const sema::TypeHandle target_type) const noexcept
{
    if (!sema::is_valid(source_type) || !sema::is_valid(target_type)
        || !this->module_.types.is_reference(source_type) || !this->module_.types.is_reference(target_type)) {
        return nullptr;
    }
    const sema::TypeInfo& source_ref = this->module_.types.get(source_type);
    const sema::TypeInfo& target_ref = this->module_.types.get(target_type);
    if (!sema::is_valid(source_ref.pointee) || !sema::is_valid(target_ref.pointee)
        || !this->module_.types.is_principal_set_trait_object(source_ref.pointee)
        || !this->module_.types.is_trait_object(target_ref.pointee)
        || this->module_.types.is_principal_set_trait_object(target_ref.pointee)) {
        return nullptr;
    }

    const sema::TypeInfo& source_object = this->module_.types.get(source_ref.pointee);
    for (const sema::TraitObjectUpcastCoercionFact& fact : this->checked_.trait_object_upcast_coercions) {
        if (!this->module_.types.same(fact.target_reference_type, target_type)
            || !this->module_.types.is_reference(fact.source_reference_type)) {
            continue;
        }
        const sema::TypeInfo& projected_ref = this->module_.types.get(fact.source_reference_type);
        if (projected_ref.pointer_mutability != source_ref.pointer_mutability
            || !sema::is_valid(projected_ref.pointee)) {
            continue;
        }
        const bool source_contains_projected_principal =
            std::ranges::any_of(source_object.trait_object_principal_types,
                [&](const sema::TypeHandle principal) {
                    return this->module_.types.same(principal, projected_ref.pointee);
                });
        if (!source_contains_projected_principal) {
            continue;
        }
        if (this->trait_object_vtable_supertrait_edge(
                fact.source_vtable_layout, fact.target_vtable_layout, fact.upcast_key,
                fact.source_reference_type, fact.target_reference_type)
            != nullptr) {
            return &fact;
        }
    }
    return nullptr;
}

const TraitObjectVTableSupertraitEdge* Lowerer::trait_object_vtable_supertrait_edge(
    const query::VTableLayoutKey& source_layout,
    const query::VTableLayoutKey& target_layout,
    const query::TraitObjectUpcastCoercionKey& upcast_key,
    const sema::TypeHandle source_reference_type,
    const sema::TypeHandle target_reference_type) const noexcept
{
    const TraitObjectVTableLayout* const layout = this->trait_object_vtable_layout(source_layout);
    if (layout == nullptr) {
        return nullptr;
    }
    for (const TraitObjectVTableSupertraitEdge& edge : layout->supertrait_edges) {
        if (edge.target_layout == target_layout && edge.upcast_key == upcast_key
            && this->module_.types.same(edge.source_reference_type, source_reference_type)
            && this->module_.types.same(edge.target_reference_type, target_reference_type)) {
            return &edge;
        }
    }
    return nullptr;
}

const PrincipalSetMetadataLayout* Lowerer::principal_set_metadata_layout(
    const query::StableFingerprint128 identity, const sema::TypeHandle source_type) const noexcept
{
    if (identity.byte_count == 0 || !sema::is_valid(source_type)) {
        return nullptr;
    }
    for (const PrincipalSetMetadataLayout& layout : this->module_.principal_set_metadata_layouts) {
        if (layout.principal_set_identity == identity && this->module_.types.same(layout.concrete_type, source_type)) {
            return &layout;
        }
    }
    return nullptr;
}

const PrincipalSetMetadataWitness* Lowerer::principal_set_metadata_witness(
    const PrincipalSetMetadataLayout& layout, const sema::TypeHandle target_object_type) const noexcept
{
    if (!sema::is_valid(target_object_type)) {
        return nullptr;
    }
    for (const PrincipalSetMetadataWitness& witness : layout.witnesses) {
        if (this->module_.types.same(witness.object_type, target_object_type)) {
            return &witness;
        }
    }
    return nullptr;
}

sema::TypeHandle Lowerer::vtable_pointer_type() noexcept
{
    return this->module_.types.pointer(
        sema::PointerMutability::const_, this->module_.types.builtin(sema::BuiltinType::u8));
}

sema::TypeHandle Lowerer::erased_trait_object_receiver_type(const sema::TraitMethodCallBinding& binding) noexcept
{
    if (sema::is_valid(binding.receiver_type) && this->module_.types.is_reference(binding.receiver_type)) {
        return binding.receiver_type;
    }
    if (sema::is_valid(binding.self_type)) {
        return this->module_.types.reference(sema::PointerMutability::const_, binding.self_type);
    }
    return sema::INVALID_TYPE_HANDLE;
}

sema::TypeHandle Lowerer::dispatch_trait_object_receiver_type(
    const sema::TraitMethodCallBinding& binding) noexcept
{
    const sema::TypeHandle receiver = this->erased_trait_object_receiver_type(binding);
    if (!sema::is_valid(receiver) || !this->module_.types.is_reference(receiver)
        || !sema::is_valid(binding.dispatch_receiver_type)) {
        return receiver;
    }
    const sema::TypeInfo& receiver_info = this->module_.types.get(receiver);
    if (receiver_info.pointee.value == binding.dispatch_receiver_type.value) {
        return receiver;
    }
    return this->module_.types.reference(receiver_info.pointer_mutability, binding.dispatch_receiver_type);
}

ValueId Lowerer::coerce_value(const ValueId value_id, const sema::TypeHandle target_type)
{
    if (!is_valid(value_id) || value_id.value >= this->module_.values.size()) {
        return value_id;
    }
    const sema::TypeHandle source_type = this->module_.values[value_id.value].type;
    if (!sema::is_valid(source_type) && this->module_.values[value_id.value].kind == ValueKind::null_literal
        && sema::is_valid(target_type) && this->module_.types.is_pointer(target_type)) {
        this->module_.values[value_id.value].type = target_type;
        return value_id;
    }
    if (!sema::is_valid(target_type) || !sema::is_valid(source_type)
        || this->module_.types.same(source_type, target_type)) {
        return value_id;
    }
    if (this->module_.types.is_reference(source_type) && this->module_.types.is_reference(target_type)) {
        const sema::TypeInfo& source_ref = this->module_.types.get(source_type);
        const sema::TypeInfo& target_ref = this->module_.types.get(target_type);
        if (sema::is_valid(source_ref.pointee) && sema::is_valid(target_ref.pointee)
            && this->module_.types.is_principal_set_trait_object(source_ref.pointee)
            && this->module_.types.is_trait_object(target_ref.pointee)
            && !this->module_.types.is_principal_set_trait_object(target_ref.pointee)) {
            const sema::TypeInfo& source_object = this->module_.types.get(source_ref.pointee);
            const sema::TypeInfo& target_object = this->module_.types.get(target_ref.pointee);
            for (base::usize principal_index = 0; principal_index < source_object.trait_object_principal_types.size();
                 ++principal_index) {
                if (!this->module_.types.same(
                        source_object.trait_object_principal_types[principal_index], target_ref.pointee)) {
                    continue;
                }
                Value value = this->module_.make_value();
                value.kind = ValueKind::trait_object_composition_project;
                value.type = target_type;
                value.object = value_id;
                value.principal_set_identity = source_object.trait_object_principal_set_identity;
                value.principal_object = target_object.trait_object_key;
                value.principal_index = base::checked_u32(principal_index, "ir principal-set projection index");

                const Value& source_value = this->module_.values[value_id.value];
                if (source_value.kind == ValueKind::trait_object_composition_pack
                    && is_valid(source_value.lhs) && source_value.lhs.value < this->module_.values.size()) {
                    const sema::TypeHandle lhs_type = this->module_.values[source_value.lhs.value].type;
                    if (this->module_.types.is_pointer(lhs_type) || this->module_.types.is_reference(lhs_type)) {
                        const sema::TypeHandle concrete_type = this->module_.types.get(lhs_type).pointee;
                        const PrincipalSetMetadataLayout* const layout =
                            this->principal_set_metadata_layout(
                                source_object.trait_object_principal_set_identity, concrete_type);
                        if (layout != nullptr) {
                            if (const PrincipalSetMetadataWitness* const witness =
                                    this->principal_set_metadata_witness(*layout, target_ref.pointee);
                                witness != nullptr) {
                                value.target_vtable_layout = witness->vtable_layout;
                            }
                        }
                    }
                }
                return this->append_value(value);
            }
            if (const sema::TraitObjectUpcastCoercionFact* const upcast =
                    this->composition_supertrait_upcast_coercion(source_type, target_type);
                upcast != nullptr) {
                const ValueId projected_principal =
                    this->coerce_value(value_id, upcast->source_reference_type);
                return this->coerce_value(projected_principal, target_type);
            }
        }
    }
    if ((this->module_.types.is_pointer(source_type) || this->module_.types.is_reference(source_type))
        && this->module_.types.is_reference(target_type)) {
        const sema::TypeInfo& source_ref = this->module_.types.get(source_type);
        const sema::TypeInfo& target_ref = this->module_.types.get(target_type);
        if (sema::is_valid(source_ref.pointee) && sema::is_valid(target_ref.pointee)
            && !this->module_.types.is_trait_object(source_ref.pointee)
            && this->module_.types.is_principal_set_trait_object(target_ref.pointee)) {
            const sema::TypeInfo& target_object = this->module_.types.get(target_ref.pointee);
            const PrincipalSetMetadataLayout* const layout =
                this->principal_set_metadata_layout(target_object.trait_object_principal_set_identity,
                    source_ref.pointee);
            if (layout != nullptr) {
                Value value = this->module_.make_value();
                value.kind = ValueKind::trait_object_composition_pack;
                value.type = target_type;
                value.lhs = value_id;
                value.principal_set_identity = target_object.trait_object_principal_set_identity;
                return this->append_value(value);
            }
        }
    }
    if (const sema::TraitObjectUpcastCoercionFact* const upcast =
            this->trait_object_upcast_coercion(source_type, target_type);
        upcast != nullptr) {
        const TraitObjectVTableSupertraitEdge* const edge = this->trait_object_vtable_supertrait_edge(
            upcast->source_vtable_layout, upcast->target_vtable_layout, upcast->upcast_key,
            upcast->source_reference_type, upcast->target_reference_type);
        if (edge != nullptr) {
            Value value = this->module_.make_value();
            value.kind = ValueKind::trait_object_upcast;
            value.type = target_type;
            value.object = value_id;
            value.vtable_layout = upcast->source_vtable_layout;
            value.target_vtable_layout = upcast->target_vtable_layout;
            value.upcast_key = upcast->upcast_key;
            value.vtable_supertrait_edge = edge->edge_index;
            return this->append_value(value);
        }
    }
    if (const sema::TraitObjectCoercionFact* const coercion =
            this->trait_object_coercion(source_type, target_type);
        coercion != nullptr) {
        Value value = this->module_.make_value();
        value.kind = ValueKind::trait_object_pack;
        value.type = target_type;
        value.lhs = value_id;
        value.vtable_layout = coercion->vtable_layout;
        return this->append_value(value);
    }
    if ((this->module_.types.is_pointer(source_type) || this->module_.types.is_reference(source_type))
        && this->module_.types.is_reference(target_type)) {
        const sema::TypeInfo& source = this->module_.types.get(source_type);
        const sema::TypeInfo& target = this->module_.types.get(target_type);
        const sema::TypeHandle canonical_target =
            this->module_.types.reference(target.pointer_mutability, target.pointee);
        const auto append_pack = [&](const sema::TraitObjectCoercionFact& coercion) -> ValueId {
            Value value = this->module_.make_value();
            value.kind = ValueKind::trait_object_pack;
            value.type = target_type;
            value.lhs = value_id;
            value.vtable_layout = coercion.vtable_layout;
            return this->append_value(value);
        };
        const sema::TypeHandle semantic_source =
            this->module_.types.reference(target.pointer_mutability, source.pointee);
        if (target.pointer_mutability == sema::PointerMutability::const_
            || source.pointer_mutability == sema::PointerMutability::mut) {
            if (const sema::TraitObjectCoercionFact* const coercion =
                    this->trait_object_coercion(semantic_source, canonical_target);
                coercion != nullptr) {
                return append_pack(*coercion);
            }
        }
        const sema::TypeHandle structural_source =
            this->module_.types.reference(source.pointer_mutability, source.pointee);
        if (const sema::TraitObjectCoercionFact* const coercion =
                this->trait_object_coercion(structural_source, canonical_target);
            coercion != nullptr) {
            return append_pack(*coercion);
        }
    }
    if (this->module_.types.is_slice(source_type) && this->module_.types.is_slice(target_type)) {
        const sema::TypeInfo& source = this->module_.types.get(source_type);
        const sema::TypeInfo& target = this->module_.types.get(target_type);
        if (target.slice_mutability == sema::PointerMutability::const_
            && source.slice_mutability == sema::PointerMutability::mut
            && this->module_.types.same(source.slice_element, target.slice_element)) {
            Value value = this->module_.make_value();
            value.kind = ValueKind::slice;
            value.type = target_type;
            value.lhs = this->append_slice_data(value_id, sema::PointerMutability::const_, target.slice_element);
            value.rhs = this->append_slice_len(value_id);
            return this->append_value(value);
        }
    }
    if (this->module_.types.is_reference(source_type) && this->module_.types.is_reference(target_type)) {
        const sema::TypeInfo& source = this->module_.types.get(source_type);
        const sema::TypeInfo& target = this->module_.types.get(target_type);
        if (target.pointer_mutability == sema::PointerMutability::const_
            && source.pointer_mutability == sema::PointerMutability::mut
            && this->module_.types.same(source.pointee, target.pointee)) {
            Value value = this->module_.make_value();
            value.kind = ValueKind::cast;
            value.type = target_type;
            value.target_type = target_type;
            value.lhs = value_id;
            value.cast_kind = CastKind::pointer;
            return this->append_value(value);
        }
    }
    if (this->module_.types.is_pointer(source_type) && this->module_.types.is_reference(target_type)) {
        const sema::TypeInfo& source = this->module_.types.get(source_type);
        const sema::TypeInfo& target = this->module_.types.get(target_type);
        if (this->module_.types.same(source.pointee, target.pointee)
            && (target.pointer_mutability == sema::PointerMutability::const_
                || source.pointer_mutability == sema::PointerMutability::mut)) {
            Value value = this->module_.make_value();
            value.kind = ValueKind::cast;
            value.type = target_type;
            value.target_type = target_type;
            value.lhs = value_id;
            value.cast_kind = CastKind::pointer;
            return this->append_value(value);
        }
    }
    const bool source_numeric = this->module_.types.is_integer(source_type)
        || this->module_.types.is_float(source_type)
        || this->module_.types.is_bool(source_type);
    const bool target_numeric = this->module_.types.is_integer(target_type)
        || this->module_.types.is_float(target_type)
        || this->module_.types.is_bool(target_type);
    if (source_numeric && target_numeric) {
        Value value = this->module_.make_value();
        value.kind = ValueKind::cast;
        value.type = target_type;
        value.target_type = target_type;
        value.lhs = value_id;
        value.cast_kind = CastKind::numeric;
        return this->append_value(value);
    }
    if (is_local_slot_type(source_type) && this->module_.types.is_pointer(target_type)) {
        Value value = this->module_.make_value();
        value.kind = ValueKind::cast;
        value.type = target_type;
        value.target_type = target_type;
        value.lhs = value_id;
        value.cast_kind = CastKind::pointer;
        return this->append_value(value);
    }
    return value_id;
}

ValueId Lowerer::append_value(const Value& value)
{
    const ValueId id = add_value(this->module_, value);
    if (this->current_function_ != nullptr && is_valid(this->current_block_)) {
        this->current_function_->blocks[this->current_block_.value].values.push_back(id);
    }
    return id;
}

void Lowerer::append_store(const ValueId target, const ValueId source)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::store;
    value.type = this->module_.types.builtin(sema::BuiltinType::void_);
    value.object = target;
    value.lhs = this->coerce_value(source, this->local_load_type(target));
    static_cast<void>(this->append_value(value));
}

void Lowerer::append_branch_if_open(const BlockId target)
{
    if (this->has_terminator(this->current_block_)) {
        return;
    }
    Terminator term;
    term.kind = TerminatorKind::branch;
    term.target = target;
    this->set_terminator(this->current_block_, term);
}

bool Lowerer::has_terminator(const BlockId block) const
{
    if (this->current_function_ == nullptr || !is_valid(block)
        || block.value >= this->current_function_->blocks.size()) {
        return true;
    }
    return this->current_function_->blocks[block.value].terminator.kind != TerminatorKind::none;
}

void Lowerer::set_terminator(const BlockId block, const Terminator& terminator) const
{
    if (this->current_function_ == nullptr || !is_valid(block)
        || block.value >= this->current_function_->blocks.size()) {
        return;
    }
    this->current_function_->blocks[block.value].terminator = terminator;
}

} // namespace aurex::ir::detail
