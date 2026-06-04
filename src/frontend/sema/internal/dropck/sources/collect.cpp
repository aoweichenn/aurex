#include <aurex/infrastructure/query/stable_hash.hpp>

#include <algorithm>
#include <optional>
#include <utility>

#include <frontend/sema/internal/dropck/private/dropck_analysis.hpp>

namespace aurex::sema {
namespace {

constexpr base::usize SEMA_DROPCK_STATEMENT_STACK_INITIAL_CAPACITY = 32;
constexpr base::usize SEMA_DROPCK_PATTERN_STACK_INITIAL_CAPACITY = 16;
constexpr base::usize SEMA_DROPCK_TYPE_STACK_INITIAL_CAPACITY = 32;

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

[[nodiscard]] bool conflict_is_dropck_action(const BodyLoanConflictKind kind) noexcept
{
    return kind == BodyLoanConflictKind::drop || kind == BodyLoanConflictKind::cleanup
        || kind == BodyLoanConflictKind::reinit;
}

} // namespace

std::size_t SemanticAnalyzerCore::DropCheckAnalyzer::TypeOutlivesKeyHash::operator()(
    const TypeOutlivesKey& key) const noexcept
{
    query::StableHashBuilder builder;
    builder.mix_u32(key.type);
    builder.mix_u32(key.region);
    return query::stable_hash_value(builder.finish());
}

std::size_t SemanticAnalyzerCore::DropCheckAnalyzer::DropCheckViolationKeyHash::operator()(
    const DropCheckViolationKey& key) const noexcept
{
    query::StableHashBuilder builder;
    builder.mix_u8(key.kind);
    builder.mix_u32(key.action);
    builder.mix_u32(key.point);
    builder.mix_u32(key.place);
    builder.mix_u32(key.type);
    builder.mix_u32(key.region);
    return query::stable_hash_value(builder.finish());
}

SemanticAnalyzerCore::DropCheckAnalyzer::DropCheckAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

void SemanticAnalyzerCore::DropCheckAnalyzer::reset(
    const FunctionLookupKey& key, const FunctionSignature& signature)
{
    this->signature_ = &signature;
    this->key_ = key;
    this->facts_ = FunctionDropCheckFacts{};
    this->facts_.function = key;
    this->facts_.diagnostic_mode_enforced = true;
    this->facts_.part_index = signature.part_index;
    this->local_types_by_name_.clear();
    this->drop_glue_cache_.clear();
    this->fact_by_type_.clear();
    this->type_borrow_cache_.clear();
    this->emitted_type_outlives_.clear();
    this->violation_dedupe_.clear();
    this->active_regions_by_point_.clear();
    this->lifetime_region_by_origin_name_.clear();
    this->concrete_origin_regions_by_type_.clear();
}

void SemanticAnalyzerCore::DropCheckAnalyzer::collect(const syntax::ItemNode& function)
{
    this->build_local_type_index(function);
    if (!this->local_dropck_inputs_may_need_graph()) {
        return;
    }

    if (this->body_graph() == nullptr && syntax::is_valid(function.body)) {
        this->core_.collect_body_flow_graph(function, this->key_);
    }

    const BodyFlowGraph* const graph = this->body_graph();
    if (graph == nullptr) {
        this->facts_.graph_missing = true;
        return;
    }

    this->facts_.actions.reserve(graph->actions.size());
    for (base::usize action_index = 0; action_index < graph->actions.size(); ++action_index) {
        const BodyFlowAction& action = graph->actions[action_index];
        const std::optional<DropCheckActionKind> kind = this->action_kind(action.kind);
        if (!kind.has_value()) {
            continue;
        }

        const TypeHandle type = this->action_type(*graph, action);
        if (!this->valid_type(type)) {
            continue;
        }

        const DropGlueCacheEntry& cache = this->cached_drop_glue(type);
        const query::StableFingerprint128 destructor_key = cache.plan.fingerprint;
        this->append_drop_action(*kind, static_cast<base::u32>(action_index), action, type, destructor_key);
        if (drop_glue_plan_needs_drop(cache.plan)) {
            this->append_drop_fact(type, cache.plan);
        }
    }
}

bool SemanticAnalyzerCore::DropCheckAnalyzer::local_dropck_inputs_may_need_graph()
{
    if (const auto loan = this->core_.state_.checked.body_loan_checks.find(this->key_);
        loan != this->core_.state_.checked.body_loan_checks.end()) {
        for (const BodyLoanConflict& conflict : loan->second.conflicts) {
            if (conflict_is_dropck_action(conflict.kind)) {
                return true;
            }
        }
    }

    for (const auto& entry : this->local_types_by_name_) {
        const TypeHandle type = entry.second;
        if (!this->valid_type(type)) {
            continue;
        }
        const DropGlueCacheEntry& cache = this->cached_drop_glue(type);
        if (drop_glue_plan_needs_drop(cache.plan)) {
            return true;
        }
    }
    return false;
}

void SemanticAnalyzerCore::DropCheckAnalyzer::build_local_type_index(const syntax::ItemNode& function)
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
    pending.reserve(SEMA_DROPCK_STATEMENT_STACK_INITIAL_CAPACITY);
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

void SemanticAnalyzerCore::DropCheckAnalyzer::index_statement_locals(const syntax::StmtId stmt)
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

void SemanticAnalyzerCore::DropCheckAnalyzer::index_pattern_bindings(
    const syntax::PatternId pattern, const TypeHandle type)
{
    std::vector<PatternTypeFrame> pending;
    pending.reserve(SEMA_DROPCK_PATTERN_STACK_INITIAL_CAPACITY);
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

TypeHandle SemanticAnalyzerCore::DropCheckAnalyzer::pattern_field_type(
    const TypeHandle owner, const IdentId field_name_id) const noexcept
{
    const StructInfo* const structure = this->core_.find_struct(owner);
    if (structure == nullptr) {
        return INVALID_TYPE_HANDLE;
    }
    const StructFieldInfo* const field = find_struct_field(*structure, field_name_id);
    return field == nullptr ? INVALID_TYPE_HANDLE : field->type;
}

TypeHandle SemanticAnalyzerCore::DropCheckAnalyzer::pattern_element_type(
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

const EnumCaseInfo* SemanticAnalyzerCore::DropCheckAnalyzer::pattern_enum_case(
    const TypeHandle owner, const syntax::PatternNode& pattern) const
{
    return this->core_.find_enum_case_by_type_and_case(owner, pattern.case_name_id, pattern.case_name);
}

const BodyFlowGraph* SemanticAnalyzerCore::DropCheckAnalyzer::body_graph() const noexcept
{
    const auto found = this->core_.state_.checked.body_flow_graphs.find(this->key_);
    return found == this->core_.state_.checked.body_flow_graphs.end() ? nullptr : &found->second;
}

const FunctionLifetimeFacts* SemanticAnalyzerCore::DropCheckAnalyzer::lifetime_facts() const noexcept
{
    const auto found = this->core_.state_.checked.lifetime_facts.find(this->key_);
    return found == this->core_.state_.checked.lifetime_facts.end() ? nullptr : &found->second;
}

FunctionLifetimeFacts* SemanticAnalyzerCore::DropCheckAnalyzer::mutable_lifetime_facts() noexcept
{
    const auto found = this->core_.state_.checked.lifetime_facts.find(this->key_);
    return found == this->core_.state_.checked.lifetime_facts.end() ? nullptr : &found->second;
}

TypeHandle SemanticAnalyzerCore::DropCheckAnalyzer::action_type(
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

TypeHandle SemanticAnalyzerCore::DropCheckAnalyzer::place_type(
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

TypeHandle SemanticAnalyzerCore::DropCheckAnalyzer::projected_type(
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

std::optional<DropCheckActionKind> SemanticAnalyzerCore::DropCheckAnalyzer::action_kind(
    const BodyFlowActionKind kind) const noexcept
{
    switch (kind) {
        case BodyFlowActionKind::drop:
            return DropCheckActionKind::explicit_drop;
        case BodyFlowActionKind::cleanup_storage:
            return DropCheckActionKind::lexical_cleanup;
        case BodyFlowActionKind::reinit:
            return DropCheckActionKind::overwrite;
        case BodyFlowActionKind::cleanup_scope:
            return DropCheckActionKind::defer_cleanup;
        case BodyFlowActionKind::return_:
            return DropCheckActionKind::early_exit;
        case BodyFlowActionKind::read:
        case BodyFlowActionKind::write:
        case BodyFlowActionKind::move_candidate:
        case BodyFlowActionKind::borrow_shared:
        case BodyFlowActionKind::borrow_mutable:
        case BodyFlowActionKind::call_receiver_reserve:
        case BodyFlowActionKind::call_receiver_activate:
        case BodyFlowActionKind::call:
        case BodyFlowActionKind::branch:
            return std::nullopt;
    }
    return std::nullopt;
}

const SemanticAnalyzerCore::DropCheckAnalyzer::DropGlueCacheEntry&
SemanticAnalyzerCore::DropCheckAnalyzer::cached_drop_glue(const TypeHandle type)
{
    const auto found = this->drop_glue_cache_.find(type.value);
    if (found != this->drop_glue_cache_.end()) {
        return found->second;
    }
    DropGlueCacheEntry entry;
    if (base::Result<DropGluePlan> plan = build_drop_glue_plan(this->core_.state_.checked, type)) {
        entry.plan = std::move(plan.value());
    }
    const auto inserted = this->drop_glue_cache_.emplace(type.value, std::move(entry));
    return inserted.first->second;
}

bool SemanticAnalyzerCore::DropCheckAnalyzer::type_can_contain_borrow(const TypeHandle type) const
{
    if (!this->valid_type(type)) {
        return false;
    }
    if (const auto cached = this->type_borrow_cache_.find(type.value); cached != this->type_borrow_cache_.end()) {
        return cached->second;
    }

    std::vector<TypeFrame> pending;
    std::unordered_set<base::u32> visited;
    pending.reserve(SEMA_DROPCK_TYPE_STACK_INITIAL_CAPACITY);
    pending.push_back(TypeFrame{type});
    while (!pending.empty()) {
        const TypeFrame frame = pending.back();
        pending.pop_back();
        if (!this->valid_type(frame.type) || !visited.insert(frame.type.value).second) {
            continue;
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(frame.type);
        switch (info.kind) {
            case TypeKind::builtin:
                if (info.builtin == BuiltinType::str) {
                    this->type_borrow_cache_[type.value] = true;
                    return true;
                }
                break;
            case TypeKind::reference:
            case TypeKind::slice:
            case TypeKind::generic_param:
            case TypeKind::associated_projection:
                this->type_borrow_cache_[type.value] = true;
                return true;
            case TypeKind::array:
                pending.push_back(TypeFrame{info.array_element});
                break;
            case TypeKind::tuple:
                for (const TypeHandle element : info.tuple_elements) {
                    pending.push_back(TypeFrame{element});
                }
                break;
            case TypeKind::function:
                for (const TypeHandle param : info.function_params) {
                    pending.push_back(TypeFrame{param});
                }
                pending.push_back(TypeFrame{info.function_return});
                break;
            case TypeKind::struct_:
                if (const StructInfo* const structure = this->core_.find_struct(frame.type); structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(TypeFrame{field.type});
                    }
                }
                break;
            case TypeKind::enum_:
                if (const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(frame.type);
                    cases != nullptr) {
                    for (const EnumCaseInfo* const enum_case : *cases) {
                        if (enum_case != nullptr) {
                            for (const TypeHandle payload : enum_case->payload_types) {
                                pending.push_back(TypeFrame{payload});
                            }
                        }
                    }
                }
                break;
            case TypeKind::pointer:
            case TypeKind::opaque_struct:
                break;
        }
    }
    this->type_borrow_cache_[type.value] = false;
    return false;
}

bool SemanticAnalyzerCore::DropCheckAnalyzer::valid_type(const TypeHandle type) const noexcept
{
    return is_valid(type) && type.value < this->core_.state_.checked.types.size();
}

void SemanticAnalyzerCore::DropCheckAnalyzer::append_drop_action(const DropCheckActionKind kind,
    const base::u32 action_index, const BodyFlowAction& action, const TypeHandle type,
    const query::StableFingerprint128& destructor_key)
{
    this->facts_.actions.push_back(DropActionFact{
        .kind = kind,
        .action = action_index,
        .point = action.point,
        .place = action.place,
        .type = type,
        .destructor_key = destructor_key,
        .range = action.range,
    });
}

void SemanticAnalyzerCore::DropCheckAnalyzer::append_drop_fact(const TypeHandle type, const DropGluePlan& plan)
{
    if (this->fact_by_type_.contains(type.value)) {
        return;
    }
    const base::usize index = this->facts_.facts.size();
    this->fact_by_type_.emplace(type.value, index);
    this->facts_.facts.push_back(DropCheckFact{
        .type = type,
        .destructor_function = {},
        .required_outlives = {},
        .drop_glue_fingerprint = plan.fingerprint,
        .fingerprint = {},
        .may_observe_fields = !plan.steps.empty(),
        .may_move_fields = false,
    });
}

} // namespace aurex::sema
