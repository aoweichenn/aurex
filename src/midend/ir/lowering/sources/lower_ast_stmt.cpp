#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <midend/ir/lowering/private/lower_ast_internal.hpp>

namespace aurex::ir::detail {

namespace {

constexpr std::string_view IR_FOR_RANGE_ZERO_LITERAL = "0";
constexpr std::string_view IR_FOR_RANGE_ONE_LITERAL = "1";

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
        this->register_local_cleanup(binding, param.name);
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
            append_store(slot_id, lower_expr(stmt.init, local_type));
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
            reversed.push_back(LocalPlaceProjection{field->field_name_id, field->field_name});
            current = field->object;
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
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

void Lowerer::append_cleanup_drop(const ValueId slot, const sema::TypeHandle type, const IrTextId name)
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
    static_cast<void>(this->append_value(drop));
}

void Lowerer::append_cleanup_drop_if(
    const ValueId slot, const ValueId flag, const sema::TypeHandle type, const IrTextId name)
{
    if (!is_valid(slot) || !is_valid(flag) || !sema::is_valid(type)) {
        return;
    }
    const ValueId initialized = this->append_load(
        flag, this->module_.types.builtin(sema::BuiltinType::bool_), this->module_.intern("drop.flag"));
    Value drop = this->module_.make_value();
    drop.kind = ValueKind::drop_if;
    drop.type = this->module_.types.builtin(sema::BuiltinType::void_);
    drop.object = slot;
    drop.lhs = initialized;
    drop.target_type = type;
    drop.name = name;
    static_cast<void>(this->append_value(drop));
    this->append_cleanup_flag_store(flag, false);
}

void Lowerer::register_local_cleanup(LocalBinding& binding, const std::string_view name)
{
    if (!this->cleanup_required(binding.type) || this->cleanup_scopes_.empty()) {
        return;
    }
    if (this->register_structured_local_cleanup(binding, name)) {
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
    });
}

bool Lowerer::register_structured_local_cleanup(LocalBinding& binding, const std::string_view name)
{
    if (!is_valid(binding.slot) || this->struct_info_for_type(binding.type) == nullptr) {
        return false;
    }
    std::vector<CleanupProjection> projections;
    const bool registered =
        this->append_structured_cleanup_bindings(binding, binding.slot, binding.type, projections, name);
    return registered && !binding.field_cleanups.empty();
}

bool Lowerer::append_structured_cleanup_bindings(LocalBinding& binding, const ValueId address,
    const sema::TypeHandle type, std::vector<CleanupProjection>& projections, const std::string_view name)
{
    const sema::StructInfo* const info = this->struct_info_for_type(type);
    if (info == nullptr || info->fields.empty()) {
        return false;
    }

    bool registered_any = false;
    for (const sema::StructFieldInfo& field : info->fields) {
        if (!this->cleanup_required(field.type)) {
            continue;
        }
        const IrTextId field_name = this->module_.intern(field.name);
        const ValueId field_address =
            this->append_field_address(address, field_name, field.type, sema::PointerMutability::mut);
        projections.push_back(CleanupProjection{field.name_id, field_name});
        const bool nested_registered =
            this->append_structured_cleanup_bindings(binding, field_address, field.type, projections, name);
        if (!nested_registered) {
            const std::string flag_name = std::string(name) + "." + std::string(field.name);
            const ValueId flag = this->append_cleanup_flag(flag_name);
            this->append_cleanup_flag_store(flag, true);
            this->append_cleanup_binding(binding, field_address, flag, field.type, field_name, projections);
        }
        projections.pop_back();
        registered_any = true;
    }
    return registered_any;
}

void Lowerer::append_cleanup_binding(LocalBinding& binding, const ValueId address, const ValueId flag,
    const sema::TypeHandle type, const IrTextId name, const std::vector<CleanupProjection>& projections)
{
    binding.field_cleanups.push_back(CleanupBinding{
        address,
        flag,
        type,
        name,
        projections,
    });
    this->cleanup_scopes_.back().push_back(CleanupAction{
        CleanupActionKind::drop_local,
        address,
        flag,
        type,
        syntax::INVALID_EXPR_ID,
        name,
    });
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
    if (!binding.field_cleanups.empty()) {
        for (base::usize index = binding.field_cleanups.size(); index > 0; --index) {
            const CleanupBinding& cleanup = binding.field_cleanups[index - 1U];
            this->append_cleanup_drop_if(cleanup.address, cleanup.flag, cleanup.type, cleanup.name);
        }
        return;
    }
    if (is_valid(binding.cleanup_flag)) {
        this->append_cleanup_drop_if(
            binding.slot, binding.cleanup_flag, binding.type, this->module_.values[binding.slot.value].name);
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
            this->append_cleanup_drop_if(cleanup.address, cleanup.flag, cleanup.type, cleanup.name);
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
        if (!binding.field_cleanups.empty()) {
            for (const CleanupBinding& cleanup : binding.field_cleanups) {
                this->append_cleanup_flag_store(cleanup.flag, true);
            }
            return;
        }
        this->append_cleanup_flag_store(binding.cleanup_flag, true);
        return;
    }
    for (const CleanupBinding& cleanup : binding.field_cleanups) {
        if (this->cleanup_binding_has_prefix(cleanup, path->projections)) {
            this->append_cleanup_flag_store(cleanup.flag, true);
        }
    }
}

void Lowerer::mark_local_moved(const sema::IdentId name_id)
{
    const auto found = this->locals_.find(name_id);
    if (found == this->locals_.end()) {
        return;
    }
    LocalBinding& binding = found->second;
    if (!binding.field_cleanups.empty()) {
        for (const CleanupBinding& cleanup : binding.field_cleanups) {
            this->append_cleanup_flag_store(cleanup.flag, false);
        }
        return;
    }
    this->append_cleanup_flag_store(binding.cleanup_flag, false);
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
}

void Lowerer::lower_for_range(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt)
{
    this->push_local_scope();
    const base::usize scope_depth = this->cleanup_scopes_.size();
    this->cleanup_scopes_.push_back({});

    const sema::TypeHandle range_type = this->stmt_local_type(stmt_id);
    const ValueId start_value = syntax::is_valid(stmt.range_start)
        ? this->lower_expr(stmt.range_start, range_type)
        : this->append_integer_literal(IR_FOR_RANGE_ZERO_LITERAL, range_type);
    const ValueId end_value = this->lower_expr(stmt.range_end, range_type);
    ValueId step_slot = INVALID_VALUE_ID;
    if (syntax::is_valid(stmt.range_step)) {
        const ValueId step_value = this->lower_expr(stmt.range_step, range_type);
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
                this->append_cleanup_drop_if(action.slot, action.flag, action.type, action.name);
                continue;
            }
            this->append_cleanup_drop(action.slot, action.type, action.name);
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
