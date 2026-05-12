#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <vector>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY = 16;
constexpr base::usize SEMA_CONTROL_FLOW_FIRST_CHILD_INDEX = 0;
constexpr base::usize SEMA_FOR_RANGE_MAX_OPERAND_COUNT = 3;

enum class ControlFlowQuery {
    guarantees_return,
    may_fallthrough,
};

enum class ControlFlowFrameKind {
    statement,
    block,
    if_statement,
};

enum class ControlFlowIfStage {
    evaluate_then,
    after_then,
    evaluate_alternate,
    after_alternate,
};

struct ControlFlowFrame {
    ControlFlowFrameKind kind = ControlFlowFrameKind::statement;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    base::usize next_child = SEMA_CONTROL_FLOW_FIRST_CHILD_INDEX;
    ControlFlowIfStage if_stage = ControlFlowIfStage::evaluate_then;
};

[[nodiscard]] const syntax::StmtNode* statement_node(
    const syntax::AstModule& module,
    const syntax::StmtId stmt
) noexcept {
    if (!syntax::is_valid(stmt) || stmt.value >= module.stmts.size()) {
        return nullptr;
    }
    return &module.stmts[stmt.value];
}

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module,
    const syntax::ExprId expr,
    const base::SourceRange fallback
) noexcept {
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return fallback;
    }
    return module.exprs[expr.value].range;
}

