#include <aurex/infrastructure/base/integer.hpp>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frontend/sema/internal/borrow/private/flow_graph.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_BODY_FLOW_POINT_ID_CONTEXT = "sema body flow point id";
constexpr std::string_view SEMA_BODY_FLOW_PLACE_ID_CONTEXT = "sema body flow place id";
constexpr base::usize SEMA_BODY_FLOW_INITIAL_TASK_CAPACITY = 64;

enum class BodyFlowTaskKind : base::u8 {
    statement,
    expression,
};

enum class BodyFlowExprContext : base::u8 {
    value,
    branch,
    place_observe,
    place_write,
};

struct BodyFlowTask {
    BodyFlowTaskKind kind = BodyFlowTaskKind::statement;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    base::u32 start = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 continuation = SEMA_BODY_FLOW_INVALID_INDEX;
    BodyFlowExprContext expr_context = BodyFlowExprContext::value;
};

struct BodyFlowExpressionStep {
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    BodyFlowExprContext context = BodyFlowExprContext::value;
};

struct BodyFlowPatternCleanupFrame {
    syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
};

[[nodiscard]] bool valid_stmt(const syntax::AstModule& module, const syntax::StmtId stmt) noexcept
{
    return syntax::is_valid(stmt) && stmt.value < module.stmts.size();
}

[[nodiscard]] bool valid_expr(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size();
}

[[nodiscard]] bool valid_pattern(const syntax::AstModule& module, const syntax::PatternId pattern) noexcept
{
    return syntax::is_valid(pattern) && pattern.value < module.patterns.size();
}

[[nodiscard]] base::SourceRange stmt_range(const syntax::AstModule& module, const syntax::StmtId stmt) noexcept
{
    return module.stmts.range(stmt.value);
}

[[nodiscard]] base::SourceRange expr_range(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return valid_expr(module, expr) ? module.exprs.range(expr.value) : base::SourceRange{};
}

[[nodiscard]] bool unary_is_address_of(const syntax::UnaryOp op) noexcept
{
    return op == syntax::UnaryOp::address_of || op == syntax::UnaryOp::address_of_mut;
}

[[nodiscard]] bool expr_is_place_like(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    switch (module.exprs.kind(expr.value)) {
        case syntax::ExprKind::name:
        case syntax::ExprKind::field:
        case syntax::ExprKind::index:
        case syntax::ExprKind::slice:
            return true;
        case syntax::ExprKind::unary: {
            const syntax::UnaryExprPayload& unary = *module.exprs.unary_payload(expr.value);
            return unary.op == syntax::UnaryOp::dereference;
        }
        default:
            return false;
    }
}

class BodyFlowGraphBuilder final {
public:
    BodyFlowGraphBuilder(const syntax::AstModule& module, const CheckedModule& checked,
        const FunctionLookupKey function, const syntax::StmtId body)
        : module_(module), checked_(checked)
    {
        this->graph_.function = function;
        this->graph_.body = body;
        this->tasks_.reserve(SEMA_BODY_FLOW_INITIAL_TASK_CAPACITY);
    }

    [[nodiscard]] BodyFlowGraph build()
    {
        const base::u32 entry = this->new_point(
            BodyFlowPointKind::entry, syntax::INVALID_STMT_ID, syntax::INVALID_EXPR_ID, base::SourceRange{});
        const base::u32 exit = this->new_point(
            BodyFlowPointKind::exit, syntax::INVALID_STMT_ID, syntax::INVALID_EXPR_ID, base::SourceRange{});
        if (valid_stmt(this->module_, this->graph_.body)) {
            this->push_statement(this->graph_.body, entry, exit);
            this->run_tasks();
        } else {
            this->add_edge(entry, exit);
        }
        return std::move(this->graph_);
    }

private:
    void run_tasks()
    {
        while (!this->tasks_.empty()) {
            const BodyFlowTask task = this->tasks_.back();
            this->tasks_.pop_back();
            switch (task.kind) {
                case BodyFlowTaskKind::statement:
                    this->process_statement(task);
                    break;
                case BodyFlowTaskKind::expression:
                    this->process_expression(task);
                    break;
            }
        }
    }

    [[nodiscard]] base::u32 new_point(const BodyFlowPointKind kind, const syntax::StmtId stmt,
        const syntax::ExprId expr, const base::SourceRange& range)
    {
        const base::u32 index = base::checked_u32(this->graph_.points.size(), SEMA_BODY_FLOW_POINT_ID_CONTEXT);
        this->graph_.points.push_back(BodyFlowPoint{
            .kind = kind,
            .stmt = stmt,
            .expr = expr,
            .range = range,
        });
        return index;
    }

    [[nodiscard]] base::u32 new_sequence_point(const base::SourceRange& range)
    {
        return this->new_point(BodyFlowPointKind::sequence, syntax::INVALID_STMT_ID, syntax::INVALID_EXPR_ID, range);
    }

    [[nodiscard]] base::u32 new_cleanup_point(const syntax::StmtId stmt, const base::SourceRange& range)
    {
        return this->new_point(BodyFlowPointKind::cleanup_scope, stmt, syntax::INVALID_EXPR_ID, range);
    }

    void add_edge(const base::u32 from, const base::u32 to)
    {
        this->graph_.edges.push_back(BodyFlowEdge{
            .from = from,
            .to = to,
        });
    }

    [[nodiscard]] base::u32 add_place(BodyFlowPlace place)
    {
        const base::u32 index = base::checked_u32(this->graph_.places.size(), SEMA_BODY_FLOW_PLACE_ID_CONTEXT);
        this->graph_.places.push_back(std::move(place));
        return index;
    }

    void add_action(const BodyFlowActionKind kind, const base::u32 point, const base::u32 place,
        const syntax::StmtId stmt, const syntax::ExprId expr, const base::SourceRange& range)
    {
        this->graph_.actions.push_back(BodyFlowAction{
            .kind = kind,
            .point = point,
            .place = place,
            .stmt = stmt,
            .expr = expr,
            .range = range,
        });
    }

    void add_point_action(const BodyFlowActionKind kind, const base::u32 point, const syntax::StmtId stmt,
        const syntax::ExprId expr, const base::SourceRange& range)
    {
        this->add_action(kind, point, SEMA_BODY_FLOW_INVALID_INDEX, stmt, expr, range);
    }

    void add_place_action(
        const BodyFlowActionKind kind, const base::u32 point, const syntax::StmtId stmt, const syntax::ExprId expr)
    {
        const base::u32 place = this->add_place(this->make_place(expr));
        this->add_action(kind, point, place, stmt, expr, expr_range(this->module_, expr));
    }

    void add_local_storage_action(const BodyFlowActionKind kind, const base::u32 point, const syntax::StmtId stmt,
        const IdentId name_id, const base::SourceRange& range)
    {
        if (!syntax::is_valid(name_id)) {
            return;
        }
        BodyFlowPlace place;
        place.root_kind = BodyFlowPlaceRootKind::local;
        place.root_name_id = name_id;
        place.range = range;
        const base::u32 place_id = this->add_place(std::move(place));
        this->add_action(kind, point, place_id, stmt, syntax::INVALID_EXPR_ID, range);
    }

    void push_statement(const syntax::StmtId stmt, const base::u32 start, const base::u32 continuation)
    {
        if (!valid_stmt(this->module_, stmt)) {
            this->add_edge(start, continuation);
            return;
        }
        this->tasks_.push_back(BodyFlowTask{
            .kind = BodyFlowTaskKind::statement,
            .stmt = stmt,
            .start = start,
            .continuation = continuation,
        });
    }

