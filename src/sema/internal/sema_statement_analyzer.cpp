#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <sema/internal/sema_statement_analyzer.hpp>

namespace aurex::sema {

SemanticAnalyzerCore::StatementAnalyzer::StatementAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

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

[[nodiscard]] std::optional<syntax::StmtNode> statement_node(const syntax::AstModule& module, const syntax::StmtId stmt)
{
    if (!syntax::is_valid(stmt) || stmt.value >= module.stmts.size()) {
        return std::nullopt;
    }
    return module.stmts[stmt.value];
}

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module, const syntax::ExprId expr, const base::SourceRange& fallback) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return fallback;
    }
    return module.exprs.range(expr.value);
}

[[nodiscard]] bool statement_binary_result_uses_operand_type(const syntax::BinaryOp op) noexcept
{
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

[[nodiscard]] bool statement_contextual_integer_expr(const syntax::AstModule& module, const syntax::ExprId candidate)
{
    std::vector<syntax::ExprId> pending;
    pending.push_back(candidate);
    while (!pending.empty()) {
        const syntax::ExprId current = pending.back();
        pending.pop_back();
        if (!syntax::is_valid(current) || current.value >= module.exprs.size()) {
            return false;
        }
        const syntax::ExprKind kind = module.exprs.kind(current.value);
        if (kind == syntax::ExprKind::integer_literal) {
            continue;
        }
        if (const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(current.value);
            kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::numeric_negate) {
            pending.push_back(unary->operand);
            continue;
        }
        if (const syntax::BinaryExprPayload* const binary = module.exprs.binary_payload(current.value);
            kind == syntax::ExprKind::binary && binary != nullptr
            && statement_binary_result_uses_operand_type(binary->op)) {
            pending.push_back(binary->lhs);
            pending.push_back(binary->rhs);
            continue;
        }
        return false;
    }
    return true;
}

[[nodiscard]] bool default_control_flow_result(const ControlFlowQuery query) noexcept
{
    return query == ControlFlowQuery::may_fallthrough;
}

[[nodiscard]] bool block_short_circuits(const ControlFlowQuery query, const bool child_result) noexcept
{
    return query == ControlFlowQuery::guarantees_return ? child_result : !child_result;
}

[[nodiscard]] bool if_then_short_circuits(const ControlFlowQuery query, const bool then_result) noexcept
{
    return query == ControlFlowQuery::guarantees_return ? !then_result : then_result;
}

[[nodiscard]] bool abrupt_stmt_result(const ControlFlowQuery query, const syntax::StmtKind kind) noexcept
{
    if (query == ControlFlowQuery::guarantees_return) {
        return kind == syntax::StmtKind::return_;
    }
    return false;
}

void finish_control_flow_frame(std::vector<ControlFlowFrame>& stack, const ControlFlowQuery query, const bool result,
    bool& has_result, bool& final_result)
{
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

void evaluate_control_flow_statement(const syntax::AstModule& module, std::vector<ControlFlowFrame>& stack,
    const ControlFlowQuery query, bool& has_result, bool& final_result)
{
    ControlFlowFrame& frame = stack.back();
    if (!syntax::is_valid(frame.stmt) || frame.stmt.value >= module.stmts.size()) {
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        return;
    }
    const syntax::StmtKind kind = module.stmts.kind(frame.stmt.value);
    switch (kind) {
        case syntax::StmtKind::return_:
        case syntax::StmtKind::break_:
        case syntax::StmtKind::continue_:
            finish_control_flow_frame(stack, query, abrupt_stmt_result(query, kind), has_result, final_result);
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

void evaluate_control_flow_block(const syntax::AstModule& module, std::vector<ControlFlowFrame>& stack,
    const ControlFlowQuery query, bool& has_result, bool& final_result)
{
    ControlFlowFrame& frame = stack.back();
    if (!syntax::is_valid(frame.stmt) || frame.stmt.value >= module.stmts.size()) {
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        return;
    }
    const syntax::AstArenaVector<syntax::StmtId>* const statements = module.stmts.block_statements(frame.stmt.value);
    if (statements == nullptr) {
        frame.kind = ControlFlowFrameKind::statement;
        return;
    }
    if (frame.next_child >= statements->size()) {
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        return;
    }
    stack.push_back(ControlFlowFrame{ControlFlowFrameKind::statement, (*statements)[frame.next_child]});
}

void evaluate_control_flow_if_statement(const syntax::AstModule& module, std::vector<ControlFlowFrame>& stack,
    const ControlFlowQuery query, bool& has_result, bool& final_result)
{
    ControlFlowFrame& frame = stack.back();
    const std::optional<syntax::StmtNode> node = statement_node(module, frame.stmt);
    if (!node.has_value() || node->kind != syntax::StmtKind::if_) {
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
        return;
    }
    if (frame.if_stage == ControlFlowIfStage::evaluate_then) {
        frame.if_stage = ControlFlowIfStage::after_then;
        stack.push_back(ControlFlowFrame{ControlFlowFrameKind::block, node->then_block});
        return;
    }
    if (frame.if_stage == ControlFlowIfStage::evaluate_alternate) {
        frame.if_stage = ControlFlowIfStage::after_alternate;
        if (syntax::is_valid(node->else_block)) {
            stack.push_back(ControlFlowFrame{ControlFlowFrameKind::block, node->else_block});
            return;
        }
        if (syntax::is_valid(node->else_if)) {
            stack.push_back(ControlFlowFrame{ControlFlowFrameKind::statement, node->else_if});
            return;
        }
        finish_control_flow_frame(stack, query, default_control_flow_result(query), has_result, final_result);
    }
}

[[nodiscard]] bool evaluate_control_flow(const syntax::AstModule& module, const syntax::StmtId stmt,
    const ControlFlowFrameKind root_kind, const ControlFlowQuery query)
{
    std::vector<ControlFlowFrame> stack;
    stack.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
    stack.push_back(ControlFlowFrame{root_kind, stmt});

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
    const syntax::AstModule& module, const syntax::ExprId expr_id) noexcept
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= module.exprs.size()) {
        return false;
    }
    const syntax::ExprKind kind = module.exprs.kind(expr_id.value);
    return kind == syntax::ExprKind::call || kind == syntax::ExprKind::try_expr
        || kind == syntax::ExprKind::unsafe_block;
}

[[nodiscard]] bool compound_assignment_binary_op(const syntax::AssignOp op, syntax::BinaryOp& binary_op) noexcept
{
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

struct SemanticAnalyzerCore::FunctionBodyContextScope {
    struct Config {
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        syntax::ItemId item = syntax::INVALID_ITEM_ID;
        TypeHandle return_type = INVALID_TYPE_HANDLE;
        ReturnTypeInference* return_inference = nullptr;
        int loop_depth = SEMA_NO_LOOP_DEPTH;
        int unsafe_context_depth = 0;
        SymbolTable symbols;
    };

    FunctionBodyContextScope(SemanticAnalyzerCore& analyzer, Config config)
        : analyzer(analyzer), previous_module(analyzer.state_.flow.current_module),
          previous_item(analyzer.state_.flow.current_item),
          previous_function_return_type(analyzer.state_.flow.current_function_return_type),
          previous_return_inference(analyzer.state_.flow.current_return_inference),
          previous_loop_depth(analyzer.state_.flow.loop_depth),
          previous_unsafe_context_depth(analyzer.state_.flow.unsafe_context_depth),
          previous_symbols(std::move(analyzer.state_.names.symbols))
    {
        this->analyzer.state_.flow.current_module = config.module;
        this->analyzer.state_.flow.current_item =
            syntax::is_valid(config.item) ? config.item : this->analyzer.state_.flow.current_item;
        this->analyzer.state_.flow.current_function_return_type = config.return_type;
        this->analyzer.state_.flow.current_return_inference = config.return_inference;
        this->analyzer.state_.flow.loop_depth = config.loop_depth;
        this->analyzer.state_.flow.unsafe_context_depth = config.unsafe_context_depth;
        this->analyzer.state_.names.symbols = std::move(config.symbols);
    }

    FunctionBodyContextScope(const FunctionBodyContextScope&) = delete;
    FunctionBodyContextScope& operator=(const FunctionBodyContextScope&) = delete;

    ~FunctionBodyContextScope()
    {
        this->analyzer.state_.flow.current_module = this->previous_module;
        this->analyzer.state_.flow.current_item = this->previous_item;
        this->analyzer.state_.flow.current_function_return_type = this->previous_function_return_type;
        this->analyzer.state_.flow.current_return_inference = this->previous_return_inference;
        this->analyzer.state_.flow.loop_depth = this->previous_loop_depth;
        this->analyzer.state_.flow.unsafe_context_depth = this->previous_unsafe_context_depth;
        this->analyzer.state_.names.symbols = std::move(this->previous_symbols);
    }

    SemanticAnalyzerCore& analyzer;
    syntax::ModuleId previous_module;
    syntax::ItemId previous_item;
    TypeHandle previous_function_return_type = INVALID_TYPE_HANDLE;
    ReturnTypeInference* previous_return_inference = nullptr;
    int previous_loop_depth = SEMA_NO_LOOP_DEPTH;
    int previous_unsafe_context_depth = 0;
    SymbolTable previous_symbols;
};

void SemanticAnalyzerCore::StatementAnalyzer::analyze_function_body(
    const syntax::ItemNode& function, const syntax::ItemId function_id)
{
    const FunctionLookupKey key = this->core_.function_key(function, function_id);
    const auto found = this->core_.state_.checked.functions.find(key);
    if (found == this->core_.state_.checked.functions.end()) {
        return;
    }
    this->core_.analyze_function_body_with_signature(
        function, key, found->second, this->core_.state_.functions.body_states[key]);
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_function_body_with_signature(const syntax::ItemNode& function,
    const FunctionLookupKey& key, const FunctionSignature& signature, FunctionBodyState& state)
{
    if (signature.has_conflict) {
        return;
    }
    if (state == FunctionBodyState::analyzing) {
        if (!is_valid(signature.return_type)) {
            this->core_.report_general(function.range, std::string(SEMA_RECURSIVE_RETURN_INFER));
        }
        return;
    }
    if (state == FunctionBodyState::analyzed) {
        return;
    }
    state = FunctionBodyState::analyzing;
    const bool infer_return_type = !syntax::is_valid(function.return_type);
    ReturnTypeInference return_inference;
    TypeHandle expected_return = signature.return_type;
    if (infer_return_type) {
        expected_return = INVALID_TYPE_HANDLE;
    }
    FunctionBodyContextScope context(this->core_,
        FunctionBodyContextScope::Config{
            .module = signature.module,
            .item = syntax::is_valid(signature.definition_item) ? signature.definition_item : signature.prototype_item,
            .return_type = expected_return,
            .return_inference = infer_return_type ? &return_inference : nullptr,
            .loop_depth = SEMA_NO_LOOP_DEPTH,
            .unsafe_context_depth = signature.is_unsafe ? this->core_.state_.flow.unsafe_context_depth + 1
                                                        : this->core_.state_.flow.unsafe_context_depth,
            .symbols = SymbolTable{},
        });

    this->core_.state_.names.symbols.push_scope(function.params.size());
    for (base::usize i = 0; i < function.params.size(); ++i) {
        const syntax::ParamDecl& param = function.params[i];
        const TypeHandle param_type =
            i < signature.param_types.size() ? signature.param_types[i] : this->core_.resolve_type(param.type);
        static_cast<void>(this->core_.can_define_local_name(param.name_id, param.name, param.range));
        const auto inserted = this->core_.state_.names.symbols.insert(
            Symbol{
                SymbolKind::parameter,
                this->core_.source_name_text(param.name_id, param.name),
                param.name_id,
                {},
                syntax::INVALID_MODULE_ID,
                param_type,
                param.range,
                false,
                syntax::Visibility::private_,
                {},
            },
            this->core_.ctx_.diagnostics);
        static_cast<void>(inserted);
    }
    this->core_.analyze_block(function.body, expected_return, infer_return_type ? &return_inference : nullptr);
    this->core_.analyze_body_moves(function, signature);
    this->core_.state_.names.symbols.pop_scope();
    if (infer_return_type) {
        this->core_.finalize_inferred_return(function, key, return_inference);
        if (is_valid(return_inference.inferred_type)
            && !this->core_.state_.checked.types.is_void(return_inference.inferred_type)
            && !this->core_.block_guarantees_return(function.body)) {
            this->core_.report_general(function.range, std::string(SEMA_NOT_ALL_PATHS_RETURN));
        }
    } else if (is_valid(expected_return) && !this->core_.state_.checked.types.is_void(expected_return)
        && !this->core_.block_guarantees_return(function.body)) {
        this->core_.report_general(function.range, std::string(SEMA_NOT_ALL_PATHS_RETURN));
    }
    state = FunctionBodyState::analyzed;
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_block(
    const syntax::StmtId block, const TypeHandle expected_return, ReturnTypeInference* const return_inference)
{
    this->core_.analyze_statement_tree(
        block, expected_return, return_inference, StatementAnalysisRootKind::scoped_block);
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_block_statements(
    const syntax::StmtId block, const TypeHandle expected_return, ReturnTypeInference* const return_inference)
{
    this->core_.analyze_statement_tree(
        block, expected_return, return_inference, StatementAnalysisRootKind::block_statements);
}

TypeHandle SemanticAnalyzerCore::StatementAnalyzer::analyze_assignment_target(const syntax::ExprId expr_id)
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->core_.ctx_.module.exprs.size()) {
        return INVALID_TYPE_HANDLE;
    }
    const syntax::NameExprPayload* const expr = this->core_.ctx_.module.exprs.name_payload(expr_id.value);
    if (expr == nullptr || !expr->scope_name.empty()) {
        return this->core_.analyze_expr(expr_id);
    }
    const base::SourceRange expr_range = this->core_.ctx_.module.exprs.range(expr_id.value);
    const Symbol* symbol = this->core_.find_symbol(expr->text_id, expr->text, expr_range);
    if (symbol == nullptr) {
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (symbol->kind == SymbolKind::function) {
        this->core_.record_expr_c_name(expr_id, symbol->c_name);
        return this->core_.record_expr_type(expr_id, this->core_.function_type_from_symbol(*symbol, expr_range));
    }
    this->core_.record_expr_c_name(expr_id, symbol->c_name);
    return this->core_.record_expr_type(expr_id, symbol->type);
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_stmt(
    const syntax::StmtId stmt_id, const TypeHandle expected_return, ReturnTypeInference* const return_inference)
{
    this->core_.analyze_statement_tree(
        stmt_id, expected_return, return_inference, StatementAnalysisRootKind::statement);
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_statement_tree(const syntax::StmtId root,
    const TypeHandle expected_return, ReturnTypeInference* const return_inference,
    const StatementAnalysisRootKind root_kind)
{
    std::vector<StatementAnalysisAction> stack;
    stack.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
    switch (root_kind) {
        case StatementAnalysisRootKind::statement:
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::statement, root});
            break;
        case StatementAnalysisRootKind::scoped_block:
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, root});
            break;
        case StatementAnalysisRootKind::block_statements:
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::block_statements, root});
            break;
    }
    while (!stack.empty()) {
        const StatementAnalysisAction action = stack.back();
        stack.pop_back();
        this->core_.analyze_statement_action(action, stack, expected_return, return_inference);
    }
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_statement_action(const StatementAnalysisAction& action,
    std::vector<StatementAnalysisAction>& stack, const TypeHandle expected_return,
    ReturnTypeInference* const return_inference)
{
    switch (action.kind) {
        case StatementAnalysisActionKind::statement:
            this->core_.analyze_statement_node(action.stmt, stack, expected_return, return_inference);
            break;
        case StatementAnalysisActionKind::scoped_block:
            this->core_.state_.names.symbols.push_scope();
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::pop_scope});
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::block_statements, action.stmt});
            break;
        case StatementAnalysisActionKind::pattern_scoped_block:
            this->core_.analyze_pattern_scoped_block(action.pattern, action.pattern_type, action.block, stack);
            break;
        case StatementAnalysisActionKind::local_pattern:
            this->core_.define_local_pattern(
                action.pattern, action.pattern_type, action.is_mutable, action.allow_refutable);
            break;
        case StatementAnalysisActionKind::block_statements:
            this->core_.analyze_statement_block(action.stmt, stack);
            break;
        case StatementAnalysisActionKind::pop_scope:
            this->core_.state_.names.symbols.pop_scope();
            break;
        case StatementAnalysisActionKind::enter_loop:
            ++this->core_.state_.flow.loop_depth;
            break;
        case StatementAnalysisActionKind::exit_loop:
            --this->core_.state_.flow.loop_depth;
            break;
        case StatementAnalysisActionKind::for_condition:
            this->core_.analyze_for_condition(action.stmt);
            break;
    }
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_statement_block(
    const syntax::StmtId block, std::vector<StatementAnalysisAction>& stack) const
{
    if (!syntax::is_valid(block) || block.value >= this->core_.ctx_.module.stmts.size()) {
        return;
    }
    const syntax::AstArenaVector<syntax::StmtId>* const statements =
        this->core_.ctx_.module.stmts.block_statements(block.value);
    if (statements == nullptr) {
        return;
    }
    for (base::usize i = statements->size(); i > 0; --i) {
        stack.push_back(StatementAnalysisAction{
            StatementAnalysisActionKind::statement,
            (*statements)[i - 1],
        });
    }
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_pattern_scoped_block(const syntax::PatternId pattern,
    const TypeHandle pattern_type, const syntax::StmtId block, std::vector<StatementAnalysisAction>& stack)
{
    std::vector<PatternBinding> bindings;
    static_cast<void>(this->core_.analyze_pattern(pattern, pattern_type, bindings));
    this->core_.state_.names.symbols.push_scope(bindings.size());
    this->core_.define_pattern_bindings(bindings, false);
    stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::pop_scope});
    stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::block_statements, block});
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_for_condition(const syntax::StmtId stmt_id)
{
    const std::optional<syntax::StmtNode> stmt = statement_node(this->core_.ctx_.module, stmt_id);
    if (!stmt.has_value() || stmt->kind != syntax::StmtKind::for_ || !syntax::is_valid(stmt->condition)) {
        return;
    }
    const TypeHandle condition = this->core_.analyze_expr(stmt->condition);
    if (!this->core_.state_.checked.types.is_bool(condition)) {
        this->core_.report_general(
            expr_range_or(this->core_.ctx_.module, stmt->condition, stmt->range), std::string(SEMA_FOR_CONDITION_BOOL));
    }
}