[[nodiscard]] bool statement_binary_result_uses_operand_type(const syntax::BinaryOp op) noexcept {
    switch (op) {
    case syntax::BinaryOp::add:
    case syntax::BinaryOp::sub:
    case syntax::BinaryOp::mul:
    case syntax::BinaryOp::div:
    case syntax::BinaryOp::mod:
    case syntax::BinaryOp::shl:
    case syntax::BinaryOp::shr:
    case syntax::BinaryOp::bit_and:
    case syntax::BinaryOp::bit_xor:
    case syntax::BinaryOp::bit_or:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool statement_contextual_integer_expr(
    const syntax::AstModule& module,
    const syntax::ExprId candidate
) {
    std::vector<syntax::ExprId> pending;
    pending.push_back(candidate);
    while (!pending.empty()) {
        const syntax::ExprId current = pending.back();
        pending.pop_back();
        if (!syntax::is_valid(current) || current.value >= module.exprs.size()) {
            return false;
        }
        const syntax::ExprNode& node = module.exprs[current.value];
        if (node.kind == syntax::ExprKind::integer_literal) {
            continue;
        }
        if (node.kind == syntax::ExprKind::unary && node.unary_op == syntax::UnaryOp::numeric_negate) {
            pending.push_back(node.unary_operand);
            continue;
        }
        if (node.kind == syntax::ExprKind::binary &&
            statement_binary_result_uses_operand_type(node.binary_op)) {
            pending.push_back(node.binary_lhs);
            pending.push_back(node.binary_rhs);
            continue;
        }
        return false;
    }
    return true;
}

[[nodiscard]] bool default_control_flow_result(const ControlFlowQuery query) noexcept {
    return query == ControlFlowQuery::may_fallthrough;
}

[[nodiscard]] bool block_short_circuits(const ControlFlowQuery query, const bool child_result) noexcept {
    return query == ControlFlowQuery::guarantees_return ? child_result : !child_result;
}

[[nodiscard]] bool if_then_short_circuits(const ControlFlowQuery query, const bool then_result) noexcept {
    return query == ControlFlowQuery::guarantees_return ? !then_result : then_result;
}

[[nodiscard]] bool abrupt_stmt_result(
    const ControlFlowQuery query,
    const syntax::StmtKind kind
) noexcept {
    if (query == ControlFlowQuery::guarantees_return) {
        return kind == syntax::StmtKind::return_;
    }
    return false;
}

void finish_control_flow_frame(
    std::vector<ControlFlowFrame>& stack,
    const ControlFlowQuery query,
    bool result,
    bool& has_result,
    bool& final_result
) {
    stack.pop_back();
    while (!stack.empty()) {
        ControlFlowFrame& parent = stack.back();
        switch (parent.kind) {
        case ControlFlowFrameKind::block:
            if (block_short_circuits(query, result)) {
                stack.pop_back();
                continue;
            }
            ++parent.next_child;
            return;
        case ControlFlowFrameKind::if_statement:
            if (parent.if_stage == ControlFlowIfStage::after_then) {
                if (if_then_short_circuits(query, result)) {
                    stack.pop_back();
                    continue;
                }
                parent.if_stage = ControlFlowIfStage::evaluate_alternate;
                return;
            }
            if (parent.if_stage == ControlFlowIfStage::after_alternate) {
                stack.pop_back();
                continue;
            }
            return;
        case ControlFlowFrameKind::statement:
            return;
        }
    }
    has_result = true;
    final_result = result;
}

void evaluate_control_flow_statement(
    const syntax::AstModule& module,
    std::vector<ControlFlowFrame>& stack,
    const ControlFlowQuery query,
    bool& has_result,
    bool& final_result
) {
    ControlFlowFrame& frame = stack.back();
    const syntax::StmtNode* const node = statement_node(module, frame.stmt);
    if (node == nullptr) {
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        return;
    }
    switch (node->kind) {
    case syntax::StmtKind::return_:
    case syntax::StmtKind::break_:
    case syntax::StmtKind::continue_:
        finish_control_flow_frame(stack, query, abrupt_stmt_result(query, node->kind), has_result, final_result);
        break;
    case syntax::StmtKind::block:
        frame.kind = ControlFlowFrameKind::block;
        frame.next_child = SEMA_CONTROL_FLOW_FIRST_CHILD_INDEX;
        break;
    case syntax::StmtKind::if_:
        frame.kind = ControlFlowFrameKind::if_statement;
        frame.if_stage = ControlFlowIfStage::evaluate_then;
        break;
    default:
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        break;
    }
}

void evaluate_control_flow_block(
    const syntax::AstModule& module,
    std::vector<ControlFlowFrame>& stack,
    const ControlFlowQuery query,
    bool& has_result,
    bool& final_result
) {
    ControlFlowFrame& frame = stack.back();
    const syntax::StmtNode* const node = statement_node(module, frame.stmt);
    if (node == nullptr) {
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        return;
    }
    if (node->kind != syntax::StmtKind::block) {
        frame.kind = ControlFlowFrameKind::statement;
        return;
    }
    if (frame.next_child >= node->statements.size()) {
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        return;
    }
    stack.push_back(ControlFlowFrame {ControlFlowFrameKind::statement, node->statements[frame.next_child]});
}

void evaluate_control_flow_if_statement(
    const syntax::AstModule& module,
    std::vector<ControlFlowFrame>& stack,
    const ControlFlowQuery query,
    bool& has_result,
    bool& final_result
) {
    ControlFlowFrame& frame = stack.back();
    const syntax::StmtNode* const node = statement_node(module, frame.stmt);
    if (node == nullptr || node->kind != syntax::StmtKind::if_) {
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        return;
    }
    if (frame.if_stage == ControlFlowIfStage::evaluate_then) {
        frame.if_stage = ControlFlowIfStage::after_then;
        stack.push_back(ControlFlowFrame {ControlFlowFrameKind::block, node->then_block});
        return;
    }
    if (frame.if_stage == ControlFlowIfStage::evaluate_alternate) {
        frame.if_stage = ControlFlowIfStage::after_alternate;
        if (syntax::is_valid(node->else_block)) {
            stack.push_back(ControlFlowFrame {ControlFlowFrameKind::block, node->else_block});
            return;
        }
        if (syntax::is_valid(node->else_if)) {
            stack.push_back(ControlFlowFrame {ControlFlowFrameKind::statement, node->else_if});
            return;
        }
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
    }
}

[[nodiscard]] bool evaluate_control_flow(
    const syntax::AstModule& module,
    const syntax::StmtId stmt,
    const ControlFlowFrameKind root_kind,
    const ControlFlowQuery query
) {
    std::vector<ControlFlowFrame> stack;
    stack.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
    stack.push_back(ControlFlowFrame {root_kind, stmt});

    bool has_result = false;
    bool final_result = default_control_flow_result(query);
    while (!has_result && !stack.empty()) {
        switch (stack.back().kind) {
        case ControlFlowFrameKind::statement:
            evaluate_control_flow_statement(module, stack, query, has_result, final_result);
            break;
        case ControlFlowFrameKind::block:
            evaluate_control_flow_block(module, stack, query, has_result, final_result);
            break;
        case ControlFlowFrameKind::if_statement:
            evaluate_control_flow_if_statement(module, stack, query, has_result, final_result);
            break;
        }
    }
    return final_result;
}

[[nodiscard]] bool is_allowed_expression_statement(
    const syntax::AstModule& module,
    const syntax::ExprId expr_id
) noexcept {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module.exprs.size()) {
        return false;
    }
    const syntax::ExprKind kind = module.exprs[expr_id.value].kind;
    return kind == syntax::ExprKind::call || kind == syntax::ExprKind::try_expr;
}

[[nodiscard]] bool compound_assignment_binary_op(
    const syntax::AssignOp op,
    syntax::BinaryOp& binary_op
) noexcept {
    switch (op) {
    case syntax::AssignOp::add:
        binary_op = syntax::BinaryOp::add;
        return true;
    case syntax::AssignOp::sub:
        binary_op = syntax::BinaryOp::sub;
        return true;
    case syntax::AssignOp::mul:
        binary_op = syntax::BinaryOp::mul;
        return true;
    case syntax::AssignOp::div:
        binary_op = syntax::BinaryOp::div;
        return true;
    case syntax::AssignOp::mod:
        binary_op = syntax::BinaryOp::mod;
        return true;
    case syntax::AssignOp::shl:
        binary_op = syntax::BinaryOp::shl;
        return true;
    case syntax::AssignOp::shr:
        binary_op = syntax::BinaryOp::shr;
        return true;
    case syntax::AssignOp::bit_and:
        binary_op = syntax::BinaryOp::bit_and;
        return true;
    case syntax::AssignOp::bit_xor:
        binary_op = syntax::BinaryOp::bit_xor;
        return true;
    case syntax::AssignOp::bit_or:
        binary_op = syntax::BinaryOp::bit_or;
        return true;
    case syntax::AssignOp::assign:
        return false;
    }
    return false;
}

} // namespace