    void push_expression(const syntax::ExprId expr, const base::u32 start, const base::u32 continuation,
        const BodyFlowExprContext context)
    {
        if (!valid_expr(this->module_, expr)) {
            this->add_edge(start, continuation);
            return;
        }
        this->tasks_.push_back(BodyFlowTask{
            .kind = BodyFlowTaskKind::expression,
            .expr = expr,
            .start = start,
            .continuation = continuation,
            .expr_context = context,
        });
    }

    void push_statement_sequence(
        const std::vector<syntax::StmtId>& statements, const base::u32 start, const base::u32 continuation)
    {
        std::vector<syntax::StmtId> valid_statements;
        valid_statements.reserve(statements.size());
        for (const syntax::StmtId stmt : statements) {
            if (valid_stmt(this->module_, stmt)) {
                valid_statements.push_back(stmt);
            }
        }
        if (valid_statements.empty()) {
            this->add_edge(start, continuation);
            return;
        }

        std::vector<base::u32> boundaries;
        boundaries.reserve(valid_statements.size());
        for (base::usize index = 1; index < valid_statements.size(); ++index) {
            boundaries.push_back(this->new_sequence_point(stmt_range(this->module_, valid_statements[index])));
        }

        for (base::usize reverse_index = valid_statements.size(); reverse_index > 0; --reverse_index) {
            const base::usize index = reverse_index - 1;
            const base::u32 item_start = index == 0 ? start : boundaries[index - 1];
            const base::u32 item_continuation = index + 1 == valid_statements.size() ? continuation : boundaries[index];
            this->push_statement(valid_statements[index], item_start, item_continuation);
        }
    }

    void push_expression_sequence(
        const std::vector<BodyFlowExpressionStep>& steps, const base::u32 start, const base::u32 continuation)
    {
        std::vector<BodyFlowExpressionStep> valid_steps;
        valid_steps.reserve(steps.size());
        for (const BodyFlowExpressionStep& step : steps) {
            if (valid_expr(this->module_, step.expr)) {
                valid_steps.push_back(step);
            }
        }
        if (valid_steps.empty()) {
            this->add_edge(start, continuation);
            return;
        }

        std::vector<base::u32> boundaries;
        boundaries.reserve(valid_steps.size());
        for (base::usize index = 1; index < valid_steps.size(); ++index) {
            boundaries.push_back(this->new_sequence_point(expr_range(this->module_, valid_steps[index].expr)));
        }

        for (base::usize reverse_index = valid_steps.size(); reverse_index > 0; --reverse_index) {
            const base::usize index = reverse_index - 1;
            const base::u32 item_start = index == 0 ? start : boundaries[index - 1];
            const base::u32 item_continuation = index + 1 == valid_steps.size() ? continuation : boundaries[index];
            this->push_expression(valid_steps[index].expr, item_start, item_continuation, valid_steps[index].context);
        }
    }

    void process_statement(const BodyFlowTask& task)
    {
        const syntax::StmtNode stmt = this->module_.stmts[task.stmt.value];
        const base::SourceRange range = stmt_range(this->module_, task.stmt);
        const base::u32 entry =
            this->new_point(BodyFlowPointKind::statement_entry, task.stmt, syntax::INVALID_EXPR_ID, range);
        const base::u32 exit =
            this->new_point(BodyFlowPointKind::statement_exit, task.stmt, syntax::INVALID_EXPR_ID, range);
        this->add_edge(task.start, entry);
        this->add_edge(exit, task.continuation);

        switch (stmt.kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                this->process_local_statement(task.stmt, stmt, entry, exit);
                break;
            case syntax::StmtKind::assign:
                this->process_assignment_statement(task.stmt, stmt, entry, exit);
                break;
            case syntax::StmtKind::if_:
                this->process_if_statement(stmt, entry, exit, range);
                break;
            case syntax::StmtKind::for_:
                this->process_for_statement(stmt, entry, exit, range);
                break;
            case syntax::StmtKind::for_range:
                this->process_for_range_statement(stmt, entry, exit, range);
                break;
            case syntax::StmtKind::while_:
                this->process_while_statement(stmt, entry, exit, range);
                break;
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
                this->add_point_action(BodyFlowActionKind::branch, exit, task.stmt, syntax::INVALID_EXPR_ID, range);
                this->add_edge(entry, exit);
                break;
            case syntax::StmtKind::defer:
                this->process_defer_statement(task.stmt, stmt, entry, exit, range);
                break;
            case syntax::StmtKind::return_:
                this->process_return_statement(task.stmt, stmt, entry, exit, range);
                break;
            case syntax::StmtKind::expr:
                this->push_expression(stmt.init, entry, exit, BodyFlowExprContext::value);
                break;
            case syntax::StmtKind::block:
                this->add_point_action(
                    BodyFlowActionKind::cleanup_scope, exit, task.stmt, syntax::INVALID_EXPR_ID, range);
                this->add_block_cleanup_storage_actions(stmt, task.stmt, exit);
                this->push_statement_sequence(stmt.statements, entry, exit);
                break;
        }
    }

    void process_local_statement(
        const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit)
    {
        if (syntax::is_valid(stmt.name_id)) {
            this->add_local_storage_action(BodyFlowActionKind::write, exit, stmt_id, stmt.name_id, stmt.range);
        }
        if (syntax::is_valid(stmt.init)) {
            this->push_expression(stmt.init, entry, exit, BodyFlowExprContext::value);
        } else {
            this->add_edge(entry, exit);
        }
    }

    void process_assignment_statement(
        const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit)
    {
        const base::u32 lhs_done = this->new_sequence_point(stmt.range);
        const base::u32 rhs_done = this->new_sequence_point(stmt.range);
        this->push_expression(stmt.lhs, entry, lhs_done, BodyFlowExprContext::place_observe);
        this->push_expression(stmt.rhs, lhs_done, rhs_done, BodyFlowExprContext::value);
        if (valid_expr(this->module_, stmt.lhs)) {
            const BodyFlowActionKind write_kind =
                this->expr_is_direct_local_name(stmt.lhs) ? BodyFlowActionKind::reinit : BodyFlowActionKind::write;
            this->add_place_action(write_kind, exit, stmt_id, stmt.lhs);
        }
        this->add_edge(rhs_done, exit);
    }

    void add_block_cleanup_storage_actions(
        const syntax::StmtNode& block, const syntax::StmtId block_stmt, const base::u32 cleanup_point)
    {
        for (const syntax::StmtId stmt_id : block.statements) {
            if (!valid_stmt(this->module_, stmt_id)) {
                continue;
            }
            const syntax::StmtNode stmt = this->module_.stmts[stmt_id.value];
            if (stmt.kind != syntax::StmtKind::let && stmt.kind != syntax::StmtKind::var) {
                continue;
            }
            if (syntax::is_valid(stmt.pattern)) {
                this->add_pattern_cleanup_storage_actions(stmt.pattern, cleanup_point, stmt_id);
                continue;
            }
            this->add_local_storage_action(
                BodyFlowActionKind::cleanup_storage, cleanup_point, block_stmt, stmt.name_id, stmt.range);
        }
    }

