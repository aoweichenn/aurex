#include <aurex/midend/ir/enum_layout.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <midend/ir/lowering/private/lower_ast_internal.hpp>

namespace aurex::ir::detail {

namespace {

constexpr std::string_view IR_FOR_RANGE_ZERO_LITERAL = "0";
constexpr std::string_view IR_FOR_RANGE_ONE_LITERAL = "1";
constexpr std::string_view IR_FOR_RANGE_VALUE_SLOT_NAME = "for.range.value";
constexpr std::string_view IR_FOR_ITERABLE_COND_BLOCK_PREFIX = "for.iterable.cond";
constexpr std::string_view IR_FOR_ITERABLE_BODY_BLOCK_PREFIX = "for.iterable.body";
constexpr std::string_view IR_FOR_ITERABLE_UPDATE_BLOCK_PREFIX = "for.iterable.update";
constexpr std::string_view IR_FOR_ITERABLE_EXIT_BLOCK_PREFIX = "for.iterable.exit";
constexpr std::string_view IR_FOR_ITERABLE_CURSOR_SLOT_NAME = "for.iterable.cursor";
constexpr std::string_view IR_FOR_ITERABLE_END_SLOT_NAME = "for.iterable.end";
constexpr std::string_view IR_FOR_PROTOCOL_ITERATOR_SLOT_NAME = "for.iterator";
constexpr std::string_view IR_FOR_PROTOCOL_SOURCE_SLOT_NAME = "for.iterable.source";
constexpr std::string_view IR_FOR_PROTOCOL_COND_BLOCK_PREFIX = "for.protocol.cond";
constexpr std::string_view IR_FOR_PROTOCOL_BODY_BLOCK_PREFIX = "for.protocol.body";
constexpr std::string_view IR_FOR_PROTOCOL_UPDATE_BLOCK_PREFIX = "for.protocol.update";
constexpr std::string_view IR_FOR_PROTOCOL_EXIT_BLOCK_PREFIX = "for.protocol.exit";
constexpr std::string_view IR_DROP_FLAG_VALUE_NAME = "drop.flag";
constexpr std::string_view IR_DROP_THEN_BLOCK_PREFIX = "drop.then";
constexpr std::string_view IR_DROP_JOIN_BLOCK_PREFIX = "drop.join";
constexpr std::string_view IR_DROP_ARRAY_INDEX_NAME = "drop.index";
constexpr std::string_view IR_DROP_ENUM_TAG_NAME = "drop.tag";
constexpr std::string_view IR_DROP_ENUM_PAYLOAD_PREFIX = "drop.payload.";
constexpr char IR_DROP_TUPLE_FIELD_PREFIX[] = "";
constexpr int IR_LOCAL_PLACE_TUPLE_FIELD_DECIMAL_BASE = 10;
constexpr std::string_view IR_LOCAL_PLACE_TUPLE_FIELD_INDEX_CONTEXT = "ir local place tuple field index";

[[nodiscard]] bool lambda_capture_kind_is_reference(const syntax::LambdaCaptureKind kind) noexcept
{
    return kind == syntax::LambdaCaptureKind::shared_reference
        || kind == syntax::LambdaCaptureKind::mutable_reference;
}

[[nodiscard]] bool lambda_capture_kind_is_mutable(const syntax::LambdaCaptureKind kind) noexcept
{
    return kind == syntax::LambdaCaptureKind::mutable_reference;
}

struct DropGlueFrame {
    ValueId address = INVALID_VALUE_ID;
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    IrTextId name = INVALID_IR_TEXT_ID;
    bool emit_self = true;
};

[[nodiscard]] BinaryOp map_compound_assignment(const syntax::AssignOp op) noexcept
{
    switch (op) {
        case syntax::AssignOp::add:
            return BinaryOp::add;
        case syntax::AssignOp::sub:
            return BinaryOp::sub;
        case syntax::AssignOp::mul:
            return BinaryOp::mul;
        case syntax::AssignOp::div:
            return BinaryOp::div;
        case syntax::AssignOp::mod:
            return BinaryOp::mod;
        case syntax::AssignOp::shl:
            return BinaryOp::shl;
        case syntax::AssignOp::shr:
            return BinaryOp::shr;
        case syntax::AssignOp::bit_and:
            return BinaryOp::bit_and;
        case syntax::AssignOp::bit_xor:
            return BinaryOp::bit_xor;
        case syntax::AssignOp::bit_or:
            return BinaryOp::bit_or;
        case syntax::AssignOp::assign:
            return BinaryOp::add;
    }
    return BinaryOp::add;
}

[[nodiscard]] bool is_deinit_self_param(const syntax::ParamDecl& param, const base::usize index) noexcept
{
    return index == 0U && param.is_deinit && param.name == "self";
}

[[nodiscard]] std::optional<base::u32> parse_local_place_tuple_field_index(const std::string_view field_name) noexcept
{
    if (field_name.empty()) {
        return std::nullopt;
    }
    base::u32 value = 0;
    const char* const begin = field_name.data();
    const char* const end = begin + field_name.size();
    const auto result = std::from_chars(begin, end, value, IR_LOCAL_PLACE_TUPLE_FIELD_DECIMAL_BASE);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::string tuple_field_name(const base::usize index)
{
    return std::to_string(index);
}

[[nodiscard]] bool for_in_iteration_plan_is_iterable(const sema::ForInIterationPlan& plan) noexcept
{
    return plan.kind == sema::ForInIterationKind::array_value || plan.kind == sema::ForInIterationKind::slice_value
        || plan.kind == sema::ForInIterationKind::protocol_iterator;
}

[[nodiscard]] bool for_in_iteration_plan_is_counted_range(const sema::ForInIterationPlan& plan) noexcept
{
    return plan.kind == sema::ForInIterationKind::counted_range;
}

[[nodiscard]] bool for_in_iteration_plan_is_range_value(const sema::ForInIterationPlan& plan) noexcept
{
    return plan.kind == sema::ForInIterationKind::range_value;
}

[[nodiscard]] sema::ForInIterationPlan fallback_counted_range_plan(
    const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const sema::TypeHandle range_type) noexcept
{
    sema::ForInIterationPlan plan;
    plan.stmt = stmt_id;
    plan.kind = sema::ForInIterationKind::counted_range;
    plan.start_expr = stmt.range_start;
    plan.end_expr = stmt.range_end;
    plan.step_expr = stmt.range_step;
    plan.item_type = range_type;
    plan.range_type = range_type;
    plan.requires_copy_item = false;
    return plan;
}

[[nodiscard]] sema::ForInIterationPlan fallback_iterable_plan(const syntax::StmtId stmt_id,
    const syntax::StmtNode& stmt, const sema::TypeTable& types, const sema::TypeHandle iterable_type,
    const sema::TypeHandle item_type) noexcept
{
    sema::ForInIterationPlan plan;
    plan.stmt = stmt_id;
    plan.iterable_expr = stmt.range_iterable;
    plan.iterable_type = iterable_type;
    plan.item_type = item_type;
    plan.index_type = types.builtin(sema::BuiltinType::usize);
    if (sema::is_valid(iterable_type) && types.is_array(iterable_type)) {
        plan.kind = sema::ForInIterationKind::array_value;
        return plan;
    }
    if (sema::is_valid(iterable_type) && types.is_slice(iterable_type)) {
        const sema::TypeInfo& slice = types.get(iterable_type);
        plan.kind = sema::ForInIterationKind::slice_value;
        plan.element_access = slice.slice_mutability;
    }
    return plan;
}

[[nodiscard]] std::string cleanup_child_flag_name(
    const std::string_view parent, const std::string_view field_name)
{
    return std::string(parent) + "." + std::string(field_name);
}

} // namespace

void Lowerer::lower_function_body(const FunctionId function_id, const FunctionBodyView body)
{
    if (!is_valid(function_id) || function_id.value >= this->module_.functions.size()) {
        return;
    }
    this->current_function_ = &this->module_.functions[function_id.value];
    this->locals_.clear();
    this->local_scopes_.clear();
    this->loop_contexts_.clear();
    this->cleanup_scopes_.clear();
    this->push_local_scope();
    this->cleanup_scopes_.push_back({});
    this->current_block_ = add_block(this->module_, *this->current_function_, "entry");

    for (base::usize i = 0; i < body.params.size(); ++i) {
        const syntax::ParamDecl& param = body.params[i];
        const sema::TypeHandle param_type =
            this->current_function_ != nullptr && i < this->current_function_->signature_params.size()
            ? this->current_function_->signature_params[i].type
            : this->syntax_type(param.type);
        Value param_value = this->module_.make_value();
        param_value.kind = ValueKind::param;
        param_value.name = this->module_.intern(param.name);
        param_value.type = param_type;
        const ValueId param_id = this->append_value(param_value);
        this->current_function_->param_values.push_back(param_id);

        Value slot = this->module_.make_value();
        slot.kind = ValueKind::alloca;
        slot.name = this->module_.intern(param.name);
        slot.type = this->module_.types.pointer(sema::PointerMutability::mut, param_value.type);
        const ValueId slot_id = this->append_value(slot);
        LocalBinding binding{
            .slot = slot_id,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = param_type,
            .is_mutable = false,
            .field_cleanups = {},
        };
        if (!is_deinit_self_param(param, i)) {
            this->register_local_cleanup(binding, param.name);
        }
        this->bind_local(param.name_id, binding);
        this->append_store(slot_id, param_id);
    }

    this->lower_block(body.body);
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(0);
        Terminator term;
        term.kind = TerminatorKind::return_;
        this->set_terminator(this->current_block_, term);
    }
    this->pop_local_scope();
    this->locals_.clear();
    this->local_scopes_.clear();
    this->cleanup_scopes_.clear();
    this->current_function_ = nullptr;
}

void Lowerer::lower_lambda_body(const FunctionId function_id, const sema::CheckedLambdaInfo& lambda)
{
    if (!is_valid(function_id) || !syntax::is_valid(lambda.expr) || lambda.expr.value >= this->ast_.exprs.size()) {
        return;
    }
    const syntax::LambdaExprPayload* const payload = this->ast_.exprs.lambda_payload(lambda.expr.value);
    if (payload == nullptr) {
        return;
    }
    if (!lambda.captures.empty()) {
        this->lower_capturing_lambda_body(function_id, lambda);
        return;
    }
    this->lower_function_body(function_id, FunctionBodyView{payload->params, lambda.body});
}

void Lowerer::lower_capturing_lambda_body(const FunctionId function_id, const sema::CheckedLambdaInfo& lambda)
{
    if (!is_valid(function_id) || function_id.value >= this->module_.functions.size()
        || !syntax::is_valid(lambda.expr) || lambda.expr.value >= this->ast_.exprs.size()) {
        return;
    }
    const syntax::LambdaExprPayload* const payload = this->ast_.exprs.lambda_payload(lambda.expr.value);
    if (payload == nullptr) {
        return;
    }

    this->current_function_ = &this->module_.functions[function_id.value];
    this->locals_.clear();
    this->local_scopes_.clear();
    this->loop_contexts_.clear();
    this->cleanup_scopes_.clear();
    this->push_local_scope();
    this->cleanup_scopes_.push_back({});
    this->current_block_ = add_block(this->module_, *this->current_function_, "entry");

    ValueId env_param = INVALID_VALUE_ID;
    if (!this->current_function_->signature_params.empty()) {
        const FunctionParam& param = this->current_function_->signature_params.front();
        Value param_value = this->module_.make_value();
        param_value.kind = ValueKind::param;
        param_value.name = param.name;
        param_value.type = param.type;
        env_param = this->append_value(param_value);
        this->current_function_->param_values.push_back(env_param);
    }
    if (!is_valid(env_param)) {
        return;
    }

    for (const sema::CheckedLambdaInfo::Capture& capture : lambda.captures) {
        const IrTextId field_name = this->module_.intern(capture.field_name.view());
        const sema::TypeHandle field_type =
            sema::is_valid(capture.field_type) ? capture.field_type : capture.type;
        const ValueId field_address =
            this->append_field_address(env_param, field_name, field_type, sema::PointerMutability::mut);
        if (lambda_capture_kind_is_reference(capture.kind)) {
            const ValueId captured_slot =
                this->append_load(field_address, field_type, this->module_.intern(capture.name.view()));
            LocalBinding binding{
                .slot = captured_slot,
                .cleanup_flag = INVALID_VALUE_ID,
                .type = capture.type,
                .is_mutable = lambda_capture_kind_is_mutable(capture.kind),
                .field_cleanups = {},
            };
            this->bind_local(capture.name_id, binding);
            continue;
        }
        const ValueId field_value =
            this->append_load(field_address, capture.type, this->module_.intern(capture.name.view()));
        Value slot = this->module_.make_value();
        slot.kind = ValueKind::alloca;
        slot.name = this->module_.intern(capture.name.view());
        slot.type = this->module_.types.pointer(sema::PointerMutability::mut, capture.type);
        const ValueId slot_id = this->append_value(slot);
        LocalBinding binding{
            .slot = slot_id,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = capture.type,
            .is_mutable = false,
            .field_cleanups = {},
        };
        this->register_local_cleanup(binding, capture.name.view());
        this->bind_local(capture.name_id, binding);
        this->append_store(slot_id, field_value);
    }

    for (base::usize i = 0; i < payload->params.size(); ++i) {
        const syntax::ParamDecl& param = payload->params[i];
        const base::usize signature_index = i + 1U;
        const sema::TypeHandle param_type =
            signature_index < this->current_function_->signature_params.size()
            ? this->current_function_->signature_params[signature_index].type
            : (i < lambda.param_types.size() ? lambda.param_types[i] : sema::INVALID_TYPE_HANDLE);
        Value param_value = this->module_.make_value();
        param_value.kind = ValueKind::param;
        param_value.name = this->module_.intern(param.name);
        param_value.type = param_type;
        const ValueId param_id = this->append_value(param_value);
        this->current_function_->param_values.push_back(param_id);

        Value slot = this->module_.make_value();
        slot.kind = ValueKind::alloca;
        slot.name = this->module_.intern(param.name);
        slot.type = this->module_.types.pointer(sema::PointerMutability::mut, param_type);
        const ValueId slot_id = this->append_value(slot);
        LocalBinding binding{
            .slot = slot_id,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = param_type,
            .is_mutable = false,
            .field_cleanups = {},
        };
        this->register_local_cleanup(binding, param.name);
        this->bind_local(param.name_id, binding);
        this->append_store(slot_id, param_id);
    }

    this->lower_block(lambda.body);
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(0);
        Terminator term;
        term.kind = TerminatorKind::return_;
        this->set_terminator(this->current_block_, term);
    }
    this->pop_local_scope();
    this->locals_.clear();
    this->local_scopes_.clear();
    this->cleanup_scopes_.clear();
    this->current_function_ = nullptr;
}

