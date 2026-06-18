#include <aurex/frontend/sema/sema_messages.hpp>

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frontend/sema/internal/borrow/private/contract.hpp>
#include <frontend/sema/internal/borrow/private/flow_graph.hpp>
#include <aurex/frontend/sema/call_arguments.hpp>

#include <frontend/sema/internal/borrow/private/loan_checker.hpp>
#include <frontend/sema/internal/core/private/sema_array_repeat_semantics.hpp>
#include <frontend/sema/internal/expressions/private/sema_statement_analyzer.hpp>

namespace aurex::sema {

SemanticAnalyzerCore::StatementAnalyzer::StatementAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

namespace {

constexpr base::usize SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY = 16;
constexpr base::usize SEMA_CONTROL_FLOW_FIRST_CHILD_INDEX = 0;
constexpr base::usize SEMA_FOR_RANGE_MAX_OPERAND_COUNT = 3;
constexpr base::u8 SEMA_CONTROL_FLOW_CACHE_UNKNOWN = 0;
constexpr base::u8 SEMA_CONTROL_FLOW_CACHE_FALSE = 1;
constexpr base::u8 SEMA_CONTROL_FLOW_CACHE_TRUE = 2;
constexpr std::string_view SEMA_LAMBDA_SYMBOL_PREFIX = "__aurex_lambda";
constexpr std::string_view SEMA_LAMBDA_ENV_SYMBOL_SUFFIX = "_env";
constexpr std::string_view SEMA_LAMBDA_CAPTURE_FIELD_PREFIX = "__capture_";

enum class ControlFlowQuery {
    guarantees_return,
    may_fallthrough,
};

enum class ControlFlowCacheKind {
    statement,
    block,
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
    ControlFlowCacheKind cache_kind = ControlFlowCacheKind::statement;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    base::usize next_child = SEMA_CONTROL_FLOW_FIRST_CHILD_INDEX;
    ControlFlowIfStage if_stage = ControlFlowIfStage::evaluate_then;
};

struct ControlFlowQueryCaches {
    std::vector<base::u8>& statements;
    std::vector<base::u8>& blocks;
};

enum class LambdaCaptureScanActionKind {
    statement,
    scoped_block,
    lambda_body,
    pattern_scoped_block,
    pattern_scoped_expr,
    block_statements,
    pop_scope,
    pattern_bindings,
    lambda_param_bindings,
    local_binding,
    expr,
};

struct LambdaCaptureScanAction {
    LambdaCaptureScanActionKind kind = LambdaCaptureScanActionKind::statement;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::StmtId block = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
    IdentId name_id = INVALID_IDENT_ID;
    base::SourceRange range{};
    std::string_view name{};
    const syntax::LambdaExprPayload* lambda = nullptr;
};

struct LambdaCaptureLocalBinding {
    IdentId name_id = INVALID_IDENT_ID;
    base::SourceRange range{};
    std::string_view name{};
    base::usize lambda_depth = 0;
};

struct LambdaCaptureCandidate {
    IdentId name_id = INVALID_IDENT_ID;
    std::string_view name{};
    TypeHandle type = INVALID_TYPE_HANDLE;
    syntax::LambdaCaptureKind kind = syntax::LambdaCaptureKind::value;
    syntax::ExprId initializer = syntax::INVALID_EXPR_ID;
    bool source_is_mutable = false;
    base::SourceRange use_range{};
    base::SourceRange declaration_range{};
};

struct LambdaCaptureListEntry {
    syntax::LambdaCaptureKind kind = syntax::LambdaCaptureKind::value;
    syntax::ExprId initializer = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
};

[[nodiscard]] std::optional<syntax::StmtNode> statement_node(const syntax::AstModule& module, const syntax::StmtId stmt)
{
    if (!syntax::is_valid(stmt) || stmt.value >= module.stmts.size()) {
        return std::nullopt;
    }
    return module.stmts[stmt.value];
}

[[nodiscard]] bool statement_valid_expr(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size();
}

[[nodiscard]] bool statement_valid_pattern(const syntax::AstModule& module, const syntax::PatternId pattern) noexcept
{
    return syntax::is_valid(pattern) && pattern.value < module.patterns.size();
}

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module, const syntax::ExprId expr, const base::SourceRange& fallback) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return fallback;
    }
    return module.exprs.range(expr.value);
}

void push_lambda_capture_expr(
    std::vector<LambdaCaptureScanAction>& pending, const syntax::AstModule& module, const syntax::ExprId expr)
{
    if (statement_valid_expr(module, expr)) {
        pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::expr, .expr = expr});
    }
}

void push_lambda_capture_statement(
    std::vector<LambdaCaptureScanAction>& pending, const syntax::AstModule& module, const syntax::StmtId stmt)
{
    if (syntax::is_valid(stmt) && stmt.value < module.stmts.size()) {
        pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::statement, .stmt = stmt});
    }
}

void push_lambda_capture_pattern_bindings(
    std::vector<LambdaCaptureScanAction>& pending, const syntax::AstModule& module, const syntax::PatternId pattern)
{
    if (statement_valid_pattern(module, pattern)) {
        pending.push_back(
            LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::pattern_bindings, .pattern = pattern});
    }
}

void push_lambda_capture_scoped_block(
    std::vector<LambdaCaptureScanAction>& pending, const syntax::AstModule& module, const syntax::StmtId block)
{
    if (syntax::is_valid(block) && block.value < module.stmts.size()) {
        pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::scoped_block, .stmt = block});
    }
}

class LambdaCaptureScanner final {
public:
    LambdaCaptureScanner(SemanticAnalyzerCore& core, const std::span<const syntax::ParamDecl> params,
        const std::span<const syntax::LambdaCaptureDecl> captures)
        : core_(core)
    {
        this->local_scopes_.push_back({});
        this->local_scopes_.back().reserve(params.size() + captures.size());
        for (const syntax::LambdaCaptureDecl& capture : captures) {
            if (syntax::is_valid(capture.initializer)) {
                this->bind_local(capture.name_id, capture.range, capture.name);
            }
        }
        for (const syntax::ParamDecl& param : params) {
            this->bind_local(param.name_id, param.range, param.name);
        }
    }

    [[nodiscard]] std::vector<LambdaCaptureCandidate> scan(const syntax::StmtId body)
    {
        std::vector<LambdaCaptureScanAction> pending;
        pending.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
        push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, body);
        while (!pending.empty()) {
            const LambdaCaptureScanAction action = pending.back();
            pending.pop_back();
            this->handle_action(action, pending);
        }
        return std::move(this->captures_);
    }