    void add_pattern_cleanup_storage_actions(
        const syntax::PatternId pattern, const base::u32 cleanup_point, const syntax::StmtId stmt_id)
    {
        std::vector<BodyFlowPatternCleanupFrame> pending;
        pending.push_back(BodyFlowPatternCleanupFrame{
            .pattern = pattern,
            .point = cleanup_point,
            .stmt = stmt_id,
        });
        while (!pending.empty()) {
            const BodyFlowPatternCleanupFrame frame = pending.back();
            pending.pop_back();
            if (!valid_pattern(this->module_, frame.pattern)) {
                continue;
            }
            const syntax::PatternNode* const node = this->module_.patterns.ptr(frame.pattern.value);
            if (node == nullptr) {
                continue;
            }
            switch (node->kind) {
                case syntax::PatternKind::binding:
                    this->add_local_storage_action(BodyFlowActionKind::cleanup_storage, frame.point, frame.stmt,
                        node->binding_name_id, node->range);
                    break;
                case syntax::PatternKind::tuple:
                case syntax::PatternKind::slice:
                    for (const syntax::PatternId element : node->elements) {
                        pending.push_back(BodyFlowPatternCleanupFrame{element, frame.point, frame.stmt});
                    }
                    break;
                case syntax::PatternKind::struct_:
                    for (const syntax::FieldPattern& field : node->field_patterns) {
                        pending.push_back(BodyFlowPatternCleanupFrame{field.pattern, frame.point, frame.stmt});
                    }
                    break;
                case syntax::PatternKind::enum_case:
                    for (const syntax::PatternId payload : node->payload_patterns) {
                        pending.push_back(BodyFlowPatternCleanupFrame{payload, frame.point, frame.stmt});
                    }
                    break;
                case syntax::PatternKind::or_pattern:
                    for (const syntax::PatternId alternative : node->alternatives) {
                        pending.push_back(BodyFlowPatternCleanupFrame{alternative, frame.point, frame.stmt});
                    }
                    break;
                case syntax::PatternKind::wildcard:
                case syntax::PatternKind::literal:
                case syntax::PatternKind::const_:
                    break;
            }
        }
    }

    void process_if_statement(
        const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit, const base::SourceRange& range)
    {
        const base::u32 condition_done = this->new_sequence_point(range);
        this->push_expression(stmt.condition, entry, condition_done, BodyFlowExprContext::branch);
        if (valid_stmt(this->module_, stmt.then_block)) {
            this->push_statement(stmt.then_block, condition_done, exit);
        }
        if (valid_stmt(this->module_, stmt.else_block)) {
            this->push_statement(stmt.else_block, condition_done, exit);
        }
        if (valid_stmt(this->module_, stmt.else_if)) {
            this->push_statement(stmt.else_if, condition_done, exit);
        }
        if (!valid_stmt(this->module_, stmt.then_block) && !valid_stmt(this->module_, stmt.else_block)
            && !valid_stmt(this->module_, stmt.else_if)) {
            this->add_edge(condition_done, exit);
        } else if (!valid_stmt(this->module_, stmt.else_block) && !valid_stmt(this->module_, stmt.else_if)) {
            this->add_edge(condition_done, exit);
        }
    }

    void process_for_statement(
        const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit, const base::SourceRange& range)
    {
        const base::u32 init_done = this->new_sequence_point(range);
        const base::u32 condition_done = this->new_sequence_point(range);
        const base::u32 body_done = this->new_sequence_point(range);
        this->push_statement(stmt.for_init, entry, init_done);
        this->push_expression(stmt.condition, init_done, condition_done, BodyFlowExprContext::branch);
        this->add_edge(condition_done, exit);
        this->push_statement(stmt.body, condition_done, body_done);
        this->push_statement(stmt.for_update, body_done, init_done);
    }

    void process_for_range_statement(
        const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit, const base::SourceRange& range)
    {
        const base::u32 range_done = this->new_sequence_point(range);
        this->push_expression_sequence(
            {
                BodyFlowExpressionStep{stmt.range_start, BodyFlowExprContext::value},
                BodyFlowExpressionStep{stmt.range_end, BodyFlowExprContext::value},
                BodyFlowExpressionStep{stmt.range_step, BodyFlowExprContext::value},
            },
            entry, range_done);
        this->add_point_action(
            BodyFlowActionKind::branch, range_done, syntax::INVALID_STMT_ID, syntax::INVALID_EXPR_ID, range);
        this->add_edge(range_done, exit);
        this->push_statement(stmt.body, range_done, range_done);
    }

    void process_while_statement(
        const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit, const base::SourceRange& range)
    {
        const base::u32 condition_done = this->new_sequence_point(range);
        this->push_expression(stmt.condition, entry, condition_done, BodyFlowExprContext::branch);
        this->add_edge(condition_done, exit);
        this->push_statement(stmt.body, condition_done, entry);
    }

    void process_defer_statement(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry,
        const base::u32 exit, const base::SourceRange& range)
    {
        const base::u32 cleanup = this->new_cleanup_point(stmt_id, range);
        this->add_point_action(BodyFlowActionKind::cleanup_scope, cleanup, stmt_id, stmt.init, range);
        this->push_expression(stmt.init, entry, cleanup, BodyFlowExprContext::value);
        this->add_edge(cleanup, exit);
    }

    void process_return_statement(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry,
        const base::u32 exit, const base::SourceRange& range)
    {
        const base::u32 return_point = this->new_sequence_point(range);
        this->push_expression(stmt.return_value, entry, return_point, BodyFlowExprContext::value);
        this->add_point_action(BodyFlowActionKind::return_, return_point, stmt_id, stmt.return_value, range);
        this->add_edge(return_point, exit);
    }

    void process_expression(const BodyFlowTask& task)
    {
        const syntax::ExprKind kind = this->module_.exprs.kind(task.expr.value);
        const base::SourceRange range = expr_range(this->module_, task.expr);
        const base::u32 entry =
            this->new_point(BodyFlowPointKind::expression_entry, syntax::INVALID_STMT_ID, task.expr, range);
        const base::u32 exit =
            this->new_point(BodyFlowPointKind::expression_exit, syntax::INVALID_STMT_ID, task.expr, range);
        this->add_edge(task.start, entry);
        this->add_edge(exit, task.continuation);

        if (task.expr_context == BodyFlowExprContext::branch) {
            this->add_point_action(BodyFlowActionKind::branch, exit, syntax::INVALID_STMT_ID, task.expr, range);
        }
        if (this->record_place_context_action(task.expr, task.expr_context, exit)) {
            this->push_place_operands(task.expr, entry, exit);
            return;
        }

        if (const syntax::UnaryExprPayload* const unary = this->module_.exprs.unary_payload(task.expr.value);
            kind == syntax::ExprKind::unary && unary != nullptr && unary_is_address_of(unary->op)) {
            const BodyFlowActionKind action = unary->op == syntax::UnaryOp::address_of_mut
                ? BodyFlowActionKind::borrow_mutable
                : BodyFlowActionKind::borrow_shared;
            this->add_place_action(action, exit, syntax::INVALID_STMT_ID, unary->operand);
            this->push_place_operands(unary->operand, entry, exit);
            return;
        }

        if (task.expr_context != BodyFlowExprContext::place_observe) {
            this->record_value_place_actions(task.expr, exit);
        }
        if (task.expr_context == BodyFlowExprContext::value || task.expr_context == BodyFlowExprContext::branch) {
            this->record_borrowed_view_result_actions(task.expr, kind, exit);
        }
        this->push_expression_children(task.expr, kind, entry, exit);
    }

    [[nodiscard]] bool record_place_context_action(
        const syntax::ExprId expr, const BodyFlowExprContext context, const base::u32 point)
    {
        switch (context) {
            case BodyFlowExprContext::place_write:
                this->add_place_action(BodyFlowActionKind::write, point, syntax::INVALID_STMT_ID, expr);
                return true;
            case BodyFlowExprContext::place_observe:
                return expr_is_place_like(this->module_, expr);
            case BodyFlowExprContext::branch:
            case BodyFlowExprContext::value:
                return false;
        }
        return false;
    }