void Lowerer::lower_generic_function_body(
    const FunctionId function_id, const sema::GenericFunctionInstanceBodyView& body)
{
    if (!sema::is_valid(body)) {
        return;
    }
    const ActiveSideTables previous_tables = this->active_side_tables_;
    this->active_side_tables_ = ActiveSideTables{
        body.side_tables,
        &body.side_tables->expr_types,
        &body.side_tables->expr_owned_use_modes,
        &body.side_tables->expr_c_name_ids,
        &body.side_tables->pattern_c_name_ids,
        &body.side_tables->syntax_type_handles,
        &body.side_tables->stmt_local_types,
        &body.side_tables->range_value_plans,
        &body.side_tables->for_in_iteration_plans,
    };
    this->lower_function_body(function_id, FunctionBodyView{body.item->params, body.body});
    this->active_side_tables_ = previous_tables;
}

void Lowerer::lower_trait_default_method_body(
    const FunctionId function_id, const sema::TraitDefaultMethodInstanceBodyView& body)
{
    if (!sema::is_valid(body)) {
        return;
    }
    const ActiveSideTables previous_tables = this->active_side_tables_;
    this->active_side_tables_ = ActiveSideTables{
        body.side_tables,
        &body.side_tables->expr_types,
        &body.side_tables->expr_owned_use_modes,
        &body.side_tables->expr_c_name_ids,
        &body.side_tables->pattern_c_name_ids,
        &body.side_tables->syntax_type_handles,
        &body.side_tables->stmt_local_types,
        &body.side_tables->range_value_plans,
        &body.side_tables->for_in_iteration_plans,
    };
    this->lower_function_body(function_id, FunctionBodyView{body.item->params, body.body});
    this->active_side_tables_ = previous_tables;
}

void Lowerer::lower_block(const syntax::StmtId block_id)
{
    this->push_local_scope();
    const base::usize scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});
    this->lower_block_contents(block_id);
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(scope_depth);
    }
    this->cleanup_scopes_.resize(scope_depth);
    this->pop_local_scope();
}

void Lowerer::lower_block_contents(const syntax::StmtId block_id)
{
    if (!syntax::is_valid(block_id) || block_id.value >= ast_.stmts.size()) {
        return;
    }
    const syntax::AstArenaVector<syntax::StmtId>* const statements = ast_.stmts.block_statements(block_id.value);
    if (statements == nullptr) {
        return;
    }
    for (const syntax::StmtId stmt : *statements) {
        if (has_terminator(current_block_)) {
            break;
        }
        lower_stmt(stmt);
    }
}

void Lowerer::lower_stmt(const syntax::StmtId stmt_id)
{
    if (!syntax::is_valid(stmt_id) || stmt_id.value >= ast_.stmts.size()) {
        return;
    }
    const syntax::StmtNode stmt = ast_.stmts[stmt_id.value];
    switch (stmt.kind) {
        case syntax::StmtKind::let:
        case syntax::StmtKind::var: {
            const sema::TypeHandle local_type = stmt_local_type(stmt_id);
            if (syntax::is_valid(stmt.pattern)) {
                const ValueId init = this->coerce_value(this->lower_expr(stmt.init, local_type), local_type);
                const ValueId source_slot = this->append_temp_alloca("tuple.pattern", local_type);
                this->append_store(source_slot, init);
                if (syntax::is_valid(stmt.else_block)) {
                    const ValueId condition = this->append_pattern_condition(stmt.pattern, source_slot, local_type);
                    const BlockId success_block = add_block(this->module_, *this->current_function_,
                        "let.else.ok" + std::to_string(this->current_function_->blocks.size()));
                    const BlockId failure_block = add_block(this->module_, *this->current_function_,
                        "let.else.fail" + std::to_string(this->current_function_->blocks.size()));
                    const BlockId join_block = add_block(this->module_, *this->current_function_,
                        "let.else.join" + std::to_string(this->current_function_->blocks.size()));
                    Terminator branch;
                    branch.kind = TerminatorKind::cond_branch;
                    branch.condition = condition;
                    branch.then_target = success_block;
                    branch.else_target = failure_block;
                    this->set_terminator(this->current_block_, branch);

                    this->push_local_scope();
                    this->current_block_ = failure_block;
                    this->lower_block(stmt.else_block);
                    this->append_branch_if_open(join_block);

                    this->current_block_ = success_block;
                    this->pop_local_scope();
                    this->lower_local_pattern(
                        stmt.pattern, source_slot, local_type, stmt.kind == syntax::StmtKind::var);
                    this->append_branch_if_open(join_block);
                    this->current_block_ = join_block;
                    break;
                }
                this->lower_local_pattern(stmt.pattern, source_slot, local_type, stmt.kind == syntax::StmtKind::var);
                break;
            }
            Value slot = this->module_.make_value();
            slot.kind = ValueKind::alloca;
            slot.name = this->module_.intern(stmt.name);
            slot.type = module_.types.pointer(sema::PointerMutability::mut, local_type);
            const ValueId slot_id = append_value(slot);
            LocalBinding binding{
                .slot = slot_id,
                .cleanup_flag = INVALID_VALUE_ID,
                .type = local_type,
                .is_mutable = stmt.kind == syntax::StmtKind::var,
                .field_cleanups = {},
            };
            append_store(slot_id, this->coerce_value(this->lower_expr(stmt.init, local_type), local_type));
            this->register_local_cleanup(binding, stmt.name);
            this->bind_local(stmt.name_id, binding);
            break;
        }
        case syntax::StmtKind::assign: {
            const sema::TypeHandle lhs_type = this->expr_type(stmt.lhs);
            const ValueId target = this->lower_place_addr(stmt.lhs);
            ValueId source = this->lower_expr(stmt.rhs, lhs_type);
            if (stmt.assign_op != syntax::AssignOp::assign) {
                const ValueId current = this->append_load(target, lhs_type);
                Value value = this->module_.make_value();
                value.kind = ValueKind::binary;
                value.type = lhs_type;
                value.binary_op = map_compound_assignment(stmt.assign_op);
                value.lhs = current;
                value.rhs = source;
                source = this->append_value(value);
            }
            this->append_place_cleanup_drop_if(stmt.lhs);
            this->append_store(target, source);
            this->mark_place_initialized(stmt.lhs);
            break;
        }
        case syntax::StmtKind::if_:
            this->lower_if(stmt);
            break;
        case syntax::StmtKind::for_:
            this->lower_for(stmt);
            break;
        case syntax::StmtKind::for_range:
            this->lower_for_range(stmt_id, stmt);
            break;
        case syntax::StmtKind::while_:
            this->lower_while(stmt);
            break;
        case syntax::StmtKind::break_: {
            if (!this->loop_contexts_.empty()) {
                this->emit_cleanup_scopes(this->loop_contexts_.back().cleanup_depth);
            }
            Terminator term;
            term.kind = TerminatorKind::branch;
            term.target = this->loop_contexts_.empty() ? INVALID_BLOCK_ID : this->loop_contexts_.back().break_target;
            this->set_terminator(this->current_block_, term);
            break;
        }
        case syntax::StmtKind::continue_: {
            if (!this->loop_contexts_.empty()) {
                this->emit_cleanup_scopes(this->loop_contexts_.back().cleanup_depth);
            }
            Terminator term;
            term.kind = TerminatorKind::branch;
            term.target = this->loop_contexts_.empty() ? INVALID_BLOCK_ID : this->loop_contexts_.back().continue_target;
            this->set_terminator(this->current_block_, term);
            break;
        }
        case syntax::StmtKind::defer:
            if (!this->cleanup_scopes_.empty()) {
                this->cleanup_scopes_.back().push_back(CleanupAction{
                    CleanupActionKind::defer_call,
                    INVALID_VALUE_ID,
                    INVALID_VALUE_ID,
                    sema::INVALID_TYPE_HANDLE,
                    stmt.init,
                    INVALID_IR_TEXT_ID,
                });
            }
            break;
        case syntax::StmtKind::return_: {
            Terminator term;
            term.kind = TerminatorKind::return_;
            if (syntax::is_valid(stmt.return_value)) {
                term.value = coerce_value(
                    lower_expr(stmt.return_value, current_function_->return_type), current_function_->return_type);
            }
            emit_cleanup_scopes(0);
            set_terminator(current_block_, term);
            break;
        }
        case syntax::StmtKind::expr:
            static_cast<void>(lower_expr(stmt.init));
            break;
        case syntax::StmtKind::block:
            lower_block(stmt_id);
            break;
    }
}

