#include <ir/lower_ast_internal.hpp>

#include <string_view>

namespace aurex::ir::detail {

namespace {

constexpr std::string_view IR_FOR_RANGE_ZERO_LITERAL = "0";
constexpr std::string_view IR_FOR_RANGE_ONE_LITERAL = "1";

[[nodiscard]] BinaryOp map_compound_assignment(const syntax::AssignOp op) noexcept {
    switch (op) {
    case syntax::AssignOp::add: return BinaryOp::add;
    case syntax::AssignOp::sub: return BinaryOp::sub;
    case syntax::AssignOp::mul: return BinaryOp::mul;
    case syntax::AssignOp::div: return BinaryOp::div;
    case syntax::AssignOp::mod: return BinaryOp::mod;
    case syntax::AssignOp::shl: return BinaryOp::shl;
    case syntax::AssignOp::shr: return BinaryOp::shr;
    case syntax::AssignOp::bit_and: return BinaryOp::bit_and;
    case syntax::AssignOp::bit_xor: return BinaryOp::bit_xor;
    case syntax::AssignOp::bit_or: return BinaryOp::bit_or;
    case syntax::AssignOp::assign: return BinaryOp::add;
    }
    return BinaryOp::add;
}

} // namespace

void Lowerer::lower_function_body(const FunctionId function_id, const syntax::ItemNode& item) {
    if (!is_valid(function_id) || function_id.value >= module_.functions.size()) {
        return;
    }
    current_function_ = &module_.functions[function_id.value];
    locals_.clear();
    loop_contexts_.clear();
    defer_scopes_.clear();
    current_block_ = add_block(*current_function_, "entry");

    for (base::usize i = 0; i < item.params.size(); ++i) {
        const syntax::ParamDecl& param = item.params[i];
        const sema::TypeHandle param_type = current_function_ != nullptr && i < current_function_->signature_params.size()
            ? current_function_->signature_params[i].type
            : syntax_type(param.type);
        Value param_value;
        param_value.kind = ValueKind::param;
        param_value.name = std::string(param.name);
        param_value.type = param_type;
        const ValueId param_id = append_value(param_value);
        current_function_->param_values.push_back(param_id);

        Value slot;
        slot.kind = ValueKind::alloca;
        slot.name = std::string(param.name);
        slot.type = module_.types.pointer(sema::PointerMutability::mut, param_value.type);
        const ValueId slot_id = append_value(slot);
        locals_[std::string(param.name)] = LocalBinding {slot_id, false};
        append_store(slot_id, param_id);
    }

    lower_block(item.body);
    if (!has_terminator(current_block_)) {
        Terminator term;
        term.kind = TerminatorKind::return_;
        set_terminator(current_block_, term);
    }
    current_function_ = nullptr;
}

void Lowerer::lower_block(const syntax::StmtId block_id) {
    const auto previous_locals = locals_;
    const base::usize scope_depth = defer_scopes_.size();
    defer_scopes_.push_back({});
    lower_block_contents(block_id);
    if (!has_terminator(current_block_)) {
        emit_deferred_scopes(scope_depth);
    }
    defer_scopes_.resize(scope_depth);
    locals_ = previous_locals;
}

void Lowerer::lower_block_contents(const syntax::StmtId block_id) {
    if (!syntax::is_valid(block_id) || block_id.value >= ast_.stmts.size()) {
        return;
    }
    const syntax::StmtNode& block = ast_.stmts[block_id.value];
    if (block.kind != syntax::StmtKind::block) {
        return;
    }
    for (const syntax::StmtId stmt : block.statements) {
        if (has_terminator(current_block_)) {
            break;
        }
        lower_stmt(stmt);
    }
}

void Lowerer::lower_stmt(const syntax::StmtId stmt_id) {
    if (!syntax::is_valid(stmt_id) || stmt_id.value >= ast_.stmts.size()) {
        return;
    }
    const syntax::StmtNode& stmt = ast_.stmts[stmt_id.value];
    switch (stmt.kind) {
    case syntax::StmtKind::let:
    case syntax::StmtKind::var: {
        const sema::TypeHandle local_type = stmt_local_type(stmt_id);
        Value slot;
        slot.kind = ValueKind::alloca;
        slot.name = std::string(stmt.name);
        slot.type = module_.types.pointer(sema::PointerMutability::mut, local_type);
        const ValueId slot_id = append_value(slot);
        locals_[std::string(stmt.name)] = LocalBinding {slot_id, stmt.kind == syntax::StmtKind::var};
        append_store(slot_id, lower_expr(stmt.init, local_type));
        break;
    }
    case syntax::StmtKind::assign: {
        const sema::TypeHandle lhs_type = this->expr_type(stmt.lhs);
        const ValueId target = this->lower_place_addr(stmt.lhs);
        ValueId source = this->lower_expr(stmt.rhs, lhs_type);
        if (stmt.assign_op != syntax::AssignOp::assign) {
            const ValueId current = this->append_load(target, lhs_type);
            Value value;
            value.kind = ValueKind::binary;
            value.type = lhs_type;
            value.binary_op = map_compound_assignment(stmt.assign_op);
            value.lhs = current;
            value.rhs = source;
            source = this->append_value(value);
        }
        this->append_store(target, source);
        break;
    }
    case syntax::StmtKind::if_:
        lower_if(stmt);
        break;
    case syntax::StmtKind::for_:
        lower_for(stmt);
        break;
    case syntax::StmtKind::for_range:
        lower_for_range(stmt_id, stmt);
        break;
    case syntax::StmtKind::while_:
        lower_while(stmt);
        break;
    case syntax::StmtKind::break_: {
        if (!loop_contexts_.empty()) {
            emit_deferred_scopes(loop_contexts_.back().defer_depth);
        }
        Terminator term;
        term.kind = TerminatorKind::branch;
        term.target = loop_contexts_.empty() ? INVALID_BLOCK_ID : loop_contexts_.back().break_target;
        set_terminator(current_block_, term);
        break;
    }
    case syntax::StmtKind::continue_: {
        if (!loop_contexts_.empty()) {
            emit_deferred_scopes(loop_contexts_.back().defer_depth);
        }
        Terminator term;
        term.kind = TerminatorKind::branch;
        term.target = loop_contexts_.empty() ? INVALID_BLOCK_ID : loop_contexts_.back().continue_target;
        set_terminator(current_block_, term);
        break;
    }
    case syntax::StmtKind::defer:
        if (!defer_scopes_.empty()) {
            defer_scopes_.back().push_back(stmt.init);
        }
        break;
    case syntax::StmtKind::return_: {
        Terminator term;
        term.kind = TerminatorKind::return_;
        if (syntax::is_valid(stmt.return_value)) {
            term.value = coerce_value(lower_expr(stmt.return_value, current_function_->return_type), current_function_->return_type);
        }
        emit_deferred_scopes(0);
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

void Lowerer::lower_if(const syntax::StmtNode& stmt) {
    const ValueId condition = lower_expr(stmt.condition);
    const BlockId then_block = add_block(*current_function_, "if.then" + std::to_string(current_function_->blocks.size()));
    const BlockId else_block = add_block(*current_function_, "if.else" + std::to_string(current_function_->blocks.size()));
    BlockId join_block = INVALID_BLOCK_ID;
    const auto ensure_join_block = [&]() -> BlockId {
        if (!is_valid(join_block)) {
            join_block = add_block(*current_function_, "if.join" + std::to_string(current_function_->blocks.size()));
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
    lower_block(stmt.then_block);
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

void Lowerer::lower_while(const syntax::StmtNode& stmt) {
    const BlockId condition_block = add_block(*current_function_, "while.cond" + std::to_string(current_function_->blocks.size()));

    append_branch_if_open(condition_block);
    current_block_ = condition_block;
    const ValueId condition = lower_expr(stmt.condition);
    const BlockId body_block = add_block(*current_function_, "while.body" + std::to_string(current_function_->blocks.size()));
    const BlockId exit_block = add_block(*current_function_, "while.exit" + std::to_string(current_function_->blocks.size()));
    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = body_block;
    cond.else_target = exit_block;
    set_terminator(current_block_, cond);

    loop_contexts_.push_back(LoopContext {exit_block, condition_block, defer_scopes_.size()});
    current_block_ = body_block;
    lower_block(stmt.body);
    append_branch_if_open(condition_block);
    loop_contexts_.pop_back();

    current_block_ = exit_block;
}

void Lowerer::lower_for(const syntax::StmtNode& stmt) {
    const auto previous_locals = locals_;
    const base::usize scope_depth = defer_scopes_.size();
    defer_scopes_.push_back({});

    if (syntax::is_valid(stmt.for_init)) {
        lower_stmt(stmt.for_init);
    }

    const BlockId condition_block = add_block(*current_function_, "for.cond" + std::to_string(current_function_->blocks.size()));
    const BlockId body_block = add_block(*current_function_, "for.body" + std::to_string(current_function_->blocks.size()));
    const BlockId update_block = add_block(*current_function_, "for.update" + std::to_string(current_function_->blocks.size()));
    const BlockId exit_block = add_block(*current_function_, "for.exit" + std::to_string(current_function_->blocks.size()));

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

    loop_contexts_.push_back(LoopContext {exit_block, update_block, defer_scopes_.size()});
    current_block_ = body_block;
    lower_block(stmt.body);
    append_branch_if_open(update_block);

    current_block_ = update_block;
    if (syntax::is_valid(stmt.for_update)) {
        lower_stmt(stmt.for_update);
    }
    append_branch_if_open(condition_block);
    loop_contexts_.pop_back();

    defer_scopes_.resize(scope_depth);
    locals_ = previous_locals;
    current_block_ = exit_block;
}

void Lowerer::lower_for_range(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt) {
    const auto previous_locals = locals_;
    const base::usize scope_depth = defer_scopes_.size();
    defer_scopes_.push_back({});

    const sema::TypeHandle range_type = stmt_local_type(stmt_id);
    const ValueId start_value = syntax::is_valid(stmt.range_start)
        ? lower_expr(stmt.range_start, range_type)
        : append_integer_literal(IR_FOR_RANGE_ZERO_LITERAL, range_type);
    const ValueId end_value = lower_expr(stmt.range_end, range_type);
    const ValueId cursor_slot = append_temp_alloca("for.range.cursor", range_type);
    const ValueId end_slot = append_temp_alloca("for.range.end", range_type);
    append_store(cursor_slot, start_value);
    append_store(end_slot, end_value);

    const BlockId condition_block = add_block(*current_function_, "for.range.cond" + std::to_string(current_function_->blocks.size()));
    const BlockId body_block = add_block(*current_function_, "for.range.body" + std::to_string(current_function_->blocks.size()));
    const BlockId update_block = add_block(*current_function_, "for.range.update" + std::to_string(current_function_->blocks.size()));
    const BlockId exit_block = add_block(*current_function_, "for.range.exit" + std::to_string(current_function_->blocks.size()));

    append_branch_if_open(condition_block);
    current_block_ = condition_block;
    const ValueId cursor_condition = append_load(cursor_slot, range_type, "for.range.cursor");
    const ValueId end_condition = append_load(end_slot, range_type, "for.range.end");
    const ValueId condition = append_binary_value(
        BinaryOp::less,
        module_.types.builtin(sema::BuiltinType::bool_),
        cursor_condition,
        end_condition
    );
    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = body_block;
    cond.else_target = exit_block;
    set_terminator(current_block_, cond);

    loop_contexts_.push_back(LoopContext {exit_block, update_block, defer_scopes_.size()});
    current_block_ = body_block;
    const ValueId loop_value = append_load(cursor_slot, range_type, std::string(stmt.name));
    const ValueId loop_slot = append_temp_alloca(std::string(stmt.name), range_type);
    locals_[std::string(stmt.name)] = LocalBinding {loop_slot, false};
    append_store(loop_slot, loop_value);
    lower_block(stmt.body);
    append_branch_if_open(update_block);

    current_block_ = update_block;
    const ValueId current = append_load(cursor_slot, range_type, "for.range.cursor");
    const ValueId one = append_integer_literal(IR_FOR_RANGE_ONE_LITERAL, range_type);
    const ValueId next = append_binary_value(BinaryOp::add, range_type, current, one);
    append_store(cursor_slot, next);
    append_branch_if_open(condition_block);
    loop_contexts_.pop_back();

    defer_scopes_.resize(scope_depth);
    locals_ = previous_locals;
    current_block_ = exit_block;
}

void Lowerer::emit_deferred_scopes(const base::usize keep_depth) {
    base::usize depth = defer_scopes_.size();
    while (depth > keep_depth) {
        const std::vector<syntax::ExprId>& scope = defer_scopes_[depth - 1];
        for (base::usize i = scope.size(); i > 0; --i) {
            static_cast<void>(lower_expr(scope[i - 1]));
        }
        --depth;
    }
}

} // namespace aurex::ir::detail