TypeHandle SemanticAnalyzerCore::StatementAnalyzer::analyze_for_range_bounds(
    const syntax::StmtId stmt_id, const syntax::StmtNode& stmt)
{
    if (!syntax::is_valid(stmt.range_end)) {
        this->core_.record_stmt_local_type(stmt_id, INVALID_TYPE_HANDLE);
        this->core_.report_general(stmt.range, std::string(SEMA_FOR_RANGE_ARITY));
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
        if (!statement_contextual_integer_expr(this->core_.ctx_.module, operands[i])) {
            operand_types[i] = this->core_.analyze_expr(operands[i]);
            range_type = operand_types[i];
            break;
        }
    }
    if (!is_valid(range_type) && !operands.empty()) {
        operand_types.front() = this->core_.analyze_expr(operands.front());
        range_type = operand_types.front();
    }
    for (base::usize i = 0; i < operands.size(); ++i) {
        if (!is_valid(operand_types[i])) {
            operand_types[i] = this->core_.analyze_expr(operands[i], range_type);
        }
    }

    const TypeHandle end = operand_types[end_index];
    const TypeHandle start = has_start ? operand_types[start_index] : end;
    const TypeHandle step = has_step ? operand_types[step_index] : range_type;

    if (syntax::is_valid(stmt.range_start) && !this->core_.state_.checked.types.is_integer(start)) {
        this->core_.report_general(expr_range_or(this->core_.ctx_.module, stmt.range_start, stmt.range),
            std::string(SEMA_RANGE_BOUNDS_INTEGER));
    }
    if (!this->core_.state_.checked.types.is_integer(end)) {
        this->core_.report_general(
            expr_range_or(this->core_.ctx_.module, stmt.range_end, stmt.range), std::string(SEMA_RANGE_BOUNDS_INTEGER));
    }
    if (has_step && !this->core_.state_.checked.types.is_integer(step)) {
        this->core_.report_general(
            expr_range_or(this->core_.ctx_.module, stmt.range_step, stmt.range), std::string(SEMA_RANGE_STEP_INTEGER));
    }
    const bool bounds_have_same_type =
        is_valid(start) && is_valid(end) && this->core_.state_.checked.types.same(start, end);
    if (is_valid(start) && is_valid(end) && !bounds_have_same_type) {
        this->core_.report_general(stmt.range, std::string(SEMA_RANGE_BOUNDS_SAME_TYPE));
    }
    if (has_step && bounds_have_same_type && this->core_.state_.checked.types.is_integer(start)
        && this->core_.state_.checked.types.is_integer(step) && !this->core_.state_.checked.types.same(start, step)) {
        this->core_.report_general(expr_range_or(this->core_.ctx_.module, stmt.range_step, stmt.range),
            std::string(SEMA_RANGE_STEP_SAME_TYPE));
    }

    const TypeHandle local_type = this->core_.state_.checked.types.is_integer(start)
        ? start
        : (this->core_.state_.checked.types.is_integer(end) ? end : step);
    this->core_.record_stmt_local_type(stmt_id, local_type);
    return local_type;
}

