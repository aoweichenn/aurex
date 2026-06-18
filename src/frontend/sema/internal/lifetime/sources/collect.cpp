#include <algorithm>
#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <frontend/sema/internal/lifetime/private/lifetime_analysis.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_LIFETIME_STATIC_ORIGIN_NAME = "static";
constexpr std::string_view SEMA_LIFETIME_SELF_ORIGIN_NAME = "self";
constexpr std::string_view SEMA_LIFETIME_LOCAL_REGION_NAME = "local";
constexpr std::string_view SEMA_LIFETIME_TEMPORARY_REGION_NAME = "temporary";
constexpr std::string_view SEMA_LIFETIME_UNKNOWN_REGION_NAME = "unknown";
constexpr std::string_view SEMA_LIFETIME_ORIGIN_KEY_SEPARATOR = " | ";
constexpr std::string_view SEMA_LIFETIME_REGION_KEY_SEPARATOR = "\x1F";
constexpr base::usize SEMA_LIFETIME_REGION_KEY_RESERVE_OVERHEAD = 32;
constexpr base::usize SEMA_LIFETIME_TYPE_STACK_CAPACITY = 32;

[[nodiscard]] bool valid_type_handle(const TypeTable& types, const TypeHandle type) noexcept
{
    return is_valid(type) && type.value < types.size();
}

[[nodiscard]] bool param_is_self(const syntax::ParamDecl& param) noexcept
{
    return param.name == SEMA_LIFETIME_SELF_ORIGIN_NAME;
}

[[nodiscard]] std::string region_lookup_key(
    const LifetimeRegionKind kind, const IdentId name_id, const std::string_view name, const base::u32 param_index)
{
    std::string key;
    key.reserve(name.size() + SEMA_LIFETIME_REGION_KEY_RESERVE_OVERHEAD);
    key.append(std::to_string(static_cast<base::u32>(kind)));
    key.append(SEMA_LIFETIME_REGION_KEY_SEPARATOR);
    key.append(std::to_string(name_id.value));
    key.append(SEMA_LIFETIME_REGION_KEY_SEPARATOR);
    key.append(std::to_string(param_index));
    key.append(SEMA_LIFETIME_REGION_KEY_SEPARATOR);
    key.append(name);
    return key;
}

[[nodiscard]] std::vector<std::string> split_origin_key(const std::string_view key)
{
    std::vector<std::string> names;
    if (key.empty()) {
        return names;
    }
    base::usize begin = 0;
    while (begin <= key.size()) {
        const base::usize separator = key.find(SEMA_LIFETIME_ORIGIN_KEY_SEPARATOR, begin);
        const base::usize end = separator == std::string_view::npos ? key.size() : separator;
        const std::string_view name = key.substr(begin, end - begin);
        if (!name.empty()) {
            names.push_back(std::string(name));
        }
        if (separator == std::string_view::npos) {
            break;
        }
        begin = separator + SEMA_LIFETIME_ORIGIN_KEY_SEPARATOR.size();
    }
    return names;
}

[[nodiscard]] bool same_lifetime_return_region(
    const LifetimeReturnRegion& lhs, const LifetimeReturnRegion& rhs) noexcept
{
    return lhs.region == rhs.region && lhs.return_expr.value == rhs.return_expr.value;
}

[[nodiscard]] bool same_lifetime_type_outlives_constraint(
    const LifetimeTypeOutlivesConstraint& lhs, const LifetimeTypeOutlivesConstraint& rhs) noexcept
{
    return lhs.type.value == rhs.type.value && lhs.region == rhs.region && lhs.reason == rhs.reason;
}

} // namespace

SemanticAnalyzerCore::LifetimeAnalyzer::LifetimeAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

std::size_t SemanticAnalyzerCore::LifetimeAnalyzer::ViolationKeyHash::operator()(
    const ViolationKey& key) const noexcept
{
    query::StableHashBuilder builder;
    builder.mix_u8(static_cast<base::u8>(key.kind));
    builder.mix_u32(key.region);
    builder.mix_u32(key.related_region);
    builder.mix_u32(key.type);
    builder.mix_u32(key.expr);
    return query::stable_hash_value(builder.finish());
}

void SemanticAnalyzerCore::LifetimeAnalyzer::analyze_signature(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    this->analyze(function, key, signature, false);
}

void SemanticAnalyzerCore::LifetimeAnalyzer::analyze_body(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    this->analyze(function, key, signature, true);
}

