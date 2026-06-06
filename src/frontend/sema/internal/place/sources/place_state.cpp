#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <frontend/sema/internal/place/private/place_state.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_PLACE_STATE_ID_CONTEXT = "sema place state id";
constexpr std::string_view SEMA_PLACE_STATE_FACTS_FINGERPRINT_MARKER = "sema.place_state.facts.v3";
constexpr base::usize SEMA_PLACE_STATE_STATEMENT_STACK_INITIAL_CAPACITY = 32;
constexpr base::usize SEMA_PLACE_STATE_PATTERN_STACK_INITIAL_CAPACITY = 16;
constexpr base::usize SEMA_PLACE_STATE_PRECHECK_STACK_INITIAL_CAPACITY = 64;
constexpr base::usize SEMA_PLACE_STATE_WORKLIST_INITIAL_CAPACITY = 32;
constexpr std::uint64_t SEMA_PLACE_STATE_VIOLATION_KEY_SHIFT = 32;

enum class PlaceStateAccessKind : base::u8 {
    read,
    write,
    reinit,
    move,
    drop,
    cleanup,
    borrow,
};

struct PlaceStateProjectionKey {
    BodyFlowPlaceProjectionKind kind = BodyFlowPlaceProjectionKind::field;
    base::u32 field_name = INVALID_IDENT_ID.value;
    base::u32 element_index = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 expr = syntax::INVALID_EXPR_ID.value;

    [[nodiscard]] friend bool operator==(
        const PlaceStateProjectionKey& lhs, const PlaceStateProjectionKey& rhs) noexcept = default;
};

struct PlaceStatePlaceKey {
    BodyFlowPlaceRootKind root_kind = BodyFlowPlaceRootKind::none;
    base::u32 root_name = INVALID_IDENT_ID.value;
    base::u32 root_expr = syntax::INVALID_EXPR_ID.value;
    std::vector<PlaceStateProjectionKey> projections;

    [[nodiscard]] friend bool operator==(
        const PlaceStatePlaceKey& lhs, const PlaceStatePlaceKey& rhs) noexcept = default;
};

struct PlaceStatePlaceKeyHash {
    [[nodiscard]] std::size_t operator()(const PlaceStatePlaceKey& key) const noexcept
    {
        query::StableHashBuilder builder;
        builder.mix_u8(static_cast<base::u8>(key.root_kind));
        builder.mix_u32(key.root_name);
        builder.mix_u32(key.root_expr);
        builder.mix_u64(static_cast<base::u64>(key.projections.size()));
        for (const PlaceStateProjectionKey& projection : key.projections) {
            builder.mix_u8(static_cast<base::u8>(projection.kind));
            builder.mix_u32(projection.field_name);
            builder.mix_u32(projection.element_index);
            builder.mix_u32(projection.expr);
        }
        return query::stable_hash_value(builder.finish());
    }
};

struct PlaceStateActionIndex {
    std::vector<std::vector<base::u32>> actions_by_point;
    std::vector<std::vector<base::u32>> outgoing_points;
};

struct PlaceRuntimeState {
    PlaceStateInitialization initialization = PlaceStateInitialization::unknown;
    PlaceStateMoveState move_state = PlaceStateMoveState::none;
    PlaceStateDropState drop_state = PlaceStateDropState::none;
    base::u32 last_move_action = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 last_partial_move_action = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 last_drop_action = SEMA_BODY_FLOW_INVALID_INDEX;
    bool partially_moved = false;
    bool drop_flag_live = false;
};

[[nodiscard]] std::optional<syntax::StmtNode> statement_node(
    const syntax::AstModule& module, const syntax::StmtId stmt)
{
    if (!syntax::is_valid(stmt) || stmt.value >= module.stmts.size()) {
        return std::nullopt;
    }
    return module.stmts[stmt.value];
}

[[nodiscard]] const syntax::PatternNode* pattern_node(
    const syntax::AstModule& module, const syntax::PatternId pattern) noexcept
{
    if (!syntax::is_valid(pattern) || pattern.value >= module.patterns.size()) {
        return nullptr;
    }
    return module.patterns.ptr(pattern.value);
}

[[nodiscard]] bool valid_expr(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size();
}

[[nodiscard]] bool valid_stmt(const syntax::AstModule& module, const syntax::StmtId stmt) noexcept
{
    return syntax::is_valid(stmt) && stmt.value < module.stmts.size();
}

[[nodiscard]] bool valid_point(const BodyFlowGraph& graph, const base::u32 point) noexcept
{
    return point != SEMA_BODY_FLOW_INVALID_INDEX && point < graph.points.size();
}