void SemanticAnalyzerCore::StatementAnalyzer::define_for_range_local(
    const syntax::StmtNode& stmt, const TypeHandle type)
{
    static_cast<void>(this->core_.can_define_local_name(stmt.name_id, stmt.name, stmt.range));
    const auto inserted = this->core_.state_.names.symbols.insert(
        Symbol{
            SymbolKind::local,
            this->core_.source_name_text(stmt.name_id, stmt.name),
            stmt.name_id,
            {},
            syntax::INVALID_MODULE_ID,
            type,
            stmt.range,
            false,
            syntax::Visibility::private_,
            {},
        },
        this->core_.ctx_.diagnostics);
    static_cast<void>(inserted);
}

void SemanticAnalyzerCore::StatementAnalyzer::define_local_pattern(
    const syntax::PatternId pattern_id, const TypeHandle type, const bool is_mutable, const bool allow_refutable)
{
    std::vector<PatternBinding> bindings;
    if (!this->core_.analyze_pattern(pattern_id, type, bindings) && !allow_refutable) {
        const syntax::PatternNode* pattern = this->core_.ctx_.module.patterns.ptr(pattern_id.value);
        this->core_.report_pattern(
            pattern == nullptr ? base::SourceRange{} : pattern->range, std::string(SEMA_LOCAL_PATTERN_REFUTABLE));
    }
    this->core_.define_pattern_bindings(bindings, is_mutable);
}