void SemanticAnalyzer::analyze_function_body(const syntax::ItemNode& function) {
    const std::string key = this->function_key(function);
    const auto found = this->checked_.functions.find(key);
    if (found == this->checked_.functions.end()) {
        return;
    }
    this->analyze_function_body_with_signature(function, key, found->second, this->function_body_states_[key]);
}

void SemanticAnalyzer::analyze_function_body_with_signature(
    const syntax::ItemNode& function,
    const std::string& key,
    const FunctionSignature& signature,
    FunctionBodyState& state
) {
    const syntax::ModuleId previous_module = this->current_module_;
    const TypeHandle previous_function_return_type = this->current_function_return_type_;
    ReturnTypeInference* const previous_return_inference = this->current_return_inference_;
    const int previous_loop_depth = this->loop_depth_;
    const SymbolTable previous_symbols = this->symbols_;
    const auto restore_context = [&]() {
        this->current_module_ = previous_module;
        this->current_function_return_type_ = previous_function_return_type;
        this->current_return_inference_ = previous_return_inference;
        this->loop_depth_ = previous_loop_depth;
        this->symbols_ = previous_symbols;
    };
    this->current_module_ = signature.module;
    this->symbols_ = SymbolTable {};
    if (signature.has_conflict) {
        restore_context();
        return;
    }
    if (state == FunctionBodyState::analyzing) {
        if (!is_valid(signature.return_type)) {
            this->report(function.range, std::string(SEMA_RECURSIVE_RETURN_INFER));
        }
        restore_context();
        return;
    }
    if (state == FunctionBodyState::analyzed) {
        restore_context();
        return;
    }
    state = FunctionBodyState::analyzing;
    this->loop_depth_ = SEMA_NO_LOOP_DEPTH;
    const bool infer_return_type = !syntax::is_valid(function.return_type);
    ReturnTypeInference return_inference;
    TypeHandle expected_return = signature.return_type;
    if (infer_return_type) {
        expected_return = INVALID_TYPE_HANDLE;
    }
    this->current_function_return_type_ = expected_return;
    this->current_return_inference_ = infer_return_type ? &return_inference : nullptr;

    this->symbols_.push_scope();
    for (base::usize i = 0; i < function.params.size(); ++i) {
        const syntax::ParamDecl& param = function.params[i];
        const TypeHandle param_type = i < signature.param_types.size()
            ? signature.param_types[i]
            : this->resolve_type(param.type);
        const auto inserted = this->symbols_.insert(Symbol {
            SymbolKind::parameter,
            std::string(param.name),
            {},
            syntax::INVALID_MODULE_ID,
            param_type,
            param.range,
            false,
            syntax::Visibility::private_,
        }, this->diagnostics_);
        static_cast<void>(inserted);
    }
    this->analyze_block(function.body, expected_return, infer_return_type ? &return_inference : nullptr);
    this->symbols_.pop_scope();
    if (infer_return_type) {
        this->finalize_inferred_return(function, key, return_inference);
        if (is_valid(return_inference.inferred_type) &&
            !this->checked_.types.is_void(return_inference.inferred_type) &&
            !this->block_guarantees_return(function.body)) {
            this->report(function.range, std::string(SEMA_NOT_ALL_PATHS_RETURN));
        }
    } else if (is_valid(expected_return) &&
        !this->checked_.types.is_void(expected_return) &&
        !this->block_guarantees_return(function.body)) {
        this->report(function.range, std::string(SEMA_NOT_ALL_PATHS_RETURN));
    }
    state = FunctionBodyState::analyzed;
    restore_context();
}

