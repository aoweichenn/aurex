#include <aurex/frontend/sema/resource_semantics.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <algorithm>
#include <charconv>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <aurex/frontend/sema/call_arguments.hpp>

#include <frontend/sema/internal/borrow/private/flow_graph.hpp>
#include <frontend/sema/internal/core/private/sema_array_repeat_semantics.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_BODY_FLOW_POINT_ID_CONTEXT = "sema body flow point id";
constexpr std::string_view SEMA_BODY_FLOW_PLACE_ID_CONTEXT = "sema body flow place id";
constexpr base::usize SEMA_BODY_FLOW_INITIAL_TASK_CAPACITY = 64;
constexpr base::usize SEMA_BODY_FLOW_CLEANUP_ITEM_INITIAL_CAPACITY = 8;
constexpr base::usize SEMA_BODY_FLOW_CONTROL_STACK_INITIAL_CAPACITY = 16;
constexpr base::usize SEMA_BODY_FLOW_FIRST_CHILD_INDEX = 0;
constexpr int SEMA_BODY_FLOW_TUPLE_FIELD_DECIMAL_BASE = 10;

enum class BodyFlowTaskKind : base::u8 {
    statement,
    expression,
};

enum class BodyFlowExprContext : base::u8 {
    value,
    branch,
    place_observe,
};

struct BodyFlowTask {
    BodyFlowTaskKind kind = BodyFlowTaskKind::statement;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    base::u32 start = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 continuation = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 return_continuation = SEMA_BODY_FLOW_INVALID_INDEX;
    BodyFlowExprContext expr_context = BodyFlowExprContext::value;
};

struct BodyFlowExpressionStep {
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    BodyFlowExprContext context = BodyFlowExprContext::value;
};

struct BodyFlowPatternCleanupFrame {
    syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
};

struct BodyFlowStructuredCleanupFrame {
    TypeHandle type = INVALID_TYPE_HANDLE;
    std::vector<BodyFlowPlaceProjection> projections;
};

enum class BodyFlowCleanupItemKind : base::u8 {
    defer_expression,
    storage,
};

enum class BodyFlowReturnFrameKind : base::u8 {
    statement,
    block,
    if_statement,
};

enum class BodyFlowReturnIfStage : base::u8 {
    evaluate_then,
    after_then,
    after_alternate,
};

struct BodyFlowCleanupItem {
    BodyFlowCleanupItemKind kind = BodyFlowCleanupItemKind::storage;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    BodyFlowPlace place;
    base::SourceRange range{};
};

struct BodyFlowReturnFrame {
    BodyFlowReturnFrameKind kind = BodyFlowReturnFrameKind::statement;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    base::usize next_child = SEMA_BODY_FLOW_FIRST_CHILD_INDEX;
    BodyFlowReturnIfStage if_stage = BodyFlowReturnIfStage::evaluate_then;
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

[[nodiscard]] std::optional<base::u32> parse_tuple_projection_index(const std::string_view field_name) noexcept
{
    if (field_name.empty()) {
        return std::nullopt;
    }
    base::u32 value = 0;
    const char* const begin = field_name.data();
    const char* const end = begin + field_name.size();
    const auto result = std::from_chars(begin, end, value, SEMA_BODY_FLOW_TUPLE_FIELD_DECIMAL_BASE);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
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
        const FunctionLookupKey function, const syntax::StmtId body, const GenericSideTables* const side_tables)
        : module_(module), checked_(checked), resources_(checked), side_tables_(side_tables)
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
            this->push_statement(this->graph_.body, entry, exit, exit);
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