    void record_value_place_actions(const syntax::ExprId expr, const base::u32 point)
    {
        if (!expr_is_place_like(this->module_, expr)) {
            return;
        }
        this->add_place_action(BodyFlowActionKind::read, point, syntax::INVALID_STMT_ID, expr);
        this->add_place_action(BodyFlowActionKind::move_candidate, point, syntax::INVALID_STMT_ID, expr);
    }

    void push_place_operands(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        if (!valid_expr(this->module_, expr)) {
            this->add_edge(entry, exit);
            return;
        }
        switch (this->module_.exprs.kind(expr.value)) {
            case syntax::ExprKind::field: {
                const syntax::FieldExprPayload& field = *this->module_.exprs.field_payload(expr.value);
                this->push_expression(field.object, entry, exit, BodyFlowExprContext::place_observe);
                return;
            }
            case syntax::ExprKind::index: {
                const syntax::IndexExprPayload& index = *this->module_.exprs.index_payload(expr.value);
                this->push_expression_sequence(
                    {
                        BodyFlowExpressionStep{index.object, BodyFlowExprContext::place_observe},
                        BodyFlowExpressionStep{index.index, BodyFlowExprContext::value},
                    },
                    entry, exit);
                return;
            }
            case syntax::ExprKind::slice: {
                const syntax::SliceExprPayload& slice = *this->module_.exprs.slice_payload(expr.value);
                this->push_expression_sequence(
                    {
                        BodyFlowExpressionStep{slice.object, BodyFlowExprContext::place_observe},
                        BodyFlowExpressionStep{slice.start, BodyFlowExprContext::value},
                        BodyFlowExpressionStep{slice.end, BodyFlowExprContext::value},
                    },
                    entry, exit);
                return;
            }
            case syntax::ExprKind::unary: {
                const syntax::UnaryExprPayload& unary = *this->module_.exprs.unary_payload(expr.value);
                if (unary.op == syntax::UnaryOp::dereference) {
                    this->push_expression(unary.operand, entry, exit, BodyFlowExprContext::value);
                    return;
                }
                break;
            }
            default:
                break;
        }
        this->add_edge(entry, exit);
    }

    void push_expression_children(
        const syntax::ExprId expr, const syntax::ExprKind kind, const base::u32 entry, const base::u32 exit)
    {
        if (expr_is_place_like(this->module_, expr)) {
            this->push_place_operands(expr, entry, exit);
            return;
        }

        switch (kind) {
            case syntax::ExprKind::generic_apply:
                this->push_generic_apply_children(expr, entry, exit);
                break;
            case syntax::ExprKind::unary:
                this->push_unary_children(expr, entry, exit);
                break;
            case syntax::ExprKind::try_expr:
                this->push_try_children(expr, entry, exit);
                break;
            case syntax::ExprKind::binary:
                this->push_binary_children(expr, entry, exit);
                break;
            case syntax::ExprKind::call:
            case syntax::ExprKind::str_from_bytes_unchecked:
                this->push_call_children(expr, entry, exit);
                break;
            case syntax::ExprKind::if_expr:
                this->push_if_expression_children(expr, entry, exit);
                break;
            case syntax::ExprKind::block_expr:
            case syntax::ExprKind::unsafe_block:
                this->push_block_expression_children(expr, entry, exit);
                break;
            case syntax::ExprKind::match_expr:
                this->push_match_expression_children(expr, entry, exit);
                break;
            case syntax::ExprKind::array_literal:
                this->push_array_children(expr, entry, exit);
                break;
            case syntax::ExprKind::tuple_literal:
                this->push_tuple_children(expr, entry, exit);
                break;
            case syntax::ExprKind::struct_literal:
                this->push_struct_literal_children(expr, entry, exit);
                break;
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
                this->push_cast_like_children(expr, entry, exit);
                break;
            case syntax::ExprKind::invalid:
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
            case syntax::ExprKind::name:
            case syntax::ExprKind::field:
            case syntax::ExprKind::index:
            case syntax::ExprKind::slice:
                this->add_edge(entry, exit);
                break;
        }
    }