void SemanticAnalyzerCore::StatementAnalyzer::analyze_statement_node(const syntax::StmtId stmt_id,
    std::vector<StatementAnalysisAction>& stack, const TypeHandle expected_return,
    ReturnTypeInference* const return_inference)
{
    const std::optional<syntax::StmtNode> stmt_ptr = statement_node(this->core_.ctx_.module, stmt_id);
    if (!stmt_ptr.has_value()) {
        return;
    }
    const syntax::StmtNode& stmt = stmt_ptr.value();
    switch (stmt.kind) {
        case syntax::StmtKind::let:
        case syntax::StmtKind::var: {
            const bool has_declared_type = syntax::is_valid(stmt.declared_type);
            const TypeHandle declared_type =
                has_declared_type ? this->core_.resolve_type(stmt.declared_type) : INVALID_TYPE_HANDLE;
            const TypeHandle init = this->core_.analyze_expr(stmt.init, declared_type);
            const TypeHandle local_type = has_declared_type ? declared_type : init;
            this->core_.record_stmt_local_type(stmt_id, local_type);
            if (!has_declared_type && !is_valid(local_type)) {
                this->core_.report_general(stmt.range, std::string(SEMA_LOCAL_TYPE_INFER));
            }
            if (is_valid(local_type) && !this->core_.is_valid_storage_type(local_type)) {
                this->core_.report_general(stmt.range, std::string(SEMA_LOCAL_STORAGE));
            }
            if (has_declared_type && !this->core_.can_assign(local_type, init, stmt.init)) {
                this->core_.report_type_mismatch(
                    stmt.range, std::string(SEMA_INITIALIZER_TYPE_MISMATCH), local_type, init);
            }
            if (syntax::is_valid(stmt.pattern)) {
                if (syntax::is_valid(stmt.else_block)) {
                    if (this->core_.block_may_fallthrough(stmt.else_block)) {
                        this->core_.report_pattern(stmt.range, std::string(SEMA_LET_ELSE_FALLTHROUGH));
                    }
                    stack.push_back(StatementAnalysisAction{
                        StatementAnalysisActionKind::local_pattern,
                        syntax::INVALID_STMT_ID,
                        syntax::INVALID_STMT_ID,
                        stmt.pattern,
                        local_type,
                        stmt.kind == syntax::StmtKind::var,
                        true,
                    });
                    stack.push_back(
                        StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, stmt.else_block});
                    break;
                }
                this->core_.define_local_pattern(stmt.pattern, local_type, stmt.kind == syntax::StmtKind::var);
                break;
            }
            if (syntax::is_valid(stmt.else_block)) {
                this->core_.report_pattern(stmt.range, std::string(SEMA_LET_ELSE_PATTERN));
                if (this->core_.block_may_fallthrough(stmt.else_block)) {
                    this->core_.report_pattern(stmt.range, std::string(SEMA_LET_ELSE_FALLTHROUGH));
                }
                stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, stmt.else_block});
            }
            static_cast<void>(this->core_.can_define_local_name(stmt.name_id, stmt.name, stmt.range));
            const auto inserted = this->core_.state_.names.symbols.insert(
                Symbol{
                    SymbolKind::local,
                    this->core_.source_name_text(stmt.name_id, stmt.name),
                    stmt.name_id,
                    {},
                    syntax::INVALID_MODULE_ID,
                    local_type,
                    stmt.range,
                    stmt.kind == syntax::StmtKind::var,
                    syntax::Visibility::private_,
                    {},
                },
                this->core_.ctx_.diagnostics);
            static_cast<void>(inserted);
            break;
        }
        case syntax::StmtKind::assign: {
            const TypeHandle lhs = this->core_.analyze_assignment_target(stmt.lhs);
            if (!this->core_.is_writable_place(stmt.lhs)) {
                this->core_.report_general(expr_range_or(this->core_.ctx_.module, stmt.lhs, stmt.range),
                    std::string(SEMA_ASSIGNMENT_LHS_WRITABLE));
            }
            syntax::BinaryOp binary_op = syntax::BinaryOp::add;
            if (compound_assignment_binary_op(stmt.assign_op, binary_op)) {
                ExprView binary;
                binary.kind = syntax::ExprKind::binary;
                binary.range = stmt.range;
                binary.binary_op = binary_op;
                binary.binary_lhs = stmt.lhs;
                binary.binary_rhs = stmt.rhs;
                const TypeHandle result = this->core_.analyze_expr(syntax::INVALID_EXPR_ID, binary, lhs);
                if (!this->core_.can_assign(lhs, result, stmt.rhs)) {
                    this->core_.report_type_mismatch(
                        stmt.range, std::string(SEMA_COMPOUND_ASSIGNMENT_TYPE_MISMATCH), lhs, result);
                }
            } else {
                const TypeHandle rhs = this->core_.analyze_expr(stmt.rhs, lhs);
                if (!this->core_.can_assign(lhs, rhs, stmt.rhs)) {
                    this->core_.report_type_mismatch(stmt.range, std::string(SEMA_ASSIGNMENT_TYPE_MISMATCH), lhs, rhs);
                }
            }
            static_cast<void>(this->core_.check_m2_value_abi(lhs, ValueAbiContext::assignment, stmt.range));
            break;
        }
        case syntax::StmtKind::if_: {
            const TypeHandle condition = this->core_.analyze_expr(stmt.condition);
            if (!syntax::is_valid(stmt.pattern) && !this->core_.state_.checked.types.is_bool(condition)) {
                this->core_.report_general(expr_range_or(this->core_.ctx_.module, stmt.condition, stmt.range),
                    std::string(SEMA_IF_CONDITION_BOOL));
            }
            if (syntax::is_valid(stmt.else_if)) {
                stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::statement, stmt.else_if});
            }
            if (syntax::is_valid(stmt.else_block)) {
                stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, stmt.else_block});
            }
            if (syntax::is_valid(stmt.pattern)) {
                stack.push_back(StatementAnalysisAction{
                    StatementAnalysisActionKind::pattern_scoped_block,
                    syntax::INVALID_STMT_ID,
                    stmt.then_block,
                    stmt.pattern,
                    condition,
                });
            } else {
                stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, stmt.then_block});
            }
            break;
        }
        case syntax::StmtKind::while_: {
            const TypeHandle condition = this->core_.analyze_expr(stmt.condition);
            if (!syntax::is_valid(stmt.pattern) && !this->core_.state_.checked.types.is_bool(condition)) {
                this->core_.report_general(expr_range_or(this->core_.ctx_.module, stmt.condition, stmt.range),
                    std::string(SEMA_WHILE_CONDITION_BOOL));
            }
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::exit_loop});
            if (syntax::is_valid(stmt.pattern)) {
                stack.push_back(StatementAnalysisAction{
                    StatementAnalysisActionKind::pattern_scoped_block,
                    syntax::INVALID_STMT_ID,
                    stmt.body,
                    stmt.pattern,
                    condition,
                });
            } else {
                stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, stmt.body});
            }
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::enter_loop});
            break;
        }
        case syntax::StmtKind::for_: {
            this->core_.state_.names.symbols.push_scope(1);
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::pop_scope});
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::exit_loop});
            if (syntax::is_valid(stmt.for_update)) {
                stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::statement, stmt.for_update});
            }
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, stmt.body});
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::enter_loop});
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::for_condition, stmt_id});
            if (syntax::is_valid(stmt.for_init)) {
                stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::statement, stmt.for_init});
            }
            break;
        }
        case syntax::StmtKind::for_range: {
            const TypeHandle range_type = this->core_.analyze_for_range_bounds(stmt_id, stmt);
            this->core_.state_.names.symbols.push_scope(1);
            this->core_.define_for_range_local(stmt, range_type);
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::pop_scope});
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::exit_loop});
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, stmt.body});
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::enter_loop});
            break;
        }
        case syntax::StmtKind::return_: {
            const TypeHandle actual = syntax::is_valid(stmt.return_value)
                ? this->core_.analyze_expr(stmt.return_value, expected_return)
                : this->core_.state_.checked.types.builtin(BuiltinType::void_);
            if (return_inference != nullptr) {
                this->core_.record_inferred_return(stmt_id, actual, *return_inference);
            } else if (is_valid(actual) && is_valid(expected_return)
                && !this->core_.can_assign(expected_return, actual, stmt.return_value)) {
                this->core_.report_type_mismatch(
                    stmt.range, std::string(SEMA_RETURN_TYPE_MISMATCH), expected_return, actual);
            }
            break;
        }
        case syntax::StmtKind::expr:
            static_cast<void>(this->core_.analyze_expr(stmt.init));
            if (syntax::is_valid(stmt.init) && stmt.init.value < this->core_.ctx_.module.exprs.size()
                && !is_allowed_expression_statement(this->core_.ctx_.module, stmt.init)) {
                this->core_.report_general(
                    this->core_.ctx_.module.exprs.range(stmt.init.value), std::string(SEMA_EXPR_STMT_CALL_OR_TRY));
            }
            break;
        case syntax::StmtKind::block:
            stack.push_back(StatementAnalysisAction{StatementAnalysisActionKind::scoped_block, stmt_id});
            break;
        case syntax::StmtKind::break_:
        case syntax::StmtKind::continue_:
            if (this->core_.state_.flow.loop_depth == SEMA_NO_LOOP_DEPTH) {
                this->core_.report_general(stmt.range, std::string(SEMA_BREAK_CONTINUE_IN_LOOP));
            }
            break;
        case syntax::StmtKind::defer:
            static_cast<void>(this->core_.analyze_expr(stmt.init));
            if (!syntax::is_valid(stmt.init) || stmt.init.value >= this->core_.ctx_.module.exprs.size()
                || this->core_.ctx_.module.exprs.kind(stmt.init.value) != syntax::ExprKind::call) {
                this->core_.report_general(stmt.range, std::string(SEMA_DEFER_CALL));
                break;
            }
            break;
    }
}