[[nodiscard]] bool valid_action(const BodyFlowGraph& graph, const base::u32 action) noexcept
{
    return action != SEMA_BODY_FLOW_INVALID_INDEX && action < graph.actions.size();
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

[[nodiscard]] const StructFieldInfo* find_struct_field(const StructInfo& structure, const IdentId field_name) noexcept
{
    for (const StructFieldInfo& field : structure.fields) {
        if (field.name_id == field_name) {
            return &field;
        }
    }
    return nullptr;
}

[[nodiscard]] PlaceStateProjectionKey projection_key(const BodyFlowPlaceProjection& projection) noexcept
{
    PlaceStateProjectionKey key{
        .kind = projection.kind,
    };
    switch (projection.kind) {
        case BodyFlowPlaceProjectionKind::field:
            key.field_name = projection.field_name_id.value;
            break;
        case BodyFlowPlaceProjectionKind::tuple_element:
            key.element_index = projection.element_index;
            break;
        case BodyFlowPlaceProjectionKind::index:
        case BodyFlowPlaceProjectionKind::slice:
        case BodyFlowPlaceProjectionKind::dereference:
            break;
    }
    return key;
}

[[nodiscard]] base::u32 normalized_root_expr(const BodyFlowPlace& place) noexcept
{
    return place.root_kind == BodyFlowPlaceRootKind::temporary ? place.root_expr.value : syntax::INVALID_EXPR_ID.value;
}

[[nodiscard]] syntax::ExprId normalized_fact_root_expr(const BodyFlowPlace& place) noexcept
{
    return place.root_kind == BodyFlowPlaceRootKind::temporary ? place.root_expr : syntax::INVALID_EXPR_ID;
}

[[nodiscard]] PlaceStatePlaceKey place_key_for_prefix(
    const BodyFlowPlace& place, const base::usize projection_count)
{
    PlaceStatePlaceKey key;
    key.root_kind = place.root_kind;
    key.root_name = place.root_name_id.value;
    key.root_expr = normalized_root_expr(place);
    key.projections.reserve(projection_count);
    for (base::usize index = 0; index < projection_count && index < place.projections.size(); ++index) {
        key.projections.push_back(projection_key(place.projections[index]));
    }
    return key;
}

[[nodiscard]] BodyFlowPlace place_prefix(const BodyFlowPlace& place, const base::usize projection_count)
{
    BodyFlowPlace prefix;
    prefix.root_kind = place.root_kind;
    prefix.root_name_id = place.root_name_id;
    prefix.root_expr = place.root_expr;
    prefix.range = place.range;
    prefix.projections.reserve(projection_count);
    for (base::usize index = 0; index < projection_count && index < place.projections.size(); ++index) {
        prefix.projections.push_back(place.projections[index]);
    }
    return prefix;
}

[[nodiscard]] std::optional<PlaceStateAccessKind> access_kind_for_event(const PlaceStateEventKind kind) noexcept
{
    switch (kind) {
        case PlaceStateEventKind::read:
            return PlaceStateAccessKind::read;
        case PlaceStateEventKind::write:
            return PlaceStateAccessKind::write;
        case PlaceStateEventKind::reinit:
            return PlaceStateAccessKind::reinit;
        case PlaceStateEventKind::move_candidate:
            return PlaceStateAccessKind::move;
        case PlaceStateEventKind::drop:
            return PlaceStateAccessKind::drop;
        case PlaceStateEventKind::cleanup:
            return PlaceStateAccessKind::cleanup;
        case PlaceStateEventKind::borrow_shared:
        case PlaceStateEventKind::borrow_mutable:
            return PlaceStateAccessKind::borrow;
    }
    return std::nullopt;
}

[[nodiscard]] bool access_requires_initialized_place(const PlaceStateAccessKind access) noexcept
{
    switch (access) {
        case PlaceStateAccessKind::read:
        case PlaceStateAccessKind::move:
        case PlaceStateAccessKind::drop:
        case PlaceStateAccessKind::borrow:
            return true;
        case PlaceStateAccessKind::write:
        case PlaceStateAccessKind::reinit:
        case PlaceStateAccessKind::cleanup:
            return false;
    }
    return false;
}

void mix_function_key(query::StableHashBuilder& builder, const FunctionLookupKey key) noexcept
{
    builder.mix_u32(key.module);
    builder.mix_u32(key.owner_type);
    builder.mix_u32(key.name.value);
}

void mix_place_fact(query::StableHashBuilder& builder, const PlaceStateFact& fact) noexcept
{
    builder.mix_u32(fact.place);
    builder.mix_u8(static_cast<base::u8>(fact.root_kind));
    builder.mix_u32(fact.root_name_id.value);
    builder.mix_u32(fact.root_expr.value);
    builder.mix_u32(fact.type.value);
    builder.mix_u64(fact.projection_count);
    builder.mix_u32(fact.first_action_point);
    builder.mix_u32(fact.last_action_point);
    builder.mix_u32(fact.last_write_point);
    builder.mix_u32(fact.last_move_point);
    builder.mix_u32(fact.last_drop_point);
    builder.mix_u64(fact.read_count);
    builder.mix_u64(fact.write_count);
    builder.mix_u64(fact.reinit_count);
    builder.mix_u64(fact.move_candidate_count);
    builder.mix_u64(fact.drop_count);
    builder.mix_u64(fact.cleanup_count);
    builder.mix_u64(fact.borrow_count);
    builder.mix_u64(fact.partial_move_count);
    builder.mix_u64(fact.skipped_drop_count);
    builder.mix_u8(static_cast<base::u8>(fact.initialization));
    builder.mix_u8(static_cast<base::u8>(fact.move_state));
    builder.mix_u8(static_cast<base::u8>(fact.drop_state));
    builder.mix_u32(fact.last_partial_move_point);
    builder.mix_u32(fact.last_reinit_point);
    builder.mix_bool(fact.needs_drop);
    builder.mix_bool(fact.has_partial_projection);
    builder.mix_bool(fact.is_partially_moved);
    builder.mix_bool(fact.drop_flag_live);
}

void mix_place_event(query::StableHashBuilder& builder, const PlaceStateEvent& event) noexcept
{
    builder.mix_u8(static_cast<base::u8>(event.kind));
    builder.mix_u32(event.place);
    builder.mix_u32(event.action);
    builder.mix_u32(event.point);
    builder.mix_u32(event.type.value);
}

void mix_place_violation(query::StableHashBuilder& builder, const PlaceStateViolation& violation) noexcept
{
    builder.mix_u8(static_cast<base::u8>(violation.kind));
    builder.mix_u32(violation.place);
    builder.mix_u32(violation.action);
    builder.mix_u32(violation.point);
    builder.mix_u32(violation.related_place);
    builder.mix_u32(violation.related_action);
    builder.mix_bool(violation.diagnostic_emitted);
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

void append_range(std::ostringstream& stream, const base::SourceRange& range)
{
    stream << range.source.value << ':' << range.begin << ".." << range.end;
}

} // namespace

std::string_view place_state_initialization_name(const PlaceStateInitialization state) noexcept
{
    switch (state) {
        case PlaceStateInitialization::unknown:
            return "unknown";
        case PlaceStateInitialization::initialized:
            return "initialized";
        case PlaceStateInitialization::maybe_initialized:
            return "maybe_initialized";
        case PlaceStateInitialization::uninitialized:
            return "uninitialized";
    }
    return "<invalid>";
}

std::string_view place_state_move_state_name(const PlaceStateMoveState state) noexcept
{
    switch (state) {
        case PlaceStateMoveState::none:
            return "none";
        case PlaceStateMoveState::move_candidate:
            return "move_candidate";
        case PlaceStateMoveState::maybe_moved:
            return "maybe_moved";
    }
    return "<invalid>";
}

std::string_view place_state_drop_state_name(const PlaceStateDropState state) noexcept
{
    switch (state) {
        case PlaceStateDropState::none:
            return "none";
        case PlaceStateDropState::drop_pending:
            return "drop_pending";
        case PlaceStateDropState::dropped:
            return "dropped";
    }
    return "<invalid>";
}

std::string_view place_state_event_kind_name(const PlaceStateEventKind kind) noexcept
{
    switch (kind) {
        case PlaceStateEventKind::read:
            return "read";
        case PlaceStateEventKind::write:
            return "write";
        case PlaceStateEventKind::reinit:
            return "reinit";
        case PlaceStateEventKind::move_candidate:
            return "move_candidate";
        case PlaceStateEventKind::drop:
            return "drop";
        case PlaceStateEventKind::cleanup:
            return "cleanup";
        case PlaceStateEventKind::borrow_shared:
            return "borrow_shared";
        case PlaceStateEventKind::borrow_mutable:
            return "borrow_mutable";
    }
    return "<invalid>";
}

std::string_view place_state_violation_kind_name(const PlaceStateViolationKind kind) noexcept
{
    switch (kind) {
        case PlaceStateViolationKind::use_after_move:
            return "use_after_move";
        case PlaceStateViolationKind::maybe_uninitialized_use:
            return "maybe_uninitialized_use";
        case PlaceStateViolationKind::use_after_partial_move:
            return "use_after_partial_move";
        case PlaceStateViolationKind::drop_after_move:
            return "drop_after_move";
        case PlaceStateViolationKind::drop_after_partial_move:
            return "drop_after_partial_move";
        case PlaceStateViolationKind::double_drop:
            return "double_drop";
    }
    return "<invalid>";
}

std::string_view place_state_violation_message(const PlaceStateViolationKind kind) noexcept
{
    switch (kind) {
        case PlaceStateViolationKind::use_after_move:
            return SEMA_PLACE_STATE_USE_AFTER_MOVE;
        case PlaceStateViolationKind::maybe_uninitialized_use:
            return SEMA_PLACE_STATE_MAYBE_UNINITIALIZED_USE;
        case PlaceStateViolationKind::use_after_partial_move:
            return SEMA_PLACE_STATE_USE_AFTER_PARTIAL_MOVE;
        case PlaceStateViolationKind::drop_after_move:
            return SEMA_PLACE_STATE_DROP_AFTER_MOVE;
        case PlaceStateViolationKind::drop_after_partial_move:
            return SEMA_PLACE_STATE_DROP_AFTER_PARTIAL_MOVE;
        case PlaceStateViolationKind::double_drop:
            return SEMA_PLACE_STATE_DOUBLE_DROP;
    }
    return SEMA_PLACE_STATE_USE_AFTER_MOVE;
}

query::StableFingerprint128 function_place_state_facts_fingerprint(const FunctionPlaceStateFacts& facts) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_PLACE_STATE_FACTS_FINGERPRINT_MARKER);
    mix_function_key(builder, facts.function);
    builder.mix_bool(facts.graph_missing);
    builder.mix_bool(facts.solved);
    builder.mix_u32(facts.part_index);
    builder.mix_u64(static_cast<base::u64>(facts.places.size()));
    for (const PlaceStateFact& fact : facts.places) {
        mix_place_fact(builder, fact);
    }
    builder.mix_u64(static_cast<base::u64>(facts.events.size()));
    for (const PlaceStateEvent& event : facts.events) {
        mix_place_event(builder, event);
    }
    builder.mix_u64(static_cast<base::u64>(facts.violations.size()));
    for (const PlaceStateViolation& violation : facts.violations) {
        mix_place_violation(builder, violation);
    }
    builder.mix_bool(facts.diagnostic_mode_enforced);
    return builder.finish();
}