    void push_generic_apply_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::GenericApplyExprPayload& payload = *this->module_.exprs.generic_apply_payload(expr.value);
        this->push_expression(payload.callee, entry, exit, BodyFlowExprContext::value);
    }

    void push_unary_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::UnaryExprPayload& payload = *this->module_.exprs.unary_payload(expr.value);
        this->push_expression(payload.operand, entry, exit, BodyFlowExprContext::value);
    }

    void push_try_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::TryExprPayload& payload = *this->module_.exprs.try_payload(expr.value);
        this->push_expression(payload.operand, entry, exit, BodyFlowExprContext::value);
    }

    void push_binary_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::BinaryExprPayload& payload = *this->module_.exprs.binary_payload(expr.value);
        this->push_expression_sequence(
            {
                BodyFlowExpressionStep{payload.lhs, BodyFlowExprContext::value},
                BodyFlowExpressionStep{payload.rhs, BodyFlowExprContext::value},
            },
            entry, exit);
    }

    void push_call_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::CallExprPayload& payload = *this->module_.exprs.call_payload(expr.value);
        this->add_point_action(
            BodyFlowActionKind::call, exit, syntax::INVALID_STMT_ID, expr, expr_range(this->module_, expr));
        const std::optional<syntax::ExprId> receiver = this->receiver_expr_for_call_callee(payload.callee);
        if (this->call_has_two_phase_receiver(expr) && receiver.has_value() && valid_expr(this->module_, *receiver)) {
            const base::SourceRange receiver_range = expr_range(this->module_, *receiver);
            const base::u32 receiver_done = this->new_sequence_point(receiver_range);
            const base::u32 receiver_place = this->add_place(this->make_place(*receiver));
            this->add_action(BodyFlowActionKind::call_receiver_reserve, receiver_done, receiver_place,
                syntax::INVALID_STMT_ID, expr, receiver_range);
            this->add_action(BodyFlowActionKind::call_receiver_activate, exit, receiver_place, syntax::INVALID_STMT_ID,
                expr, receiver_range);
            this->record_call_return_borrow_actions(expr, payload, exit);
            this->push_expression(payload.callee, entry, receiver_done, BodyFlowExprContext::value);
            std::vector<BodyFlowExpressionStep> arg_steps;
            arg_steps.reserve(payload.args.size());
            for (const syntax::ExprId arg : payload.args) {
                arg_steps.push_back(BodyFlowExpressionStep{arg, BodyFlowExprContext::value});
            }
            this->push_expression_sequence(arg_steps, receiver_done, exit);
            return;
        }
        this->record_call_return_borrow_actions(expr, payload, exit);
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(payload.args.size() + 1);
        steps.push_back(BodyFlowExpressionStep{payload.callee, BodyFlowExprContext::value});
        for (const syntax::ExprId arg : payload.args) {
            steps.push_back(BodyFlowExpressionStep{arg, BodyFlowExprContext::value});
        }
        this->push_expression_sequence(steps, entry, exit);
    }

    [[nodiscard]] bool call_has_two_phase_receiver(const syntax::ExprId call) const noexcept
    {
        if (const FunctionCallBinding* const binding = this->checked_.function_call_binding_for_expr(call);
            binding != nullptr && binding->receiver_two_phase_eligible) {
            return true;
        }
        if (const TraitMethodCallBinding* const binding = this->checked_.trait_method_call_binding_for_expr(call);
            binding != nullptr && binding->receiver_two_phase_eligible) {
            return true;
        }
        return false;
    }

    void record_borrowed_view_result_actions(
        const syntax::ExprId expr, const syntax::ExprKind kind, const base::u32 point)
    {
        if (const syntax::SliceExprPayload* const slice = this->module_.exprs.slice_payload(expr.value);
            kind == syntax::ExprKind::slice && slice != nullptr) {
            this->add_return_borrow_action(this->borrow_action_for_result(expr), point, expr, slice->object);
            return;
        }
        if (const syntax::CastExprPayload* const cast = this->module_.exprs.cast_payload(expr.value);
            kind == syntax::ExprKind::str_from_utf8_checked && cast != nullptr) {
            this->add_return_borrow_action(BodyFlowActionKind::borrow_shared, point, expr, cast->expr);
        }
    }

    void record_call_return_borrow_actions(
        const syntax::ExprId expr, const syntax::CallExprPayload& payload, const base::u32 point)
    {
        if (this->module_.exprs.kind(expr.value) != syntax::ExprKind::call) {
            return;
        }
        if (const FunctionCallBinding* const binding = this->checked_.function_call_binding_for_expr(expr);
            binding != nullptr) {
            if (const auto contract = this->checked_.borrow_contracts.find(binding->function_key);
                contract != this->checked_.borrow_contracts.end()
                && contract->second.source != FunctionBorrowContractSource::inferred) {
                this->record_contract_return_borrow_actions(
                    contract->second, payload, binding->callee_expr, binding->receiver_arg_count, expr, point);
                return;
            }
            if (const auto summary = this->checked_.borrow_summaries.find(binding->function_key);
                summary != this->checked_.borrow_summaries.end()) {
                this->record_summary_return_borrow_actions(
                    summary->second, payload, binding->callee_expr, binding->receiver_arg_count, expr, point);
                return;
            }
            if (const auto contract = this->checked_.borrow_contracts.find(binding->function_key);
                contract != this->checked_.borrow_contracts.end()) {
                this->record_contract_return_borrow_actions(
                    contract->second, payload, binding->callee_expr, binding->receiver_arg_count, expr, point);
                return;
            }
            if (this->type_can_contain_borrow(binding->return_type)) {
                this->record_unknown_return_borrow_actions(
                    payload, binding->callee_expr, binding->receiver_arg_count, expr, point);
            }
            return;
        }
        if (const TraitMethodCallBinding* const binding = this->checked_.trait_method_call_binding_for_expr(expr);
            binding != nullptr) {
            const base::u32 receiver_arg_count = is_valid(binding->receiver_type) ? 1U : 0U;
            if (const auto contract = this->checked_.borrow_contracts.find(binding->function_key);
                contract != this->checked_.borrow_contracts.end()
                && contract->second.source != FunctionBorrowContractSource::inferred) {
                this->record_contract_return_borrow_actions(
                    contract->second, payload, binding->callee_expr, receiver_arg_count, expr, point);
                return;
            }
            if (const auto summary = this->checked_.borrow_summaries.find(binding->function_key);
                summary != this->checked_.borrow_summaries.end()) {
                this->record_summary_return_borrow_actions(
                    summary->second, payload, binding->callee_expr, receiver_arg_count, expr, point);
                return;
            }
            if (const auto contract = this->checked_.borrow_contracts.find(binding->function_key);
                contract != this->checked_.borrow_contracts.end()) {
                this->record_contract_return_borrow_actions(
                    contract->second, payload, binding->callee_expr, receiver_arg_count, expr, point);
                return;
            }
            if (this->type_can_contain_borrow(binding->return_type)) {
                this->record_unknown_return_borrow_actions(
                    payload, binding->callee_expr, receiver_arg_count, expr, point);
            }
        }
    }

    void record_summary_return_borrow_actions(const FunctionBorrowSummary& summary,
        const syntax::CallExprPayload& payload, const syntax::ExprId callee, const base::u32 receiver_arg_count,
        const syntax::ExprId call_expr, const base::u32 point)
    {
        bool recorded_unknown = false;
        const auto record_unknown = [&]() {
            if (recorded_unknown) {
                return;
            }
            this->record_unknown_return_borrow_actions(payload, callee, receiver_arg_count, call_expr, point);
            recorded_unknown = true;
        };
        if (summary.has_unknown_return_origin) {
            record_unknown();
        }
        for (const FunctionBorrowReturnOrigin& dependency : summary.return_origins) {
            if (dependency.origin_index >= summary.origins.size()) {
                record_unknown();
                continue;
            }
            const BorrowSummaryOrigin& origin = summary.origins[dependency.origin_index];
            if (origin.kind == BorrowSummaryOriginKind::parameter) {
                this->record_param_return_borrow_action(
                    payload, callee, receiver_arg_count, origin.param_index, call_expr, point);
            } else if (origin.kind != BorrowSummaryOriginKind::static_) {
                record_unknown();
            }
        }
    }

    void record_contract_return_borrow_actions(const FunctionBorrowContract& contract,
        const syntax::CallExprPayload& payload, const syntax::ExprId callee, const base::u32 receiver_arg_count,
        const syntax::ExprId call_expr, const base::u32 point)
    {
        if (contract.unknown_return_allowed) {
            this->record_unknown_return_borrow_actions(payload, callee, receiver_arg_count, call_expr, point);
            return;
        }
        for (const BorrowContractSelector& selector : contract.return_selectors) {
            switch (selector.kind) {
                case BorrowContractSelectorKind::parameter:
                case BorrowContractSelectorKind::self:
                    this->record_param_return_borrow_action(
                        payload, callee, receiver_arg_count, selector.param_index, call_expr, point);
                    break;
                case BorrowContractSelectorKind::static_:
                    break;
                case BorrowContractSelectorKind::unknown:
                    this->record_unknown_return_borrow_actions(payload, callee, receiver_arg_count, call_expr, point);
                    return;
            }
        }
    }

    void record_param_return_borrow_action(const syntax::CallExprPayload& payload, const syntax::ExprId callee,
        const base::u32 receiver_arg_count, const base::u32 param_index, const syntax::ExprId call_expr,
        const base::u32 point)
    {
        if (param_index == SEMA_BORROW_SUMMARY_INVALID_INDEX) {
            return;
        }
        const syntax::ExprId source = this->call_argument_for_param(payload, callee, receiver_arg_count, param_index);
        this->add_return_borrow_action(this->borrow_action_for_result(call_expr), point, call_expr, source);
    }

    void record_unknown_return_borrow_actions(const syntax::CallExprPayload& payload, const syntax::ExprId callee,
        const base::u32 receiver_arg_count, const syntax::ExprId call_expr, const base::u32 point)
    {
        this->add_unknown_return_borrow_action(this->borrow_action_for_result(call_expr), point, call_expr);
        std::unordered_set<base::u32> recorded_sources;
        const base::u32 parameter_count =
            receiver_arg_count + base::checked_u32(payload.args.size(), SEMA_BODY_FLOW_PLACE_ID_CONTEXT);
        for (base::u32 param_index = 0; param_index < parameter_count; ++param_index) {
            const syntax::ExprId source =
                this->call_argument_for_param(payload, callee, receiver_arg_count, param_index);
            if (!this->expr_can_be_conservative_return_source(source)
                || !recorded_sources.insert(source.value).second) {
                continue;
            }
            this->add_return_borrow_action(this->borrow_action_for_result(call_expr), point, call_expr, source);
        }
    }

    [[nodiscard]] bool expr_can_be_conservative_return_source(const syntax::ExprId expr) const
    {
        if (!valid_expr(this->module_, expr)) {
            return false;
        }
        if (this->address_of_operand(expr).has_value()) {
            return true;
        }
        return this->type_can_contain_borrow(this->cached_expr_type(expr));
    }

    void add_unknown_return_borrow_action(
        const BodyFlowActionKind kind, const base::u32 point, const syntax::ExprId result_expr)
    {
        if (!valid_expr(this->module_, result_expr)) {
            return;
        }
        BodyFlowPlace place;
        place.root_kind = BodyFlowPlaceRootKind::unknown;
        place.root_expr = result_expr;
        place.range = expr_range(this->module_, result_expr);
        const base::u32 place_id = this->add_place(std::move(place));
        this->add_action(
            kind, point, place_id, syntax::INVALID_STMT_ID, result_expr, expr_range(this->module_, result_expr));
    }

    [[nodiscard]] syntax::ExprId call_argument_for_param(const syntax::CallExprPayload& payload,
        const syntax::ExprId callee, const base::u32 receiver_arg_count, const base::u32 param_index) const
    {
        if (receiver_arg_count != 0 && param_index < receiver_arg_count) {
            const std::optional<syntax::ExprId> receiver = this->receiver_expr_for_call_callee(callee);
            return receiver.has_value() ? *receiver : syntax::INVALID_EXPR_ID;
        }
        const base::u32 arg_index = param_index - receiver_arg_count;
        return arg_index < payload.args.size() ? payload.args[arg_index] : syntax::INVALID_EXPR_ID;
    }

    void add_return_borrow_action(const BodyFlowActionKind kind, const base::u32 point,
        const syntax::ExprId result_expr, const syntax::ExprId source_expr)
    {
        if (!valid_expr(this->module_, result_expr) || !valid_expr(this->module_, source_expr)) {
            return;
        }
        const base::u32 place = this->add_place(this->make_borrow_source_place(source_expr));
        this->add_action(
            kind, point, place, syntax::INVALID_STMT_ID, result_expr, expr_range(this->module_, result_expr));
    }

    [[nodiscard]] BodyFlowActionKind borrow_action_for_result(const syntax::ExprId expr) const noexcept
    {
        const TypeHandle type = this->cached_expr_type(expr);
        if (!is_valid(type) || type.value >= this->checked_.types.size()) {
            return BodyFlowActionKind::borrow_shared;
        }
        const TypeInfo& info = this->checked_.types.get(type);
        if ((info.kind == TypeKind::reference && info.pointer_mutability == PointerMutability::mut)
            || (info.kind == TypeKind::slice && info.slice_mutability == PointerMutability::mut)) {
            return BodyFlowActionKind::borrow_mutable;
        }
        return BodyFlowActionKind::borrow_shared;
    }

    [[nodiscard]] BodyFlowPlace make_borrow_source_place(const syntax::ExprId source_expr)
    {
        if (const std::optional<syntax::ExprId> operand = this->address_of_operand(source_expr); operand.has_value()) {
            return this->make_place(*operand);
        }
        BodyFlowPlace place = this->make_place(source_expr);
        if (this->expr_is_indirect_borrow_carrier(source_expr)) {
            place.projections.push_back(BodyFlowPlaceProjection{
                .kind = BodyFlowPlaceProjectionKind::dereference,
                .expr = source_expr,
            });
        }
        return place;
    }

    [[nodiscard]] bool expr_is_indirect_borrow_carrier(const syntax::ExprId expr) const noexcept
    {
        const TypeHandle type = this->cached_expr_type(expr);
        if (!is_valid(type) || type.value >= this->checked_.types.size()) {
            return false;
        }
        const TypeInfo& info = this->checked_.types.get(type);
        return info.kind == TypeKind::reference || info.kind == TypeKind::slice
            || (info.kind == TypeKind::builtin && info.builtin == BuiltinType::str);
    }

    [[nodiscard]] TypeHandle cached_expr_type(const syntax::ExprId expr) const noexcept
    {
        return valid_expr(this->module_, expr) && expr.value < this->checked_.expr_types.size()
            ? this->checked_.expr_types[expr.value]
            : INVALID_TYPE_HANDLE;
    }

    [[nodiscard]] const StructInfo* find_struct(const TypeHandle type) const noexcept
    {
        for (const auto& entry : this->checked_.structs) {
            if (entry.second.type.value == type.value) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool type_can_contain_borrow(const TypeHandle type) const
    {
        if (!is_valid(type)) {
            return false;
        }
        std::vector<TypeHandle> pending{type};
        std::unordered_set<base::u32> visited;
        while (!pending.empty()) {
            const TypeHandle current = pending.back();
            pending.pop_back();
            if (!is_valid(current) || current.value >= this->checked_.types.size()
                || !visited.insert(current.value).second) {
                continue;
            }
            const TypeInfo& info = this->checked_.types.get(current);
            switch (info.kind) {
                case TypeKind::builtin:
                    if (info.builtin == BuiltinType::str) {
                        return true;
                    }
                    break;
                case TypeKind::reference:
                case TypeKind::slice:
                case TypeKind::generic_param:
                case TypeKind::associated_projection:
                    return true;
                case TypeKind::array:
                    pending.push_back(info.array_element);
                    break;
                case TypeKind::tuple:
                    pending.insert(pending.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                    break;
                case TypeKind::struct_: {
                    const StructInfo* const structure = this->find_struct(current);
                    if (structure != nullptr) {
                        for (const StructFieldInfo& field : structure->fields) {
                            pending.push_back(field.type);
                        }
                    }
                    break;
                }
                case TypeKind::enum_: {
                    for (const auto& entry : this->checked_.enum_cases) {
                        const EnumCaseInfo& enum_case = entry.second;
                        if (enum_case.type.value == current.value) {
                            pending.insert(
                                pending.end(), enum_case.payload_types.begin(), enum_case.payload_types.end());
                        }
                    }
                    break;
                }
                case TypeKind::pointer:
                case TypeKind::function:
                case TypeKind::opaque_struct:
                    break;
            }
        }
        return false;
    }

    [[nodiscard]] std::optional<syntax::ExprId> address_of_operand(const syntax::ExprId expr) const noexcept
    {
        if (!valid_expr(this->module_, expr) || this->module_.exprs.kind(expr.value) != syntax::ExprKind::unary) {
            return std::nullopt;
        }
        const syntax::UnaryExprPayload* const unary = this->module_.exprs.unary_payload(expr.value);
        if (unary == nullptr || !unary_is_address_of(unary->op)) {
            return std::nullopt;
        }
        return unary->operand;
    }

    [[nodiscard]] std::optional<syntax::ExprId> receiver_expr_for_call_callee(const syntax::ExprId callee) const
    {
        syntax::ExprId current = callee;
        std::vector<base::u32> visited;
        while (valid_expr(this->module_, current)) {
            if (std::ranges::find(visited, current.value) != visited.end()) {
                return std::nullopt;
            }
            visited.push_back(current.value);
            const syntax::ExprKind kind = this->module_.exprs.kind(current.value);
            if (kind == syntax::ExprKind::generic_apply) {
                const syntax::GenericApplyExprPayload* const apply =
                    this->module_.exprs.generic_apply_payload(current.value);
                if (apply == nullptr) {
                    return std::nullopt;
                }
                current = apply->callee;
                continue;
            }
            if (kind == syntax::ExprKind::field) {
                const syntax::FieldExprPayload* const field = this->module_.exprs.field_payload(current.value);
                return field == nullptr ? std::nullopt : std::optional<syntax::ExprId>{field->object};
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    void push_if_expression_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::IfExprPayload& payload = *this->module_.exprs.if_payload(expr.value);
        const base::u32 condition_done = this->new_sequence_point(expr_range(this->module_, expr));
        this->push_expression(payload.condition, entry, condition_done, BodyFlowExprContext::branch);
        this->push_expression(payload.then_expr, condition_done, exit, BodyFlowExprContext::value);
        this->push_expression(payload.else_expr, condition_done, exit, BodyFlowExprContext::value);
    }

    void push_block_expression_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::BlockExprPayload& payload = *this->module_.exprs.block_payload(expr.value);
        const base::u32 block_done = this->new_sequence_point(expr_range(this->module_, expr));
        this->push_statement(payload.block, entry, block_done);
        this->push_expression(payload.result, block_done, exit, BodyFlowExprContext::value);
    }

    void push_match_expression_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::MatchExprPayload& payload = *this->module_.exprs.match_payload(expr.value);
        const base::u32 value_done = this->new_sequence_point(expr_range(this->module_, expr));
        this->push_expression(payload.value, entry, value_done, BodyFlowExprContext::branch);
        if (payload.arms.empty()) {
            this->add_edge(value_done, exit);
            return;
        }
        for (const syntax::MatchArm& arm : payload.arms) {
            const base::u32 guard_done = this->new_sequence_point(arm.range);
            this->push_expression(arm.guard, value_done, guard_done, BodyFlowExprContext::branch);
            this->push_expression(arm.value, guard_done, exit, BodyFlowExprContext::value);
        }
    }

    void push_array_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::ArrayExprPayload& payload = *this->module_.exprs.array_payload(expr.value);
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(payload.elements.size() + 2);
        for (const syntax::ExprId element : payload.elements) {
            steps.push_back(BodyFlowExpressionStep{element, BodyFlowExprContext::value});
        }
        steps.push_back(BodyFlowExpressionStep{payload.repeat_value, BodyFlowExprContext::value});
        steps.push_back(BodyFlowExpressionStep{payload.repeat_count, BodyFlowExprContext::value});
        this->push_expression_sequence(steps, entry, exit);
    }

    void push_tuple_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::AstArenaVector<syntax::ExprId>& elements = *this->module_.exprs.tuple_elements(expr.value);
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(elements.size());
        for (const syntax::ExprId element : elements) {
            steps.push_back(BodyFlowExpressionStep{element, BodyFlowExprContext::value});
        }
        this->push_expression_sequence(steps, entry, exit);
    }

    void push_struct_literal_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::StructLiteralExprPayload& payload = *this->module_.exprs.struct_literal_payload(expr.value);
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(payload.field_inits.size() + 1);
        steps.push_back(BodyFlowExpressionStep{payload.object, BodyFlowExprContext::place_observe});
        for (const syntax::FieldInit& field : payload.field_inits) {
            steps.push_back(BodyFlowExpressionStep{field.value, BodyFlowExprContext::value});
        }
        this->push_expression_sequence(steps, entry, exit);
    }

    void push_cast_like_children(const syntax::ExprId expr, const base::u32 entry, const base::u32 exit)
    {
        const syntax::CastExprPayload& payload = *this->module_.exprs.cast_payload(expr.value);
        this->push_expression(payload.expr, entry, exit, BodyFlowExprContext::value);
    }

    [[nodiscard]] BodyFlowPlace make_place(const syntax::ExprId expr)
    {
        BodyFlowPlace place;
        place.root_kind = BodyFlowPlaceRootKind::unknown;
        place.root_expr = expr;
        place.range = expr_range(this->module_, expr);

        syntax::ExprId current = expr;
        std::vector<BodyFlowPlaceProjection> reversed_projections;
        while (valid_expr(this->module_, current)) {
            switch (this->module_.exprs.kind(current.value)) {
                case syntax::ExprKind::name:
                    this->finish_name_place(current, place, reversed_projections);
                    return place;
                case syntax::ExprKind::field:
                    current = this->extend_field_place(current, reversed_projections);
                    break;
                case syntax::ExprKind::index:
                    current = this->extend_index_place(current, reversed_projections);
                    break;
                case syntax::ExprKind::slice:
                    current = this->extend_slice_place(current, reversed_projections);
                    break;
                case syntax::ExprKind::unary: {
                    const std::optional<syntax::ExprId> next = this->extend_deref_place(current, reversed_projections);
                    if (!next.has_value()) {
                        return place;
                    }
                    current = next.value();
                    break;
                }
                default:
                    place.root_kind = BodyFlowPlaceRootKind::temporary;
                    place.root_expr = current;
                    std::ranges::reverse(reversed_projections);
                    place.projections = std::move(reversed_projections);
                    return place;
            }
        }

        std::ranges::reverse(reversed_projections);
        place.projections = std::move(reversed_projections);
        return place;
    }

    [[nodiscard]] bool expr_is_direct_local_name(const syntax::ExprId expr) const noexcept
    {
        if (!valid_expr(this->module_, expr) || this->module_.exprs.kind(expr.value) != syntax::ExprKind::name) {
            return false;
        }
        const syntax::NameExprPayload* const name = this->module_.exprs.name_payload(expr.value);
        return name != nullptr && name->scope_name.empty();
    }

    void finish_name_place(
        const syntax::ExprId expr, BodyFlowPlace& place, std::vector<BodyFlowPlaceProjection>& reversed_projections)
    {
        const syntax::NameExprPayload& payload = *this->module_.exprs.name_payload(expr.value);
        place.root_kind = BodyFlowPlaceRootKind::local;
        place.root_expr = expr;
        place.root_name_id = this->resolve_name_id(payload.text_id, payload.text);
        if (!payload.scope_name.empty()) {
            place.root_kind = BodyFlowPlaceRootKind::unknown;
        }
        std::ranges::reverse(reversed_projections);
        place.projections = std::move(reversed_projections);
    }

    [[nodiscard]] syntax::ExprId extend_field_place(
        const syntax::ExprId expr, std::vector<BodyFlowPlaceProjection>& reversed_projections)
    {
        const syntax::FieldExprPayload& payload = *this->module_.exprs.field_payload(expr.value);
        reversed_projections.push_back(BodyFlowPlaceProjection{
            .kind = BodyFlowPlaceProjectionKind::field,
            .field_name_id = this->resolve_name_id(payload.field_name_id, payload.field_name),
            .expr = expr,
        });
        return payload.object;
    }

    [[nodiscard]] syntax::ExprId extend_index_place(
        const syntax::ExprId expr, std::vector<BodyFlowPlaceProjection>& reversed_projections) const
    {
        const syntax::IndexExprPayload& payload = *this->module_.exprs.index_payload(expr.value);
        reversed_projections.push_back(BodyFlowPlaceProjection{
            .kind = BodyFlowPlaceProjectionKind::index,
            .expr = payload.index,
        });
        return payload.object;
    }

    [[nodiscard]] syntax::ExprId extend_slice_place(
        const syntax::ExprId expr, std::vector<BodyFlowPlaceProjection>& reversed_projections) const
    {
        const syntax::SliceExprPayload& payload = *this->module_.exprs.slice_payload(expr.value);
        reversed_projections.push_back(BodyFlowPlaceProjection{
            .kind = BodyFlowPlaceProjectionKind::slice,
            .expr = expr,
        });
        return payload.object;
    }

    [[nodiscard]] std::optional<syntax::ExprId> extend_deref_place(
        const syntax::ExprId expr, std::vector<BodyFlowPlaceProjection>& reversed_projections) const
    {
        const syntax::UnaryExprPayload& payload = *this->module_.exprs.unary_payload(expr.value);
        if (payload.op != syntax::UnaryOp::dereference) {
            return std::nullopt;
        }
        reversed_projections.push_back(BodyFlowPlaceProjection{
            .kind = BodyFlowPlaceProjectionKind::dereference,
            .expr = expr,
        });
        return payload.operand;
    }

    [[nodiscard]] IdentId resolve_name_id(const IdentId existing, const std::string_view fallback) const noexcept
    {
        static_cast<void>(fallback);
        return existing;
    }

    const syntax::AstModule& module_;
    const CheckedModule& checked_;
    BodyFlowGraph graph_;
    std::vector<BodyFlowTask> tasks_;
};