bool SemanticAnalyzerCore::StatementAnalyzer::block_guarantees_return(const syntax::StmtId block_id) const
{
    return evaluate_control_flow(
        this->core_.ctx_.module, block_id, ControlFlowFrameKind::block, ControlFlowQuery::guarantees_return);
}

bool SemanticAnalyzerCore::StatementAnalyzer::stmt_guarantees_return(const syntax::StmtId stmt_id) const
{
    return evaluate_control_flow(
        this->core_.ctx_.module, stmt_id, ControlFlowFrameKind::statement, ControlFlowQuery::guarantees_return);
}

bool SemanticAnalyzerCore::StatementAnalyzer::block_may_fallthrough(const syntax::StmtId block_id) const
{
    return evaluate_control_flow(
        this->core_.ctx_.module, block_id, ControlFlowFrameKind::block, ControlFlowQuery::may_fallthrough);
}

bool SemanticAnalyzerCore::StatementAnalyzer::stmt_may_fallthrough(const syntax::StmtId stmt_id) const
{
    return evaluate_control_flow(
        this->core_.ctx_.module, stmt_id, ControlFlowFrameKind::statement, ControlFlowQuery::may_fallthrough);
}

void SemanticAnalyzerCore::StatementAnalyzer::record_inferred_return(
    const syntax::StmtId stmt_id, const TypeHandle actual, ReturnTypeInference& inference)
{
    inference.returns.push_back(stmt_id);
    if (!is_valid(actual)) {
        const std::optional<syntax::StmtNode> stmt = statement_node(this->core_.ctx_.module, stmt_id);
        if (stmt.has_value() && this->core_.is_null_result_expr(stmt->return_value)) {
            inference.pending_null_returns.push_back(stmt_id);
            return;
        }
        this->core_.report_return_inference_diagnostic(stmt_id, SEMA_RETURN_TYPE_INFER);
        return;
    }
    if (!is_valid(inference.inferred_type)) {
        inference.inferred_type = actual;
        return;
    }
    if (!this->core_.state_.checked.types.same(inference.inferred_type, actual)) {
        this->core_.report_return_inference_diagnostic(stmt_id, SEMA_INFERRED_RETURN_TYPE_MISMATCH);
    }
}

