#include <aurex/base/integer.hpp>
#include <aurex/query/stable_hash.hpp>
#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <sema/internal/sema_body_loan_checker.hpp>
#include <sema/internal/sema_diagnostics.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_BODY_LOAN_ID_CONTEXT = "sema body loan id";
constexpr std::string_view SEMA_BODY_LOAN_FINGERPRINT_MARKER = "sema.body_loan_check.v1";
constexpr base::usize SEMA_BODY_LOAN_INITIAL_WORKLIST_CAPACITY = 32;
constexpr base::usize SEMA_BODY_LOAN_PRECHECK_INITIAL_STACK_CAPACITY = 64;

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

[[nodiscard]] bool action_reads_carrier(const BodyFlowAction& action, const IdentId carrier_name_id) noexcept
{
    return syntax::is_valid(carrier_name_id) && action.kind == BodyFlowActionKind::read
        && syntax::is_valid(action.expr);
}

[[nodiscard]] bool reborrow_parent_access_conflicts(
    const BodyLoanKind parent_kind, const BodyLoanKind child_kind, const BodyLoanAccessKind access) noexcept
{
    if (parent_kind == BodyLoanKind::mutable_ || child_kind == BodyLoanKind::mutable_) {
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
        const BodyFlowGraph& graph, std::vector<bool> move_action_consumes, const BodyLoanDiagnosticMode mode)
        : module_(module), function_(function), graph_(graph), move_action_consumes_(std::move(move_action_consumes))
    {
        this->result_.function = key;
        this->result_.diagnostic_mode = mode;
    }

    [[nodiscard]] BodyLoanCheckResult run()
    {
        this->build_graph_index();
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
        for (base::usize action_index = 0; action_index < this->graph_.actions.size(); ++action_index) {
            const BodyFlowAction& action = this->graph_.actions[action_index];
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

    void collect_loans()
    {
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
            this->result_.loans.push_back(std::move(loan));
        }
    }

    void bind_reborrow_parents()
    {
        this->effective_loan_places_.clear();
        this->effective_loan_places_.reserve(this->result_.loans.size());
        for (base::usize loan_index = 0; loan_index < this->result_.loans.size(); ++loan_index) {
            BodyLoan& loan = this->result_.loans[loan_index];
            this->bind_reborrow_parent_for_loan(loan_index, loan);
        }
        for (base::usize loan_index = 0; loan_index < this->result_.loans.size(); ++loan_index) {
            this->effective_loan_places_.push_back(this->make_effective_loan_place(loan_index));
        }
    }

    void bind_reborrow_parent_for_loan(const base::usize loan_index, BodyLoan& loan) const
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
        base::u32 parent_loan = SEMA_BODY_LOAN_INVALID_INDEX;
        base::u32 parent_action = 0;
        for (base::usize candidate_index = 0; candidate_index < this->result_.loans.size(); ++candidate_index) {
            const BodyLoan& candidate = this->result_.loans[candidate_index];
            if (candidate_index == loan_index || candidate.issued_action >= loan.issued_action
                || candidate.carrier_name_id != carrier) {
                continue;
            }
            if (parent_loan == SEMA_BODY_LOAN_INVALID_INDEX || candidate.issued_action > parent_action) {
                parent_loan = base::checked_u32(candidate_index, SEMA_BODY_LOAN_ID_CONTEXT);
                parent_action = candidate.issued_action;
            }
        }
        loan.parent_loan = parent_loan;
    }

    [[nodiscard]] BodyFlowPlace make_effective_loan_place(const base::usize loan_index) const
    {
        const BodyLoan& loan = this->result_.loans[loan_index];
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
        std::vector<bool> used_activation(this->graph_.actions.size(), false);
        for (base::usize reservation_index = 0; reservation_index < this->graph_.actions.size(); ++reservation_index) {
            const BodyFlowAction& reservation = this->graph_.actions[reservation_index];
            if (reservation.kind != BodyFlowActionKind::call_receiver_reserve
                || !valid_place(this->graph_, reservation.place)) {
                continue;
            }
            const std::optional<BodyLoanActionSite> activation =
                this->find_two_phase_activation(reservation, used_activation);
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
            used_activation[activation->action] = true;
        }
    }

    [[nodiscard]] std::optional<BodyLoanActionSite> find_two_phase_activation(
        const BodyFlowAction& reservation, const std::vector<bool>& used_activation) const
    {
        for (base::usize action_index = 0; action_index < this->graph_.actions.size(); ++action_index) {
            if (action_index >= used_activation.size() || used_activation[action_index]) {
                continue;
            }
            const BodyFlowAction& action = this->graph_.actions[action_index];
            if (action.kind == BodyFlowActionKind::call_receiver_activate && action.expr.value == reservation.expr.value
                && action.place == reservation.place) {
                return BodyLoanActionSite{
                    .action = base::checked_u32(action_index, SEMA_BODY_LOAN_ID_CONTEXT),
                    .point = action.point,
                    .range = action.range,
                };
            }
        }
        return std::nullopt;
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
        std::vector<syntax::StmtId> pending;
        pending.push_back(this->function_.body);
        while (!pending.empty()) {
            const syntax::StmtId stmt = pending.back();
            pending.pop_back();
            if (!valid_stmt(this->module_, stmt)) {
                continue;
            }
            const syntax::StmtNode node = this->module_.stmts[stmt.value];
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
            && syntax::is_valid(local_init_borrow) && local_init_borrow.value == loan.expr.value) {
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
        return false;
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

    [[nodiscard]] base::u32 find_carrier_definition_point(
        const IdentId carrier_name_id, const syntax::StmtId stmt) const noexcept
    {
        for (const BodyFlowAction& action : this->graph_.actions) {
            if (action_is_storage_definition(action.kind) && action.stmt.value == stmt.value
                && this->action_place_has_root(action, carrier_name_id)) {
                return action.point;
            }
        }
        return SEMA_BODY_FLOW_INVALID_INDEX;
    }

    void compute_carrier_liveness()
    {
        this->carrier_live_after_.assign(
            this->result_.loans.size(), std::vector<bool>(this->graph_.points.size(), false));
        for (base::usize loan_index = 0; loan_index < this->result_.loans.size(); ++loan_index) {
            const BodyLoan& loan = this->result_.loans[loan_index];
            if (!syntax::is_valid(loan.carrier_name_id)) {
                continue;
            }
            std::vector<bool> uses(this->graph_.points.size(), false);
            std::vector<bool> definitions(this->graph_.points.size(), false);
            for (base::usize action_index = 0; action_index < this->graph_.actions.size(); ++action_index) {
                const BodyFlowAction& action = this->graph_.actions[action_index];
                if (!valid_point(this->graph_, action.point)
                    || !this->action_place_has_root(action, loan.carrier_name_id)) {
                    continue;
                }
                if (action_reads_carrier(action, loan.carrier_name_id)) {
                    uses[action.point] = true;
                }
                if (action_is_storage_definition(action.kind) && action.point != loan.carrier_definition_point) {
                    definitions[action.point] = true;
                }
            }
            this->solve_live_after_for_loan(loan_index, uses, definitions);
        }
    }

    [[nodiscard]] bool action_place_has_root(const BodyFlowAction& action, const IdentId name_id) const noexcept
    {
        if (!valid_place(this->graph_, action.place)) {
            return false;
        }
        const BodyFlowPlace& place = this->graph_.places[action.place];
        return place.root_kind == BodyFlowPlaceRootKind::local && place.root_name_id == name_id;
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
        for (base::usize loan_index = 0; loan_index < this->result_.loans.size(); ++loan_index) {
            if (this->result_.loans[loan_index].issued_action != action_index) {
                continue;
            }
            active.push_back(base::checked_u32(loan_index, SEMA_BODY_LOAN_ID_CONTEXT));
            return;
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
            if (action_reads_carrier(candidate, loan.carrier_name_id)) {
                return BodyLoanCarrierUseSite{
                    .point = candidate.point,
                    .range = candidate.range,
                };
            }
            if (action_is_storage_definition(candidate.kind) && candidate.point != loan.carrier_definition_point) {
                carrier_redefined = true;
                return std::nullopt;
            }
        }
        return std::nullopt;
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
    const BodyFlowGraph& graph_;
    std::vector<bool> move_action_consumes_;
    BodyLoanCheckResult result_;
    BodyLoanActionIndex index_;
    std::vector<std::vector<bool>> carrier_live_after_;
    std::vector<std::vector<base::u32>> active_in_;
    std::vector<std::vector<base::u32>> active_out_;
    std::vector<BodyFlowPlace> effective_loan_places_;
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
            case TypeKind::reference:
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
            case TypeKind::builtin:
            case TypeKind::pointer:
            case TypeKind::slice:
            case TypeKind::function:
            case TypeKind::opaque_struct:
            case TypeKind::generic_param:
            case TypeKind::associated_projection:
                break;
        }
    }
    return false;
}

bool SemanticAnalyzerCore::BodyLoanChecker::statement_may_bind_reference_loan(const syntax::StmtId stmt_id) const
{
    if (!valid_stmt(this->core_.ctx_.module, stmt_id)) {
        return false;
    }
    const syntax::StmtNode stmt = this->core_.ctx_.module.stmts[stmt_id.value];
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

bool SemanticAnalyzerCore::BodyLoanChecker::statement_may_have_two_phase_receiver(const syntax::StmtId stmt_id) const
{
    if (!valid_stmt(this->core_.ctx_.module, stmt_id)) {
        return false;
    }
    const syntax::StmtNode stmt = this->core_.ctx_.module.stmts[stmt_id.value];
    switch (stmt.kind) {
        case syntax::StmtKind::let:
        case syntax::StmtKind::var:
            return this->expr_may_have_two_phase_receiver(stmt.init);
        case syntax::StmtKind::assign:
            return this->expr_may_have_two_phase_receiver(stmt.lhs) || this->expr_may_have_two_phase_receiver(stmt.rhs);
        case syntax::StmtKind::if_:
        case syntax::StmtKind::while_:
            return this->expr_may_have_two_phase_receiver(stmt.condition);
        case syntax::StmtKind::for_:
            return this->expr_may_have_two_phase_receiver(stmt.condition);
        case syntax::StmtKind::for_range:
            return this->expr_may_have_two_phase_receiver(stmt.range_start)
                || this->expr_may_have_two_phase_receiver(stmt.range_end)
                || this->expr_may_have_two_phase_receiver(stmt.range_step);
        case syntax::StmtKind::defer:
        case syntax::StmtKind::expr:
            return this->expr_may_have_two_phase_receiver(stmt.init);
        case syntax::StmtKind::return_:
            return this->expr_may_have_two_phase_receiver(stmt.return_value);
        case syntax::StmtKind::break_:
        case syntax::StmtKind::continue_:
        case syntax::StmtKind::block:
            return false;
    }
    return false;
}

bool SemanticAnalyzerCore::BodyLoanChecker::expr_may_have_two_phase_receiver(const syntax::ExprId expr) const
{
    std::vector<syntax::ExprId> pending;
    pending.push_back(expr);
    std::unordered_set<base::u32> visited;
    while (!pending.empty()) {
        const syntax::ExprId current = pending.back();
        pending.pop_back();
        if (!valid_expr(this->core_.ctx_.module, current) || !visited.insert(current.value).second) {
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
        switch (this->core_.ctx_.module.exprs.kind(current.value)) {
            case syntax::ExprKind::generic_apply: {
                const syntax::GenericApplyExprPayload* const apply =
                    this->core_.ctx_.module.exprs.generic_apply_payload(current.value);
                if (apply != nullptr) {
                    pending.push_back(apply->callee);
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
                const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(current.value);
                const syntax::UnaryExprPayload* const unary =
                    this->core_.ctx_.module.exprs.unary_payload(current.value);
                const syntax::TryExprPayload* const try_expr = this->core_.ctx_.module.exprs.try_payload(current.value);
                if (cast != nullptr) {
                    pending.push_back(cast->expr);
                } else if (unary != nullptr) {
                    pending.push_back(unary->operand);
                } else if (try_expr != nullptr) {
                    pending.push_back(try_expr->operand);
                }
                break;
            }
            case syntax::ExprKind::binary: {
                const syntax::BinaryExprPayload* const binary =
                    this->core_.ctx_.module.exprs.binary_payload(current.value);
                if (binary != nullptr) {
                    pending.push_back(binary->lhs);
                    pending.push_back(binary->rhs);
                }
                break;
            }
            case syntax::ExprKind::call:
            case syntax::ExprKind::str_from_bytes_unchecked: {
                const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
                if (call != nullptr) {
                    pending.push_back(call->callee);
                    pending.insert(pending.end(), call->args.begin(), call->args.end());
                }
                break;
            }
            case syntax::ExprKind::field: {
                const syntax::FieldExprPayload* const field =
                    this->core_.ctx_.module.exprs.field_payload(current.value);
                if (field != nullptr) {
                    pending.push_back(field->object);
                }
                break;
            }
            case syntax::ExprKind::index: {
                const syntax::IndexExprPayload* const index =
                    this->core_.ctx_.module.exprs.index_payload(current.value);
                if (index != nullptr) {
                    pending.push_back(index->object);
                    pending.push_back(index->index);
                }
                break;
            }
            case syntax::ExprKind::slice: {
                const syntax::SliceExprPayload* const slice =
                    this->core_.ctx_.module.exprs.slice_payload(current.value);
                if (slice != nullptr) {
                    pending.push_back(slice->object);
                    pending.push_back(slice->start);
                    pending.push_back(slice->end);
                }
                break;
            }
            case syntax::ExprKind::if_expr: {
                const syntax::IfExprPayload* const if_expr = this->core_.ctx_.module.exprs.if_payload(current.value);
                if (if_expr != nullptr) {
                    pending.push_back(if_expr->condition);
                    pending.push_back(if_expr->then_expr);
                    pending.push_back(if_expr->else_expr);
                }
                break;
            }
            case syntax::ExprKind::array_literal: {
                const syntax::ArrayExprPayload* const array =
                    this->core_.ctx_.module.exprs.array_payload(current.value);
                if (array != nullptr) {
                    pending.insert(pending.end(), array->elements.begin(), array->elements.end());
                    pending.push_back(array->repeat_value);
                    pending.push_back(array->repeat_count);
                }
                break;
            }
            case syntax::ExprKind::tuple_literal: {
                const syntax::AstArenaVector<syntax::ExprId>* const elements =
                    this->core_.ctx_.module.exprs.tuple_elements(current.value);
                if (elements != nullptr) {
                    pending.insert(pending.end(), elements->begin(), elements->end());
                }
                break;
            }
            case syntax::ExprKind::struct_literal: {
                const syntax::StructLiteralExprPayload* const literal =
                    this->core_.ctx_.module.exprs.struct_literal_payload(current.value);
                if (literal != nullptr) {
                    pending.push_back(literal->object);
                    for (const syntax::FieldInit& init : literal->field_inits) {
                        pending.push_back(init.value);
                    }
                }
                break;
            }
            case syntax::ExprKind::match_expr: {
                const syntax::MatchExprPayload* const match =
                    this->core_.ctx_.module.exprs.match_payload(current.value);
                if (match != nullptr) {
                    pending.push_back(match->value);
                    for (const syntax::MatchArm& arm : match->arms) {
                        pending.push_back(arm.guard);
                        pending.push_back(arm.value);
                    }
                }
                break;
            }
            case syntax::ExprKind::block_expr:
            case syntax::ExprKind::unsafe_block: {
                const syntax::BlockExprPayload* const block =
                    this->core_.ctx_.module.exprs.block_payload(current.value);
                if (block != nullptr) {
                    pending.push_back(block->result);
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
        if (this->statement_may_bind_reference_loan(stmt_id) || this->statement_may_have_two_phase_receiver(stmt_id)) {
            return true;
        }
        const syntax::StmtNode stmt = this->core_.ctx_.module.stmts[stmt_id.value];
        switch (stmt.kind) {
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
    BodyLoanCheckResult result =
        BodyLoanSolver(this->core_.ctx_.module, function, key, found->second, std::move(move_action_consumes), mode)
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