private:
    void handle_action(
        const LambdaCaptureScanAction& action, std::vector<LambdaCaptureScanAction>& pending)
    {
        switch (action.kind) {
            case LambdaCaptureScanActionKind::statement:
                this->push_statement(action.stmt, pending);
                break;
            case LambdaCaptureScanActionKind::scoped_block:
                this->push_scope();
                pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::pop_scope});
                pending.push_back(
                    LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::block_statements, .stmt = action.stmt});
                break;
            case LambdaCaptureScanActionKind::lambda_body:
                this->push_scope();
                this->push_lambda_boundary();
                pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::pop_scope});
                pending.push_back(LambdaCaptureScanAction{
                    .kind = LambdaCaptureScanActionKind::block_statements,
                    .stmt = action.lambda != nullptr ? action.lambda->body : syntax::INVALID_STMT_ID,
                });
                pending.push_back(LambdaCaptureScanAction{
                    .kind = LambdaCaptureScanActionKind::lambda_param_bindings,
                    .lambda = action.lambda,
                });
                break;
            case LambdaCaptureScanActionKind::pattern_scoped_block:
                this->push_scope();
                pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::pop_scope});
                pending.push_back(LambdaCaptureScanAction{
                    .kind = LambdaCaptureScanActionKind::block_statements,
                    .stmt = action.block,
                });
                push_lambda_capture_pattern_bindings(pending, this->core_.ctx_.module, action.pattern);
                break;
            case LambdaCaptureScanActionKind::pattern_scoped_expr:
                this->push_scope();
                pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::pop_scope});
                push_lambda_capture_expr(pending, this->core_.ctx_.module, action.expr);
                push_lambda_capture_pattern_bindings(pending, this->core_.ctx_.module, action.pattern);
                break;
            case LambdaCaptureScanActionKind::block_statements:
                this->push_block_statements(action.stmt, pending);
                break;
            case LambdaCaptureScanActionKind::pop_scope:
                this->pop_scope();
                break;
            case LambdaCaptureScanActionKind::pattern_bindings:
                this->bind_pattern(action.pattern);
                break;
            case LambdaCaptureScanActionKind::lambda_param_bindings:
                this->bind_lambda_params(action.lambda);
                break;
            case LambdaCaptureScanActionKind::local_binding:
                this->bind_local(action.name_id, action.range, action.name);
                break;
            case LambdaCaptureScanActionKind::expr:
                this->push_expr(action.expr, pending);
                break;
        }
    }

    void push_statement(const syntax::StmtId stmt_id, std::vector<LambdaCaptureScanAction>& pending)
    {
        const std::optional<syntax::StmtNode> stmt = statement_node(this->core_.ctx_.module, stmt_id);
        if (!stmt.has_value()) {
            return;
        }
        switch (stmt->kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                if (syntax::is_valid(stmt->pattern)) {
                    if (syntax::is_valid(stmt->else_block)) {
                        pending.push_back(LambdaCaptureScanAction{
                            .kind = LambdaCaptureScanActionKind::pattern_bindings,
                            .pattern = stmt->pattern,
                        });
                        push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, stmt->else_block);
                    } else {
                        pending.push_back(LambdaCaptureScanAction{
                            .kind = LambdaCaptureScanActionKind::pattern_bindings,
                            .pattern = stmt->pattern,
                        });
                    }
                } else if (syntax::is_valid(stmt->name_id)) {
                    pending.push_back(LambdaCaptureScanAction{
                        .kind = LambdaCaptureScanActionKind::local_binding,
                        .stmt = stmt_id,
                        .name_id = stmt->name_id,
                        .range = stmt->range,
                        .name = stmt->name,
                    });
                    if (syntax::is_valid(stmt->else_block)) {
                        push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, stmt->else_block);
                    }
                }
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->init);
                break;
            case syntax::StmtKind::assign:
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->rhs);
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->lhs);
                break;
            case syntax::StmtKind::if_:
                if (syntax::is_valid(stmt->else_if)) {
                    push_lambda_capture_statement(pending, this->core_.ctx_.module, stmt->else_if);
                }
                if (syntax::is_valid(stmt->else_block)) {
                    push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, stmt->else_block);
                }
                if (syntax::is_valid(stmt->pattern)) {
                    pending.push_back(LambdaCaptureScanAction{
                        .kind = LambdaCaptureScanActionKind::pattern_scoped_block,
                        .block = stmt->then_block,
                        .pattern = stmt->pattern,
                    });
                } else {
                    push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, stmt->then_block);
                }
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->condition);
                break;
            case syntax::StmtKind::while_:
                if (syntax::is_valid(stmt->pattern)) {
                    pending.push_back(LambdaCaptureScanAction{
                        .kind = LambdaCaptureScanActionKind::pattern_scoped_block,
                        .block = stmt->body,
                        .pattern = stmt->pattern,
                    });
                } else {
                    push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, stmt->body);
                }
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->condition);
                break;
            case syntax::StmtKind::for_:
                this->push_scope();
                pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::pop_scope});
                if (syntax::is_valid(stmt->for_update)) {
                    push_lambda_capture_statement(pending, this->core_.ctx_.module, stmt->for_update);
                }
                push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, stmt->body);
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->condition);
                if (syntax::is_valid(stmt->for_init)) {
                    push_lambda_capture_statement(pending, this->core_.ctx_.module, stmt->for_init);
                }
                break;
            case syntax::StmtKind::for_range:
                this->push_scope();
                pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::pop_scope});
                push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, stmt->body);
                pending.push_back(LambdaCaptureScanAction{
                    .kind = LambdaCaptureScanActionKind::local_binding,
                    .name_id = stmt->name_id,
                    .range = stmt->range,
                    .name = stmt->name,
                });
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->range_step);
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->range_end);
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->range_start);
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->range_iterable);
                break;
            case syntax::StmtKind::return_:
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->return_value);
                break;
            case syntax::StmtKind::expr:
            case syntax::StmtKind::defer:
                push_lambda_capture_expr(pending, this->core_.ctx_.module, stmt->init);
                break;
            case syntax::StmtKind::block:
                push_lambda_capture_scoped_block(pending, this->core_.ctx_.module, stmt_id);
                break;
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
                break;
        }
    }

    void push_block_statements(const syntax::StmtId block, std::vector<LambdaCaptureScanAction>& pending) const
    {
        if (!syntax::is_valid(block) || block.value >= this->core_.ctx_.module.stmts.size()) {
            return;
        }
        const syntax::AstArenaVector<syntax::StmtId>* const statements =
            this->core_.ctx_.module.stmts.block_statements(block.value);
        if (statements == nullptr) {
            return;
        }
        for (base::usize index = statements->size(); index > 0; --index) {
            push_lambda_capture_statement(pending, this->core_.ctx_.module, (*statements)[index - 1]);
        }
    }

    void push_expr(const syntax::ExprId expr, std::vector<LambdaCaptureScanAction>& pending)
    {
        if (!statement_valid_expr(this->core_.ctx_.module, expr)) {
            return;
        }
        const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(expr.value);
        if (kind == syntax::ExprKind::name) {
            this->report_capture_if_needed(expr);
            return;
        }
        switch (kind) {
            case syntax::ExprKind::unary: {
                const syntax::UnaryExprPayload* const unary = this->core_.ctx_.module.exprs.unary_payload(expr.value);
                if (unary != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, unary->operand);
                }
                break;
            }
            case syntax::ExprKind::try_expr: {
                const syntax::TryExprPayload* const try_expr = this->core_.ctx_.module.exprs.try_payload(expr.value);
                if (try_expr != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, try_expr->operand);
                }
                break;
            }
            case syntax::ExprKind::binary: {
                const syntax::BinaryExprPayload* const binary =
                    this->core_.ctx_.module.exprs.binary_payload(expr.value);
                if (binary != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, binary->rhs);
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, binary->lhs);
                }
                break;
            }
            case syntax::ExprKind::call:
            case syntax::ExprKind::str_from_bytes_unchecked: {
                const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(expr.value);
                if (call != nullptr) {
                    const std::span<const syntax::ExprId> call_args = kind == syntax::ExprKind::call
                        ? checked_ordered_call_args_or_source(this->core_.state_.checked, expr, *call)
                        : std::span<const syntax::ExprId>{call->args.data(), call->args.size()};
                    for (const syntax::ExprId arg : call_args) {
                        push_lambda_capture_expr(pending, this->core_.ctx_.module, arg);
                    }
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, call->callee);
                }
                break;
            }
            case syntax::ExprKind::generic_apply: {
                const syntax::GenericApplyExprPayload* const apply =
                    this->core_.ctx_.module.exprs.generic_apply_payload(expr.value);
                if (apply != nullptr) {
                    for (const syntax::GenericArgDecl& arg : apply->generic_args) {
                        if (arg.kind == syntax::GenericArgKind::const_expr) {
                            push_lambda_capture_expr(pending, this->core_.ctx_.module, arg.const_expr);
                        }
                    }
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, apply->callee);
                }
                break;
            }
            case syntax::ExprKind::if_expr: {
                const syntax::IfExprPayload* const if_expr = this->core_.ctx_.module.exprs.if_payload(expr.value);
                if (if_expr != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, if_expr->else_expr);
                    if (syntax::is_valid(if_expr->condition_pattern)) {
                        pending.push_back(LambdaCaptureScanAction{
                            .kind = LambdaCaptureScanActionKind::pattern_scoped_expr,
                            .expr = if_expr->then_expr,
                            .pattern = if_expr->condition_pattern,
                        });
                    } else {
                        push_lambda_capture_expr(pending, this->core_.ctx_.module, if_expr->then_expr);
                    }
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, if_expr->condition);
                }
                break;
            }
            case syntax::ExprKind::block_expr:
            case syntax::ExprKind::unsafe_block: {
                const syntax::BlockExprPayload* const block = this->core_.ctx_.module.exprs.block_payload(expr.value);
                if (block != nullptr) {
                    this->push_scope();
                    pending.push_back(LambdaCaptureScanAction{.kind = LambdaCaptureScanActionKind::pop_scope});
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, block->result);
                    pending.push_back(LambdaCaptureScanAction{
                        .kind = LambdaCaptureScanActionKind::block_statements,
                        .stmt = block->block,
                    });
                }
                break;
            }
            case syntax::ExprKind::match_expr: {
                const syntax::MatchExprPayload* const match = this->core_.ctx_.module.exprs.match_payload(expr.value);
                if (match != nullptr) {
                    for (const syntax::MatchArm& arm : match->arms) {
                        if (syntax::is_valid(arm.guard)) {
                            pending.push_back(LambdaCaptureScanAction{
                                .kind = LambdaCaptureScanActionKind::pattern_scoped_expr,
                                .expr = arm.guard,
                                .pattern = arm.pattern,
                            });
                        }
                        pending.push_back(LambdaCaptureScanAction{
                            .kind = LambdaCaptureScanActionKind::pattern_scoped_expr,
                            .expr = arm.value,
                            .pattern = arm.pattern,
                        });
                    }
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, match->value);
                }
                break;
            }
            case syntax::ExprKind::array_literal: {
                const syntax::ArrayExprPayload* const array = this->core_.ctx_.module.exprs.array_payload(expr.value);
                if (array != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, array->repeat_count);
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, array->repeat_value);
                    for (const syntax::ExprId element : array->elements) {
                        push_lambda_capture_expr(pending, this->core_.ctx_.module, element);
                    }
                }
                break;
            }
            case syntax::ExprKind::tuple_literal: {
                const syntax::AstArenaVector<syntax::ExprId>* const tuple =
                    this->core_.ctx_.module.exprs.tuple_elements(expr.value);
                if (tuple != nullptr) {
                    for (const syntax::ExprId element : *tuple) {
                        push_lambda_capture_expr(pending, this->core_.ctx_.module, element);
                    }
                }
                break;
            }
            case syntax::ExprKind::field: {
                const syntax::FieldExprPayload* const field = this->core_.ctx_.module.exprs.field_payload(expr.value);
                if (field != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, field->object);
                }
                break;
            }
            case syntax::ExprKind::index: {
                const syntax::IndexExprPayload* const index = this->core_.ctx_.module.exprs.index_payload(expr.value);
                if (index != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, index->index);
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, index->object);
                }
                break;
            }
            case syntax::ExprKind::slice: {
                const syntax::SliceExprPayload* const slice = this->core_.ctx_.module.exprs.slice_payload(expr.value);
                if (slice != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, slice->end);
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, slice->start);
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, slice->object);
                }
                break;
            }
            case syntax::ExprKind::struct_literal: {
                const syntax::StructLiteralExprPayload* const structure =
                    this->core_.ctx_.module.exprs.struct_literal_payload(expr.value);
                if (structure != nullptr) {
                    for (const syntax::FieldInit& field : structure->field_inits) {
                        push_lambda_capture_expr(pending, this->core_.ctx_.module, field.value);
                    }
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, structure->object);
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
                const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(expr.value);
                if (cast != nullptr) {
                    push_lambda_capture_expr(pending, this->core_.ctx_.module, cast->expr);
                }
                break;
            }
            case syntax::ExprKind::lambda:
                if (const syntax::LambdaExprPayload* const lambda =
                        this->core_.ctx_.module.exprs.lambda_payload(expr.value);
                    lambda != nullptr) {
                    pending.push_back(LambdaCaptureScanAction{
                        .kind = LambdaCaptureScanActionKind::lambda_body,
                        .lambda = lambda,
                    });
                    for (base::usize index = lambda->captures.size(); index > 0; --index) {
                        push_lambda_capture_expr(
                            pending, this->core_.ctx_.module, lambda->captures[index - 1].initializer);
                    }
                }
                break;
            case syntax::ExprKind::name:
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
                break;
        }
    }

    void bind_lambda_params(const syntax::LambdaExprPayload* const lambda)
    {
        if (lambda == nullptr) {
            return;
        }
        for (const syntax::LambdaCaptureDecl& capture : lambda->captures) {
            if (syntax::is_valid(capture.initializer)) {
                this->bind_local(capture.name_id, capture.range, capture.name);
            }
        }
        for (const syntax::ParamDecl& param : lambda->params) {
            this->bind_local(param.name_id, param.range, param.name);
        }
    }

    void bind_pattern(const syntax::PatternId pattern)
    {
        std::vector<syntax::PatternId> pending;
        pending.push_back(pattern);
        while (!pending.empty()) {
            const syntax::PatternId current = pending.back();
            pending.pop_back();
            if (!statement_valid_pattern(this->core_.ctx_.module, current)) {
                continue;
            }
            const syntax::PatternNode* const node = this->core_.ctx_.module.patterns.ptr(current.value);
            if (node == nullptr) {
                continue;
            }
            switch (node->kind) {
                case syntax::PatternKind::binding:
                    this->bind_local(node->binding_name_id, node->range, node->binding_name);
                    break;
                case syntax::PatternKind::literal:
                case syntax::PatternKind::enum_case:
                    for (const IdentId binding : node->binding_name_ids) {
                        this->bind_local(binding, node->range, {});
                    }
                    for (const syntax::PatternId payload : node->payload_patterns) {
                        pending.push_back(payload);
                    }
                    break;
                case syntax::PatternKind::tuple:
                case syntax::PatternKind::slice:
                    for (const syntax::PatternId element : node->elements) {
                        pending.push_back(element);
                    }
                    break;
                case syntax::PatternKind::struct_:
                    for (const syntax::FieldPattern& field : node->field_patterns) {
                        pending.push_back(field.pattern);
                    }
                    break;
                case syntax::PatternKind::or_pattern:
                    for (const syntax::PatternId alternative : node->alternatives) {
                        pending.push_back(alternative);
                    }
                    break;
                case syntax::PatternKind::wildcard:
                case syntax::PatternKind::const_:
                    break;
            }
        }
    }

    void report_capture_if_needed(const syntax::ExprId expr)
    {
        const syntax::NameExprPayload* const name = this->core_.ctx_.module.exprs.name_payload(expr.value);
        if (name == nullptr || !name->scope_name.empty() || this->is_local_to_current_lambda(name->text_id)) {
            return;
        }
        if (const LambdaCaptureLocalBinding* const local_binding = this->outer_lambda_binding(name->text_id);
            local_binding != nullptr) {
            return;
        }
        const Symbol* const symbol = this->core_.state_.names.symbols.find(name->text_id);
        if (symbol == nullptr || (symbol->kind != SymbolKind::local && symbol->kind != SymbolKind::parameter)) {
            return;
        }
        this->record_capture(expr, name->text, *symbol);
    }

    void record_capture(const syntax::ExprId expr, const std::string_view name, const Symbol& symbol)
    {
        if (!is_valid(symbol.name_id) || this->captured_name_ids_.contains(symbol.name_id)) {
            return;
        }
        this->captured_name_ids_.insert(symbol.name_id);
        this->captures_.push_back(LambdaCaptureCandidate{
            symbol.name_id,
            name.empty() ? symbol.name.view() : name,
            symbol.type,
            syntax::LambdaCaptureKind::value,
            syntax::INVALID_EXPR_ID,
            symbol.is_mutable,
            expr_range_or(this->core_.ctx_.module, expr, symbol.range),
            symbol.range,
        });
    }

    void push_scope()
    {
        this->local_scopes_.push_back({});
    }

    void pop_scope() noexcept
    {
        if (this->local_scopes_.size() > 1U) {
            this->local_scopes_.pop_back();
        }
        if (this->lambda_scope_depths_.empty()) {
            return;
        }
        if (this->local_scopes_.size() < this->lambda_scope_depths_.back()) {
            this->pop_lambda_boundary();
        }
    }

    void bind_local(const IdentId name_id, const base::SourceRange& range, const std::string_view name)
    {
        if (!is_valid(name_id) || this->local_scopes_.empty()) {
            return;
        }
        this->local_scopes_.back().push_back(
            LambdaCaptureLocalBinding{name_id, range, name, this->current_lambda_depth_});
    }

    void push_lambda_boundary()
    {
        ++this->current_lambda_depth_;
        this->lambda_scope_depths_.push_back(this->local_scopes_.size());
    }

    void pop_lambda_boundary() noexcept
    {
        if (this->current_lambda_depth_ > 0) {
            --this->current_lambda_depth_;
        }
        if (!this->lambda_scope_depths_.empty()) {
            this->lambda_scope_depths_.pop_back();
        }
    }

    [[nodiscard]] const LambdaCaptureLocalBinding* local_binding(const IdentId name_id) const noexcept
    {
        if (!is_valid(name_id)) {
            return nullptr;
        }
        for (auto scope = this->local_scopes_.rbegin(); scope != this->local_scopes_.rend(); ++scope) {
            for (auto binding = scope->rbegin(); binding != scope->rend(); ++binding) {
                if (binding->name_id == name_id) {
                    return &*binding;
                }
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool is_local_to_current_lambda(const IdentId name_id) const noexcept
    {
        const LambdaCaptureLocalBinding* const binding = this->local_binding(name_id);
        return binding != nullptr && binding->lambda_depth == this->current_lambda_depth_;
    }

    [[nodiscard]] const LambdaCaptureLocalBinding* outer_lambda_binding(const IdentId name_id) const noexcept
    {
        const LambdaCaptureLocalBinding* const binding = this->local_binding(name_id);
        if (binding == nullptr || binding->lambda_depth >= this->current_lambda_depth_) {
            return nullptr;
        }
        return binding;
    }

    SemanticAnalyzerCore& core_;
    std::vector<std::vector<LambdaCaptureLocalBinding>> local_scopes_;
    std::vector<base::usize> lambda_scope_depths_;
    std::unordered_set<IdentId, IdentIdHash> captured_name_ids_;
    std::vector<LambdaCaptureCandidate> captures_;
    base::usize current_lambda_depth_ = 0;
};

[[nodiscard]] std::string lambda_symbol_name(
    const syntax::ModuleId module, const syntax::ItemId item, const syntax::ExprId expr)
{
    std::string name(SEMA_LAMBDA_SYMBOL_PREFIX);
    name.push_back('_');
    name += "m";
    name += std::to_string(module.value);
    name.push_back('_');
    name += "i";
    name += std::to_string(item.value);
    name.push_back('_');
    name += "e";
    name += std::to_string(expr.value);
    return name;
}

[[nodiscard]] std::string lambda_environment_symbol_name(const std::string_view lambda_symbol)
{
    std::string name(lambda_symbol);
    name += SEMA_LAMBDA_ENV_SYMBOL_SUFFIX;
    return name;
}

[[nodiscard]] std::string lambda_capture_field_name(const base::usize index, const std::string_view capture_name)
{
    std::string name(SEMA_LAMBDA_CAPTURE_FIELD_PREFIX);
    name += std::to_string(index);
    if (!capture_name.empty()) {
        name.push_back('_');
        name += capture_name;
    }
    return name;
}

[[nodiscard]] bool lambda_capture_kind_is_reference(const syntax::LambdaCaptureKind kind) noexcept
{
    return kind == syntax::LambdaCaptureKind::shared_reference
        || kind == syntax::LambdaCaptureKind::mutable_reference;
}

[[nodiscard]] bool lambda_capture_kind_is_default(const syntax::LambdaCaptureKind kind) noexcept
{
    return kind == syntax::LambdaCaptureKind::default_value || kind == syntax::LambdaCaptureKind::default_reference;
}

[[nodiscard]] bool lambda_capture_kind_is_mutable(const syntax::LambdaCaptureKind kind) noexcept
{
    return kind == syntax::LambdaCaptureKind::mutable_reference;
}

[[nodiscard]] bool lambda_capture_has_initializer(const LambdaCaptureCandidate& capture) noexcept
{
    return syntax::is_valid(capture.initializer);
}

[[nodiscard]] bool lambda_capture_requires_copy(const LambdaCaptureCandidate& capture) noexcept
{
    return capture.kind == syntax::LambdaCaptureKind::value && !lambda_capture_has_initializer(capture);
}

[[nodiscard]] syntax::LambdaCaptureKind lambda_capture_default_explicit_kind(
    const syntax::LambdaCaptureKind kind) noexcept
{
    if (kind == syntax::LambdaCaptureKind::default_reference) {
        return syntax::LambdaCaptureKind::shared_reference;
    }
    return syntax::LambdaCaptureKind::value;
}

[[nodiscard]] bool lambda_capture_redundant_with_default(
    const syntax::LambdaCaptureKind default_kind, const syntax::LambdaCaptureKind explicit_kind) noexcept
{
    if (lambda_capture_default_explicit_kind(default_kind) == syntax::LambdaCaptureKind::value) {
        return explicit_kind == syntax::LambdaCaptureKind::value;
    }
    return explicit_kind == syntax::LambdaCaptureKind::shared_reference;
}

[[nodiscard]] TypeHandle lambda_capture_environment_field_type(
    TypeTable& types, const LambdaCaptureCandidate& capture)
{
    if (lambda_capture_kind_is_reference(capture.kind)) {
        return types.pointer(PointerMutability::mut, capture.type);
    }
    return capture.type;
}

[[nodiscard]] TypeHandle lambda_checked_capture_environment_field_type(
    TypeTable& types, const CheckedLambdaInfo::Capture& capture)
{
    if (is_valid(capture.field_type)) {
        return capture.field_type;
    }
    if (lambda_capture_kind_is_reference(capture.kind)) {
        return types.pointer(PointerMutability::mut, capture.type);
    }
    return capture.type;
}

[[nodiscard]] bool lambda_captures_contain_array(
    const TypeTable& types, const std::span<const LambdaCaptureCandidate> captures) noexcept
{
    for (const LambdaCaptureCandidate& capture : captures) {
        if (types.contains_array(capture.type)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool lambda_capture_type_can_contain_borrow(
    const SemanticAnalyzerCore& core, const TypeHandle type)
{
    if (!is_valid(type)) {
        return false;
    }
    std::vector<TypeHandle> pending{type};
    std::unordered_set<base::u32> visited;
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || current.value >= core.state_.checked.types.size()
            || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = core.state_.checked.types.get(current);
        switch (info.kind) {
            case TypeKind::builtin:
                if (info.builtin == BuiltinType::str) {
                    return true;
                }
                break;
            case TypeKind::reference:
            case TypeKind::slice:
            case TypeKind::trait_object:
                return true;
            case TypeKind::array:
                pending.push_back(info.array_element);
                break;
            case TypeKind::tuple:
                pending.insert(pending.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                break;
            case TypeKind::struct_: {
                const StructInfo* const structure = core.find_struct(current);
                if (structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case TypeKind::enum_: {
                const SemanticAnalyzerCore::EnumCaseList* const cases = core.find_enum_cases_by_type(current);
                if (cases != nullptr) {
                    for (const EnumCaseInfo* const enum_case : *cases) {
                        if (enum_case != nullptr) {
                            pending.insert(
                                pending.end(), enum_case->payload_types.begin(), enum_case->payload_types.end());
                        }
                    }
                }
                break;
            }
            case TypeKind::generic_param:
            case TypeKind::associated_projection:
            case TypeKind::pointer:
            case TypeKind::function:
            case TypeKind::opaque_struct:
                break;
        }
    }
    return false;
}

[[nodiscard]] bool lambda_capture_type_is_generic_dependent(
    const SemanticAnalyzerCore& core, const TypeHandle type)
{
    if (!is_valid(type)) {
        return false;
    }
    std::vector<TypeHandle> pending{type};
    std::unordered_set<base::u32> visited;
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || current.value >= core.state_.checked.types.size()
            || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = core.state_.checked.types.get(current);
        switch (info.kind) {
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
                const StructInfo* const structure = core.find_struct(current);
                if (structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case TypeKind::enum_: {
                const auto* const cases = core.find_enum_cases_by_type(current);
                if (cases != nullptr) {
                    for (const EnumCaseInfo* const enum_case : *cases) {
                        if (enum_case != nullptr) {
                            pending.insert(
                                pending.end(), enum_case->payload_types.begin(), enum_case->payload_types.end());
                        }
                    }
                }
                break;
            }
            case TypeKind::builtin:
            case TypeKind::reference:
            case TypeKind::slice:
            case TypeKind::pointer:
            case TypeKind::function:
            case TypeKind::opaque_struct:
            case TypeKind::trait_object:
                break;
        }
    }
    return false;
}

void report_noncopy_lambda_capture(
    SemanticAnalyzerCore& core, const LambdaCaptureCandidate& capture)
{
    core.report_general(capture.use_range, std::string(SEMA_LAMBDA_CAPTURE_COPY_UNSUPPORTED));
    core.report_note(capture.declaration_range, SemanticDiagnosticKind::general,
        "captured value `" + std::string(capture.name) + "` is declared here");
}

void report_borrowed_view_lambda_capture(
    SemanticAnalyzerCore& core, const LambdaCaptureCandidate& capture)
{
    core.report_general(capture.use_range, std::string(SEMA_LAMBDA_CAPTURE_BORROW_UNSUPPORTED));
    core.report_note(capture.declaration_range, SemanticDiagnosticKind::general,
        "captured value `" + std::string(capture.name) + "` is declared here");
}

void report_generic_dependent_lambda_capture(
    SemanticAnalyzerCore& core, const LambdaCaptureCandidate& capture)
{
    core.report_general(capture.use_range, std::string(SEMA_LAMBDA_CAPTURE_GENERIC_UNSUPPORTED));
    core.report_note(capture.declaration_range, SemanticDiagnosticKind::general,
        "captured value `" + std::string(capture.name) + "` is declared here");
}

void check_lambda_capture_mode(SemanticAnalyzerCore& core, const LambdaCaptureCandidate& capture,
    const LambdaCaptureListEntry& entry, bool& has_unsupported_capture)
{
    if (entry.kind != syntax::LambdaCaptureKind::mutable_reference || capture.source_is_mutable) {
        return;
    }
    core.report_general(entry.range, std::string(SEMA_LAMBDA_CAPTURE_MUTABLE_REQUIRES_MUTABLE_SOURCE));
    core.report_note(capture.declaration_range, SemanticDiagnosticKind::general,
        "captured value `" + std::string(capture.name) + "` is declared here");
    has_unsupported_capture = true;
}

void check_lambda_capture_list(SemanticAnalyzerCore& core, const SemanticAnalyzerCore::ExprView& expr,
    const std::span<LambdaCaptureCandidate> captures, bool& has_unsupported_capture)
{
    std::unordered_map<IdentId, LambdaCaptureListEntry, IdentIdHash> listed;
    listed.reserve(expr.lambda_captures.size());
    std::optional<LambdaCaptureListEntry> default_capture;
    bool seen_named_capture = false;
    for (const syntax::LambdaCaptureDecl& capture : expr.lambda_captures) {
        if (lambda_capture_kind_is_default(capture.kind)) {
            if (default_capture.has_value()) {
                core.report_general(capture.range, std::string(SEMA_LAMBDA_CAPTURE_DEFAULT_DUPLICATE));
                has_unsupported_capture = true;
            }
            if (seen_named_capture) {
                core.report_general(capture.range, std::string(SEMA_LAMBDA_CAPTURE_DEFAULT_FIRST));
                has_unsupported_capture = true;
            }
            if (!default_capture.has_value()) {
                default_capture = LambdaCaptureListEntry{
                    capture.kind,
                    syntax::INVALID_EXPR_ID,
                    capture.range,
                };
            }
            continue;
        }
        if (!is_valid(capture.name_id)) {
            continue;
        }
        seen_named_capture = true;
        if (!syntax::is_valid(capture.initializer) && default_capture.has_value()
            && lambda_capture_redundant_with_default(default_capture->kind, capture.kind)) {
            core.report_general(capture.range, std::string(SEMA_LAMBDA_CAPTURE_REDUNDANT_WITH_DEFAULT));
            has_unsupported_capture = true;
        }
        const auto inserted = listed.emplace(
            capture.name_id, LambdaCaptureListEntry{capture.kind, capture.initializer, capture.range});
        if (!inserted.second) {
            core.report_general(capture.range, std::string(SEMA_LAMBDA_CAPTURE_DUPLICATE));
            has_unsupported_capture = true;
        }
    }

    std::unordered_set<IdentId, IdentIdHash> used;
    used.reserve(captures.size());
    for (LambdaCaptureCandidate& capture : captures) {
        if (!is_valid(capture.name_id)) {
            continue;
        }
        used.insert(capture.name_id);
        const auto listed_capture = listed.find(capture.name_id);
        if (listed_capture == listed.end()) {
            if (default_capture.has_value()) {
                capture.kind = lambda_capture_default_explicit_kind(default_capture->kind);
                continue;
            }
            core.report_general(capture.use_range, std::string(SEMA_LAMBDA_CAPTURE_NOT_LISTED));
            core.report_note(capture.declaration_range, SemanticDiagnosticKind::general,
                "captured value `" + std::string(capture.name) + "` is declared here");
            has_unsupported_capture = true;
            continue;
        }
        capture.kind = listed_capture->second.kind;
        capture.initializer = listed_capture->second.initializer;
        check_lambda_capture_mode(core, capture, listed_capture->second, has_unsupported_capture);
    }

    for (const syntax::LambdaCaptureDecl& capture : expr.lambda_captures) {
        if (is_valid(capture.name_id) && !syntax::is_valid(capture.initializer) && !used.contains(capture.name_id)) {
            core.report_general(capture.range, std::string(SEMA_LAMBDA_CAPTURE_UNUSED));
            has_unsupported_capture = true;
        }
    }
}

void order_lambda_captures_by_capture_list(
    const SemanticAnalyzerCore::ExprView& expr, std::vector<LambdaCaptureCandidate>& captures)
{
    if (captures.empty()) {
        return;
    }
    std::unordered_map<IdentId, base::usize, IdentIdHash> capture_indexes;
    capture_indexes.reserve(captures.size());
    for (base::usize index = 0; index < captures.size(); ++index) {
        if (is_valid(captures[index].name_id)) {
            capture_indexes.emplace(captures[index].name_id, index);
        }
    }

    std::unordered_set<IdentId, IdentIdHash> ordered_names;
    ordered_names.reserve(captures.size());
    std::vector<LambdaCaptureCandidate> ordered;
    ordered.reserve(captures.size());
    for (const syntax::LambdaCaptureDecl& declaration : expr.lambda_captures) {
        if (lambda_capture_kind_is_default(declaration.kind) || !is_valid(declaration.name_id)
            || ordered_names.contains(declaration.name_id)) {
            continue;
        }
        const auto found = capture_indexes.find(declaration.name_id);
        if (found == capture_indexes.end()) {
            continue;
        }
        ordered_names.insert(declaration.name_id);
        ordered.push_back(std::move(captures[found->second]));
    }
    for (LambdaCaptureCandidate& capture : captures) {
        if (is_valid(capture.name_id) && ordered_names.contains(capture.name_id)) {
            continue;
        }
        if (is_valid(capture.name_id)) {
            ordered_names.insert(capture.name_id);
        }
        ordered.push_back(std::move(capture));
    }
    captures = std::move(ordered);
}

void append_lambda_init_captures(SemanticAnalyzerCore& core, const SemanticAnalyzerCore::ExprView& expr,
    std::vector<LambdaCaptureCandidate>& captures, bool& has_unsupported_capture)
{
    std::unordered_set<IdentId, IdentIdHash> existing;
    existing.reserve(captures.size());
    for (const LambdaCaptureCandidate& capture : captures) {
        if (is_valid(capture.name_id)) {
            existing.insert(capture.name_id);
        }
    }
    for (const syntax::LambdaCaptureDecl& capture : expr.lambda_captures) {
        if (!is_valid(capture.name_id) || !syntax::is_valid(capture.initializer)) {
            continue;
        }
        if (existing.contains(capture.name_id)) {
            continue;
        }
        TypeHandle capture_type = INVALID_TYPE_HANDLE;
        bool source_is_mutable = false;
        if (lambda_capture_kind_is_reference(capture.kind)) {
            const SemanticAnalyzerCore::PlaceInfo place = core.analyze_place_info(capture.initializer, true);
            core.require_place_projection_safety(place, capture.range);
            capture_type = place.type;
            source_is_mutable = place.is_writable;
            if (capture.kind == syntax::LambdaCaptureKind::mutable_reference && !place.is_writable) {
                core.report_general(capture.range, std::string(SEMA_LAMBDA_CAPTURE_MUTABLE_REQUIRES_MUTABLE_SOURCE));
                has_unsupported_capture = true;
            }
        } else {
            capture_type = core.analyze_expr(capture.initializer);
        }
        captures.push_back(LambdaCaptureCandidate{
            capture.name_id,
            capture.name,
            capture_type,
            capture.kind,
            capture.initializer,
            source_is_mutable,
            expr_range_or(core.ctx_.module, capture.initializer, capture.range),
            capture.range,
        });
        existing.insert(capture.name_id);
    }
}

[[nodiscard]] bool expr_is_indexed_or_dereferenced_place(
    const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return false;
    }
    const syntax::ExprKind kind = module.exprs.kind(expr.value);
    if (kind == syntax::ExprKind::index) {
        return true;
    }
    const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(expr.value);
    return kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::dereference;
}

[[nodiscard]] bool expr_is_field_place(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size()
        && module.exprs.kind(expr.value) == syntax::ExprKind::field;
}

void push_storage_escape_guard_expr(
    std::vector<syntax::ExprId>& pending, const syntax::AstModule& module, const syntax::ExprId expr)
{
    if (statement_valid_expr(module, expr)) {
        pending.push_back(expr);
    }
}

[[nodiscard]] bool expr_may_produce_storage_escape_carrier(
    const syntax::AstModule& module, const syntax::ExprId expr)
{
    std::vector<syntax::ExprId> pending;
    pending.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
    push_storage_escape_guard_expr(pending, module, expr);
    while (!pending.empty()) {
        const syntax::ExprId current = pending.back();
        pending.pop_back();
        if (!statement_valid_expr(module, current)) {
            continue;
        }
        const syntax::ExprKind kind = module.exprs.kind(current.value);
        switch (kind) {
            case syntax::ExprKind::unary: {
                const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(current.value);
                if (unary == nullptr) {
                    break;
                }
                if (unary->op == syntax::UnaryOp::address_of || unary->op == syntax::UnaryOp::address_of_mut
                    || unary->op == syntax::UnaryOp::dereference) {
                    return true;
                }
                push_storage_escape_guard_expr(pending, module, unary->operand);
                break;
            }
            case syntax::ExprKind::call:
            case syntax::ExprKind::ptr_addr:
            case syntax::ExprKind::paddr:
            case syntax::ExprKind::slice:
            case syntax::ExprKind::slice_data:
            case syntax::ExprKind::str_data:
            case syntax::ExprKind::str_from_utf8_checked:
            case syntax::ExprKind::str_from_bytes_unchecked:
                return true;
            case syntax::ExprKind::generic_apply: {
                const syntax::GenericApplyExprPayload* const apply = module.exprs.generic_apply_payload(current.value);
                if (apply != nullptr) {
                    push_storage_escape_guard_expr(pending, module, apply->callee);
                }
                break;
            }
            case syntax::ExprKind::binary: {
                const syntax::BinaryExprPayload* const binary = module.exprs.binary_payload(current.value);
                if (binary != nullptr) {
                    push_storage_escape_guard_expr(pending, module, binary->lhs);
                    push_storage_escape_guard_expr(pending, module, binary->rhs);
                }
                break;
            }
            case syntax::ExprKind::try_expr:
            case syntax::ExprKind::cast:
            case syntax::ExprKind::pcast:
            case syntax::ExprKind::bcast:
            case syntax::ExprKind::size_of:
            case syntax::ExprKind::align_of: {
                const syntax::CastExprPayload* const cast = module.exprs.cast_payload(current.value);
                const syntax::TryExprPayload* const try_expr = module.exprs.try_payload(current.value);
                if (cast != nullptr) {
                    push_storage_escape_guard_expr(pending, module, cast->expr);
                } else if (try_expr != nullptr) {
                    push_storage_escape_guard_expr(pending, module, try_expr->operand);
                }
                break;
            }
            case syntax::ExprKind::if_expr: {
                const syntax::IfExprPayload* const if_expr = module.exprs.if_payload(current.value);
                if (if_expr != nullptr) {
                    push_storage_escape_guard_expr(pending, module, if_expr->then_expr);
                    push_storage_escape_guard_expr(pending, module, if_expr->else_expr);
                }
                break;
            }
            case syntax::ExprKind::block_expr:
            case syntax::ExprKind::unsafe_block: {
                const syntax::BlockExprPayload* const block = module.exprs.block_payload(current.value);
                if (block != nullptr) {
                    push_storage_escape_guard_expr(pending, module, block->result);
                }
                break;
            }
            case syntax::ExprKind::match_expr: {
                const syntax::MatchExprPayload* const match = module.exprs.match_payload(current.value);
                if (match != nullptr) {
                    for (const syntax::MatchArm& arm : match->arms) {
                        push_storage_escape_guard_expr(pending, module, arm.value);
                    }
                }
                break;
            }
            case syntax::ExprKind::array_literal: {
                const syntax::ArrayExprPayload* const array = module.exprs.array_payload(current.value);
                if (array != nullptr) {
                    push_storage_escape_guard_expr(pending, module, array->repeat_value);
                    for (const syntax::ExprId element : array->elements) {
                        push_storage_escape_guard_expr(pending, module, element);
                    }
                }
                break;
            }
            case syntax::ExprKind::tuple_literal: {
                const syntax::AstArenaVector<syntax::ExprId>* const tuple =
                    module.exprs.tuple_elements(current.value);
                if (tuple != nullptr) {
                    for (const syntax::ExprId element : *tuple) {
                        push_storage_escape_guard_expr(pending, module, element);
                    }
                }
                break;
            }
            case syntax::ExprKind::struct_literal: {
                const syntax::StructLiteralExprPayload* const structure =
                    module.exprs.struct_literal_payload(current.value);
                if (structure != nullptr) {
                    for (const syntax::FieldInit& field : structure->field_inits) {
                        push_storage_escape_guard_expr(pending, module, field.value);
                    }
                }
                break;
            }
            case syntax::ExprKind::field: {
                const syntax::FieldExprPayload* const field = module.exprs.field_payload(current.value);
                if (field != nullptr) {
                    push_storage_escape_guard_expr(pending, module, field->object);
                }
                break;
            }
            case syntax::ExprKind::index: {
                const syntax::IndexExprPayload* const index = module.exprs.index_payload(current.value);
                if (index != nullptr) {
                    push_storage_escape_guard_expr(pending, module, index->object);
                    push_storage_escape_guard_expr(pending, module, index->index);
                }
                break;
            }
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
            case syntax::ExprKind::lambda:
            case syntax::ExprKind::invalid:
            case syntax::ExprKind::slice_len:
            case syntax::ExprKind::str_byte_len:
            case syntax::ExprKind::str_is_valid_utf8:
                break;
        }
    }
    return false;
}

[[nodiscard]] bool expr_is_unqualified_name(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return false;
    }
    const syntax::NameExprPayload* const name = module.exprs.name_payload(expr.value);
    return name != nullptr && name->scope_name.empty();
}

[[nodiscard]] bool body_may_need_storage_escape_guard(const syntax::AstModule& module, const syntax::StmtId root)
{
    std::vector<syntax::StmtId> pending;
    pending.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
    if (syntax::is_valid(root)) {
        pending.push_back(root);
    }
    while (!pending.empty()) {
        const syntax::StmtId stmt_id = pending.back();
        pending.pop_back();
        const std::optional<syntax::StmtNode> stmt = statement_node(module, stmt_id);
        if (!stmt.has_value()) {
            continue;
        }
        switch (stmt->kind) {
            case syntax::StmtKind::assign:
                if (syntax::is_valid(stmt->lhs) && !expr_is_unqualified_name(module, stmt->lhs)
                    && expr_may_produce_storage_escape_carrier(module, stmt->rhs)) {
                    return true;
                }
                break;
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                if (syntax::is_valid(stmt->else_block)) {
                    pending.push_back(stmt->else_block);
                }
                break;
            case syntax::StmtKind::if_:
                if (syntax::is_valid(stmt->then_block)) {
                    pending.push_back(stmt->then_block);
                }
                if (syntax::is_valid(stmt->else_if)) {
                    pending.push_back(stmt->else_if);
                }
                if (syntax::is_valid(stmt->else_block)) {
                    pending.push_back(stmt->else_block);
                }
                break;
            case syntax::StmtKind::while_:
            case syntax::StmtKind::for_range:
                if (syntax::is_valid(stmt->body)) {
                    pending.push_back(stmt->body);
                }
                break;
            case syntax::StmtKind::for_:
                if (syntax::is_valid(stmt->for_init)) {
                    pending.push_back(stmt->for_init);
                }
                if (syntax::is_valid(stmt->body)) {
                    pending.push_back(stmt->body);
                }
                if (syntax::is_valid(stmt->for_update)) {
                    pending.push_back(stmt->for_update);
                }
                break;
            case syntax::StmtKind::block: {
                const syntax::AstArenaVector<syntax::StmtId>* const statements =
                    module.stmts.block_statements(stmt_id.value);
                if (statements != nullptr) {
                    for (const syntax::StmtId child : *statements) {
                        if (syntax::is_valid(child)) {
                            pending.push_back(child);
                        }
                    }
                }
                break;
            }
            case syntax::StmtKind::return_:
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
            case syntax::StmtKind::defer:
            case syntax::StmtKind::expr:
                break;
        }
    }
    return false;
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

[[nodiscard]] bool control_flow_cache_key_is_valid(
    const syntax::AstModule& module, const syntax::StmtId stmt) noexcept
{
    return syntax::is_valid(stmt) && stmt.value < module.stmts.size();
}

[[nodiscard]] std::vector<base::u8>& control_flow_cache_bucket(
    ControlFlowQueryCaches& caches, const ControlFlowCacheKind kind) noexcept
{
    return kind == ControlFlowCacheKind::block ? caches.blocks : caches.statements;
}

[[nodiscard]] const std::vector<base::u8>& control_flow_cache_bucket(
    const ControlFlowQueryCaches& caches, const ControlFlowCacheKind kind) noexcept
{
    return kind == ControlFlowCacheKind::block ? caches.blocks : caches.statements;
}

[[nodiscard]] std::optional<bool> read_control_flow_cache(
    const syntax::AstModule& module, const ControlFlowQueryCaches& caches, const ControlFlowCacheKind kind,
    const syntax::StmtId stmt) noexcept
{
    if (!control_flow_cache_key_is_valid(module, stmt)) {
        return std::nullopt;
    }
    const std::vector<base::u8>& cache = control_flow_cache_bucket(caches, kind);
    if (stmt.value >= cache.size()) {
        return std::nullopt;
    }
    const base::u8 value = cache[stmt.value];
    if (value == SEMA_CONTROL_FLOW_CACHE_TRUE) {
        return true;
    }
    if (value == SEMA_CONTROL_FLOW_CACHE_FALSE) {
        return false;
    }
    return std::nullopt;
}

void write_control_flow_cache(const syntax::AstModule& module, ControlFlowQueryCaches& caches,
    const ControlFlowCacheKind kind, const syntax::StmtId stmt, const bool result)
{
    if (!control_flow_cache_key_is_valid(module, stmt)) {
        return;
    }
    std::vector<base::u8>& cache = control_flow_cache_bucket(caches, kind);
    if (cache.size() < module.stmts.size()) {
        cache.resize(module.stmts.size(), SEMA_CONTROL_FLOW_CACHE_UNKNOWN);
    }
    cache[stmt.value] = result ? SEMA_CONTROL_FLOW_CACHE_TRUE : SEMA_CONTROL_FLOW_CACHE_FALSE;
}

void cache_control_flow_frame_result(const syntax::AstModule& module, ControlFlowQueryCaches& caches,
    const ControlFlowFrame& frame, const bool result)
{
    write_control_flow_cache(module, caches, frame.cache_kind, frame.stmt, result);
}

void propagate_control_flow_result(const syntax::AstModule& module, std::vector<ControlFlowFrame>& stack,
    ControlFlowQueryCaches& caches, const ControlFlowQuery query, const bool result, bool& has_result,
    bool& final_result)
{
    while (!stack.empty()) {
        ControlFlowFrame& parent = stack.back();
        switch (parent.kind) {
            case ControlFlowFrameKind::block:
                if (block_short_circuits(query, result)) {
                    cache_control_flow_frame_result(module, caches, parent, result);
                    stack.pop_back();
                    continue;
                }
                ++parent.next_child;
                return;
            case ControlFlowFrameKind::if_statement:
                if (parent.if_stage == ControlFlowIfStage::after_then) {
                    if (if_then_short_circuits(query, result)) {
                        cache_control_flow_frame_result(module, caches, parent, result);
                        stack.pop_back();
                        continue;
                    }
                    parent.if_stage = ControlFlowIfStage::evaluate_alternate;
                    return;
                }
                if (parent.if_stage == ControlFlowIfStage::after_alternate) {
                    cache_control_flow_frame_result(module, caches, parent, result);
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

void finish_control_flow_frame(std::vector<ControlFlowFrame>& stack, const ControlFlowQuery query, const bool result,
    const syntax::AstModule& module, ControlFlowQueryCaches& caches, bool& has_result, bool& final_result)
{
    const ControlFlowFrame frame = stack.back();
    cache_control_flow_frame_result(module, caches, frame, result);
    stack.pop_back();
    propagate_control_flow_result(module, stack, caches, query, result, has_result, final_result);
}

void evaluate_control_flow_statement(const syntax::AstModule& module, std::vector<ControlFlowFrame>& stack,
    ControlFlowQueryCaches& caches, const ControlFlowQuery query, bool& has_result, bool& final_result)
{
    ControlFlowFrame& frame = stack.back();
    if (!syntax::is_valid(frame.stmt) || frame.stmt.value >= module.stmts.size()) {
        finish_control_flow_frame(
            stack, query, default_control_flow_result(query), module, caches, has_result, final_result);
        return;
    }
    const syntax::StmtKind kind = module.stmts.kind(frame.stmt.value);
    switch (kind) {
        case syntax::StmtKind::return_:
        case syntax::StmtKind::break_:
        case syntax::StmtKind::continue_:
            finish_control_flow_frame(
                stack, query, abrupt_stmt_result(query, kind), module, caches, has_result, final_result);
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
            finish_control_flow_frame(
                stack, query, default_control_flow_result(query), module, caches, has_result, final_result);
            break;
    }
}

void evaluate_control_flow_block(const syntax::AstModule& module, std::vector<ControlFlowFrame>& stack,
    ControlFlowQueryCaches& caches, const ControlFlowQuery query, bool& has_result, bool& final_result)
{
    ControlFlowFrame& frame = stack.back();
    if (!syntax::is_valid(frame.stmt) || frame.stmt.value >= module.stmts.size()) {
        finish_control_flow_frame(
            stack, query, default_control_flow_result(query), module, caches, has_result, final_result);
        return;
    }
    const syntax::AstArenaVector<syntax::StmtId>* const statements = module.stmts.block_statements(frame.stmt.value);
    if (statements == nullptr) {
        frame.kind = ControlFlowFrameKind::statement;
        return;
    }
    if (frame.next_child >= statements->size()) {
        finish_control_flow_frame(
            stack, query, default_control_flow_result(query), module, caches, has_result, final_result);
        return;
    }
    const syntax::StmtId child = (*statements)[frame.next_child];
    if (const std::optional<bool> cached =
            read_control_flow_cache(module, caches, ControlFlowCacheKind::statement, child);
        cached.has_value()) {
        propagate_control_flow_result(module, stack, caches, query, *cached, has_result, final_result);
        return;
    }
    stack.push_back(ControlFlowFrame{ControlFlowFrameKind::statement, ControlFlowCacheKind::statement, child});
}

void evaluate_control_flow_if_statement(const syntax::AstModule& module, std::vector<ControlFlowFrame>& stack,
    ControlFlowQueryCaches& caches, const ControlFlowQuery query, bool& has_result, bool& final_result)
{
    ControlFlowFrame& frame = stack.back();
    const std::optional<syntax::StmtNode> node = statement_node(module, frame.stmt);
    if (!node.has_value() || node->kind != syntax::StmtKind::if_) {
        finish_control_flow_frame(
            stack, query, default_control_flow_result(query), module, caches, has_result, final_result);
        return;
    }
    if (frame.if_stage == ControlFlowIfStage::evaluate_then) {
        frame.if_stage = ControlFlowIfStage::after_then;
        if (const std::optional<bool> cached =
                read_control_flow_cache(module, caches, ControlFlowCacheKind::block, node->then_block);
            cached.has_value()) {
            propagate_control_flow_result(module, stack, caches, query, *cached, has_result, final_result);
            return;
        }
        stack.push_back(ControlFlowFrame{ControlFlowFrameKind::block, ControlFlowCacheKind::block, node->then_block});
        return;
    }
    if (frame.if_stage == ControlFlowIfStage::evaluate_alternate) {
        frame.if_stage = ControlFlowIfStage::after_alternate;
        if (syntax::is_valid(node->else_block)) {
            if (const std::optional<bool> cached =
                    read_control_flow_cache(module, caches, ControlFlowCacheKind::block, node->else_block);
                cached.has_value()) {
                propagate_control_flow_result(module, stack, caches, query, *cached, has_result, final_result);
                return;
            }
            stack.push_back(
                ControlFlowFrame{ControlFlowFrameKind::block, ControlFlowCacheKind::block, node->else_block});
            return;
        }
        if (syntax::is_valid(node->else_if)) {
            if (const std::optional<bool> cached =
                    read_control_flow_cache(module, caches, ControlFlowCacheKind::statement, node->else_if);
                cached.has_value()) {
                propagate_control_flow_result(module, stack, caches, query, *cached, has_result, final_result);
                return;
            }
            stack.push_back(
                ControlFlowFrame{ControlFlowFrameKind::statement, ControlFlowCacheKind::statement, node->else_if});
            return;
        }
        finish_control_flow_frame(
            stack, query, default_control_flow_result(query), module, caches, has_result, final_result);
    }
}

[[nodiscard]] bool evaluate_control_flow(const syntax::AstModule& module, const syntax::StmtId stmt,
    const ControlFlowFrameKind root_kind, const ControlFlowCacheKind root_cache_kind, const ControlFlowQuery query,
    ControlFlowQueryCaches caches)
{
    if (const std::optional<bool> cached = read_control_flow_cache(module, caches, root_cache_kind, stmt);
        cached.has_value()) {
        return *cached;
    }
    std::vector<ControlFlowFrame> stack;
    stack.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
    stack.push_back(ControlFlowFrame{root_kind, root_cache_kind, stmt});

    bool has_result = false;
    bool final_result = default_control_flow_result(query);
    while (!has_result && !stack.empty()) {
        switch (stack.back().kind) {
            case ControlFlowFrameKind::statement:
                evaluate_control_flow_statement(module, stack, caches, query, has_result, final_result);
                break;
            case ControlFlowFrameKind::block:
                evaluate_control_flow_block(module, stack, caches, query, has_result, final_result);
                break;
            case ControlFlowFrameKind::if_statement:
                evaluate_control_flow_if_statement(module, stack, caches, query, has_result, final_result);
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

[[nodiscard]] bool source_range_contains(const base::SourceRange& outer, const base::SourceRange& inner) noexcept
{
    return outer.source.value == inner.source.value && inner.begin >= outer.begin && inner.end <= outer.end;
}

[[nodiscard]] bool expr_range_contains_try_expr(const syntax::AstModule& module, const syntax::ExprId root_expr)
{
    if (!syntax::is_valid(root_expr) || root_expr.value >= module.exprs.size()) {
        return false;
    }
    const base::SourceRange root_range = module.exprs.range(root_expr.value);
    for (base::usize index = 0; index < module.exprs.size(); ++index) {
        if (module.exprs.kind(index) == syntax::ExprKind::try_expr
            && source_range_contains(root_range, module.exprs.range(index))) {
            return true;
        }
    }
    return false;
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

class SemanticAnalyzerCore::BorrowEscapeAnalyzer final {
public:
    explicit BorrowEscapeAnalyzer(SemanticAnalyzerCore& core, const bool include_return_escapes) noexcept
        : core_(core), include_return_escapes_(include_return_escapes)
    {
    }

    void analyze_function(const syntax::ItemNode& function)
    {
        this->scopes_.clear();
        this->type_borrow_cache_.clear();
        this->reported_exprs_.clear();
        this->push_scope();
        for (const syntax::ParamDecl& param : function.params) {
            this->bind_storage(param.name_id, BorrowOrigin{param.range, true});
            this->bind_borrowed_value(param.name_id, {});
        }
        std::vector<Task> tasks;
        tasks.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
        this->push_scoped_block(tasks, function.body);
        this->run_tasks(tasks);
    }

private:
    struct BorrowOrigin {
        base::SourceRange range{};
        bool present = false;
    };

    struct BorrowedValueBinding {
        BorrowOrigin origin{};
    };

    struct Scope {
        std::unordered_map<IdentId, BorrowOrigin, IdentIdHash> storage;
        std::unordered_map<IdentId, BorrowedValueBinding, IdentIdHash> borrowed_values;
        std::unordered_map<IdentId, BorrowOrigin, IdentIdHash> pointer_values;
    };

    enum class TaskKind {
        scoped_block,
        block_statements,
        statement,
        pop_scope,
    };

    struct Task {
        TaskKind kind = TaskKind::statement;
        syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    };

    void push_scope()
    {
        this->scopes_.emplace_back();
    }

    void pop_scope()
    {
        if (!this->scopes_.empty()) {
            this->scopes_.pop_back();
        }
    }

    void bind_storage(const IdentId name, const BorrowOrigin origin)
    {
        if (!is_valid(name) || this->scopes_.empty()) {
            return;
        }
        this->scopes_.back().storage[name] = origin;
    }

    void bind_borrowed_value(const IdentId name, const BorrowOrigin origin)
    {
        if (!is_valid(name) || this->scopes_.empty()) {
            return;
        }
        this->scopes_.back().borrowed_values[name] = BorrowedValueBinding{origin};
    }

    void bind_pointer_value(const IdentId name, const BorrowOrigin origin)
    {
        if (!is_valid(name) || this->scopes_.empty()) {
            return;
        }
        this->scopes_.back().pointer_values[name] = origin;
    }

    void assign_borrowed_value(const IdentId name, const BorrowOrigin origin)
    {
        if (!is_valid(name)) {
            return;
        }
        for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
            if (scope->storage.contains(name)) {
                scope->borrowed_values[name] = BorrowedValueBinding{origin};
                return;
            }
        }
        this->bind_borrowed_value(name, origin);
    }

    void assign_pointer_value(const IdentId name, const BorrowOrigin origin)
    {
        if (!is_valid(name)) {
            return;
        }
        for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
            if (scope->storage.contains(name)) {
                scope->pointer_values[name] = origin;
                return;
            }
        }
        this->bind_pointer_value(name, origin);
    }

    [[nodiscard]] BorrowOrigin lookup_storage(const IdentId name) const
    {
        if (!is_valid(name)) {
            return {};
        }
        for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
            const auto found = scope->storage.find(name);
            if (found != scope->storage.end()) {
                return found->second;
            }
        }
        return {};
    }

    [[nodiscard]] BorrowOrigin lookup_borrowed_value(const IdentId name) const
    {
        if (!is_valid(name)) {
            return {};
        }
        for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
            const auto found = scope->borrowed_values.find(name);
            if (found != scope->borrowed_values.end()) {
                return found->second.origin;
            }
        }
        return {};
    }

    [[nodiscard]] BorrowOrigin lookup_pointer_value(const IdentId name) const
    {
        if (!is_valid(name)) {
            return {};
        }
        for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
            const auto found = scope->pointer_values.find(name);
            if (found != scope->pointer_values.end()) {
                return found->second;
            }
        }
        return {};
    }

    void push_scoped_block(std::vector<Task>& tasks, const syntax::StmtId block) const
    {
        if (!syntax::is_valid(block)) {
            return;
        }
        tasks.push_back(Task{TaskKind::scoped_block, block});
    }

    void push_statement(std::vector<Task>& tasks, const syntax::StmtId stmt) const
    {
        if (!syntax::is_valid(stmt)) {
            return;
        }
        tasks.push_back(Task{TaskKind::statement, stmt});
    }

    void run_tasks(std::vector<Task>& tasks)
    {
        while (!tasks.empty()) {
            const Task task = tasks.back();
            tasks.pop_back();
            switch (task.kind) {
                case TaskKind::scoped_block:
                    this->push_scope();
                    tasks.push_back(Task{TaskKind::pop_scope, syntax::INVALID_STMT_ID});
                    tasks.push_back(Task{TaskKind::block_statements, task.stmt});
                    break;
                case TaskKind::block_statements:
                    this->push_block_statements(tasks, task.stmt);
                    break;
                case TaskKind::statement:
                    this->analyze_statement(tasks, task.stmt);
                    break;
                case TaskKind::pop_scope:
                    this->pop_scope();
                    break;
            }
        }
    }

    void push_block_statements(std::vector<Task>& tasks, const syntax::StmtId block) const
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
            this->push_statement(tasks, (*statements)[i - 1]);
        }
    }

    void analyze_statement(std::vector<Task>& tasks, const syntax::StmtId stmt_id)
    {
        const std::optional<syntax::StmtNode> stmt = statement_node(this->core_.ctx_.module, stmt_id);
        if (!stmt.has_value()) {
            return;
        }
        switch (stmt->kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                this->analyze_local_declaration(stmt_id, *stmt);
                this->push_scoped_block(tasks, stmt->else_block);
                break;
            case syntax::StmtKind::assign:
                this->analyze_assignment(*stmt);
                break;
            case syntax::StmtKind::if_:
                this->push_scoped_block(tasks, stmt->else_block);
                this->push_statement(tasks, stmt->else_if);
                this->push_scoped_block(tasks, stmt->then_block);
                break;
            case syntax::StmtKind::while_:
                this->push_scoped_block(tasks, stmt->body);
                break;
            case syntax::StmtKind::for_:
                this->push_scope();
                tasks.push_back(Task{TaskKind::pop_scope, syntax::INVALID_STMT_ID});
                this->push_statement(tasks, stmt->for_update);
                this->push_scoped_block(tasks, stmt->body);
                this->push_statement(tasks, stmt->for_init);
                break;
            case syntax::StmtKind::for_range:
                this->push_scope();
                tasks.push_back(Task{TaskKind::pop_scope, syntax::INVALID_STMT_ID});
                this->bind_storage(stmt->name_id, BorrowOrigin{stmt->range, true});
                this->bind_borrowed_value(stmt->name_id, {});
                this->push_scoped_block(tasks, stmt->body);
                break;
            case syntax::StmtKind::return_:
                if (this->include_return_escapes_) {
                    this->report_if_borrowed_local_escape(stmt->return_value);
                }
                break;
            case syntax::StmtKind::block:
                this->push_scoped_block(tasks, stmt_id);
                break;
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
            case syntax::StmtKind::defer:
            case syntax::StmtKind::expr:
                break;
        }
    }

    void analyze_local_declaration(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt)
    {
        const TypeHandle local_type = this->core_.cached_stmt_local_type(stmt_id);
        if (syntax::is_valid(stmt.pattern)) {
            this->bind_pattern_storage(stmt.pattern, local_type, stmt.init);
            return;
        }
        if (!is_valid(stmt.name_id)) {
            return;
        }
        const BorrowOrigin origin =
            this->type_can_contain_borrow(local_type) ? this->borrow_origin(stmt.init) : BorrowOrigin{};
        const BorrowOrigin pointer_origin =
            this->core_.state_.checked.types.is_pointer(local_type) ? this->pointer_origin(stmt.init) : BorrowOrigin{};
        this->bind_storage(stmt.name_id, BorrowOrigin{stmt.range, true});
        this->bind_borrowed_value(stmt.name_id, origin);
        this->bind_pointer_value(stmt.name_id, pointer_origin);
    }

    void analyze_assignment(const syntax::StmtNode& stmt)
    {
        const BorrowOrigin origin = this->borrow_origin(stmt.rhs);
        const IdentId assigned_name = this->unqualified_name_id(stmt.lhs);
        if (is_valid(assigned_name)) {
            this->assign_borrowed_value(assigned_name,
                this->type_can_contain_borrow(this->core_.cached_expr_type(stmt.lhs)) ? origin : BorrowOrigin{});
            this->assign_pointer_value(assigned_name,
                this->core_.state_.checked.types.is_pointer(this->core_.cached_expr_type(stmt.lhs))
                    ? this->pointer_origin(stmt.rhs)
                    : BorrowOrigin{});
            return;
        }
        if (origin.present) {
            this->report_escape(stmt.rhs, origin);
        }
    }

    struct PatternFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
        syntax::ExprId source = syntax::INVALID_EXPR_ID;
    };

    void bind_pattern_storage(const syntax::PatternId pattern, const TypeHandle type, const syntax::ExprId source)
    {
        std::vector<PatternFrame> pending{{pattern, type, source}};
        while (!pending.empty()) {
            const PatternFrame frame = pending.back();
            pending.pop_back();
            if (!syntax::is_valid(frame.pattern) || frame.pattern.value >= this->core_.ctx_.module.patterns.size()) {
                continue;
            }
            const syntax::PatternNode* const node = this->core_.ctx_.module.patterns.ptr(frame.pattern.value);
            if (node == nullptr) {
                continue;
            }
            switch (node->kind) {
                case syntax::PatternKind::binding:
                    this->bind_pattern_binding(*node, frame.type, frame.source);
                    break;
                case syntax::PatternKind::tuple:
                    this->push_tuple_pattern_frames(pending, *node, frame.type, frame.source);
                    break;
                case syntax::PatternKind::slice:
                    this->push_slice_pattern_frames(pending, *node, frame.type, frame.source);
                    break;
                case syntax::PatternKind::struct_:
                    this->push_struct_pattern_frames(pending, *node, frame.type, frame.source);
                    break;
                case syntax::PatternKind::enum_case:
                    this->push_enum_pattern_frames(pending, *node, frame.type, frame.source);
                    break;
                case syntax::PatternKind::or_pattern:
                    for (const syntax::PatternId alternative : node->alternatives) {
                        pending.push_back(PatternFrame{alternative, frame.type, frame.source});
                    }
                    break;
                case syntax::PatternKind::wildcard:
                case syntax::PatternKind::literal:
                case syntax::PatternKind::const_:
                    break;
            }
        }
    }

    void bind_pattern_binding(const syntax::PatternNode& pattern, const TypeHandle type, const syntax::ExprId source)
    {
        this->bind_storage(pattern.binding_name_id, BorrowOrigin{pattern.range, true});
        this->bind_borrowed_value(pattern.binding_name_id,
            this->type_can_contain_borrow(type) ? this->borrow_origin(source) : BorrowOrigin{});
        this->bind_pointer_value(pattern.binding_name_id,
            this->core_.state_.checked.types.is_pointer(type) ? this->pointer_origin(source) : BorrowOrigin{});
    }

    void push_tuple_pattern_frames(std::vector<PatternFrame>& pending, const syntax::PatternNode& pattern,
        const TypeHandle type, const syntax::ExprId source) const
    {
        const syntax::AstArenaVector<syntax::ExprId>* const tuple_source =
            syntax::is_valid(source) && source.value < this->core_.ctx_.module.exprs.size()
            ? this->core_.ctx_.module.exprs.tuple_elements(source.value)
            : nullptr;
        const TypeInfo* const tuple_type =
            this->core_.state_.checked.types.is_tuple(type) ? &this->core_.state_.checked.types.get(type) : nullptr;
        for (base::usize index = pattern.elements.size(); index > 0; --index) {
            const base::usize element_index = index - 1;
            const TypeHandle element_type = tuple_type != nullptr && element_index < tuple_type->tuple_elements.size()
                ? tuple_type->tuple_elements[element_index]
                : INVALID_TYPE_HANDLE;
            const syntax::ExprId element_source = tuple_source != nullptr && element_index < tuple_source->size()
                ? (*tuple_source)[element_index]
                : source;
            pending.push_back(PatternFrame{pattern.elements[element_index], element_type, element_source});
        }
    }

    void push_slice_pattern_frames(std::vector<PatternFrame>& pending, const syntax::PatternNode& pattern,
        const TypeHandle type, const syntax::ExprId source) const
    {
        const syntax::ArrayExprPayload* const array_source =
            syntax::is_valid(source) && source.value < this->core_.ctx_.module.exprs.size()
            ? this->core_.ctx_.module.exprs.array_payload(source.value)
            : nullptr;
        TypeHandle element_type = INVALID_TYPE_HANDLE;
        if (is_valid(type) && type.value < this->core_.state_.checked.types.size()) {
            const TypeInfo& info = this->core_.state_.checked.types.get(type);
            if (info.kind == TypeKind::array) {
                element_type = info.array_element;
            } else if (info.kind == TypeKind::slice) {
                element_type = info.slice_element;
            }
        }
        for (base::usize index = pattern.elements.size(); index > 0; --index) {
            const base::usize element_index = index - 1;
            const syntax::ExprId element_source =
                array_source != nullptr && element_index < array_source->elements.size()
                ? array_source->elements[element_index]
                : source;
            pending.push_back(PatternFrame{pattern.elements[element_index], element_type, element_source});
        }
    }

    void push_struct_pattern_frames(std::vector<PatternFrame>& pending, const syntax::PatternNode& pattern,
        const TypeHandle type, const syntax::ExprId source) const
    {
        const StructInfo* const structure = this->core_.find_struct(type);
        const syntax::StructLiteralExprPayload* const struct_source =
            syntax::is_valid(source) && source.value < this->core_.ctx_.module.exprs.size()
            ? this->core_.ctx_.module.exprs.struct_literal_payload(source.value)
            : nullptr;
        for (const syntax::FieldPattern& field : pattern.field_patterns) {
            const StructFieldInfo* const field_info =
                structure == nullptr ? nullptr : this->pattern_struct_field(*structure, field.name_id);
            pending.push_back(PatternFrame{
                field.pattern,
                field_info == nullptr ? INVALID_TYPE_HANDLE : field_info->type,
                struct_field_source(struct_source, field.name_id, source),
            });
        }
    }

    void push_enum_pattern_frames(std::vector<PatternFrame>& pending, const syntax::PatternNode& pattern,
        const TypeHandle type, const syntax::ExprId source) const
    {
        const EnumCaseInfo* const enum_case =
            this->core_.find_enum_case_by_type_and_case(type, pattern.case_name_id, pattern.case_name);
        for (base::usize index = pattern.payload_patterns.size(); index > 0; --index) {
            const base::usize payload_index = index - 1;
            pending.push_back(PatternFrame{
                pattern.payload_patterns[payload_index],
                enum_case == nullptr || payload_index >= enum_case->payload_types.size()
                    ? INVALID_TYPE_HANDLE
                    : enum_case->payload_types[payload_index],
                source,
            });
        }
    }

    [[nodiscard]] syntax::ExprId struct_field_source(const syntax::StructLiteralExprPayload* const source,
        const IdentId field_name, const syntax::ExprId fallback) const noexcept
    {
        if (source == nullptr) {
            return fallback;
        }
        for (const syntax::FieldInit& init : source->field_inits) {
            if (init.name_id == field_name) {
                return init.value;
            }
        }
        return fallback;
    }

    [[nodiscard]] const StructFieldInfo* pattern_struct_field(
        const StructInfo& structure, const IdentId field_name) const noexcept
    {
        for (const StructFieldInfo& field : structure.fields) {
            if (field.name_id == field_name) {
                return &field;
            }
        }
        return nullptr;
    }

    [[nodiscard]] IdentId unqualified_name_id(const syntax::ExprId expr) const noexcept
    {
        if (!syntax::is_valid(expr) || expr.value >= this->core_.ctx_.module.exprs.size()) {
            return INVALID_IDENT_ID;
        }
        const syntax::NameExprPayload* const name = this->core_.ctx_.module.exprs.name_payload(expr.value);
        return name != nullptr && name->scope_name.empty() ? name->text_id : INVALID_IDENT_ID;
    }

    [[nodiscard]] BorrowOrigin borrowed_name_origin(const syntax::ExprId expr) const
    {
        return this->lookup_borrowed_value(this->unqualified_name_id(expr));
    }

    [[nodiscard]] BorrowOrigin pointer_name_origin(const syntax::ExprId expr) const
    {
        return this->lookup_pointer_value(this->unqualified_name_id(expr));
    }

    [[nodiscard]] BorrowOrigin storage_name_origin(const syntax::ExprId expr) const
    {
        return this->lookup_storage(this->unqualified_name_id(expr));
    }

    [[nodiscard]] BorrowOrigin place_storage_origin(const syntax::ExprId expr) const
    {
        syntax::ExprId current = expr;
        bool traversed_projection = false;
        while (syntax::is_valid(current) && current.value < this->core_.ctx_.module.exprs.size()) {
            const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
            if (kind == syntax::ExprKind::name) {
                if (const BorrowOrigin alias = this->borrowed_name_origin(current); alias.present) {
                    return alias;
                }
                if (traversed_projection
                    && this->core_.state_.checked.types.is_reference(this->core_.cached_expr_type(current))) {
                    return {};
                }
                return this->storage_name_origin(current);
            }
            if (const syntax::FieldExprPayload* const field =
                    this->core_.ctx_.module.exprs.field_payload(current.value);
                kind == syntax::ExprKind::field && field != nullptr) {
                traversed_projection = true;
                current = field->object;
                continue;
            }
            if (const syntax::IndexExprPayload* const index =
                    this->core_.ctx_.module.exprs.index_payload(current.value);
                kind == syntax::ExprKind::index && index != nullptr) {
                traversed_projection = true;
                current = index->object;
                continue;
            }
            if (const syntax::UnaryExprPayload* const unary =
                    this->core_.ctx_.module.exprs.unary_payload(current.value);
                kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::dereference) {
                return this->borrowed_name_origin(unary->operand);
            }
            break;
        }
        return {};
    }

    [[nodiscard]] BorrowOrigin slice_source_origin(const syntax::ExprId expr) const
    {
        if (const BorrowOrigin alias = this->borrowed_name_origin(expr); alias.present) {
            return alias;
        }
        const TypeHandle type = this->core_.cached_expr_type(expr);
        if (this->core_.state_.checked.types.is_array(type)) {
            return this->place_storage_origin(expr);
        }
        return {};
    }

    [[nodiscard]] BorrowOrigin borrowed_carrier_origin(const syntax::ExprId expr)
    {
        std::vector<syntax::ExprId> pending{expr};
        std::unordered_set<base::u32> visited;
        while (!pending.empty()) {
            const syntax::ExprId current = pending.back();
            pending.pop_back();
            if (!syntax::is_valid(current) || current.value >= this->core_.ctx_.module.exprs.size()
                || !visited.insert(current.value).second) {
                continue;
            }
            const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
            if (kind == syntax::ExprKind::name) {
                if (const BorrowOrigin origin = this->borrowed_name_origin(current); origin.present) {
                    return origin;
                }
                continue;
            }
            if (const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
                kind == syntax::ExprKind::call && call != nullptr
                && this->type_can_contain_borrow(this->core_.cached_expr_type(current))) {
                if (const BorrowOrigin origin = this->method_receiver_origin(call->callee); origin.present) {
                    return origin;
                }
                for (const syntax::ExprId arg : this->ordered_call_args(current, *call)) {
                    pending.push_back(arg);
                }
                continue;
            }
            if (const syntax::SliceExprPayload* const slice =
                    this->core_.ctx_.module.exprs.slice_payload(current.value);
                kind == syntax::ExprKind::slice && slice != nullptr) {
                if (const BorrowOrigin origin = this->slice_source_origin(slice->object); origin.present) {
                    return origin;
                }
                continue;
            }
            if (const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(current.value);
                cast != nullptr && kind == syntax::ExprKind::str_from_utf8_checked) {
                pending.push_back(cast->expr);
                continue;
            }
            if (const syntax::BlockExprPayload* const block =
                    this->core_.ctx_.module.exprs.block_payload(current.value);
                block != nullptr && (kind == syntax::ExprKind::block_expr || kind == syntax::ExprKind::unsafe_block)) {
                if (const BorrowOrigin origin = this->block_result_borrow_origin(*block); origin.present) {
                    return origin;
                }
            }
        }
        return {};
    }

    [[nodiscard]] BorrowOrigin method_receiver_origin(const syntax::ExprId callee)
    {
        syntax::ExprId current = callee;
        std::unordered_set<base::u32> visited;
        while (syntax::is_valid(current) && current.value < this->core_.ctx_.module.exprs.size()
            && visited.insert(current.value).second) {
            const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
            if (const syntax::GenericApplyExprPayload* const generic =
                    this->core_.ctx_.module.exprs.generic_apply_payload(current.value);
                kind == syntax::ExprKind::generic_apply && generic != nullptr) {
                current = generic->callee;
                continue;
            }
            if (const syntax::FieldExprPayload* const field =
                    this->core_.ctx_.module.exprs.field_payload(current.value);
                kind == syntax::ExprKind::field && field != nullptr) {
                if (const BorrowOrigin origin = this->borrowed_carrier_origin(field->object); origin.present) {
                    return origin;
                }
                return this->place_storage_origin(field->object);
            }
            break;
        }
        return {};
    }

    [[nodiscard]] BorrowOrigin pointer_origin(const syntax::ExprId expr)
    {
        std::vector<syntax::ExprId> pending{expr};
        std::unordered_set<base::u32> visited;
        while (!pending.empty()) {
            const syntax::ExprId current = pending.back();
            pending.pop_back();
            if (!syntax::is_valid(current) || current.value >= this->core_.ctx_.module.exprs.size()
                || !visited.insert(current.value).second) {
                continue;
            }
            const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
            if (kind == syntax::ExprKind::name) {
                if (const BorrowOrigin origin = this->pointer_name_origin(current); origin.present) {
                    return origin;
                }
                continue;
            }
            if (const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
                kind == syntax::ExprKind::call && call != nullptr) {
                for (const syntax::ExprId arg : this->ordered_call_args(current, *call)) {
                    pending.push_back(arg);
                }
                pending.push_back(call->callee);
                continue;
            }
            if (const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(current.value);
                cast != nullptr
                && (kind == syntax::ExprKind::slice_data || kind == syntax::ExprKind::str_data
                    || kind == syntax::ExprKind::cast || kind == syntax::ExprKind::pcast
                    || kind == syntax::ExprKind::bcast || kind == syntax::ExprKind::paddr)) {
                if (kind == syntax::ExprKind::slice_data || kind == syntax::ExprKind::str_data) {
                    if (const BorrowOrigin origin = this->borrowed_carrier_origin(cast->expr); origin.present) {
                        return origin;
                    }
                    continue;
                }
                pending.push_back(cast->expr);
                continue;
            }
            if (const syntax::BlockExprPayload* const block =
                    this->core_.ctx_.module.exprs.block_payload(current.value);
                block != nullptr && (kind == syntax::ExprKind::block_expr || kind == syntax::ExprKind::unsafe_block)) {
                if (const BorrowOrigin origin = this->block_result_pointer_origin(*block); origin.present) {
                    return origin;
                }
            }
        }
        return {};
    }

    [[nodiscard]] BorrowOrigin borrow_origin(const syntax::ExprId expr)
    {
        std::vector<syntax::ExprId> pending{expr};
        std::unordered_set<base::u32> visited;
        while (!pending.empty()) {
            const syntax::ExprId current = pending.back();
            pending.pop_back();
            if (!syntax::is_valid(current) || current.value >= this->core_.ctx_.module.exprs.size()
                || !visited.insert(current.value).second) {
                continue;
            }
            const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
            if (kind != syntax::ExprKind::str_from_bytes_unchecked
                && !this->type_can_contain_borrow(this->core_.cached_expr_type(current))) {
                continue;
            }
            if (kind == syntax::ExprKind::name) {
                if (const BorrowOrigin origin = this->borrowed_name_origin(current); origin.present) {
                    return origin;
                }
                continue;
            }
            if (const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
                kind == syntax::ExprKind::call && call != nullptr) {
                if (const BorrowOrigin origin = this->method_receiver_origin(call->callee); origin.present) {
                    return origin;
                }
                for (const syntax::ExprId arg : this->ordered_call_args(current, *call)) {
                    pending.push_back(arg);
                }
                continue;
            }
            if (const syntax::UnaryExprPayload* const unary =
                    this->core_.ctx_.module.exprs.unary_payload(current.value);
                kind == syntax::ExprKind::unary && unary != nullptr
                && (unary->op == syntax::UnaryOp::address_of || unary->op == syntax::UnaryOp::address_of_mut)) {
                if (const BorrowOrigin origin = this->place_storage_origin(unary->operand); origin.present) {
                    return origin;
                }
                continue;
            }
            if (const syntax::SliceExprPayload* const slice =
                    this->core_.ctx_.module.exprs.slice_payload(current.value);
                kind == syntax::ExprKind::slice && slice != nullptr) {
                if (const BorrowOrigin origin = this->slice_source_origin(slice->object); origin.present) {
                    return origin;
                }
                continue;
            }
            if (const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
                kind == syntax::ExprKind::str_from_bytes_unchecked && call != nullptr && !call->args.empty()) {
                if (const BorrowOrigin origin = this->pointer_origin(call->args.front()); origin.present) {
                    return origin;
                }
                continue;
            }
            if (const syntax::BlockExprPayload* const block =
                    this->core_.ctx_.module.exprs.block_payload(current.value);
                block != nullptr && (kind == syntax::ExprKind::block_expr || kind == syntax::ExprKind::unsafe_block)) {
                if (const BorrowOrigin origin = this->block_result_borrow_origin(*block); origin.present) {
                    return origin;
                }
                continue;
            }
            if (this->push_expression_children_for_origin(pending, current, kind)) {
                continue;
            }
        }
        return {};
    }

    [[nodiscard]] bool push_expression_children_for_origin(
        std::vector<syntax::ExprId>& pending, const syntax::ExprId expr, const syntax::ExprKind kind)
    {
        if (const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(expr.value);
            cast != nullptr && kind == syntax::ExprKind::str_from_utf8_checked) {
            pending.push_back(cast->expr);
            return true;
        }
        if (const syntax::IfExprPayload* const if_expr = this->core_.ctx_.module.exprs.if_payload(expr.value);
            kind == syntax::ExprKind::if_expr && if_expr != nullptr) {
            pending.push_back(if_expr->else_expr);
            pending.push_back(if_expr->then_expr);
            return true;
        }
        if (const syntax::MatchExprPayload* const match = this->core_.ctx_.module.exprs.match_payload(expr.value);
            kind == syntax::ExprKind::match_expr && match != nullptr) {
            for (const syntax::MatchArm& arm : match->arms) {
                pending.push_back(arm.value);
            }
            return true;
        }
        if (const syntax::ArrayExprPayload* const array = this->core_.ctx_.module.exprs.array_payload(expr.value);
            kind == syntax::ExprKind::array_literal && array != nullptr) {
            if (array_repeat_value_should_be_visited(array_repeat_runtime_semantics(this->core_.ctx_.module,
                    this->core_.state_.checked.types, this->core_.cached_expr_type(expr), expr))) {
                pending.push_back(array->repeat_value);
            }
            for (const syntax::ExprId element : array->elements) {
                pending.push_back(element);
            }
            return true;
        }
        if (const syntax::AstArenaVector<syntax::ExprId>* const tuple =
                this->core_.ctx_.module.exprs.tuple_elements(expr.value);
            kind == syntax::ExprKind::tuple_literal && tuple != nullptr) {
            for (const syntax::ExprId element : *tuple) {
                pending.push_back(element);
            }
            return true;
        }
        if (const syntax::StructLiteralExprPayload* const structure =
                this->core_.ctx_.module.exprs.struct_literal_payload(expr.value);
            kind == syntax::ExprKind::struct_literal && structure != nullptr) {
            for (const syntax::FieldInit& field : structure->field_inits) {
                pending.push_back(field.value);
            }
            return true;
        }
        if (const syntax::FieldExprPayload* const field = this->core_.ctx_.module.exprs.field_payload(expr.value);
            kind == syntax::ExprKind::field && field != nullptr) {
            pending.push_back(field->object);
            return true;
        }
        if (const syntax::IndexExprPayload* const index = this->core_.ctx_.module.exprs.index_payload(expr.value);
            kind == syntax::ExprKind::index && index != nullptr) {
            pending.push_back(index->object);
            return true;
        }
        return false;
    }

    [[nodiscard]] std::span<const syntax::ExprId> ordered_call_args(
        const syntax::ExprId call, const syntax::CallExprPayload& payload) const noexcept
    {
        return checked_ordered_call_args_or_source(this->core_.state_.checked, call, payload);
    }

    [[nodiscard]] BorrowOrigin block_result_borrow_origin(const syntax::BlockExprPayload& block)
    {
        return this->block_result_origin(block, [this](const syntax::ExprId result) {
            return this->borrow_origin(result);
        });
    }

    [[nodiscard]] BorrowOrigin block_result_pointer_origin(const syntax::BlockExprPayload& block)
    {
        return this->block_result_origin(block, [this](const syntax::ExprId result) {
            return this->pointer_origin(result);
        });
    }

    template <typename OriginFn>
    [[nodiscard]] BorrowOrigin block_result_origin(const syntax::BlockExprPayload& block, OriginFn origin)
    {
        this->push_scope();
        std::vector<Task> tasks;
        tasks.reserve(SEMA_STATEMENT_TRAVERSAL_INITIAL_STACK_CAPACITY);
        this->push_block_statements(tasks, block.block);
        this->run_tasks(tasks);
        const BorrowOrigin result = origin(block.result);
        this->pop_scope();
        return result;
    }

    [[nodiscard]] bool type_can_contain_borrow(const TypeHandle type) const
    {
        if (!is_valid(type)) {
            return false;
        }
        const auto cached = this->type_borrow_cache_.find(type.value);
        if (cached != this->type_borrow_cache_.end()) {
            return cached->second;
        }

        bool result = false;
        std::vector<TypeHandle> pending{type};
        std::unordered_set<base::u32> visited;
        while (!pending.empty() && !result) {
            const TypeHandle current = pending.back();
            pending.pop_back();
            if (!is_valid(current) || current.value >= this->core_.state_.checked.types.size()
                || !visited.insert(current.value).second) {
                continue;
            }
            const TypeInfo& info = this->core_.state_.checked.types.get(current);
            switch (info.kind) {
                case TypeKind::builtin:
                    result = info.builtin == BuiltinType::str;
                    break;
                case TypeKind::reference:
                case TypeKind::slice:
                    result = true;
                    break;
                case TypeKind::generic_param:
                case TypeKind::associated_projection:
                    break;
                case TypeKind::array:
                    pending.push_back(info.array_element);
                    break;
                case TypeKind::tuple:
                    pending.insert(pending.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                    break;
                case TypeKind::struct_: {
                    const StructInfo* const structure = this->core_.find_struct(current);
                    if (structure != nullptr) {
                        for (const StructFieldInfo& field : structure->fields) {
                            pending.push_back(field.type);
                        }
                    }
                    break;
                }
                case TypeKind::enum_: {
                    if (const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(current);
                        cases != nullptr) {
                        for (const EnumCaseInfo* const enum_case : *cases) {
                            if (enum_case != nullptr) {
                                pending.insert(
                                    pending.end(), enum_case->payload_types.begin(), enum_case->payload_types.end());
                            }
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
        this->type_borrow_cache_[type.value] = result;
        return result;
    }

    void report_if_borrowed_local_escape(const syntax::ExprId expr)
    {
        if (!this->type_can_contain_borrow(this->core_.cached_expr_type(expr))) {
            return;
        }
        const BorrowOrigin origin = this->borrow_origin(expr);
        if (origin.present) {
            this->report_escape(expr, origin);
        }
    }

    void report_escape(const syntax::ExprId expr, const BorrowOrigin origin)
    {
        if (!origin.present || !syntax::is_valid(expr) || !this->reported_exprs_.insert(expr.value).second) {
            return;
        }
        this->core_.report_general(
            expr_range_or(this->core_.ctx_.module, expr, origin.range), std::string(SEMA_BORROWED_LOCAL_ESCAPE));
        this->core_.report_note(origin.range, SemanticDiagnosticKind::general, std::string(SEMA_BORROWED_LOCAL_ORIGIN));
    }

    SemanticAnalyzerCore& core_;
    std::vector<Scope> scopes_;
    mutable std::unordered_map<base::u32, bool> type_borrow_cache_;
    std::unordered_set<base::u32> reported_exprs_;
    bool include_return_escapes_ = true;
};

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

    FunctionBodyContextScope(SemanticAnalyzerCore& owner_analyzer, Config config)
        : analyzer(owner_analyzer), previous_module(owner_analyzer.state_.flow.current_module),
          previous_item(owner_analyzer.state_.flow.current_item),
          previous_function_return_type(owner_analyzer.state_.flow.current_function_return_type),
          previous_return_inference(owner_analyzer.state_.flow.current_return_inference),
          previous_loop_depth(owner_analyzer.state_.flow.loop_depth),
          previous_unsafe_context_depth(owner_analyzer.state_.flow.unsafe_context_depth),
          previous_symbols(std::move(owner_analyzer.state_.names.symbols))
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
    SemanticAnalyzerCore::BodyLoanChecker body_loan_checker(this->core_);
    const bool may_need_body_loan_check = body_loan_checker.may_need_local_loan_check(function);
    const bool may_need_place_state_check = this->core_.may_need_place_state_check(function, signature);
    const bool collect_body_flow =
        this->core_.ctx_.options.retain_body_flow_graphs || may_need_body_loan_check || may_need_place_state_check;
    if (collect_body_flow) {
        this->core_.collect_body_flow_graph(function, key);
        this->core_.check_body_loans(function, key, BodyLoanDiagnosticMode::enforced);
    } else {
        body_loan_checker.record_empty(key, BodyLoanDiagnosticMode::enforced);
    }
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
    const auto summary_signature = this->core_.state_.checked.functions.find(key);
    const FunctionSignature& finalized_signature =
        summary_signature == this->core_.state_.checked.functions.end() ? signature : summary_signature->second;
    this->core_.build_borrow_summary(function, key, finalized_signature);
    const auto summary = this->core_.state_.checked.borrow_summaries.find(key);
    if (summary != this->core_.state_.checked.borrow_summaries.end()
        && summary->second.return_type_can_contain_borrow
        && this->core_.state_.checked.body_flow_graphs.find(key)
            == this->core_.state_.checked.body_flow_graphs.end()) {
        this->core_.collect_body_flow_graph(function, key);
    }
    this->core_.check_borrow_contract(function, key, finalized_signature);
    this->core_.analyze_lifetimes(function, key, finalized_signature);
    this->core_.analyze_dropck(function, key, finalized_signature);
    this->core_.analyze_place_states(function, key, finalized_signature);
    const auto storage_escape_summary = this->core_.state_.checked.borrow_summaries.find(key);
    const bool needs_storage_escape_guard = storage_escape_summary == this->core_.state_.checked.borrow_summaries.end()
        || (storage_escape_summary->second.storage_escapes.empty()
            && body_may_need_storage_escape_guard(this->core_.ctx_.module, function.body));
    if (needs_storage_escape_guard) {
        this->core_.analyze_borrow_escapes(function, false);
    }
    if (!this->core_.ctx_.options.retain_body_flow_graphs) {
        this->core_.state_.checked.body_flow_graphs.erase(key);
    }
    state = FunctionBodyState::analyzed;
}

TypeHandle SemanticAnalyzerCore::StatementAnalyzer::analyze_lambda_expr(
    const syntax::ExprId expr_id, const ExprView& expr)
{
    std::vector<TypeHandle> param_types;
    param_types.reserve(expr.lambda_params.size());
    for (const syntax::ParamDecl& param : expr.lambda_params) {
        const TypeHandle param_type = this->core_.resolve_type(param.type);
        if (!this->core_.is_valid_storage_type(param_type)) {
            this->core_.report_general(param.range, std::string(SEMA_FUNCTION_PARAMETER_STORAGE));
        }
        static_cast<void>(this->core_.check_m2_value_abi(param_type, ValueAbiContext::function_type_parameter,
            param.range));
        param_types.push_back(param_type);
    }

    const TypeHandle return_type = this->core_.resolve_type(expr.lambda_return_type);
    if (is_valid(return_type) && !this->core_.state_.checked.types.is_void(return_type)
        && !this->core_.is_valid_storage_type(return_type)) {
        this->core_.report_general(expr.range, std::string(SEMA_FUNCTION_TYPE_RETURN_STORAGE));
    }
    static_cast<void>(
        this->core_.check_m2_value_abi(return_type, ValueAbiContext::function_type_return, expr.range));

    const TypeHandle function_type = this->core_.state_.checked.types.function(
        FunctionCallConv::aurex, false, false, param_types, return_type);
    const syntax::ModuleId module = this->core_.state_.flow.current_module;
    const syntax::ItemId owner_item = this->core_.state_.flow.current_item;
    const std::string symbol = lambda_symbol_name(module, owner_item, expr_id);
    const IdentId symbol_id = this->core_.intern_generated_key(symbol);
    std::vector<LambdaCaptureCandidate> captures =
        LambdaCaptureScanner(this->core_, expr.lambda_params, expr.lambda_captures).scan(expr.lambda_body);
    bool has_unsupported_capture = false;
    append_lambda_init_captures(this->core_, expr, captures, has_unsupported_capture);
    check_lambda_capture_list(this->core_, expr, captures, has_unsupported_capture);
    order_lambda_captures_by_capture_list(expr, captures);
    for (const LambdaCaptureCandidate& capture : captures) {
        if (lambda_capture_type_is_generic_dependent(this->core_, capture.type)) {
            report_generic_dependent_lambda_capture(this->core_, capture);
            has_unsupported_capture = true;
            continue;
        }
        if (lambda_capture_requires_copy(capture)
            && !this->core_.type_satisfies_capability(capture.type, CapabilityKind::copy)) {
            report_noncopy_lambda_capture(this->core_, capture);
            has_unsupported_capture = true;
        }
        if (lambda_capture_type_can_contain_borrow(this->core_, capture.type)) {
            report_borrowed_view_lambda_capture(this->core_, capture);
            has_unsupported_capture = true;
        }
    }

    const bool has_captures = !captures.empty();
    const std::string environment_symbol = lambda_environment_symbol_name(symbol);
    const IdentId environment_symbol_id = has_captures ? this->core_.intern_generated_key(environment_symbol)
                                                       : INVALID_IDENT_ID;
    TypeHandle environment_type = INVALID_TYPE_HANDLE;
    if (has_captures && !has_unsupported_capture) {
        environment_type = this->core_.state_.checked.types.named_struct(
            environment_symbol, environment_symbol,
            lambda_captures_contain_array(this->core_.state_.checked.types, captures));
    }
    const TypeHandle lambda_type = has_captures ? environment_type : function_type;

    CheckedLambdaInfo info = this->core_.state_.checked.make_lambda_info();
    info.expr = expr_id;
    info.name = this->core_.state_.checked.intern_text(symbol);
    info.name_id = symbol_id;
    info.c_name = this->core_.state_.checked.intern_text(symbol);
    info.c_name_id = this->core_.state_.checked.intern_c_name(symbol);
    info.module = module;
    info.owner_item = owner_item;
    info.type = lambda_type;
    info.function_type = function_type;
    info.environment_type = environment_type;
    info.environment_name = this->core_.state_.checked.intern_text(environment_symbol);
    info.environment_name_id = environment_symbol_id;
    info.environment_c_name = this->core_.state_.checked.intern_text(environment_symbol);
    info.environment_c_name_id =
        has_captures ? this->core_.state_.checked.intern_c_name(environment_symbol) : INVALID_IDENT_ID;
    info.return_type = return_type;
    info.param_types = this->core_.state_.checked.copy_type_handle_list(param_types);
    info.body = expr.lambda_body;
    info.range = expr.range;
    info.has_unsupported_capture = has_unsupported_capture;
    info.captures.reserve(captures.size());
    for (base::usize index = 0; index < captures.size(); ++index) {
        const LambdaCaptureCandidate& capture = captures[index];
        const std::string field_name = lambda_capture_field_name(index, capture.name);
        const IdentId field_name_id = this->core_.intern_generated_key(field_name);
        info.captures.push_back(CheckedLambdaInfo::Capture{
            this->core_.source_name_text(capture.name_id, capture.name),
            capture.name_id,
            this->core_.state_.checked.intern_text(field_name),
            field_name_id,
            capture.type,
            lambda_capture_environment_field_type(this->core_.state_.checked.types, capture),
            capture.kind,
            capture.initializer,
            capture.use_range,
            capture.declaration_range,
        });
    }
    if (has_captures && !has_unsupported_capture && is_valid(environment_type)) {
        StructInfo environment = this->core_.state_.checked.make_struct_info();
        environment.name = info.environment_name;
        environment.name_id = environment_symbol_id;
        environment.c_name = info.environment_c_name;
        environment.module = module;
        environment.type = environment_type;
        environment.visibility = syntax::Visibility::private_;
        environment.part_index = this->core_.item_part_index(owner_item);
        environment.stable_id = this->core_.stable_definition_id(
            module, StableSymbolKind::synthetic, environment_symbol_id, environment_symbol);
        environment.incremental_key = this->core_.stable_incremental_key(environment.stable_id, environment_symbol);
        environment.fields.reserve(info.captures.size());
        for (base::usize index = 0; index < info.captures.size(); ++index) {
            const CheckedLambdaInfo::Capture& capture = info.captures[index];
            environment.fields.push_back(StructFieldInfo{
                capture.field_name,
                capture.field_name_id,
                capture.field_name,
                module,
                lambda_checked_capture_environment_field_type(this->core_.state_.checked.types, capture),
                capture.use_range,
                syntax::Visibility::private_,
                this->core_.stable_member_key(environment.stable_id, StableSymbolKind::struct_field,
                    capture.field_name_id, capture.field_name.view(), static_cast<base::u32>(index)),
            });
        }
        const ModuleLookupKey environment_key{module.value, environment_symbol_id};
        const auto inserted = this->core_.state_.checked.structs.emplace(environment_key, std::move(environment));
        this->core_.state_.types.struct_infos_by_type[environment_type.value] = &inserted.first->second;
    }
    this->core_.state_.checked.lambdas.push_back(std::move(info));
    const CheckedLambdaInfo& checked_lambda = this->core_.state_.checked.lambdas.back();

    if (has_unsupported_capture) {
        this->core_.record_expr_c_name(expr_id, symbol);
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    FunctionBodyContextScope context(this->core_,
        FunctionBodyContextScope::Config{
            .module = module,
            .item = owner_item,
            .return_type = return_type,
            .return_inference = nullptr,
            .loop_depth = SEMA_NO_LOOP_DEPTH,
            .unsafe_context_depth = this->core_.state_.flow.unsafe_context_depth,
            .symbols = SymbolTable{},
        });
    this->core_.state_.names.symbols.push_scope(expr.lambda_params.size() + checked_lambda.captures.size());
    for (const CheckedLambdaInfo::Capture& capture : checked_lambda.captures) {
        static_cast<void>(this->core_.state_.names.symbols.insert(
            Symbol{
                SymbolKind::parameter,
                capture.name,
                capture.name_id,
                {},
                syntax::INVALID_MODULE_ID,
                capture.type,
                capture.declaration_range,
                lambda_capture_kind_is_mutable(capture.kind),
                syntax::Visibility::private_,
                {},
            },
            this->core_.ctx_.diagnostics));
    }
    for (base::usize index = 0; index < expr.lambda_params.size(); ++index) {
        const syntax::ParamDecl& param = expr.lambda_params[index];
        const TypeHandle param_type = index < param_types.size() ? param_types[index] : INVALID_TYPE_HANDLE;
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
    this->core_.analyze_block(expr.lambda_body, return_type, nullptr);
    this->core_.state_.names.symbols.pop_scope();

    if (is_valid(return_type) && !this->core_.state_.checked.types.is_void(return_type)
        && !this->core_.block_guarantees_return(expr.lambda_body)) {
        this->core_.report_general(expr.range, std::string(SEMA_NOT_ALL_PATHS_RETURN));
    }

    this->core_.record_expr_c_name(expr_id, symbol);
    return this->core_.record_expr_type(expr_id, lambda_type);
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

bool SemanticAnalyzerCore::StatementAnalyzer::resource_assignment_requires_unsupported_diagnostic(
    const syntax::ExprId lhs, const TypeHandle lhs_type) const
{
    if (!syntax::is_valid(lhs) || !is_valid(lhs_type)
        || this->core_.type_satisfies_capability(lhs_type, CapabilityKind::copy)) {
        return false;
    }
    if (expr_is_indexed_or_dereferenced_place(this->core_.ctx_.module, lhs)) {
        return true;
    }
    return expr_is_field_place(this->core_.ctx_.module, lhs) && !this->is_owned_local_field_assignment(lhs);
}

bool SemanticAnalyzerCore::StatementAnalyzer::is_owned_local_field_assignment(const syntax::ExprId lhs) const
{
    syntax::ExprId current = lhs;
    bool saw_field = false;
    while (syntax::is_valid(current) && current.value < this->core_.ctx_.module.exprs.size()) {
        const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
        if (kind == syntax::ExprKind::field) {
            const syntax::FieldExprPayload* const field = this->core_.ctx_.module.exprs.field_payload(current.value);
            if (field == nullptr) {
                return false;
            }
            const TypeHandle object_type = this->core_.cached_expr_type(field->object);
            if (!is_valid(object_type) || this->core_.state_.checked.types.is_pointer(object_type)
                || this->core_.state_.checked.types.is_reference(object_type)) {
                return false;
            }
            saw_field = true;
            current = field->object;
            continue;
        }
        if (kind == syntax::ExprKind::name) {
            const syntax::NameExprPayload* const name = this->core_.ctx_.module.exprs.name_payload(current.value);
            return saw_field && name != nullptr && name->scope_name.empty();
        }
        return false;
    }
    return false;
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
    if (syntax::is_valid(stmt.range_iterable)) {
        const TypeHandle iterable_type = this->core_.analyze_expr(stmt.range_iterable);
        TypeHandle element_type = INVALID_TYPE_HANDLE;
        if (is_valid(iterable_type) && this->core_.state_.checked.types.is_array(iterable_type)) {
            element_type = this->core_.state_.checked.types.get(iterable_type).array_element;
        } else if (is_valid(iterable_type) && this->core_.state_.checked.types.is_slice(iterable_type)) {
            element_type = this->core_.state_.checked.types.get(iterable_type).slice_element;
        } else {
            this->core_.report_general(
                expr_range_or(this->core_.ctx_.module, stmt.range_iterable, stmt.range),
                std::string(SEMA_FOR_IN_ARRAY_OR_SLICE));
        }
        if (is_valid(element_type) && !this->core_.type_satisfies_capability(element_type, CapabilityKind::copy)) {
            this->core_.report_general(
                expr_range_or(this->core_.ctx_.module, stmt.range_iterable, stmt.range),
                std::string(SEMA_FOR_IN_ELEMENT_COPY));
        }
        this->core_.record_stmt_local_type(stmt_id, element_type);
        return element_type;
    }

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
            if (stmt.assign_op == syntax::AssignOp::assign
                && this->resource_assignment_requires_unsupported_diagnostic(stmt.lhs, lhs)) {
                this->core_.report_unsupported(expr_range_or(this->core_.ctx_.module, stmt.lhs, stmt.range),
                    std::string(SEMA_RESOURCE_PLACE_ASSIGNMENT_UNSUPPORTED));
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
            if (expr_range_contains_try_expr(this->core_.ctx_.module, stmt.init)) {
                this->core_.report_general(stmt.range, std::string(SEMA_DEFER_EARLY_EXIT));
                break;
            }
            break;
    }
}

bool SemanticAnalyzerCore::StatementAnalyzer::block_guarantees_return(const syntax::StmtId block_id) const
{
    return evaluate_control_flow(this->core_.ctx_.module, block_id, ControlFlowFrameKind::block,
        ControlFlowCacheKind::block, ControlFlowQuery::guarantees_return,
        ControlFlowQueryCaches{
            this->core_.state_.control_flow_queries.stmt_guarantees_return,
            this->core_.state_.control_flow_queries.block_guarantees_return,
        });
}

bool SemanticAnalyzerCore::StatementAnalyzer::stmt_guarantees_return(const syntax::StmtId stmt_id) const
{
    return evaluate_control_flow(this->core_.ctx_.module, stmt_id, ControlFlowFrameKind::statement,
        ControlFlowCacheKind::statement, ControlFlowQuery::guarantees_return,
        ControlFlowQueryCaches{
            this->core_.state_.control_flow_queries.stmt_guarantees_return,
            this->core_.state_.control_flow_queries.block_guarantees_return,
        });
}

bool SemanticAnalyzerCore::StatementAnalyzer::block_may_fallthrough(const syntax::StmtId block_id) const
{
    return evaluate_control_flow(this->core_.ctx_.module, block_id, ControlFlowFrameKind::block,
        ControlFlowCacheKind::block, ControlFlowQuery::may_fallthrough,
        ControlFlowQueryCaches{
            this->core_.state_.control_flow_queries.stmt_may_fallthrough,
            this->core_.state_.control_flow_queries.block_may_fallthrough,
        });
}

bool SemanticAnalyzerCore::StatementAnalyzer::stmt_may_fallthrough(const syntax::StmtId stmt_id) const
{
    return evaluate_control_flow(this->core_.ctx_.module, stmt_id, ControlFlowFrameKind::statement,
        ControlFlowCacheKind::statement, ControlFlowQuery::may_fallthrough,
        ControlFlowQueryCaches{
            this->core_.state_.control_flow_queries.stmt_may_fallthrough,
            this->core_.state_.control_flow_queries.block_may_fallthrough,
        });
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

void SemanticAnalyzerCore::analyze_borrow_escapes(
    const syntax::ItemNode& function, const bool include_return_escapes)
{
    BorrowEscapeAnalyzer(*this, include_return_escapes).analyze_function(function);
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

TypeHandle SemanticAnalyzerCore::analyze_lambda_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return StatementAnalyzer(*this).analyze_lambda_expr(expr_id, expr);
}

} // namespace aurex::sema