std::string dump_function_place_state_facts(const FunctionPlaceStateFacts& facts)
{
    std::ostringstream stream;
    stream << "place_state_facts function=" << facts.function.module << ':' << facts.function.owner_type << ':';
    append_optional_name_id(stream, facts.function.name);
    stream << " places=" << facts.places.size() << " events=" << facts.events.size()
           << " violations=" << facts.violations.size()
           << " solved=" << (facts.solved ? "true" : "false")
           << " enforced=" << (facts.diagnostic_mode_enforced ? "true" : "false")
           << " graph_missing=" << (facts.graph_missing ? "true" : "false")
           << " fingerprint=" << query::debug_string(function_place_state_facts_fingerprint(facts)) << '\n';

    stream << "places:\n";
    for (base::usize index = 0; index < facts.places.size(); ++index) {
        const PlaceStateFact& fact = facts.places[index];
        stream << "  p" << index << " place=" << fact.place
               << " root=" << body_flow_place_root_kind_name(fact.root_kind) << " name=";
        append_optional_name_id(stream, fact.root_name_id);
        stream << " root_expr=";
        append_optional_expr_id(stream, fact.root_expr);
        stream << " type=" << fact.type.value << " projections=" << fact.projection_count
               << " first=" << fact.first_action_point << " last=" << fact.last_action_point
               << " last_write=" << fact.last_write_point << " last_move=" << fact.last_move_point
               << " last_drop=" << fact.last_drop_point << " reads=" << fact.read_count
               << " writes=" << fact.write_count << " reinits=" << fact.reinit_count
               << " moves=" << fact.move_candidate_count << " drops=" << fact.drop_count
               << " cleanups=" << fact.cleanup_count << " borrows=" << fact.borrow_count
               << " partial_moves=" << fact.partial_move_count << " skipped_drops=" << fact.skipped_drop_count
               << " init=" << place_state_initialization_name(fact.initialization)
               << " move=" << place_state_move_state_name(fact.move_state)
               << " drop=" << place_state_drop_state_name(fact.drop_state)
               << " last_partial_move=" << fact.last_partial_move_point
               << " last_reinit=" << fact.last_reinit_point
               << " needs_drop=" << (fact.needs_drop ? "true" : "false")
               << " partial=" << (fact.has_partial_projection ? "true" : "false")
               << " partially_moved=" << (fact.is_partially_moved ? "true" : "false")
               << " drop_flag_live=" << (fact.drop_flag_live ? "true" : "false") << '\n';
    }

    stream << "events:\n";
    for (base::usize index = 0; index < facts.events.size(); ++index) {
        const PlaceStateEvent& event = facts.events[index];
        stream << "  e" << index << ' ' << place_state_event_kind_name(event.kind) << " place=" << event.place
               << " action=a" << event.action << " point=p" << event.point << " type=" << event.type.value
               << " range=";
        append_range(stream, event.range);
        stream << '\n';
    }

    stream << "violations:\n";
    for (base::usize index = 0; index < facts.violations.size(); ++index) {
        const PlaceStateViolation& violation = facts.violations[index];
        stream << "  v" << index << ' ' << place_state_violation_kind_name(violation.kind)
               << " place=" << violation.place << " action=a" << violation.action << " point=p" << violation.point
               << " related_place=" << violation.related_place << " related_action=a" << violation.related_action
               << " emitted=" << (violation.diagnostic_emitted ? "true" : "false") << " range=";
        append_range(stream, violation.range);
        stream << " related_range=";
        append_range(stream, violation.related_range);
        stream << '\n';
    }
    return stream.str();
}