void SemanticAnalyzer::analyze_block(
    const syntax::StmtId block,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    this->analyze_statement_tree(block, expected_return, return_inference, StatementAnalysisRootKind::scoped_block);
}

void SemanticAnalyzer::analyze_block_statements(
    const syntax::StmtId block,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    this->analyze_statement_tree(block, expected_return, return_inference, StatementAnalysisRootKind::block_statements);
}

TypeHandle SemanticAnalyzer::analyze_assignment_target(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->module_.exprs.size()) {
        return INVALID_TYPE_HANDLE;
    }
    const syntax::ExprNode& expr = this->module_.exprs[expr_id.value];
    if (expr.kind != syntax::ExprKind::name || !expr.scope_name.empty()) {
        return this->analyze_expr(expr_id);
    }
    const Symbol* symbol = this->find_symbol(expr.text, expr.range);
    if (symbol == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (symbol->kind == SymbolKind::function) {
        this->report(expr.range, sema_function_name_value_message(expr.text));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    this->record_expr_c_name(expr_id, symbol->c_name);
    return this->record_expr_type(expr_id, symbol->type);
}

void SemanticAnalyzer::analyze_stmt(
    const syntax::StmtId stmt_id,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    this->analyze_statement_tree(stmt_id, expected_return, return_inference, StatementAnalysisRootKind::statement);
}

void SemanticAnalyzer::analyze_statement_tree(
    const syntax::StmtId root,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference,
    const StatementAnalysisRootKind root_kind
) {
    std::vector<StatementAnalysisAction> stack;
    stack.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
    switch (root_kind) {
    case StatementAnalysisRootKind::statement:
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::statement, root});
        break;
    case StatementAnalysisRootKind::scoped_block:
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::scoped_block, root});
        break;
    case StatementAnalysisRootKind::block_statements:
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::block_statements, root});
        break;
    }
    while (!stack.empty()) {
        const StatementAnalysisAction action = stack.back();
        stack.pop_back();
        this->analyze_statement_action(action, stack, expected_return, return_inference);
    }
}

void SemanticAnalyzer::analyze_statement_action(
    const StatementAnalysisAction& action,
    std::vector<StatementAnalysisAction>& stack,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    switch (action.kind) {
    case StatementAnalysisActionKind::statement:
        this->analyze_statement_node(action.stmt, stack, expected_return, return_inference);
        break;
    case StatementAnalysisActionKind::scoped_block:
        this->symbols_.push_scope();
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::pop_scope});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::block_statements, action.stmt});
        break;
    case StatementAnalysisActionKind::block_statements:
        this->analyze_statement_block(action.stmt, stack);
        break;
    case StatementAnalysisActionKind::pop_scope:
        this->symbols_.pop_scope();
        break;
    case StatementAnalysisActionKind::enter_loop:
        ++this->loop_depth_;
        break;
    case StatementAnalysisActionKind::exit_loop:
        --this->loop_depth_;
        break;
    case StatementAnalysisActionKind::for_condition:
        this->analyze_for_condition(action.stmt);
        break;
    }
}