void SemanticAnalyzerCore::StatementAnalyzer::finalize_inferred_return(
    const syntax::ItemNode& function, const FunctionLookupKey& key, ReturnTypeInference& inference)
{
    this->core_.resolve_pending_null_returns(inference);
    TypeHandle return_type = inference.inferred_type;
    if (inference.returns.empty()) {
        return_type = this->core_.state_.checked.types.builtin(BuiltinType::void_);
    }
    if (!is_valid(return_type)) {
        return_type = this->core_.state_.checked.types.builtin(BuiltinType::void_);
    }
    this->core_.validate_function_return_type(function, return_type);
    if (const auto found = this->core_.state_.checked.functions.find(key);
        found != this->core_.state_.checked.functions.end()) {
        found->second.return_type = return_type;
        if (const auto global = this->core_.state_.functions.global_values.find(key);
            global != this->core_.state_.functions.global_values.end()) {
            global->second.type = this->core_.function_type_from_signature(found->second);
        }
    }
}

void SemanticAnalyzerCore::StatementAnalyzer::resolve_pending_null_returns(ReturnTypeInference& inference)
{
    if (inference.pending_null_returns.empty()) {
        return;
    }
    if (!is_valid(inference.inferred_type)) {
        this->core_.report_return_inference_diagnostic(inference.pending_null_returns.front(), SEMA_RETURN_TYPE_INFER);
        return;
    }
    if (!this->core_.state_.checked.types.is_pointer(inference.inferred_type)) {
        for (const syntax::StmtId stmt_id : inference.pending_null_returns) {
            this->core_.report_return_inference_diagnostic(stmt_id, SEMA_INFERRED_RETURN_TYPE_MISMATCH);
        }
        return;
    }
    for (const syntax::StmtId stmt_id : inference.pending_null_returns) {
        const std::optional<syntax::StmtNode> stmt = statement_node(this->core_.ctx_.module, stmt_id);
        if (!stmt.has_value() || !syntax::is_valid(stmt->return_value)) {
            continue;
        }
        const TypeHandle actual = this->core_.analyze_expr(stmt->return_value, inference.inferred_type);
        if (!is_valid(actual) || !this->core_.can_assign(inference.inferred_type, actual, stmt->return_value)) {
            this->core_.report_return_inference_diagnostic(stmt_id, SEMA_INFERRED_RETURN_TYPE_MISMATCH);
        }
    }
}

