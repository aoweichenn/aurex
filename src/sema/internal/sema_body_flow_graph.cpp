#include <aurex/base/integer.hpp>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include <sema/internal/sema_body_flow_graph.hpp>

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

[[nodiscard]] bool valid_stmt(const syntax::AstModule& module, const syntax::StmtId stmt) noexcept
{
    return syntax::is_valid(stmt) && stmt.value < module.stmts.size();
}

[[nodiscard]] bool valid_expr(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size();
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
    BodyFlowGraphBuilder(const syntax::AstModule& module, const FunctionLookupKey function, const syntax::StmtId body)
        : module_(module)
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
                this->push_statement_sequence(stmt.statements, entry, exit);
                break;
        }
    }

    void process_local_statement(
        const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit)
    {
        if (syntax::is_valid(stmt.name_id)) {
            BodyFlowPlace place;
            place.root_kind = BodyFlowPlaceRootKind::local;
            place.root_name_id = stmt.name_id;
            place.range = stmt.range;
            const base::u32 place_id = this->add_place(std::move(place));
            this->add_action(BodyFlowActionKind::write, exit, place_id, stmt_id, syntax::INVALID_EXPR_ID, stmt.range);
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
            this->add_place_action(BodyFlowActionKind::write, exit, stmt_id, stmt.lhs);
        }
        this->add_edge(rhs_done, exit);
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
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(payload.args.size() + 1);
        steps.push_back(BodyFlowExpressionStep{payload.callee, BodyFlowExprContext::value});
        for (const syntax::ExprId arg : payload.args) {
            steps.push_back(BodyFlowExpressionStep{arg, BodyFlowExprContext::value});
        }
        this->push_expression_sequence(steps, entry, exit);
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
        case BodyFlowActionKind::move_candidate:
            return "move_candidate";
        case BodyFlowActionKind::borrow_shared:
            return "borrow_shared";
        case BodyFlowActionKind::borrow_mutable:
            return "borrow_mutable";
        case BodyFlowActionKind::call:
            return "call";
        case BodyFlowActionKind::return_:
            return "return";
        case BodyFlowActionKind::branch:
            return "branch";
        case BodyFlowActionKind::cleanup_scope:
            return "cleanup_scope";
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
    BodyFlowGraph graph = BodyFlowGraphBuilder(this->core_.ctx_.module, key, function.body).build();
    this->core_.state_.checked.body_flow_graphs[key] = std::move(graph);
}

void SemanticAnalyzerCore::collect_body_flow_graph(const syntax::ItemNode& function, const FunctionLookupKey& key)
{
    BodyFlowAnalyzer(*this).collect(function, key);
}

} // namespace aurex::sema
