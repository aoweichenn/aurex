#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <frontend/sema/internal/borrow/private/loan_checker.hpp>
#include <frontend/sema/internal/diagnostics/private/sema_diagnostics.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_BODY_LOAN_ID_CONTEXT = "sema body loan id";
constexpr std::string_view SEMA_BODY_LOAN_FINGERPRINT_MARKER = "sema.body_loan_check.v1";
constexpr base::usize SEMA_BODY_LOAN_INITIAL_WORKLIST_CAPACITY = 32;
constexpr base::usize SEMA_BODY_LOAN_PRECHECK_INITIAL_STACK_CAPACITY = 64;
constexpr base::u8 SEMA_BODY_LOAN_TYPE_REF_CACHE_UNKNOWN = 0;
constexpr base::u8 SEMA_BODY_LOAN_TYPE_REF_CACHE_FALSE = 1;
constexpr base::u8 SEMA_BODY_LOAN_TYPE_REF_CACHE_TRUE = 2;

enum class BodyLoanAccessKind : base::u8 {
    read,
    write,
    reinit,
    move,
    drop,
    shared_borrow,
    mutable_borrow,
    cleanup,
};

struct BodyLoanActionIndex {
    std::vector<std::vector<base::u32>> actions_by_point;
    std::vector<std::vector<base::u32>> incoming_points;
    std::vector<std::vector<base::u32>> outgoing_points;
};

struct BodyLoanDiagnosticSite {
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::SourceRange range{};
};

struct BodyLoanCarrierUseSite {
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::SourceRange range{};
};

struct BodyLoanActionSite {
    base::u32 action = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::SourceRange range{};
};

struct BodyLoanLatestCarrierLoan {
    base::u32 loan = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 issued_action = SEMA_BODY_LOAN_INVALID_INDEX;
};

struct BodyLoanCarrierBinding {
    IdentId carrier_name_id = INVALID_IDENT_ID;
    syntax::StmtId enclosing_stmt = syntax::INVALID_STMT_ID;
    base::u32 carrier_definition_point = SEMA_BODY_FLOW_INVALID_INDEX;
};

struct BodyLoanCarrierDefinitionKey {
    base::u32 name_id = INVALID_IDENT_ID.value;
    base::u32 stmt = syntax::INVALID_STMT_ID.value;

    [[nodiscard]] friend bool operator==(
        const BodyLoanCarrierDefinitionKey& lhs, const BodyLoanCarrierDefinitionKey& rhs) noexcept = default;
};

struct BodyLoanCarrierDefinitionKeyHash {
    [[nodiscard]] std::size_t operator()(const BodyLoanCarrierDefinitionKey& key) const noexcept
    {
        query::StableHashBuilder builder;
        builder.mix_u32(key.name_id);
        builder.mix_u32(key.stmt);
        return query::stable_hash_value(builder.finish());
    }
};

struct BodyLoanCarrierLivenessActions {
    std::vector<base::u32> use_points;
    std::vector<base::u32> definition_points;
};

struct BodyLoanTwoPhaseActivationKey {
    base::u32 expr = syntax::INVALID_EXPR_ID.value;
    base::u32 place = SEMA_BODY_FLOW_INVALID_INDEX;

    [[nodiscard]] friend bool operator==(
        const BodyLoanTwoPhaseActivationKey& lhs, const BodyLoanTwoPhaseActivationKey& rhs) noexcept = default;
};

struct BodyLoanTwoPhaseActivationKeyHash {
    [[nodiscard]] std::size_t operator()(const BodyLoanTwoPhaseActivationKey& key) const noexcept
    {
        query::StableHashBuilder builder;
        builder.mix_u32(key.expr);
        builder.mix_u32(key.place);
        return query::stable_hash_value(builder.finish());
    }
};

struct BodyLoanTwoPhaseActivationQueue {
    std::vector<BodyLoanActionSite> activations;
    base::usize next = 0;
};

[[nodiscard]] bool valid_point(const BodyFlowGraph& graph, const base::u32 point) noexcept
{
    return point != SEMA_BODY_FLOW_INVALID_INDEX && point < graph.points.size();
}

[[nodiscard]] bool valid_place(const BodyFlowGraph& graph, const base::u32 place) noexcept
{
    return place != SEMA_BODY_FLOW_INVALID_INDEX && place < graph.places.size();
}

[[nodiscard]] bool valid_action(const BodyFlowGraph& graph, const base::u32 action) noexcept
{
    return action != SEMA_BODY_LOAN_INVALID_INDEX && action < graph.actions.size();
}

[[nodiscard]] bool valid_expr(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size();
}

[[nodiscard]] bool valid_stmt(const syntax::AstModule& module, const syntax::StmtId stmt) noexcept
{
    return syntax::is_valid(stmt) && stmt.value < module.stmts.size();
}

void push_precheck_expr(
    std::vector<syntax::ExprId>& pending, const syntax::AstModule& module, const syntax::ExprId expr)
{
    if (valid_expr(module, expr)) {
        pending.push_back(expr);
    }
}

void push_precheck_stmt(
    std::vector<syntax::StmtId>& pending, const syntax::AstModule& module, const syntax::StmtId stmt)
{
    if (valid_stmt(module, stmt)) {
        pending.push_back(stmt);
    }
}

void push_precheck_block_statements(
    std::vector<syntax::StmtId>& pending, const syntax::AstModule& module, const syntax::StmtId block)
{
    if (!valid_stmt(module, block)) {
        return;
    }
    const syntax::AstArenaVector<syntax::StmtId>* const statements = module.stmts.block_statements(block.value);
    if (statements == nullptr) {
        return;
    }
    pending.insert(pending.end(), statements->begin(), statements->end());
}

void push_precheck_statement_children(const syntax::AstModule& module, const syntax::StmtNode& stmt,
    std::vector<syntax::ExprId>& pending_exprs, std::vector<syntax::StmtId>& pending_stmts)
{
    switch (stmt.kind) {
        case syntax::StmtKind::let:
        case syntax::StmtKind::var:
            push_precheck_expr(pending_exprs, module, stmt.init);
            push_precheck_stmt(pending_stmts, module, stmt.else_block);
            break;
        case syntax::StmtKind::assign:
            push_precheck_expr(pending_exprs, module, stmt.lhs);
            push_precheck_expr(pending_exprs, module, stmt.rhs);
            break;
        case syntax::StmtKind::if_:
            push_precheck_expr(pending_exprs, module, stmt.condition);
            push_precheck_stmt(pending_stmts, module, stmt.then_block);
            push_precheck_stmt(pending_stmts, module, stmt.else_block);
            push_precheck_stmt(pending_stmts, module, stmt.else_if);
            break;
        case syntax::StmtKind::for_:
            push_precheck_stmt(pending_stmts, module, stmt.for_init);
            push_precheck_expr(pending_exprs, module, stmt.condition);
            push_precheck_stmt(pending_stmts, module, stmt.body);
            push_precheck_stmt(pending_stmts, module, stmt.for_update);
            break;
        case syntax::StmtKind::for_range:
            push_precheck_expr(pending_exprs, module, stmt.range_start);
            push_precheck_expr(pending_exprs, module, stmt.range_end);
            push_precheck_expr(pending_exprs, module, stmt.range_step);
            push_precheck_stmt(pending_stmts, module, stmt.body);
            break;
        case syntax::StmtKind::while_:
            push_precheck_expr(pending_exprs, module, stmt.condition);
            push_precheck_stmt(pending_stmts, module, stmt.body);
            break;
        case syntax::StmtKind::defer:
        case syntax::StmtKind::expr:
            push_precheck_expr(pending_exprs, module, stmt.init);
            break;
        case syntax::StmtKind::return_:
            push_precheck_expr(pending_exprs, module, stmt.return_value);
            break;
        case syntax::StmtKind::block:
            pending_stmts.insert(pending_stmts.end(), stmt.statements.begin(), stmt.statements.end());
            break;
        case syntax::StmtKind::break_:
        case syntax::StmtKind::continue_:
            break;
    }
}

void push_precheck_expression_children(const syntax::AstModule& module, const syntax::ExprId expr,
    std::vector<syntax::ExprId>& pending_exprs, std::vector<syntax::StmtId>& pending_stmts)
{
    if (!valid_expr(module, expr)) {
        return;
    }
    switch (module.exprs.kind(expr.value)) {
        case syntax::ExprKind::generic_apply: {
            const syntax::GenericApplyExprPayload* const apply = module.exprs.generic_apply_payload(expr.value);
            if (apply != nullptr) {
                push_precheck_expr(pending_exprs, module, apply->callee);
            }
            break;
        }
        case syntax::ExprKind::unary:
        case syntax::ExprKind::try_expr:
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
            const syntax::CastExprPayload* const cast = module.exprs.cast_payload(expr.value);
            const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(expr.value);
            const syntax::TryExprPayload* const try_expr = module.exprs.try_payload(expr.value);
            if (cast != nullptr) {
                push_precheck_expr(pending_exprs, module, cast->expr);
            } else if (unary != nullptr) {
                push_precheck_expr(pending_exprs, module, unary->operand);
            } else if (try_expr != nullptr) {
                push_precheck_expr(pending_exprs, module, try_expr->operand);
            }
            break;
        }
        case syntax::ExprKind::binary: {
            const syntax::BinaryExprPayload* const binary = module.exprs.binary_payload(expr.value);
            if (binary != nullptr) {
                push_precheck_expr(pending_exprs, module, binary->lhs);
                push_precheck_expr(pending_exprs, module, binary->rhs);
            }
            break;
        }
        case syntax::ExprKind::call:
        case syntax::ExprKind::str_from_bytes_unchecked: {
            const syntax::CallExprPayload* const call = module.exprs.call_payload(expr.value);
            if (call != nullptr) {
                push_precheck_expr(pending_exprs, module, call->callee);
                pending_exprs.insert(pending_exprs.end(), call->args.begin(), call->args.end());
            }
            break;
        }
        case syntax::ExprKind::field: {
            const syntax::FieldExprPayload* const field = module.exprs.field_payload(expr.value);
            if (field != nullptr) {
                push_precheck_expr(pending_exprs, module, field->object);
            }
            break;
        }
        case syntax::ExprKind::index: {
            const syntax::IndexExprPayload* const index = module.exprs.index_payload(expr.value);
            if (index != nullptr) {
                push_precheck_expr(pending_exprs, module, index->object);
                push_precheck_expr(pending_exprs, module, index->index);
            }
            break;
        }
        case syntax::ExprKind::slice: {
            const syntax::SliceExprPayload* const slice = module.exprs.slice_payload(expr.value);
            if (slice != nullptr) {
                push_precheck_expr(pending_exprs, module, slice->object);
                push_precheck_expr(pending_exprs, module, slice->start);
                push_precheck_expr(pending_exprs, module, slice->end);
            }
            break;
        }
        case syntax::ExprKind::if_expr: {
            const syntax::IfExprPayload* const if_expr = module.exprs.if_payload(expr.value);
            if (if_expr != nullptr) {
                push_precheck_expr(pending_exprs, module, if_expr->condition);
                push_precheck_expr(pending_exprs, module, if_expr->then_expr);
                push_precheck_expr(pending_exprs, module, if_expr->else_expr);
            }
            break;
        }
        case syntax::ExprKind::array_literal: {
            const syntax::ArrayExprPayload* const array = module.exprs.array_payload(expr.value);
            if (array != nullptr) {
                pending_exprs.insert(pending_exprs.end(), array->elements.begin(), array->elements.end());
                push_precheck_expr(pending_exprs, module, array->repeat_value);
                push_precheck_expr(pending_exprs, module, array->repeat_count);
            }
            break;
        }
        case syntax::ExprKind::tuple_literal: {
            const syntax::AstArenaVector<syntax::ExprId>* const elements = module.exprs.tuple_elements(expr.value);
            if (elements != nullptr) {
                pending_exprs.insert(pending_exprs.end(), elements->begin(), elements->end());
            }
            break;
        }
        case syntax::ExprKind::struct_literal: {
            const syntax::StructLiteralExprPayload* const literal = module.exprs.struct_literal_payload(expr.value);
            if (literal != nullptr) {
                push_precheck_expr(pending_exprs, module, literal->object);
                for (const syntax::FieldInit& init : literal->field_inits) {
                    push_precheck_expr(pending_exprs, module, init.value);
                }
            }
            break;
        }
        case syntax::ExprKind::match_expr: {
            const syntax::MatchExprPayload* const match = module.exprs.match_payload(expr.value);
            if (match != nullptr) {
                push_precheck_expr(pending_exprs, module, match->value);
                for (const syntax::MatchArm& arm : match->arms) {
                    push_precheck_expr(pending_exprs, module, arm.guard);
                    push_precheck_expr(pending_exprs, module, arm.value);
                }
            }
            break;
        }
        case syntax::ExprKind::block_expr:
        case syntax::ExprKind::unsafe_block: {
            const syntax::BlockExprPayload* const block = module.exprs.block_payload(expr.value);
            if (block != nullptr) {
                push_precheck_block_statements(pending_stmts, module, block->block);
                push_precheck_expr(pending_exprs, module, block->result);
            }
            break;
        }
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
            break;
    }
}