void SemanticAnalyzerCore::StatementAnalyzer::report_return_inference_diagnostic(
    const syntax::StmtId stmt_id, const std::string_view message) const
{
    const std::optional<syntax::StmtNode> stmt = statement_node(this->core_.ctx_.module, stmt_id);
    if (!stmt.has_value()) {
        return;
    }
    this->core_.report_general(stmt->range, std::string(message));
}

void SemanticAnalyzerCore::StatementAnalyzer::validate_function_return_type(
    const syntax::ItemNode& function, const TypeHandle return_type) const
{
    static_cast<void>(this->core_.check_m2_value_abi(return_type, ValueAbiContext::return_value, function.range));
}

void SemanticAnalyzerCore::StatementAnalyzer::ensure_function_return_known(
    const FunctionSignature& signature, const base::SourceRange& use_range)
{
    if (is_valid(signature.return_type) || signature.is_extern_c) {
        return;
    }
    FunctionLookupKey key = signature.semantic_key;
    if (!is_valid(key)) {
        key = signature.is_method
            ? this->core_.method_function_lookup_key(signature.module, signature.method_owner_type, signature.name_id)
            : this->core_.function_lookup_key(signature.module, signature.name_id);
    }
    const FunctionBodyState state = this->core_.state_.functions.body_states.contains(key)
        ? this->core_.state_.functions.body_states.at(key)
        : FunctionBodyState::not_started;
    if (state == FunctionBodyState::analyzing) {
        this->core_.report_general(use_range, std::string(SEMA_RECURSIVE_RETURN_INFER));
        return;
    }
    const auto item_found = this->core_.state_.functions.definition_items.find(key);
    if (item_found == this->core_.state_.functions.definition_items.end() || !syntax::is_valid(item_found->second)
        || item_found->second.value >= this->core_.ctx_.module.items.size()) {
        this->core_.report_general(use_range, std::string(SEMA_RETURN_TYPE_INFER));
        return;
    }
    this->core_.analyze_function_body(this->core_.ctx_.module.items[item_found->second.value], item_found->second);
}

void SemanticAnalyzerCore::analyze_function_body(const syntax::ItemNode& function, const syntax::ItemId function_id)
{
    StatementAnalyzer(*this).analyze_function_body(function, function_id);
}