void SemanticAnalyzerCore::LifetimeAnalyzer::analyze(const syntax::ItemNode& function, const FunctionLookupKey& key,
    const FunctionSignature& signature, const bool include_body_facts)
{
    this->reset(function, key, signature, include_body_facts);
    this->collect_origin_params();
    this->collect_parameter_regions();
    this->collect_reference_origin_facts();
    this->collect_signature_type_lifetime_infos();
    this->collect_return_regions();
    this->collect_storage_escape_regions();
    this->solve();
    this->enforce_return_origin_subset();
    this->enforce_ambiguous_elision();
    this->enforce_type_outlives();
    this->enforce_diagnostics();
    this->finalize_facts();
}

void SemanticAnalyzerCore::LifetimeAnalyzer::reset(const syntax::ItemNode& function, const FunctionLookupKey& key,
    const FunctionSignature& signature, const bool include_body_facts)
{
    this->function_ = &function;
    this->signature_ = &signature;
    this->key_ = key;
    this->item_ = this->signature_item();
    this->include_body_facts_ = include_body_facts;
    this->declared_origin_regions_.clear();
    this->region_lookup_.clear();
    this->parameter_regions_.assign(signature.param_types.size(), SEMA_LIFETIME_INVALID_INDEX);
    this->outlives_successors_.clear();
    this->outlives_reachability_cache_.clear();
    this->outlives_reachability_cached_.clear();
    this->violation_lookup_.clear();
    this->type_borrow_cache_.clear();
    this->concrete_borrow_surface_cache_.clear();
    this->facts_ = FunctionLifetimeFacts{};
    this->facts_.function = key;
    this->facts_.return_type = signature.return_type;
    this->facts_.part_index = signature.part_index;
    this->facts_.diagnostic_mode_enforced = true;
}