[[nodiscard]] bool action_is_borrow(const BodyFlowActionKind kind) noexcept
{
    return kind == BodyFlowActionKind::borrow_shared || kind == BodyFlowActionKind::borrow_mutable;
}

[[nodiscard]] bool action_is_storage_definition(const BodyFlowActionKind kind) noexcept
{
    return kind == BodyFlowActionKind::write || kind == BodyFlowActionKind::reinit;
}

[[nodiscard]] std::optional<BodyLoanAccessKind> access_kind_for_action(const BodyFlowActionKind kind) noexcept
{
    switch (kind) {
        case BodyFlowActionKind::read:
            return BodyLoanAccessKind::read;
        case BodyFlowActionKind::write:
            return BodyLoanAccessKind::write;
        case BodyFlowActionKind::reinit:
            return BodyLoanAccessKind::reinit;
        case BodyFlowActionKind::move_candidate:
            return BodyLoanAccessKind::move;
        case BodyFlowActionKind::drop:
            return BodyLoanAccessKind::drop;
        case BodyFlowActionKind::borrow_shared:
            return BodyLoanAccessKind::shared_borrow;
        case BodyFlowActionKind::borrow_mutable:
            return BodyLoanAccessKind::mutable_borrow;
        case BodyFlowActionKind::cleanup_storage:
            return BodyLoanAccessKind::cleanup;
        case BodyFlowActionKind::call_receiver_reserve:
        case BodyFlowActionKind::call_receiver_activate:
        case BodyFlowActionKind::call:
        case BodyFlowActionKind::return_:
        case BodyFlowActionKind::branch:
        case BodyFlowActionKind::cleanup_scope:
            return std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] BodyLoanConflictKind conflict_kind_for_access(const BodyLoanAccessKind access) noexcept
{
    switch (access) {
        case BodyLoanAccessKind::read:
            return BodyLoanConflictKind::read;
        case BodyLoanAccessKind::write:
            return BodyLoanConflictKind::write;
        case BodyLoanAccessKind::reinit:
            return BodyLoanConflictKind::reinit;
        case BodyLoanAccessKind::move:
            return BodyLoanConflictKind::move;
        case BodyLoanAccessKind::drop:
            return BodyLoanConflictKind::drop;
        case BodyLoanAccessKind::shared_borrow:
            return BodyLoanConflictKind::shared_borrow;
        case BodyLoanAccessKind::mutable_borrow:
            return BodyLoanConflictKind::mutable_borrow;
        case BodyLoanAccessKind::cleanup:
            return BodyLoanConflictKind::cleanup;
    }
    return BodyLoanConflictKind::write;
}

[[nodiscard]] bool loan_conflicts_with_access(const BodyLoanKind loan_kind, const BodyLoanAccessKind access) noexcept
{
    if (loan_kind == BodyLoanKind::mutable_) {
        return true;
    }
    switch (access) {
        case BodyLoanAccessKind::read:
        case BodyLoanAccessKind::shared_borrow:
            return false;
        case BodyLoanAccessKind::write:
        case BodyLoanAccessKind::reinit:
        case BodyLoanAccessKind::move:
        case BodyLoanAccessKind::drop:
        case BodyLoanAccessKind::mutable_borrow:
        case BodyLoanAccessKind::cleanup:
            return true;
    }
    return true;
}

[[nodiscard]] bool projections_conflict(const BodyFlowPlaceProjection& lhs, const BodyFlowPlaceProjection& rhs) noexcept
{
    if (lhs.kind != rhs.kind) {
        return true;
    }
    if (lhs.kind == BodyFlowPlaceProjectionKind::field) {
        return !syntax::is_valid(lhs.field_name_id) || !syntax::is_valid(rhs.field_name_id)
            || lhs.field_name_id == rhs.field_name_id;
    }
    return true;
}

[[nodiscard]] bool places_conflict(const BodyFlowPlace& lhs, const BodyFlowPlace& rhs) noexcept
{
    if (lhs.root_kind != rhs.root_kind) {
        return lhs.root_kind == BodyFlowPlaceRootKind::unknown || rhs.root_kind == BodyFlowPlaceRootKind::unknown;
    }
    switch (lhs.root_kind) {
        case BodyFlowPlaceRootKind::local:
            if (lhs.root_name_id != rhs.root_name_id) {
                return false;
            }
            break;
        case BodyFlowPlaceRootKind::temporary:
            if (lhs.root_expr.value != rhs.root_expr.value) {
                return false;
            }
            break;
        case BodyFlowPlaceRootKind::none:
            return false;
        case BodyFlowPlaceRootKind::unknown:
            return true;
    }

    const base::usize shared_count = std::min(lhs.projections.size(), rhs.projections.size());
    for (base::usize index = 0; index < shared_count; ++index) {
        if (!projections_conflict(lhs.projections[index], rhs.projections[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool reborrow_parent_access_conflicts(
    const BodyLoanKind parent_kind, const BodyLoanKind child_kind, const BodyLoanAccessKind access) noexcept
{
    static_cast<void>(parent_kind);
    if (child_kind == BodyLoanKind::mutable_) {
        return true;
    }
    switch (access) {
        case BodyLoanAccessKind::read:
        case BodyLoanAccessKind::shared_borrow:
            return false;
        case BodyLoanAccessKind::write:
        case BodyLoanAccessKind::reinit:
        case BodyLoanAccessKind::move:
        case BodyLoanAccessKind::drop:
        case BodyLoanAccessKind::mutable_borrow:
        case BodyLoanAccessKind::cleanup:
            return true;
    }
    return true;
}

[[nodiscard]] bool same_source_range(const base::SourceRange& lhs, const base::SourceRange& rhs) noexcept
{
    return lhs.source.value == rhs.source.value && lhs.begin == rhs.begin && lhs.end == rhs.end;
}

[[nodiscard]] bool same_diagnostic_site(const BodyLoanDiagnosticSite& site, const BodyLoanConflict& conflict) noexcept
{
    return same_source_range(site.range, conflict.range);
}

[[nodiscard]] bool record_reported_diagnostic_site(
    std::vector<BodyLoanDiagnosticSite>& sites, const BodyLoanConflict& conflict)
{
    if (std::ranges::any_of(sites, [&conflict](const BodyLoanDiagnosticSite& site) {
            return same_diagnostic_site(site, conflict);
        })) {
        return false;
    }
    sites.push_back(BodyLoanDiagnosticSite{
        .point = conflict.point,
        .range = conflict.range,
    });
    return true;
}

void append_optional_name_id(std::ostringstream& stream, const IdentId name)
{
    if (syntax::is_valid(name)) {
        stream << '#' << name.value;
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

void append_optional_stmt_id(std::ostringstream& stream, const syntax::StmtId stmt)
{
    if (syntax::is_valid(stmt)) {
        stream << 's' << stmt.value;
        return;
    }
    stream << '-';
}

void append_range(std::ostringstream& stream, const base::SourceRange& range)
{
    stream << range.source.value << ':' << range.begin << ".." << range.end;
}

void mix_function_key(query::StableHashBuilder& builder, const FunctionLookupKey key) noexcept
{
    builder.mix_u32(key.module);
    builder.mix_u32(key.owner_type);
    builder.mix_u32(key.name.value);
}

void mix_origin(query::StableHashBuilder& builder, const BodyLoanOrigin& origin) noexcept
{
    builder.mix_u8(static_cast<base::u8>(origin.kind));
    builder.mix_u32(origin.name_id.value);
    builder.mix_u32(origin.expr.value);
}

void mix_two_phase_borrow(query::StableHashBuilder& builder, const BodyTwoPhaseBorrow& borrow) noexcept
{
    builder.mix_u32(borrow.reservation_action);
    builder.mix_u32(borrow.activation_action);
    builder.mix_u32(borrow.reservation_point);
    builder.mix_u32(borrow.activation_point);
    builder.mix_u32(borrow.place);
    builder.mix_u32(borrow.call_expr.value);
    builder.mix_bool(borrow.diagnostic_emitted);
}

[[nodiscard]] std::string body_loan_invalidating_action_message(const BodyLoanConflictKind kind)
{
    std::string message(SEMA_ACTIVE_BORROW_INVALIDATING_ACTION);
    message.append(" (");
    message.append(body_loan_conflict_kind_name(kind));
    message.push_back(')');
    return message;
}

[[nodiscard]] bool conflict_is_two_phase(const BodyLoanConflictKind kind) noexcept
{
    return kind == BodyLoanConflictKind::two_phase_reservation || kind == BodyLoanConflictKind::two_phase_activation;
}

class BodyLoanSolver final {
public:
    BodyLoanSolver(const syntax::AstModule& module, const syntax::ItemNode& function, const FunctionLookupKey key,
        const CheckedModule& checked, const BodyFlowGraph& graph, std::vector<bool> move_action_consumes,
        const BodyLoanDiagnosticMode mode)
        : module_(module), function_(function), checked_(checked), graph_(graph),
          move_action_consumes_(std::move(move_action_consumes))
    {
        this->result_.function = key;
        this->result_.diagnostic_mode = mode;
    }

    [[nodiscard]] BodyLoanCheckResult run()
    {
        this->build_graph_index();
        this->build_carrier_indexes();
        this->collect_loans();
        this->bind_reborrow_parents();
        this->collect_two_phase_borrows();
        this->compute_carrier_liveness();
        this->check_two_phase_reservations();
        this->propagate_active_loans();
        return std::move(this->result_);
    }

private:
    void build_graph_index()
    {
        this->index_.actions_by_point.assign(this->graph_.points.size(), {});
        this->read_action_observes_reference_.assign(this->graph_.actions.size(), false);
        for (base::usize action_index = 0; action_index < this->graph_.actions.size(); ++action_index) {
            const BodyFlowAction& action = this->graph_.actions[action_index];
            this->read_action_observes_reference_[action_index] = this->read_action_observes_reference(action);
            if (!valid_point(this->graph_, action.point)) {
                continue;
            }
            this->index_.actions_by_point[action.point].push_back(
                base::checked_u32(action_index, SEMA_BODY_LOAN_ID_CONTEXT));
        }

        this->index_.incoming_points.assign(this->graph_.points.size(), {});
        this->index_.outgoing_points.assign(this->graph_.points.size(), {});
        for (const BodyFlowEdge& edge : this->graph_.edges) {
            if (!valid_point(this->graph_, edge.from) || !valid_point(this->graph_, edge.to)) {
                continue;
            }
            this->index_.outgoing_points[edge.from].push_back(edge.to);
            this->index_.incoming_points[edge.to].push_back(edge.from);
        }
    }

    void build_carrier_indexes()
    {
        this->carrier_definition_points_.clear();
        this->direct_carrier_by_borrow_operand_.clear();
        this->indexed_carrier_by_loan_expr_.clear();
        this->build_carrier_definition_index();
        this->build_direct_carrier_binding_index();
        this->build_result_carrier_binding_index();
    }

    void build_carrier_definition_index()
    {
        this->carrier_definition_points_.reserve(this->graph_.actions.size());
        for (const BodyFlowAction& action : this->graph_.actions) {
            if (!action_is_storage_definition(action.kind) || !valid_place(this->graph_, action.place)
                || !syntax::is_valid(action.stmt)) {
                continue;
            }
            const BodyFlowPlace& place = this->graph_.places[action.place];
            if (place.root_kind != BodyFlowPlaceRootKind::local || !syntax::is_valid(place.root_name_id)
                || !place.projections.empty()) {
                continue;
            }
            this->carrier_definition_points_.try_emplace(
                BodyLoanCarrierDefinitionKey{
                    .name_id = place.root_name_id.value,
                    .stmt = action.stmt.value,
                },
                action.point);
        }
    }

    void build_direct_carrier_binding_index()
    {
        std::vector<syntax::StmtId> pending{this->function_.body};
        while (!pending.empty()) {
            const syntax::StmtId stmt = pending.back();
            pending.pop_back();
            if (!valid_stmt(this->module_, stmt)) {
                continue;
            }
            const syntax::StmtNode& node = this->module_.stmts[stmt.value];
            this->index_direct_carrier_binding_from_statement(stmt, node);
            this->push_child_statements(node, pending);
        }
    }

    void index_direct_carrier_binding_from_statement(const syntax::StmtId stmt, const syntax::StmtNode& node)
    {
        if ((node.kind == syntax::StmtKind::let || node.kind == syntax::StmtKind::var)
            && syntax::is_valid(node.name_id)) {
            this->index_direct_carrier_binding(this->direct_address_operand(node.init), node.name_id, stmt);
            return;
        }
        if (node.kind != syntax::StmtKind::assign) {
            return;
        }
        const IdentId lhs = this->direct_name_id(node.lhs);
        if (syntax::is_valid(lhs)) {
            this->index_direct_carrier_binding(this->direct_address_operand(node.rhs), lhs, stmt);
        }
    }

    void index_direct_carrier_binding(
        const syntax::ExprId borrowed_operand, const IdentId carrier_name_id, const syntax::StmtId stmt)
    {
        if (!valid_expr(this->module_, borrowed_operand) || !syntax::is_valid(carrier_name_id)) {
            return;
        }
        this->direct_carrier_by_borrow_operand_.try_emplace(
            borrowed_operand.value,
            BodyLoanCarrierBinding{
                .carrier_name_id = carrier_name_id,
                .enclosing_stmt = stmt,
                .carrier_definition_point = this->find_carrier_definition_point(carrier_name_id, stmt),
            });
    }

    void build_result_carrier_binding_index()
    {
        std::vector<syntax::StmtId> pending{this->function_.body};
        while (!pending.empty()) {
            const syntax::StmtId stmt = pending.back();
            pending.pop_back();
            if (!valid_stmt(this->module_, stmt)) {
                continue;
            }
            const syntax::StmtNode& node = this->module_.stmts[stmt.value];
            this->index_result_carrier_binding_from_statement(stmt, node);
            this->push_child_statements(node, pending);
        }
    }

    void index_result_carrier_binding_from_statement(const syntax::StmtId stmt, const syntax::StmtNode& node)
    {
        if ((node.kind == syntax::StmtKind::let || node.kind == syntax::StmtKind::var)
            && syntax::is_valid(node.name_id)) {
            this->index_result_carrier_binding(node.init, node.name_id, stmt);
            return;
        }
        if (node.kind != syntax::StmtKind::assign) {
            return;
        }
        const IdentId carrier_name_id = this->place_root_name_id(node.lhs);
        if (syntax::is_valid(carrier_name_id)) {
            this->index_result_carrier_binding(node.rhs, carrier_name_id, stmt);
        }
    }

    void index_result_carrier_binding(
        const syntax::ExprId result_expr, const IdentId carrier_name_id, const syntax::StmtId stmt)
    {
        if (!valid_expr(this->module_, result_expr) || !syntax::is_valid(carrier_name_id)) {
            return;
        }
        std::vector<base::u32> loan_exprs;
        this->collect_result_loan_exprs(result_expr, loan_exprs);
        if (loan_exprs.empty()) {
            return;
        }
        const BodyLoanCarrierBinding binding{
            .carrier_name_id = carrier_name_id,
            .enclosing_stmt = stmt,
            .carrier_definition_point = this->find_carrier_definition_point(carrier_name_id, stmt),
        };
        for (const base::u32 loan_expr : loan_exprs) {
            this->indexed_carrier_by_loan_expr_.try_emplace(loan_expr, binding);
        }
    }

    void collect_loans()
    {
        this->loan_by_issued_action_.assign(this->graph_.actions.size(), SEMA_BODY_LOAN_INVALID_INDEX);
        for (base::usize action_index = 0; action_index < this->graph_.actions.size(); ++action_index) {
            const BodyFlowAction& action = this->graph_.actions[action_index];
            if (!action_is_borrow(action.kind) || !valid_place(this->graph_, action.place)) {
                continue;
            }
            BodyLoan loan;
            loan.kind =
                action.kind == BodyFlowActionKind::borrow_mutable ? BodyLoanKind::mutable_ : BodyLoanKind::shared;
            loan.issued_action = base::checked_u32(action_index, SEMA_BODY_LOAN_ID_CONTEXT);
            loan.issued_point = action.point;
            loan.place = action.place;
            loan.origin = this->make_origin(this->graph_.places[action.place]);
            loan.enclosing_stmt = this->enclosing_statement(action);
            loan.expr = action.expr;
            loan.range = action.range;
            this->bind_carrier(loan);
            this->result_.origins.push_back(loan.origin);
            const base::u32 loan_index =
                base::checked_u32(this->result_.loans.size(), SEMA_BODY_LOAN_ID_CONTEXT);
            if (action_index < this->loan_by_issued_action_.size()) {
                this->loan_by_issued_action_[action_index] = loan_index;
            }
            this->result_.loans.push_back(std::move(loan));
        }
    }

    void bind_reborrow_parents()
    {
        this->effective_loan_places_.clear();
        this->effective_loan_places_.reserve(this->result_.loans.size());
        std::unordered_map<IdentId, BodyLoanLatestCarrierLoan, IdentIdHash> latest_loan_by_carrier;
        latest_loan_by_carrier.reserve(this->result_.loans.size());
        for (base::usize loan_index = 0; loan_index < this->result_.loans.size(); ++loan_index) {
            BodyLoan& loan = this->result_.loans[loan_index];
            this->bind_reborrow_parent_for_loan(loan, latest_loan_by_carrier);
            if (syntax::is_valid(loan.carrier_name_id)) {
                latest_loan_by_carrier[loan.carrier_name_id] = BodyLoanLatestCarrierLoan{
                    .loan = base::checked_u32(loan_index, SEMA_BODY_LOAN_ID_CONTEXT),
                    .issued_action = loan.issued_action,
                };
            }
        }
        for (base::usize loan_index = 0; loan_index < this->result_.loans.size(); ++loan_index) {
            this->effective_loan_places_.push_back(this->make_effective_loan_place(loan_index));
        }
    }

    void bind_reborrow_parent_for_loan(BodyLoan& loan,
        const std::unordered_map<IdentId, BodyLoanLatestCarrierLoan, IdentIdHash>& latest_loan_by_carrier) const
    {
        if (!valid_place(this->graph_, loan.place)) {
            return;
        }
        const BodyFlowPlace& place = this->graph_.places[loan.place];
        if (place.root_kind != BodyFlowPlaceRootKind::local || place.projections.empty()
            || place.projections.front().kind != BodyFlowPlaceProjectionKind::dereference) {
            return;
        }
        const IdentId carrier = place.root_name_id;
        if (!syntax::is_valid(carrier)) {
            return;
        }
        const auto parent = latest_loan_by_carrier.find(carrier);
        if (parent == latest_loan_by_carrier.end() || parent->second.issued_action >= loan.issued_action) {
            return;
        }
        loan.parent_loan = parent->second.loan;
    }

    [[nodiscard]] BodyFlowPlace make_effective_loan_place(const base::usize loan_index) const
    {
        const BodyLoan& loan = this->result_.loans[loan_index];
        if (valid_place(this->graph_, loan.place)) {
            const BodyFlowPlace& loan_place = this->graph_.places[loan.place];
            if (loan_place.root_kind == BodyFlowPlaceRootKind::unknown && syntax::is_valid(loan.carrier_name_id)) {
                BodyFlowPlace non_local_unknown;
                non_local_unknown.range = loan_place.range;
                return non_local_unknown;
            }
        }
        if (loan.parent_loan == SEMA_BODY_LOAN_INVALID_INDEX || loan.parent_loan >= this->result_.loans.size()
            || !valid_place(this->graph_, loan.place)) {
            return valid_place(this->graph_, loan.place) ? this->graph_.places[loan.place] : BodyFlowPlace{};
        }
        BodyFlowPlace place = this->loan_effective_place(loan.parent_loan);
        const BodyFlowPlace& child_place = this->graph_.places[loan.place];
        if (!child_place.projections.empty()) {
            place.projections.insert(
                place.projections.end(), child_place.projections.begin() + 1, child_place.projections.end());
        }
        place.range = child_place.range;
        return place;
    }

    void collect_two_phase_borrows()
    {
        this->two_phase_by_reservation_action_.assign(this->graph_.actions.size(), SEMA_BODY_LOAN_INVALID_INDEX);
        this->two_phase_by_activation_action_.assign(this->graph_.actions.size(), SEMA_BODY_LOAN_INVALID_INDEX);
        std::unordered_map<BodyLoanTwoPhaseActivationKey, BodyLoanTwoPhaseActivationQueue,
            BodyLoanTwoPhaseActivationKeyHash>
            activations = this->collect_two_phase_activation_index();
        for (base::usize reservation_index = 0; reservation_index < this->graph_.actions.size(); ++reservation_index) {
            const BodyFlowAction& reservation = this->graph_.actions[reservation_index];
            if (reservation.kind != BodyFlowActionKind::call_receiver_reserve
                || !valid_place(this->graph_, reservation.place)) {
                continue;
            }
            const std::optional<BodyLoanActionSite> activation =
                this->take_two_phase_activation(activations, reservation);
            if (!activation.has_value()) {
                continue;
            }
            BodyTwoPhaseBorrow borrow;
            borrow.reservation_action = base::checked_u32(reservation_index, SEMA_BODY_LOAN_ID_CONTEXT);
            borrow.activation_action = activation->action;
            borrow.reservation_point = reservation.point;
            borrow.activation_point = activation->point;
            borrow.place = reservation.place;
            borrow.call_expr = reservation.expr;
            borrow.range = reservation.range;
            const base::u32 index =
                base::checked_u32(this->result_.two_phase_borrows.size(), SEMA_BODY_LOAN_ID_CONTEXT);
            this->result_.two_phase_borrows.push_back(std::move(borrow));
            this->two_phase_by_reservation_action_[reservation_index] = index;
            this->two_phase_by_activation_action_[activation->action] = index;
        }
    }

    [[nodiscard]] std::unordered_map<BodyLoanTwoPhaseActivationKey, BodyLoanTwoPhaseActivationQueue,
        BodyLoanTwoPhaseActivationKeyHash>
    collect_two_phase_activation_index() const
    {
        std::unordered_map<BodyLoanTwoPhaseActivationKey, BodyLoanTwoPhaseActivationQueue,
            BodyLoanTwoPhaseActivationKeyHash>
            activations;
        activations.reserve(this->graph_.actions.size());
        for (base::usize action_index = 0; action_index < this->graph_.actions.size(); ++action_index) {
            const BodyFlowAction& action = this->graph_.actions[action_index];
            if (action.kind != BodyFlowActionKind::call_receiver_activate || !valid_place(this->graph_, action.place)) {
                continue;
            }
            BodyLoanTwoPhaseActivationQueue& queue = activations[BodyLoanTwoPhaseActivationKey{
                .expr = action.expr.value,
                .place = action.place,
            }];
            queue.activations.push_back(BodyLoanActionSite{
                .action = base::checked_u32(action_index, SEMA_BODY_LOAN_ID_CONTEXT),
                .point = action.point,
                .range = action.range,
            });
        }
        return activations;
    }

    [[nodiscard]] std::optional<BodyLoanActionSite> take_two_phase_activation(
        std::unordered_map<BodyLoanTwoPhaseActivationKey, BodyLoanTwoPhaseActivationQueue,
            BodyLoanTwoPhaseActivationKeyHash>& activations,
        const BodyFlowAction& reservation) const
    {
        const auto found = activations.find(BodyLoanTwoPhaseActivationKey{
            .expr = reservation.expr.value,
            .place = reservation.place,
        });
        if (found == activations.end() || found->second.next >= found->second.activations.size()) {
            return std::nullopt;
        }
        BodyLoanTwoPhaseActivationQueue& queue = found->second;
        const BodyLoanActionSite activation = queue.activations[queue.next];
        ++queue.next;
        return activation;
    }

    [[nodiscard]] BodyLoanOrigin make_origin(const BodyFlowPlace& place) const noexcept
    {
        BodyLoanOrigin origin;
        origin.range = place.range;
        switch (place.root_kind) {
            case BodyFlowPlaceRootKind::local:
                origin.kind = BodyLoanOriginKind::local;
                origin.name_id = place.root_name_id;
                origin.expr = place.root_expr;
                break;
            case BodyFlowPlaceRootKind::temporary:
                origin.kind = BodyLoanOriginKind::temporary;
                origin.expr = place.root_expr;
                break;
            case BodyFlowPlaceRootKind::unknown:
                origin.kind = BodyLoanOriginKind::unknown;
                origin.expr = place.root_expr;
                break;
            case BodyFlowPlaceRootKind::none:
                origin.kind = BodyLoanOriginKind::none;
                break;
        }
        return origin;
    }

    [[nodiscard]] syntax::StmtId enclosing_statement(const BodyFlowAction& action) const noexcept
    {
        if (syntax::is_valid(action.stmt)) {
            return action.stmt;
        }
        if (valid_point(this->graph_, action.point)) {
            return this->graph_.points[action.point].stmt;
        }
        return syntax::INVALID_STMT_ID;
    }

    void bind_carrier(BodyLoan& loan) const
    {
        if (syntax::is_valid(loan.expr)) {
            const auto direct = this->direct_carrier_by_borrow_operand_.find(loan.expr.value);
            if (direct != this->direct_carrier_by_borrow_operand_.end()) {
                loan.carrier_name_id = direct->second.carrier_name_id;
                loan.enclosing_stmt = direct->second.enclosing_stmt;
                loan.carrier_definition_point = direct->second.carrier_definition_point;
                return;
            }
            const auto indexed = this->indexed_carrier_by_loan_expr_.find(loan.expr.value);
            if (indexed != this->indexed_carrier_by_loan_expr_.end()) {
                loan.carrier_name_id = indexed->second.carrier_name_id;
                loan.enclosing_stmt = indexed->second.enclosing_stmt;
                loan.carrier_definition_point = indexed->second.carrier_definition_point;
                return;
            }
        }
        std::vector<syntax::StmtId> pending;
        pending.push_back(this->function_.body);
        while (!pending.empty()) {
            const syntax::StmtId stmt = pending.back();
            pending.pop_back();
            if (!valid_stmt(this->module_, stmt)) {
                continue;
            }
            const syntax::StmtNode& node = this->module_.stmts[stmt.value];
            if (this->bind_carrier_from_statement(loan, stmt, node)) {
                return;
            }
            this->push_child_statements(node, pending);
        }
    }

    [[nodiscard]] bool bind_carrier_from_statement(
        BodyLoan& loan, const syntax::StmtId stmt, const syntax::StmtNode& node) const
    {
        const syntax::ExprId local_init_borrow = this->direct_address_operand(node.init);
        if ((node.kind == syntax::StmtKind::let || node.kind == syntax::StmtKind::var) && syntax::is_valid(node.name_id)
            && ((syntax::is_valid(local_init_borrow) && local_init_borrow.value == loan.expr.value)
                || this->expr_result_contains_loan(node.init, loan))) {
            loan.carrier_name_id = node.name_id;
            loan.enclosing_stmt = stmt;
            loan.carrier_definition_point = this->find_carrier_definition_point(loan.carrier_name_id, stmt);
            return true;
        }
        const syntax::ExprId assignment_borrow = this->direct_address_operand(node.rhs);
        if (node.kind == syntax::StmtKind::assign && syntax::is_valid(assignment_borrow)
            && assignment_borrow.value == loan.expr.value) {
            const IdentId lhs = this->direct_name_id(node.lhs);
            if (syntax::is_valid(lhs)) {
                loan.carrier_name_id = lhs;
                loan.enclosing_stmt = stmt;
                loan.carrier_definition_point = this->find_carrier_definition_point(loan.carrier_name_id, stmt);
                return true;
            }
        }
        if (node.kind == syntax::StmtKind::assign && this->expr_is_loan_result(node.rhs, loan)) {
            const IdentId lhs = this->direct_name_id(node.lhs);
            if (syntax::is_valid(lhs)) {
                loan.carrier_name_id = lhs;
                loan.enclosing_stmt = stmt;
                loan.carrier_definition_point = this->find_carrier_definition_point(loan.carrier_name_id, stmt);
                return true;
            }
        }
        if (node.kind == syntax::StmtKind::assign && this->expr_result_contains_loan(node.rhs, loan)) {
            const IdentId lhs = this->place_root_name_id(node.lhs);
            if (syntax::is_valid(lhs)) {
                loan.carrier_name_id = lhs;
                loan.enclosing_stmt = stmt;
                loan.carrier_definition_point = this->find_carrier_definition_point(loan.carrier_name_id, stmt);
                return true;
            }
        }
        return false;
    }

    void collect_result_loan_exprs(const syntax::ExprId expr, std::vector<base::u32>& loan_exprs) const
    {
        std::vector<syntax::ExprId> pending{expr};
        std::unordered_set<base::u32> visited;
        while (!pending.empty()) {
            const syntax::ExprId current = pending.back();
            pending.pop_back();
            if (!valid_expr(this->module_, current) || !visited.insert(current.value).second) {
                continue;
            }
            loan_exprs.push_back(current.value);
            const syntax::ExprId borrowed_operand = this->direct_address_operand(current);
            if (syntax::is_valid(borrowed_operand)) {
                loan_exprs.push_back(borrowed_operand.value);
            }
            if (!this->type_contains_reference(this->cached_expr_type(current))) {
                continue;
            }
            this->push_value_result_children(pending, current);
        }
        this->sort_unique(loan_exprs);
    }

    [[nodiscard]] bool expr_is_loan_result(const syntax::ExprId expr, const BodyLoan& loan) const noexcept
    {
        return syntax::is_valid(expr) && syntax::is_valid(loan.expr) && expr.value == loan.expr.value;
    }

    [[nodiscard]] bool expr_result_contains_loan(const syntax::ExprId expr, const BodyLoan& loan) const
    {
        if (!valid_expr(this->module_, expr) || !valid_expr(this->module_, loan.expr)) {
            return false;
        }
        std::vector<syntax::ExprId> pending{expr};
        std::unordered_set<base::u32> visited;
        while (!pending.empty()) {
            const syntax::ExprId current = pending.back();
            pending.pop_back();
            if (!valid_expr(this->module_, current) || !visited.insert(current.value).second) {
                continue;
            }
            if (this->expr_is_loan_result(current, loan)) {
                return true;
            }
            const syntax::ExprId borrowed_operand = this->direct_address_operand(current);
            if (syntax::is_valid(borrowed_operand) && borrowed_operand.value == loan.expr.value) {
                return true;
            }
            if (!this->type_contains_reference(this->cached_expr_type(current))) {
                continue;
            }
            this->push_value_result_children(pending, current);
        }
        return false;
    }

    void push_value_result_children(std::vector<syntax::ExprId>& pending, const syntax::ExprId expr) const
    {
        const syntax::ExprKind kind = this->module_.exprs.kind(expr.value);
        if (const syntax::CastExprPayload* const cast = this->module_.exprs.cast_payload(expr.value); cast != nullptr
            && (kind == syntax::ExprKind::cast || kind == syntax::ExprKind::pcast || kind == syntax::ExprKind::bcast
                || kind == syntax::ExprKind::str_from_utf8_checked)) {
            pending.push_back(cast->expr);
            return;
        }
        if (const syntax::TryExprPayload* const try_expr = this->module_.exprs.try_payload(expr.value);
            kind == syntax::ExprKind::try_expr && try_expr != nullptr) {
            pending.push_back(try_expr->operand);
            return;
        }
        if (const syntax::IfExprPayload* const if_expr = this->module_.exprs.if_payload(expr.value);
            kind == syntax::ExprKind::if_expr && if_expr != nullptr) {
            pending.push_back(if_expr->else_expr);
            pending.push_back(if_expr->then_expr);
            return;
        }
        if (const syntax::MatchExprPayload* const match = this->module_.exprs.match_payload(expr.value);
            kind == syntax::ExprKind::match_expr && match != nullptr) {
            for (const syntax::MatchArm& arm : match->arms) {
                pending.push_back(arm.value);
            }
            return;
        }
        if (const syntax::ArrayExprPayload* const array = this->module_.exprs.array_payload(expr.value);
            kind == syntax::ExprKind::array_literal && array != nullptr) {
            pending.push_back(array->repeat_value);
            for (const syntax::ExprId element : array->elements) {
                pending.push_back(element);
            }
            return;
        }
        if (const syntax::AstArenaVector<syntax::ExprId>* const tuple = this->module_.exprs.tuple_elements(expr.value);
            kind == syntax::ExprKind::tuple_literal && tuple != nullptr) {
            pending.insert(pending.end(), tuple->begin(), tuple->end());
            return;
        }
        if (const syntax::StructLiteralExprPayload* const literal =
                this->module_.exprs.struct_literal_payload(expr.value);
            kind == syntax::ExprKind::struct_literal && literal != nullptr) {
            for (const syntax::FieldInit& field : literal->field_inits) {
                pending.push_back(field.value);
            }
            return;
        }
        if (const syntax::BlockExprPayload* const block = this->module_.exprs.block_payload(expr.value);
            block != nullptr && (kind == syntax::ExprKind::block_expr || kind == syntax::ExprKind::unsafe_block)) {
            pending.push_back(block->result);
            return;
        }
        if (const syntax::FieldExprPayload* const field = this->module_.exprs.field_payload(expr.value);
            kind == syntax::ExprKind::field && field != nullptr) {
            pending.push_back(field->object);
            return;
        }
        if (const syntax::IndexExprPayload* const index = this->module_.exprs.index_payload(expr.value);
            kind == syntax::ExprKind::index && index != nullptr) {
            pending.push_back(index->object);
            return;
        }
        if (const syntax::SliceExprPayload* const slice = this->module_.exprs.slice_payload(expr.value);
            kind == syntax::ExprKind::slice && slice != nullptr) {
            pending.push_back(slice->object);
            return;
        }
    }

    void push_child_statements(const syntax::StmtNode& node, std::vector<syntax::StmtId>& pending) const
    {
        switch (node.kind) {
            case syntax::StmtKind::block:
                pending.insert(pending.end(), node.statements.begin(), node.statements.end());
                break;
            case syntax::StmtKind::if_:
                pending.push_back(node.then_block);
                pending.push_back(node.else_block);
                pending.push_back(node.else_if);
                break;
            case syntax::StmtKind::for_:
                pending.push_back(node.for_init);
                pending.push_back(node.for_update);
                pending.push_back(node.body);
                break;
            case syntax::StmtKind::for_range:
            case syntax::StmtKind::while_:
                pending.push_back(node.body);
                break;
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
            case syntax::StmtKind::assign:
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
            case syntax::StmtKind::defer:
            case syntax::StmtKind::return_:
            case syntax::StmtKind::expr:
                break;
        }
    }

    [[nodiscard]] syntax::ExprId direct_address_operand(const syntax::ExprId expr) const noexcept
    {
        if (!valid_expr(this->module_, expr) || this->module_.exprs.kind(expr.value) != syntax::ExprKind::unary) {
            return syntax::INVALID_EXPR_ID;
        }
        const syntax::UnaryExprPayload* const unary = this->module_.exprs.unary_payload(expr.value);
        if (unary == nullptr
            || (unary->op != syntax::UnaryOp::address_of && unary->op != syntax::UnaryOp::address_of_mut)) {
            return syntax::INVALID_EXPR_ID;
        }
        return unary->operand;
    }

    [[nodiscard]] IdentId direct_name_id(const syntax::ExprId expr) const noexcept
    {
        if (!valid_expr(this->module_, expr) || this->module_.exprs.kind(expr.value) != syntax::ExprKind::name) {
            return INVALID_IDENT_ID;
        }
        const syntax::NameExprPayload* const name = this->module_.exprs.name_payload(expr.value);
        if (name == nullptr || !name->scope_name.empty()) {
            return INVALID_IDENT_ID;
        }
        return name->text_id;
    }

    [[nodiscard]] IdentId place_root_name_id(const syntax::ExprId expr) const noexcept
    {
        syntax::ExprId current = expr;
        std::unordered_set<base::u32> visited;
        while (valid_expr(this->module_, current) && visited.insert(current.value).second) {
            const syntax::ExprKind kind = this->module_.exprs.kind(current.value);
            if (kind == syntax::ExprKind::name) {
                return this->direct_name_id(current);
            }
            if (const syntax::FieldExprPayload* const field = this->module_.exprs.field_payload(current.value);
                kind == syntax::ExprKind::field && field != nullptr) {
                current = field->object;
                continue;
            }
            if (const syntax::IndexExprPayload* const index = this->module_.exprs.index_payload(current.value);
                kind == syntax::ExprKind::index && index != nullptr) {
                current = index->object;
                continue;
            }
            if (const syntax::SliceExprPayload* const slice = this->module_.exprs.slice_payload(current.value);
                kind == syntax::ExprKind::slice && slice != nullptr) {
                current = slice->object;
                continue;
            }
            if (const syntax::UnaryExprPayload* const unary = this->module_.exprs.unary_payload(current.value);
                kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::dereference) {
                current = unary->operand;
                continue;
            }
            break;
        }
        return INVALID_IDENT_ID;
    }

    [[nodiscard]] base::u32 find_carrier_definition_point(
        const IdentId carrier_name_id, const syntax::StmtId stmt) const noexcept
    {
        if (!syntax::is_valid(carrier_name_id) || !syntax::is_valid(stmt)) {
            return SEMA_BODY_FLOW_INVALID_INDEX;
        }
        const auto found = this->carrier_definition_points_.find(BodyLoanCarrierDefinitionKey{
            .name_id = carrier_name_id.value,
            .stmt = stmt.value,
        });
        if (found != this->carrier_definition_points_.end()) {
            return found->second;
        }
        return SEMA_BODY_FLOW_INVALID_INDEX;
    }

    void compute_carrier_liveness()
    {
        this->carrier_live_after_.assign(
            this->result_.loans.size(), std::vector<bool>(this->graph_.points.size(), false));
        const std::unordered_map<IdentId, BodyLoanCarrierLivenessActions, IdentIdHash> carrier_actions =
            this->collect_carrier_liveness_actions();
        for (base::usize loan_index = 0; loan_index < this->result_.loans.size(); ++loan_index) {
            const BodyLoan& loan = this->result_.loans[loan_index];
            if (!syntax::is_valid(loan.carrier_name_id)) {
                continue;
            }
            const auto actions = carrier_actions.find(loan.carrier_name_id);
            if (actions == carrier_actions.end() || actions->second.use_points.empty()) {
                continue;
            }
            std::vector<bool> uses(this->graph_.points.size(), false);
            std::vector<bool> definitions(this->graph_.points.size(), false);
            for (const base::u32 point : actions->second.use_points) {
                if (valid_point(this->graph_, point)) {
                    uses[point] = true;
                }
            }
            for (const base::u32 point : actions->second.definition_points) {
                if (valid_point(this->graph_, point) && point != loan.carrier_definition_point) {
                    definitions[point] = true;
                }
            }
            this->solve_live_after_for_loan(loan_index, uses, definitions);
        }
    }

    [[nodiscard]] std::unordered_map<IdentId, BodyLoanCarrierLivenessActions, IdentIdHash>
    collect_carrier_liveness_actions() const
    {
        std::unordered_map<IdentId, BodyLoanCarrierLivenessActions, IdentIdHash> result;
        result.reserve(this->result_.loans.size());
        for (base::usize action_index = 0; action_index < this->graph_.actions.size(); ++action_index) {
            const BodyFlowAction& action = this->graph_.actions[action_index];
            if (!valid_point(this->graph_, action.point) || !valid_place(this->graph_, action.place)) {
                continue;
            }
            const BodyFlowPlace& place = this->graph_.places[action.place];
            if (place.root_kind != BodyFlowPlaceRootKind::local || !syntax::is_valid(place.root_name_id)) {
                continue;
            }
            BodyLoanCarrierLivenessActions& actions = result[place.root_name_id];
            if (this->action_reads_carrier(
                    base::checked_u32(action_index, SEMA_BODY_LOAN_ID_CONTEXT), action, place.root_name_id)) {
                actions.use_points.push_back(action.point);
            }
            if (action_is_storage_definition(action.kind) && place.projections.empty()) {
                actions.definition_points.push_back(action.point);
            }
        }
        return result;
    }

    [[nodiscard]] bool action_place_has_root(const BodyFlowAction& action, const IdentId name_id) const noexcept
    {
        if (!valid_place(this->graph_, action.place)) {
            return false;
        }
        const BodyFlowPlace& place = this->graph_.places[action.place];
        return place.root_kind == BodyFlowPlaceRootKind::local && place.root_name_id == name_id;
    }

    [[nodiscard]] bool action_defines_whole_carrier(const BodyFlowAction& action, const IdentId name_id) const noexcept
    {
        if (!action_is_storage_definition(action.kind) || !valid_place(this->graph_, action.place)) {
            return false;
        }
        const BodyFlowPlace& place = this->graph_.places[action.place];
        return place.root_kind == BodyFlowPlaceRootKind::local && place.root_name_id == name_id
            && place.projections.empty();
    }

    [[nodiscard]] bool action_reads_carrier(
        const base::u32 action_index, const BodyFlowAction& action, const IdentId carrier_name_id) const noexcept
    {
        return syntax::is_valid(carrier_name_id) && action_index < this->read_action_observes_reference_.size()
            && this->read_action_observes_reference_[action_index] && syntax::is_valid(action.expr);
    }

    [[nodiscard]] bool read_action_observes_reference(const BodyFlowAction& action) const
    {
        if (action.kind != BodyFlowActionKind::read) {
            return false;
        }
        const TypeHandle expr_type = this->cached_expr_type(action.expr);
        if (!is_valid(expr_type)) {
            return syntax::is_valid(action.expr);
        }
        if (this->type_contains_reference(expr_type)) {
            return true;
        }
        if (!valid_place(this->graph_, action.place)) {
            return false;
        }
        const BodyFlowPlace& place = this->graph_.places[action.place];
        if (this->type_is_direct_borrow_carrier(this->cached_expr_type(place.root_expr))) {
            return true;
        }
        return std::ranges::any_of(place.projections, [](const BodyFlowPlaceProjection& projection) {
            return projection.kind == BodyFlowPlaceProjectionKind::dereference;
        });
    }

    [[nodiscard]] bool type_is_direct_borrow_carrier(const TypeHandle type) const noexcept
    {
        if (!is_valid(type) || type.value >= this->checked_.types.size()) {
            return false;
        }
        const TypeInfo& info = this->checked_.types.get(type);
        switch (info.kind) {
            case TypeKind::builtin:
                return info.builtin == BuiltinType::str;
            case TypeKind::reference:
            case TypeKind::slice:
            case TypeKind::generic_param:
            case TypeKind::associated_projection:
                return true;
            case TypeKind::array:
            case TypeKind::tuple:
            case TypeKind::struct_:
            case TypeKind::enum_:
            case TypeKind::pointer:
            case TypeKind::function:
            case TypeKind::opaque_struct:
                return false;
        }
        return false;
    }

    void solve_live_after_for_loan(
        const base::usize loan_index, const std::vector<bool>& uses, const std::vector<bool>& definitions)
    {
        std::vector<bool>& live_after = this->carrier_live_after_[loan_index];
        bool changed = true;
        while (changed) {
            changed = false;
            for (base::usize reverse_index = this->graph_.points.size(); reverse_index > 0; --reverse_index) {
                const base::usize point = reverse_index - 1;
                bool next_live = false;
                for (const base::u32 successor : this->index_.outgoing_points[point]) {
                    next_live = next_live || uses[successor] || (live_after[successor] && !definitions[successor]);
                }
                if (next_live != live_after[point]) {
                    live_after[point] = next_live;
                    changed = true;
                }
            }
        }
    }

    void check_two_phase_reservations()
    {
        for (base::usize two_phase_index = 0; two_phase_index < this->result_.two_phase_borrows.size();
            ++two_phase_index) {
            this->check_two_phase_reservation(two_phase_index);
        }
    }

    void check_two_phase_reservation(const base::usize two_phase_index)
    {
        const BodyTwoPhaseBorrow& borrow = this->result_.two_phase_borrows[two_phase_index];
        if (!valid_point(this->graph_, borrow.reservation_point)
            || !valid_action(this->graph_, borrow.activation_action) || !valid_place(this->graph_, borrow.place)) {
            return;
        }
        std::vector<bool> visited(this->graph_.points.size(), false);
        std::vector<base::u32> pending;
        pending.reserve(SEMA_BODY_LOAN_INITIAL_WORKLIST_CAPACITY);
        pending.push_back(borrow.reservation_point);
        visited[borrow.reservation_point] = true;
        while (!pending.empty()) {
            const base::u32 point = pending.back();
            pending.pop_back();
            if (this->check_two_phase_reservation_point(two_phase_index, point)) {
                continue;
            }
            for (const base::u32 successor : this->index_.outgoing_points[point]) {
                if (successor >= visited.size() || visited[successor]) {
                    continue;
                }
                visited[successor] = true;
                pending.push_back(successor);
            }
        }
    }

    [[nodiscard]] bool check_two_phase_reservation_point(const base::usize two_phase_index, const base::u32 point)
    {
        const BodyTwoPhaseBorrow& borrow = this->result_.two_phase_borrows[two_phase_index];
        if (!valid_point(this->graph_, point)) {
            return true;
        }
        for (const base::u32 action_index : this->index_.actions_by_point[point]) {
            if (action_index == borrow.activation_action) {
                return true;
            }
            if (!valid_action(this->graph_, action_index) || action_index == borrow.reservation_action) {
                continue;
            }
            const BodyFlowAction& action = this->graph_.actions[action_index];
            if ((action.kind == BodyFlowActionKind::call_receiver_reserve
                    || action.kind == BodyFlowActionKind::call_receiver_activate)
                && valid_place(this->graph_, action.place)
                && places_conflict(this->graph_.places[borrow.place], this->graph_.places[action.place])) {
                this->record_two_phase_conflict(
                    two_phase_index, action_index, action, BodyLoanConflictKind::two_phase_reservation);
                continue;
            }
            std::optional<BodyLoanAccessKind> access = access_kind_for_action(action.kind);
            if (access == BodyLoanAccessKind::move && !this->move_action_consumes_action(action_index)) {
                access = std::nullopt;
            }
            if (!access.has_value() || !this->two_phase_reservation_access_conflicts(access.value())
                || !valid_place(this->graph_, action.place)) {
                continue;
            }
            if (!places_conflict(this->graph_.places[borrow.place], this->graph_.places[action.place])) {
                continue;
            }
            this->record_two_phase_conflict(
                two_phase_index, action_index, action, BodyLoanConflictKind::two_phase_reservation);
        }
        return false;
    }

    [[nodiscard]] bool two_phase_reservation_access_conflicts(const BodyLoanAccessKind access) const noexcept
    {
        switch (access) {
            case BodyLoanAccessKind::read:
            case BodyLoanAccessKind::shared_borrow:
                return false;
            case BodyLoanAccessKind::write:
            case BodyLoanAccessKind::reinit:
            case BodyLoanAccessKind::move:
            case BodyLoanAccessKind::drop:
            case BodyLoanAccessKind::mutable_borrow:
            case BodyLoanAccessKind::cleanup:
                return true;
        }
        return true;
    }

    void propagate_active_loans()
    {
        const base::usize point_count = this->graph_.points.size();
        this->active_in_.assign(point_count, {});
        this->active_out_.assign(point_count, {});
        std::vector<base::u32> worklist;
        worklist.reserve(SEMA_BODY_LOAN_INITIAL_WORKLIST_CAPACITY);
        std::vector<bool> queued(point_count, false);
        for (base::usize point = 0; point < point_count; ++point) {
            worklist.push_back(base::checked_u32(point, SEMA_BODY_LOAN_ID_CONTEXT));
            queued[point] = true;
        }
        while (!worklist.empty()) {
            const base::u32 point = worklist.back();
            worklist.pop_back();
            queued[point] = false;
            std::vector<base::u32> merged_in = this->merge_predecessor_out(point);
            if (merged_in != this->active_in_[point]) {
                this->active_in_[point] = std::move(merged_in);
            }
            std::vector<base::u32> out = this->active_in_[point];
            this->process_point(point, out);
            this->sort_unique(out);
            if (out == this->active_out_[point]) {
                continue;
            }
            this->active_out_[point] = std::move(out);
            for (const base::u32 successor : this->index_.outgoing_points[point]) {
                if (!queued[successor]) {
                    worklist.push_back(successor);
                    queued[successor] = true;
                }
            }
        }
    }

    [[nodiscard]] std::vector<base::u32> merge_predecessor_out(const base::u32 point) const
    {
        std::vector<base::u32> merged;
        for (const base::u32 predecessor : this->index_.incoming_points[point]) {
            merged.insert(merged.end(), this->active_out_[predecessor].begin(), this->active_out_[predecessor].end());
        }
        this->sort_unique(merged);
        return merged;
    }

    void process_point(const base::u32 point, std::vector<base::u32>& active)
    {
        this->expire_loans_at_point(point, active);
        for (const base::u32 action_index : this->index_.actions_by_point[point]) {
            if (!valid_action(this->graph_, action_index)) {
                continue;
            }
            const BodyFlowAction& action = this->graph_.actions[action_index];
            std::optional<BodyLoanAccessKind> access = access_kind_for_action(action.kind);
            if (access == BodyLoanAccessKind::move && !this->move_action_consumes_action(action_index)) {
                access = std::nullopt;
            }
            if (access.has_value() && valid_place(this->graph_, action.place)) {
                this->check_action_conflicts(action_index, action, access.value(), active);
            }
            if (action.kind == BodyFlowActionKind::call_receiver_activate && valid_place(this->graph_, action.place)) {
                this->check_two_phase_activation_conflicts(action_index, action, active);
            }
            if (action_is_borrow(action.kind)) {
                this->activate_loan_for_action(action_index, active);
            }
        }
        this->expire_loans_at_point(point, active);
    }

    void activate_loan_for_action(const base::u32 action_index, std::vector<base::u32>& active) const
    {
        if (action_index >= this->loan_by_issued_action_.size()) {
            return;
        }
        const base::u32 loan_index = this->loan_by_issued_action_[action_index];
        if (loan_index != SEMA_BODY_LOAN_INVALID_INDEX) {
            active.push_back(loan_index);
        }
    }

    void expire_loans_at_point(const base::u32 point, std::vector<base::u32>& active) const
    {
        active.erase(std::remove_if(active.begin(), active.end(),
                         [this, point](const base::u32 loan_index) {
                             return loan_index >= this->result_.loans.size()
                                 || !this->loan_live_after_point(this->result_.loans[loan_index], loan_index, point);
                         }),
            active.end());
    }

    [[nodiscard]] bool loan_live_after_point(
        const BodyLoan& loan, const base::usize loan_index, const base::u32 point) const noexcept
    {
        if (syntax::is_valid(loan.carrier_name_id)) {
            const bool has_liveness =
                loan_index < this->carrier_live_after_.size() && point < this->carrier_live_after_[loan_index].size();
            if (point == loan.issued_point && valid_point(this->graph_, loan.carrier_definition_point)
                && loan_index < this->carrier_live_after_.size()
                && loan.carrier_definition_point < this->carrier_live_after_[loan_index].size()) {
                return this->carrier_live_after_[loan_index][loan.carrier_definition_point];
            }
            if (point == loan.carrier_definition_point && has_liveness) {
                return this->carrier_live_after_[loan_index][point];
            }
            return loan_index < this->carrier_live_after_.size() && point < this->carrier_live_after_[loan_index].size()
                && this->carrier_live_after_[loan_index][point];
        }
        return point == loan.issued_point;
    }

    void check_action_conflicts(const base::u32 action_index, const BodyFlowAction& action,
        const BodyLoanAccessKind access, const std::vector<base::u32>& active)
    {
        for (const base::u32 loan_index : active) {
            if (loan_index >= this->result_.loans.size()) {
                continue;
            }
            const BodyLoan& loan = this->result_.loans[loan_index];
            if (loan.issued_action == action_index || !valid_place(this->graph_, loan.place)) {
                continue;
            }
            if (this->action_defines_loan_carrier(action, loan)) {
                continue;
            }
            if (this->record_reborrow_parent_use_conflict(loan_index, action_index, action, access)) {
                continue;
            }
            if (!loan_conflicts_with_access(loan.kind, access)) {
                continue;
            }
            const BodyFlowPlace& loan_place = this->loan_effective_place(loan_index);
            const BodyFlowPlace& action_place = this->graph_.places[action.place];
            if (!places_conflict(loan_place, action_place)) {
                continue;
            }
            this->record_conflict(loan_index, action_index, action, access);
        }
    }

    [[nodiscard]] bool action_defines_loan_carrier(const BodyFlowAction& action, const BodyLoan& loan) const noexcept
    {
        return action.point == loan.carrier_definition_point
            && this->action_defines_whole_carrier(action, loan.carrier_name_id);
    }

    [[nodiscard]] bool record_reborrow_parent_use_conflict(const base::u32 loan_index, const base::u32 action_index,
        const BodyFlowAction& action, const BodyLoanAccessKind access)
    {
        if (loan_index >= this->result_.loans.size()) {
            return false;
        }
        const BodyLoan& child = this->result_.loans[loan_index];
        if (child.parent_loan == SEMA_BODY_LOAN_INVALID_INDEX || child.parent_loan >= this->result_.loans.size()
            || action.point == child.issued_point) {
            return false;
        }
        const BodyLoan& parent = this->result_.loans[child.parent_loan];
        if (!this->action_place_has_root(action, parent.carrier_name_id)
            || !reborrow_parent_access_conflicts(parent.kind, child.kind, access)) {
            return false;
        }
        this->record_loan_conflict(BodyLoanConflictKind::reborrow_parent_use, loan_index, action_index, action);
        return true;
    }

    void check_two_phase_activation_conflicts(
        const base::u32 action_index, const BodyFlowAction& action, const std::vector<base::u32>& active)
    {
        const base::u32 two_phase_index = this->two_phase_index_for_activation(action_index);
        if (two_phase_index == SEMA_BODY_LOAN_INVALID_INDEX) {
            return;
        }
        for (const base::u32 loan_index : active) {
            if (loan_index >= this->result_.loans.size()
                || !valid_place(this->graph_, this->result_.loans[loan_index].place)) {
                continue;
            }
            const BodyLoan& loan = this->result_.loans[loan_index];
            if (!loan_conflicts_with_access(loan.kind, BodyLoanAccessKind::mutable_borrow)) {
                continue;
            }
            const BodyFlowPlace& loan_place = this->loan_effective_place(loan_index);
            const BodyFlowPlace& action_place = this->graph_.places[action.place];
            if (!places_conflict(loan_place, action_place)) {
                continue;
            }
            this->record_two_phase_activation_conflict(two_phase_index, loan_index, action_index, action);
        }
    }

    [[nodiscard]] const BodyFlowPlace& loan_effective_place(const base::usize loan_index) const noexcept
    {
        if (loan_index < this->effective_loan_places_.size()) {
            return this->effective_loan_places_[loan_index];
        }
        const BodyLoan& loan = this->result_.loans[loan_index];
        return this->graph_.places[loan.place];
    }

    [[nodiscard]] base::u32 two_phase_index_for_activation(const base::u32 action_index) const noexcept
    {
        if (action_index < this->two_phase_by_activation_action_.size()) {
            return this->two_phase_by_activation_action_[action_index];
        }
        return SEMA_BODY_LOAN_INVALID_INDEX;
    }

    void record_conflict(const base::u32 loan_index, const base::u32 action_index, const BodyFlowAction& action,
        const BodyLoanAccessKind access)
    {
        this->record_loan_conflict(conflict_kind_for_access(access), loan_index, action_index, action);
    }

    void record_loan_conflict(const BodyLoanConflictKind kind, const base::u32 loan_index, const base::u32 action_index,
        const BodyFlowAction& action)
    {
        constexpr std::uint64_t SEMA_BODY_LOAN_CONFLICT_KEY_SHIFT = 32;
        const std::uint64_t key =
            (static_cast<std::uint64_t>(loan_index) << SEMA_BODY_LOAN_CONFLICT_KEY_SHIFT) | action_index;
        if (!this->seen_conflicts_.insert(key).second) {
            return;
        }
        const BodyLoanCarrierUseSite later_use = this->find_later_carrier_use(loan_index, action_index, action.point);
        this->result_.conflicts.push_back(BodyLoanConflict{
            .kind = kind,
            .loan = loan_index,
            .action = action_index,
            .point = action.point,
            .place = action.place,
            .diagnostic_emitted = false,
            .range = action.range,
            .later_use_point = later_use.point,
            .later_use_range = later_use.range,
        });
    }

    void record_two_phase_conflict(const base::usize two_phase_index, const base::u32 action_index,
        const BodyFlowAction& action, const BodyLoanConflictKind kind)
    {
        constexpr std::uint64_t SEMA_BODY_LOAN_CONFLICT_KEY_SHIFT = 32;
        const std::uint64_t key =
            (static_cast<std::uint64_t>(two_phase_index) << SEMA_BODY_LOAN_CONFLICT_KEY_SHIFT) | action_index;
        if (!this->seen_two_phase_conflicts_.insert(key).second) {
            return;
        }
        this->result_.conflicts.push_back(BodyLoanConflict{
            .kind = kind,
            .loan = SEMA_BODY_LOAN_INVALID_INDEX,
            .two_phase_borrow = base::checked_u32(two_phase_index, SEMA_BODY_LOAN_ID_CONTEXT),
            .action = action_index,
            .point = action.point,
            .place = action.place,
            .diagnostic_emitted = false,
            .range = action.range,
        });
    }

    void record_two_phase_activation_conflict(const base::u32 two_phase_index, const base::u32 loan_index,
        const base::u32 action_index, const BodyFlowAction& action)
    {
        constexpr std::uint64_t SEMA_BODY_LOAN_CONFLICT_KEY_SHIFT = 32;
        const std::uint64_t key =
            (static_cast<std::uint64_t>(two_phase_index) << SEMA_BODY_LOAN_CONFLICT_KEY_SHIFT) | action_index;
        if (!this->seen_two_phase_conflicts_.insert(key).second) {
            return;
        }
        this->result_.conflicts.push_back(BodyLoanConflict{
            .kind = BodyLoanConflictKind::two_phase_activation,
            .loan = loan_index,
            .two_phase_borrow = two_phase_index,
            .action = action_index,
            .point = action.point,
            .place = action.place,
            .diagnostic_emitted = false,
            .range = action.range,
        });
    }

    [[nodiscard]] BodyLoanCarrierUseSite find_later_carrier_use(
        const base::u32 loan_index, const base::u32 action_index, const base::u32 conflict_point) const
    {
        if (loan_index >= this->result_.loans.size()) {
            return {};
        }
        const BodyLoan& loan = this->result_.loans[loan_index];
        if (!syntax::is_valid(loan.carrier_name_id)) {
            return {};
        }
        const bool live_after_conflict = loan_index < this->carrier_live_after_.size()
            && conflict_point < this->carrier_live_after_[loan_index].size()
            && this->carrier_live_after_[loan_index][conflict_point];
        if (!live_after_conflict) {
            return {};
        }

        std::vector<bool> visited(this->graph_.points.size(), false);
        std::vector<base::u32> pending;
        pending.reserve(SEMA_BODY_LOAN_INITIAL_WORKLIST_CAPACITY);
        pending.push_back(conflict_point);
        if (valid_point(this->graph_, conflict_point)) {
            visited[conflict_point] = true;
        }
        while (!pending.empty()) {
            const base::u32 point = pending.back();
            pending.pop_back();
            bool carrier_redefined = false;
            const std::optional<BodyLoanCarrierUseSite> use = this->find_later_carrier_use_at_point(
                loan, point, action_index, point == conflict_point, carrier_redefined);
            if (use.has_value()) {
                return *use;
            }
            if (carrier_redefined || !valid_point(this->graph_, point)) {
                continue;
            }
            for (const base::u32 successor : this->index_.outgoing_points[point]) {
                if (successor >= visited.size() || visited[successor]) {
                    continue;
                }
                visited[successor] = true;
                pending.push_back(successor);
            }
        }
        return {};
    }

    [[nodiscard]] std::optional<BodyLoanCarrierUseSite> find_later_carrier_use_at_point(const BodyLoan& loan,
        const base::u32 point, const base::u32 action_index, const bool skip_through_action,
        bool& carrier_redefined) const
    {
        carrier_redefined = false;
        if (!valid_point(this->graph_, point)) {
            return std::nullopt;
        }
        for (const base::u32 candidate_index : this->index_.actions_by_point[point]) {
            if (skip_through_action && candidate_index <= action_index) {
                continue;
            }
            if (!valid_action(this->graph_, candidate_index)) {
                continue;
            }
            const BodyFlowAction& candidate = this->graph_.actions[candidate_index];
            if (!this->action_place_has_root(candidate, loan.carrier_name_id)) {
                continue;
            }
            if (this->action_reads_carrier(candidate_index, candidate, loan.carrier_name_id)) {
                return BodyLoanCarrierUseSite{
                    .point = candidate.point,
                    .range = candidate.range,
                };
            }
            if (this->action_defines_whole_carrier(candidate, loan.carrier_name_id)
                && candidate.point != loan.carrier_definition_point) {
                carrier_redefined = true;
                return std::nullopt;
            }
        }
        return std::nullopt;
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

    [[nodiscard]] bool type_contains_reference(const TypeHandle type) const
    {
        if (!is_valid(type)) {
            return false;
        }
        if (type.value < this->type_contains_reference_cache_.size()) {
            const base::u8 cached = this->type_contains_reference_cache_[type.value];
            if (cached == SEMA_BODY_LOAN_TYPE_REF_CACHE_TRUE) {
                return true;
            }
            if (cached == SEMA_BODY_LOAN_TYPE_REF_CACHE_FALSE) {
                return false;
            }
        }
        const bool result = this->compute_type_contains_reference(type);
        if (type.value < this->type_contains_reference_cache_.size()) {
            this->type_contains_reference_cache_[type.value] =
                result ? SEMA_BODY_LOAN_TYPE_REF_CACHE_TRUE : SEMA_BODY_LOAN_TYPE_REF_CACHE_FALSE;
        }
        return result;
    }

    [[nodiscard]] bool compute_type_contains_reference(const TypeHandle type) const
    {
        std::vector<TypeHandle> pending;
        pending.reserve(SEMA_BODY_LOAN_PRECHECK_INITIAL_STACK_CAPACITY);
        std::unordered_set<base::u32> visited;
        pending.push_back(type);
        while (!pending.empty()) {
            const TypeHandle current = pending.back();
            pending.pop_back();
            if (!is_valid(current) || current.value >= this->checked_.types.size()
                || !visited.insert(current.value).second) {
                continue;
            }
            if (current.value < this->type_contains_reference_cache_.size()) {
                const base::u8 cached = this->type_contains_reference_cache_[current.value];
                if (cached == SEMA_BODY_LOAN_TYPE_REF_CACHE_TRUE) {
                    return true;
                }
                if (cached == SEMA_BODY_LOAN_TYPE_REF_CACHE_FALSE) {
                    continue;
                }
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

    [[nodiscard]] bool move_action_consumes_action(const base::u32 action_index) const noexcept
    {
        return action_index < this->move_action_consumes_.size() && this->move_action_consumes_[action_index];
    }

    static void sort_unique(std::vector<base::u32>& values)
    {
        std::ranges::sort(values);
        values.erase(std::ranges::unique(values).begin(), values.end());
    }

    const syntax::AstModule& module_;
    const syntax::ItemNode& function_;
    const CheckedModule& checked_;
    const BodyFlowGraph& graph_;
    std::vector<bool> move_action_consumes_;
    BodyLoanCheckResult result_;
    BodyLoanActionIndex index_;
    std::vector<bool> read_action_observes_reference_;
    std::unordered_map<BodyLoanCarrierDefinitionKey, base::u32, BodyLoanCarrierDefinitionKeyHash>
        carrier_definition_points_;
    std::unordered_map<base::u32, BodyLoanCarrierBinding> direct_carrier_by_borrow_operand_;
    std::unordered_map<base::u32, BodyLoanCarrierBinding> indexed_carrier_by_loan_expr_;
    std::vector<base::u32> loan_by_issued_action_;
    std::vector<std::vector<bool>> carrier_live_after_;
    std::vector<std::vector<base::u32>> active_in_;
    std::vector<std::vector<base::u32>> active_out_;
    std::vector<BodyFlowPlace> effective_loan_places_;
    mutable std::vector<base::u8> type_contains_reference_cache_ =
        std::vector<base::u8>(checked_.types.size(), SEMA_BODY_LOAN_TYPE_REF_CACHE_UNKNOWN);
    std::vector<base::u32> two_phase_by_reservation_action_;
    std::vector<base::u32> two_phase_by_activation_action_;
    std::unordered_set<std::uint64_t> seen_conflicts_;
    std::unordered_set<std::uint64_t> seen_two_phase_conflicts_;
};

} // namespace

std::string_view body_loan_kind_name(const BodyLoanKind kind) noexcept
{
    switch (kind) {
        case BodyLoanKind::shared:
            return "shared";
        case BodyLoanKind::mutable_:
            return "mutable";
    }
    return "<invalid>";
}

std::string_view body_loan_origin_kind_name(const BodyLoanOriginKind kind) noexcept
{
    switch (kind) {
        case BodyLoanOriginKind::none:
            return "none";
        case BodyLoanOriginKind::local:
            return "local";
        case BodyLoanOriginKind::temporary:
            return "temporary";
        case BodyLoanOriginKind::unknown:
            return "unknown";
    }
    return "<invalid>";
}

std::string_view body_loan_diagnostic_mode_name(const BodyLoanDiagnosticMode mode) noexcept
{
    switch (mode) {
        case BodyLoanDiagnosticMode::shadow:
            return "shadow";
        case BodyLoanDiagnosticMode::enforced:
            return "enforced";
    }
    return "<invalid>";
}

std::string_view body_loan_conflict_kind_name(const BodyLoanConflictKind kind) noexcept
{
    switch (kind) {
        case BodyLoanConflictKind::read:
            return "read";
        case BodyLoanConflictKind::write:
            return "write";
        case BodyLoanConflictKind::reinit:
            return "reinit";
        case BodyLoanConflictKind::move:
            return "move";
        case BodyLoanConflictKind::drop:
            return "drop";
        case BodyLoanConflictKind::shared_borrow:
            return "shared_borrow";
        case BodyLoanConflictKind::mutable_borrow:
            return "mutable_borrow";
        case BodyLoanConflictKind::cleanup:
            return "cleanup";
        case BodyLoanConflictKind::reborrow_parent_use:
            return "reborrow_parent_use";
        case BodyLoanConflictKind::two_phase_reservation:
            return "two_phase_reservation";
        case BodyLoanConflictKind::two_phase_activation:
            return "two_phase_activation";
    }
    return "<invalid>";
}

query::StableFingerprint128 body_loan_check_fingerprint(const BodyLoanCheckResult& result) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_BODY_LOAN_FINGERPRINT_MARKER);
    mix_function_key(builder, result.function);
    builder.mix_u8(static_cast<base::u8>(result.diagnostic_mode));
    builder.mix_bool(result.graph_missing);

    builder.mix_u64(result.origins.size());
    for (const BodyLoanOrigin& origin : result.origins) {
        mix_origin(builder, origin);
    }

    builder.mix_u64(result.loans.size());
    for (const BodyLoan& loan : result.loans) {
        builder.mix_u8(static_cast<base::u8>(loan.kind));
        mix_origin(builder, loan.origin);
        builder.mix_u32(loan.parent_loan);
        builder.mix_u32(loan.issued_action);
        builder.mix_u32(loan.issued_point);
        builder.mix_u32(loan.place);
        builder.mix_u32(loan.carrier_name_id.value);
        builder.mix_u32(loan.carrier_definition_point);
        builder.mix_u32(loan.enclosing_stmt.value);
        builder.mix_u32(loan.expr.value);
    }

    builder.mix_u64(result.two_phase_borrows.size());
    for (const BodyTwoPhaseBorrow& borrow : result.two_phase_borrows) {
        mix_two_phase_borrow(builder, borrow);
    }

    builder.mix_u64(result.conflicts.size());
    for (const BodyLoanConflict& conflict : result.conflicts) {
        builder.mix_u8(static_cast<base::u8>(conflict.kind));
        builder.mix_u32(conflict.loan);
        builder.mix_u32(conflict.two_phase_borrow);
        builder.mix_u32(conflict.action);
        builder.mix_u32(conflict.point);
        builder.mix_u32(conflict.place);
        builder.mix_bool(conflict.diagnostic_emitted);
        builder.mix_u32(conflict.later_use_point);
    }
    return builder.finish();
}

std::string dump_body_loan_check_result(const BodyLoanCheckResult& result)
{
    std::ostringstream stream;
    stream << "body_loans function=" << result.function.module << ':' << result.function.owner_type << ':';
    append_optional_name_id(stream, result.function.name);
    stream << " mode=" << body_loan_diagnostic_mode_name(result.diagnostic_mode)
           << " graph_missing=" << (result.graph_missing ? "true" : "false") << '\n';

    stream << "origins:\n";
    for (base::usize index = 0; index < result.origins.size(); ++index) {
        const BodyLoanOrigin& origin = result.origins[index];
        stream << "  o" << index << ' ' << body_loan_origin_kind_name(origin.kind) << " name=";
        append_optional_name_id(stream, origin.name_id);
        stream << " expr=";
        append_optional_expr_id(stream, origin.expr);
        stream << " range=";
        append_range(stream, origin.range);
        stream << '\n';
    }

    stream << "loans:\n";
    for (base::usize index = 0; index < result.loans.size(); ++index) {
        const BodyLoan& loan = result.loans[index];
        stream << "  l" << index << ' ' << body_loan_kind_name(loan.kind) << " place=" << loan.place << " parent=";
        if (loan.parent_loan == SEMA_BODY_LOAN_INVALID_INDEX) {
            stream << '-';
        } else {
            stream << 'l' << loan.parent_loan;
        }
        stream << " issued_action=a" << loan.issued_action << " issued_point=p" << loan.issued_point << " carrier=";
        append_optional_name_id(stream, loan.carrier_name_id);
        stream << " stmt=";
        append_optional_stmt_id(stream, loan.enclosing_stmt);
        stream << " range=";
        append_range(stream, loan.range);
        stream << '\n';
    }

    stream << "two_phase_borrows:\n";
    for (base::usize index = 0; index < result.two_phase_borrows.size(); ++index) {
        const BodyTwoPhaseBorrow& borrow = result.two_phase_borrows[index];
        stream << "  t" << index << " place=" << borrow.place << " reserve=a" << borrow.reservation_action << "/p"
               << borrow.reservation_point << " activate=a" << borrow.activation_action << "/p"
               << borrow.activation_point << " call=";
        append_optional_expr_id(stream, borrow.call_expr);
        stream << " emitted=" << (borrow.diagnostic_emitted ? "true" : "false") << " range=";
        append_range(stream, borrow.range);
        stream << '\n';
    }

    stream << "conflicts:\n";
    for (base::usize index = 0; index < result.conflicts.size(); ++index) {
        const BodyLoanConflict& conflict = result.conflicts[index];
        stream << "  c" << index << ' ' << body_loan_conflict_kind_name(conflict.kind) << " loan=l" << conflict.loan
               << " two_phase=t" << conflict.two_phase_borrow << " action=a" << conflict.action << " point=p"
               << conflict.point << " place=" << conflict.place
               << " emitted=" << (conflict.diagnostic_emitted ? "true" : "false") << " range=";
        append_range(stream, conflict.range);
        stream << " later_use=p" << conflict.later_use_point << " range=";
        append_range(stream, conflict.later_use_range);
        stream << '\n';
    }
    stream << "fingerprint=" << query::debug_string(body_loan_check_fingerprint(result)) << '\n';
    return stream.str();
}

SemanticAnalyzerCore::BodyLoanChecker::BodyLoanChecker(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

bool SemanticAnalyzerCore::BodyLoanChecker::type_contains_reference(const TypeHandle type) const
{
    if (!is_valid(type)) {
        return false;
    }
    std::vector<TypeHandle> pending;
    pending.reserve(SEMA_BODY_LOAN_PRECHECK_INITIAL_STACK_CAPACITY);
    std::unordered_set<base::u32> visited;
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || current.value >= this->core_.state_.checked.types.size()
            || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(current);
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
                const StructInfo* const structure = this->core_.find_struct(current);
                if (structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case TypeKind::enum_: {
                const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(current);
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
            case TypeKind::pointer:
            case TypeKind::function:
            case TypeKind::opaque_struct:
                break;
        }
    }
    return false;
}

bool SemanticAnalyzerCore::BodyLoanChecker::statement_may_need_local_loan_check(const syntax::StmtId stmt_id) const
{
    if (!valid_stmt(this->core_.ctx_.module, stmt_id)) {
        return false;
    }
    if (this->statement_may_bind_reference_loan_shallow(stmt_id)) {
        return true;
    }
    const syntax::StmtNode& stmt = this->core_.ctx_.module.stmts[stmt_id.value];
    switch (stmt.kind) {
        case syntax::StmtKind::let:
        case syntax::StmtKind::var:
            return this->expr_may_need_local_loan_check(stmt.init);
        case syntax::StmtKind::assign:
            return this->expr_may_need_local_loan_check(stmt.lhs)
                || this->expr_may_need_local_loan_check(stmt.rhs);
        case syntax::StmtKind::if_:
        case syntax::StmtKind::while_:
            return this->expr_may_need_local_loan_check(stmt.condition);
        case syntax::StmtKind::for_:
            return this->expr_may_need_local_loan_check(stmt.condition);
        case syntax::StmtKind::for_range:
            return this->expr_may_need_local_loan_check(stmt.range_start)
                || this->expr_may_need_local_loan_check(stmt.range_end)
                || this->expr_may_need_local_loan_check(stmt.range_step);
        case syntax::StmtKind::defer:
        case syntax::StmtKind::expr:
            return this->expr_may_need_local_loan_check(stmt.init);
        case syntax::StmtKind::return_:
            return this->expr_may_need_local_loan_check(stmt.return_value);
        case syntax::StmtKind::break_:
        case syntax::StmtKind::continue_:
        case syntax::StmtKind::block:
            return false;
    }
    return false;
}

bool SemanticAnalyzerCore::BodyLoanChecker::statement_may_bind_reference_loan_shallow(
    const syntax::StmtId stmt_id) const
{
    if (!valid_stmt(this->core_.ctx_.module, stmt_id)) {
        return false;
    }
    const syntax::StmtNode& stmt = this->core_.ctx_.module.stmts[stmt_id.value];
    switch (stmt.kind) {
        case syntax::StmtKind::let:
        case syntax::StmtKind::var:
            return this->type_contains_reference(this->core_.cached_stmt_local_type(stmt_id))
                && this->type_contains_reference(this->core_.cached_expr_type(stmt.init));
        case syntax::StmtKind::assign:
            return this->type_contains_reference(this->core_.cached_expr_type(stmt.lhs))
                && this->type_contains_reference(this->core_.cached_expr_type(stmt.rhs));
        case syntax::StmtKind::if_:
        case syntax::StmtKind::for_:
        case syntax::StmtKind::for_range:
        case syntax::StmtKind::while_:
        case syntax::StmtKind::break_:
        case syntax::StmtKind::continue_:
        case syntax::StmtKind::defer:
        case syntax::StmtKind::return_:
        case syntax::StmtKind::expr:
        case syntax::StmtKind::block:
            return false;
    }
    return false;
}

bool SemanticAnalyzerCore::BodyLoanChecker::expr_may_need_local_loan_check(const syntax::ExprId expr) const
{
    std::vector<syntax::ExprId> pending_exprs;
    pending_exprs.reserve(SEMA_BODY_LOAN_PRECHECK_INITIAL_STACK_CAPACITY);
    push_precheck_expr(pending_exprs, this->core_.ctx_.module, expr);
    std::vector<syntax::StmtId> pending_stmts;
    pending_stmts.reserve(SEMA_BODY_LOAN_PRECHECK_INITIAL_STACK_CAPACITY);
    std::unordered_set<base::u32> visited_exprs;
    std::unordered_set<base::u32> visited_stmts;
    while (!pending_exprs.empty() || !pending_stmts.empty()) {
        if (!pending_stmts.empty()) {
            const syntax::StmtId stmt_id = pending_stmts.back();
            pending_stmts.pop_back();
            if (!valid_stmt(this->core_.ctx_.module, stmt_id) || !visited_stmts.insert(stmt_id.value).second) {
                continue;
            }
            if (this->statement_may_bind_reference_loan_shallow(stmt_id)) {
                return true;
            }
            push_precheck_statement_children(
                this->core_.ctx_.module, this->core_.ctx_.module.stmts[stmt_id.value], pending_exprs, pending_stmts);
            continue;
        }
        const syntax::ExprId current = pending_exprs.back();
        pending_exprs.pop_back();
        if (!valid_expr(this->core_.ctx_.module, current) || !visited_exprs.insert(current.value).second) {
            continue;
        }
        if (const FunctionCallBinding* const binding =
                this->core_.state_.checked.function_call_binding_for_expr(current);
            binding != nullptr && binding->receiver_two_phase_eligible) {
            return true;
        }
        if (const TraitMethodCallBinding* const binding =
                this->core_.state_.checked.trait_method_call_binding_for_expr(current);
            binding != nullptr && binding->receiver_two_phase_eligible) {
            return true;
        }
        push_precheck_expression_children(this->core_.ctx_.module, current, pending_exprs, pending_stmts);
    }
    return false;
}

bool SemanticAnalyzerCore::BodyLoanChecker::may_need_local_loan_check(const syntax::ItemNode& function) const
{
    std::vector<syntax::StmtId> pending;
    pending.reserve(SEMA_BODY_LOAN_PRECHECK_INITIAL_STACK_CAPACITY);
    pending.push_back(function.body);
    while (!pending.empty()) {
        const syntax::StmtId stmt_id = pending.back();
        pending.pop_back();
        if (!valid_stmt(this->core_.ctx_.module, stmt_id)) {
            continue;
        }
        if (this->statement_may_need_local_loan_check(stmt_id)) {
            return true;
        }
        const syntax::StmtNode& stmt = this->core_.ctx_.module.stmts[stmt_id.value];
        switch (stmt.kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                pending.push_back(stmt.else_block);
                break;
            case syntax::StmtKind::if_:
                pending.push_back(stmt.then_block);
                pending.push_back(stmt.else_block);
                pending.push_back(stmt.else_if);
                break;
            case syntax::StmtKind::for_:
                pending.push_back(stmt.for_init);
                pending.push_back(stmt.for_update);
                pending.push_back(stmt.body);
                break;
            case syntax::StmtKind::for_range:
            case syntax::StmtKind::while_:
                pending.push_back(stmt.body);
                break;
            case syntax::StmtKind::block:
                pending.insert(pending.end(), stmt.statements.begin(), stmt.statements.end());
                break;
            case syntax::StmtKind::assign:
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
            case syntax::StmtKind::defer:
            case syntax::StmtKind::return_:
            case syntax::StmtKind::expr:
                break;
        }
    }
    return false;
}

void SemanticAnalyzerCore::BodyLoanChecker::record_empty(
    const FunctionLookupKey& key, const BodyLoanDiagnosticMode mode)
{
    BodyLoanCheckResult result;
    result.function = key;
    result.diagnostic_mode = mode;
    this->core_.state_.checked.body_loan_checks[key] = std::move(result);
}

void SemanticAnalyzerCore::BodyLoanChecker::check(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const BodyLoanDiagnosticMode mode)
{
    const auto found = this->core_.state_.checked.body_flow_graphs.find(key);
    if (found == this->core_.state_.checked.body_flow_graphs.end()) {
        BodyLoanCheckResult result;
        result.function = key;
        result.diagnostic_mode = mode;
        result.graph_missing = true;
        this->core_.state_.checked.body_loan_checks[key] = std::move(result);
        return;
    }
    std::vector<bool> move_action_consumes(found->second.actions.size(), false);
    for (base::usize action_index = 0; action_index < found->second.actions.size(); ++action_index) {
        const BodyFlowAction& action = found->second.actions[action_index];
        move_action_consumes[action_index] = action.kind == BodyFlowActionKind::move_candidate
            && this->core_.cached_expr_owned_use_mode(action.expr) == OwnedUseMode::owned_consume;
    }
    BodyLoanCheckResult result = BodyLoanSolver(this->core_.ctx_.module, function, key, this->core_.state_.checked,
        found->second, std::move(move_action_consumes), mode)
                                     .run();
    if (mode == BodyLoanDiagnosticMode::enforced) {
        std::vector<BodyLoanDiagnosticSite> reported_sites;
        reported_sites.reserve(result.conflicts.size());
        for (BodyLoanConflict& conflict : result.conflicts) {
            if (!record_reported_diagnostic_site(reported_sites, conflict)) {
                continue;
            }
            if (conflict_is_two_phase(conflict.kind)) {
                if (conflict.two_phase_borrow >= result.two_phase_borrows.size()) {
                    continue;
                }
                BodyTwoPhaseBorrow& borrow = result.two_phase_borrows[conflict.two_phase_borrow];
                this->core_.report_general(conflict.range, std::string(SEMA_TWO_PHASE_RECEIVER_CONFLICT));
                this->core_.report_note(
                    borrow.range, SemanticDiagnosticKind::general, std::string(SEMA_TWO_PHASE_RECEIVER_RESERVED));
                if (valid_action(found->second, borrow.activation_action)) {
                    const BodyFlowAction& activation = found->second.actions[borrow.activation_action];
                    this->core_.report_note(activation.range, SemanticDiagnosticKind::general,
                        std::string(SEMA_TWO_PHASE_RECEIVER_ACTIVATED));
                }
                borrow.diagnostic_emitted = true;
                conflict.diagnostic_emitted = true;
                continue;
            }
            if (conflict.loan >= result.loans.size()) {
                continue;
            }
            const BodyLoan& loan = result.loans[conflict.loan];
            if (conflict.kind == BodyLoanConflictKind::reborrow_parent_use) {
                this->core_.report_general(conflict.range, std::string(SEMA_REBORROW_PARENT_USE_CONFLICT));
                this->core_.report_note(
                    loan.range, SemanticDiagnosticKind::general, std::string(SEMA_REBORROW_CHILD_CREATED));
                this->core_.report_note(conflict.range, SemanticDiagnosticKind::general,
                    body_loan_invalidating_action_message(conflict.kind));
                conflict.diagnostic_emitted = true;
                continue;
            }
            this->core_.report_general(conflict.range, std::string(SEMA_ACTIVE_BORROW_CONFLICT));
            this->core_.report_note(
                loan.range, SemanticDiagnosticKind::general, std::string(SEMA_ACTIVE_BORROW_CREATED));
            this->core_.report_note(
                conflict.range, SemanticDiagnosticKind::general, body_loan_invalidating_action_message(conflict.kind));
            if (valid_point(found->second, conflict.later_use_point)) {
                this->core_.report_note(conflict.later_use_range, SemanticDiagnosticKind::general,
                    std::string(SEMA_ACTIVE_BORROW_LATER_CARRIER_USE));
            }
            conflict.diagnostic_emitted = true;
        }
    }
    this->core_.state_.checked.body_loan_checks[key] = std::move(result);
}

void SemanticAnalyzerCore::check_body_loans(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const BodyLoanDiagnosticMode mode)
{
    BodyLoanChecker(*this).check(function, key, mode);
}

} // namespace aurex::sema
