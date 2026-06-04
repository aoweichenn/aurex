#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <algorithm>
#include <optional>
#include <sstream>
#include <utility>

#include <frontend/sema/internal/place/private/place_state.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_PLACE_STATE_ID_CONTEXT = "sema place state id";
constexpr std::string_view SEMA_PLACE_STATE_FACTS_FINGERPRINT_MARKER = "sema.place_state.facts.v1";
constexpr base::usize SEMA_PLACE_STATE_STATEMENT_STACK_INITIAL_CAPACITY = 32;
constexpr base::usize SEMA_PLACE_STATE_PATTERN_STACK_INITIAL_CAPACITY = 16;

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

[[nodiscard]] const StructFieldInfo* find_struct_field(const StructInfo& structure, const IdentId field_name) noexcept
{
    for (const StructFieldInfo& field : structure.fields) {
        if (field.name_id == field_name) {
            return &field;
        }
    }
    return nullptr;
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
    builder.mix_u8(static_cast<base::u8>(fact.initialization));
    builder.mix_u8(static_cast<base::u8>(fact.move_state));
    builder.mix_u8(static_cast<base::u8>(fact.drop_state));
    builder.mix_bool(fact.needs_drop);
    builder.mix_bool(fact.has_partial_projection);
}

void mix_place_event(query::StableHashBuilder& builder, const PlaceStateEvent& event) noexcept
{
    builder.mix_u8(static_cast<base::u8>(event.kind));
    builder.mix_u32(event.place);
    builder.mix_u32(event.action);
    builder.mix_u32(event.point);
    builder.mix_u32(event.type.value);
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
    return builder.finish();
}

std::string dump_function_place_state_facts(const FunctionPlaceStateFacts& facts)
{
    std::ostringstream stream;
    stream << "place_state_facts function=" << facts.function.module << ':' << facts.function.owner_type << ':';
    append_optional_name_id(stream, facts.function.name);
    stream << " places=" << facts.places.size() << " events=" << facts.events.size()
           << " solved=" << (facts.solved ? "true" : "false")
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
               << " init=" << place_state_initialization_name(fact.initialization)
               << " move=" << place_state_move_state_name(fact.move_state)
               << " drop=" << place_state_drop_state_name(fact.drop_state)
               << " needs_drop=" << (fact.needs_drop ? "true" : "false")
               << " partial=" << (fact.has_partial_projection ? "true" : "false") << '\n';
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
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::collect(const syntax::ItemNode& function)
{
    const BodyFlowGraph* const graph = this->body_graph();
    if (graph == nullptr) {
        return;
    }

    this->build_local_type_index(function);
    this->collect_place_facts(*graph);
    this->collect_action_events(*graph);
    this->facts_.solved = true;
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

void SemanticAnalyzerCore::PlaceStateAnalyzer::collect_place_facts(const BodyFlowGraph& graph)
{
    this->facts_.places.reserve(graph.places.size());
    for (base::usize index = 0; index < graph.places.size(); ++index) {
        const BodyFlowPlace& place = graph.places[index];
        const TypeHandle type = this->place_type(graph, place);
        const bool initialized_root = place.root_kind == BodyFlowPlaceRootKind::temporary;
        const bool needs_drop = this->type_needs_drop(type);
        PlaceStateFact fact;
        fact.place = base::checked_u32(index, SEMA_PLACE_STATE_ID_CONTEXT);
        fact.root_kind = place.root_kind;
        fact.root_name_id = place.root_name_id;
        fact.root_expr = place.root_expr;
        fact.type = type;
        fact.projection_count = static_cast<base::u64>(place.projections.size());
        fact.initialization =
            initialized_root ? PlaceStateInitialization::initialized : PlaceStateInitialization::unknown;
        fact.drop_state = initialized_root && needs_drop ? PlaceStateDropState::drop_pending : PlaceStateDropState::none;
        fact.needs_drop = needs_drop;
        fact.has_partial_projection = !place.projections.empty();
        this->facts_.places.push_back(fact);
    }
}

void SemanticAnalyzerCore::PlaceStateAnalyzer::collect_action_events(const BodyFlowGraph& graph)
{
    this->facts_.events.reserve(graph.actions.size());
    for (base::usize action_index = 0; action_index < graph.actions.size(); ++action_index) {
        const BodyFlowAction& action = graph.actions[action_index];
        if (action.place == SEMA_BODY_FLOW_INVALID_INDEX || action.place >= this->facts_.places.size()) {
            continue;
        }
        const std::optional<PlaceStateEventKind> kind = this->event_kind(action.kind);
        if (!kind.has_value()) {
            continue;
        }

        const base::u32 checked_action = base::checked_u32(action_index, SEMA_PLACE_STATE_ID_CONTEXT);
        const TypeHandle type = this->action_type(graph, action);
        this->facts_.events.push_back(PlaceStateEvent{
            .kind = *kind,
            .place = action.place,
            .action = checked_action,
            .point = action.point,
            .type = type,
            .range = action.range,
        });
        this->apply_event_to_fact(this->facts_.places[action.place], *kind, action);
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
            fact.initialization = PlaceStateInitialization::initialized;
            fact.move_state = PlaceStateMoveState::none;
            fact.drop_state = fact.needs_drop ? PlaceStateDropState::drop_pending : PlaceStateDropState::none;
            break;
        case PlaceStateEventKind::reinit:
            ++fact.reinit_count;
            fact.last_write_point = action.point;
            fact.initialization = PlaceStateInitialization::initialized;
            fact.move_state = PlaceStateMoveState::none;
            fact.drop_state = fact.needs_drop ? PlaceStateDropState::drop_pending : PlaceStateDropState::none;
            break;
        case PlaceStateEventKind::move_candidate:
            ++fact.move_candidate_count;
            fact.last_move_point = action.point;
            if (this->core_.cached_expr_owned_use_mode(action.expr) == OwnedUseMode::owned_consume) {
                fact.initialization = PlaceStateInitialization::uninitialized;
                fact.move_state = PlaceStateMoveState::maybe_moved;
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
            break;
        case PlaceStateEventKind::cleanup:
            ++fact.cleanup_count;
            fact.last_drop_point = action.point;
            fact.initialization = PlaceStateInitialization::uninitialized;
            fact.drop_state = PlaceStateDropState::dropped;
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

void SemanticAnalyzerCore::analyze_place_states(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    PlaceStateAnalyzer(*this).analyze(function, key, signature);
}

} // namespace aurex::sema