void append_optional_stmt_id(std::ostringstream& stream, const syntax::StmtId stmt)
{
    if (syntax::is_valid(stmt)) {
        stream << 's' << stmt.value;
        return;
    }
    stream << '-';
}

void append_optional_expr_id(std::ostringstream& stream, const syntax::ExprId expr)
{
    if (syntax::is_valid(expr)) {
        stream << 'e' << expr.value;
        return;
    }
    stream << '-';
}

void append_optional_name_id(std::ostringstream& stream, const IdentId name)
{
    if (syntax::is_valid(name)) {
        stream << '#' << name.value;
        return;
    }
    stream << '-';
}

void append_range(std::ostringstream& stream, const base::SourceRange& range)
{
    stream << range.source.value << ':' << range.begin << ".." << range.end;
}

} // namespace

std::string_view body_flow_point_kind_name(const BodyFlowPointKind kind) noexcept
{
    switch (kind) {
        case BodyFlowPointKind::entry:
            return "entry";
        case BodyFlowPointKind::exit:
            return "exit";
        case BodyFlowPointKind::statement_entry:
            return "statement_entry";
        case BodyFlowPointKind::statement_exit:
            return "statement_exit";
        case BodyFlowPointKind::expression_entry:
            return "expression_entry";
        case BodyFlowPointKind::expression_exit:
            return "expression_exit";
        case BodyFlowPointKind::sequence:
            return "sequence";
        case BodyFlowPointKind::cleanup_scope:
            return "cleanup_scope";
    }
    return "<invalid>";
}