void SemanticAnalyzerCore::analyze_function_body_with_signature(const syntax::ItemNode& function,
    const FunctionLookupKey& key, const FunctionSignature& signature, FunctionBodyState& state)
{
    StatementAnalyzer(*this).analyze_function_body_with_signature(function, key, signature, state);
}

void SemanticAnalyzerCore::analyze_block(
    const syntax::StmtId block, const TypeHandle expected_return, ReturnTypeInference* const return_inference)
{
    StatementAnalyzer(*this).analyze_block(block, expected_return, return_inference);
}

void SemanticAnalyzerCore::analyze_block_statements(
    const syntax::StmtId block, const TypeHandle expected_return, ReturnTypeInference* const return_inference)
{
    StatementAnalyzer(*this).analyze_block_statements(block, expected_return, return_inference);
}

TypeHandle SemanticAnalyzerCore::analyze_assignment_target(const syntax::ExprId expr_id)
{
    return StatementAnalyzer(*this).analyze_assignment_target(expr_id);
}

void SemanticAnalyzerCore::analyze_stmt(
    const syntax::StmtId stmt_id, const TypeHandle expected_return, ReturnTypeInference* const return_inference)
{
    StatementAnalyzer(*this).analyze_stmt(stmt_id, expected_return, return_inference);
}

void SemanticAnalyzerCore::analyze_statement_tree(const syntax::StmtId root, const TypeHandle expected_return,
    ReturnTypeInference* const return_inference, const StatementAnalysisRootKind root_kind)
{
    StatementAnalyzer(*this).analyze_statement_tree(root, expected_return, return_inference, root_kind);
}

void SemanticAnalyzerCore::analyze_statement_action(const StatementAnalysisAction& action,
    std::vector<StatementAnalysisAction>& stack, const TypeHandle expected_return,
    ReturnTypeInference* const return_inference)
{
    StatementAnalyzer(*this).analyze_statement_action(action, stack, expected_return, return_inference);
}

void SemanticAnalyzerCore::analyze_statement_block(
    const syntax::StmtId block, std::vector<StatementAnalysisAction>& stack) const
{
    StatementAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).analyze_statement_block(block, stack);
}

void SemanticAnalyzerCore::analyze_pattern_scoped_block(const syntax::PatternId pattern, const TypeHandle pattern_type,
    const syntax::StmtId block, std::vector<StatementAnalysisAction>& stack)
{
    StatementAnalyzer(*this).analyze_pattern_scoped_block(pattern, pattern_type, block, stack);
}

void SemanticAnalyzerCore::analyze_for_condition(const syntax::StmtId stmt_id)
{
    StatementAnalyzer(*this).analyze_for_condition(stmt_id);
}

TypeHandle SemanticAnalyzerCore::analyze_for_range_bounds(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt)
{
    return StatementAnalyzer(*this).analyze_for_range_bounds(stmt_id, stmt);
}

void SemanticAnalyzerCore::define_for_range_local(const syntax::StmtNode& stmt, const TypeHandle type)
{
    StatementAnalyzer(*this).define_for_range_local(stmt, type);
}

void SemanticAnalyzerCore::define_local_pattern(
    const syntax::PatternId pattern_id, const TypeHandle type, const bool is_mutable, const bool allow_refutable)
{
    StatementAnalyzer(*this).define_local_pattern(pattern_id, type, is_mutable, allow_refutable);
}

void SemanticAnalyzerCore::analyze_statement_node(const syntax::StmtId stmt_id,
    std::vector<StatementAnalysisAction>& stack, const TypeHandle expected_return,
    ReturnTypeInference* const return_inference)
{
    StatementAnalyzer(*this).analyze_statement_node(stmt_id, stack, expected_return, return_inference);
}

bool SemanticAnalyzerCore::block_guarantees_return(const syntax::StmtId block_id) const
{
    return StatementAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).block_guarantees_return(block_id);
}

bool SemanticAnalyzerCore::stmt_guarantees_return(const syntax::StmtId stmt_id) const
{
    return StatementAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).stmt_guarantees_return(stmt_id);
}

bool SemanticAnalyzerCore::block_may_fallthrough(const syntax::StmtId block_id) const
{
    return StatementAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).block_may_fallthrough(block_id);
}

bool SemanticAnalyzerCore::stmt_may_fallthrough(const syntax::StmtId stmt_id) const
{
    return StatementAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).stmt_may_fallthrough(stmt_id);
}

void SemanticAnalyzerCore::record_inferred_return(
    const syntax::StmtId stmt_id, const TypeHandle actual, ReturnTypeInference& inference)
{
    StatementAnalyzer(*this).record_inferred_return(stmt_id, actual, inference);
}

void SemanticAnalyzerCore::finalize_inferred_return(
    const syntax::ItemNode& function, const FunctionLookupKey& key, ReturnTypeInference& inference)
{
    StatementAnalyzer(*this).finalize_inferred_return(function, key, inference);
}

void SemanticAnalyzerCore::resolve_pending_null_returns(ReturnTypeInference& inference)
{
    StatementAnalyzer(*this).resolve_pending_null_returns(inference);
}

void SemanticAnalyzerCore::report_return_inference_diagnostic(
    const syntax::StmtId stmt_id, const std::string_view message) const
{
    StatementAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).report_return_inference_diagnostic(stmt_id, message);
}

void SemanticAnalyzerCore::validate_function_return_type(
    const syntax::ItemNode& function, const TypeHandle return_type) const
{
    StatementAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).validate_function_return_type(function, return_type);
}

void SemanticAnalyzerCore::ensure_function_return_known(
    const FunctionSignature& signature, const base::SourceRange& use_range)
{
    StatementAnalyzer(*this).ensure_function_return_known(signature, use_range);
}

} // namespace aurex::sema