void Lowerer::lower_local_pattern(const syntax::PatternId pattern_id, const ValueId source_address,
    const sema::TypeHandle source_type, const bool is_mutable)
{
    this->bind_pattern_locals_with_mutability(pattern_id, source_address, source_type, is_mutable);
}

void Lowerer::lower_if(const syntax::StmtNode& stmt)
{
    ValueId condition = INVALID_VALUE_ID;
    ValueId condition_slot = INVALID_VALUE_ID;
    const sema::TypeHandle condition_type = this->expr_type(stmt.condition);
    if (syntax::is_valid(stmt.pattern)) {
        condition_slot = this->append_temp_alloca("if.pattern", condition_type);
        this->append_store(condition_slot, this->lower_expr(stmt.condition));
        condition = this->append_pattern_condition(stmt.pattern, condition_slot, condition_type);
    } else {
        condition = lower_expr(stmt.condition);
    }
    const BlockId then_block =
        add_block(this->module_, *current_function_, "if.then" + std::to_string(current_function_->blocks.size()));
    const BlockId else_block =
        add_block(this->module_, *current_function_, "if.else" + std::to_string(current_function_->blocks.size()));
    BlockId join_block = INVALID_BLOCK_ID;
    const auto ensure_join_block = [&]() -> BlockId {
        if (!is_valid(join_block)) {
            join_block = add_block(
                this->module_, *current_function_, "if.join" + std::to_string(current_function_->blocks.size()));
        }
        return join_block;
    };

    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = then_block;
    cond.else_target = else_block;
    set_terminator(current_block_, cond);

    current_block_ = then_block;
    this->push_local_scope();
    if (syntax::is_valid(stmt.pattern)) {
        this->bind_pattern_locals(stmt.pattern, condition_slot, condition_type);
    }
    lower_block(stmt.then_block);
    this->pop_local_scope();
    const bool then_open = !has_terminator(current_block_);
    if (then_open) {
        append_branch_if_open(ensure_join_block());
    }

    current_block_ = else_block;
    if (syntax::is_valid(stmt.else_block)) {
        lower_block(stmt.else_block);
    }
    if (syntax::is_valid(stmt.else_if)) {
        lower_stmt(stmt.else_if);
    }
    const bool else_open = !has_terminator(current_block_);
    if (else_open) {
        append_branch_if_open(ensure_join_block());
    }

    current_block_ = is_valid(join_block) ? join_block : INVALID_BLOCK_ID;
}

void Lowerer::lower_while(const syntax::StmtNode& stmt)
{
    const BlockId condition_block =
        add_block(this->module_, *current_function_, "while.cond" + std::to_string(current_function_->blocks.size()));

    append_branch_if_open(condition_block);
    current_block_ = condition_block;
    ValueId condition = INVALID_VALUE_ID;
    ValueId condition_slot = INVALID_VALUE_ID;
    const sema::TypeHandle condition_type = this->expr_type(stmt.condition);
    if (syntax::is_valid(stmt.pattern)) {
        condition_slot = this->append_temp_alloca("while.pattern", condition_type);
        this->append_store(condition_slot, this->lower_expr(stmt.condition));
        condition = this->append_pattern_condition(stmt.pattern, condition_slot, condition_type);
    } else {
        condition = lower_expr(stmt.condition);
    }
    const BlockId body_block =
        add_block(this->module_, *current_function_, "while.body" + std::to_string(current_function_->blocks.size()));
    const BlockId exit_block =
        add_block(this->module_, *current_function_, "while.exit" + std::to_string(current_function_->blocks.size()));
    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = body_block;
    cond.else_target = exit_block;
    set_terminator(current_block_, cond);

    loop_contexts_.push_back(LoopContext{exit_block, condition_block, cleanup_scopes_.size()});
    current_block_ = body_block;
    this->push_local_scope();
    if (syntax::is_valid(stmt.pattern)) {
        this->bind_pattern_locals(stmt.pattern, condition_slot, condition_type);
    }
    lower_block(stmt.body);
    this->pop_local_scope();
    append_branch_if_open(condition_block);
    loop_contexts_.pop_back();

    current_block_ = exit_block;
}

void Lowerer::lower_for(const syntax::StmtNode& stmt)
{
    this->push_local_scope();
    const base::usize scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});

    if (syntax::is_valid(stmt.for_init)) {
        lower_stmt(stmt.for_init);
    }

    const BlockId condition_block =
        add_block(this->module_, *current_function_, "for.cond" + std::to_string(current_function_->blocks.size()));
    const BlockId body_block =
        add_block(this->module_, *current_function_, "for.body" + std::to_string(current_function_->blocks.size()));
    const BlockId update_block =
        add_block(this->module_, *current_function_, "for.update" + std::to_string(current_function_->blocks.size()));
    const BlockId exit_block =
        add_block(this->module_, *current_function_, "for.exit" + std::to_string(current_function_->blocks.size()));

    append_branch_if_open(condition_block);
    current_block_ = condition_block;
    if (syntax::is_valid(stmt.condition)) {
        const ValueId condition = lower_expr(stmt.condition);
        Terminator cond;
        cond.kind = TerminatorKind::cond_branch;
        cond.condition = condition;
        cond.then_target = body_block;
        cond.else_target = exit_block;
        set_terminator(current_block_, cond);
    } else {
        append_branch_if_open(body_block);
    }

    loop_contexts_.push_back(LoopContext{exit_block, update_block, cleanup_scopes_.size()});
    current_block_ = body_block;
    lower_block(stmt.body);
    append_branch_if_open(update_block);

    current_block_ = update_block;
    if (syntax::is_valid(stmt.for_update)) {
        lower_stmt(stmt.for_update);
    }
    append_branch_if_open(condition_block);
    loop_contexts_.pop_back();

    this->current_block_ = exit_block;
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(scope_depth);
    }
    this->cleanup_scopes_.resize(scope_depth);
    this->pop_local_scope();
}

ValueId Lowerer::append_for_range_condition(
    const ValueId cursor_slot, const ValueId end_slot, const ValueId step_slot, const sema::TypeHandle range_type)
{
    const sema::TypeHandle bool_type = this->module_.types.builtin(sema::BuiltinType::bool_);
    const ValueId cursor_condition =
        this->append_load(cursor_slot, range_type, this->module_.intern("for.range.cursor"));
    const ValueId end_condition = this->append_load(end_slot, range_type, this->module_.intern("for.range.end"));
    if (!is_valid(step_slot)) {
        return this->append_binary_value(BinaryOp::less, bool_type, cursor_condition, end_condition);
    }

    const ValueId step_condition = this->append_load(step_slot, range_type, this->module_.intern("for.range.step"));
    const ValueId zero = this->append_integer_literal(IR_FOR_RANGE_ZERO_LITERAL, range_type);
    const ValueId positive_step = this->append_binary_value(BinaryOp::greater, bool_type, step_condition, zero);
    const ValueId negative_step = this->append_binary_value(BinaryOp::less, bool_type, step_condition, zero);
    const ValueId forward_bound = this->append_binary_value(BinaryOp::less, bool_type, cursor_condition, end_condition);
    const ValueId backward_bound =
        this->append_binary_value(BinaryOp::greater, bool_type, cursor_condition, end_condition);
    const ValueId forward_active =
        this->append_binary_value(BinaryOp::logical_and, bool_type, positive_step, forward_bound);
    const ValueId backward_active =
        this->append_binary_value(BinaryOp::logical_and, bool_type, negative_step, backward_bound);
    return this->append_binary_value(BinaryOp::logical_or, bool_type, forward_active, backward_active);
}

std::optional<ForIterableSource> Lowerer::lower_for_iterable_source(const sema::ForInIterationPlan& plan)
{
    if (!syntax::is_valid(plan.iterable_expr) || !sema::is_valid(plan.iterable_type)
        || !sema::is_valid(plan.item_type)) {
        return std::nullopt;
    }

    const sema::TypeHandle usize_type = this->module_.types.builtin(sema::BuiltinType::usize);
    if (plan.kind == sema::ForInIterationKind::array_value && this->module_.types.is_array(plan.iterable_type)) {
        const sema::TypeInfo& array = this->module_.types.get(plan.iterable_type);
        const PlaceAddress iterable = this->lower_object_place_or_value(plan.iterable_expr);
        if (!is_valid(iterable.address)) {
            return std::nullopt;
        }
        return ForIterableSource{
            .base_address = iterable.address,
            .length = this->append_integer_literal(std::to_string(array.array_count), usize_type),
            .element_type = plan.item_type,
            .mutability = iterable.is_mutable ? sema::PointerMutability::mut : sema::PointerMutability::const_,
        };
    }

    if (plan.kind == sema::ForInIterationKind::slice_value && this->module_.types.is_slice(plan.iterable_type)) {
        const sema::TypeInfo& slice = this->module_.types.get(plan.iterable_type);
        const ValueId slice_value = this->lower_expr(plan.iterable_expr, plan.iterable_type);
        if (!is_valid(slice_value)) {
            return std::nullopt;
        }
        return ForIterableSource{
            .base_address = this->append_slice_data(slice_value, slice.slice_mutability, slice.slice_element),
            .length = this->append_slice_len(slice_value),
            .element_type = plan.item_type,
            .mutability = plan.element_access,
        };
    }

    return std::nullopt;
}

ValueId Lowerer::append_for_iterable_element_address(
    const ForIterableSource& source, const ValueId index, const IrTextId name)
{
    Value value = this->module_.make_value();
    value.kind = ValueKind::index_addr;
    value.type = this->module_.types.pointer(source.mutability, source.element_type);
    value.name = name;
    value.object = source.base_address;
    value.index = index;
    return this->append_value(value);
}

CallTarget Lowerer::call_target(const sema::FunctionLookupKey& function)
{
    if (!sema::is_valid(function)) {
        return {};
    }
    const auto signature = this->checked_.functions.find(function);
    if (signature == this->checked_.functions.end() || signature->second.c_name.empty()) {
        return {};
    }
    const IrTextId symbol = this->module_.intern(signature->second.c_name.view());
    const auto found = this->function_symbols_.find(symbol);
    if (found == this->function_symbols_.end()) {
        return CallTarget{INVALID_FUNCTION_ID, symbol};
    }
    return CallTarget{found->second, symbol};
}

