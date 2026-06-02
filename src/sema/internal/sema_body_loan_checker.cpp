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

[[nodiscard]] bool same_source_range(const base::SourceRange& lhs, const base::SourceRange& rhs) noexcept
{
    return lhs.source.value == rhs.source.value && lhs.begin == rhs.begin && lhs.end == rhs.end;
}

[[nodiscard]] bool same_diagnostic_site(const BodyLoanDiagnosticSite& site, const BodyLoanConflict& conflict) noexcept
{
    return site.point == conflict.point && same_source_range(site.range, conflict.range);
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

[[nodiscard]] std::string body_loan_invalidating_action_message(const BodyLoanConflictKind kind)
{
    std::string message(SEMA_ACTIVE_BORROW_INVALIDATING_ACTION);
    message.append(" (");
    message.append(body_loan_conflict_kind_name(kind));
    message.push_back(')');
    return message;
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
        this->compute_carrier_liveness();
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
            if (loan.issued_action == action_index || !loan_conflicts_with_access(loan.kind, access)
                || !valid_place(this->graph_, loan.place)) {
                continue;
            }
            const BodyFlowPlace& loan_place = this->graph_.places[loan.place];
            const BodyFlowPlace& action_place = this->graph_.places[action.place];
            if (!places_conflict(loan_place, action_place)) {
                continue;
            }
            this->record_conflict(loan_index, action_index, action, access);
        }
    }

    void record_conflict(const base::u32 loan_index, const base::u32 action_index, const BodyFlowAction& action,
        const BodyLoanAccessKind access)
    {
        constexpr std::uint64_t SEMA_BODY_LOAN_CONFLICT_KEY_SHIFT = 32;
        const std::uint64_t key =
            (static_cast<std::uint64_t>(loan_index) << SEMA_BODY_LOAN_CONFLICT_KEY_SHIFT) | action_index;
        if (!this->seen_conflicts_.insert(key).second) {
            return;
        }
        const BodyLoanCarrierUseSite later_use = this->find_later_carrier_use(loan_index, action_index, action.point);
        this->result_.conflicts.push_back(BodyLoanConflict{
            .kind = conflict_kind_for_access(access),
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
    std::unordered_set<std::uint64_t> seen_conflicts_;
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
        builder.mix_u32(loan.issued_action);
        builder.mix_u32(loan.issued_point);
        builder.mix_u32(loan.place);
        builder.mix_u32(loan.carrier_name_id.value);
        builder.mix_u32(loan.carrier_definition_point);
        builder.mix_u32(loan.enclosing_stmt.value);
        builder.mix_u32(loan.expr.value);
    }

    builder.mix_u64(result.conflicts.size());
    for (const BodyLoanConflict& conflict : result.conflicts) {
        builder.mix_u8(static_cast<base::u8>(conflict.kind));
        builder.mix_u32(conflict.loan);
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
        stream << "  l" << index << ' ' << body_loan_kind_name(loan.kind) << " place=" << loan.place
               << " issued_action=a" << loan.issued_action << " issued_point=p" << loan.issued_point << " carrier=";
        append_optional_name_id(stream, loan.carrier_name_id);
        stream << " stmt=";
        append_optional_stmt_id(stream, loan.enclosing_stmt);
        stream << " range=";
        append_range(stream, loan.range);
        stream << '\n';
    }

    stream << "conflicts:\n";
    for (base::usize index = 0; index < result.conflicts.size(); ++index) {
        const BodyLoanConflict& conflict = result.conflicts[index];
        stream << "  c" << index << ' ' << body_loan_conflict_kind_name(conflict.kind) << " loan=l" << conflict.loan
               << " action=a" << conflict.action << " point=p" << conflict.point << " place=" << conflict.place
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
        if (this->statement_may_bind_reference_loan(stmt_id)) {
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
            if (conflict.loan >= result.loans.size() || !record_reported_diagnostic_site(reported_sites, conflict)) {
                continue;
            }
            const BodyLoan& loan = result.loans[conflict.loan];
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