SemanticAnalyzerCore::PlaceStateAnalyzer::PlaceStateAnalyzer(SemanticAnalyzerCore& core) noexcept
    : core_(core), resources_(core.state_.checked)
{
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::analyze(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    this->reset(key, signature);
    this->collect(function);
    this->finalize();
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::reset(
    const FunctionLookupKey& key, const FunctionSignature& signature)
{
    this->signature_ = &signature;
    this->key_ = key;
    this->facts_ = FunctionPlaceStateFacts{};
    this->facts_.function = key;
    this->facts_.part_index = signature.part_index;
    this->local_types_by_name_.clear();
    this->needs_drop_by_type_.clear();
    this->parameter_name_ids_.clear();
    this->graph_place_to_fact_place_.clear();
    this->parent_place_by_place_.clear();
    this->child_places_by_place_.clear();
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::collect(const syntax::ItemNode& function)
{
    const BodyFlowGraph* const graph = this->body_graph();
    if (graph == nullptr) {
        if (this->signature_ != nullptr && this->may_need_check(function, *this->signature_)) {
            this->facts_.graph_missing = true;
        }
        return;
    }

    this->build_local_type_index(function);
    this->collect_place_facts(*graph);
    this->collect_action_events(*graph);
    this->solve_place_states(*graph);
    this->enforce_diagnostics();
    this->facts_.solved = true;
}

bool SemanticAnalyzerCore::PlaceStateAnalyzer::may_need_check(
    const syntax::ItemNode& function, const FunctionSignature& signature)
{
    for (const TypeHandle param_type : signature.param_types) {
        if (this->type_needs_drop(param_type)) {
            return true;
        }
    }

    std::vector<syntax::StmtId> pending_stmts;
    std::vector<syntax::ExprId> pending_exprs;
    pending_stmts.reserve(SEMA_PLACE_STATE_PRECHECK_STACK_INITIAL_CAPACITY);
    pending_exprs.reserve(SEMA_PLACE_STATE_PRECHECK_STACK_INITIAL_CAPACITY);
    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, function.body);

    while (!pending_stmts.empty() || !pending_exprs.empty()) {
        while (!pending_stmts.empty()) {
            const syntax::StmtId stmt_id = pending_stmts.back();
            pending_stmts.pop_back();
            const std::optional<syntax::StmtNode> stmt = statement_node(this->core_.ctx_.module, stmt_id);
            if (!stmt.has_value()) {
                continue;
            }
            if (this->type_needs_drop(this->core_.cached_stmt_local_type(stmt_id))) {
                return true;
            }
            switch (stmt->kind) {
                case syntax::StmtKind::let:
                case syntax::StmtKind::var:
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->init);
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->else_block);
                    break;
                case syntax::StmtKind::assign:
                    return true;
                case syntax::StmtKind::if_:
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->condition);
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->then_block);
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->else_block);
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->else_if);
                    break;
                case syntax::StmtKind::for_:
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->for_init);
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->condition);
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->body);
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->for_update);
                    break;
                case syntax::StmtKind::for_range:
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->range_start);
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->range_end);
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->range_step);
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->body);
                    break;
                case syntax::StmtKind::while_:
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->condition);
                    push_precheck_stmt(pending_stmts, this->core_.ctx_.module, stmt->body);
                    break;
                case syntax::StmtKind::defer:
                case syntax::StmtKind::expr:
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->init);
                    break;
                case syntax::StmtKind::return_:
                    push_precheck_expr(pending_exprs, this->core_.ctx_.module, stmt->return_value);
                    break;
                case syntax::StmtKind::block:
                    pending_stmts.insert(pending_stmts.end(), stmt->statements.begin(), stmt->statements.end());
                    break;
                case syntax::StmtKind::break_:
                case syntax::StmtKind::continue_:
                    break;
            }
        }

        if (pending_exprs.empty()) {
            continue;
        }
        const syntax::ExprId expr = pending_exprs.back();
        pending_exprs.pop_back();
        if (!valid_expr(this->core_.ctx_.module, expr)) {
            continue;
        }
        if (this->core_.cached_expr_owned_use_mode(expr) == OwnedUseMode::owned_consume
            || this->type_needs_drop(this->core_.cached_expr_type(expr))) {
            return true;
        }

        const syntax::AstModule& module = this->core_.ctx_.module;
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
                const syntax::StructLiteralExprPayload* const literal =
                    module.exprs.struct_literal_payload(expr.value);
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
                    push_precheck_stmt(pending_stmts, module, block->block);
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
    return false;
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::finalize()
{
    if (!this->facts_.solved && !this->facts_.graph_missing && this->facts_.places.empty()
        && this->facts_.events.empty()) {
        this->core_.state_.checked.place_state_facts.erase(this->key_);
        return;
    }
    this->facts_.fingerprint = function_place_state_facts_fingerprint(this->facts_);
    this->core_.state_.checked.place_state_facts[this->key_] = std::move(this->facts_);
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::build_local_type_index(const syntax::ItemNode& function)
{
    if (this->signature_ != nullptr) {
        const base::usize count = std::min(function.params.size(), this->signature_->param_types.size());
        for (base::usize index = 0; index < count; ++index) {
            const syntax::ParamDecl& param = function.params[index];
            if (syntax::is_valid(param.name_id) && this->valid_type(this->signature_->param_types[index])) {
                this->local_types_by_name_[param.name_id.value] = this->signature_->param_types[index];
                this->parameter_name_ids_.insert(param.name_id.value);
            }
        }
    }

    std::vector<syntax::StmtId> pending;
    pending.reserve(SEMA_PLACE_STATE_STATEMENT_STACK_INITIAL_CAPACITY);
    if (syntax::is_valid(function.body)) {
        pending.push_back(function.body);
    }
    while (!pending.empty()) {
        const syntax::StmtId stmt = pending.back();
        pending.pop_back();
        const std::optional<syntax::StmtNode> node = statement_node(this->core_.ctx_.module, stmt);
        if (!node.has_value()) {
            continue;
        }
        this->index_statement_locals(stmt);
        switch (node->kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                if (syntax::is_valid(node->else_block)) {
                    pending.push_back(node->else_block);
                }
                break;
            case syntax::StmtKind::if_:
                if (syntax::is_valid(node->then_block)) {
                    pending.push_back(node->then_block);
                }
                if (syntax::is_valid(node->else_if)) {
                    pending.push_back(node->else_if);
                }
                if (syntax::is_valid(node->else_block)) {
                    pending.push_back(node->else_block);
                }
                break;
            case syntax::StmtKind::for_:
                if (syntax::is_valid(node->for_init)) {
                    pending.push_back(node->for_init);
                }
                if (syntax::is_valid(node->body)) {
                    pending.push_back(node->body);
                }
                if (syntax::is_valid(node->for_update)) {
                    pending.push_back(node->for_update);
                }
                break;
            case syntax::StmtKind::for_range:
            case syntax::StmtKind::while_:
                if (syntax::is_valid(node->body)) {
                    pending.push_back(node->body);
                }
                break;
            case syntax::StmtKind::block:
                pending.insert(pending.end(), node->statements.begin(), node->statements.end());
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
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::index_statement_locals(const syntax::StmtId stmt)
{
    const std::optional<syntax::StmtNode> node = statement_node(this->core_.ctx_.module, stmt);
    if (!node.has_value() || (node->kind != syntax::StmtKind::let && node->kind != syntax::StmtKind::var)) {
        return;
    }

    const TypeHandle type = this->core_.cached_stmt_local_type(stmt);
    if (!this->valid_type(type)) {
        return;
    }
    if (syntax::is_valid(node->pattern)) {
        this->index_pattern_bindings(node->pattern, type);
        return;
    }
    if (syntax::is_valid(node->name_id)) {
        this->local_types_by_name_[node->name_id.value] = type;
    }
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::index_pattern_bindings(
    const syntax::PatternId pattern, const TypeHandle type)
{
    std::vector<PatternTypeFrame> pending;
    pending.reserve(SEMA_PLACE_STATE_PATTERN_STACK_INITIAL_CAPACITY);
    pending.push_back(PatternTypeFrame{pattern, type});
    while (!pending.empty()) {
        const PatternTypeFrame frame = pending.back();
        pending.pop_back();
        const syntax::PatternNode* const node = pattern_node(this->core_.ctx_.module, frame.pattern);
        if (node == nullptr) {
            continue;
        }
        switch (node->kind) {
            case syntax::PatternKind::binding:
                if (syntax::is_valid(node->binding_name_id) && this->valid_type(frame.type)) {
                    this->local_types_by_name_[node->binding_name_id.value] = frame.type;
                }
                break;
            case syntax::PatternKind::tuple:
            case syntax::PatternKind::slice:
                for (base::usize index = 0; index < node->elements.size(); ++index) {
                    pending.push_back(PatternTypeFrame{
                        node->elements[index],
                        this->pattern_element_type(frame.type, index),
                    });
                }
                break;
            case syntax::PatternKind::struct_:
                for (const syntax::FieldPattern& field : node->field_patterns) {
                    pending.push_back(PatternTypeFrame{
                        field.pattern,
                        this->pattern_field_type(frame.type, field.name_id),
                    });
                }
                break;
            case syntax::PatternKind::enum_case:
                if (const EnumCaseInfo* const enum_case = this->pattern_enum_case(frame.type, *node);
                    enum_case != nullptr) {
                    const base::usize count = std::min(node->payload_patterns.size(), enum_case->payload_types.size());
                    for (base::usize index = 0; index < count; ++index) {
                        pending.push_back(PatternTypeFrame{
                            node->payload_patterns[index],
                            enum_case->payload_types[index],
                        });
                    }
                }
                break;
            case syntax::PatternKind::or_pattern:
                for (const syntax::PatternId alternative : node->alternatives) {
                    pending.push_back(PatternTypeFrame{alternative, frame.type});
                }
                break;
            case syntax::PatternKind::wildcard:
            case syntax::PatternKind::literal:
            case syntax::PatternKind::const_:
                break;
        }
    }
}

TypeHandle SemanticAnalyzerCore::PlaceStateAnalyzer::pattern_field_type(
    const TypeHandle owner, const IdentId field_name_id) const noexcept
{
    const StructInfo* const structure = this->core_.find_struct(owner);
    if (structure == nullptr) {
        return INVALID_TYPE_HANDLE;
    }
    const StructFieldInfo* const field = find_struct_field(*structure, field_name_id);
    return field == nullptr ? INVALID_TYPE_HANDLE : field->type;
}

TypeHandle SemanticAnalyzerCore::PlaceStateAnalyzer::pattern_element_type(
    const TypeHandle owner, const base::usize index) const noexcept
{
    if (!this->valid_type(owner)) {
        return INVALID_TYPE_HANDLE;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(owner);
    if (info.kind == TypeKind::tuple) {
        return index < info.tuple_elements.size() ? info.tuple_elements[index] : INVALID_TYPE_HANDLE;
    }
    if (info.kind == TypeKind::array) {
        return info.array_element;
    }
    if (info.kind == TypeKind::slice) {
        return info.slice_element;
    }
    return INVALID_TYPE_HANDLE;
}

const EnumCaseInfo* SemanticAnalyzerCore::PlaceStateAnalyzer::pattern_enum_case(
    const TypeHandle owner, const syntax::PatternNode& pattern) const
{
    return this->core_.find_enum_case_by_type_and_case(owner, pattern.case_name_id, pattern.case_name);
}

const BodyFlowGraph* SemanticAnalyzerCore::PlaceStateAnalyzer::body_graph() const noexcept
{
    const auto found = this->core_.state_.checked.body_flow_graphs.find(this->key_);
    return found == this->core_.state_.checked.body_flow_graphs.end() ? nullptr : &found->second;
}

TypeHandle SemanticAnalyzerCore::PlaceStateAnalyzer::action_type(
    const BodyFlowGraph& graph, const BodyFlowAction& action) const
{
    if (valid_expr(this->core_.ctx_.module, action.expr)) {
        const TypeHandle expr_type = this->core_.cached_expr_type(action.expr);
        if (this->valid_type(expr_type)) {
            return expr_type;
        }
    }
    if (action.place != SEMA_BODY_FLOW_INVALID_INDEX && action.place < graph.places.size()) {
        return this->place_type(graph, graph.places[action.place]);
    }
    return INVALID_TYPE_HANDLE;
}

TypeHandle SemanticAnalyzerCore::PlaceStateAnalyzer::place_type(
    const BodyFlowGraph&, const BodyFlowPlace& place) const
{
    TypeHandle current = INVALID_TYPE_HANDLE;
    switch (place.root_kind) {
        case BodyFlowPlaceRootKind::local: {
            const auto found = this->local_types_by_name_.find(place.root_name_id.value);
            current = found == this->local_types_by_name_.end() ? INVALID_TYPE_HANDLE : found->second;
            break;
        }
        case BodyFlowPlaceRootKind::temporary:
            current = this->core_.cached_expr_type(place.root_expr);
            break;
        case BodyFlowPlaceRootKind::unknown:
        case BodyFlowPlaceRootKind::none:
            return INVALID_TYPE_HANDLE;
    }
    for (const BodyFlowPlaceProjection& projection : place.projections) {
        current = this->projected_type(current, projection);
        if (!this->valid_type(current)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    return current;
}

TypeHandle SemanticAnalyzerCore::PlaceStateAnalyzer::projected_type(
    const TypeHandle current, const BodyFlowPlaceProjection& projection) const
{
    if (!this->valid_type(current)) {
        return INVALID_TYPE_HANDLE;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(current);
    switch (projection.kind) {
        case BodyFlowPlaceProjectionKind::field:
            if (const StructInfo* const structure = this->core_.find_struct(current); structure != nullptr) {
                const StructFieldInfo* const field = find_struct_field(*structure, projection.field_name_id);
                return field == nullptr ? INVALID_TYPE_HANDLE : field->type;
            }
            return INVALID_TYPE_HANDLE;
        case BodyFlowPlaceProjectionKind::tuple_element:
            if (info.kind == TypeKind::tuple && projection.element_index < info.tuple_elements.size()) {
                return info.tuple_elements[projection.element_index];
            }
            return INVALID_TYPE_HANDLE;
        case BodyFlowPlaceProjectionKind::index:
            if (info.kind == TypeKind::array) {
                return info.array_element;
            }
            if (info.kind == TypeKind::slice) {
                return info.slice_element;
            }
            return INVALID_TYPE_HANDLE;
        case BodyFlowPlaceProjectionKind::dereference:
            if (info.kind == TypeKind::reference || info.kind == TypeKind::pointer) {
                return info.pointee;
            }
            return INVALID_TYPE_HANDLE;
        case BodyFlowPlaceProjectionKind::slice:
            if (info.kind == TypeKind::array) {
                return this->core_.state_.checked.types.slice(PointerMutability::const_, info.array_element);
            }
            if (info.kind == TypeKind::slice) {
                return current;
            }
            return INVALID_TYPE_HANDLE;
    }
    return INVALID_TYPE_HANDLE;
}

std::optional<PlaceStateEventKind> SemanticAnalyzerCore::PlaceStateAnalyzer::event_kind(
    const BodyFlowActionKind kind) const noexcept
{
    switch (kind) {
        case BodyFlowActionKind::read:
            return PlaceStateEventKind::read;
        case BodyFlowActionKind::write:
            return PlaceStateEventKind::write;
        case BodyFlowActionKind::reinit:
            return PlaceStateEventKind::reinit;
        case BodyFlowActionKind::move_candidate:
            return PlaceStateEventKind::move_candidate;
        case BodyFlowActionKind::drop:
            return PlaceStateEventKind::drop;
        case BodyFlowActionKind::cleanup_scope:
        case BodyFlowActionKind::cleanup_storage:
            return PlaceStateEventKind::cleanup;
        case BodyFlowActionKind::borrow_shared:
            return PlaceStateEventKind::borrow_shared;
        case BodyFlowActionKind::borrow_mutable:
        case BodyFlowActionKind::call_receiver_reserve:
        case BodyFlowActionKind::call_receiver_activate:
            return PlaceStateEventKind::borrow_mutable;
        case BodyFlowActionKind::call:
        case BodyFlowActionKind::return_:
        case BodyFlowActionKind::branch:
            return std::nullopt;
    }
    return std::nullopt;
}

std::optional<PlaceStateEventKind> SemanticAnalyzerCore::PlaceStateAnalyzer::event_kind_for_action(
    const BodyFlowGraph& graph, const BodyFlowAction& action) const
{
    const std::optional<PlaceStateEventKind> kind = this->event_kind(action.kind);
    if (!kind.has_value()) {
        return std::nullopt;
    }
    if ((*kind == PlaceStateEventKind::borrow_shared || *kind == PlaceStateEventKind::borrow_mutable)
        && !this->action_borrow_is_place_state_event(graph, action)) {
        return std::nullopt;
    }
    return kind;
}

bool SemanticAnalyzerCore::PlaceStateAnalyzer::action_borrow_is_place_state_event(
    const BodyFlowGraph&, const BodyFlowAction& action) const
{
    if (action.kind != BodyFlowActionKind::borrow_shared && action.kind != BodyFlowActionKind::borrow_mutable) {
        return true;
    }
    if (!valid_expr(this->core_.ctx_.module, action.expr)) {
        return true;
    }
    const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(action.expr.value);
    if (kind != syntax::ExprKind::call && kind != syntax::ExprKind::str_from_bytes_unchecked) {
        return true;
    }
    return this->expr_type_is_definite_borrow_carrier(action.expr);
}

bool SemanticAnalyzerCore::PlaceStateAnalyzer::expr_type_is_definite_borrow_carrier(const syntax::ExprId expr) const
{
    const TypeHandle type = this->core_.cached_expr_type(expr);
    if (!this->valid_type(type)) {
        return false;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(type);
    return info.kind == TypeKind::reference || info.kind == TypeKind::slice
        || (info.kind == TypeKind::builtin && info.builtin == BuiltinType::str);
}

bool SemanticAnalyzerCore::PlaceStateAnalyzer::type_needs_drop(const TypeHandle type)
{
    if (!this->valid_type(type)) {
        return false;
    }
    const auto found = this->needs_drop_by_type_.find(type.value);
    if (found != this->needs_drop_by_type_.end()) {
        return found->second;
    }
    const bool needs_drop = resource_needs_drop(this->resources_.classify(type));
    this->needs_drop_by_type_.emplace(type.value, needs_drop);
    return needs_drop;
}

bool SemanticAnalyzerCore::PlaceStateAnalyzer::valid_type(const TypeHandle type) const noexcept
{
    return is_valid(type) && type.value < this->core_.state_.checked.types.size();
}

bool SemanticAnalyzerCore::PlaceStateAnalyzer::is_parameter_name(const IdentId name) const noexcept
{
    return syntax::is_valid(name) && this->parameter_name_ids_.find(name.value) != this->parameter_name_ids_.end();
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::collect_place_facts(const BodyFlowGraph& graph)
{
    std::unordered_map<PlaceStatePlaceKey, base::u32, PlaceStatePlaceKeyHash> place_lookup;
    place_lookup.reserve(graph.places.size());
    this->graph_place_to_fact_place_.assign(graph.places.size(), SEMA_BODY_FLOW_INVALID_INDEX);
    this->facts_.places.reserve(graph.places.size());
    for (base::usize index = 0; index < graph.places.size(); ++index) {
        const BodyFlowPlace& place = graph.places[index];
        base::u32 fact_place = SEMA_BODY_FLOW_INVALID_INDEX;
        for (base::usize projection_count = 0; projection_count <= place.projections.size(); ++projection_count) {
            const PlaceStatePlaceKey key = place_key_for_prefix(place, projection_count);
            if (const auto found = place_lookup.find(key); found != place_lookup.end()) {
                fact_place = found->second;
                continue;
            }

            const BodyFlowPlace prefix = place_prefix(place, projection_count);
            const TypeHandle type = this->place_type(graph, prefix);
            const bool initialized_root =
                prefix.root_kind == BodyFlowPlaceRootKind::temporary || this->is_parameter_name(prefix.root_name_id);
            const bool needs_drop = this->type_needs_drop(type);
            const base::u32 parent = projection_count == 0 ? SEMA_BODY_FLOW_INVALID_INDEX : fact_place;
            const base::u32 new_place =
                base::checked_u32(this->facts_.places.size(), SEMA_PLACE_STATE_ID_CONTEXT);
            place_lookup.emplace(key, new_place);
            this->parent_place_by_place_.push_back(parent);
            this->child_places_by_place_.push_back({});
            if (parent != SEMA_BODY_FLOW_INVALID_INDEX && parent < this->child_places_by_place_.size()) {
                this->child_places_by_place_[parent].push_back(new_place);
            }

            PlaceStateFact fact;
            fact.place = new_place;
            fact.root_kind = prefix.root_kind;
            fact.root_name_id = prefix.root_name_id;
            fact.root_expr = normalized_fact_root_expr(prefix);
            fact.type = type;
            fact.projection_count = static_cast<base::u64>(projection_count);
            fact.initialization =
                initialized_root ? PlaceStateInitialization::initialized : PlaceStateInitialization::unknown;
            fact.drop_state =
                initialized_root && needs_drop ? PlaceStateDropState::drop_pending : PlaceStateDropState::none;
            fact.needs_drop = needs_drop;
            fact.has_partial_projection = projection_count != 0;
            fact.drop_flag_live = initialized_root && needs_drop;
            this->facts_.places.push_back(fact);
            fact_place = new_place;
        }
        this->graph_place_to_fact_place_[index] = fact_place;
    }
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::collect_action_events(const BodyFlowGraph& graph)
{
    this->facts_.events.reserve(graph.actions.size());
    for (base::usize action_index = 0; action_index < graph.actions.size(); ++action_index) {
        const BodyFlowAction& action = graph.actions[action_index];
        if (action.place == SEMA_BODY_FLOW_INVALID_INDEX || action.place >= this->graph_place_to_fact_place_.size()) {
            continue;
        }
        const base::u32 fact_place = this->graph_place_to_fact_place_[action.place];
        if (fact_place == SEMA_BODY_FLOW_INVALID_INDEX || fact_place >= this->facts_.places.size()) {
            continue;
        }
        const std::optional<PlaceStateEventKind> kind = this->event_kind_for_action(graph, action);
        if (!kind.has_value()) {
            continue;
        }

        const base::u32 checked_action = base::checked_u32(action_index, SEMA_PLACE_STATE_ID_CONTEXT);
        const TypeHandle type = this->action_type(graph, action);
        this->facts_.events.push_back(PlaceStateEvent{
            .kind = *kind,
            .place = fact_place,
            .action = checked_action,
            .point = action.point,
            .type = type,
            .range = action.range,
        });
        this->apply_event_to_fact(this->facts_.places[fact_place], *kind, action);
    }
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::apply_event_to_fact(
    PlaceStateFact& fact, const PlaceStateEventKind kind, const BodyFlowAction& action)
{
    if (fact.first_action_point == SEMA_BODY_FLOW_INVALID_INDEX) {
        fact.first_action_point = action.point;
    }
    fact.last_action_point = action.point;

    switch (kind) {
        case PlaceStateEventKind::read:
            ++fact.read_count;
            if (fact.initialization == PlaceStateInitialization::unknown) {
                fact.initialization = PlaceStateInitialization::maybe_initialized;
            }
            break;
        case PlaceStateEventKind::write:
            ++fact.write_count;
            fact.last_write_point = action.point;
            fact.last_reinit_point = action.point;
            fact.initialization = PlaceStateInitialization::initialized;
            fact.move_state = PlaceStateMoveState::none;
            fact.drop_state = fact.needs_drop ? PlaceStateDropState::drop_pending : PlaceStateDropState::none;
            fact.drop_flag_live = fact.needs_drop;
            break;
        case PlaceStateEventKind::reinit:
            ++fact.reinit_count;
            fact.last_write_point = action.point;
            fact.last_reinit_point = action.point;
            fact.initialization = PlaceStateInitialization::initialized;
            fact.move_state = PlaceStateMoveState::none;
            fact.drop_state = fact.needs_drop ? PlaceStateDropState::drop_pending : PlaceStateDropState::none;
            fact.drop_flag_live = fact.needs_drop;
            break;
        case PlaceStateEventKind::move_candidate:
            ++fact.move_candidate_count;
            fact.last_move_point = action.point;
            if (this->core_.cached_expr_owned_use_mode(action.expr) == OwnedUseMode::owned_consume) {
                if (fact.has_partial_projection) {
                    ++fact.partial_move_count;
                    fact.last_partial_move_point = action.point;
                }
                fact.initialization = PlaceStateInitialization::uninitialized;
                fact.move_state = PlaceStateMoveState::maybe_moved;
                fact.drop_state = PlaceStateDropState::none;
                fact.drop_flag_live = false;
            } else {
                fact.move_state = PlaceStateMoveState::move_candidate;
                if (fact.initialization == PlaceStateInitialization::unknown) {
                    fact.initialization = PlaceStateInitialization::maybe_initialized;
                }
            }
            break;
        case PlaceStateEventKind::drop:
            ++fact.drop_count;
            fact.last_drop_point = action.point;
            fact.initialization = PlaceStateInitialization::uninitialized;
            fact.drop_state = PlaceStateDropState::dropped;
            fact.drop_flag_live = false;
            break;
        case PlaceStateEventKind::cleanup:
            ++fact.cleanup_count;
            fact.last_drop_point = action.point;
            if (fact.initialization == PlaceStateInitialization::uninitialized) {
                ++fact.skipped_drop_count;
            }
            fact.initialization = PlaceStateInitialization::uninitialized;
            fact.drop_state = PlaceStateDropState::dropped;
            fact.drop_flag_live = false;
            break;
        case PlaceStateEventKind::borrow_shared:
        case PlaceStateEventKind::borrow_mutable:
            ++fact.borrow_count;
            if (fact.initialization == PlaceStateInitialization::unknown) {
                fact.initialization = PlaceStateInitialization::maybe_initialized;
            }
            break;
    }
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::solve_place_states(const BodyFlowGraph& graph)
{
    if (this->facts_.places.empty() || graph.points.empty()) {
        return;
    }

    PlaceStateActionIndex index;
    index.actions_by_point.resize(graph.points.size());
    index.outgoing_points.resize(graph.points.size());
    for (base::usize action_index = 0; action_index < graph.actions.size(); ++action_index) {
        const BodyFlowAction& action = graph.actions[action_index];
        if (valid_point(graph, action.point)) {
            index.actions_by_point[action.point].push_back(base::checked_u32(action_index, SEMA_PLACE_STATE_ID_CONTEXT));
        }
    }
    for (const BodyFlowEdge& edge : graph.edges) {
        if (valid_point(graph, edge.from) && valid_point(graph, edge.to)) {
            index.outgoing_points[edge.from].push_back(edge.to);
        }
    }
    std::vector<bool> enforced_place(this->facts_.places.size(), false);
    for (base::usize reverse_index = this->facts_.places.size(); reverse_index > 0; --reverse_index) {
        const base::usize place = reverse_index - 1U;
        bool enforced = this->facts_.places[place].needs_drop;
        if (place < this->child_places_by_place_.size()) {
            for (const base::u32 child : this->child_places_by_place_[place]) {
                enforced = enforced || (child < enforced_place.size() && enforced_place[child]);
            }
        }
        enforced_place[place] = enforced;
    }

    auto initial_state = [&]() {
        std::vector<PlaceRuntimeState> state;
        state.reserve(this->facts_.places.size());
        for (const PlaceStateFact& fact : this->facts_.places) {
            const bool initialized_root =
                fact.root_kind == BodyFlowPlaceRootKind::temporary || this->is_parameter_name(fact.root_name_id);
            state.push_back(PlaceRuntimeState{
                .initialization =
                    initialized_root ? PlaceStateInitialization::initialized : PlaceStateInitialization::unknown,
                .move_state = PlaceStateMoveState::none,
                .drop_state = initialized_root && fact.needs_drop ? PlaceStateDropState::drop_pending
                                                                  : PlaceStateDropState::none,
                .drop_flag_live = initialized_root && fact.needs_drop,
            });
        }
        return state;
    };

    const auto merge_initialization =
        [](const PlaceStateInitialization lhs, const PlaceStateInitialization rhs) noexcept {
            if (lhs == rhs) {
                return lhs;
            }
            return PlaceStateInitialization::maybe_initialized;
        };
    const auto merge_move = [](const PlaceStateMoveState lhs, const PlaceStateMoveState rhs) noexcept {
        if (lhs == rhs) {
            return lhs;
        }
        if (lhs == PlaceStateMoveState::maybe_moved || rhs == PlaceStateMoveState::maybe_moved) {
            return PlaceStateMoveState::maybe_moved;
        }
        if (lhs == PlaceStateMoveState::move_candidate || rhs == PlaceStateMoveState::move_candidate) {
            return PlaceStateMoveState::move_candidate;
        }
        return PlaceStateMoveState::none;
    };
    const auto merge_drop = [](const PlaceStateDropState lhs, const PlaceStateDropState rhs) noexcept {
        if (lhs == rhs) {
            return lhs;
        }
        if (lhs == PlaceStateDropState::dropped || rhs == PlaceStateDropState::dropped) {
            return PlaceStateDropState::dropped;
        }
        if (lhs == PlaceStateDropState::drop_pending || rhs == PlaceStateDropState::drop_pending) {
            return PlaceStateDropState::drop_pending;
        }
        return PlaceStateDropState::none;
    };
    const auto latest_action = [](const base::u32 lhs, const base::u32 rhs) noexcept {
        if (lhs == SEMA_BODY_FLOW_INVALID_INDEX) {
            return rhs;
        }
        if (rhs == SEMA_BODY_FLOW_INVALID_INDEX) {
            return lhs;
        }
        return std::max(lhs, rhs);
    };
    auto merge_state = [&](std::vector<PlaceRuntimeState>& target, const std::vector<PlaceRuntimeState>& source) {
        bool changed = false;
        for (base::usize place_index = 0; place_index < target.size() && place_index < source.size(); ++place_index) {
            PlaceRuntimeState& lhs = target[place_index];
            const PlaceRuntimeState& rhs = source[place_index];
            const PlaceRuntimeState before = lhs;
            lhs.initialization = merge_initialization(lhs.initialization, rhs.initialization);
            lhs.move_state = merge_move(lhs.move_state, rhs.move_state);
            lhs.drop_state = merge_drop(lhs.drop_state, rhs.drop_state);
            lhs.last_move_action = latest_action(lhs.last_move_action, rhs.last_move_action);
            lhs.last_partial_move_action = latest_action(lhs.last_partial_move_action, rhs.last_partial_move_action);
            lhs.last_drop_action = latest_action(lhs.last_drop_action, rhs.last_drop_action);
            lhs.partially_moved = lhs.partially_moved || rhs.partially_moved;
            lhs.drop_flag_live = lhs.drop_flag_live && rhs.drop_flag_live;
            changed = changed || !(lhs.initialization == before.initialization && lhs.move_state == before.move_state
                                      && lhs.drop_state == before.drop_state
                                      && lhs.last_move_action == before.last_move_action
                                      && lhs.last_partial_move_action == before.last_partial_move_action
                                      && lhs.last_drop_action == before.last_drop_action
                                      && lhs.partially_moved == before.partially_moved
                                      && lhs.drop_flag_live == before.drop_flag_live);
        }
        return changed;
    };

    auto has_moved_child_or_descendant = [&](const std::vector<PlaceRuntimeState>& state, const base::u32 place) {
        if (place >= this->child_places_by_place_.size()) {
            return false;
        }
        for (const base::u32 child : this->child_places_by_place_[place]) {
            if (child >= state.size()) {
                continue;
            }
            const PlaceRuntimeState& child_state = state[child];
            if (child_state.initialization == PlaceStateInitialization::uninitialized
                || child_state.move_state == PlaceStateMoveState::maybe_moved || child_state.partially_moved) {
                return true;
            }
        }
        return false;
    };

    auto refresh_ancestors = [&](std::vector<PlaceRuntimeState>& state, base::u32 place) {
        while (place < this->parent_place_by_place_.size()) {
            const base::u32 parent = this->parent_place_by_place_[place];
            if (parent == SEMA_BODY_FLOW_INVALID_INDEX || parent >= state.size()) {
                break;
            }
            state[parent].partially_moved = has_moved_child_or_descendant(state, parent);
            if (state[parent].partially_moved) {
                state[parent].move_state = PlaceStateMoveState::maybe_moved;
            } else if (state[parent].move_state == PlaceStateMoveState::maybe_moved
                       && state[parent].initialization == PlaceStateInitialization::initialized) {
                state[parent].move_state = PlaceStateMoveState::none;
            }
            place = parent;
        }
    };

    auto for_place_and_descendants = [&](const base::u32 place, auto&& callback) {
        if (place >= this->facts_.places.size()) {
            return;
        }
        std::vector<base::u32> pending;
        pending.reserve(SEMA_PLACE_STATE_WORKLIST_INITIAL_CAPACITY);
        pending.push_back(place);
        while (!pending.empty()) {
            const base::u32 current = pending.back();
            pending.pop_back();
            if (current >= this->facts_.places.size()) {
                continue;
            }
            callback(current);
            if (current < this->child_places_by_place_.size()) {
                pending.insert(pending.end(), this->child_places_by_place_[current].begin(),
                    this->child_places_by_place_[current].end());
            }
        }
    };

    auto mark_initialized = [&](std::vector<PlaceRuntimeState>& state, const base::u32 place, const base::u32 action) {
        for_place_and_descendants(place, [&](const base::u32 current) {
            PlaceRuntimeState& runtime = state[current];
            runtime.initialization = PlaceStateInitialization::initialized;
            runtime.move_state = PlaceStateMoveState::none;
            runtime.drop_state =
                this->facts_.places[current].needs_drop ? PlaceStateDropState::drop_pending : PlaceStateDropState::none;
            runtime.last_move_action = SEMA_BODY_FLOW_INVALID_INDEX;
            runtime.last_partial_move_action = SEMA_BODY_FLOW_INVALID_INDEX;
            runtime.partially_moved = false;
            runtime.drop_flag_live = this->facts_.places[current].needs_drop;
            this->facts_.places[current].last_reinit_point =
                valid_action(graph, action) ? graph.actions[action].point : this->facts_.places[current].last_reinit_point;
        });
        refresh_ancestors(state, place);
    };

    auto mark_moved = [&](std::vector<PlaceRuntimeState>& state, const base::u32 place, const base::u32 action) {
        for_place_and_descendants(place, [&](const base::u32 current) {
            PlaceRuntimeState& runtime = state[current];
            runtime.initialization = PlaceStateInitialization::uninitialized;
            runtime.move_state = PlaceStateMoveState::maybe_moved;
            runtime.drop_state = PlaceStateDropState::none;
            runtime.last_move_action = action;
            runtime.drop_flag_live = false;
        });
        base::u32 ancestor = place < this->parent_place_by_place_.size() ? this->parent_place_by_place_[place]
                                                                        : SEMA_BODY_FLOW_INVALID_INDEX;
        while (ancestor != SEMA_BODY_FLOW_INVALID_INDEX && ancestor < state.size()) {
            state[ancestor].partially_moved = true;
            state[ancestor].move_state = PlaceStateMoveState::maybe_moved;
            state[ancestor].last_partial_move_action = action;
            if (ancestor < this->parent_place_by_place_.size()) {
                ancestor = this->parent_place_by_place_[ancestor];
            } else {
                break;
            }
        }
    };

    auto mark_dropped = [&](std::vector<PlaceRuntimeState>& state, const base::u32 place, const base::u32 action) {
        for_place_and_descendants(place, [&](const base::u32 current) {
            PlaceRuntimeState& runtime = state[current];
            runtime.initialization = PlaceStateInitialization::uninitialized;
            runtime.move_state = PlaceStateMoveState::none;
            runtime.drop_state = PlaceStateDropState::dropped;
            runtime.last_drop_action = action;
            runtime.drop_flag_live = false;
        });
        refresh_ancestors(state, place);
    };

    std::unordered_set<std::uint64_t> seen_violations;
    auto record_violation = [&](const PlaceStateViolationKind kind, const base::u32 place, const base::u32 action,
                                const base::u32 related_action) {
        const std::uint64_t key = (static_cast<std::uint64_t>(place) << SEMA_PLACE_STATE_VIOLATION_KEY_SHIFT)
            ^ (static_cast<std::uint64_t>(action) << 8U) ^ static_cast<base::u8>(kind);
        if (!seen_violations.insert(key).second || !valid_action(graph, action)) {
            return;
        }
        const BodyFlowAction& site = graph.actions[action];
        PlaceStateViolation violation;
        violation.kind = kind;
        violation.place = place;
        violation.action = action;
        violation.point = site.point;
        violation.related_place = place;
        violation.related_action = related_action;
        violation.range = site.range;
        if (valid_action(graph, related_action)) {
            violation.related_range = graph.actions[related_action].range;
        }
        this->facts_.violations.push_back(violation);
    };

    auto validate_access = [&](const std::vector<PlaceRuntimeState>& state, const base::u32 place,
                               const base::u32 action_index, const PlaceStateAccessKind access) {
        if (place >= state.size()) {
            return;
        }
        if (place >= enforced_place.size() || !enforced_place[place]) {
            return;
        }
        const PlaceRuntimeState& runtime = state[place];
        const bool moved_descendant = runtime.partially_moved;
        if (access == PlaceStateAccessKind::drop && runtime.drop_state == PlaceStateDropState::dropped) {
            record_violation(PlaceStateViolationKind::double_drop, place, action_index, runtime.last_drop_action);
            return;
        }
        if (access_requires_initialized_place(access)) {
            if (runtime.initialization == PlaceStateInitialization::uninitialized) {
                const PlaceStateViolationKind kind = access == PlaceStateAccessKind::drop
                    ? PlaceStateViolationKind::drop_after_move
                    : PlaceStateViolationKind::use_after_move;
                record_violation(kind, place, action_index, runtime.last_move_action);
            } else if (runtime.initialization == PlaceStateInitialization::maybe_initialized
                       || runtime.initialization == PlaceStateInitialization::unknown) {
                if (valid_action(graph, runtime.last_move_action) || runtime.partially_moved || moved_descendant) {
                    record_violation(
                        PlaceStateViolationKind::maybe_uninitialized_use, place, action_index, runtime.last_move_action);
                }
            }
        }
        if (moved_descendant
            && (access == PlaceStateAccessKind::read || access == PlaceStateAccessKind::move
                || access == PlaceStateAccessKind::borrow || access == PlaceStateAccessKind::drop)) {
            const PlaceStateViolationKind kind = access == PlaceStateAccessKind::drop
                ? PlaceStateViolationKind::drop_after_partial_move
                : PlaceStateViolationKind::use_after_partial_move;
            record_violation(kind, place, action_index, runtime.last_partial_move_action);
        }
    };

    auto apply_action = [&](std::vector<PlaceRuntimeState>& state, const base::u32 action_index) {
        if (!valid_action(graph, action_index)) {
            return;
        }
        const BodyFlowAction& action = graph.actions[action_index];
        if (action.place == SEMA_BODY_FLOW_INVALID_INDEX || action.place >= this->graph_place_to_fact_place_.size()) {
            return;
        }
        const base::u32 place = this->graph_place_to_fact_place_[action.place];
        if (place == SEMA_BODY_FLOW_INVALID_INDEX || place >= state.size()) {
            return;
        }
        const std::optional<PlaceStateEventKind> event = this->event_kind_for_action(graph, action);
        if (!event.has_value()) {
            return;
        }
        const std::optional<PlaceStateAccessKind> access = access_kind_for_event(*event);
        if (!access.has_value()) {
            return;
        }
        validate_access(state, place, action_index, *access);

        switch (*access) {
            case PlaceStateAccessKind::write:
            case PlaceStateAccessKind::reinit:
                mark_initialized(state, place, action_index);
                break;
            case PlaceStateAccessKind::move:
                if (this->core_.cached_expr_owned_use_mode(action.expr) == OwnedUseMode::owned_consume) {
                    mark_moved(state, place, action_index);
                }
                break;
            case PlaceStateAccessKind::drop:
                mark_dropped(state, place, action_index);
                break;
            case PlaceStateAccessKind::cleanup:
                if (state[place].drop_flag_live || state[place].initialization != PlaceStateInitialization::uninitialized) {
                    mark_dropped(state, place, action_index);
                }
                break;
            case PlaceStateAccessKind::read:
            case PlaceStateAccessKind::borrow:
                break;
        }
    };

    const base::u32 entry_point = 0U;
    const base::u32 exit_point = graph.points.size() > 1U ? 1U : entry_point;
    std::vector<std::vector<PlaceRuntimeState>> in_states(graph.points.size(), initial_state());
    std::vector<std::vector<PlaceRuntimeState>> out_states(graph.points.size(), initial_state());
    std::vector<bool> reached(graph.points.size(), false);
    std::vector<base::u32> worklist;
    worklist.reserve(SEMA_PLACE_STATE_WORKLIST_INITIAL_CAPACITY);
    if (entry_point < graph.points.size()) {
        reached[entry_point] = true;
        in_states[entry_point] = initial_state();
        worklist.push_back(entry_point);
    }

    while (!worklist.empty()) {
        const base::u32 point = worklist.back();
        worklist.pop_back();
        if (!valid_point(graph, point)) {
            continue;
        }
        std::vector<PlaceRuntimeState> state = in_states[point];
        for (const base::u32 action_index : index.actions_by_point[point]) {
            apply_action(state, action_index);
        }
        out_states[point] = state;
        for (const base::u32 successor : index.outgoing_points[point]) {
            if (!valid_point(graph, successor)) {
                continue;
            }
            bool changed = false;
            if (!reached[successor]) {
                in_states[successor] = state;
                reached[successor] = true;
                changed = true;
            } else {
                changed = merge_state(in_states[successor], state);
            }
            if (changed) {
                worklist.push_back(successor);
            }
        }
    }

    const std::vector<PlaceRuntimeState>& final_state =
        exit_point < in_states.size() && reached[exit_point] ? in_states[exit_point] : out_states[entry_point];
    for (base::usize place = 0; place < this->facts_.places.size() && place < final_state.size(); ++place) {
        PlaceStateFact& fact = this->facts_.places[place];
        const PlaceRuntimeState& runtime = final_state[place];
        fact.initialization = runtime.initialization;
        fact.move_state = runtime.move_state;
        fact.drop_state = runtime.drop_state;
        fact.is_partially_moved = runtime.partially_moved;
        fact.drop_flag_live = runtime.drop_flag_live;
        if (valid_action(graph, runtime.last_partial_move_action)) {
            fact.last_partial_move_point = graph.actions[runtime.last_partial_move_action].point;
        }
        if (valid_action(graph, runtime.last_move_action)) {
            fact.last_move_point = graph.actions[runtime.last_move_action].point;
        }
        if (valid_action(graph, runtime.last_drop_action)) {
            fact.last_drop_point = graph.actions[runtime.last_drop_action].point;
        }
    }
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::enforce_diagnostics()
{
    this->facts_.diagnostic_mode_enforced = true;
    std::unordered_set<std::uint64_t> reported_sites;
    for (PlaceStateViolation& violation : this->facts_.violations) {
        const std::uint64_t site_key =
            (static_cast<std::uint64_t>(violation.point) << SEMA_PLACE_STATE_VIOLATION_KEY_SHIFT)
            ^ static_cast<base::u8>(violation.kind);
        if (!reported_sites.insert(site_key).second) {
            violation.diagnostic_emitted = true;
            continue;
        }
        this->core_.report_general(violation.range, std::string(place_state_violation_message(violation.kind)));
        if (violation.related_action != SEMA_BODY_FLOW_INVALID_INDEX) {
            this->core_.report_note(
                violation.related_range, SemanticDiagnosticKind::general, std::string(SEMA_PLACE_STATE_MOVE_OCCURRED));
        }
        violation.diagnostic_emitted = true;
    }
}

void SemanticAnalyzerCore::analyze_place_states(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    PlaceStateAnalyzer(*this).analyze(function, key, signature);
}

bool SemanticAnalyzerCore::may_need_place_state_check(
    const syntax::ItemNode& function, const FunctionSignature& signature)
{
    return PlaceStateAnalyzer(*this).may_need_check(function, signature);
}

} // namespace aurex::sema