void SemanticAnalyzer::analyze_statement_block(
    const syntax::StmtId block,
    std::vector<StatementAnalysisAction>& stack
) {
    const syntax::StmtNode* const stmt = statement_node(this->module_, block);
    if (stmt == nullptr || stmt->kind != syntax::StmtKind::block) {
        return;
    }
    for (base::usize i = stmt->statements.size(); i > 0; --i) {
        stack.push_back(StatementAnalysisAction {
            StatementAnalysisActionKind::statement,
            stmt->statements[i - 1],
        });
    }
}

void SemanticAnalyzer::analyze_for_condition(const syntax::StmtId stmt_id) {
    const syntax::StmtNode* const stmt = statement_node(this->module_, stmt_id);
    if (stmt == nullptr || stmt->kind != syntax::StmtKind::for_ || !syntax::is_valid(stmt->condition)) {
        return;
    }
    const TypeHandle condition = this->analyze_expr(stmt->condition);
    if (!this->checked_.types.is_bool(condition)) {
        this->report(expr_range_or(this->module_, stmt->condition, stmt->range), std::string(SEMA_FOR_CONDITION_BOOL));
    }
}

TypeHandle SemanticAnalyzer::analyze_for_range_bounds(
    const syntax::StmtId stmt_id,
    const syntax::StmtNode& stmt
) {
    if (!syntax::is_valid(stmt.range_end)) {
        this->record_stmt_local_type(stmt_id, INVALID_TYPE_HANDLE);
        this->report(stmt.range, std::string(SEMA_FOR_RANGE_ARITY));
        return INVALID_TYPE_HANDLE;
    }

    std::vector<syntax::ExprId> operands;
    operands.reserve(SEMA_FOR_RANGE_MAX_OPERAND_COUNT);
    bool has_start = false;
    bool has_step = false;
    base::usize start_index = 0;
    base::usize end_index = 0;
    base::usize step_index = 0;
    if (syntax::is_valid(stmt.range_start)) {
        has_start = true;
        start_index = operands.size();
        operands.push_back(stmt.range_start);
    }
    end_index = operands.size();
    operands.push_back(stmt.range_end);
    if (syntax::is_valid(stmt.range_step)) {
        has_step = true;
        step_index = operands.size();
        operands.push_back(stmt.range_step);
    }

    std::vector<TypeHandle> operand_types(operands.size(), INVALID_TYPE_HANDLE);
    TypeHandle range_type = INVALID_TYPE_HANDLE;
    for (base::usize i = 0; i < operands.size(); ++i) {
        if (!statement_contextual_integer_expr(this->module_, operands[i])) {
            operand_types[i] = this->analyze_expr(operands[i]);
            range_type = operand_types[i];
            break;
        }
    }
    if (!is_valid(range_type) && !operands.empty()) {
        operand_types.front() = this->analyze_expr(operands.front());
        range_type = operand_types.front();
    }
    for (base::usize i = 0; i < operands.size(); ++i) {
        if (!is_valid(operand_types[i])) {
            operand_types[i] = this->analyze_expr(operands[i], range_type);
        }
    }

    const TypeHandle end = operand_types[end_index];
    const TypeHandle start = has_start ? operand_types[start_index] : end;
    const TypeHandle step = has_step ? operand_types[step_index] : range_type;

    if (syntax::is_valid(stmt.range_start) && !this->checked_.types.is_integer(start)) {
        this->report(expr_range_or(this->module_, stmt.range_start, stmt.range), std::string(SEMA_RANGE_BOUNDS_INTEGER));
    }
    if (!this->checked_.types.is_integer(end)) {
        this->report(expr_range_or(this->module_, stmt.range_end, stmt.range), std::string(SEMA_RANGE_BOUNDS_INTEGER));
    }
    if (has_step && !this->checked_.types.is_integer(step)) {
        this->report(expr_range_or(this->module_, stmt.range_step, stmt.range), std::string(SEMA_RANGE_STEP_INTEGER));
    }
    const bool bounds_have_same_type =
        is_valid(start) &&
        is_valid(end) &&
        this->checked_.types.same(start, end);
    if (is_valid(start) && is_valid(end) && !bounds_have_same_type) {
        this->report(stmt.range, std::string(SEMA_RANGE_BOUNDS_SAME_TYPE));
    }
    if (has_step &&
        bounds_have_same_type &&
        this->checked_.types.is_integer(start) &&
        this->checked_.types.is_integer(step) &&
        !this->checked_.types.same(start, step)) {
        this->report(expr_range_or(this->module_, stmt.range_step, stmt.range), std::string(SEMA_RANGE_STEP_SAME_TYPE));
    }

    const TypeHandle local_type = this->checked_.types.is_integer(start)
        ? start
        : (this->checked_.types.is_integer(end) ? end : step);
    this->record_stmt_local_type(stmt_id, local_type);
    return local_type;
}