ValueId Lowerer::append_for_protocol_receiver(
    const sema::ForInProtocolCallPlan& call, const ValueId receiver_slot, const sema::TypeHandle receiver_storage_type)
{
    if (!is_valid(receiver_slot) || !sema::is_valid(receiver_storage_type) || !sema::is_valid(call.param_type)) {
        return INVALID_VALUE_ID;
    }
    if (this->module_.types.is_pointer(call.param_type) || this->module_.types.is_reference(call.param_type)) {
        if (this->module_.types.is_pointer(receiver_storage_type)
            || this->module_.types.is_reference(receiver_storage_type)) {
            return this->coerce_value(
                this->append_load(receiver_slot, receiver_storage_type, this->module_.intern(call.method_name.view())),
                call.param_type);
        }
        return this->coerce_value(receiver_slot, call.param_type);
    }
    return this->coerce_value(
        this->append_load(receiver_slot, receiver_storage_type, this->module_.intern(call.method_name.view())),
        call.param_type);
}

ValueId Lowerer::append_for_protocol_call(
    const sema::ForInProtocolCallPlan& call, const ValueId receiver_slot, const sema::TypeHandle receiver_storage_type)
{
    const CallTarget target = this->call_target(call.function_key);
    const sema::TypeHandle return_type = sema::is_valid(call.return_type)
        ? call.return_type
        : this->module_.types.builtin(sema::BuiltinType::void_);
    Value value = this->module_.make_value();
    value.kind = ValueKind::call;
    value.type = return_type;
    value.name = target.symbol;
    value.call_target = target.function;
    const sema::TypeHandle param_type =
        is_valid(target.function) ? this->call_param_type(target.function, 0U) : call.param_type;
    const ValueId receiver = this->append_for_protocol_receiver(call, receiver_slot, receiver_storage_type);
    if (is_valid(receiver)) {
        value.args.push_back(this->coerce_value(receiver, param_type));
    }
    return this->append_value(value);
}

sema::ResourceSemanticsSummary Lowerer::resource_summary(const sema::TypeHandle type)
{
    if (!sema::is_valid(type)) {
        return this->resources_.classify(type);
    }
    const auto found = this->resource_cache_.find(type.value);
    if (found != this->resource_cache_.end()) {
        return found->second;
    }
    const sema::ResourceSemanticsSummary summary = this->resources_.classify(type);
    this->resource_cache_.emplace(type.value, summary);
    return summary;
}

bool Lowerer::cleanup_required(const sema::TypeHandle type)
{
    return sema::is_valid(type) && !this->module_.types.is_void(type)
        && sema::resource_needs_drop(this->resource_summary(type));
}

const LocalBinding* Lowerer::local_binding_for_name_expr(const syntax::ExprId expr_id) const noexcept
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ast_.exprs.size()
        || this->ast_.exprs.kind(expr_id.value) != syntax::ExprKind::name) {
        return nullptr;
    }
    const syntax::NameExprPayload* const name = this->ast_.exprs.name_payload(expr_id.value);
    if (name == nullptr || !name->scope_name.empty()) {
        return nullptr;
    }
    const auto found = this->locals_.find(name->text_id);
    return found == this->locals_.end() ? nullptr : &found->second;
}