syntax::ItemId SemanticAnalyzerCore::LifetimeAnalyzer::signature_item() const noexcept
{
    assert(this->signature_ != nullptr);
    if (syntax::is_valid(this->signature_->definition_item)) {
        return this->signature_->definition_item;
    }
    if (syntax::is_valid(this->signature_->prototype_item)) {
        return this->signature_->prototype_item;
    }
    return this->core_.state_.flow.current_item;
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::fact_belongs_to_current_item(const ReferenceOriginFact& fact) const noexcept
{
    return syntax::is_valid(this->item_) && fact.item.value == this->item_.value
        && fact.module.value == this->signature_->module.value;
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::has_declared_borrow_contract() const noexcept
{
    return this->function_->borrow_contract.present;
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::has_declared_origin_params() const noexcept
{
    return std::ranges::any_of(this->function_->generic_params, [](const syntax::GenericParamDecl& param) {
        return param.kind == syntax::GenericParamKind::origin;
    });
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::boundary_requires_explicit_contract() const noexcept
{
    return syntax::visibility_is_public(this->signature_->visibility) || this->signature_->is_extern_c
        || this->signature_->has_prototype || !this->signature_->has_definition;
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::function_has_body_flow_graph() const noexcept
{
    return this->core_.state_.checked.body_flow_graphs.find(this->key_)
        != this->core_.state_.checked.body_flow_graphs.end();
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::add_region(const LifetimeRegionKind kind, const IdentId name_id,
    const std::string_view name, const base::u32 param_index, const base::SourceRange& range)
{
    const std::string key = region_lookup_key(kind, name_id, name, param_index);
    if (const auto found = this->region_lookup_.find(key); found != this->region_lookup_.end()) {
        return found->second;
    }
    const base::u32 index = static_cast<base::u32>(this->facts_.regions.size());
    LifetimeRegion region;
    region.kind = kind;
    region.name_id = name_id;
    region.name = this->core_.state_.checked.intern_text(name);
    region.param_index = param_index;
    region.range = range;
    this->facts_.regions.push_back(region);
    this->region_lookup_.emplace(key, index);
    return index;
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::static_region()
{
    return this->add_region(LifetimeRegionKind::static_, INVALID_IDENT_ID, SEMA_LIFETIME_STATIC_ORIGIN_NAME,
        SEMA_LIFETIME_INVALID_INDEX, this->signature_->range);
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::unknown_region(const base::SourceRange& range)
{
    return this->add_region(
        LifetimeRegionKind::unknown, INVALID_IDENT_ID, SEMA_LIFETIME_UNKNOWN_REGION_NAME,
        SEMA_LIFETIME_INVALID_INDEX, range);
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::local_region(
    const IdentId name_id, const base::SourceRange& range)
{
    return this->add_region(
        LifetimeRegionKind::local, name_id, SEMA_LIFETIME_LOCAL_REGION_NAME, SEMA_LIFETIME_INVALID_INDEX, range);
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::temporary_region(const base::SourceRange& range)
{
    return this->add_region(LifetimeRegionKind::temporary, INVALID_IDENT_ID, SEMA_LIFETIME_TEMPORARY_REGION_NAME,
        SEMA_LIFETIME_INVALID_INDEX, range);
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::parameter_region(const base::usize index)
{
    if (index < this->parameter_regions_.size() && this->parameter_regions_[index] != SEMA_LIFETIME_INVALID_INDEX) {
        return this->parameter_regions_[index];
    }

    IdentId name_id = INVALID_IDENT_ID;
    std::string_view name;
    base::SourceRange range = this->signature_->range;
    LifetimeRegionKind kind = LifetimeRegionKind::parameter;
    if (index < this->function_->params.size()) {
        const syntax::ParamDecl& param = this->function_->params[index];
        name_id = param.name_id;
        name = param.name;
        range = param.range;
        if (param_is_self(param)) {
            kind = LifetimeRegionKind::self;
        }
    }
    const base::u32 region = this->add_region(kind, name_id, name, static_cast<base::u32>(index), range);
    if (index < this->parameter_regions_.size()) {
        this->parameter_regions_[index] = region;
    }
    return region;
}

std::optional<base::u32> SemanticAnalyzerCore::LifetimeAnalyzer::find_declared_origin(
    const std::string_view name) const
{
    const auto found = this->declared_origin_regions_.find(std::string(name));
    if (found == this->declared_origin_regions_.end()) {
        return std::nullopt;
    }
    return found->second;
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::declared_or_unknown_origin(
    const std::string_view name, const IdentId name_id, const base::SourceRange& range,
    const bool emit_unknown_violation)
{
    if (const std::optional<base::u32> declared = this->find_declared_origin(name)) {
        return *declared;
    }
    const base::u32 region = this->add_region(
        LifetimeRegionKind::unknown, name_id, name, SEMA_LIFETIME_INVALID_INDEX, range);
    if (emit_unknown_violation) {
        this->add_violation(LifetimeViolationKind::unknown_origin, region, SEMA_LIFETIME_INVALID_INDEX,
            INVALID_TYPE_HANDLE, syntax::INVALID_EXPR_ID, false, range);
    }
    return region;
}

void SemanticAnalyzerCore::LifetimeAnalyzer::register_declared_origin(
    const std::string_view name, const base::u32 region)
{
    this->declared_origin_regions_[std::string(name)] = region;
}

void SemanticAnalyzerCore::LifetimeAnalyzer::sort_unique(RegionSet& set) const
{
    std::ranges::sort(set.regions);
    set.regions.erase(std::ranges::unique(set.regions).begin(), set.regions.end());
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_origin_params()
{
    this->register_declared_origin(SEMA_LIFETIME_STATIC_ORIGIN_NAME, this->static_region());
    for (const syntax::GenericParamDecl& param : this->function_->generic_params) {
        if (param.kind != syntax::GenericParamKind::origin) {
            continue;
        }
        const base::u32 region = this->add_region(
            LifetimeRegionKind::explicit_origin, param.name_id, param.name, SEMA_LIFETIME_INVALID_INDEX, param.range);
        this->register_declared_origin(param.name, region);
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_parameter_regions()
{
    for (base::usize index = 0; index < this->signature_->param_types.size(); ++index) {
        if (this->type_can_contain_borrow(this->signature_->param_types[index])) {
            const base::u32 region = this->parameter_region(index);
            if (index < this->function_->params.size() && param_is_self(this->function_->params[index])) {
                this->register_declared_origin(SEMA_LIFETIME_SELF_ORIGIN_NAME, region);
            }
        }
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_reference_origin_facts()
{
    for (const ReferenceOriginFact& fact : this->core_.state_.checked.reference_origin_facts) {
        if (!this->fact_belongs_to_current_item(fact)) {
            continue;
        }
        const base::usize origin_count = fact.origin_names.size();
        for (base::usize index = 0; index < origin_count; ++index) {
            const std::string_view name = fact.origin_names[index].view();
            const IdentId name_id = index < fact.origin_name_ids.size() ? fact.origin_name_ids[index] : INVALID_IDENT_ID;
            const base::u32 region = this->declared_or_unknown_origin(name, name_id, fact.range, true);
            this->collect_reference_type_constraints(
                fact.semantic_type, region, LifetimeConstraintReason::reference_type, fact.range);
            this->record_type_lifetime_info(fact.semantic_type, LifetimeConstraintReason::reference_type, fact.range);
            this->record_generic_lifetime_predicate(
                fact.semantic_type, name, name_id, GenericLifetimePredicateSource::explicit_origin, fact.range);
        }
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_signature_type_lifetime_infos()
{
    for (base::usize index = 0; index < this->signature_->param_types.size(); ++index) {
        const base::SourceRange range =
            index < this->function_->params.size() ? this->function_->params[index].range : this->signature_->range;
        this->record_type_lifetime_info(
            this->signature_->param_types[index], LifetimeConstraintReason::reference_type, range);
    }
    this->record_type_lifetime_info(
        this->signature_->return_type, LifetimeConstraintReason::return_type, this->signature_->range);
}

void SemanticAnalyzerCore::LifetimeAnalyzer::record_type_lifetime_info(
    const TypeHandle type, const LifetimeConstraintReason reason, const base::SourceRange& range)
{
    static_cast<void>(reason);
    if (!valid_type_handle(this->core_.state_.checked.types, type)) {
        return;
    }
    const TypeInfo& type_info = this->core_.state_.checked.types.get(type);
    TypeLifetimeInfo info;
    info.type = type;
    info.can_contain_borrow = this->type_can_contain_borrow(type);
    if (!info.can_contain_borrow) {
        return;
    }
    info.has_concrete_borrow_surface = this->type_has_concrete_borrow_surface(type);
    info.part_index = this->signature_->part_index;
    std::vector<OriginName> origins = this->origin_names_for_type(type);
    std::ranges::sort(origins, [](const OriginName& lhs, const OriginName& rhs) {
        return std::tie(lhs.name, lhs.name_id.value) < std::tie(rhs.name, rhs.name_id.value);
    });
    origins.erase(std::ranges::unique(origins, [](const OriginName& lhs, const OriginName& rhs) {
        return lhs.name == rhs.name && lhs.name_id.value == rhs.name_id.value;
    }).begin(), origins.end());
    for (const OriginName& origin : origins) {
        info.origin_names.push_back(this->core_.state_.checked.intern_text(origin.name));
        this->record_generic_lifetime_predicate(
            type, origin.name, origin.name_id, GenericLifetimePredicateSource::explicit_origin, origin.range);
    }
    if (info.can_contain_borrow && origins.empty()) {
        const GenericLifetimePredicateSource source = type_info.kind == TypeKind::associated_projection
            ? GenericLifetimePredicateSource::associated_projection
            : GenericLifetimePredicateSource::inferred_reference;
        this->record_generic_lifetime_predicate(
            type, SEMA_LIFETIME_UNKNOWN_REGION_NAME, INVALID_IDENT_ID, source, range);
    }
    info.fingerprint = type_lifetime_info_fingerprint(info);
    SemanticAnalyzerCore::TypeLifetimeInfoIndexKey key{
        .type = info.type.value,
        .fingerprint = info.fingerprint,
    };
    if (this->core_.state_.lifetime_indexes.type_infos.insert(key).second) {
        this->core_.state_.checked.type_lifetime_infos.push_back(std::move(info));
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::record_generic_lifetime_predicate(const TypeHandle type,
    const std::string_view origin, const IdentId origin_id, const GenericLifetimePredicateSource source,
    const base::SourceRange& range)
{
    if (!valid_type_handle(this->core_.state_.checked.types, type)) {
        return;
    }
    GenericLifetimePredicate predicate;
    predicate.subject_type = type;
    predicate.origin_name = this->core_.state_.checked.intern_text(origin);
    predicate.origin_name_id = origin_id;
    predicate.source = source;
    predicate.range = range;
    predicate.part_index = this->signature_->part_index;
    predicate.fingerprint = generic_lifetime_predicate_fingerprint(predicate);
    SemanticAnalyzerCore::GenericLifetimePredicateIndexKey key{
        .subject_type = predicate.subject_type.value,
        .origin_name = std::string(predicate.origin_name.view()),
        .source = static_cast<base::u8>(predicate.source),
    };
    if (this->core_.state_.lifetime_indexes.generic_predicates.insert(std::move(key)).second) {
        this->core_.state_.checked.generic_lifetime_predicates.push_back(std::move(predicate));
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_reference_type_constraints(const TypeHandle reference_type,
    const base::u32 region, const LifetimeConstraintReason reason, const base::SourceRange& range)
{
    if (!valid_type_handle(this->core_.state_.checked.types, reference_type)) {
        return;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(reference_type);
    if (info.kind != TypeKind::reference) {
        return;
    }
    this->facts_.type_outlives_constraints.push_back(LifetimeTypeOutlivesConstraint{
        .type = info.pointee,
        .region = region,
        .reason = reason,
        .range = range,
    });
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_return_regions()
{
    const auto summary = this->core_.state_.checked.borrow_summaries.find(this->key_);
    if (this->include_body_facts_ && summary != this->core_.state_.checked.borrow_summaries.end()) {
        this->collect_return_regions_from_summary(summary->second);
        return;
    }
    const auto contract = this->core_.state_.checked.borrow_contracts.find(this->key_);
    if (contract != this->core_.state_.checked.borrow_contracts.end()) {
        this->collect_return_regions_from_contract(contract->second);
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_return_regions_from_summary(const FunctionBorrowSummary& summary)
{
    for (const FunctionBorrowReturnOrigin& returned : summary.return_origins) {
        if (returned.origin_index >= summary.origins.size()) {
            continue;
        }
        RegionSet regions = this->regions_for_summary_origin(summary.origins[returned.origin_index]);
        for (const base::u32 region : regions.regions) {
            this->append_return_region(region, returned.return_expr, returned.range);
        }
    }
    if (summary.has_unknown_return_origin && !this->return_regions_contain_unknown()) {
        this->append_unknown_return_region(syntax::INVALID_EXPR_ID, this->signature_->range);
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_return_regions_from_contract(const FunctionBorrowContract& contract)
{
    for (const BorrowContractSelector& selector : contract.return_selectors) {
        RegionSet regions = this->regions_for_contract_selector(selector);
        for (const base::u32 region : regions.regions) {
            this->append_return_region(region, syntax::INVALID_EXPR_ID, selector.range);
        }
    }
    if (contract.unknown_return_allowed && contract.return_selectors.empty()) {
        this->append_return_region(this->unknown_region(contract.range), syntax::INVALID_EXPR_ID, contract.range);
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_storage_escape_regions()
{
    if (!this->include_body_facts_) {
        return;
    }
    const auto summary = this->core_.state_.checked.borrow_summaries.find(this->key_);
    if (summary == this->core_.state_.checked.borrow_summaries.end()) {
        return;
    }
    for (const FunctionBorrowStorageEscape& escape : summary->second.storage_escapes) {
        this->collect_storage_escape_region(summary->second, escape);
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_storage_escape_region(
    const FunctionBorrowSummary& summary, const FunctionBorrowStorageEscape& escape)
{
    if (escape.origin_index >= summary.origins.size()) {
        return;
    }
    const BorrowSummaryOrigin& origin = summary.origins[escape.origin_index];
    switch (origin.kind) {
        case BorrowSummaryOriginKind::local: {
            const base::u32 region = this->local_region(origin.name_id, origin.range);
            this->add_violation(LifetimeViolationKind::local_escape, region, SEMA_LIFETIME_INVALID_INDEX,
                INVALID_TYPE_HANDLE, escape.stored_expr, false, escape.range);
            break;
        }
        case BorrowSummaryOriginKind::temporary: {
            const base::u32 region = this->temporary_region(origin.range);
            this->add_violation(LifetimeViolationKind::local_escape, region, SEMA_LIFETIME_INVALID_INDEX,
                INVALID_TYPE_HANDLE, escape.stored_expr, false, escape.range);
            break;
        }
        case BorrowSummaryOriginKind::parameter:
            if (origin.storage_slot) {
                const base::u32 region = this->local_region(origin.name_id, origin.range);
                this->add_violation(LifetimeViolationKind::local_escape, region, SEMA_LIFETIME_INVALID_INDEX,
                    INVALID_TYPE_HANDLE, escape.stored_expr, false, escape.range);
            }
            break;
        case BorrowSummaryOriginKind::unknown: {
            const base::u32 region = this->unknown_region(origin.range);
            this->add_violation(LifetimeViolationKind::unknown_escape, region, SEMA_LIFETIME_INVALID_INDEX,
                INVALID_TYPE_HANDLE, escape.stored_expr, false, escape.range);
            break;
        }
        case BorrowSummaryOriginKind::none:
        case BorrowSummaryOriginKind::static_:
            break;
    }
}

SemanticAnalyzerCore::LifetimeAnalyzer::RegionSet
SemanticAnalyzerCore::LifetimeAnalyzer::regions_for_summary_origin(const BorrowSummaryOrigin& origin)
{
    RegionSet result;
    switch (origin.kind) {
        case BorrowSummaryOriginKind::parameter:
            if (origin.storage_slot) {
                result.regions.push_back(this->local_region(origin.name_id, origin.range));
                this->add_violation(LifetimeViolationKind::local_escape, result.regions.back(),
                    SEMA_LIFETIME_INVALID_INDEX, INVALID_TYPE_HANDLE, origin.expr, false, origin.range);
            } else {
                result = this->regions_for_param_type_origin(origin.param_index);
                if (result.regions.empty()) {
                    result.regions.push_back(this->parameter_region(origin.param_index));
                }
            }
            break;
        case BorrowSummaryOriginKind::static_:
            result.regions.push_back(this->static_region());
            break;
        case BorrowSummaryOriginKind::local:
            result.regions.push_back(this->local_region(origin.name_id, origin.range));
            this->add_violation(LifetimeViolationKind::local_escape, result.regions.back(),
                SEMA_LIFETIME_INVALID_INDEX, INVALID_TYPE_HANDLE, origin.expr, false, origin.range);
            break;
        case BorrowSummaryOriginKind::temporary:
            result.regions.push_back(this->temporary_region(origin.range));
            this->add_violation(LifetimeViolationKind::local_escape, result.regions.back(),
                SEMA_LIFETIME_INVALID_INDEX, INVALID_TYPE_HANDLE, origin.expr, false, origin.range);
            break;
        case BorrowSummaryOriginKind::unknown:
            result.regions.push_back(this->unknown_region(origin.range));
            this->add_violation(LifetimeViolationKind::unknown_escape, result.regions.back(),
                SEMA_LIFETIME_INVALID_INDEX, INVALID_TYPE_HANDLE, origin.expr, false, origin.range);
            result.unknown = true;
            break;
        case BorrowSummaryOriginKind::none:
            break;
    }
    this->sort_unique(result);
    return result;
}

SemanticAnalyzerCore::LifetimeAnalyzer::RegionSet
SemanticAnalyzerCore::LifetimeAnalyzer::regions_for_contract_selector(const BorrowContractSelector& selector)
{
    RegionSet result;
    switch (selector.kind) {
        case BorrowContractSelectorKind::parameter:
            result = this->regions_for_param_type_origin(selector.param_index);
            if (result.regions.empty()) {
                result.regions.push_back(this->parameter_region(selector.param_index));
            }
            break;
        case BorrowContractSelectorKind::self:
            result.regions.push_back(this->parameter_region(selector.param_index));
            break;
        case BorrowContractSelectorKind::static_:
            result.regions.push_back(this->static_region());
            break;
        case BorrowContractSelectorKind::unknown:
            result.regions.push_back(this->unknown_region(selector.range));
            result.unknown = true;
            break;
    }
    this->sort_unique(result);
    return result;
}

SemanticAnalyzerCore::LifetimeAnalyzer::RegionSet
SemanticAnalyzerCore::LifetimeAnalyzer::regions_for_param_type_origin(const base::usize param_index)
{
    if (param_index >= this->signature_->param_types.size()) {
        return RegionSet{};
    }
    const base::SourceRange range =
        param_index < this->function_->params.size()
            ? this->function_->params[param_index].range
            : this->signature_->range;
    return this->regions_for_type_origin_key(this->signature_->param_types[param_index], range);
}

SemanticAnalyzerCore::LifetimeAnalyzer::RegionSet
SemanticAnalyzerCore::LifetimeAnalyzer::regions_for_type_origin_key(
    const TypeHandle type, const base::SourceRange& range)
{
    RegionSet result;
    for (const OriginName& origin : this->origin_names_for_type(type)) {
        result.regions.push_back(this->declared_or_unknown_origin(origin.name, origin.name_id, range, false));
    }
    this->sort_unique(result);
    return result;
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::return_regions_contain_unknown() const noexcept
{
    return std::ranges::any_of(this->facts_.return_regions, [this](const LifetimeReturnRegion& returned) {
        return returned.region < this->facts_.regions.size()
            && this->facts_.regions[returned.region].kind == LifetimeRegionKind::unknown;
    });
}

void SemanticAnalyzerCore::LifetimeAnalyzer::append_unknown_return_region(
    const syntax::ExprId expr, const base::SourceRange& range)
{
    const base::u32 region = this->unknown_region(range);
    this->append_return_region(region, expr, range);
    this->add_violation(LifetimeViolationKind::unknown_escape, region, SEMA_LIFETIME_INVALID_INDEX,
        INVALID_TYPE_HANDLE, expr, false, range);
}

std::vector<SemanticAnalyzerCore::LifetimeAnalyzer::OriginName>
SemanticAnalyzerCore::LifetimeAnalyzer::origin_names_for_type(const TypeHandle type) const
{
    std::vector<OriginName> result;
    const TypeTable& types = this->core_.state_.checked.types;
    if (!valid_type_handle(types, type)) {
        return result;
    }

    std::vector<TypeHandle> pending;
    std::unordered_set<base::u32> visited;
    pending.reserve(SEMA_LIFETIME_TYPE_STACK_CAPACITY);
    visited.reserve(SEMA_LIFETIME_TYPE_STACK_CAPACITY);
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!valid_type_handle(types, current) || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = types.get(current);
        switch (info.kind) {
            case TypeKind::reference:
                for (const std::string& name : split_origin_key(info.reference_origin_key.view())) {
                    result.push_back(OriginName{.name = name});
                }
                pending.push_back(info.pointee);
                break;
            case TypeKind::array:
                pending.push_back(info.array_element);
                break;
            case TypeKind::range:
                pending.push_back(info.range_element);
                break;
            case TypeKind::slice:
                pending.push_back(info.slice_element);
                break;
            case TypeKind::tuple:
                pending.insert(pending.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                break;
            case TypeKind::function:
                pending.insert(pending.end(), info.function_params.begin(), info.function_params.end());
                pending.push_back(info.function_return);
                break;
            case TypeKind::struct_: {
                if (const StructInfo* const structure = this->core_.find_struct(current); structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case TypeKind::enum_: {
                if (const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(current); cases != nullptr) {
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
            case TypeKind::opaque_struct:
            case TypeKind::generic_param:
            case TypeKind::associated_projection:
            case TypeKind::trait_object:
                break;
        }
    }
    return result;
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::type_can_contain_borrow(const TypeHandle type) const
{
    const TypeTable& types = this->core_.state_.checked.types;
    if (!valid_type_handle(types, type)) {
        return false;
    }
    if (const auto cached = this->type_borrow_cache_.find(type.value); cached != this->type_borrow_cache_.end()) {
        return cached->second;
    }

    std::vector<TypeHandle> pending;
    std::unordered_set<base::u32> visited;
    pending.reserve(SEMA_LIFETIME_TYPE_STACK_CAPACITY);
    visited.reserve(SEMA_LIFETIME_TYPE_STACK_CAPACITY);
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!valid_type_handle(types, current) || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = types.get(current);
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
                pending.push_back(info.array_element);
                break;
            case TypeKind::range:
                pending.push_back(info.range_element);
                break;
            case TypeKind::tuple:
                pending.insert(pending.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                break;
            case TypeKind::function:
                pending.insert(pending.end(), info.function_params.begin(), info.function_params.end());
                pending.push_back(info.function_return);
                break;
            case TypeKind::struct_: {
                if (const StructInfo* const structure = this->core_.find_struct(current); structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case TypeKind::enum_: {
                if (const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(current); cases != nullptr) {
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
            case TypeKind::opaque_struct:
            case TypeKind::trait_object:
                break;
        }
    }
    this->type_borrow_cache_[type.value] = false;
    return false;
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::type_has_concrete_borrow_surface(const TypeHandle type) const
{
    const TypeTable& types = this->core_.state_.checked.types;
    if (!valid_type_handle(types, type)) {
        return false;
    }
    if (const auto cached = this->concrete_borrow_surface_cache_.find(type.value);
        cached != this->concrete_borrow_surface_cache_.end()) {
        return cached->second;
    }

    std::vector<TypeHandle> pending;
    std::unordered_set<base::u32> visited;
    pending.reserve(SEMA_LIFETIME_TYPE_STACK_CAPACITY);
    visited.reserve(SEMA_LIFETIME_TYPE_STACK_CAPACITY);
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!valid_type_handle(types, current) || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = types.get(current);
        switch (info.kind) {
            case TypeKind::builtin:
                if (info.builtin == BuiltinType::str) {
                    this->concrete_borrow_surface_cache_[type.value] = true;
                    return true;
                }
                break;
            case TypeKind::reference:
            case TypeKind::slice:
                this->concrete_borrow_surface_cache_[type.value] = true;
                return true;
            case TypeKind::array:
                pending.push_back(info.array_element);
                break;
            case TypeKind::range:
                pending.push_back(info.range_element);
                break;
            case TypeKind::tuple:
                pending.insert(pending.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                break;
            case TypeKind::function:
                pending.insert(pending.end(), info.function_params.begin(), info.function_params.end());
                pending.push_back(info.function_return);
                break;
            case TypeKind::struct_: {
                if (const StructInfo* const structure = this->core_.find_struct(current); structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case TypeKind::enum_: {
                if (const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(current); cases != nullptr) {
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
            case TypeKind::opaque_struct:
            case TypeKind::trait_object:
                break;
        }
    }
    this->concrete_borrow_surface_cache_[type.value] = false;
    return false;
}

void SemanticAnalyzerCore::LifetimeAnalyzer::append_return_region(
    const base::u32 region, const syntax::ExprId expr, const base::SourceRange& range)
{
    this->facts_.return_regions.push_back(LifetimeReturnRegion{
        .region = region,
        .return_expr = expr,
        .range = range,
    });
}

void SemanticAnalyzerCore::LifetimeAnalyzer::finalize_facts()
{
    std::ranges::sort(this->facts_.type_outlives_constraints,
        [](const LifetimeTypeOutlivesConstraint& lhs, const LifetimeTypeOutlivesConstraint& rhs) {
            return std::tie(lhs.type.value, lhs.region, lhs.reason)
                < std::tie(rhs.type.value, rhs.region, rhs.reason);
        });
    this->facts_.type_outlives_constraints.erase(
        std::ranges::unique(this->facts_.type_outlives_constraints, same_lifetime_type_outlives_constraint).begin(),
        this->facts_.type_outlives_constraints.end());

    std::ranges::sort(this->facts_.live_ranges,
        [](const LifetimeRegionLiveRange& lhs, const LifetimeRegionLiveRange& rhs) {
            return std::tie(lhs.region, lhs.first_point, lhs.last_point, lhs.point_count)
                < std::tie(rhs.region, rhs.first_point, rhs.last_point, rhs.point_count);
        });
    this->facts_.live_ranges.erase(
        std::ranges::unique(this->facts_.live_ranges,
            [](const LifetimeRegionLiveRange& lhs, const LifetimeRegionLiveRange& rhs) {
                return lhs.region == rhs.region && lhs.first_point == rhs.first_point
                    && lhs.last_point == rhs.last_point && lhs.point_count == rhs.point_count;
            }).begin(),
        this->facts_.live_ranges.end());

    std::ranges::sort(this->facts_.return_regions,
        [](const LifetimeReturnRegion& lhs, const LifetimeReturnRegion& rhs) {
            return std::tie(lhs.region, lhs.return_expr.value) < std::tie(rhs.region, rhs.return_expr.value);
        });
    this->facts_.return_regions.erase(
        std::ranges::unique(this->facts_.return_regions, same_lifetime_return_region).begin(),
        this->facts_.return_regions.end());

    std::ranges::sort(this->facts_.violations,
        [](const LifetimeViolation& lhs, const LifetimeViolation& rhs) {
            return std::tie(lhs.kind, lhs.region, lhs.related_region, lhs.type.value, lhs.expr.value,
                       lhs.diagnostic_emitted)
                < std::tie(rhs.kind, rhs.region, rhs.related_region, rhs.type.value, rhs.expr.value,
                    rhs.diagnostic_emitted);
        });
    this->facts_.violations.erase(
        std::ranges::unique(this->facts_.violations, [](const LifetimeViolation& lhs, const LifetimeViolation& rhs) {
            return lhs.kind == rhs.kind && lhs.region == rhs.region && lhs.related_region == rhs.related_region
                && lhs.type.value == rhs.type.value && lhs.expr.value == rhs.expr.value
                && lhs.diagnostic_emitted == rhs.diagnostic_emitted;
        }).begin(),
        this->facts_.violations.end());

    this->facts_.fingerprint = function_lifetime_facts_fingerprint(this->facts_);
    this->core_.state_.checked.lifetime_facts[this->key_] = std::move(this->facts_);
}

} // namespace aurex::sema