void SemanticAnalyzer::define_for_range_local(const syntax::StmtNode& stmt, const TypeHandle type) {
    const auto inserted = this->symbols_.insert(Symbol {
        SymbolKind::local,
        std::string(stmt.name),
        {},
        syntax::INVALID_MODULE_ID,
        type,
        stmt.range,
        false,
        syntax::Visibility::private_,
    }, this->diagnostics_);
    static_cast<void>(inserted);
}

void SemanticAnalyzer::analyze_statement_node(
    const syntax::StmtId stmt_id,
    std::vector<StatementAnalysisAction>& stack,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    const syntax::StmtNode* const stmt_ptr = statement_node(this->module_, stmt_id);
    if (stmt_ptr == nullptr) {
        return;
    }
    const syntax::StmtNode& stmt = *stmt_ptr;
    switch (stmt.kind) {
    case syntax::StmtKind::let:
    case syntax::StmtKind::var: {
        const bool has_declared_type = syntax::is_valid(stmt.declared_type);
        const TypeHandle declared_type = has_declared_type ? this->resolve_type(stmt.declared_type) : INVALID_TYPE_HANDLE;
        const TypeHandle init = this->analyze_expr(stmt.init, declared_type);
        const TypeHandle local_type = has_declared_type ? declared_type : init;
        this->record_stmt_local_type(stmt_id, local_type);
        if (!has_declared_type && !is_valid(local_type)) {
            this->report(stmt.range, std::string(SEMA_LOCAL_TYPE_INFER));
        }
        if (is_valid(local_type) && !this->is_valid_storage_type(local_type)) {
            this->report(stmt.range, std::string(SEMA_LOCAL_STORAGE));
        }
        if (has_declared_type && !this->can_assign(local_type, init, stmt.init)) {
            this->report(stmt.range, std::string(SEMA_INITIALIZER_TYPE_MISMATCH));
        }
        const auto inserted = this->symbols_.insert(Symbol {
            SymbolKind::local,
            std::string(stmt.name),
            {},
            syntax::INVALID_MODULE_ID,
            local_type,
            stmt.range,
            stmt.kind == syntax::StmtKind::var,
            syntax::Visibility::private_,
        }, this->diagnostics_);
        static_cast<void>(inserted);
        break;
    }
    case syntax::StmtKind::assign: {
        if (!this->is_writable_place(stmt.lhs)) {
            this->report(expr_range_or(this->module_, stmt.lhs, stmt.range), std::string(SEMA_ASSIGNMENT_LHS_WRITABLE));
        }
        const TypeHandle lhs = this->analyze_assignment_target(stmt.lhs);
        syntax::BinaryOp binary_op = syntax::BinaryOp::add;
        if (compound_assignment_binary_op(stmt.assign_op, binary_op)) {
            syntax::ExprNode binary;
            binary.kind = syntax::ExprKind::binary;
            binary.range = stmt.range;
            binary.binary_op = binary_op;
            binary.binary_lhs = stmt.lhs;
            binary.binary_rhs = stmt.rhs;
            const TypeHandle result = this->analyze_expr(syntax::INVALID_EXPR_ID, binary, lhs);
            if (!this->can_assign(lhs, result, stmt.rhs)) {
                this->report(stmt.range, std::string(SEMA_COMPOUND_ASSIGNMENT_TYPE_MISMATCH));
            }
        } else {
            const TypeHandle rhs = this->analyze_expr(stmt.rhs, lhs);
            if (!this->can_assign(lhs, rhs, stmt.rhs)) {
                this->report(stmt.range, std::string(SEMA_ASSIGNMENT_TYPE_MISMATCH));
            }
        }
        if (this->checked_.types.contains_array(lhs)) {
            this->report(stmt.range, std::string(SEMA_ARRAY_ASSIGNMENT_UNSUPPORTED));
        }
        break;
    }
    case syntax::StmtKind::if_: {
        const TypeHandle condition = this->analyze_expr(stmt.condition);
        if (!this->checked_.types.is_bool(condition)) {
            this->report(expr_range_or(this->module_, stmt.condition, stmt.range), std::string(SEMA_IF_CONDITION_BOOL));
        }
        if (syntax::is_valid(stmt.else_if)) {
            stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::statement, stmt.else_if});
        }
        if (syntax::is_valid(stmt.else_block)) {
            stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::scoped_block, stmt.else_block});
        }
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::scoped_block, stmt.then_block});
        break;
    }
    case syntax::StmtKind::while_: {
        const TypeHandle condition = this->analyze_expr(stmt.condition);
        if (!this->checked_.types.is_bool(condition)) {
            this->report(expr_range_or(this->module_, stmt.condition, stmt.range), std::string(SEMA_WHILE_CONDITION_BOOL));
        }
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::exit_loop});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::scoped_block, stmt.body});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::enter_loop});
        break;
    }
    case syntax::StmtKind::for_: {
        this->symbols_.push_scope();
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::pop_scope});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::exit_loop});
        if (syntax::is_valid(stmt.for_update)) {
            stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::statement, stmt.for_update});
        }
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::scoped_block, stmt.body});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::enter_loop});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::for_condition, stmt_id});
        if (syntax::is_valid(stmt.for_init)) {
            stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::statement, stmt.for_init});
        }
        break;
    }
    case syntax::StmtKind::for_range: {
        const TypeHandle range_type = this->analyze_for_range_bounds(stmt_id, stmt);
        this->symbols_.push_scope();
        this->define_for_range_local(stmt, range_type);
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::pop_scope});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::exit_loop});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::scoped_block, stmt.body});
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::enter_loop});
        break;
    }
    case syntax::StmtKind::return_: {
        const TypeHandle actual = syntax::is_valid(stmt.return_value)
            ? this->analyze_expr(stmt.return_value, expected_return)
            : this->checked_.types.builtin(BuiltinType::void_);
        if (return_inference != nullptr) {
            this->record_inferred_return(stmt_id, actual, *return_inference);
        } else if (is_valid(actual) &&
            is_valid(expected_return) &&
            !this->can_assign(expected_return, actual, stmt.return_value)) {
            this->report(stmt.range, std::string(SEMA_RETURN_TYPE_MISMATCH));
        }
        break;
    }
    case syntax::StmtKind::expr:
        if (syntax::is_valid(stmt.init) &&
            stmt.init.value < this->module_.exprs.size() &&
            !is_allowed_expression_statement(this->module_, stmt.init)) {
            this->report(this->module_.exprs[stmt.init.value].range, std::string(SEMA_EXPR_STMT_CALL_OR_TRY));
        }
        static_cast<void>(this->analyze_expr(stmt.init));
        break;
    case syntax::StmtKind::block:
        stack.push_back(StatementAnalysisAction {StatementAnalysisActionKind::scoped_block, stmt_id});
        break;
    case syntax::StmtKind::break_:
    case syntax::StmtKind::continue_:
        if (this->loop_depth_ == SEMA_NO_LOOP_DEPTH) {
            this->report(stmt.range, std::string(SEMA_BREAK_CONTINUE_IN_LOOP));
        }
        break;
    case syntax::StmtKind::defer:
        if (!syntax::is_valid(stmt.init) ||
            stmt.init.value >= this->module_.exprs.size() ||
            this->module_.exprs[stmt.init.value].kind != syntax::ExprKind::call) {
            this->report(stmt.range, std::string(SEMA_DEFER_CALL));
            break;
        }
        static_cast<void>(this->analyze_expr(stmt.init));
        break;
    }
}