std::optional<LocalPlacePath> Lowerer::local_place_path(const syntax::ExprId expr_id) const
{
    if (!syntax::is_valid(expr_id)) {
        return std::nullopt;
    }
    syntax::ExprId current = expr_id;
    std::vector<LocalPlaceProjection> reversed;
    while (syntax::is_valid(current) && current.value < this->ast_.exprs.size()) {
        const syntax::ExprKind kind = this->ast_.exprs.kind(current.value);
        if (kind == syntax::ExprKind::name) {
            const syntax::NameExprPayload* const name = this->ast_.exprs.name_payload(current.value);
            if (name == nullptr || !name->scope_name.empty()) {
                return std::nullopt;
            }
            std::ranges::reverse(reversed);
            return LocalPlacePath{name->text_id, std::move(reversed)};
        }
        if (kind == syntax::ExprKind::field) {
            const syntax::FieldExprPayload* const field = this->ast_.exprs.field_payload(current.value);
            if (field == nullptr) {
                return std::nullopt;
            }
            reversed.push_back(this->local_place_projection_for_field_expr(*field));
            current = field->object;
            continue;
        }
        if (kind == syntax::ExprKind::index) {
            const syntax::IndexExprPayload* const index = this->ast_.exprs.index_payload(current.value);
            if (index == nullptr) {
                return std::nullopt;
            }
            reversed.push_back(LocalPlaceProjection{
                .kind = LocalPlaceProjectionKind::index,
                .field_name_id = sema::INVALID_IDENT_ID,
                .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
                .field_name = {},
            });
            current = index->object;
            continue;
        }
        if (kind == syntax::ExprKind::slice) {
            const syntax::SliceExprPayload* const slice = this->ast_.exprs.slice_payload(current.value);
            if (slice == nullptr) {
                return std::nullopt;
            }
            reversed.push_back(LocalPlaceProjection{
                .kind = LocalPlaceProjectionKind::slice,
                .field_name_id = sema::INVALID_IDENT_ID,
                .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
                .field_name = {},
            });
            current = slice->object;
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

LocalPlaceProjection Lowerer::local_place_projection_for_field_expr(const syntax::FieldExprPayload& field) const
{
    const sema::TypeHandle object_type = this->expr_type(field.object);
    if (sema::is_valid(object_type) && object_type.value < this->module_.types.size()
        && this->module_.types.is_tuple(object_type)) {
        const sema::TypeInfo& tuple = this->module_.types.get(object_type);
        const std::optional<base::u32> element_index = parse_local_place_tuple_field_index(field.field_name);
        if (element_index.has_value() && *element_index < tuple.tuple_elements.size()) {
            return LocalPlaceProjection{
                .kind = LocalPlaceProjectionKind::tuple_element,
                .field_name_id = sema::INVALID_IDENT_ID,
                .element_index = *element_index,
                .field_name = field.field_name,
            };
        }
    }
    return LocalPlaceProjection{
        .kind = LocalPlaceProjectionKind::field,
        .field_name_id = field.field_name_id,
        .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
        .field_name = field.field_name,
    };
}

const LocalBinding* Lowerer::local_binding_for_place_path(const LocalPlacePath& path) const noexcept
{
    if (!sema::is_valid(path.root_name_id)) {
        return nullptr;
    }
    const auto found = this->locals_.find(path.root_name_id);
    return found == this->locals_.end() ? nullptr : &found->second;
}

const sema::StructInfo* Lowerer::struct_info_for_type(const sema::TypeHandle type) const noexcept
{
    if (!sema::is_valid(type)) {
        return nullptr;
    }
    for (const auto& entry : this->checked_.structs) {
        const sema::StructInfo& info = entry.second;
        if (this->module_.types.same(info.type, type)) {
            return &info;
        }
    }
    return nullptr;
}

ValueId Lowerer::append_bool_literal(const bool value)
{
    Value literal = this->module_.make_value();
    literal.kind = ValueKind::bool_literal;
    literal.type = this->module_.types.builtin(sema::BuiltinType::bool_);
    literal.text = this->module_.intern(value ? "true" : "false");
    return this->append_value(literal);
}

ValueId Lowerer::append_cleanup_flag(const std::string_view name)
{
    Value flag = this->module_.make_value();
    flag.kind = ValueKind::alloca;
    flag.name = this->module_.intern(std::string("drop.flag.") + std::string(name));
    flag.type = this->module_.types.pointer(
        sema::PointerMutability::mut, this->module_.types.builtin(sema::BuiltinType::bool_));
    return this->append_value(flag);
}

void Lowerer::append_cleanup_flag_store(const ValueId flag, const bool initialized)
{
    if (!is_valid(flag)) {
        return;
    }
    this->append_store(flag, this->append_bool_literal(initialized));
}

const sema::DestructorInfo* Lowerer::custom_destructor_info(const sema::TypeHandle type) const noexcept
{
    if (!sema::is_valid(type)) {
        return nullptr;
    }
    const auto found = this->checked_.destructors.find(type.value);
    return found == this->checked_.destructors.end() ? nullptr : &found->second;
}

CallTarget Lowerer::destructor_call_target(const sema::DestructorInfo& destructor)
{
    const auto signature = this->checked_.functions.find(destructor.function_key);
    if (signature == this->checked_.functions.end() || signature->second.c_name.empty()) {
        return {};
    }
    const IrTextId symbol = this->module_.intern(signature->second.c_name.view());
    const auto function = this->function_symbols_.find(symbol);
    if (function == this->function_symbols_.end()) {
        return CallTarget{INVALID_FUNCTION_ID, symbol};
    }
    return CallTarget{function->second, symbol};
}

bool Lowerer::type_may_emit_runtime_drop(const sema::TypeHandle type, const CleanupDropMode mode)
{
    if (!sema::is_valid(type) || type.value >= this->module_.types.size()) {
        return false;
    }

    std::vector<sema::TypeHandle> pending;
    std::vector<base::u32> visited;
    pending.push_back(type);
    while (!pending.empty()) {
        const sema::TypeHandle current = pending.back();
        pending.pop_back();
        if (!sema::is_valid(current) || current.value >= this->module_.types.size()
            || std::ranges::find(visited, current.value) != visited.end()) {
            continue;
        }
        visited.push_back(current.value);

        if (this->custom_destructor_info(current) != nullptr) {
            return true;
        }
        if (mode == CleanupDropMode::custom_destructor_only) {
            continue;
        }

        const sema::TypeInfo& info = this->module_.types.get(current);
        switch (info.kind) {
            case sema::TypeKind::struct_: {
                const sema::StructInfo* const struct_info = this->struct_info_for_type(current);
                if (struct_info == nullptr) {
                    break;
                }
                for (const sema::StructFieldInfo& field : struct_info->fields) {
                    if (this->cleanup_required(field.type)) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case sema::TypeKind::tuple:
                for (const sema::TypeHandle element : info.tuple_elements) {
                    if (this->cleanup_required(element)) {
                        pending.push_back(element);
                    }
                }
                break;
            case sema::TypeKind::array:
                if (info.array_count != 0U && this->cleanup_required(info.array_element)) {
                    pending.push_back(info.array_element);
                }
                break;
            case sema::TypeKind::range:
                if (this->cleanup_required(info.range_element)) {
                    pending.push_back(info.range_element);
                }
                break;
            case sema::TypeKind::enum_:
                for (const auto& entry : this->checked_.enum_cases) {
                    const sema::EnumCaseInfo& enum_case = entry.second;
                    if (enum_case.type.value == current.value && this->cleanup_required(enum_case.payload_type)) {
                        pending.push_back(enum_case.payload_type);
                    }
                }
                break;
            case sema::TypeKind::builtin:
            case sema::TypeKind::pointer:
            case sema::TypeKind::reference:
            case sema::TypeKind::slice:
            case sema::TypeKind::function:
            case sema::TypeKind::generic_param:
            case sema::TypeKind::associated_projection:
            case sema::TypeKind::opaque_struct:
            case sema::TypeKind::trait_object:
                break;
        }
    }
    return false;
}

CleanupAbiPolicy Lowerer::cleanup_abi_policy(const sema::TypeHandle type, const CleanupDropMode mode) const
{
    if (!sema::is_valid(type) || type.value >= this->module_.types.size()) {
        return CleanupAbiPolicy::unknown_marker_only;
    }
    if (this->custom_destructor_info(type) != nullptr) {
        return CleanupAbiPolicy::static_custom_destructor;
    }
    if (mode == CleanupDropMode::custom_destructor_only) {
        return CleanupAbiPolicy::unknown_marker_only;
    }

    const sema::TypeInfo& info = this->module_.types.get(type);
    switch (info.kind) {
        case sema::TypeKind::generic_param:
            return CleanupAbiPolicy::generic_marker_only;
        case sema::TypeKind::associated_projection:
            return CleanupAbiPolicy::associated_projection_marker_only;
        case sema::TypeKind::opaque_struct:
            return CleanupAbiPolicy::opaque_marker_only;
        case sema::TypeKind::struct_:
        case sema::TypeKind::enum_:
        case sema::TypeKind::tuple:
        case sema::TypeKind::array:
        case sema::TypeKind::range:
            return CleanupAbiPolicy::structural_static;
        case sema::TypeKind::builtin:
        case sema::TypeKind::pointer:
        case sema::TypeKind::reference:
        case sema::TypeKind::slice:
        case sema::TypeKind::function:
        case sema::TypeKind::trait_object:
            return CleanupAbiPolicy::unknown_marker_only;
    }
    return CleanupAbiPolicy::unknown_marker_only;
}

bool Lowerer::append_custom_destructor_call(
    const ValueId slot, const sema::TypeHandle type, const IrTextId name)
{
    const sema::DestructorInfo* const destructor = this->custom_destructor_info(type);
    if (destructor == nullptr) {
        return false;
    }
    const CallTarget target = this->destructor_call_target(*destructor);
    if (!is_valid(target.function) || target.function.value >= this->module_.functions.size()) {
        return false;
    }
    const sema::TypeHandle param_type = this->call_param_type(target.function, 0U);
    const ValueId self_value = this->coerce_value(this->append_load(slot, type, name), param_type);

    Value call = this->module_.make_value();
    call.kind = ValueKind::call;
    call.type = this->module_.types.builtin(sema::BuiltinType::void_);
    call.name = target.symbol;
    call.call_target = target.function;
    call.args.push_back(self_value);
    static_cast<void>(this->append_value(call));
    return true;
}

bool Lowerer::append_runtime_drop_glue(
    const ValueId slot, const sema::TypeHandle type, const IrTextId name, const CleanupDropMode mode)
{
    if (!is_valid(slot) || !sema::is_valid(type) || type.value >= this->module_.types.size()) {
        return false;
    }

    bool emitted = this->append_custom_destructor_call(slot, type, name);
    if (mode == CleanupDropMode::custom_destructor_only) {
        return emitted;
    }

    struct EnumPayloadDrop {
        const sema::EnumCaseInfo* enum_case = nullptr;
        ValueId payload_address = INVALID_VALUE_ID;
    };

    std::vector<DropGlueFrame> pending;
    pending.push_back(DropGlueFrame{slot, type, name, false});
    while (!pending.empty()) {
        const DropGlueFrame frame = pending.back();
        pending.pop_back();
        if (!sema::is_valid(frame.type) || frame.type.value >= this->module_.types.size()) {
            continue;
        }

        if (frame.emit_self) {
            emitted = this->append_custom_destructor_call(frame.address, frame.type, frame.name) || emitted;
        }

        const sema::TypeInfo& info = this->module_.types.get(frame.type);
        switch (info.kind) {
            case sema::TypeKind::struct_: {
                const sema::StructInfo* const struct_info = this->struct_info_for_type(frame.type);
                if (struct_info == nullptr) {
                    break;
                }
                for (const sema::StructFieldInfo& field : struct_info->fields) {
                    if (!this->cleanup_required(field.type)) {
                        continue;
                    }
                    const IrTextId field_name = this->module_.intern(field.name);
                    const ValueId field_address =
                        this->append_field_address(frame.address, field_name, field.type, sema::PointerMutability::mut);
                    pending.push_back(DropGlueFrame{field_address, field.type, field_name, true});
                }
                break;
            }
            case sema::TypeKind::tuple:
                for (base::usize index = 0; index < info.tuple_elements.size(); ++index) {
                    const sema::TypeHandle element_type = info.tuple_elements[index];
                    if (!this->cleanup_required(element_type)) {
                        continue;
                    }
                    const IrTextId element_name =
                        this->module_.intern(std::string(IR_DROP_TUPLE_FIELD_PREFIX) + std::to_string(index));
                    const ValueId element_address =
                        this->append_field_address(frame.address, element_name, element_type, sema::PointerMutability::mut);
                    pending.push_back(DropGlueFrame{element_address, element_type, element_name, true});
                }
                break;
            case sema::TypeKind::array:
                if (info.array_count == 0U || !this->cleanup_required(info.array_element)) {
                    break;
                }
                for (base::u64 element_index = 0; element_index < info.array_count; ++element_index) {
                    const ValueId index_value = this->append_integer_literal(
                        std::to_string(element_index), this->module_.types.builtin(sema::BuiltinType::usize));
                    Value element = this->module_.make_value();
                    element.kind = ValueKind::index_addr;
                    element.type = this->module_.types.pointer(sema::PointerMutability::mut, info.array_element);
                    element.name = this->module_.intern(IR_DROP_ARRAY_INDEX_NAME);
                    element.object = frame.address;
                    element.index = index_value;
                    pending.push_back(
                        DropGlueFrame{this->append_value(element), info.array_element, element.name, true});
                }
                break;
            case sema::TypeKind::range: {
                if (!this->cleanup_required(info.range_element)) {
                    break;
                }
                const std::array<std::string_view, sema::SEMA_RANGE_VALUE_FIELD_COUNT> field_names{
                    sema::SEMA_RANGE_VALUE_START_FIELD,
                    sema::SEMA_RANGE_VALUE_END_FIELD,
                    sema::SEMA_RANGE_VALUE_STEP_FIELD,
                };
                for (const std::string_view field_name_text : field_names) {
                    const IrTextId field_name = this->module_.intern(field_name_text);
                    const ValueId field_address = this->append_field_address(
                        frame.address, field_name, info.range_element, sema::PointerMutability::mut);
                    pending.push_back(DropGlueFrame{field_address, info.range_element, field_name, true});
                }
                break;
            }
            case sema::TypeKind::enum_: {
                if (!is_payload_enum(this->module_.types, frame.type)) {
                    break;
                }
                const sema::TypeHandle tag_type = enum_tag_type(this->module_.types, frame.type);
                const ValueId tag = this->append_load(this->enum_field_addr(
                                                          frame.address, this->module_.intern(IR_ENUM_TAG_FIELD_NAME)),
                    tag_type, this->module_.intern(IR_DROP_ENUM_TAG_NAME));
                const ValueId payload_storage =
                    this->enum_field_addr(frame.address, this->module_.intern(IR_ENUM_PAYLOAD_FIELD_NAME));
                std::vector<EnumPayloadDrop> payload_drops;
                for (const auto& entry : this->checked_.enum_cases) {
                    const sema::EnumCaseInfo& enum_case = entry.second;
                    if (enum_case.type.value != frame.type.value || !this->cleanup_required(enum_case.payload_type)
                        || !this->type_may_emit_runtime_drop(enum_case.payload_type, CleanupDropMode::full)) {
                        continue;
                    }
                    Value cast = this->module_.make_value();
                    cast.kind = ValueKind::cast;
                    cast.type = this->module_.types.pointer(sema::PointerMutability::mut, enum_case.payload_type);
                    cast.target_type = cast.type;
                    cast.lhs = payload_storage;
                    cast.cast_kind = CastKind::pointer;
                    payload_drops.push_back(EnumPayloadDrop{&enum_case, this->append_value(cast)});
                }
                std::ranges::sort(payload_drops, [](const EnumPayloadDrop& lhs, const EnumPayloadDrop& rhs) {
                    return lhs.enum_case->c_name.view() < rhs.enum_case->c_name.view();
                });
                for (const EnumPayloadDrop& payload : payload_drops) {
                    Value equals = this->module_.make_value();
                    equals.kind = ValueKind::binary;
                    equals.type = this->module_.types.builtin(sema::BuiltinType::bool_);
                    equals.binary_op = BinaryOp::equal;
                    equals.lhs = tag;
                    equals.rhs = this->append_enum_tag_literal(payload.enum_case->c_name, tag_type);
                    const ValueId condition = this->append_value(equals);
                    this->append_conditional_runtime_drop(condition, payload.payload_address,
                        payload.enum_case->payload_type,
                        this->module_.intern(
                            std::string(IR_DROP_ENUM_PAYLOAD_PREFIX) + std::string(payload.enum_case->case_name)),
                        CleanupDropMode::full);
                    emitted = true;
                }
                break;
            }
            case sema::TypeKind::builtin:
            case sema::TypeKind::pointer:
            case sema::TypeKind::reference:
            case sema::TypeKind::slice:
            case sema::TypeKind::function:
            case sema::TypeKind::generic_param:
            case sema::TypeKind::associated_projection:
            case sema::TypeKind::opaque_struct:
            case sema::TypeKind::trait_object:
                break;
        }
    }
    return emitted;
}

void Lowerer::append_conditional_runtime_drop(const ValueId condition, const ValueId slot,
    const sema::TypeHandle type, const IrTextId name, const CleanupDropMode mode)
{
    if (!is_valid(condition) || !is_valid(slot) || !this->type_may_emit_runtime_drop(type, mode)
        || this->current_function_ == nullptr || this->has_terminator(this->current_block_)) {
        return;
    }

    const BlockId then_block = add_block(this->module_, *this->current_function_,
        std::string(IR_DROP_THEN_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));
    const BlockId join_block = add_block(this->module_, *this->current_function_,
        std::string(IR_DROP_JOIN_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));

    Terminator branch;
    branch.kind = TerminatorKind::cond_branch;
    branch.condition = condition;
    branch.then_target = then_block;
    branch.else_target = join_block;
    this->set_terminator(this->current_block_, branch);

    this->current_block_ = then_block;
    static_cast<void>(this->append_runtime_drop_glue(slot, type, name, mode));
    this->append_branch_if_open(join_block);
    this->current_block_ = join_block;
}

void Lowerer::append_cleanup_drop(
    const ValueId slot, const sema::TypeHandle type, const IrTextId name, const CleanupDropMode mode)
{
    if (!is_valid(slot) || !sema::is_valid(type)) {
        return;
    }
    Value drop = this->module_.make_value();
    drop.kind = ValueKind::drop;
    drop.type = this->module_.types.builtin(sema::BuiltinType::void_);
    drop.object = slot;
    drop.target_type = type;
    drop.name = name;
    drop.cleanup_policy = this->cleanup_abi_policy(type, mode);
    static_cast<void>(this->append_value(drop));
    static_cast<void>(this->append_runtime_drop_glue(slot, type, name, mode));
}

void Lowerer::append_cleanup_drop_if(
    const ValueId slot, const ValueId flag, const sema::TypeHandle type, const IrTextId name,
    const CleanupDropMode mode)
{
    if (!is_valid(slot) || !is_valid(flag) || !sema::is_valid(type)) {
        return;
    }
    const ValueId initialized = this->append_load(
        flag, this->module_.types.builtin(sema::BuiltinType::bool_), this->module_.intern(IR_DROP_FLAG_VALUE_NAME));
    Value drop = this->module_.make_value();
    drop.kind = ValueKind::drop_if;
    drop.type = this->module_.types.builtin(sema::BuiltinType::void_);
    drop.object = slot;
    drop.lhs = initialized;
    drop.target_type = type;
    drop.name = name;
    drop.cleanup_policy = this->cleanup_abi_policy(type, mode);
    static_cast<void>(this->append_value(drop));
    this->append_conditional_runtime_drop(initialized, slot, type, name, mode);
    this->append_cleanup_flag_store(flag, false);
}

void Lowerer::register_local_cleanup(LocalBinding& binding, const std::string_view name)
{
    if (!this->cleanup_required(binding.type) || this->cleanup_scopes_.empty()) {
        return;
    }
    const bool registered_structural = this->register_structured_local_cleanup(binding, name);
    const bool has_custom_destructor = this->custom_destructor_info(binding.type) != nullptr;
    if (has_custom_destructor) {
        binding.cleanup_flag = this->append_cleanup_flag(name);
        this->append_cleanup_flag_store(binding.cleanup_flag, true);
        this->cleanup_scopes_.back().push_back(CleanupAction{
            CleanupActionKind::drop_local,
            binding.slot,
            binding.cleanup_flag,
            binding.type,
            syntax::INVALID_EXPR_ID,
            this->module_.intern(name),
            registered_structural ? CleanupDropMode::custom_destructor_only : CleanupDropMode::full,
        });
        return;
    }
    if (registered_structural) {
        return;
    }
    binding.cleanup_flag = this->append_cleanup_flag(name);
    this->append_cleanup_flag_store(binding.cleanup_flag, true);
    this->cleanup_scopes_.back().push_back(CleanupAction{
        CleanupActionKind::drop_local,
        binding.slot,
        binding.cleanup_flag,
        binding.type,
        syntax::INVALID_EXPR_ID,
        this->module_.intern(name),
        CleanupDropMode::full,
    });
}

bool Lowerer::register_structured_local_cleanup(LocalBinding& binding, const std::string_view name)
{
    if (!is_valid(binding.slot)) {
        return false;
    }
    const std::vector<CleanupProjection> projections;
    const bool registered =
        this->append_structured_cleanup_bindings(binding, binding.slot, binding.type, projections, name);
    return registered && !binding.field_cleanups.empty();
}

bool Lowerer::append_structured_cleanup_bindings(LocalBinding& binding, const ValueId address,
    const sema::TypeHandle type, const std::vector<CleanupProjection>& projections, const std::string_view name)
{
    if (!is_valid(address) || !sema::is_valid(type)) {
        return false;
    }

    bool expanded_any = false;
    std::vector<StructuredCleanupFrame> pending;
    pending.push_back(StructuredCleanupFrame{
        .address = address,
        .type = type,
        .flag_name = std::string(name),
        .name = INVALID_IR_TEXT_ID,
        .projections = projections,
    });

    while (!pending.empty()) {
        StructuredCleanupFrame frame = std::move(pending.back());
        pending.pop_back();
        if (!is_valid(frame.address) || !sema::is_valid(frame.type) || frame.type.value >= this->module_.types.size()) {
            continue;
        }
        if (this->push_structured_cleanup_children(frame, pending)) {
            expanded_any = true;
            continue;
        }
        if (!frame.projections.empty()) {
            const IrTextId cleanup_name =
                frame.name == INVALID_IR_TEXT_ID ? this->module_.intern(frame.flag_name) : frame.name;
            const ValueId flag = this->append_cleanup_flag(frame.flag_name);
            this->append_cleanup_flag_store(flag, true);
            this->append_cleanup_binding(
                binding, frame.address, flag, frame.type, cleanup_name, frame.projections);
        }
    }
    return expanded_any;
}

bool Lowerer::push_structured_cleanup_children(
    const StructuredCleanupFrame& frame, std::vector<StructuredCleanupFrame>& pending)
{
    if (!sema::is_valid(frame.type) || frame.type.value >= this->module_.types.size()) {
        return false;
    }
    const sema::TypeInfo& info = this->module_.types.get(frame.type);
    if (info.kind == sema::TypeKind::tuple) {
        bool pushed_any = false;
        for (base::usize index = info.tuple_elements.size(); index > 0; --index) {
            const base::usize element_index = index - 1U;
            const sema::TypeHandle element_type = info.tuple_elements[element_index];
            if (!this->cleanup_required(element_type)) {
                continue;
            }
            const std::string field_name_text = tuple_field_name(element_index);
            const IrTextId field_name = this->module_.intern(field_name_text);
            const ValueId field_address =
                this->append_field_address(frame.address, field_name, element_type, sema::PointerMutability::mut);
            if (!is_valid(field_address)) {
                continue;
            }
            StructuredCleanupFrame child{
                .address = field_address,
                .type = element_type,
                .flag_name = cleanup_child_flag_name(frame.flag_name, field_name_text),
                .name = field_name,
                .projections = frame.projections,
            };
            child.projections.push_back(CleanupProjection{
                .kind = LocalPlaceProjectionKind::tuple_element,
                .field_name_id = sema::INVALID_IDENT_ID,
                .element_index = base::checked_u32(element_index, IR_LOCAL_PLACE_TUPLE_FIELD_INDEX_CONTEXT),
                .field_name = field_name,
            });
            pending.push_back(std::move(child));
            pushed_any = true;
        }
        return pushed_any;
    }

    const sema::StructInfo* const struct_info = this->struct_info_for_type(frame.type);
    if (struct_info == nullptr || struct_info->fields.empty()) {
        return false;
    }
    bool pushed_any = false;
    for (base::usize index = struct_info->fields.size(); index > 0; --index) {
        const sema::StructFieldInfo& field = struct_info->fields[index - 1U];
        if (!this->cleanup_required(field.type)) {
            continue;
        }
        const IrTextId field_name = this->module_.intern(field.name);
        const ValueId field_address =
            this->append_field_address(frame.address, field_name, field.type, sema::PointerMutability::mut);
        if (!is_valid(field_address)) {
            continue;
        }
        StructuredCleanupFrame child{
            .address = field_address,
            .type = field.type,
            .flag_name = cleanup_child_flag_name(frame.flag_name, field.name),
            .name = field_name,
            .projections = frame.projections,
        };
        child.projections.push_back(CleanupProjection{
            .kind = LocalPlaceProjectionKind::field,
            .field_name_id = field.name_id,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = field_name,
        });
        pending.push_back(std::move(child));
        pushed_any = true;
    }
    return pushed_any;
}

void Lowerer::append_cleanup_binding(LocalBinding& binding, const ValueId address, const ValueId flag,
    const sema::TypeHandle type, const IrTextId name, const std::vector<CleanupProjection>& projections,
    const CleanupDropMode mode)
{
    binding.field_cleanups.push_back(CleanupBinding{
        .address = address,
        .flag = flag,
        .type = type,
        .name = name,
        .projections = projections,
        .drop_mode = mode,
    });
    this->cleanup_scopes_.back().push_back(CleanupAction{
        .kind = CleanupActionKind::drop_local,
        .slot = address,
        .flag = flag,
        .type = type,
        .defer_expr = syntax::INVALID_EXPR_ID,
        .name = name,
        .drop_mode = mode,
    });
}

void Lowerer::append_root_cleanup_flag_from_fields(const LocalBinding& binding)
{
    if (!is_valid(binding.cleanup_flag) || binding.field_cleanups.empty()) {
        return;
    }
    ValueId all_initialized = INVALID_VALUE_ID;
    const sema::TypeHandle bool_type = this->module_.types.builtin(sema::BuiltinType::bool_);
    for (const CleanupBinding& cleanup : binding.field_cleanups) {
        const ValueId initialized =
            this->append_load(cleanup.flag, bool_type, this->module_.intern(IR_DROP_FLAG_VALUE_NAME));
        all_initialized = is_valid(all_initialized)
            ? this->append_binary_value(BinaryOp::logical_and, bool_type, all_initialized, initialized)
            : initialized;
    }
    if (is_valid(all_initialized)) {
        this->append_store(binding.cleanup_flag, all_initialized);
    }
}

ValueId Lowerer::append_field_address(const ValueId object, const IrTextId field_name,
    const sema::TypeHandle field_type, const sema::PointerMutability mutability)
{
    if (!is_valid(object) || !sema::is_valid(field_type)) {
        return INVALID_VALUE_ID;
    }
    Value value = this->module_.make_value();
    value.kind = ValueKind::field_addr;
    value.type = this->module_.types.pointer(mutability, field_type);
    value.name = field_name;
    value.object = object;
    return this->append_value(value);
}

bool Lowerer::cleanup_projection_matches(
    const CleanupProjection& cleanup, const LocalPlaceProjection& place) const noexcept
{
    if (cleanup.kind != place.kind) {
        return false;
    }
    if (cleanup.kind == LocalPlaceProjectionKind::index || cleanup.kind == LocalPlaceProjectionKind::slice) {
        return true;
    }
    if (cleanup.kind == LocalPlaceProjectionKind::tuple_element) {
        return cleanup.element_index != sema::SEMA_BODY_FLOW_INVALID_INDEX
            && place.element_index != sema::SEMA_BODY_FLOW_INVALID_INDEX
            && cleanup.element_index == place.element_index;
    }
    if (sema::is_valid(cleanup.field_name_id) && sema::is_valid(place.field_name_id)) {
        return cleanup.field_name_id == place.field_name_id;
    }
    return cleanup.field_name != INVALID_IR_TEXT_ID && this->module_.text(cleanup.field_name) == place.field_name;
}

bool Lowerer::cleanup_binding_has_prefix(
    const CleanupBinding& binding, const std::span<const LocalPlaceProjection> projections) const noexcept
{
    if (projections.empty()) {
        return true;
    }
    if (binding.projections.size() < projections.size()) {
        return false;
    }
    for (base::usize index = 0; index < projections.size(); ++index) {
        if (!this->cleanup_projection_matches(binding.projections[index], projections[index])) {
            return false;
        }
    }
    return true;
}

void Lowerer::append_local_cleanup_drop_if(const LocalBinding& binding)
{
    if (is_valid(binding.cleanup_flag)) {
        this->append_cleanup_drop_if(binding.slot, binding.cleanup_flag, binding.type,
            this->module_.values[binding.slot.value].name,
            binding.field_cleanups.empty() ? CleanupDropMode::full : CleanupDropMode::custom_destructor_only);
    }
    if (!binding.field_cleanups.empty()) {
        for (base::usize index = binding.field_cleanups.size(); index > 0; --index) {
            const CleanupBinding& cleanup = binding.field_cleanups[index - 1U];
            this->append_cleanup_drop_if(cleanup.address, cleanup.flag, cleanup.type, cleanup.name, cleanup.drop_mode);
        }
        return;
    }
}

void Lowerer::append_place_cleanup_drop_if(const syntax::ExprId expr_id)
{
    const std::optional<LocalPlacePath> path = this->local_place_path(expr_id);
    if (!path.has_value()) {
        return;
    }
    const LocalBinding* const binding = this->local_binding_for_place_path(*path);
    if (binding == nullptr) {
        return;
    }
    if (path->projections.empty()) {
        this->append_local_cleanup_drop_if(*binding);
        return;
    }
    for (base::usize index = binding->field_cleanups.size(); index > 0; --index) {
        const CleanupBinding& cleanup = binding->field_cleanups[index - 1U];
        if (this->cleanup_binding_has_prefix(cleanup, path->projections)) {
            this->append_cleanup_drop_if(cleanup.address, cleanup.flag, cleanup.type, cleanup.name, cleanup.drop_mode);
        }
    }
}

void Lowerer::mark_place_initialized(const syntax::ExprId expr_id)
{
    const std::optional<LocalPlacePath> path = this->local_place_path(expr_id);
    if (!path.has_value()) {
        return;
    }
    const auto found = this->locals_.find(path->root_name_id);
    if (found == this->locals_.end()) {
        return;
    }
    LocalBinding& binding = found->second;
    if (path->projections.empty()) {
        this->append_cleanup_flag_store(binding.cleanup_flag, true);
        if (!binding.field_cleanups.empty()) {
            for (const CleanupBinding& cleanup : binding.field_cleanups) {
                this->append_cleanup_flag_store(cleanup.flag, true);
            }
            return;
        }
        return;
    }
    for (const CleanupBinding& cleanup : binding.field_cleanups) {
        if (this->cleanup_binding_has_prefix(cleanup, path->projections)) {
            this->append_cleanup_flag_store(cleanup.flag, true);
        }
    }
    this->append_root_cleanup_flag_from_fields(binding);
}

void Lowerer::mark_local_moved(const sema::IdentId name_id)
{
    const auto found = this->locals_.find(name_id);
    if (found == this->locals_.end()) {
        return;
    }
    LocalBinding& binding = found->second;
    this->append_cleanup_flag_store(binding.cleanup_flag, false);
    if (!binding.field_cleanups.empty()) {
        for (const CleanupBinding& cleanup : binding.field_cleanups) {
            this->append_cleanup_flag_store(cleanup.flag, false);
        }
        return;
    }
}

void Lowerer::mark_expr_place_moved(const syntax::ExprId expr_id)
{
    if (this->expr_owned_use_mode(expr_id) != sema::OwnedUseMode::owned_consume) {
        return;
    }
    const std::optional<LocalPlacePath> path = this->local_place_path(expr_id);
    if (!path.has_value()) {
        return;
    }
    if (path->projections.empty()) {
        this->mark_local_moved(path->root_name_id);
        return;
    }
    const auto found = this->locals_.find(path->root_name_id);
    if (found == this->locals_.end()) {
        return;
    }
    for (const CleanupBinding& cleanup : found->second.field_cleanups) {
        if (this->cleanup_binding_has_prefix(cleanup, path->projections)) {
            this->append_cleanup_flag_store(cleanup.flag, false);
        }
    }
    this->append_cleanup_flag_store(found->second.cleanup_flag, false);
}

void Lowerer::lower_for_range(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt)
{
    const sema::ForInIterationPlan* const checked_plan = this->for_in_iteration_plan(stmt_id);
    const bool plan_is_iterable = checked_plan != nullptr && for_in_iteration_plan_is_iterable(*checked_plan);
    const bool plan_is_counted_range =
        checked_plan != nullptr && for_in_iteration_plan_is_counted_range(*checked_plan);
    if (checked_plan != nullptr && for_in_iteration_plan_is_range_value(*checked_plan)) {
        this->lower_for_range_value(stmt_id, stmt, *checked_plan);
        return;
    }
    if (plan_is_iterable || (!plan_is_counted_range && syntax::is_valid(stmt.range_iterable))) {
        this->lower_for_iterable(stmt_id, stmt);
        return;
    }

    this->push_local_scope();
    const base::usize scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});

    const sema::ForInIterationPlan fallback =
        fallback_counted_range_plan(stmt_id, stmt, this->stmt_local_type(stmt_id));
    const sema::ForInIterationPlan& plan = plan_is_counted_range ? *checked_plan : fallback;
    const sema::TypeHandle range_type = plan.range_type;
    const ValueId start_value = syntax::is_valid(plan.start_expr)
        ? this->lower_expr(plan.start_expr, range_type)
        : this->append_integer_literal(IR_FOR_RANGE_ZERO_LITERAL, range_type);
    const ValueId end_value = this->lower_expr(plan.end_expr, range_type);
    ValueId step_slot = INVALID_VALUE_ID;
    if (syntax::is_valid(plan.step_expr)) {
        const ValueId step_value = this->lower_expr(plan.step_expr, range_type);
        step_slot = this->append_temp_alloca("for.range.step", range_type);
        this->append_store(step_slot, step_value);
    }
    const ValueId cursor_slot = this->append_temp_alloca("for.range.cursor", range_type);
    const ValueId end_slot = this->append_temp_alloca("for.range.end", range_type);
    this->append_store(cursor_slot, start_value);
    this->append_store(end_slot, end_value);

    const BlockId condition_block = add_block(this->module_, *this->current_function_,
        "for.range.cond" + std::to_string(this->current_function_->blocks.size()));
    const BlockId body_block = add_block(this->module_, *this->current_function_,
        "for.range.body" + std::to_string(this->current_function_->blocks.size()));
    const BlockId update_block = add_block(this->module_, *this->current_function_,
        "for.range.update" + std::to_string(this->current_function_->blocks.size()));
    const BlockId exit_block = add_block(this->module_, *this->current_function_,
        "for.range.exit" + std::to_string(this->current_function_->blocks.size()));

    this->append_branch_if_open(condition_block);
    this->current_block_ = condition_block;
    const ValueId condition = this->append_for_range_condition(cursor_slot, end_slot, step_slot, range_type);
    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = body_block;
    cond.else_target = exit_block;
    this->set_terminator(this->current_block_, cond);

    this->loop_contexts_.push_back(LoopContext{exit_block, update_block, this->cleanup_scopes_.size()});
    this->current_block_ = body_block;
    const IrTextId loop_name = this->module_.intern(stmt.name);
    const ValueId loop_value = this->append_load(cursor_slot, range_type, loop_name);
    const ValueId loop_slot = this->append_temp_alloca(stmt.name, range_type);
    this->bind_local(stmt.name_id,
        LocalBinding{
            .slot = loop_slot,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = range_type,
            .is_mutable = false,
            .field_cleanups = {},
        });
    this->append_store(loop_slot, loop_value);
    this->lower_block(stmt.body);
    this->append_branch_if_open(update_block);

    this->current_block_ = update_block;
    const ValueId current = this->append_load(cursor_slot, range_type, this->module_.intern("for.range.cursor"));
    const ValueId step = is_valid(step_slot)
        ? this->append_load(step_slot, range_type, this->module_.intern("for.range.step"))
        : this->append_integer_literal(IR_FOR_RANGE_ONE_LITERAL, range_type);
    const ValueId next = this->append_binary_value(BinaryOp::add, range_type, current, step);
    this->append_store(cursor_slot, next);
    this->append_branch_if_open(condition_block);
    this->loop_contexts_.pop_back();

    this->current_block_ = exit_block;
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(scope_depth);
    }
    this->cleanup_scopes_.resize(scope_depth);
    this->pop_local_scope();
}

void Lowerer::lower_for_range_value(
    const syntax::StmtId, const syntax::StmtNode& stmt, const sema::ForInIterationPlan& plan)
{
    this->push_local_scope();
    const base::usize scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});

    const sema::TypeHandle range_value_type = plan.iterable_type;
    const sema::TypeHandle range_type = plan.range_type;
    const ValueId range_value = this->lower_expr(plan.iterable_expr, range_value_type);
    const ValueId range_slot = this->append_temp_alloca(IR_FOR_RANGE_VALUE_SLOT_NAME, range_value_type);
    this->append_store(range_slot, range_value);

    const auto load_range_field = [&](const std::string_view field_name) -> ValueId {
        const IrTextId field = this->module_.intern(field_name);
        const ValueId address =
            this->append_field_address(range_slot, field, range_type, sema::PointerMutability::const_);
        return this->append_load(address, range_type, field);
    };
    const ValueId start_value = load_range_field(sema::SEMA_RANGE_VALUE_START_FIELD);
    const ValueId end_value = load_range_field(sema::SEMA_RANGE_VALUE_END_FIELD);
    const ValueId step_value = load_range_field(sema::SEMA_RANGE_VALUE_STEP_FIELD);

    const ValueId cursor_slot = this->append_temp_alloca("for.range.cursor", range_type);
    const ValueId end_slot = this->append_temp_alloca("for.range.end", range_type);
    const ValueId step_slot = this->append_temp_alloca("for.range.step", range_type);
    this->append_store(cursor_slot, start_value);
    this->append_store(end_slot, end_value);
    this->append_store(step_slot, step_value);

    const BlockId condition_block = add_block(this->module_, *this->current_function_,
        "for.range.cond" + std::to_string(this->current_function_->blocks.size()));
    const BlockId body_block = add_block(this->module_, *this->current_function_,
        "for.range.body" + std::to_string(this->current_function_->blocks.size()));
    const BlockId update_block = add_block(this->module_, *this->current_function_,
        "for.range.update" + std::to_string(this->current_function_->blocks.size()));
    const BlockId exit_block = add_block(this->module_, *this->current_function_,
        "for.range.exit" + std::to_string(this->current_function_->blocks.size()));

    this->append_branch_if_open(condition_block);
    this->current_block_ = condition_block;
    const ValueId condition = this->append_for_range_condition(cursor_slot, end_slot, step_slot, range_type);
    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = body_block;
    cond.else_target = exit_block;
    this->set_terminator(this->current_block_, cond);

    this->loop_contexts_.push_back(LoopContext{exit_block, update_block, this->cleanup_scopes_.size()});
    this->current_block_ = body_block;
    const IrTextId loop_name = this->module_.intern(stmt.name);
    const ValueId loop_value = this->append_load(cursor_slot, range_type, loop_name);
    const ValueId loop_slot = this->append_temp_alloca(stmt.name, range_type);
    this->bind_local(stmt.name_id,
        LocalBinding{
            .slot = loop_slot,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = range_type,
            .is_mutable = false,
            .field_cleanups = {},
        });
    this->append_store(loop_slot, loop_value);
    this->lower_block(stmt.body);
    this->append_branch_if_open(update_block);

    this->current_block_ = update_block;
    const ValueId current = this->append_load(cursor_slot, range_type, this->module_.intern("for.range.cursor"));
    const ValueId step = this->append_load(step_slot, range_type, this->module_.intern("for.range.step"));
    const ValueId next = this->append_binary_value(BinaryOp::add, range_type, current, step);
    this->append_store(cursor_slot, next);
    this->append_branch_if_open(condition_block);
    this->loop_contexts_.pop_back();

    this->current_block_ = exit_block;
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(scope_depth);
    }
    this->cleanup_scopes_.resize(scope_depth);
    this->pop_local_scope();
}