std::string_view body_flow_action_kind_name(const BodyFlowActionKind kind) noexcept
{
    switch (kind) {
        case BodyFlowActionKind::read:
            return "read";
        case BodyFlowActionKind::write:
            return "write";
        case BodyFlowActionKind::reinit:
            return "reinit";
        case BodyFlowActionKind::move_candidate:
            return "move_candidate";
        case BodyFlowActionKind::drop:
            return "drop";
        case BodyFlowActionKind::borrow_shared:
            return "borrow_shared";
        case BodyFlowActionKind::borrow_mutable:
            return "borrow_mutable";
        case BodyFlowActionKind::call_receiver_reserve:
            return "call_receiver_reserve";
        case BodyFlowActionKind::call_receiver_activate:
            return "call_receiver_activate";
        case BodyFlowActionKind::call:
            return "call";
        case BodyFlowActionKind::return_:
            return "return";
        case BodyFlowActionKind::branch:
            return "branch";
        case BodyFlowActionKind::cleanup_scope:
            return "cleanup_scope";
        case BodyFlowActionKind::cleanup_storage:
            return "cleanup_storage";
    }
    return "<invalid>";
}

std::string_view body_flow_place_root_kind_name(const BodyFlowPlaceRootKind kind) noexcept
{
    switch (kind) {
        case BodyFlowPlaceRootKind::none:
            return "none";
        case BodyFlowPlaceRootKind::local:
            return "local";
        case BodyFlowPlaceRootKind::temporary:
            return "temporary";
        case BodyFlowPlaceRootKind::unknown:
            return "unknown";
    }
    return "<invalid>";
}