bool SemanticAnalyzer::block_guarantees_return(const syntax::StmtId block_id) const {
    return evaluate_control_flow(this->module_, block_id, ControlFlowFrameKind::block, ControlFlowQuery::guarantees_return);
}

bool SemanticAnalyzer::stmt_guarantees_return(const syntax::StmtId stmt_id) const {
    return evaluate_control_flow(this->module_, stmt_id, ControlFlowFrameKind::statement, ControlFlowQuery::guarantees_return);
}

bool SemanticAnalyzer::block_may_fallthrough(const syntax::StmtId block_id) const {
    return evaluate_control_flow(this->module_, block_id, ControlFlowFrameKind::block, ControlFlowQuery::may_fallthrough);
}

bool SemanticAnalyzer::stmt_may_fallthrough(const syntax::StmtId stmt_id) const {
    return evaluate_control_flow(this->module_, stmt_id, ControlFlowFrameKind::statement, ControlFlowQuery::may_fallthrough);
}

void SemanticAnalyzer::record_inferred_return(
    const syntax::StmtId stmt_id,
    const TypeHandle actual,
    ReturnTypeInference& inference
) {
    inference.returns.push_back(stmt_id);
    if (!is_valid(actual)) {
        if (syntax::is_valid(stmt_id) && stmt_id.value < this->module_.stmts.size()) {
            const syntax::StmtNode& stmt = this->module_.stmts[stmt_id.value];
            this->report(stmt.range, std::string(SEMA_RETURN_TYPE_INFER));
        }
        return;
    }
    if (!is_valid(inference.inferred_type)) {
        inference.inferred_type = actual;
        return;
    }
    if (!this->checked_.types.same(inference.inferred_type, actual)) {
        if (syntax::is_valid(stmt_id) && stmt_id.value < this->module_.stmts.size()) {
            const syntax::StmtNode& stmt = this->module_.stmts[stmt_id.value];
            this->report(stmt.range, std::string(SEMA_INFERRED_RETURN_TYPE_MISMATCH));
        }
    }
}