void Lowerer::lower_for_iterable(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt)
{
    const sema::ForInIterationPlan* const checked_plan = this->for_in_iteration_plan(stmt_id);
    if (checked_plan != nullptr && checked_plan->kind == sema::ForInIterationKind::protocol_iterator) {
        this->lower_for_protocol_iterator(stmt_id, stmt, *checked_plan);
        return;
    }

    this->push_local_scope();
    const base::usize scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});

    const sema::ForInIterationPlan fallback =
        fallback_iterable_plan(stmt_id, stmt, this->module_.types, this->expr_type(stmt.range_iterable),
            this->stmt_local_type(stmt_id));
    const sema::ForInIterationPlan& plan =
        checked_plan != nullptr && for_in_iteration_plan_is_iterable(*checked_plan) ? *checked_plan : fallback;
    const sema::TypeHandle element_type = plan.item_type;
    const std::optional<ForIterableSource> source = this->lower_for_iterable_source(plan);
    if (!source.has_value()) {
        if (!this->has_terminator(this->current_block_)) {
            this->emit_cleanup_scopes(scope_depth);
        }
        this->cleanup_scopes_.resize(scope_depth);
        this->pop_local_scope();
        return;
    }

    const sema::TypeHandle usize_type = this->module_.types.builtin(sema::BuiltinType::usize);
    const ValueId cursor_slot = this->append_temp_alloca(IR_FOR_ITERABLE_CURSOR_SLOT_NAME, usize_type);
    const ValueId end_slot = this->append_temp_alloca(IR_FOR_ITERABLE_END_SLOT_NAME, usize_type);
    this->append_store(cursor_slot, this->append_integer_literal(IR_FOR_RANGE_ZERO_LITERAL, usize_type));
    this->append_store(end_slot, source->length);

    const BlockId condition_block = add_block(this->module_, *this->current_function_,
        std::string(IR_FOR_ITERABLE_COND_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));
    const BlockId body_block = add_block(this->module_, *this->current_function_,
        std::string(IR_FOR_ITERABLE_BODY_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));
    const BlockId update_block = add_block(this->module_, *this->current_function_,
        std::string(IR_FOR_ITERABLE_UPDATE_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));
    const BlockId exit_block = add_block(this->module_, *this->current_function_,
        std::string(IR_FOR_ITERABLE_EXIT_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));

    this->append_branch_if_open(condition_block);
    this->current_block_ = condition_block;
    const sema::TypeHandle bool_type = this->module_.types.builtin(sema::BuiltinType::bool_);
    const ValueId cursor_condition =
        this->append_load(cursor_slot, usize_type, this->module_.intern(IR_FOR_ITERABLE_CURSOR_SLOT_NAME));
    const ValueId end_condition =
        this->append_load(end_slot, usize_type, this->module_.intern(IR_FOR_ITERABLE_END_SLOT_NAME));
    const ValueId condition =
        this->append_binary_value(BinaryOp::less, bool_type, cursor_condition, end_condition);
    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = body_block;
    cond.else_target = exit_block;
    this->set_terminator(this->current_block_, cond);

    this->loop_contexts_.push_back(LoopContext{exit_block, update_block, this->cleanup_scopes_.size()});
    this->current_block_ = body_block;
    const IrTextId loop_name = this->module_.intern(stmt.name);
    const ValueId element_index =
        this->append_load(cursor_slot, usize_type, this->module_.intern(IR_FOR_ITERABLE_CURSOR_SLOT_NAME));
    const ValueId element_address = this->append_for_iterable_element_address(*source, element_index, loop_name);
    const ValueId element_value = this->append_load(element_address, element_type, loop_name);
    const ValueId loop_slot = this->append_temp_alloca(stmt.name, element_type);
    this->bind_local(stmt.name_id,
        LocalBinding{
            .slot = loop_slot,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = element_type,
            .is_mutable = false,
            .field_cleanups = {},
        });
    this->append_store(loop_slot, element_value);
    this->lower_block(stmt.body);
    this->append_branch_if_open(update_block);

    this->current_block_ = update_block;
    const ValueId current =
        this->append_load(cursor_slot, usize_type, this->module_.intern(IR_FOR_ITERABLE_CURSOR_SLOT_NAME));
    const ValueId one = this->append_integer_literal(IR_FOR_RANGE_ONE_LITERAL, usize_type);
    const ValueId next = this->append_binary_value(BinaryOp::add, usize_type, current, one);
    this->append_store(cursor_slot, next);
    this->append_branch_if_open(condition_block);
    this->loop_contexts_.pop_back();

    this->current_block_ = exit_block;
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(scope_depth);
    }
    this->cleanup_scopes_.resize(scope_depth);
    this->pop_local_scope();
}