std::string_view body_flow_place_projection_kind_name(const BodyFlowPlaceProjectionKind kind) noexcept
{
    switch (kind) {
        case BodyFlowPlaceProjectionKind::field:
            return "field";
        case BodyFlowPlaceProjectionKind::index:
            return "index";
        case BodyFlowPlaceProjectionKind::dereference:
            return "dereference";
        case BodyFlowPlaceProjectionKind::slice:
            return "slice";
    }
    return "<invalid>";
}

std::string dump_body_flow_graph(const BodyFlowGraph& graph)
{
    std::ostringstream stream;
    stream << "body_flow function=" << graph.function.module << ':' << graph.function.owner_type << ':';
    append_optional_name_id(stream, graph.function.name);
    stream << " body=";
    append_optional_stmt_id(stream, graph.body);
    stream << " collect_only=" << (graph.collect_only ? "true" : "false") << '\n';

    stream << "points:\n";
    for (base::usize index = 0; index < graph.points.size(); ++index) {
        const BodyFlowPoint& point = graph.points[index];
        stream << "  p" << index << ' ' << body_flow_point_kind_name(point.kind) << " stmt=";
        append_optional_stmt_id(stream, point.stmt);
        stream << " expr=";
        append_optional_expr_id(stream, point.expr);
        stream << " range=";
        append_range(stream, point.range);
        stream << '\n';
    }

    stream << "edges:\n";
    for (const BodyFlowEdge& edge : graph.edges) {
        stream << "  p" << edge.from << " -> p" << edge.to << '\n';
    }

    stream << "places:\n";
    for (base::usize index = 0; index < graph.places.size(); ++index) {
        const BodyFlowPlace& place = graph.places[index];
        stream << "  place" << index << " root=" << body_flow_place_root_kind_name(place.root_kind) << " name=";
        append_optional_name_id(stream, place.root_name_id);
        stream << " root_expr=";
        append_optional_expr_id(stream, place.root_expr);
        stream << " projections=";
        if (place.projections.empty()) {
            stream << '-';
        } else {
            for (const BodyFlowPlaceProjection& projection : place.projections) {
                stream << ' ' << body_flow_place_projection_kind_name(projection.kind) << '(';
                append_optional_name_id(stream, projection.field_name_id);
                stream << ',';
                append_optional_expr_id(stream, projection.expr);
                stream << ')';
            }
        }
        stream << " range=";
        append_range(stream, place.range);
        stream << '\n';
    }

    stream << "actions:\n";
    for (base::usize index = 0; index < graph.actions.size(); ++index) {
        const BodyFlowAction& action = graph.actions[index];
        stream << "  a" << index << ' ' << body_flow_action_kind_name(action.kind) << " point=p" << action.point
               << " place=";
        if (action.place == SEMA_BODY_FLOW_INVALID_INDEX) {
            stream << '-';
        } else {
            stream << "place" << action.place;
        }
        stream << " stmt=";
        append_optional_stmt_id(stream, action.stmt);
        stream << " expr=";
        append_optional_expr_id(stream, action.expr);
        stream << " range=";
        append_range(stream, action.range);
        stream << '\n';
    }
    return stream.str();
}

SemanticAnalyzerCore::BodyFlowAnalyzer::BodyFlowAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

void SemanticAnalyzerCore::BodyFlowAnalyzer::collect(const syntax::ItemNode& function, const FunctionLookupKey& key)
{
    BodyFlowGraph graph =
        BodyFlowGraphBuilder(this->core_.ctx_.module, this->core_.state_.checked, key, function.body).build();
    this->core_.state_.checked.body_flow_graphs[key] = std::move(graph);
}

void SemanticAnalyzerCore::collect_body_flow_graph(const syntax::ItemNode& function, const FunctionLookupKey& key)
{
    BodyFlowAnalyzer(*this).collect(function, key);
}

} // namespace aurex::sema