void SemanticAnalyzer::finalize_inferred_return(
    const syntax::ItemNode& function,
    const std::string& key,
    ReturnTypeInference& inference
) {
    TypeHandle return_type = inference.inferred_type;
    if (inference.returns.empty()) {
        return_type = this->checked_.types.builtin(BuiltinType::void_);
    }
    if (!is_valid(return_type)) {
        return_type = this->checked_.types.builtin(BuiltinType::void_);
    }
    this->validate_function_return_type(function, return_type);
    if (const auto found = this->checked_.functions.find(key); found != this->checked_.functions.end()) {
        found->second.return_type = return_type;
    }
    if (const auto global = this->global_values_.find(key); global != this->global_values_.end()) {
        global->second.type = return_type;
    }
}

void SemanticAnalyzer::validate_function_return_type(const syntax::ItemNode& function, const TypeHandle return_type) {
    if (this->checked_.types.is_array(return_type)) {
        this->report(function.range, std::string(SEMA_ARRAY_RETURN_UNSUPPORTED));
    }
    if (this->checked_.types.contains_array(return_type)) {
        this->report(function.range, std::string(SEMA_ARRAY_STRUCT_RETURN_UNSUPPORTED));
    }
}

void SemanticAnalyzer::ensure_function_return_known(
    const FunctionSignature& signature,
    const base::SourceRange use_range
) {
    if (is_valid(signature.return_type) || signature.is_extern_c) {
        return;
    }
    const std::string key = signature.is_method
        ? this->method_key(signature.module, signature.method_owner_type, signature.name)
        : this->module_key(signature.module, signature.name);
    const FunctionBodyState state = this->function_body_states_.contains(key)
        ? this->function_body_states_.at(key)
        : FunctionBodyState::not_started;
    if (state == FunctionBodyState::analyzing) {
        this->report(use_range, std::string(SEMA_RECURSIVE_RETURN_INFER));
        return;
    }
    const auto item_found = this->function_definition_items_.find(key);
    if (item_found == this->function_definition_items_.end() ||
        !syntax::is_valid(item_found->second) ||
        item_found->second.value >= this->module_.items.size()) {
        this->report(use_range, std::string(SEMA_RETURN_TYPE_INFER));
        return;
    }
    this->analyze_function_body(this->module_.items[item_found->second.value]);
}

} // namespace aurex::sema