void Lowerer::lower_for_protocol_iterator(
    const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const sema::ForInIterationPlan& plan)
{
    static_cast<void>(stmt_id);
    this->push_local_scope();
    const base::usize scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});

    ValueId source_slot = INVALID_VALUE_ID;
    if (plan.protocol_source == sema::ForInProtocolSourceKind::iter_method) {
        if (plan.iter_call.receiver_access == sema::ReceiverAccessKind::consuming) {
            const ValueId source_value = this->lower_expr(plan.iterable_expr, plan.iterable_type);
            source_slot = this->append_temp_alloca(IR_FOR_PROTOCOL_SOURCE_SLOT_NAME, plan.iterable_type);
            this->append_store(source_slot, this->coerce_value(source_value, plan.iterable_type));
        } else {
            const PlaceAddress source_place = this->lower_place_address(plan.iterable_expr);
            source_slot = source_place.address;
            if (!is_valid(source_slot)) {
                const ValueId source_value = this->lower_expr(plan.iterable_expr, plan.iterable_type);
                source_slot = this->append_temp_alloca(IR_FOR_PROTOCOL_SOURCE_SLOT_NAME, plan.iterable_type);
                this->append_store(source_slot, this->coerce_value(source_value, plan.iterable_type));
                LocalBinding source_binding{
                    .slot = source_slot,
                    .cleanup_flag = INVALID_VALUE_ID,
                    .type = plan.iterable_type,
                    .is_mutable = false,
                    .field_cleanups = {},
                };
                this->register_local_cleanup(source_binding, IR_FOR_PROTOCOL_SOURCE_SLOT_NAME);
            }
        }
    }

    const ValueId iterator_slot = this->append_temp_alloca(IR_FOR_PROTOCOL_ITERATOR_SLOT_NAME, plan.iterator_type);
    ValueId iterator_value = INVALID_VALUE_ID;
    if (plan.protocol_source == sema::ForInProtocolSourceKind::iter_method) {
        iterator_value = this->append_for_protocol_call(plan.iter_call, source_slot, plan.iterable_type);
    } else {
        iterator_value = this->lower_expr(plan.iterable_expr, plan.iterator_type);
    }
    this->append_store(iterator_slot, this->coerce_value(iterator_value, plan.iterator_type));
    LocalBinding iterator_binding{
        .slot = iterator_slot,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = plan.iterator_type,
        .is_mutable = true,
        .field_cleanups = {},
    };
    this->register_local_cleanup(iterator_binding, IR_FOR_PROTOCOL_ITERATOR_SLOT_NAME);

    const BlockId condition_block = add_block(this->module_, *this->current_function_,
        std::string(IR_FOR_PROTOCOL_COND_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));
    const BlockId body_block = add_block(this->module_, *this->current_function_,
        std::string(IR_FOR_PROTOCOL_BODY_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));
    const BlockId update_block = add_block(this->module_, *this->current_function_,
        std::string(IR_FOR_PROTOCOL_UPDATE_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));
    const BlockId exit_block = add_block(this->module_, *this->current_function_,
        std::string(IR_FOR_PROTOCOL_EXIT_BLOCK_PREFIX) + std::to_string(this->current_function_->blocks.size()));

    this->append_branch_if_open(condition_block);
    this->current_block_ = condition_block;
    const ValueId condition =
        this->append_for_protocol_call(plan.has_next_call, iterator_slot, plan.iterator_type);
    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = body_block;
    cond.else_target = exit_block;
    this->set_terminator(this->current_block_, cond);

    this->current_block_ = body_block;
    this->push_local_scope();
    const base::usize iteration_scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});
    this->loop_contexts_.push_back(LoopContext{exit_block, update_block, iteration_scope_depth});

    const ValueId loop_value = this->append_for_protocol_call(plan.next_call, iterator_slot, plan.iterator_type);
    const ValueId loop_slot = this->append_temp_alloca(stmt.name, plan.item_type);
    this->append_store(loop_slot, this->coerce_value(loop_value, plan.item_type));
    LocalBinding loop_binding{
        .slot = loop_slot,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = plan.item_type,
        .is_mutable = false,
        .field_cleanups = {},
    };
    this->register_local_cleanup(loop_binding, stmt.name);
    this->bind_local(stmt.name_id, loop_binding);
    this->lower_block_contents(stmt.body);
    this->loop_contexts_.pop_back();
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(iteration_scope_depth);
    }
    this->cleanup_scopes_.resize(iteration_scope_depth);
    this->pop_local_scope();
    this->append_branch_if_open(update_block);

    this->current_block_ = update_block;
    this->append_branch_if_open(condition_block);

    this->current_block_ = exit_block;
    if (!this->has_terminator(this->current_block_)) {
        this->emit_cleanup_scopes(scope_depth);
    }
    this->cleanup_scopes_.resize(scope_depth);
    this->pop_local_scope();
}