    void push_statement(const syntax::StmtId stmt, const base::u32 start, const base::u32 continuation,
        const base::u32 return_continuation)
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
            .return_continuation = return_continuation,
        });
    }

    void push_expression(const syntax::ExprId expr, const base::u32 start, const base::u32 continuation,
        const BodyFlowExprContext context, const base::u32 return_continuation)
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
            .return_continuation = return_continuation,
            .expr_context = context,
        });
    }

    void push_statement_sequence(const std::vector<syntax::StmtId>& statements, const base::u32 start,
        const base::u32 continuation, const base::u32 return_continuation)
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
            this->push_statement(valid_statements[index], item_start, item_continuation, return_continuation);
        }
    }

    void push_expression_sequence(
        const std::vector<BodyFlowExpressionStep>& steps, const base::u32 start, const base::u32 continuation,
        const base::u32 return_continuation)
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
            this->push_expression(
                valid_steps[index].expr, item_start, item_continuation, valid_steps[index].context, return_continuation);
        }
    }

    void push_block_statement_sequence(const syntax::StmtId block_stmt, const syntax::StmtNode& block,
        const base::u32 start, const base::u32 continuation, const base::u32 return_continuation)
    {
        std::vector<syntax::StmtId> valid_statements;
        valid_statements.reserve(block.statements.size());
        for (const syntax::StmtId stmt : block.statements) {
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

        std::vector<base::u32> return_continuations(valid_statements.size(), return_continuation);
        std::vector<BodyFlowCleanupItem> registered_cleanup_items;
        registered_cleanup_items.reserve(
            std::min<base::usize>(valid_statements.size(), SEMA_BODY_FLOW_CLEANUP_ITEM_INITIAL_CAPACITY));
        base::usize cached_cleanup_count = 0;
        base::u32 cached_cleanup_continuation = SEMA_BODY_FLOW_INVALID_INDEX;
        for (base::usize index = 0; index < valid_statements.size(); ++index) {
            const syntax::StmtId stmt = valid_statements[index];
            if (!registered_cleanup_items.empty() && this->statement_contains_return(stmt)) {
                if (cached_cleanup_continuation == SEMA_BODY_FLOW_INVALID_INDEX
                    || cached_cleanup_count != registered_cleanup_items.size()) {
                    cached_cleanup_count = registered_cleanup_items.size();
                    cached_cleanup_continuation = this->new_sequence_point(stmt_range(this->module_, stmt));
                    this->push_cleanup_item_sequence(
                        registered_cleanup_items, cached_cleanup_continuation, return_continuation,
                        return_continuation);
                }
                return_continuations[index] = cached_cleanup_continuation;
            }
            this->collect_statement_cleanup_items(registered_cleanup_items, block_stmt, stmt);
        }

        for (base::usize reverse_index = valid_statements.size(); reverse_index > 0; --reverse_index) {
            const base::usize index = reverse_index - 1;
            const base::u32 item_start = index == 0 ? start : boundaries[index - 1];
            const base::u32 item_continuation = index + 1 == valid_statements.size() ? continuation : boundaries[index];
            this->push_statement(
                valid_statements[index], item_start, item_continuation, return_continuations[index]);
        }
    }

    [[nodiscard]] std::vector<BodyFlowCleanupItem> collect_block_cleanup_items(
        const syntax::StmtId block_stmt, const syntax::StmtNode& block)
    {
        std::vector<BodyFlowCleanupItem> items;
        items.reserve(std::min<base::usize>(block.statements.size(), SEMA_BODY_FLOW_CLEANUP_ITEM_INITIAL_CAPACITY));
        for (const syntax::StmtId stmt_id : block.statements) {
            this->collect_statement_cleanup_items(items, block_stmt, stmt_id);
        }
        return items;
    }

    void collect_statement_cleanup_items(
        std::vector<BodyFlowCleanupItem>& items, const syntax::StmtId block_stmt, const syntax::StmtId stmt_id)
    {
        if (!valid_stmt(this->module_, stmt_id)) {
            return;
        }
        const syntax::StmtNode& stmt = this->module_.stmts[stmt_id.value];
        if (stmt.kind == syntax::StmtKind::defer) {
            if (valid_expr(this->module_, stmt.init)) {
                items.push_back(BodyFlowCleanupItem{
                    .kind = BodyFlowCleanupItemKind::defer_expression,
                    .stmt = stmt_id,
                    .expr = stmt.init,
                    .place = {},
                    .range = stmt_range(this->module_, stmt_id),
                });
            }
            return;
        }
        if (stmt.kind != syntax::StmtKind::let && stmt.kind != syntax::StmtKind::var) {
            return;
        }
        if (syntax::is_valid(stmt.pattern)) {
            this->collect_pattern_cleanup_storage_items(items, stmt.pattern, stmt_id);
            return;
        }
        this->collect_local_cleanup_storage_items(items, stmt, block_stmt, stmt_id);
    }

    [[nodiscard]] bool statement_contains_return(const syntax::StmtId stmt_id) const
    {
        std::vector<syntax::StmtId> pending;
        std::vector<syntax::ExprId> pending_exprs;
        pending.reserve(SEMA_BODY_FLOW_CONTROL_STACK_INITIAL_CAPACITY);
        pending_exprs.reserve(SEMA_BODY_FLOW_CONTROL_STACK_INITIAL_CAPACITY);
        this->push_return_scan_stmt(pending, stmt_id);
        while (!pending.empty() || !pending_exprs.empty()) {
            while (!pending.empty()) {
                const syntax::StmtId current = pending.back();
                pending.pop_back();
                if (this->scan_statement_for_return(current, pending, pending_exprs)) {
                    return true;
                }
            }
            if (pending_exprs.empty()) {
                continue;
            }
            const syntax::ExprId expr = pending_exprs.back();
            pending_exprs.pop_back();
            if (this->scan_expression_for_return(expr, pending, pending_exprs)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool scan_statement_for_return(const syntax::StmtId stmt_id,
        std::vector<syntax::StmtId>& pending_stmts, std::vector<syntax::ExprId>& pending_exprs) const
    {
        if (!valid_stmt(this->module_, stmt_id)) {
            return false;
        }
        const syntax::StmtNode& stmt = this->module_.stmts[stmt_id.value];
        switch (stmt.kind) {
            case syntax::StmtKind::return_:
                return true;
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                this->push_return_scan_expr(pending_exprs, stmt.init);
                this->push_return_scan_stmt(pending_stmts, stmt.else_block);
                break;
            case syntax::StmtKind::assign:
                this->push_return_scan_expr(pending_exprs, stmt.lhs);
                this->push_return_scan_expr(pending_exprs, stmt.rhs);
                break;
            case syntax::StmtKind::if_:
                this->push_return_scan_expr(pending_exprs, stmt.condition);
                this->push_return_scan_stmt(pending_stmts, stmt.then_block);
                this->push_return_scan_stmt(pending_stmts, stmt.else_block);
                this->push_return_scan_stmt(pending_stmts, stmt.else_if);
                break;
            case syntax::StmtKind::for_:
                this->push_return_scan_stmt(pending_stmts, stmt.for_init);
                this->push_return_scan_expr(pending_exprs, stmt.condition);
                this->push_return_scan_stmt(pending_stmts, stmt.body);
                this->push_return_scan_stmt(pending_stmts, stmt.for_update);
                break;
            case syntax::StmtKind::for_range:
                this->push_return_scan_expr(pending_exprs, stmt.range_start);
                this->push_return_scan_expr(pending_exprs, stmt.range_end);
                this->push_return_scan_expr(pending_exprs, stmt.range_step);
                this->push_return_scan_expr(pending_exprs, stmt.range_iterable);
                this->push_return_scan_stmt(pending_stmts, stmt.body);
                break;
            case syntax::StmtKind::while_:
                this->push_return_scan_expr(pending_exprs, stmt.condition);
                this->push_return_scan_stmt(pending_stmts, stmt.body);
                break;
            case syntax::StmtKind::expr:
                this->push_return_scan_expr(pending_exprs, stmt.init);
                break;
            case syntax::StmtKind::block:
                pending_stmts.insert(pending_stmts.end(), stmt.statements.begin(), stmt.statements.end());
                break;
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
            case syntax::StmtKind::defer:
                break;
        }
        return false;
    }

    [[nodiscard]] bool scan_expression_for_return(const syntax::ExprId expr_id,
        std::vector<syntax::StmtId>& pending_stmts, std::vector<syntax::ExprId>& pending_exprs) const
    {
        if (!valid_expr(this->module_, expr_id)) {
            return false;
        }
        switch (this->module_.exprs.kind(expr_id.value)) {
            case syntax::ExprKind::generic_apply: {
                const syntax::GenericApplyExprPayload* const payload =
                    this->module_.exprs.generic_apply_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->callee);
                }
                break;
            }
            case syntax::ExprKind::unary: {
                const syntax::UnaryExprPayload* const payload = this->module_.exprs.unary_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->operand);
                }
                break;
            }
            case syntax::ExprKind::try_expr: {
                const syntax::TryExprPayload* const payload = this->module_.exprs.try_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->operand);
                }
                break;
            }
            case syntax::ExprKind::binary: {
                const syntax::BinaryExprPayload* const payload = this->module_.exprs.binary_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->lhs);
                    this->push_return_scan_expr(pending_exprs, payload->rhs);
                }
                break;
            }
            case syntax::ExprKind::call:
            case syntax::ExprKind::str_from_bytes_unchecked: {
                const syntax::CallExprPayload* const payload = this->module_.exprs.call_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->callee);
                    const std::span<const syntax::ExprId> call_args =
                        this->module_.exprs.kind(expr_id.value) == syntax::ExprKind::call
                        ? this->call_ordered_args(expr_id, *payload)
                        : std::span<const syntax::ExprId>{payload->args.data(), payload->args.size()};
                    for (const syntax::ExprId arg : call_args) {
                        this->push_return_scan_expr(pending_exprs, arg);
                    }
                }
                break;
            }
            case syntax::ExprKind::if_expr: {
                const syntax::IfExprPayload* const payload = this->module_.exprs.if_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->condition);
                    this->push_return_scan_expr(pending_exprs, payload->then_expr);
                    this->push_return_scan_expr(pending_exprs, payload->else_expr);
                }
                break;
            }
            case syntax::ExprKind::block_expr:
            case syntax::ExprKind::unsafe_block: {
                const syntax::BlockExprPayload* const payload = this->module_.exprs.block_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_stmt(pending_stmts, payload->block);
                    this->push_return_scan_expr(pending_exprs, payload->result);
                }
                break;
            }
            case syntax::ExprKind::match_expr: {
                const syntax::MatchExprPayload* const payload = this->module_.exprs.match_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->value);
                    for (const syntax::MatchArm& arm : payload->arms) {
                        this->push_return_scan_expr(pending_exprs, arm.guard);
                        this->push_return_scan_expr(pending_exprs, arm.value);
                    }
                }
                break;
            }
            case syntax::ExprKind::array_literal: {
                const syntax::ArrayExprPayload* const payload = this->module_.exprs.array_payload(expr_id.value);
                if (payload != nullptr) {
                    for (const syntax::ExprId element : payload->elements) {
                        this->push_return_scan_expr(pending_exprs, element);
                    }
                    if (array_repeat_value_should_be_visited(array_repeat_runtime_semantics(
                            this->module_, this->checked_.types, this->cached_expr_type(expr_id), expr_id))) {
                        this->push_return_scan_expr(pending_exprs, payload->repeat_value);
                    }
                    this->push_return_scan_expr(pending_exprs, payload->repeat_count);
                }
                break;
            }
            case syntax::ExprKind::tuple_literal: {
                const syntax::AstArenaVector<syntax::ExprId>* const elements =
                    this->module_.exprs.tuple_elements(expr_id.value);
                if (elements != nullptr) {
                    for (const syntax::ExprId element : *elements) {
                        this->push_return_scan_expr(pending_exprs, element);
                    }
                }
                break;
            }
            case syntax::ExprKind::field: {
                const syntax::FieldExprPayload* const payload = this->module_.exprs.field_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->object);
                }
                break;
            }
            case syntax::ExprKind::index: {
                const syntax::IndexExprPayload* const payload = this->module_.exprs.index_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->object);
                    this->push_return_scan_expr(pending_exprs, payload->index);
                }
                break;
            }
            case syntax::ExprKind::slice: {
                const syntax::SliceExprPayload* const payload = this->module_.exprs.slice_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->object);
                    this->push_return_scan_expr(pending_exprs, payload->start);
                    this->push_return_scan_expr(pending_exprs, payload->end);
                }
                break;
            }
            case syntax::ExprKind::struct_literal: {
                const syntax::StructLiteralExprPayload* const payload =
                    this->module_.exprs.struct_literal_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->object);
                    for (const syntax::FieldInit& field : payload->field_inits) {
                        this->push_return_scan_expr(pending_exprs, field.value);
                    }
                }
                break;
            }
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
            case syntax::ExprKind::str_from_utf8_checked: {
                const syntax::CastExprPayload* const payload = this->module_.exprs.cast_payload(expr_id.value);
                if (payload != nullptr) {
                    this->push_return_scan_expr(pending_exprs, payload->expr);
                }
                break;
            }
            case syntax::ExprKind::invalid:
            case syntax::ExprKind::lambda:
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
                break;
        }
        return false;
    }

    void push_return_scan_stmt(std::vector<syntax::StmtId>& pending, const syntax::StmtId stmt) const
    {
        if (valid_stmt(this->module_, stmt)) {
            pending.push_back(stmt);
        }
    }

    void push_return_scan_expr(std::vector<syntax::ExprId>& pending, const syntax::ExprId expr) const
    {
        if (valid_expr(this->module_, expr)) {
            pending.push_back(expr);
        }
    }

    [[nodiscard]] bool statement_may_fallthrough(const syntax::StmtId stmt_id) const
    {
        if (!valid_stmt(this->module_, stmt_id)) {
            return true;
        }
        std::vector<BodyFlowReturnFrame> stack;
        stack.reserve(SEMA_BODY_FLOW_CONTROL_STACK_INITIAL_CAPACITY);
        stack.push_back(BodyFlowReturnFrame{
            .kind = BodyFlowReturnFrameKind::statement,
            .stmt = stmt_id,
        });

        bool has_result = false;
        bool final_result = true;
        while (!has_result && !stack.empty()) {
            switch (stack.back().kind) {
                case BodyFlowReturnFrameKind::statement:
                    this->evaluate_statement_fallthrough_frame(stack, has_result, final_result);
                    break;
                case BodyFlowReturnFrameKind::block:
                    this->evaluate_block_fallthrough_frame(stack, has_result, final_result);
                    break;
                case BodyFlowReturnFrameKind::if_statement:
                    this->evaluate_if_fallthrough_frame(stack, has_result, final_result);
                    break;
            }
        }
        return final_result;
    }

    [[nodiscard]] bool block_may_fallthrough(const syntax::StmtNode& block) const
    {
        std::vector<BodyFlowReturnFrame> stack;
        stack.reserve(SEMA_BODY_FLOW_CONTROL_STACK_INITIAL_CAPACITY);
        for (const syntax::StmtId stmt_id : block.statements) {
            if (!valid_stmt(this->module_, stmt_id)) {
                continue;
            }
            if (!this->statement_may_fallthrough(stmt_id)) {
                return false;
            }
        }
        return true;
    }

    void evaluate_statement_fallthrough_frame(
        std::vector<BodyFlowReturnFrame>& stack, bool& has_result, bool& final_result) const
    {
        BodyFlowReturnFrame& frame = stack.back();
        if (!valid_stmt(this->module_, frame.stmt)) {
            this->finish_fallthrough_frame(stack, true, has_result, final_result);
            return;
        }
        const syntax::StmtNode& stmt = this->module_.stmts[frame.stmt.value];
        switch (stmt.kind) {
            case syntax::StmtKind::return_:
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
                this->finish_fallthrough_frame(stack, false, has_result, final_result);
                break;
            case syntax::StmtKind::block:
                frame.kind = BodyFlowReturnFrameKind::block;
                frame.next_child = SEMA_BODY_FLOW_FIRST_CHILD_INDEX;
                break;
            case syntax::StmtKind::if_:
                frame.kind = BodyFlowReturnFrameKind::if_statement;
                frame.if_stage = BodyFlowReturnIfStage::evaluate_then;
                break;
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
            case syntax::StmtKind::assign:
            case syntax::StmtKind::for_:
            case syntax::StmtKind::for_range:
            case syntax::StmtKind::while_:
            case syntax::StmtKind::defer:
            case syntax::StmtKind::expr:
                this->finish_fallthrough_frame(stack, true, has_result, final_result);
                break;
        }
    }

    void evaluate_block_fallthrough_frame(
        std::vector<BodyFlowReturnFrame>& stack, bool& has_result, bool& final_result) const
    {
        BodyFlowReturnFrame& frame = stack.back();
        if (!valid_stmt(this->module_, frame.stmt)) {
            this->finish_fallthrough_frame(stack, true, has_result, final_result);
            return;
        }
        const syntax::AstArenaVector<syntax::StmtId>* const statements =
            this->module_.stmts.block_statements(frame.stmt.value);
        if (statements == nullptr) {
            frame.kind = BodyFlowReturnFrameKind::statement;
            return;
        }
        if (frame.next_child >= statements->size()) {
            this->finish_fallthrough_frame(stack, true, has_result, final_result);
            return;
        }
        const syntax::StmtId child = (*statements)[frame.next_child];
        stack.push_back(BodyFlowReturnFrame{
            .kind = BodyFlowReturnFrameKind::statement,
            .stmt = child,
        });
    }

    void evaluate_if_fallthrough_frame(
        std::vector<BodyFlowReturnFrame>& stack, bool& has_result, bool& final_result) const
    {
        BodyFlowReturnFrame& frame = stack.back();
        if (!valid_stmt(this->module_, frame.stmt)) {
            this->finish_fallthrough_frame(stack, true, has_result, final_result);
            return;
        }
        const syntax::StmtNode& stmt = this->module_.stmts[frame.stmt.value];
        if (stmt.kind != syntax::StmtKind::if_) {
            this->finish_fallthrough_frame(stack, true, has_result, final_result);
            return;
        }
        if (frame.if_stage == BodyFlowReturnIfStage::evaluate_then) {
            frame.if_stage = BodyFlowReturnIfStage::after_then;
            stack.push_back(BodyFlowReturnFrame{
                .kind = BodyFlowReturnFrameKind::block,
                .stmt = stmt.then_block,
            });
            return;
        }
    }

    void finish_fallthrough_frame(
        std::vector<BodyFlowReturnFrame>& stack, const bool result, bool& has_result, bool& final_result) const
    {
        stack.pop_back();
        while (!stack.empty()) {
            BodyFlowReturnFrame& parent = stack.back();
            switch (parent.kind) {
                case BodyFlowReturnFrameKind::statement:
                    return;
                case BodyFlowReturnFrameKind::block:
                    if (result) {
                        ++parent.next_child;
                        return;
                    }
                    stack.pop_back();
                    continue;
                case BodyFlowReturnFrameKind::if_statement:
                    if (parent.if_stage == BodyFlowReturnIfStage::after_then) {
                        if (result) {
                            this->finish_fallthrough_frame(stack, true, has_result, final_result);
                            return;
                        }
                        const syntax::StmtNode& stmt = this->module_.stmts[parent.stmt.value];
                        parent.if_stage = BodyFlowReturnIfStage::after_alternate;
                        if (valid_stmt(this->module_, stmt.else_block)) {
                            stack.push_back(BodyFlowReturnFrame{
                                .kind = BodyFlowReturnFrameKind::block,
                                .stmt = stmt.else_block,
                            });
                            return;
                        }
                        if (valid_stmt(this->module_, stmt.else_if)) {
                            stack.push_back(BodyFlowReturnFrame{
                                .kind = BodyFlowReturnFrameKind::statement,
                                .stmt = stmt.else_if,
                            });
                            return;
                        }
                        this->finish_fallthrough_frame(stack, true, has_result, final_result);
                        return;
                    }
                    if (parent.if_stage == BodyFlowReturnIfStage::after_alternate) {
                        stack.pop_back();
                        continue;
                    }
                    return;
            }
        }
        has_result = true;
        final_result = result;
    }

    void push_cleanup_item_sequence(std::vector<BodyFlowCleanupItem> items, const base::u32 start,
        const base::u32 continuation, const base::u32 return_continuation)
    {
        std::vector<base::usize> execution_order;
        execution_order.reserve(items.size());
        for (base::usize item_index = items.size(); item_index > 0; --item_index) {
            execution_order.push_back(item_index - 1U);
        }

        std::vector<base::u32> cleanup_points;
        cleanup_points.reserve(execution_order.size());
        for (const base::usize item_index : execution_order) {
            BodyFlowCleanupItem& item = items[item_index];
            const base::u32 point = item.kind == BodyFlowCleanupItemKind::defer_expression
                ? this->new_cleanup_point(item.stmt, item.range)
                : this->new_sequence_point(item.range);
            if (item.kind == BodyFlowCleanupItemKind::defer_expression) {
                this->add_point_action(BodyFlowActionKind::cleanup_scope, point, item.stmt, item.expr, item.range);
            } else {
                const base::u32 place = this->add_place(std::move(item.place));
                this->add_action(
                    BodyFlowActionKind::cleanup_storage, point, place, item.stmt, syntax::INVALID_EXPR_ID, item.range);
            }
            cleanup_points.push_back(point);
        }

        this->add_edge(start, cleanup_points.front());
        for (base::usize cleanup_index = 0; cleanup_index < cleanup_points.size(); ++cleanup_index) {
            const base::usize item_index = execution_order[cleanup_index];
            const base::u32 item_continuation = cleanup_index + 1U == cleanup_points.size()
                ? continuation
                : cleanup_points[cleanup_index + 1U];
            if (items[item_index].kind == BodyFlowCleanupItemKind::defer_expression) {
                this->push_expression(items[item_index].expr, cleanup_points[cleanup_index], item_continuation,
                    BodyFlowExprContext::value, return_continuation);
                continue;
            }
            this->add_edge(cleanup_points[cleanup_index], item_continuation);
        }
    }

    void collect_local_storage_cleanup_item(std::vector<BodyFlowCleanupItem>& items, const syntax::StmtId stmt,
        const IdentId name_id, const base::SourceRange& range)
    {
        if (!syntax::is_valid(name_id)) {
            return;
        }
        BodyFlowPlace place;
        place.root_kind = BodyFlowPlaceRootKind::local;
        place.root_name_id = name_id;
        place.range = range;
        items.push_back(BodyFlowCleanupItem{
            .kind = BodyFlowCleanupItemKind::storage,
            .stmt = stmt,
            .place = std::move(place),
            .range = range,
        });
    }

    void collect_local_projected_storage_cleanup_item(std::vector<BodyFlowCleanupItem>& items,
        const syntax::StmtId stmt, const IdentId name_id, std::vector<BodyFlowPlaceProjection> projections,
        const base::SourceRange& range)
    {
        BodyFlowPlace place;
        place.root_kind = BodyFlowPlaceRootKind::local;
        place.root_name_id = name_id;
        place.projections = std::move(projections);
        place.range = range;
        items.push_back(BodyFlowCleanupItem{
            .kind = BodyFlowCleanupItemKind::storage,
            .stmt = stmt,
            .place = std::move(place),
            .range = range,
        });
    }

    void collect_local_cleanup_storage_items(std::vector<BodyFlowCleanupItem>& items, const syntax::StmtNode& stmt,
        const syntax::StmtId block_stmt, const syntax::StmtId local_stmt)
    {
        if (!syntax::is_valid(stmt.name_id)) {
            return;
        }
        if (this->collect_structured_cleanup_storage_items(
                items, stmt.name_id, this->cached_stmt_local_type(local_stmt), block_stmt, stmt.range)) {
            return;
        }
        this->collect_local_storage_cleanup_item(items, block_stmt, stmt.name_id, stmt.range);
    }

    [[nodiscard]] bool collect_structured_cleanup_storage_items(std::vector<BodyFlowCleanupItem>& items,
        const IdentId root_name, const TypeHandle type, const syntax::StmtId block_stmt,
        const base::SourceRange& range)
    {
        bool expanded_any = false;
        std::vector<BodyFlowStructuredCleanupFrame> pending;
        pending.push_back(BodyFlowStructuredCleanupFrame{
            .type = type,
            .projections = {},
        });

        while (!pending.empty()) {
            BodyFlowStructuredCleanupFrame frame = std::move(pending.back());
            pending.pop_back();
            if (!is_valid(frame.type) || frame.type.value >= this->checked_.types.size()) {
                continue;
            }
            const TypeInfo& info = this->checked_.types.get(frame.type);
            if (info.kind == TypeKind::tuple) {
                expanded_any = this->push_tuple_element_cleanup_storage_actions(frame, pending) || expanded_any;
                continue;
            }
            const StructInfo* const structure = this->find_struct(frame.type);
            if (structure != nullptr && !structure->fields.empty()) {
                expanded_any = this->push_struct_field_cleanup_storage_actions(*structure, frame, pending)
                    || expanded_any;
                continue;
            }

            if (!frame.projections.empty()) {
                this->collect_local_projected_storage_cleanup_item(
                    items, block_stmt, root_name, std::move(frame.projections), range);
            }
        }
        return expanded_any;
    }

    void collect_pattern_cleanup_storage_items(
        std::vector<BodyFlowCleanupItem>& items, const syntax::PatternId pattern, const syntax::StmtId stmt_id)
    {
        std::vector<BodyFlowPatternCleanupFrame> pending;
        pending.push_back(BodyFlowPatternCleanupFrame{.pattern = pattern, .stmt = stmt_id});
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
                    this->collect_local_storage_cleanup_item(items, frame.stmt, node->binding_name_id, node->range);
                    break;
                case syntax::PatternKind::tuple:
                case syntax::PatternKind::slice:
                    for (const syntax::PatternId element : node->elements) {
                        pending.push_back(BodyFlowPatternCleanupFrame{.pattern = element, .stmt = frame.stmt});
                    }
                    break;
                case syntax::PatternKind::struct_:
                    for (const syntax::FieldPattern& field : node->field_patterns) {
                        pending.push_back(BodyFlowPatternCleanupFrame{.pattern = field.pattern, .stmt = frame.stmt});
                    }
                    break;
                case syntax::PatternKind::enum_case:
                    for (const syntax::PatternId payload : node->payload_patterns) {
                        pending.push_back(BodyFlowPatternCleanupFrame{.pattern = payload, .stmt = frame.stmt});
                    }
                    break;
                case syntax::PatternKind::or_pattern:
                    for (const syntax::PatternId alternative : node->alternatives) {
                        pending.push_back(BodyFlowPatternCleanupFrame{.pattern = alternative, .stmt = frame.stmt});
                    }
                    break;
                case syntax::PatternKind::wildcard:
                case syntax::PatternKind::literal:
                case syntax::PatternKind::const_:
                    break;
            }
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
        if (stmt.kind != syntax::StmtKind::return_) {
            this->add_edge(exit, task.continuation);
        }

        switch (stmt.kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                this->process_local_statement(task.stmt, stmt, entry, exit, task.return_continuation);
                break;
            case syntax::StmtKind::assign:
                this->process_assignment_statement(task.stmt, stmt, entry, exit, task.return_continuation);
                break;
            case syntax::StmtKind::if_:
                this->process_if_statement(stmt, entry, exit, task.return_continuation, range);
                break;
            case syntax::StmtKind::for_:
                this->process_for_statement(stmt, entry, exit, task.return_continuation, range);
                break;
            case syntax::StmtKind::for_range:
                this->process_for_range_statement(stmt, entry, exit, task.return_continuation, range);
                break;
            case syntax::StmtKind::while_:
                this->process_while_statement(stmt, entry, exit, task.return_continuation, range);
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
                this->process_return_statement(task.stmt, stmt, entry, task.return_continuation, range);
                break;
            case syntax::StmtKind::expr:
                this->push_expression(stmt.init, entry, exit, BodyFlowExprContext::value, task.return_continuation);
                break;
            case syntax::StmtKind::block:
                this->process_block_statement(task.stmt, stmt, entry, exit, task.return_continuation, range);
                break;
        }
    }

    void process_block_statement(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry,
        const base::u32 exit, const base::u32 return_continuation, const base::SourceRange& range)
    {
        this->add_point_action(BodyFlowActionKind::cleanup_scope, exit, stmt_id, syntax::INVALID_EXPR_ID, range);

        std::vector<BodyFlowCleanupItem> cleanup_items = this->collect_block_cleanup_items(stmt_id, stmt);
        if (cleanup_items.empty()) {
            this->push_statement_sequence(stmt.statements, entry, exit, return_continuation);
            return;
        }

        const base::u32 cleanup_start = this->new_sequence_point(range);
        this->push_block_statement_sequence(stmt_id, stmt, entry, cleanup_start, return_continuation);
        if (this->block_may_fallthrough(stmt)) {
            this->push_cleanup_item_sequence(cleanup_items, cleanup_start, exit, return_continuation);
        }
    }

    void process_local_statement(
        const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit,
        const base::u32 return_continuation)
    {
        if (syntax::is_valid(stmt.name_id)) {
            this->add_local_storage_action(BodyFlowActionKind::write, exit, stmt_id, stmt.name_id, stmt.range);
        }
        if (syntax::is_valid(stmt.init)) {
            this->push_expression(stmt.init, entry, exit, BodyFlowExprContext::value, return_continuation);
        } else {
            this->add_edge(entry, exit);
        }
    }

    void process_assignment_statement(
        const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit,
        const base::u32 return_continuation)
    {
        const base::u32 lhs_done = this->new_sequence_point(stmt.range);
        const base::u32 rhs_done = this->new_sequence_point(stmt.range);
        this->push_expression(stmt.lhs, entry, lhs_done, BodyFlowExprContext::place_observe, return_continuation);
        this->push_expression(stmt.rhs, lhs_done, rhs_done, BodyFlowExprContext::value, return_continuation);
        if (valid_expr(this->module_, stmt.lhs)) {
            const BodyFlowActionKind write_kind = this->assignment_write_kind(stmt.lhs);
            this->add_place_action(write_kind, exit, stmt_id, stmt.lhs);
        }
        this->add_edge(rhs_done, exit);
    }

    [[nodiscard]] BodyFlowActionKind assignment_write_kind(const syntax::ExprId lhs) const noexcept
    {
        if (this->expr_is_direct_local_name(lhs) || this->expr_is_field_projection(lhs)) {
            return BodyFlowActionKind::reinit;
        }
        return BodyFlowActionKind::write;
    }

    [[nodiscard]] bool push_tuple_element_cleanup_storage_actions(const BodyFlowStructuredCleanupFrame& frame,
        std::vector<BodyFlowStructuredCleanupFrame>& pending) const
    {
        const TypeInfo& tuple = this->checked_.types.get(frame.type);
        bool pushed_any = false;
        for (base::usize index = tuple.tuple_elements.size(); index > 0; --index) {
            const base::usize element_index = index - 1U;
            const TypeHandle element_type = tuple.tuple_elements[element_index];
            if (!this->type_needs_drop(element_type)) {
                continue;
            }
            BodyFlowStructuredCleanupFrame child{
                .type = element_type,
                .projections = frame.projections,
            };
            child.projections.push_back(BodyFlowPlaceProjection{
                .kind = BodyFlowPlaceProjectionKind::tuple_element,
                .field_name_id = INVALID_IDENT_ID,
                .element_index = base::checked_u32(element_index, SEMA_BODY_FLOW_PLACE_ID_CONTEXT),
                .expr = syntax::INVALID_EXPR_ID,
            });
            pending.push_back(std::move(child));
            pushed_any = true;
        }
        return pushed_any;
    }

    [[nodiscard]] bool push_struct_field_cleanup_storage_actions(const StructInfo& structure,
        const BodyFlowStructuredCleanupFrame& frame, std::vector<BodyFlowStructuredCleanupFrame>& pending) const
    {
        bool pushed_any = false;
        for (base::usize index = structure.fields.size(); index > 0; --index) {
            const StructFieldInfo& field = structure.fields[index - 1U];
            if (!this->type_needs_drop(field.type)) {
                continue;
            }
            BodyFlowStructuredCleanupFrame child{
                .type = field.type,
                .projections = frame.projections,
            };
            child.projections.push_back(BodyFlowPlaceProjection{
                .kind = BodyFlowPlaceProjectionKind::field,
                .field_name_id = field.name_id,
                .element_index = SEMA_BODY_FLOW_INVALID_INDEX,
                .expr = syntax::INVALID_EXPR_ID,
            });
            pending.push_back(std::move(child));
            pushed_any = true;
        }
        return pushed_any;
    }

    void process_if_statement(const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit,
        const base::u32 return_continuation, const base::SourceRange& range)
    {
        const base::u32 condition_done = this->new_sequence_point(range);
        this->push_expression(stmt.condition, entry, condition_done, BodyFlowExprContext::branch, return_continuation);
        if (valid_stmt(this->module_, stmt.then_block)) {
            this->push_statement(stmt.then_block, condition_done, exit, return_continuation);
        }
        if (valid_stmt(this->module_, stmt.else_block)) {
            this->push_statement(stmt.else_block, condition_done, exit, return_continuation);
        }
        if (valid_stmt(this->module_, stmt.else_if)) {
            this->push_statement(stmt.else_if, condition_done, exit, return_continuation);
        }
        if (!valid_stmt(this->module_, stmt.then_block) && !valid_stmt(this->module_, stmt.else_block)
            && !valid_stmt(this->module_, stmt.else_if)) {
            this->add_edge(condition_done, exit);
        } else if (!valid_stmt(this->module_, stmt.else_block) && !valid_stmt(this->module_, stmt.else_if)) {
            this->add_edge(condition_done, exit);
        }
    }

    void process_for_statement(const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit,
        const base::u32 return_continuation, const base::SourceRange& range)
    {
        const base::u32 init_done = this->new_sequence_point(range);
        const base::u32 condition_done = this->new_sequence_point(range);
        const base::u32 body_done = this->new_sequence_point(range);
        this->push_statement(stmt.for_init, entry, init_done, return_continuation);
        this->push_expression(
            stmt.condition, init_done, condition_done, BodyFlowExprContext::branch, return_continuation);
        this->add_edge(condition_done, exit);
        this->push_statement(stmt.body, condition_done, body_done, return_continuation);
        this->push_statement(stmt.for_update, body_done, init_done, return_continuation);
    }

    void process_for_range_statement(const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit,
        const base::u32 return_continuation, const base::SourceRange& range)
    {
        const base::u32 range_done = this->new_sequence_point(range);
        this->push_expression_sequence(
            {
                BodyFlowExpressionStep{stmt.range_start, BodyFlowExprContext::value},
                BodyFlowExpressionStep{stmt.range_end, BodyFlowExprContext::value},
                BodyFlowExpressionStep{stmt.range_step, BodyFlowExprContext::value},
                BodyFlowExpressionStep{stmt.range_iterable, BodyFlowExprContext::value},
            },
            entry, range_done, return_continuation);
        this->add_point_action(
            BodyFlowActionKind::branch, range_done, syntax::INVALID_STMT_ID, syntax::INVALID_EXPR_ID, range);
        this->add_edge(range_done, exit);
        this->push_statement(stmt.body, range_done, range_done, return_continuation);
    }

    void process_while_statement(const syntax::StmtNode& stmt, const base::u32 entry, const base::u32 exit,
        const base::u32 return_continuation, const base::SourceRange& range)
    {
        const base::u32 condition_done = this->new_sequence_point(range);
        this->push_expression(stmt.condition, entry, condition_done, BodyFlowExprContext::branch, return_continuation);
        this->add_edge(condition_done, exit);
        this->push_statement(stmt.body, condition_done, entry, return_continuation);
    }

    void process_defer_statement(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry,
        const base::u32 exit, const base::SourceRange& range)
    {
        static_cast<void>(stmt_id);
        static_cast<void>(stmt);
        static_cast<void>(range);
        this->add_edge(entry, exit);
    }

    void process_return_statement(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, const base::u32 entry,
        const base::u32 exit, const base::SourceRange& range)
    {
        const base::u32 return_point = this->new_sequence_point(range);
        this->push_expression(stmt.return_value, entry, return_point, BodyFlowExprContext::value, exit);
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
        if (this->record_place_context_action(task.expr, task.expr_context)) {
            this->push_place_operands(task.expr, entry, exit, task.return_continuation);
            return;
        }

        if (const syntax::UnaryExprPayload* const unary = this->module_.exprs.unary_payload(task.expr.value);
            kind == syntax::ExprKind::unary && unary != nullptr && unary_is_address_of(unary->op)) {
            const BodyFlowActionKind action = unary->op == syntax::UnaryOp::address_of_mut
                ? BodyFlowActionKind::borrow_mutable
                : BodyFlowActionKind::borrow_shared;
            this->add_place_action(action, exit, syntax::INVALID_STMT_ID, unary->operand);
            this->push_place_operands(unary->operand, entry, exit, task.return_continuation);
            return;
        }

        if (task.expr_context != BodyFlowExprContext::place_observe) {
            this->record_value_place_actions(task.expr, exit);
        }
        if (task.expr_context == BodyFlowExprContext::value || task.expr_context == BodyFlowExprContext::branch) {
            this->record_borrowed_view_result_actions(task.expr, kind, exit);
        }
        this->push_expression_children(task.expr, kind, entry, exit, task.return_continuation);
    }

    [[nodiscard]] bool record_place_context_action(const syntax::ExprId expr, const BodyFlowExprContext context)
    {
        switch (context) {
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

    void push_place_operands(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        if (!valid_expr(this->module_, expr)) {
            this->add_edge(entry, exit);
            return;
        }
        switch (this->module_.exprs.kind(expr.value)) {
            case syntax::ExprKind::field: {
                const syntax::FieldExprPayload& field = *this->module_.exprs.field_payload(expr.value);
                this->push_expression(
                    field.object, entry, exit, BodyFlowExprContext::place_observe, return_continuation);
                return;
            }
            case syntax::ExprKind::index: {
                const syntax::IndexExprPayload& index = *this->module_.exprs.index_payload(expr.value);
                this->push_expression_sequence(
                    {
                        BodyFlowExpressionStep{index.object, BodyFlowExprContext::place_observe},
                        BodyFlowExpressionStep{index.index, BodyFlowExprContext::value},
                    },
                    entry, exit, return_continuation);
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
                    entry, exit, return_continuation);
                return;
            }
            case syntax::ExprKind::unary: {
                const syntax::UnaryExprPayload& unary = *this->module_.exprs.unary_payload(expr.value);
                if (unary.op == syntax::UnaryOp::dereference) {
                    this->push_expression(unary.operand, entry, exit, BodyFlowExprContext::value, return_continuation);
                    return;
                }
                break;
            }
            default:
                break;
        }
        this->add_edge(entry, exit);
    }

    void push_expression_children(const syntax::ExprId expr, const syntax::ExprKind kind, const base::u32 entry,
        const base::u32 exit, const base::u32 return_continuation)
    {
        if (expr_is_place_like(this->module_, expr)) {
            this->push_place_operands(expr, entry, exit, return_continuation);
            return;
        }

        switch (kind) {
            case syntax::ExprKind::generic_apply:
                this->push_generic_apply_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::unary:
                this->push_unary_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::try_expr:
                this->push_try_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::binary:
                this->push_binary_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::call:
            case syntax::ExprKind::str_from_bytes_unchecked:
                this->push_call_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::if_expr:
                this->push_if_expression_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::block_expr:
            case syntax::ExprKind::unsafe_block:
                this->push_block_expression_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::match_expr:
                this->push_match_expression_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::array_literal:
                this->push_array_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::tuple_literal:
                this->push_tuple_children(expr, entry, exit, return_continuation);
                break;
            case syntax::ExprKind::lambda:
                this->add_edge(entry, exit);
                break;
            case syntax::ExprKind::struct_literal:
                this->push_struct_literal_children(expr, entry, exit, return_continuation);
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
                this->push_cast_like_children(expr, entry, exit, return_continuation);
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

    void push_generic_apply_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::GenericApplyExprPayload& payload = *this->module_.exprs.generic_apply_payload(expr.value);
        this->push_expression(payload.callee, entry, exit, BodyFlowExprContext::value, return_continuation);
    }

    void push_unary_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::UnaryExprPayload& payload = *this->module_.exprs.unary_payload(expr.value);
        this->push_expression(payload.operand, entry, exit, BodyFlowExprContext::value, return_continuation);
    }

    void push_try_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::TryExprPayload& payload = *this->module_.exprs.try_payload(expr.value);
        this->push_expression(payload.operand, entry, exit, BodyFlowExprContext::value, return_continuation);
    }

    void push_binary_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::BinaryExprPayload& payload = *this->module_.exprs.binary_payload(expr.value);
        this->push_expression_sequence(
            {
                BodyFlowExpressionStep{payload.lhs, BodyFlowExprContext::value},
                BodyFlowExpressionStep{payload.rhs, BodyFlowExprContext::value},
            },
            entry, exit, return_continuation);
    }

    void push_call_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::CallExprPayload& payload = *this->module_.exprs.call_payload(expr.value);
        const std::span<const syntax::ExprId> call_args = this->call_ordered_args(expr, payload);
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
            this->push_expression(payload.callee, entry, receiver_done, BodyFlowExprContext::value, return_continuation);
            std::vector<BodyFlowExpressionStep> arg_steps;
            arg_steps.reserve(call_args.size());
            for (const syntax::ExprId arg : call_args) {
                arg_steps.push_back(BodyFlowExpressionStep{arg, BodyFlowExprContext::value});
            }
            this->push_expression_sequence(arg_steps, receiver_done, exit, return_continuation);
            return;
        }
        this->record_call_return_borrow_actions(expr, payload, exit);
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(call_args.size() + 1);
        steps.push_back(BodyFlowExpressionStep{payload.callee, BodyFlowExprContext::value});
        for (const syntax::ExprId arg : call_args) {
            steps.push_back(BodyFlowExpressionStep{arg, BodyFlowExprContext::value});
        }
        this->push_expression_sequence(steps, entry, exit, return_continuation);
    }

    [[nodiscard]] std::span<const syntax::ExprId> call_ordered_args(
        const syntax::ExprId call, const syntax::CallExprPayload& payload) const noexcept
    {
        if (const FunctionCallBinding* const binding = this->checked_.function_call_binding_for_expr(call);
            binding != nullptr) {
            return ordered_call_args_or_source(binding->ordered_args, payload);
        }
        if (const TraitMethodCallBinding* const binding = this->checked_.trait_method_call_binding_for_expr(call);
            binding != nullptr) {
            return ordered_call_args_or_source(binding->ordered_args, payload);
        }
        return {payload.args.data(), payload.args.size()};
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
                    contract->second, this->call_ordered_args(expr, payload), binding->callee_expr,
                    binding->receiver_arg_count, expr, point);
                return;
            }
            if (const auto summary = this->checked_.borrow_summaries.find(binding->function_key);
                summary != this->checked_.borrow_summaries.end()) {
                this->record_summary_return_borrow_actions(
                    summary->second, this->call_ordered_args(expr, payload), binding->callee_expr,
                    binding->receiver_arg_count, expr, point);
                return;
            }
            if (const auto contract = this->checked_.borrow_contracts.find(binding->function_key);
                contract != this->checked_.borrow_contracts.end()) {
                this->record_contract_return_borrow_actions(
                    contract->second, this->call_ordered_args(expr, payload), binding->callee_expr,
                    binding->receiver_arg_count, expr, point);
                return;
            }
            if (this->type_can_contain_borrow(binding->return_type)) {
                this->record_unknown_return_borrow_actions(
                    this->call_ordered_args(expr, payload), binding->callee_expr, binding->receiver_arg_count,
                    expr, point);
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
                    contract->second, this->call_ordered_args(expr, payload), binding->callee_expr,
                    receiver_arg_count, expr, point);
                return;
            }
            if (const auto summary = this->checked_.borrow_summaries.find(binding->function_key);
                summary != this->checked_.borrow_summaries.end()) {
                this->record_summary_return_borrow_actions(
                    summary->second, this->call_ordered_args(expr, payload), binding->callee_expr,
                    receiver_arg_count, expr, point);
                return;
            }
            if (const auto contract = this->checked_.borrow_contracts.find(binding->function_key);
                contract != this->checked_.borrow_contracts.end()) {
                this->record_contract_return_borrow_actions(
                    contract->second, this->call_ordered_args(expr, payload), binding->callee_expr,
                    receiver_arg_count, expr, point);
                return;
            }
            if (this->type_can_contain_borrow(binding->return_type)) {
                this->record_unknown_return_borrow_actions(
                    this->call_ordered_args(expr, payload), binding->callee_expr, receiver_arg_count, expr, point);
            }
        }
    }

    void record_summary_return_borrow_actions(const FunctionBorrowSummary& summary,
        const std::span<const syntax::ExprId> args, const syntax::ExprId callee, const base::u32 receiver_arg_count,
        const syntax::ExprId call_expr, const base::u32 point)
    {
        bool recorded_unknown = false;
        const auto record_unknown = [&]() {
            if (recorded_unknown) {
                return;
            }
            this->record_unknown_return_borrow_actions(args, callee, receiver_arg_count, call_expr, point);
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
                    args, callee, receiver_arg_count, origin.param_index, call_expr, point);
            } else if (origin.kind != BorrowSummaryOriginKind::static_) {
                record_unknown();
            }
        }
    }

    void record_contract_return_borrow_actions(const FunctionBorrowContract& contract,
        const std::span<const syntax::ExprId> args, const syntax::ExprId callee, const base::u32 receiver_arg_count,
        const syntax::ExprId call_expr, const base::u32 point)
    {
        if (contract.unknown_return_allowed) {
            this->record_unknown_return_borrow_actions(args, callee, receiver_arg_count, call_expr, point);
            return;
        }
        for (const BorrowContractSelector& selector : contract.return_selectors) {
            switch (selector.kind) {
                case BorrowContractSelectorKind::parameter:
                case BorrowContractSelectorKind::self:
                    this->record_param_return_borrow_action(
                        args, callee, receiver_arg_count, selector.param_index, call_expr, point);
                    break;
                case BorrowContractSelectorKind::static_:
                    break;
                case BorrowContractSelectorKind::unknown:
                    this->record_unknown_return_borrow_actions(args, callee, receiver_arg_count, call_expr, point);
                    return;
            }
        }
    }

    void record_param_return_borrow_action(const std::span<const syntax::ExprId> args, const syntax::ExprId callee,
        const base::u32 receiver_arg_count, const base::u32 param_index, const syntax::ExprId call_expr,
        const base::u32 point)
    {
        if (param_index == SEMA_BORROW_SUMMARY_INVALID_INDEX) {
            return;
        }
        const syntax::ExprId source = this->call_argument_for_param(args, callee, receiver_arg_count, param_index);
        this->add_return_borrow_action(this->borrow_action_for_result(call_expr), point, call_expr, source);
    }

    void record_unknown_return_borrow_actions(const std::span<const syntax::ExprId> args, const syntax::ExprId callee,
        const base::u32 receiver_arg_count, const syntax::ExprId call_expr, const base::u32 point)
    {
        this->add_unknown_return_borrow_action(this->borrow_action_for_result(call_expr), point, call_expr);
        std::unordered_set<base::u32> recorded_sources;
        const base::u32 parameter_count =
            receiver_arg_count + base::checked_u32(args.size(), SEMA_BODY_FLOW_PLACE_ID_CONTEXT);
        for (base::u32 param_index = 0; param_index < parameter_count; ++param_index) {
            const syntax::ExprId source =
                this->call_argument_for_param(args, callee, receiver_arg_count, param_index);
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

    [[nodiscard]] syntax::ExprId call_argument_for_param(const std::span<const syntax::ExprId> args,
        const syntax::ExprId callee, const base::u32 receiver_arg_count, const base::u32 param_index) const
    {
        if (receiver_arg_count != 0 && param_index < receiver_arg_count) {
            const std::optional<syntax::ExprId> receiver = this->receiver_expr_for_call_callee(callee);
            return receiver.has_value() ? *receiver : syntax::INVALID_EXPR_ID;
        }
        const base::u32 arg_index = param_index - receiver_arg_count;
        return arg_index < args.size() ? args[arg_index] : syntax::INVALID_EXPR_ID;
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
        if (this->side_tables_ != nullptr) {
            if (this->side_tables_->sparse) {
                const base::usize local_index = this->side_tables_->local_expr_index(expr);
                if (local_index != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
                    && local_index < this->side_tables_->expr_types.size()) {
                    return this->side_tables_->expr_types[local_index];
                }
                const auto found = this->side_tables_->sparse_expr_types.find(expr.value);
                return found == this->side_tables_->sparse_expr_types.end() ? INVALID_TYPE_HANDLE : found->second;
            }
            return syntax::is_valid(expr) && expr.value < this->side_tables_->expr_types.size()
                ? this->side_tables_->expr_types[expr.value]
                : INVALID_TYPE_HANDLE;
        }
        return valid_expr(this->module_, expr) && expr.value < this->checked_.expr_types.size()
            ? this->checked_.expr_types[expr.value]
            : INVALID_TYPE_HANDLE;
    }

    [[nodiscard]] TypeHandle cached_stmt_local_type(const syntax::StmtId stmt) const noexcept
    {
        if (this->side_tables_ != nullptr) {
            if (this->side_tables_->sparse) {
                const base::usize local_index = this->side_tables_->local_stmt_index(stmt);
                if (local_index != SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
                    && local_index < this->side_tables_->stmt_local_types.size()) {
                    return this->side_tables_->stmt_local_types[local_index];
                }
                const auto found = this->side_tables_->sparse_stmt_local_types.find(stmt.value);
                return found == this->side_tables_->sparse_stmt_local_types.end() ? INVALID_TYPE_HANDLE : found->second;
            }
            return valid_stmt(this->module_, stmt) && stmt.value < this->side_tables_->stmt_local_types.size()
                ? this->side_tables_->stmt_local_types[stmt.value]
                : INVALID_TYPE_HANDLE;
        }
        return valid_stmt(this->module_, stmt) && stmt.value < this->checked_.stmt_local_types.size()
            ? this->checked_.stmt_local_types[stmt.value]
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

    [[nodiscard]] bool type_needs_drop(const TypeHandle type) const
    {
        return is_valid(type) && type.value < this->checked_.types.size()
            && resource_needs_drop(this->resources_.classify(type));
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
                case TypeKind::trait_object:
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

    void push_if_expression_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::IfExprPayload& payload = *this->module_.exprs.if_payload(expr.value);
        const base::u32 condition_done = this->new_sequence_point(expr_range(this->module_, expr));
        this->push_expression(payload.condition, entry, condition_done, BodyFlowExprContext::branch, return_continuation);
        this->push_expression(payload.then_expr, condition_done, exit, BodyFlowExprContext::value, return_continuation);
        this->push_expression(payload.else_expr, condition_done, exit, BodyFlowExprContext::value, return_continuation);
    }

    void push_block_expression_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::BlockExprPayload& payload = *this->module_.exprs.block_payload(expr.value);
        const base::u32 block_done = this->new_sequence_point(expr_range(this->module_, expr));
        this->push_statement(payload.block, entry, block_done, return_continuation);
        this->push_expression(payload.result, block_done, exit, BodyFlowExprContext::value, return_continuation);
    }

    void push_match_expression_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::MatchExprPayload& payload = *this->module_.exprs.match_payload(expr.value);
        const base::u32 value_done = this->new_sequence_point(expr_range(this->module_, expr));
        this->push_expression(payload.value, entry, value_done, BodyFlowExprContext::branch, return_continuation);
        if (payload.arms.empty()) {
            this->add_edge(value_done, exit);
            return;
        }
        for (const syntax::MatchArm& arm : payload.arms) {
            const base::u32 guard_done = this->new_sequence_point(arm.range);
            this->push_expression(arm.guard, value_done, guard_done, BodyFlowExprContext::branch, return_continuation);
            this->push_expression(arm.value, guard_done, exit, BodyFlowExprContext::value, return_continuation);
        }
    }

    void push_array_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::ArrayExprPayload& payload = *this->module_.exprs.array_payload(expr.value);
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(payload.elements.size() + 2);
        for (const syntax::ExprId element : payload.elements) {
            steps.push_back(BodyFlowExpressionStep{element, BodyFlowExprContext::value});
        }
        if (array_repeat_value_should_be_visited(array_repeat_runtime_semantics(
                this->module_, this->checked_.types, this->cached_expr_type(expr), expr))) {
            steps.push_back(BodyFlowExpressionStep{payload.repeat_value, BodyFlowExprContext::value});
        }
        steps.push_back(BodyFlowExpressionStep{payload.repeat_count, BodyFlowExprContext::value});
        this->push_expression_sequence(steps, entry, exit, return_continuation);
    }

    void push_tuple_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::AstArenaVector<syntax::ExprId>& elements = *this->module_.exprs.tuple_elements(expr.value);
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(elements.size());
        for (const syntax::ExprId element : elements) {
            steps.push_back(BodyFlowExpressionStep{element, BodyFlowExprContext::value});
        }
        this->push_expression_sequence(steps, entry, exit, return_continuation);
    }

    void push_struct_literal_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::StructLiteralExprPayload& payload = *this->module_.exprs.struct_literal_payload(expr.value);
        std::vector<BodyFlowExpressionStep> steps;
        steps.reserve(payload.field_inits.size() + 1);
        steps.push_back(BodyFlowExpressionStep{payload.object, BodyFlowExprContext::place_observe});
        for (const syntax::FieldInit& field : payload.field_inits) {
            steps.push_back(BodyFlowExpressionStep{field.value, BodyFlowExprContext::value});
        }
        this->push_expression_sequence(steps, entry, exit, return_continuation);
    }

    void push_cast_like_children(
        const syntax::ExprId expr, const base::u32 entry, const base::u32 exit, const base::u32 return_continuation)
    {
        const syntax::CastExprPayload& payload = *this->module_.exprs.cast_payload(expr.value);
        this->push_expression(payload.expr, entry, exit, BodyFlowExprContext::value, return_continuation);
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

    [[nodiscard]] bool expr_is_field_projection(const syntax::ExprId expr) const noexcept
    {
        return valid_expr(this->module_, expr) && this->module_.exprs.kind(expr.value) == syntax::ExprKind::field;
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
        const TypeHandle object_type = this->cached_expr_type(payload.object);
        if (is_valid(object_type) && object_type.value < this->checked_.types.size()) {
            const TypeInfo& info = this->checked_.types.get(object_type);
            const std::optional<base::u32> element_index = parse_tuple_projection_index(payload.field_name);
            if (info.kind == TypeKind::tuple && element_index.has_value()
                && *element_index < info.tuple_elements.size()) {
                reversed_projections.push_back(BodyFlowPlaceProjection{
                    .kind = BodyFlowPlaceProjectionKind::tuple_element,
                    .field_name_id = INVALID_IDENT_ID,
                    .element_index = *element_index,
                    .expr = expr,
                });
                return payload.object;
            }
        }
        reversed_projections.push_back(BodyFlowPlaceProjection{
            .kind = BodyFlowPlaceProjectionKind::field,
            .field_name_id = this->resolve_name_id(payload.field_name_id, payload.field_name),
            .element_index = SEMA_BODY_FLOW_INVALID_INDEX,
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
    ResourceSemanticsClassifier resources_;
    const GenericSideTables* side_tables_ = nullptr;
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
        case BodyFlowPlaceProjectionKind::tuple_element:
            return "tuple_element";
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
                if (projection.element_index == SEMA_BODY_FLOW_INVALID_INDEX) {
                    stream << '-';
                } else {
                    stream << projection.element_index;
                }
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
    BodyFlowGraph graph = BodyFlowGraphBuilder(this->core_.ctx_.module, this->core_.state_.checked, key, function.body,
        this->core_.state_.flow.current_side_tables.side_tables)
                              .build();
    this->core_.state_.checked.body_flow_graphs[key] = std::move(graph);
}

void SemanticAnalyzerCore::collect_body_flow_graph(const syntax::ItemNode& function, const FunctionLookupKey& key)
{
    BodyFlowAnalyzer(*this).collect(function, key);
}

} // namespace aurex::sema