void Lowerer::emit_cleanup_scopes(const base::usize keep_depth)
{
    base::usize depth = cleanup_scopes_.size();
    while (depth > keep_depth) {
        const std::vector<CleanupAction>& scope = cleanup_scopes_[depth - 1];
        for (base::usize i = scope.size(); i > 0; --i) {
            const CleanupAction& action = scope[i - 1];
            if (action.kind == CleanupActionKind::defer_call) {
                static_cast<void>(lower_expr(action.defer_expr));
                continue;
            }
            if (is_valid(action.flag)) {
                this->append_cleanup_drop_if(action.slot, action.flag, action.type, action.name, action.drop_mode);
                continue;
            }
            this->append_cleanup_drop(action.slot, action.type, action.name, action.drop_mode);
        }
        --depth;
    }
}

void Lowerer::push_local_scope()
{
    this->local_scopes_.push_back(LocalScopeFrame{});
}

void Lowerer::pop_local_scope()
{
    if (this->local_scopes_.empty()) {
        return;
    }
    LocalScopeFrame frame = std::move(this->local_scopes_.back());
    this->local_scopes_.pop_back();
    for (const auto& [name_id, previous] : frame.previous_bindings) {
        if (previous.has_value()) {
            this->locals_[name_id] = *previous;
        } else {
            this->locals_.erase(name_id);
        }
    }
}

void Lowerer::bind_local(const sema::IdentId name_id, const LocalBinding binding)
{
    if (!this->local_scopes_.empty()) {
        LocalScopeFrame& scope = this->local_scopes_.back();
        if (!scope.previous_bindings.contains(name_id)) {
            if (const auto found = this->locals_.find(name_id); found != this->locals_.end()) {
                scope.previous_bindings.emplace(name_id, found->second);
            } else {
                scope.previous_bindings.emplace(name_id, std::nullopt);
            }
        }
    }
    this->locals_[name_id] = binding;
}

} // namespace aurex::ir::detail
