#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/resource_semantics.hpp>
#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/principal_set_composition_facts.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string_view>
#include <tuple>
#include <utility>

namespace aurex::sema {

namespace {

constexpr std::size_t SEMA_TRAIT_IMPL_HASH_MIX = 0x9e3779b97f4a7c15ULL;
constexpr base::usize SEMA_TRAIT_IMPL_HASH_LEFT_SHIFT = 6;
constexpr base::usize SEMA_TRAIT_IMPL_HASH_RIGHT_SHIFT = 2;
constexpr std::string_view SEMA_TRAIT_METHOD_CALL_BINDING_ID_CONTEXT = "sema trait method call binding id";
constexpr std::string_view SEMA_FUNCTION_CALL_BINDING_ID_CONTEXT = "sema function call binding id";
constexpr std::string_view SEMA_DESTRUCTOR_INFO_FINGERPRINT_MARKER = "sema.destructor.info.v1";
constexpr std::string_view SEMA_DESTRUCTORS_FINGERPRINT_MARKER = "sema.destructors.v1";
constexpr std::string_view SEMA_MOVE_REJECTION_FACTS_FINGERPRINT_MARKER = "sema.move_rejection.facts.v1";
constexpr std::string_view SEMA_TRAIT_OBJECT_FACTS_FINGERPRINT_MARKER = "sema.trait_object.facts.v1";

[[nodiscard]] std::size_t mix_trait_impl_hash(std::size_t hash, const std::uint64_t value) noexcept
{
    hash ^= static_cast<std::size_t>(value) + SEMA_TRAIT_IMPL_HASH_MIX + (hash << SEMA_TRAIT_IMPL_HASH_LEFT_SHIFT)
        + (hash >> SEMA_TRAIT_IMPL_HASH_RIGHT_SHIFT);
    return hash;
}

void mix_function_lookup_key(query::StableHashBuilder& builder, const FunctionLookupKey key) noexcept
{
    builder.mix_u32(key.module);
    builder.mix_u32(key.owner_type);
    builder.mix_u32(key.name.value);
}

} // namespace

std::string_view owned_use_mode_name(const OwnedUseMode mode) noexcept
{
    switch (mode) {
        case OwnedUseMode::none:
            return "none";
        case OwnedUseMode::owned_copy:
            return "owned_copy";
        case OwnedUseMode::owned_consume:
            return "owned_consume";
        case OwnedUseMode::shared_borrow:
            return "shared_borrow";
        case OwnedUseMode::mutable_borrow:
            return "mutable_borrow";
        case OwnedUseMode::place_only:
            return "place_only";
    }
    return "<invalid>";
}

std::string_view for_in_iteration_kind_name(const ForInIterationKind kind) noexcept
{
    switch (kind) {
        case ForInIterationKind::none:
            return "none";
        case ForInIterationKind::counted_range:
            return "counted_range";
        case ForInIterationKind::array_value:
            return "array_value";
        case ForInIterationKind::slice_value:
            return "slice_value";
        case ForInIterationKind::protocol_iterator:
            return "protocol_iterator";
    }
    return "<invalid>";
}

std::string_view for_in_item_mode_name(const ForInItemMode mode) noexcept
{
    switch (mode) {
        case ForInItemMode::immutable_value_copy:
            return "immutable_value_copy";
    }
    return "<invalid>";
}

std::string_view for_in_protocol_source_kind_name(const ForInProtocolSourceKind kind) noexcept
{
    switch (kind) {
        case ForInProtocolSourceKind::direct_iterator:
            return "direct_iterator";
        case ForInProtocolSourceKind::iter_method:
            return "iter_method";
    }
    return "<invalid>";
}

std::string_view for_in_protocol_call_kind_name(const ForInProtocolCallKind kind) noexcept
{
    switch (kind) {
        case ForInProtocolCallKind::none:
            return "none";
        case ForInProtocolCallKind::inherent_method:
            return "inherent_method";
        case ForInProtocolCallKind::trait_static_method:
            return "trait_static_method";
    }
    return "<invalid>";
}

std::string_view receiver_access_kind_name(const ReceiverAccessKind kind) noexcept
{
    switch (kind) {
        case ReceiverAccessKind::none:
            return "none";
        case ReceiverAccessKind::shared:
            return "shared";
        case ReceiverAccessKind::mutable_:
            return "mutable";
        case ReceiverAccessKind::consuming:
            return "consuming";
    }
    return "<invalid>";
}

std::size_t TraitImplLookupKeyHash::operator()(const TraitImplLookupKey key) const noexcept
{
    std::size_t hash = static_cast<std::size_t>(key.trait_module);
    hash = mix_trait_impl_hash(hash, static_cast<std::uint64_t>(key.trait_name.value));
    hash = mix_trait_impl_hash(hash, key.self_type);
    hash = mix_trait_impl_hash(hash, key.trait_args.primary);
    hash = mix_trait_impl_hash(hash, key.trait_args.secondary);
    hash = mix_trait_impl_hash(hash, key.trait_args.byte_count);
    return hash;
}

PatternCaseNameTable::PatternCaseNameTable() : names_()
{
}

PatternCaseNameTable::PatternCaseNameTable(const PatternCaseNameTable& other) : PatternCaseNameTable()
{
    this->copy_from(other);
}

PatternCaseNameTable& PatternCaseNameTable::operator=(const PatternCaseNameTable& other)
{
    if (this == &other) {
        return *this;
    }
    PatternCaseNameTable copy(other);
    *this = std::move(copy);
    return *this;
}

PatternCaseNameTable::PatternCaseNameTable(PatternCaseNameTable&& other) noexcept : PatternCaseNameTable()
{
    this->swap(other);
}

PatternCaseNameTable& PatternCaseNameTable::operator=(PatternCaseNameTable&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

bool PatternCaseNameTable::empty() const noexcept
{
    return this->names_.empty();
}

base::usize PatternCaseNameTable::size() const noexcept
{
    return this->names_.size();
}

bool PatternCaseNameTable::contains(const base::u32 pattern) const
{
    return this->names_.contains(pattern);
}

void PatternCaseNameTable::reserve(const base::usize pattern_count)
{
    if (pattern_count == 0) {
        return;
    }
    this->ensure_storage();
    this->names_.reserve(pattern_count);
}

void PatternCaseNameTable::clear() noexcept
{
    this->names_.clear();
}

PatternCaseNameTable::iterator PatternCaseNameTable::begin() noexcept
{
    return this->names_.begin();
}

PatternCaseNameTable::iterator PatternCaseNameTable::end() noexcept
{
    return this->names_.end();
}

PatternCaseNameTable::const_iterator PatternCaseNameTable::begin() const noexcept
{
    return this->names_.begin();
}

PatternCaseNameTable::const_iterator PatternCaseNameTable::end() const noexcept
{
    return this->names_.end();
}

PatternCaseNameTable::const_iterator PatternCaseNameTable::find(const base::u32 pattern) const
{
    return this->names_.find(pattern);
}

PatternCaseNameTable::iterator PatternCaseNameTable::find(const base::u32 pattern)
{
    return this->names_.find(pattern);
}

CNameIdSet& PatternCaseNameTable::operator[](const base::u32 pattern)
{
    this->ensure_storage();
    if (const auto found = this->names_.find(pattern); found != this->names_.end()) {
        return found->second;
    }
    const auto inserted = this->names_.emplace(pattern, this->make_bucket());
    return inserted.first->second;
}

void PatternCaseNameTable::insert(const base::u32 pattern, const IdentId c_name_id)
{
    static_cast<void>((*this)[pattern].insert(c_name_id));
}

void PatternCaseNameTable::merge(const base::u32 pattern, const CNameIdSet& source)
{
    if (source.empty()) {
        return;
    }
    CNameIdSet& target = (*this)[pattern];
    target.insert(source.begin(), source.end());
}

base::usize PatternCaseNameTable::arena_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize PatternCaseNameTable::arena_blocks() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

CNameIdSet PatternCaseNameTable::make_bucket()
{
    this->ensure_storage();
    return make_sema_set<IdentId, IdentIdHash>(*this->arena_, IdentIdHash{});
}

void PatternCaseNameTable::ensure_storage()
{
    if (this->arena_ != nullptr) {
        return;
    }
    this->arena_ = std::make_unique<base::BumpAllocator>(SEMA_PATTERN_CASE_NAME_TABLE_BLOCK_BYTES);
    this->names_ = make_sema_map<base::u32, CNameIdSet>(*this->arena_);
}

void PatternCaseNameTable::swap(PatternCaseNameTable& other) noexcept
{
    using std::swap;
    swap(this->arena_, other.arena_);
    this->names_.swap(other.names_);
}

void PatternCaseNameTable::copy_from(const PatternCaseNameTable& other)
{
    if (other.empty()) {
        return;
    }
    this->reserve(other.size());
    for (const auto& entry : other) {
        CNameIdSet& bucket = (*this)[entry.first];
        bucket.reserve(entry.second.size());
        bucket.insert(entry.second.begin(), entry.second.end());
    }
}

GenericSideTables::GenericSideTables()
    : arena_(std::make_unique<base::BumpAllocator>(SEMA_GENERIC_SIDE_TABLE_BLOCK_BYTES)),
      expr_node_ids(make_sema_vector<base::u32>(*this->arena_)),
      pattern_node_ids(make_sema_vector<base::u32>(*this->arena_)),
      type_node_ids(make_sema_vector<base::u32>(*this->arena_)),
      stmt_node_ids(make_sema_vector<base::u32>(*this->arena_)),
      expr_intrinsic_types(make_sema_vector<TypeHandle>(*this->arena_)),
      expr_types(make_sema_vector<TypeHandle>(*this->arena_)),
      expr_owned_use_modes(make_sema_vector<OwnedUseMode>(*this->arena_)),
      expr_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      pattern_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      syntax_type_handles(make_sema_vector<TypeHandle>(*this->arena_)),
      stmt_local_types(make_sema_vector<TypeHandle>(*this->arena_)),
      sparse_expr_intrinsic_types(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
      sparse_expr_types(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
      sparse_expr_owned_use_modes(make_sema_map<base::u32, OwnedUseMode>(*this->arena_)),
      sparse_expr_c_name_ids(make_sema_map<base::u32, IdentId>(*this->arena_)),
      sparse_pattern_c_name_ids(make_sema_map<base::u32, IdentId>(*this->arena_)),
      sparse_syntax_type_handles(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
      sparse_stmt_local_types(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
      for_in_iteration_plans(make_sema_map<base::u32, ForInIterationPlan>(*this->arena_))
{
}

GenericSideTables::GenericSideTables(const GenericSideTables& other) : GenericSideTables()
{
    this->copy_from(other);
}

GenericSideTables& GenericSideTables::operator=(const GenericSideTables& other)
{
    if (this == &other) {
        return *this;
    }
    GenericSideTables copy(other);
    *this = std::move(copy);
    return *this;
}

GenericSideTables::GenericSideTables(GenericSideTables&& other) noexcept : GenericSideTables()
{
    this->swap(other);
}

GenericSideTables& GenericSideTables::operator=(GenericSideTables&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

base::usize GenericSideTables::arena_bytes() const noexcept
{
    return (this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes())
        + (this->analysis_arena_ == nullptr ? 0 : this->analysis_arena_->allocated_bytes());
}

base::usize GenericSideTables::arena_blocks() const noexcept
{
    return (this->arena_ == nullptr ? 0 : this->arena_->block_count())
        + (this->analysis_arena_ == nullptr ? 0 : this->analysis_arena_->block_count());
}

void GenericSideTables::record_sparse_fallback(const GenericSparseFallbackKind kind) noexcept
{
    switch (kind) {
        case GenericSparseFallbackKind::expr_intrinsic_type:
            this->sparse_fallbacks.expr_intrinsic_types += 1;
            break;
        case GenericSparseFallbackKind::expr_type:
            this->sparse_fallbacks.expr_types += 1;
            break;
        case GenericSparseFallbackKind::expr_expected_type:
            this->sparse_fallbacks.expr_expected_types += 1;
            break;
        case GenericSparseFallbackKind::expr_owned_use_mode:
            this->sparse_fallbacks.expr_owned_use_modes += 1;
            break;
        case GenericSparseFallbackKind::expr_c_name:
            this->sparse_fallbacks.expr_c_name_ids += 1;
            break;
        case GenericSparseFallbackKind::pattern_c_name:
            this->sparse_fallbacks.pattern_c_name_ids += 1;
            break;
        case GenericSparseFallbackKind::pattern_case_name:
            this->sparse_fallbacks.pattern_case_name_ids += 1;
            break;
        case GenericSparseFallbackKind::syntax_type:
            this->sparse_fallbacks.syntax_type_handles += 1;
            break;
        case GenericSparseFallbackKind::stmt_local_type:
            this->sparse_fallbacks.stmt_local_types += 1;
            break;
    }
}

void GenericSideTables::configure_local_dense(
    const GenericNodeSpan expr, const GenericNodeSpan pattern, const GenericNodeSpan type, const GenericNodeSpan stmt)
{
    this->configure_local_dense(GenericSideTableLocalLayoutView{
        expr,
        pattern,
        type,
        stmt,
    });
}

void GenericSideTables::configure_local_dense(const GenericSideTableLocalLayoutView& local_layout)
{
    this->sparse = true;
    this->local_dense = true;
    this->expr_span = local_layout.expr_span;
    this->pattern_span = local_layout.pattern_span;
    this->type_span = local_layout.type_span;
    this->stmt_span = local_layout.stmt_span;
    this->layout = nullptr;
    this->expr_node_ids.assign(local_layout.expr_node_ids.begin(), local_layout.expr_node_ids.end());
    this->pattern_node_ids.assign(local_layout.pattern_node_ids.begin(), local_layout.pattern_node_ids.end());
    this->type_node_ids.assign(local_layout.type_node_ids.begin(), local_layout.type_node_ids.end());
    this->stmt_node_ids.assign(local_layout.stmt_node_ids.begin(), local_layout.stmt_node_ids.end());
    const base::usize expr_count =
        this->expr_node_ids.empty() ? local_layout.expr_span.count : this->expr_node_ids.size();
    const base::usize pattern_count =
        this->pattern_node_ids.empty() ? local_layout.pattern_span.count : this->pattern_node_ids.size();
    const base::usize type_count =
        this->type_node_ids.empty() ? local_layout.type_span.count : this->type_node_ids.size();
    const base::usize stmt_count =
        this->stmt_node_ids.empty() ? local_layout.stmt_span.count : this->stmt_node_ids.size();
    this->expr_intrinsic_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_owned_use_modes.clear();
    this->prepare_analysis_only_storage(expr_count);
    this->expr_expected_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_c_name_ids.assign(expr_count, INVALID_IDENT_ID);
    this->pattern_c_name_ids.assign(pattern_count, INVALID_IDENT_ID);
    this->syntax_type_handles.assign(type_count, INVALID_TYPE_HANDLE);
    this->stmt_local_types.assign(stmt_count, INVALID_TYPE_HANDLE);
    this->sparse_expr_intrinsic_types.clear();
    this->sparse_expr_types.clear();
    this->sparse_expr_owned_use_modes.clear();
    this->sparse_expr_c_name_ids.clear();
    this->sparse_pattern_c_name_ids.clear();
    this->sparse_syntax_type_handles.clear();
    this->sparse_stmt_local_types.clear();
    this->for_in_iteration_plans.clear();
    this->sparse_fallbacks = {};
}

void GenericSideTables::configure_local_dense(const GenericSideTableLayout& shared_layout)
{
    this->sparse = true;
    this->local_dense = true;
    this->expr_span = shared_layout.expr_span;
    this->pattern_span = shared_layout.pattern_span;
    this->type_span = shared_layout.type_span;
    this->stmt_span = shared_layout.stmt_span;
    this->layout = &shared_layout;
    this->expr_node_ids.clear();
    this->pattern_node_ids.clear();
    this->type_node_ids.clear();
    this->stmt_node_ids.clear();
    const base::usize expr_count =
        shared_layout.expr_node_ids.empty() ? shared_layout.expr_span.count : shared_layout.expr_node_ids.size();
    const base::usize pattern_count = shared_layout.pattern_node_ids.empty() ? shared_layout.pattern_span.count
                                                                             : shared_layout.pattern_node_ids.size();
    const base::usize type_count =
        shared_layout.type_node_ids.empty() ? shared_layout.type_span.count : shared_layout.type_node_ids.size();
    const base::usize stmt_count =
        shared_layout.stmt_node_ids.empty() ? shared_layout.stmt_span.count : shared_layout.stmt_node_ids.size();
    this->expr_intrinsic_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_owned_use_modes.clear();
    this->prepare_analysis_only_storage(expr_count);
    this->expr_expected_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_c_name_ids.assign(expr_count, INVALID_IDENT_ID);
    this->pattern_c_name_ids.assign(pattern_count, INVALID_IDENT_ID);
    this->syntax_type_handles.assign(type_count, INVALID_TYPE_HANDLE);
    this->stmt_local_types.assign(stmt_count, INVALID_TYPE_HANDLE);
    this->sparse_expr_intrinsic_types.clear();
    this->sparse_expr_types.clear();
    this->sparse_expr_owned_use_modes.clear();
    this->sparse_expr_c_name_ids.clear();
    this->sparse_pattern_c_name_ids.clear();
    this->sparse_syntax_type_handles.clear();
    this->sparse_stmt_local_types.clear();
    this->for_in_iteration_plans.clear();
    this->sparse_fallbacks = {};
}

void GenericSideTables::bind_local_dense_layout(const GenericSideTableLayout& shared_layout) noexcept
{
    this->sparse = true;
    this->local_dense = true;
    this->expr_span = shared_layout.expr_span;
    this->pattern_span = shared_layout.pattern_span;
    this->type_span = shared_layout.type_span;
    this->stmt_span = shared_layout.stmt_span;
    this->layout = &shared_layout;
}

void GenericSideTables::prepare_analysis_only_storage(const base::usize expr_count)
{
    this->expr_expected_types = SemaTypeTable{};
    this->sparse_expr_expected_types = SemaMap<base::u32, TypeHandle>{};
    this->analysis_arena_ = std::make_unique<base::BumpAllocator>(SEMA_GENERIC_SIDE_TABLE_BLOCK_BYTES);
    this->expr_expected_types = make_sema_vector<TypeHandle>(*this->analysis_arena_);
    this->sparse_expr_expected_types = make_sema_map<base::u32, TypeHandle>(*this->analysis_arena_);
    this->expr_expected_types.reserve(expr_count);
}

void GenericSideTables::release_analysis_only_storage()
{
    this->expr_expected_types = SemaTypeTable{};
    this->sparse_expr_expected_types = SemaMap<base::u32, TypeHandle>{};
    this->analysis_arena_.reset();
    this->pattern_case_name_ids = PatternCaseNameTable{};
}

void GenericSideTables::swap(GenericSideTables& other) noexcept
{
    using std::swap;
    swap(this->sparse, other.sparse);
    swap(this->local_dense, other.local_dense);
    swap(this->expr_span, other.expr_span);
    swap(this->pattern_span, other.pattern_span);
    swap(this->type_span, other.type_span);
    swap(this->stmt_span, other.stmt_span);
    swap(this->layout, other.layout);
    this->expr_node_ids.swap(other.expr_node_ids);
    this->pattern_node_ids.swap(other.pattern_node_ids);
    this->type_node_ids.swap(other.type_node_ids);
    this->stmt_node_ids.swap(other.stmt_node_ids);
    this->expr_intrinsic_types.swap(other.expr_intrinsic_types);
    this->expr_types.swap(other.expr_types);
    this->expr_expected_types.swap(other.expr_expected_types);
    this->expr_owned_use_modes.swap(other.expr_owned_use_modes);
    this->expr_c_name_ids.swap(other.expr_c_name_ids);
    this->pattern_c_name_ids.swap(other.pattern_c_name_ids);
    this->syntax_type_handles.swap(other.syntax_type_handles);
    this->stmt_local_types.swap(other.stmt_local_types);
    this->sparse_expr_intrinsic_types.swap(other.sparse_expr_intrinsic_types);
    this->sparse_expr_types.swap(other.sparse_expr_types);
    this->sparse_expr_expected_types.swap(other.sparse_expr_expected_types);
    this->sparse_expr_owned_use_modes.swap(other.sparse_expr_owned_use_modes);
    this->sparse_expr_c_name_ids.swap(other.sparse_expr_c_name_ids);
    this->sparse_pattern_c_name_ids.swap(other.sparse_pattern_c_name_ids);
    swap(this->pattern_case_name_ids, other.pattern_case_name_ids);
    this->sparse_syntax_type_handles.swap(other.sparse_syntax_type_handles);
    this->sparse_stmt_local_types.swap(other.sparse_stmt_local_types);
    this->for_in_iteration_plans.swap(other.for_in_iteration_plans);
    swap(this->sparse_fallbacks, other.sparse_fallbacks);
    swap(this->arena_, other.arena_);
    swap(this->analysis_arena_, other.analysis_arena_);
}

void GenericSideTables::copy_from(const GenericSideTables& other)
{
    this->sparse = other.sparse;
    this->local_dense = other.local_dense;
    this->expr_span = other.expr_span;
    this->pattern_span = other.pattern_span;
    this->type_span = other.type_span;
    this->stmt_span = other.stmt_span;
    this->layout = other.layout;
    this->expr_node_ids.assign(other.expr_node_ids.begin(), other.expr_node_ids.end());
    this->pattern_node_ids.assign(other.pattern_node_ids.begin(), other.pattern_node_ids.end());
    this->type_node_ids.assign(other.type_node_ids.begin(), other.type_node_ids.end());
    this->stmt_node_ids.assign(other.stmt_node_ids.begin(), other.stmt_node_ids.end());
    this->expr_intrinsic_types.assign(other.expr_intrinsic_types.begin(), other.expr_intrinsic_types.end());
    this->expr_types.assign(other.expr_types.begin(), other.expr_types.end());
    this->expr_owned_use_modes.assign(other.expr_owned_use_modes.begin(), other.expr_owned_use_modes.end());
    if (other.expr_expected_types.empty() && other.sparse_expr_expected_types.empty()) {
        this->release_analysis_only_storage();
    } else {
        this->prepare_analysis_only_storage(other.expr_expected_types.size());
        this->expr_expected_types.assign(other.expr_expected_types.begin(), other.expr_expected_types.end());
        this->sparse_expr_expected_types = other.sparse_expr_expected_types;
    }
    this->expr_c_name_ids.assign(other.expr_c_name_ids.begin(), other.expr_c_name_ids.end());
    this->pattern_c_name_ids.assign(other.pattern_c_name_ids.begin(), other.pattern_c_name_ids.end());
    this->syntax_type_handles.assign(other.syntax_type_handles.begin(), other.syntax_type_handles.end());
    this->stmt_local_types.assign(other.stmt_local_types.begin(), other.stmt_local_types.end());
    this->sparse_expr_intrinsic_types = other.sparse_expr_intrinsic_types;
    this->sparse_expr_types = other.sparse_expr_types;
    this->sparse_expr_owned_use_modes = other.sparse_expr_owned_use_modes;
    this->sparse_expr_c_name_ids = other.sparse_expr_c_name_ids;
    this->sparse_pattern_c_name_ids = other.sparse_pattern_c_name_ids;
    this->pattern_case_name_ids = other.pattern_case_name_ids;
    this->sparse_syntax_type_handles = other.sparse_syntax_type_handles;
    this->sparse_stmt_local_types = other.sparse_stmt_local_types;
    this->for_in_iteration_plans = other.for_in_iteration_plans;
    this->sparse_fallbacks = other.sparse_fallbacks;
}

CheckedModule::CheckedModule()
    : arena_(std::make_unique<base::BumpAllocator>()),
      expr_intrinsic_types(make_sema_vector<TypeHandle>(*this->arena_)),
      expr_types(make_sema_vector<TypeHandle>(*this->arena_)),
      expr_owned_use_modes(make_sema_vector<OwnedUseMode>(*this->arena_)),
      expr_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      pattern_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      syntax_type_handles(make_sema_vector<TypeHandle>(*this->arena_)),
      stmt_local_types(make_sema_vector<TypeHandle>(*this->arena_)),
      for_in_iteration_plans(make_sema_map<base::u32, ForInIterationPlan>(*this->arena_)),
      item_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      coercions(make_sema_vector<CoercionRecord>(*this->arena_)),
      lambdas(make_sema_vector<CheckedLambdaInfo>(*this->arena_)),
      functions(make_sema_map<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      structs(make_sema_map<ModuleLookupKey, StructInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash{})),
      enum_cases(
          make_sema_map<ModuleLookupKey, EnumCaseInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash{})),
      derived_capabilities_by_type(make_sema_map<base::u32, DerivedCapabilityList>(*this->arena_)),
      type_aliases(
          make_sema_map<ModuleLookupKey, TypeAliasInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash{})),
      traits(make_sema_map<ModuleLookupKey, TraitSignature, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash{})),
      trait_impls(make_sema_map<TraitImplLookupKey, TraitImplInfo, TraitImplLookupKeyHash>(
          *this->arena_, TraitImplLookupKeyHash{})),
      trait_predicates(make_sema_vector<TraitPredicate>(*this->arena_)),
      trait_obligations(make_sema_vector<TraitObligation>(*this->arena_)),
      trait_evidence(make_sema_vector<TraitEvidence>(*this->arena_)),
      trait_method_calls(make_sema_vector<TraitMethodCallBinding>(*this->arena_)),
      function_calls(make_sema_vector<FunctionCallBinding>(*this->arena_)),
      trait_object_method_slots(make_sema_vector<TraitObjectMethodSlotFact>(*this->arena_)),
      trait_object_callability(make_sema_vector<TraitObjectCallabilityFact>(*this->arena_)),
      vtable_layouts(make_sema_vector<VTableLayoutFact>(*this->arena_)),
      trait_object_coercions(make_sema_vector<TraitObjectCoercionFact>(*this->arena_)),
      trait_supertrait_edges(make_sema_vector<TraitSupertraitEdgeFact>(*this->arena_)),
      trait_object_upcast_coercions(make_sema_vector<TraitObjectUpcastCoercionFact>(*this->arena_)),
      trait_method_call_by_expr(make_sema_map<base::u32, base::u32>(*this->arena_)),
      function_call_by_expr(make_sema_map<base::u32, base::u32>(*this->arena_)),
      borrow_summaries(make_sema_map<FunctionLookupKey, FunctionBorrowSummary, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      borrow_contracts(make_sema_map<FunctionLookupKey, FunctionBorrowContract, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      move_rejection_facts(make_sema_map<FunctionLookupKey, FunctionMoveRejectionFacts, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      lifetime_origin_params(make_sema_vector<LifetimeOriginParamInfo>(*this->arena_)),
      reference_origin_facts(make_sema_vector<ReferenceOriginFact>(*this->arena_)),
      type_lifetime_infos(make_sema_vector<TypeLifetimeInfo>(*this->arena_)),
      generic_lifetime_predicates(make_sema_vector<GenericLifetimePredicate>(*this->arena_)),
      lifetime_facts(make_sema_map<FunctionLookupKey, FunctionLifetimeFacts, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      dropck_facts(make_sema_map<FunctionLookupKey, FunctionDropCheckFacts, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      destructors(make_sema_map<base::u32, DestructorInfo>(*this->arena_)),
      body_flow_graphs(make_sema_map<FunctionLookupKey, BodyFlowGraph, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      place_state_facts(make_sema_map<FunctionLookupKey, FunctionPlaceStateFacts, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      body_loan_checks(make_sema_map<FunctionLookupKey, BodyLoanCheckResult, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      param_envs(make_sema_vector<ParamEnvInfo>(*this->arena_)),
      generic_template_signatures(make_sema_vector<GenericTemplateSignatureInfo>(*this->arena_)),
      generic_side_table_layouts(make_sema_deque<GenericSideTableLayout>(*this->arena_)),
      generic_enum_instances(make_sema_deque<GenericEnumInstanceInfo>(*this->arena_)),
      generic_type_alias_instances(make_sema_deque<GenericTypeAliasInstanceInfo>(*this->arena_)),
      generic_function_instances(make_sema_deque<GenericFunctionInstanceInfo>(*this->arena_)),
      trait_default_method_instances(make_sema_deque<TraitDefaultMethodInstanceInfo>(*this->arena_))
{
}

CheckedModule::CheckedModule(const CheckedModule& other) : CheckedModule()
{
    this->copy_from(other);
}

CheckedModule& CheckedModule::operator=(const CheckedModule& other)
{
    if (this == &other) {
        return *this;
    }
    CheckedModule copy(other);
    *this = std::move(copy);
    return *this;
}

CheckedModule::CheckedModule(CheckedModule&& other) noexcept : CheckedModule()
{
    this->swap(other);
}

CheckedModule& CheckedModule::operator=(CheckedModule&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

base::usize CheckedModule::arena_bytes() const noexcept
{
    return (this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes())
        + (this->analysis_arena_ == nullptr ? 0 : this->analysis_arena_->allocated_bytes());
}

base::usize CheckedModule::arena_blocks() const noexcept
{
    return (this->arena_ == nullptr ? 0 : this->arena_->block_count())
        + (this->analysis_arena_ == nullptr ? 0 : this->analysis_arena_->block_count());
}

void CheckedModule::swap(CheckedModule& other) noexcept
{
    using std::swap;
    const IdentifierInterner* const this_c_names = &this->c_names;
    const IdentifierInterner* const other_c_names = &other.c_names;
    swap(this->arena_, other.arena_);
    swap(this->analysis_arena_, other.analysis_arena_);
    swap(this->c_names, other.c_names);
    swap(this->types, other.types);
    this->expr_intrinsic_types.swap(other.expr_intrinsic_types);
    this->expr_types.swap(other.expr_types);
    this->expr_expected_types.swap(other.expr_expected_types);
    this->expr_owned_use_modes.swap(other.expr_owned_use_modes);
    this->expr_c_name_ids.swap(other.expr_c_name_ids);
    this->pattern_c_name_ids.swap(other.pattern_c_name_ids);
    swap(this->pattern_case_name_ids, other.pattern_case_name_ids);
    this->syntax_type_handles.swap(other.syntax_type_handles);
    this->stmt_local_types.swap(other.stmt_local_types);
    this->for_in_iteration_plans.swap(other.for_in_iteration_plans);
    this->item_c_name_ids.swap(other.item_c_name_ids);
    this->coercions.swap(other.coercions);
    this->lambdas.swap(other.lambdas);
    this->functions.swap(other.functions);
    this->structs.swap(other.structs);
    this->enum_cases.swap(other.enum_cases);
    this->derived_capabilities_by_type.swap(other.derived_capabilities_by_type);
    this->type_aliases.swap(other.type_aliases);
    this->traits.swap(other.traits);
    this->trait_impls.swap(other.trait_impls);
    this->trait_predicates.swap(other.trait_predicates);
    this->trait_obligations.swap(other.trait_obligations);
    this->trait_evidence.swap(other.trait_evidence);
    this->trait_method_calls.swap(other.trait_method_calls);
    this->function_calls.swap(other.function_calls);
    this->trait_object_method_slots.swap(other.trait_object_method_slots);
    this->trait_object_callability.swap(other.trait_object_callability);
    this->vtable_layouts.swap(other.vtable_layouts);
    this->trait_object_coercions.swap(other.trait_object_coercions);
    this->trait_supertrait_edges.swap(other.trait_supertrait_edges);
    this->trait_object_upcast_coercions.swap(other.trait_object_upcast_coercions);
    swap(this->principal_set_composition_facts, other.principal_set_composition_facts);
    this->trait_method_call_by_expr.swap(other.trait_method_call_by_expr);
    this->function_call_by_expr.swap(other.function_call_by_expr);
    this->borrow_summaries.swap(other.borrow_summaries);
    this->borrow_contracts.swap(other.borrow_contracts);
    this->move_rejection_facts.swap(other.move_rejection_facts);
    this->lifetime_origin_params.swap(other.lifetime_origin_params);
    this->reference_origin_facts.swap(other.reference_origin_facts);
    this->type_lifetime_infos.swap(other.type_lifetime_infos);
    this->generic_lifetime_predicates.swap(other.generic_lifetime_predicates);
    this->lifetime_facts.swap(other.lifetime_facts);
    this->dropck_facts.swap(other.dropck_facts);
    this->destructors.swap(other.destructors);
    this->body_flow_graphs.swap(other.body_flow_graphs);
    this->place_state_facts.swap(other.place_state_facts);
    this->body_loan_checks.swap(other.body_loan_checks);
    this->param_envs.swap(other.param_envs);
    this->generic_template_signatures.swap(other.generic_template_signatures);
    this->generic_side_table_layouts.swap(other.generic_side_table_layouts);
    this->generic_enum_instances.swap(other.generic_enum_instances);
    this->generic_type_alias_instances.swap(other.generic_type_alias_instances);
    this->generic_function_instances.swap(other.generic_function_instances);
    this->trait_default_method_instances.swap(other.trait_default_method_instances);
    swap(this->normalized_ast, other.normalized_ast);
    this->rebind_interned_texts(other_c_names, this->c_names);
    other.rebind_interned_texts(this_c_names, other.c_names);
    this->rebind_generic_instance_layouts();
    other.rebind_generic_instance_layouts();
}

void CheckedModule::copy_from(const CheckedModule& other)
{
    this->c_names = other.c_names;
    this->types = other.types;
    this->expr_intrinsic_types.assign(other.expr_intrinsic_types.begin(), other.expr_intrinsic_types.end());
    this->expr_types.assign(other.expr_types.begin(), other.expr_types.end());
    this->expr_owned_use_modes.assign(other.expr_owned_use_modes.begin(), other.expr_owned_use_modes.end());
    if (other.expr_expected_types.empty()) {
        this->expr_expected_types = SemaTypeTable{};
        this->analysis_arena_.reset();
    } else {
        this->prepare_analysis_only_storage(other.expr_expected_types.size());
        this->expr_expected_types.assign(other.expr_expected_types.begin(), other.expr_expected_types.end());
    }
    this->expr_c_name_ids.assign(other.expr_c_name_ids.begin(), other.expr_c_name_ids.end());
    this->pattern_c_name_ids.assign(other.pattern_c_name_ids.begin(), other.pattern_c_name_ids.end());
    this->pattern_case_name_ids = other.pattern_case_name_ids;
    this->syntax_type_handles.assign(other.syntax_type_handles.begin(), other.syntax_type_handles.end());
    this->stmt_local_types.assign(other.stmt_local_types.begin(), other.stmt_local_types.end());
    this->for_in_iteration_plans.clear();
    this->for_in_iteration_plans.reserve(other.for_in_iteration_plans.size());
    for (const auto& entry : other.for_in_iteration_plans) {
        this->for_in_iteration_plans.emplace(entry.first, this->clone_for_in_iteration_plan(entry.second));
    }
    this->item_c_name_ids.assign(other.item_c_name_ids.begin(), other.item_c_name_ids.end());
    this->coercions.assign(other.coercions.begin(), other.coercions.end());
    this->lambdas.clear();
    this->lambdas.reserve(other.lambdas.size());
    for (const CheckedLambdaInfo& lambda : other.lambdas) {
        this->lambdas.push_back(this->clone_lambda_info(lambda));
    }
    this->functions.clear();
    this->functions.reserve(other.functions.size());
    for (const auto& entry : other.functions) {
        this->functions.emplace(entry.first, this->clone_function_signature(entry.second));
    }
    this->structs.clear();
    this->structs.reserve(other.structs.size());
    for (const auto& entry : other.structs) {
        this->structs.emplace(entry.first, this->clone_struct_info(entry.second));
    }
    this->enum_cases.clear();
    this->enum_cases.reserve(other.enum_cases.size());
    for (const auto& entry : other.enum_cases) {
        this->enum_cases.emplace(entry.first, this->clone_enum_case_info(entry.second));
    }
    this->derived_capabilities_by_type.clear();
    this->derived_capabilities_by_type.reserve(other.derived_capabilities_by_type.size());
    for (const auto& entry : other.derived_capabilities_by_type) {
        this->derived_capabilities_by_type.emplace(entry.first, this->copy_derived_capability_list(entry.second));
    }
    this->type_aliases.clear();
    this->type_aliases.reserve(other.type_aliases.size());
    for (const auto& entry : other.type_aliases) {
        TypeAliasInfo alias;
        alias.name = this->intern_text(entry.second.name);
        alias.name_id = entry.second.name_id;
        alias.module = entry.second.module;
        alias.item = entry.second.item;
        alias.target = entry.second.target;
        alias.range = entry.second.range;
        alias.visibility = entry.second.visibility;
        alias.stable_id = entry.second.stable_id;
        alias.incremental_key = entry.second.incremental_key;
        alias.part_index = entry.second.part_index;
        this->type_aliases.emplace(entry.first, alias);
    }
    this->traits.clear();
    this->traits.reserve(other.traits.size());
    for (const auto& entry : other.traits) {
        this->traits.emplace(entry.first, this->clone_trait_signature(entry.second));
    }
    this->trait_impls.clear();
    this->trait_impls.reserve(other.trait_impls.size());
    for (const auto& entry : other.trait_impls) {
        this->trait_impls.emplace(entry.first, this->clone_trait_impl_info(entry.second));
    }
    this->trait_predicates.clear();
    this->trait_predicates.reserve(other.trait_predicates.size());
    for (const TraitPredicate& predicate : other.trait_predicates) {
        this->trait_predicates.push_back(this->clone_trait_predicate(predicate));
    }
    this->trait_obligations.clear();
    this->trait_obligations.reserve(other.trait_obligations.size());
    for (const TraitObligation& obligation : other.trait_obligations) {
        this->trait_obligations.push_back(this->clone_trait_obligation(obligation));
    }
    this->trait_evidence.clear();
    this->trait_evidence.reserve(other.trait_evidence.size());
    for (const TraitEvidence& evidence : other.trait_evidence) {
        this->trait_evidence.push_back(this->clone_trait_evidence(evidence));
    }
    this->trait_method_calls.clear();
    this->trait_method_call_by_expr.clear();
    this->trait_method_calls.reserve(other.trait_method_calls.size());
    this->trait_method_call_by_expr.reserve(other.trait_method_calls.size());
    for (const TraitMethodCallBinding& binding : other.trait_method_calls) {
        this->append_trait_method_call_binding(this->clone_trait_method_call_binding(binding));
    }
    this->function_calls.clear();
    this->function_call_by_expr.clear();
    this->function_calls.reserve(other.function_calls.size());
    this->function_call_by_expr.reserve(other.function_calls.size());
    for (const FunctionCallBinding& binding : other.function_calls) {
        this->append_function_call_binding(this->clone_function_call_binding(binding));
    }
    this->trait_object_method_slots.clear();
    this->trait_object_method_slots.reserve(other.trait_object_method_slots.size());
    for (const TraitObjectMethodSlotFact& fact : other.trait_object_method_slots) {
        this->trait_object_method_slots.push_back(this->clone_trait_object_method_slot_fact(fact));
    }
    this->trait_object_callability.clear();
    this->trait_object_callability.reserve(other.trait_object_callability.size());
    for (const TraitObjectCallabilityFact& fact : other.trait_object_callability) {
        this->trait_object_callability.push_back(this->clone_trait_object_callability_fact(fact));
    }
    this->vtable_layouts.clear();
    this->vtable_layouts.reserve(other.vtable_layouts.size());
    for (const VTableLayoutFact& fact : other.vtable_layouts) {
        this->vtable_layouts.push_back(this->clone_vtable_layout_fact(fact));
    }
    this->trait_object_coercions.clear();
    this->trait_object_coercions.reserve(other.trait_object_coercions.size());
    for (const TraitObjectCoercionFact& fact : other.trait_object_coercions) {
        this->trait_object_coercions.push_back(this->clone_trait_object_coercion_fact(fact));
    }
    this->trait_supertrait_edges.clear();
    this->trait_supertrait_edges.reserve(other.trait_supertrait_edges.size());
    for (const TraitSupertraitEdgeFact& fact : other.trait_supertrait_edges) {
        this->trait_supertrait_edges.push_back(this->clone_trait_supertrait_edge_fact(fact));
    }
    this->trait_object_upcast_coercions.clear();
    this->trait_object_upcast_coercions.reserve(other.trait_object_upcast_coercions.size());
    for (const TraitObjectUpcastCoercionFact& fact : other.trait_object_upcast_coercions) {
        this->trait_object_upcast_coercions.push_back(this->clone_trait_object_upcast_coercion_fact(fact));
    }
    this->principal_set_composition_facts = other.principal_set_composition_facts;
    this->borrow_summaries.clear();
    this->borrow_summaries.reserve(other.borrow_summaries.size());
    for (const auto& entry : other.borrow_summaries) {
        this->borrow_summaries.emplace(entry.first, entry.second);
    }
    this->borrow_contracts.clear();
    this->borrow_contracts.reserve(other.borrow_contracts.size());
    for (const auto& entry : other.borrow_contracts) {
        this->borrow_contracts.emplace(entry.first, entry.second);
    }
    this->move_rejection_facts.clear();
    this->move_rejection_facts.reserve(other.move_rejection_facts.size());
    for (const auto& entry : other.move_rejection_facts) {
        this->move_rejection_facts.emplace(entry.first, this->clone_function_move_rejection_facts(entry.second));
    }
    this->lifetime_origin_params.clear();
    this->lifetime_origin_params.reserve(other.lifetime_origin_params.size());
    for (const LifetimeOriginParamInfo& origin_param : other.lifetime_origin_params) {
        this->lifetime_origin_params.push_back(this->clone_lifetime_origin_param(origin_param));
    }
    this->reference_origin_facts.clear();
    this->reference_origin_facts.reserve(other.reference_origin_facts.size());
    for (const ReferenceOriginFact& fact : other.reference_origin_facts) {
        this->reference_origin_facts.push_back(this->clone_reference_origin_fact(fact));
    }
    this->type_lifetime_infos.clear();
    this->type_lifetime_infos.reserve(other.type_lifetime_infos.size());
    for (const TypeLifetimeInfo& info : other.type_lifetime_infos) {
        this->type_lifetime_infos.push_back(this->clone_type_lifetime_info(info));
    }
    this->generic_lifetime_predicates.clear();
    this->generic_lifetime_predicates.reserve(other.generic_lifetime_predicates.size());
    for (const GenericLifetimePredicate& predicate : other.generic_lifetime_predicates) {
        this->generic_lifetime_predicates.push_back(this->clone_generic_lifetime_predicate(predicate));
    }
    this->lifetime_facts.clear();
    this->lifetime_facts.reserve(other.lifetime_facts.size());
    for (const auto& entry : other.lifetime_facts) {
        this->lifetime_facts.emplace(entry.first, this->clone_function_lifetime_facts(entry.second));
    }
    this->dropck_facts.clear();
    this->dropck_facts.reserve(other.dropck_facts.size());
    for (const auto& entry : other.dropck_facts) {
        this->dropck_facts.emplace(entry.first, this->clone_function_drop_check_facts(entry.second));
    }
    this->destructors.clear();
    this->destructors.reserve(other.destructors.size());
    for (const auto& entry : other.destructors) {
        this->destructors.emplace(entry.first, this->clone_destructor_info(entry.second));
    }
    this->body_flow_graphs.clear();
    this->body_flow_graphs.reserve(other.body_flow_graphs.size());
    for (const auto& entry : other.body_flow_graphs) {
        this->body_flow_graphs.emplace(entry.first, entry.second);
    }
    this->place_state_facts.clear();
    this->place_state_facts.reserve(other.place_state_facts.size());
    for (const auto& entry : other.place_state_facts) {
        this->place_state_facts.emplace(entry.first, this->clone_function_place_state_facts(entry.second));
    }
    this->body_loan_checks.clear();
    this->body_loan_checks.reserve(other.body_loan_checks.size());
    for (const auto& entry : other.body_loan_checks) {
        this->body_loan_checks.emplace(entry.first, entry.second);
    }
    this->param_envs.clear();
    this->param_envs.reserve(other.param_envs.size());
    for (const ParamEnvInfo& param_env : other.param_envs) {
        this->param_envs.push_back(this->clone_param_env_info(param_env));
    }
    this->generic_template_signatures.clear();
    this->generic_template_signatures.reserve(other.generic_template_signatures.size());
    for (const GenericTemplateSignatureInfo& signature : other.generic_template_signatures) {
        this->generic_template_signatures.push_back(this->clone_generic_template_signature_info(signature));
    }
    this->generic_side_table_layouts.clear();
    for (const GenericSideTableLayout& layout : other.generic_side_table_layouts) {
        this->generic_side_table_layouts.push_back(this->clone_generic_side_table_layout(layout));
    }
    this->generic_enum_instances.clear();
    for (const GenericEnumInstanceInfo& instance : other.generic_enum_instances) {
        this->generic_enum_instances.push_back(this->clone_generic_enum_instance(instance));
    }
    this->generic_type_alias_instances.clear();
    for (const GenericTypeAliasInstanceInfo& instance : other.generic_type_alias_instances) {
        this->generic_type_alias_instances.push_back(this->clone_generic_type_alias_instance(instance));
    }
    this->generic_function_instances.clear();
    for (const GenericFunctionInstanceInfo& instance : other.generic_function_instances) {
        this->generic_function_instances.push_back(this->clone_generic_function_instance(instance));
    }
    this->trait_default_method_instances.clear();
    for (const TraitDefaultMethodInstanceInfo& instance : other.trait_default_method_instances) {
        this->trait_default_method_instances.push_back(this->clone_trait_default_method_instance(instance));
    }
    this->rebind_generic_instance_layouts();
    this->normalized_ast = other.normalized_ast;
}

TypeHandleList CheckedModule::make_type_handle_list() const
{
    return make_sema_vector<TypeHandle>(*this->arena_);
}

TypeHandleList CheckedModule::copy_type_handle_list(const std::span<const TypeHandle> values) const
{
    TypeHandleList copy = this->make_type_handle_list();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

SemaVector<syntax::ExprId> CheckedModule::make_expr_id_list() const
{
    return make_sema_vector<syntax::ExprId>(*this->arena_);
}

SemaVector<syntax::ExprId> CheckedModule::copy_expr_id_list(const std::span<const syntax::ExprId> values) const
{
    SemaVector<syntax::ExprId> copy = this->make_expr_id_list();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

FunctionParamInfoList CheckedModule::make_function_param_info_list() const
{
    return make_sema_vector<FunctionParamInfo>(*this->arena_);
}

FunctionParamInfoList CheckedModule::copy_function_param_info_list(
    const std::span<const FunctionParamInfo> values)
{
    FunctionParamInfoList copy = this->make_function_param_info_list();
    copy.reserve(values.size());
    for (const FunctionParamInfo& value : values) {
        copy.push_back(FunctionParamInfo{
            this->intern_text(value.name),
            value.name_id,
            value.default_value,
            value.range,
        });
    }
    return copy;
}

SemaVector<StructFieldInfo> CheckedModule::make_struct_field_list() const
{
    return make_sema_vector<StructFieldInfo>(*this->arena_);
}

SemaVector<StructFieldInfo> CheckedModule::copy_struct_field_list(const std::span<const StructFieldInfo> values)
{
    SemaVector<StructFieldInfo> copy = this->make_struct_field_list();
    copy.reserve(values.size());
    for (const StructFieldInfo& field : values) {
        StructFieldInfo field_copy;
        field_copy.name = this->intern_text(field.name);
        field_copy.name_id = field.name_id;
        field_copy.c_name = this->intern_text(field.c_name);
        field_copy.module = field.module;
        field_copy.type = field.type;
        field_copy.range = field.range;
        field_copy.visibility = field.visibility;
        field_copy.stable_key = field.stable_key;
        copy.push_back(field_copy);
    }
    return copy;
}

DerivedCapabilityList CheckedModule::make_derived_capability_list() const
{
    return make_sema_vector<DerivedCapabilityInfo>(*this->arena_);
}

DerivedCapabilityList CheckedModule::copy_derived_capability_list(
    const std::span<const DerivedCapabilityInfo> values) const
{
    DerivedCapabilityList copy = this->make_derived_capability_list();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

SemaIndexTable CheckedModule::make_index_table() const
{
    return make_sema_vector<base::u32>(*this->arena_);
}

SemaIndexTable CheckedModule::copy_index_table(const std::span<const base::u32> values) const
{
    SemaIndexTable copy = this->make_index_table();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

FunctionSignature CheckedModule::make_function_signature() const
{
    FunctionSignature signature;
    signature.param_types = this->make_type_handle_list();
    signature.params = this->make_function_param_info_list();
    signature.generic_args = this->make_type_handle_list();
    return signature;
}

CheckedLambdaInfo CheckedModule::make_lambda_info() const
{
    CheckedLambdaInfo info;
    info.param_types = this->make_type_handle_list();
    info.captures = make_sema_vector<CheckedLambdaInfo::Capture>(*this->arena_);
    return info;
}

StructInfo CheckedModule::make_struct_info() const
{
    StructInfo info;
    info.fields = this->make_struct_field_list();
    return info;
}

EnumCaseInfo CheckedModule::make_enum_case_info() const
{
    EnumCaseInfo info;
    info.payload_types = this->make_type_handle_list();
    return info;
}

TraitMethodRequirement CheckedModule::make_trait_method_requirement() const
{
    TraitMethodRequirement requirement;
    requirement.param_types = this->make_type_handle_list();
    requirement.params = this->make_function_param_info_list();
    return requirement;
}

TraitAssociatedTypeRequirement CheckedModule::make_trait_associated_type_requirement() const
{
    return {};
}

TraitSupertraitInfo CheckedModule::make_trait_supertrait_info() const
{
    TraitSupertraitInfo info;
    info.parent_trait_args = this->make_type_handle_list();
    return info;
}

TraitSignature CheckedModule::make_trait_signature() const
{
    TraitSignature signature;
    signature.generic_params = make_sema_vector<IdentId>(*this->arena_);
    signature.supertraits = make_sema_vector<TraitSupertraitInfo>(*this->arena_);
    signature.associated_types = make_sema_vector<TraitAssociatedTypeRequirement>(*this->arena_);
    signature.requirements = make_sema_vector<TraitMethodRequirement>(*this->arena_);
    return signature;
}

TraitImplMethodInfo CheckedModule::make_trait_impl_method_info() const
{
    return {};
}

TraitImplAssociatedTypeInfo CheckedModule::make_trait_impl_associated_type_info() const
{
    return {};
}

TraitImplInfo CheckedModule::make_trait_impl_info() const
{
    TraitImplInfo info;
    info.trait_args = this->make_type_handle_list();
    info.associated_types = make_sema_vector<TraitImplAssociatedTypeInfo>(*this->arena_);
    info.methods = make_sema_vector<TraitImplMethodInfo>(*this->arena_);
    return info;
}

TraitPredicate CheckedModule::make_trait_predicate() const
{
    TraitPredicate predicate;
    predicate.trait_args = this->make_type_handle_list();
    predicate.associated_type_equalities = make_sema_vector<TraitImplAssociatedTypeInfo>(*this->arena_);
    return predicate;
}

TraitObligation CheckedModule::make_trait_obligation() const
{
    return {};
}

TraitEvidence CheckedModule::make_trait_evidence() const
{
    return {};
}

TraitMethodCallBinding CheckedModule::make_trait_method_call_binding() const
{
    TraitMethodCallBinding binding;
    binding.ordered_args = make_sema_vector<syntax::ExprId>(*this->arena_);
    return binding;
}

FunctionCallBinding CheckedModule::make_function_call_binding() const
{
    FunctionCallBinding binding;
    binding.ordered_args = make_sema_vector<syntax::ExprId>(*this->arena_);
    return binding;
}

TraitObjectMethodSlotFact CheckedModule::make_trait_object_method_slot_fact() const
{
    return {};
}

TraitObjectCallabilityFact CheckedModule::make_trait_object_callability_fact() const
{
    return {};
}

VTableLayoutFact CheckedModule::make_vtable_layout_fact() const
{
    VTableLayoutFact fact;
    fact.method_slots = make_sema_vector<VTableMethodSlotFact>(*this->arena_);
    return fact;
}

TraitObjectCoercionFact CheckedModule::make_trait_object_coercion_fact() const
{
    return {};
}

TraitSupertraitEdgeFact CheckedModule::make_trait_supertrait_edge_fact() const
{
    TraitSupertraitEdgeFact fact;
    fact.parent_trait_args = this->make_type_handle_list();
    return fact;
}

TraitObjectUpcastCoercionFact CheckedModule::make_trait_object_upcast_coercion_fact() const
{
    return {};
}

void CheckedModule::append_trait_method_call_binding(TraitMethodCallBinding binding)
{
    const syntax::ExprId call_expr = binding.call_expr;
    const syntax::ExprId callee_expr = binding.callee_expr;
    const base::u32 index =
        base::checked_u32(this->trait_method_calls.size(), SEMA_TRAIT_METHOD_CALL_BINDING_ID_CONTEXT);
    this->trait_method_calls.push_back(std::move(binding));
    if (syntax::is_valid(call_expr)) {
        this->trait_method_call_by_expr[call_expr.value] = index;
    }
    if (syntax::is_valid(callee_expr)) {
        this->trait_method_call_by_expr[callee_expr.value] = index;
    }
}

void CheckedModule::append_function_call_binding(FunctionCallBinding binding)
{
    const syntax::ExprId call_expr = binding.call_expr;
    const base::u32 index = base::checked_u32(this->function_calls.size(), SEMA_FUNCTION_CALL_BINDING_ID_CONTEXT);
    this->function_calls.push_back(std::move(binding));
    if (syntax::is_valid(call_expr)) {
        this->function_call_by_expr[call_expr.value] = index;
    }
}

const TraitMethodCallBinding* CheckedModule::trait_method_call_binding_for_call_expr(
    const syntax::ExprId call_expr) const noexcept
{
    if (!syntax::is_valid(call_expr)) {
        return nullptr;
    }
    if (const auto found = this->trait_method_call_by_expr.find(call_expr.value);
        found != this->trait_method_call_by_expr.end() && found->second < this->trait_method_calls.size()
        && this->trait_method_calls[found->second].call_expr.value == call_expr.value) {
        return &this->trait_method_calls[found->second];
    }
    for (const TraitMethodCallBinding& binding : this->trait_method_calls) {
        if (binding.call_expr.value == call_expr.value) {
            return &binding;
        }
    }
    return nullptr;
}

const TraitMethodCallBinding* CheckedModule::trait_method_call_binding_for_callee_expr(
    const syntax::ExprId callee_expr) const noexcept
{
    if (!syntax::is_valid(callee_expr)) {
        return nullptr;
    }
    if (const auto found = this->trait_method_call_by_expr.find(callee_expr.value);
        found != this->trait_method_call_by_expr.end() && found->second < this->trait_method_calls.size()
        && this->trait_method_calls[found->second].callee_expr.value == callee_expr.value) {
        return &this->trait_method_calls[found->second];
    }
    for (const TraitMethodCallBinding& binding : this->trait_method_calls) {
        if (binding.callee_expr.value == callee_expr.value) {
            return &binding;
        }
    }
    return nullptr;
}

const TraitMethodCallBinding* CheckedModule::trait_method_call_binding_for_expr(
    const syntax::ExprId call_expr) const noexcept
{
    if (const TraitMethodCallBinding* const binding = this->trait_method_call_binding_for_call_expr(call_expr);
        binding != nullptr) {
        return binding;
    }
    return this->trait_method_call_binding_for_callee_expr(call_expr);
}

const FunctionCallBinding* CheckedModule::function_call_binding_for_expr(const syntax::ExprId call_expr) const noexcept
{
    if (!syntax::is_valid(call_expr)) {
        return nullptr;
    }
    if (const auto found = this->function_call_by_expr.find(call_expr.value);
        found != this->function_call_by_expr.end() && found->second < this->function_calls.size()) {
        return &this->function_calls[found->second];
    }
    if (this->function_call_by_expr.size() == this->function_calls.size()) {
        return nullptr;
    }
    for (const FunctionCallBinding& binding : this->function_calls) {
        if (binding.call_expr.value == call_expr.value) {
            return &binding;
        }
    }
    return nullptr;
}

ParamEnvInfo CheckedModule::make_param_env_info() const
{
    ParamEnvInfo info;
    info.predicate_indices = this->make_index_table();
    return info;
}

GenericSideTableLayout CheckedModule::make_generic_side_table_layout(
    const GenericSideTableLocalLayoutView& source) const
{
    GenericSideTableLayout layout;
    layout.expr_span = source.expr_span;
    layout.pattern_span = source.pattern_span;
    layout.type_span = source.type_span;
    layout.stmt_span = source.stmt_span;
    layout.expr_node_ids = this->copy_index_table(source.expr_node_ids);
    layout.pattern_node_ids = this->copy_index_table(source.pattern_node_ids);
    layout.type_node_ids = this->copy_index_table(source.type_node_ids);
    layout.stmt_node_ids = this->copy_index_table(source.stmt_node_ids);
    return layout;
}

base::usize CheckedModule::append_generic_side_table_layout(const GenericSideTableLocalLayoutView& layout)
{
    const base::usize index = this->generic_side_table_layouts.size();
    this->generic_side_table_layouts.push_back(this->make_generic_side_table_layout(layout));
    return index;
}

const GenericSideTableLayout* CheckedModule::generic_side_table_layout(const base::usize index) const noexcept
{
    return index < this->generic_side_table_layouts.size() ? &this->generic_side_table_layouts[index] : nullptr;
}

FunctionSignature CheckedModule::clone_function_signature(const FunctionSignature& other)
{
    FunctionSignature copy = this->make_function_signature();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.semantic_key = other.semantic_key;
    copy.stable_id = other.stable_id;
    copy.incremental_key = other.incremental_key;
    copy.generic_instance_key = other.generic_instance_key;
    copy.c_name = this->intern_text(other.c_name);
    copy.module = other.module;
    copy.method_owner_type = other.method_owner_type;
    copy.trait_module = other.trait_module;
    copy.trait_name_id = other.trait_name_id;
    copy.return_type = other.return_type;
    copy.param_types = this->copy_type_handle_list(other.param_types);
    copy.params = this->copy_function_param_info_list(other.params);
    copy.generic_args = this->copy_type_handle_list(other.generic_args);
    copy.range = other.range;
    copy.is_extern_c = other.is_extern_c;
    copy.is_export_c = other.is_export_c;
    copy.is_unsafe = other.is_unsafe;
    copy.is_variadic = other.is_variadic;
    copy.has_prototype = other.has_prototype;
    copy.has_definition = other.has_definition;
    copy.has_conflict = other.has_conflict;
    copy.is_method = other.is_method;
    copy.has_self_param = other.has_self_param;
    copy.is_trait_impl_method = other.is_trait_impl_method;
    copy.is_trait_default_method_instance = other.is_trait_default_method_instance;
    copy.is_destructor = other.is_destructor;
    copy.visibility = other.visibility;
    copy.prototype_item = other.prototype_item;
    copy.definition_item = other.definition_item;
    copy.part_index = other.part_index;
    return copy;
}

CheckedLambdaInfo CheckedModule::clone_lambda_info(const CheckedLambdaInfo& other)
{
    CheckedLambdaInfo copy = this->make_lambda_info();
    copy.expr = other.expr;
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.c_name = this->intern_text(other.c_name);
    copy.c_name_id = other.c_name_id;
    copy.module = other.module;
    copy.owner_item = other.owner_item;
    copy.type = other.type;
    copy.function_type = other.function_type;
    copy.environment_type = other.environment_type;
    copy.environment_name = this->intern_text(other.environment_name);
    copy.environment_name_id = other.environment_name_id;
    copy.environment_c_name = this->intern_text(other.environment_c_name);
    copy.environment_c_name_id = other.environment_c_name_id;
    copy.return_type = other.return_type;
    copy.param_types = this->copy_type_handle_list(other.param_types);
    copy.body = other.body;
    copy.range = other.range;
    copy.has_unsupported_capture = other.has_unsupported_capture;
    copy.captures.reserve(other.captures.size());
    for (const CheckedLambdaInfo::Capture& capture : other.captures) {
        copy.captures.push_back(CheckedLambdaInfo::Capture{
            this->intern_text(capture.name),
            capture.name_id,
            this->intern_text(capture.field_name),
            capture.field_name_id,
            capture.type,
            capture.field_type,
            capture.kind,
            capture.initializer,
            capture.use_range,
            capture.declaration_range,
        });
    }
    return copy;
}

StructInfo CheckedModule::clone_struct_info(const StructInfo& other)
{
    StructInfo copy = this->make_struct_info();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.c_name = this->intern_text(other.c_name);
    copy.module = other.module;
    copy.type = other.type;
    copy.fields = this->copy_struct_field_list(other.fields);
    copy.is_opaque = other.is_opaque;
    copy.is_generic_placeholder = other.is_generic_placeholder;
    copy.visibility = other.visibility;
    copy.stable_id = other.stable_id;
    copy.incremental_key = other.incremental_key;
    copy.generic_instance_key = other.generic_instance_key;
    copy.part_index = other.part_index;
    return copy;
}

EnumCaseInfo CheckedModule::clone_enum_case_info(const EnumCaseInfo& other)
{
    EnumCaseInfo copy = this->make_enum_case_info();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.c_name = this->intern_text(other.c_name);
    copy.module = other.module;
    copy.type = other.type;
    copy.payload_type = other.payload_type;
    copy.payload_types = this->copy_type_handle_list(other.payload_types);
    copy.value_text = this->intern_text(other.value_text);
    copy.range = other.range;
    copy.enum_name = this->intern_text(other.enum_name);
    copy.case_name = this->intern_text(other.case_name);
    copy.case_name_id = other.case_name_id;
    copy.visibility = other.visibility;
    copy.stable_id = other.stable_id;
    copy.stable_case_key = other.stable_case_key;
    copy.incremental_key = other.incremental_key;
    copy.generic_instance_key = other.generic_instance_key;
    copy.part_index = other.part_index;
    return copy;
}

TraitMethodRequirement CheckedModule::clone_trait_method_requirement(const TraitMethodRequirement& other)
{
    TraitMethodRequirement copy = this->make_trait_method_requirement();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.module = other.module;
    copy.item = other.item;
    copy.return_type = other.return_type;
    copy.param_types = this->copy_type_handle_list(other.param_types);
    copy.default_body = other.default_body;
    copy.range = other.range;
    copy.is_unsafe = other.is_unsafe;
    copy.is_variadic = other.is_variadic;
    copy.has_self_param = other.has_self_param;
    copy.has_default_body = other.has_default_body;
    copy.has_borrow_contract = other.has_borrow_contract;
    copy.visibility = other.visibility;
    copy.stable_key = other.stable_key;
    copy.borrow_contract = other.borrow_contract;
    copy.ordinal = other.ordinal;
    return copy;
}

TraitAssociatedTypeRequirement CheckedModule::clone_trait_associated_type_requirement(
    const TraitAssociatedTypeRequirement& other)
{
    TraitAssociatedTypeRequirement copy = this->make_trait_associated_type_requirement();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.module = other.module;
    copy.item = other.item;
    copy.range = other.range;
    copy.visibility = other.visibility;
    copy.stable_key = other.stable_key;
    copy.member_key = other.member_key;
    copy.ordinal = other.ordinal;
    return copy;
}

TraitSupertraitInfo CheckedModule::clone_trait_supertrait_info(const TraitSupertraitInfo& other)
{
    TraitSupertraitInfo copy = this->make_trait_supertrait_info();
    copy.child_trait_key = other.child_trait_key;
    copy.parent_trait_key = other.parent_trait_key;
    copy.child_trait_name = this->intern_text(other.child_trait_name);
    copy.child_trait_name_id = other.child_trait_name_id;
    copy.child_trait_module = other.child_trait_module;
    copy.parent_trait_name = this->intern_text(other.parent_trait_name);
    copy.parent_trait_name_id = other.parent_trait_name_id;
    copy.parent_trait_module = other.parent_trait_module;
    copy.parent_trait_args = this->copy_type_handle_list(other.parent_trait_args);
    copy.edge_fingerprint = other.edge_fingerprint;
    copy.direct_edge_ordinal = other.direct_edge_ordinal;
    copy.closure_depth = other.closure_depth;
    copy.range = other.range;
    copy.part_index = other.part_index;
    return copy;
}

TraitSignature CheckedModule::clone_trait_signature(const TraitSignature& other)
{
    TraitSignature copy = this->make_trait_signature();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.module = other.module;
    copy.item = other.item;
    copy.visibility = other.visibility;
    copy.stable_id = other.stable_id;
    copy.incremental_key = other.incremental_key;
    copy.generic_params.reserve(other.generic_params.size());
    copy.generic_params.insert(copy.generic_params.end(), other.generic_params.begin(), other.generic_params.end());
    copy.supertraits.reserve(other.supertraits.size());
    for (const TraitSupertraitInfo& supertrait : other.supertraits) {
        copy.supertraits.push_back(this->clone_trait_supertrait_info(supertrait));
    }
    copy.associated_types.reserve(other.associated_types.size());
    for (const TraitAssociatedTypeRequirement& associated_type : other.associated_types) {
        copy.associated_types.push_back(this->clone_trait_associated_type_requirement(associated_type));
    }
    copy.requirements.reserve(other.requirements.size());
    for (const TraitMethodRequirement& requirement : other.requirements) {
        copy.requirements.push_back(this->clone_trait_method_requirement(requirement));
    }
    copy.range = other.range;
    copy.part_index = other.part_index;
    return copy;
}

TraitImplAssociatedTypeInfo CheckedModule::clone_trait_impl_associated_type_info(
    const TraitImplAssociatedTypeInfo& other)
{
    TraitImplAssociatedTypeInfo copy = this->make_trait_impl_associated_type_info();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.item = other.item;
    copy.syntax_type = other.syntax_type;
    copy.value_type = other.value_type;
    copy.member_key = other.member_key;
    copy.requirement_ordinal = other.requirement_ordinal;
    return copy;
}

TraitImplMethodInfo CheckedModule::clone_trait_impl_method_info(const TraitImplMethodInfo& other)
{
    TraitImplMethodInfo copy = this->make_trait_impl_method_info();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.item = other.item;
    copy.function_key = other.function_key;
    copy.requirement_ordinal = other.requirement_ordinal;
    copy.origin = other.origin;
    return copy;
}

TraitImplInfo CheckedModule::clone_trait_impl_info(const TraitImplInfo& other)
{
    TraitImplInfo copy = this->make_trait_impl_info();
    copy.key = other.key;
    copy.trait_name = this->intern_text(other.trait_name);
    copy.trait_name_id = other.trait_name_id;
    copy.trait_module = other.trait_module;
    copy.self_type = other.self_type;
    copy.trait_args = this->copy_type_handle_list(other.trait_args);
    copy.coherence_fingerprint = other.coherence_fingerprint;
    copy.predicate_index = other.predicate_index;
    copy.item = other.item;
    copy.module = other.module;
    copy.visibility = other.visibility;
    copy.stable_id = other.stable_id;
    copy.incremental_key = other.incremental_key;
    copy.associated_types.reserve(other.associated_types.size());
    for (const TraitImplAssociatedTypeInfo& associated_type : other.associated_types) {
        copy.associated_types.push_back(this->clone_trait_impl_associated_type_info(associated_type));
    }
    copy.methods.reserve(other.methods.size());
    for (const TraitImplMethodInfo& method : other.methods) {
        copy.methods.push_back(this->clone_trait_impl_method_info(method));
    }
    copy.range = other.range;
    copy.part_index = other.part_index;
    return copy;
}

TraitPredicate CheckedModule::clone_trait_predicate(const TraitPredicate& other)
{
    TraitPredicate copy = this->make_trait_predicate();
    copy.index = other.index;
    copy.kind = other.kind;
    copy.origin = other.origin;
    copy.subject_type = other.subject_type;
    copy.subject_param_name_id = other.subject_param_name_id;
    copy.subject_param_identity = other.subject_param_identity;
    copy.subject_param_index = other.subject_param_index;
    copy.builtin_capability = other.builtin_capability;
    copy.trait_name = this->intern_text(other.trait_name);
    copy.trait_name_id = other.trait_name_id;
    copy.trait_module = other.trait_module;
    copy.trait_stable_id = other.trait_stable_id;
    copy.trait_args = this->copy_type_handle_list(other.trait_args);
    copy.associated_type_equalities.reserve(other.associated_type_equalities.size());
    for (const TraitImplAssociatedTypeInfo& equality : other.associated_type_equalities) {
        copy.associated_type_equalities.push_back(this->clone_trait_impl_associated_type_info(equality));
    }
    copy.canonical_fingerprint = other.canonical_fingerprint;
    copy.module = other.module;
    copy.item = other.item;
    copy.range = other.range;
    copy.part_index = other.part_index;
    return copy;
}

TraitObligation CheckedModule::clone_trait_obligation(const TraitObligation& other) const
{
    return other;
}

TraitEvidence CheckedModule::clone_trait_evidence(const TraitEvidence& other) const
{
    return other;
}

ForInProtocolCallPlan CheckedModule::clone_for_in_protocol_call_plan(const ForInProtocolCallPlan& other)
{
    ForInProtocolCallPlan copy = other;
    copy.method_name = this->intern_text(other.method_name);
    return copy;
}

ForInIterationPlan CheckedModule::clone_for_in_iteration_plan(const ForInIterationPlan& other)
{
    ForInIterationPlan copy = other;
    copy.iter_call = this->clone_for_in_protocol_call_plan(other.iter_call);
    copy.has_next_call = this->clone_for_in_protocol_call_plan(other.has_next_call);
    copy.next_call = this->clone_for_in_protocol_call_plan(other.next_call);
    return copy;
}

TraitMethodCallBinding CheckedModule::clone_trait_method_call_binding(const TraitMethodCallBinding& other)
{
    TraitMethodCallBinding copy = other;
    copy.method_name = this->intern_text(other.method_name);
    copy.ordered_args = make_sema_vector<syntax::ExprId>(*this->arena_);
    copy.ordered_args.assign(other.ordered_args.begin(), other.ordered_args.end());
    return copy;
}

FunctionCallBinding CheckedModule::clone_function_call_binding(const FunctionCallBinding& other) const
{
    FunctionCallBinding copy = other;
    copy.ordered_args = make_sema_vector<syntax::ExprId>(*this->arena_);
    copy.ordered_args.assign(other.ordered_args.begin(), other.ordered_args.end());
    return copy;
}

TraitObjectMethodSlotFact CheckedModule::clone_trait_object_method_slot_fact(
    const TraitObjectMethodSlotFact& other)
{
    TraitObjectMethodSlotFact copy = other;
    copy.method_name = this->intern_text(other.method_name);
    return copy;
}

TraitObjectCallabilityFact CheckedModule::clone_trait_object_callability_fact(
    const TraitObjectCallabilityFact& other)
{
    TraitObjectCallabilityFact copy = other;
    copy.trait_name = this->intern_text(other.trait_name);
    return copy;
}

VTableLayoutFact CheckedModule::clone_vtable_layout_fact(const VTableLayoutFact& other)
{
    VTableLayoutFact copy = this->make_vtable_layout_fact();
    copy.layout_key = other.layout_key;
    copy.concrete_type = other.concrete_type;
    copy.object_type = other.object_type;
    copy.impl_key = other.impl_key;
    copy.impl_evidence = other.impl_evidence;
    copy.method_slot_count = other.method_slot_count;
    copy.method_slots.reserve(other.method_slots.size());
    for (const VTableMethodSlotFact& slot : other.method_slots) {
        VTableMethodSlotFact slot_copy = slot;
        slot_copy.method_name = this->intern_text(slot.method_name);
        slot_copy.param_types = this->copy_type_handle_list(slot.param_types);
        copy.method_slots.push_back(slot_copy);
    }
    copy.range = other.range;
    copy.part_index = other.part_index;
    return copy;
}

TraitObjectCoercionFact CheckedModule::clone_trait_object_coercion_fact(
    const TraitObjectCoercionFact& other) const
{
    return other;
}

TraitSupertraitEdgeFact CheckedModule::clone_trait_supertrait_edge_fact(const TraitSupertraitEdgeFact& other)
{
    TraitSupertraitEdgeFact copy = this->make_trait_supertrait_edge_fact();
    copy.child_trait_key = other.child_trait_key;
    copy.parent_trait_key = other.parent_trait_key;
    copy.child_trait_name = this->intern_text(other.child_trait_name);
    copy.child_trait_name_id = other.child_trait_name_id;
    copy.child_trait_module = other.child_trait_module;
    copy.parent_trait_name = this->intern_text(other.parent_trait_name);
    copy.parent_trait_name_id = other.parent_trait_name_id;
    copy.parent_trait_module = other.parent_trait_module;
    copy.parent_trait_args = this->copy_type_handle_list(other.parent_trait_args);
    copy.edge_fingerprint = other.edge_fingerprint;
    copy.direct_edge_ordinal = other.direct_edge_ordinal;
    copy.closure_depth = other.closure_depth;
    copy.range = other.range;
    copy.part_index = other.part_index;
    return copy;
}

TraitObjectUpcastCoercionFact CheckedModule::clone_trait_object_upcast_coercion_fact(
    const TraitObjectUpcastCoercionFact& other) const
{
    return other;
}

LifetimeOriginParamInfo CheckedModule::clone_lifetime_origin_param(const LifetimeOriginParamInfo& other)
{
    LifetimeOriginParamInfo copy = other;
    copy.name = this->intern_text(other.name);
    return copy;
}

ReferenceOriginFact CheckedModule::clone_reference_origin_fact(const ReferenceOriginFact& other)
{
    ReferenceOriginFact copy = other;
    copy.origin_names.clear();
    copy.origin_names.reserve(other.origin_names.size());
    for (const InternedText origin : other.origin_names) {
        copy.origin_names.push_back(this->intern_text(origin));
    }
    return copy;
}

TypeLifetimeInfo CheckedModule::clone_type_lifetime_info(const TypeLifetimeInfo& other)
{
    TypeLifetimeInfo copy = other;
    copy.origin_names.clear();
    copy.origin_names.reserve(other.origin_names.size());
    for (const InternedText origin : other.origin_names) {
        copy.origin_names.push_back(this->intern_text(origin));
    }
    return copy;
}

GenericLifetimePredicate CheckedModule::clone_generic_lifetime_predicate(
    const GenericLifetimePredicate& other)
{
    GenericLifetimePredicate copy = other;
    copy.origin_name = this->intern_text(other.origin_name);
    return copy;
}

FunctionLifetimeFacts CheckedModule::clone_function_lifetime_facts(const FunctionLifetimeFacts& other)
{
    FunctionLifetimeFacts copy = other;
    copy.regions.clear();
    copy.regions.reserve(other.regions.size());
    for (LifetimeRegion region : other.regions) {
        region.name = this->intern_text(region.name);
        copy.regions.push_back(region);
    }
    return copy;
}

FunctionDropCheckFacts CheckedModule::clone_function_drop_check_facts(const FunctionDropCheckFacts& other)
{
    return other;
}

FunctionMoveRejectionFacts CheckedModule::clone_function_move_rejection_facts(
    const FunctionMoveRejectionFacts& other) const
{
    return other;
}

std::string_view move_rejection_kind_name(const MoveRejectionKind kind) noexcept
{
    switch (kind) {
        case MoveRejectionKind::indexed_element:
            return "indexed_element";
        case MoveRejectionKind::pattern_payload:
            return "pattern_payload";
        case MoveRejectionKind::try_payload:
            return "try_payload";
    }
    return "<invalid>";
}

query::StableFingerprint128 function_move_rejection_facts_fingerprint(
    const FunctionMoveRejectionFacts& facts) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_MOVE_REJECTION_FACTS_FINGERPRINT_MARKER);
    mix_function_lookup_key(builder, facts.function);
    builder.mix_u32(facts.part_index);
    builder.mix_u64(static_cast<base::u64>(facts.rejections.size()));
    for (const MoveRejectionFact& rejection : facts.rejections) {
        builder.mix_u8(static_cast<base::u8>(rejection.kind));
        builder.mix_u32(rejection.expr.value);
        builder.mix_u32(rejection.stmt.value);
        builder.mix_u32(rejection.pattern.value);
        builder.mix_u32(rejection.tracked_type.value);
        builder.mix_fingerprint(rejection.resource_fingerprint);
        builder.mix_bool(rejection.diagnostic_emitted);
    }
    return builder.finish();
}

std::string summarize_function_move_rejection_facts(const FunctionMoveRejectionFacts& facts)
{
    base::u64 pattern_payload_count = 0;
    base::u64 try_payload_count = 0;
    base::u64 indexed_element_count = 0;
    base::u64 emitted_count = 0;
    for (const MoveRejectionFact& rejection : facts.rejections) {
        if (rejection.diagnostic_emitted) {
            ++emitted_count;
        }
        switch (rejection.kind) {
            case MoveRejectionKind::indexed_element:
                ++indexed_element_count;
                break;
            case MoveRejectionKind::pattern_payload:
                ++pattern_payload_count;
                break;
            case MoveRejectionKind::try_payload:
                ++try_payload_count;
                break;
        }
    }

    std::ostringstream label;
    label << "move_rejection_facts rejections=" << facts.rejections.size()
          << " pattern_payload=" << pattern_payload_count
          << " try_payload=" << try_payload_count
          << " indexed_element=" << indexed_element_count
          << " diagnostics=" << emitted_count
          << " fingerprint=" << query::debug_string(function_move_rejection_facts_fingerprint(facts));
    return label.str();
}

std::string dump_function_move_rejection_facts(const FunctionMoveRejectionFacts& facts)
{
    std::ostringstream stream;
    stream << "move_rejection_facts function=" << facts.function.module << ':' << facts.function.owner_type << ':';
    if (syntax::is_valid(facts.function.name)) {
        stream << '#' << facts.function.name.value;
    } else {
        stream << '-';
    }
    stream << " rejections=" << facts.rejections.size()
           << " fingerprint=" << query::debug_string(function_move_rejection_facts_fingerprint(facts)) << '\n';
    for (base::usize index = 0; index < facts.rejections.size(); ++index) {
        const MoveRejectionFact& rejection = facts.rejections[index];
        stream << "  r" << index << ' ' << move_rejection_kind_name(rejection.kind)
               << " expr=e" << rejection.expr.value
               << " stmt=s" << rejection.stmt.value
               << " pattern=p" << rejection.pattern.value
               << " tracked_type=" << rejection.tracked_type.value
               << " emitted=" << (rejection.diagnostic_emitted ? "true" : "false")
               << " resource=" << query::debug_string(rejection.resource_fingerprint) << '\n';
    }
    return stream.str();
}

DestructorInfo CheckedModule::clone_destructor_info(const DestructorInfo& other) const
{
    return other;
}

query::StableFingerprint128 destructor_info_fingerprint(const DestructorInfo& info) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_DESTRUCTOR_INFO_FINGERPRINT_MARKER);
    builder.mix_u32(info.module.value);
    builder.mix_u32(info.impl_item.value);
    builder.mix_u32(info.method_item.value);
    builder.mix_u32(info.self_type.value);
    mix_function_lookup_key(builder, info.function_key);
    builder.mix_u32(info.part_index);
    return builder.finish();
}

query::StableFingerprint128 checked_destructors_fingerprint(const CheckedModule& checked) noexcept
{
    std::vector<const DestructorInfo*> destructors;
    destructors.reserve(checked.destructors.size());
    for (const auto& entry : checked.destructors) {
        destructors.push_back(&entry.second);
    }
    std::sort(destructors.begin(), destructors.end(), [](const DestructorInfo* lhs, const DestructorInfo* rhs) {
        return lhs->self_type.value < rhs->self_type.value;
    });

    query::StableHashBuilder builder;
    builder.mix_string(SEMA_DESTRUCTORS_FINGERPRINT_MARKER);
    builder.mix_u64(static_cast<base::u64>(destructors.size()));
    for (const DestructorInfo* const info : destructors) {
        builder.mix_fingerprint(destructor_info_fingerprint(*info));
    }
    return builder.finish();
}

query::StableFingerprint128 trait_object_facts_fingerprint(const CheckedModule& checked) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_TRAIT_OBJECT_FACTS_FINGERPRINT_MARKER);
    builder.mix_u64(static_cast<base::u64>(checked.trait_object_callability.size()));
    for (const TraitObjectCallabilityFact& fact : checked.trait_object_callability) {
        builder.mix_u64(fact.object_type_key.global_id);
        builder.mix_u32(fact.object_type.value);
        builder.mix_u32(fact.trait_module.value);
        builder.mix_u32(fact.trait_name_id.value);
        builder.mix_u32(fact.method_slot_count);
        builder.mix_fingerprint(fact.slot_schema);
    }
    builder.mix_u64(static_cast<base::u64>(checked.trait_object_method_slots.size()));
    for (const TraitObjectMethodSlotFact& fact : checked.trait_object_method_slots) {
        builder.mix_u64(fact.object_type_key.global_id);
        builder.mix_u32(fact.object_type.value);
        builder.mix_u32(fact.trait_module.value);
        builder.mix_u32(fact.trait_name_id.value);
        builder.mix_u32(fact.method_name_id.value);
        builder.mix_u32(fact.requirement_ordinal);
        builder.mix_u32(fact.slot);
        builder.mix_u32(fact.receiver_type.value);
        builder.mix_u32(fact.return_type.value);
        builder.mix_u8(static_cast<base::u8>(fact.receiver_access));
        builder.mix_fingerprint(fact.slot_schema);
    }
    builder.mix_u64(static_cast<base::u64>(checked.vtable_layouts.size()));
    for (const VTableLayoutFact& fact : checked.vtable_layouts) {
        builder.mix_u64(fact.layout_key.global_id);
        builder.mix_u32(fact.concrete_type.value);
        builder.mix_u32(fact.object_type.value);
        builder.mix_u32(fact.impl_key.trait_module);
        builder.mix_u32(fact.impl_key.trait_name.value);
        builder.mix_u32(fact.impl_key.self_type);
        builder.mix_fingerprint(fact.impl_key.trait_args);
        builder.mix_fingerprint(fact.impl_evidence);
        builder.mix_u32(fact.method_slot_count);
        builder.mix_u64(static_cast<base::u64>(fact.method_slots.size()));
        for (const VTableMethodSlotFact& slot : fact.method_slots) {
            builder.mix_u64(slot.object_type_key.global_id);
            builder.mix_u32(slot.concrete_type.value);
            builder.mix_u32(slot.object_type.value);
            builder.mix_u32(slot.method_name_id.value);
            mix_function_lookup_key(builder, slot.function_key);
            builder.mix_u32(slot.requirement_ordinal);
            builder.mix_u32(slot.slot);
            builder.mix_u32(slot.receiver_type.value);
            builder.mix_u32(slot.return_type.value);
            builder.mix_u64(static_cast<base::u64>(slot.param_types.size()));
            for (const TypeHandle param : slot.param_types) {
                builder.mix_u32(param.value);
            }
            builder.mix_u8(static_cast<base::u8>(slot.receiver_access));
            builder.mix_u8(static_cast<base::u8>(slot.origin));
        }
    }
    builder.mix_u64(static_cast<base::u64>(checked.trait_object_coercions.size()));
    for (const TraitObjectCoercionFact& fact : checked.trait_object_coercions) {
        builder.mix_u64(fact.coercion_key.global_id);
        builder.mix_u32(fact.expr.value);
        builder.mix_u32(fact.source_reference_type.value);
        builder.mix_u32(fact.target_reference_type.value);
        builder.mix_u32(fact.source_type.value);
        builder.mix_u32(fact.object_type.value);
        builder.mix_u64(fact.vtable_layout.global_id);
        builder.mix_u8(static_cast<base::u8>(fact.borrow_kind));
    }
    builder.mix_u64(static_cast<base::u64>(checked.trait_supertrait_edges.size()));
    for (const TraitSupertraitEdgeFact& fact : checked.trait_supertrait_edges) {
        builder.mix_u64(fact.child_trait_key.global_id);
        builder.mix_u64(fact.parent_trait_key.global_id);
        builder.mix_u32(fact.child_trait_module.value);
        builder.mix_u32(fact.child_trait_name_id.value);
        builder.mix_u32(fact.parent_trait_module.value);
        builder.mix_u32(fact.parent_trait_name_id.value);
        builder.mix_u64(static_cast<base::u64>(fact.parent_trait_args.size()));
        for (const TypeHandle arg : fact.parent_trait_args) {
            builder.mix_u32(arg.value);
        }
        builder.mix_fingerprint(fact.edge_fingerprint);
        builder.mix_u32(fact.direct_edge_ordinal);
        builder.mix_u32(fact.closure_depth);
    }
    builder.mix_u64(static_cast<base::u64>(checked.trait_object_upcast_coercions.size()));
    for (const TraitObjectUpcastCoercionFact& fact : checked.trait_object_upcast_coercions) {
        builder.mix_u64(fact.upcast_key.global_id);
        builder.mix_u32(fact.expr.value);
        builder.mix_u32(fact.source_reference_type.value);
        builder.mix_u32(fact.target_reference_type.value);
        builder.mix_u32(fact.source_object_type.value);
        builder.mix_u32(fact.target_object_type.value);
        builder.mix_u64(fact.source_vtable_layout.global_id);
        builder.mix_u64(fact.target_vtable_layout.global_id);
        builder.mix_fingerprint(fact.edge_fingerprint);
        builder.mix_u8(static_cast<base::u8>(fact.borrow_kind));
    }
    builder.mix_fingerprint(
        query::principal_set_composition_facts_fingerprint(checked.principal_set_composition_facts));
    return builder.finish();
}

FunctionPlaceStateFacts CheckedModule::clone_function_place_state_facts(const FunctionPlaceStateFacts& other)
{
    return other;
}

ParamEnvInfo CheckedModule::clone_param_env_info(const ParamEnvInfo& other)
{
    ParamEnvInfo copy = this->make_param_env_info();
    copy.module = other.module;
    copy.item = other.item;
    copy.owner_name = this->intern_text(other.owner_name);
    copy.owner_name_id = other.owner_name_id;
    copy.owner_stable_id = other.owner_stable_id;
    copy.key = other.key;
    copy.predicate_indices = this->copy_index_table(other.predicate_indices);
    copy.range = other.range;
    copy.part_index = other.part_index;
    return copy;
}

GenericTemplateSignatureInfo CheckedModule::clone_generic_template_signature_info(
    const GenericTemplateSignatureInfo& other)
{
    return GenericTemplateSignatureInfo{
        this->intern_text(other.name),
        other.name_id,
        other.module,
        other.visibility,
        other.stable_id,
        other.incremental_key,
        other.name_space,
        other.param_count,
        other.constraint_count,
        other.part_index,
    };
}

GenericSideTableLayout CheckedModule::clone_generic_side_table_layout(const GenericSideTableLayout& other) const
{
    return this->make_generic_side_table_layout(GenericSideTableLocalLayoutView{
        other.expr_span,
        other.pattern_span,
        other.type_span,
        other.stmt_span,
        other.expr_node_ids,
        other.pattern_node_ids,
        other.type_node_ids,
        other.stmt_node_ids,
    });
}

GenericEnumInstanceInfo CheckedModule::clone_generic_enum_instance(const GenericEnumInstanceInfo& other) const
{
    return other;
}

GenericTypeAliasInstanceInfo CheckedModule::clone_generic_type_alias_instance(
    const GenericTypeAliasInstanceInfo& other) const
{
    return other;
}

GenericFunctionInstanceInfo CheckedModule::clone_generic_function_instance(const GenericFunctionInstanceInfo& other)
{
    GenericFunctionInstanceInfo copy;
    copy.key = other.key;
    copy.item = other.item;
    copy.body = other.body;
    copy.generic_instance_key = other.generic_instance_key;
    copy.signature = this->clone_function_signature(other.signature);
    copy.side_table_layout_index = other.side_table_layout_index;
    copy.side_tables = other.side_tables;
    return copy;
}

TraitDefaultMethodInstanceInfo CheckedModule::clone_trait_default_method_instance(
    const TraitDefaultMethodInstanceInfo& other)
{
    TraitDefaultMethodInstanceInfo copy;
    copy.key = other.key;
    copy.item = other.item;
    copy.body = other.body;
    copy.impl_key = other.impl_key;
    copy.trait_module = other.trait_module;
    copy.trait_name_id = other.trait_name_id;
    copy.requirement_ordinal = other.requirement_ordinal;
    copy.signature = this->clone_function_signature(other.signature);
    copy.side_table_layout_index = other.side_table_layout_index;
    copy.side_tables = other.side_tables;
    return copy;
}

GenericFunctionInstanceBodyView CheckedModule::generic_function_instance_body_view(
    const syntax::AstModule& ast, const base::usize index) const noexcept
{
    if (index >= this->generic_function_instances.size()) {
        return {};
    }
    return this->generic_function_instance_body_view(ast, this->generic_function_instances[index]);
}

GenericFunctionInstanceBodyView CheckedModule::generic_function_instance_body_view(
    const syntax::AstModule& ast, const GenericFunctionInstanceInfo& instance) const noexcept
{
    if (!syntax::is_valid(instance.item) || instance.item.value >= ast.items.size()) {
        return {};
    }
    const syntax::ItemNode* const item = ast.items.ptr(instance.item.value);
    if (item == nullptr || item->kind != syntax::ItemKind::fn_decl) {
        return {};
    }
    if (!syntax::is_valid(instance.body) || instance.body.value >= ast.stmts.size()) {
        return {};
    }
    return GenericFunctionInstanceBodyView{
        &instance,
        &instance.signature,
        &instance.side_tables,
        item,
        instance.body,
    };
}

TraitDefaultMethodInstanceBodyView CheckedModule::trait_default_method_instance_body_view(
    const syntax::AstModule& ast, const base::usize index) const noexcept
{
    if (index >= this->trait_default_method_instances.size()) {
        return {};
    }
    return this->trait_default_method_instance_body_view(ast, this->trait_default_method_instances[index]);
}

TraitDefaultMethodInstanceBodyView CheckedModule::trait_default_method_instance_body_view(
    const syntax::AstModule& ast, const TraitDefaultMethodInstanceInfo& instance) const noexcept
{
    if (!syntax::is_valid(instance.item) || instance.item.value >= ast.items.size()) {
        return {};
    }
    const syntax::ItemNode* const item = ast.items.ptr(instance.item.value);
    if (item == nullptr || item->kind != syntax::ItemKind::fn_decl || !item->is_trait_default_method) {
        return {};
    }
    if (!syntax::is_valid(instance.body) || instance.body.value >= ast.stmts.size()) {
        return {};
    }
    return TraitDefaultMethodInstanceBodyView{
        &instance,
        &instance.signature,
        &instance.side_tables,
        item,
        instance.body,
    };
}

void CheckedModule::prepare_analysis_only_storage(const base::usize expr_count)
{
    this->expr_expected_types = SemaTypeTable{};
    this->analysis_arena_ = std::make_unique<base::BumpAllocator>(SEMA_GENERIC_SIDE_TABLE_BLOCK_BYTES);
    this->expr_expected_types = make_sema_vector<TypeHandle>(*this->analysis_arena_);
    this->expr_expected_types.reserve(expr_count);
}

void CheckedModule::release_analysis_only_storage()
{
    this->expr_expected_types = SemaTypeTable{};
    this->analysis_arena_.reset();
    this->pattern_case_name_ids = PatternCaseNameTable{};
    for (GenericFunctionInstanceInfo& instance : this->generic_function_instances) {
        instance.side_tables.release_analysis_only_storage();
    }
    for (TraitDefaultMethodInstanceInfo& instance : this->trait_default_method_instances) {
        instance.side_tables.release_analysis_only_storage();
    }
}

namespace {

void rebind_function_signature_texts(
    FunctionSignature& signature, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(signature.name, from, to);
    rebind_interned_text(signature.c_name, from, to);
    for (FunctionParamInfo& param : signature.params) {
        rebind_interned_text(param.name, from, to);
    }
}

void rebind_struct_info_texts(
    StructInfo& info, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(info.name, from, to);
    rebind_interned_text(info.c_name, from, to);
    for (StructFieldInfo& field : info.fields) {
        rebind_interned_text(field.name, from, to);
        rebind_interned_text(field.c_name, from, to);
    }
}

void rebind_enum_case_info_texts(
    EnumCaseInfo& info, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(info.name, from, to);
    rebind_interned_text(info.c_name, from, to);
    rebind_interned_text(info.value_text, from, to);
    rebind_interned_text(info.enum_name, from, to);
    rebind_interned_text(info.case_name, from, to);
}

void rebind_lambda_info_texts(
    CheckedLambdaInfo& info, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(info.name, from, to);
    rebind_interned_text(info.c_name, from, to);
    rebind_interned_text(info.environment_name, from, to);
    rebind_interned_text(info.environment_c_name, from, to);
    for (CheckedLambdaInfo::Capture& capture : info.captures) {
        rebind_interned_text(capture.name, from, to);
        rebind_interned_text(capture.field_name, from, to);
    }
}

void rebind_trait_signature_texts(
    TraitSignature& signature, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(signature.name, from, to);
    for (TraitAssociatedTypeRequirement& associated_type : signature.associated_types) {
        rebind_interned_text(associated_type.name, from, to);
    }
    for (TraitMethodRequirement& requirement : signature.requirements) {
        rebind_interned_text(requirement.name, from, to);
        for (FunctionParamInfo& param : requirement.params) {
            rebind_interned_text(param.name, from, to);
        }
    }
}

void rebind_trait_impl_info_texts(
    TraitImplInfo& info, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(info.trait_name, from, to);
    for (TraitImplAssociatedTypeInfo& associated_type : info.associated_types) {
        rebind_interned_text(associated_type.name, from, to);
    }
    for (TraitImplMethodInfo& method : info.methods) {
        rebind_interned_text(method.name, from, to);
    }
}

void rebind_trait_predicate_texts(
    TraitPredicate& predicate, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(predicate.trait_name, from, to);
    for (TraitImplAssociatedTypeInfo& equality : predicate.associated_type_equalities) {
        rebind_interned_text(equality.name, from, to);
    }
}

void rebind_param_env_info_texts(
    ParamEnvInfo& param_env, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(param_env.owner_name, from, to);
}

void rebind_trait_method_call_binding_texts(
    TraitMethodCallBinding& binding, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(binding.method_name, from, to);
}

void rebind_trait_object_method_slot_fact_texts(
    TraitObjectMethodSlotFact& fact, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(fact.method_name, from, to);
}

void rebind_trait_object_callability_fact_texts(
    TraitObjectCallabilityFact& fact, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(fact.trait_name, from, to);
}

void rebind_vtable_layout_fact_texts(
    VTableLayoutFact& fact, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    for (VTableMethodSlotFact& slot : fact.method_slots) {
        rebind_interned_text(slot.method_name, from, to);
    }
}

void rebind_trait_supertrait_info_texts(
    TraitSupertraitInfo& info, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(info.child_trait_name, from, to);
    rebind_interned_text(info.parent_trait_name, from, to);
}

void rebind_trait_supertrait_edge_fact_texts(
    TraitSupertraitEdgeFact& fact, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(fact.child_trait_name, from, to);
    rebind_interned_text(fact.parent_trait_name, from, to);
}

} // namespace

void CheckedModule::rebind_interned_texts(const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    for (auto& entry : this->functions) {
        rebind_function_signature_texts(entry.second, from, to);
    }
    for (auto& entry : this->structs) {
        rebind_struct_info_texts(entry.second, from, to);
    }
    for (auto& entry : this->enum_cases) {
        rebind_enum_case_info_texts(entry.second, from, to);
    }
    for (CheckedLambdaInfo& lambda : this->lambdas) {
        rebind_lambda_info_texts(lambda, from, to);
    }
    for (auto& entry : this->type_aliases) {
        rebind_interned_text(entry.second.name, from, to);
    }
    for (auto& entry : this->traits) {
        rebind_trait_signature_texts(entry.second, from, to);
        for (TraitSupertraitInfo& supertrait : entry.second.supertraits) {
            rebind_trait_supertrait_info_texts(supertrait, from, to);
        }
    }
    for (auto& entry : this->trait_impls) {
        rebind_trait_impl_info_texts(entry.second, from, to);
    }
    for (TraitPredicate& predicate : this->trait_predicates) {
        rebind_trait_predicate_texts(predicate, from, to);
    }
    for (TraitMethodCallBinding& binding : this->trait_method_calls) {
        rebind_trait_method_call_binding_texts(binding, from, to);
    }
    for (TraitObjectMethodSlotFact& fact : this->trait_object_method_slots) {
        rebind_trait_object_method_slot_fact_texts(fact, from, to);
    }
    for (TraitObjectCallabilityFact& fact : this->trait_object_callability) {
        rebind_trait_object_callability_fact_texts(fact, from, to);
    }
    for (VTableLayoutFact& fact : this->vtable_layouts) {
        rebind_vtable_layout_fact_texts(fact, from, to);
    }
    for (TraitSupertraitEdgeFact& fact : this->trait_supertrait_edges) {
        rebind_trait_supertrait_edge_fact_texts(fact, from, to);
    }
    for (LifetimeOriginParamInfo& origin_param : this->lifetime_origin_params) {
        rebind_interned_text(origin_param.name, from, to);
    }
    for (ReferenceOriginFact& fact : this->reference_origin_facts) {
        for (InternedText& origin : fact.origin_names) {
            rebind_interned_text(origin, from, to);
        }
    }
    for (TypeLifetimeInfo& info : this->type_lifetime_infos) {
        for (InternedText& origin : info.origin_names) {
            rebind_interned_text(origin, from, to);
        }
    }
    for (GenericLifetimePredicate& predicate : this->generic_lifetime_predicates) {
        rebind_interned_text(predicate.origin_name, from, to);
    }
    for (auto& entry : this->lifetime_facts) {
        for (LifetimeRegion& region : entry.second.regions) {
            rebind_interned_text(region.name, from, to);
        }
    }
    for (ParamEnvInfo& param_env : this->param_envs) {
        rebind_param_env_info_texts(param_env, from, to);
    }
    for (GenericTemplateSignatureInfo& signature : this->generic_template_signatures) {
        rebind_interned_text(signature.name, from, to);
    }
    for (GenericFunctionInstanceInfo& instance : this->generic_function_instances) {
        rebind_function_signature_texts(instance.signature, from, to);
    }
    for (TraitDefaultMethodInstanceInfo& instance : this->trait_default_method_instances) {
        rebind_function_signature_texts(instance.signature, from, to);
    }
}

void CheckedModule::rebind_generic_instance_layouts() noexcept
{
    for (GenericFunctionInstanceInfo& instance : this->generic_function_instances) {
        const GenericSideTableLayout* const layout = this->generic_side_table_layout(instance.side_table_layout_index);
        if (layout != nullptr && instance.side_tables.local_dense) {
            instance.side_tables.bind_local_dense_layout(*layout);
        }
    }
    for (TraitDefaultMethodInstanceInfo& instance : this->trait_default_method_instances) {
        const GenericSideTableLayout* const layout = this->generic_side_table_layout(instance.side_table_layout_index);
        if (layout != nullptr && instance.side_tables.local_dense) {
            instance.side_tables.bind_local_dense_layout(*layout);
        }
    }
}

void CheckedModule::reserve_side_table_storage(const base::usize expr_count, const base::usize pattern_count,
    const base::usize type_count, const base::usize stmt_count, const base::usize item_count) const
{
    const base::usize type_handle_slots = (expr_count * 2U) + type_count + stmt_count;
    const base::usize ident_slots = expr_count + pattern_count + item_count;
    const base::usize bytes = type_handle_slots * sizeof(TypeHandle) + ident_slots * sizeof(IdentId);
    this->arena_->reserve_touched(bytes);
}

bool is_valid(const GenericFunctionInstanceBodyView& view) noexcept
{
    return view.instance != nullptr && view.signature != nullptr && view.side_tables != nullptr && view.item != nullptr
        && view.item->kind == syntax::ItemKind::fn_decl && syntax::is_valid(view.body)
        && query::is_valid(view.instance->generic_instance_key)
        && view.signature->generic_instance_key == view.instance->generic_instance_key && view.signature->has_definition
        && !view.signature->has_conflict;
}

bool is_valid(const TraitDefaultMethodInstanceBodyView& view) noexcept
{
    return view.instance != nullptr && view.signature != nullptr && view.side_tables != nullptr && view.item != nullptr
        && view.item->kind == syntax::ItemKind::fn_decl && view.item->is_trait_default_method
        && syntax::is_valid(view.body) && is_valid(view.instance->impl_key) && view.signature->has_definition
        && !view.signature->has_conflict && view.signature->is_trait_default_method_instance;
}

namespace {

constexpr base::u32 SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX = 0;

[[nodiscard]] std::string_view generic_template_namespace_name(const query::DefNamespace name_space) noexcept
{
    switch (name_space) {
        case query::DefNamespace::value:
            return "value";
        case query::DefNamespace::type:
            return "type";
        case query::DefNamespace::member:
            return "member";
        case query::DefNamespace::trait_:
            return "trait";
        case query::DefNamespace::impl_:
            return "impl";
        case query::DefNamespace::synthetic:
            return "synthetic";
    }
    return "unknown";
}

[[nodiscard]] bool checked_has_non_primary_parts(const CheckedModule& checked)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const auto& entry : checked.structs) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const auto& entry : checked.type_aliases) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const auto& entry : checked.enum_cases) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const auto& entry : checked.traits) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const auto& entry : checked.trait_impls) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const TraitPredicate& predicate : checked.trait_predicates) {
        if (predicate.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const TraitMethodCallBinding& binding : checked.trait_method_calls) {
        if (binding.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const FunctionCallBinding& binding : checked.function_calls) {
        if (binding.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const auto& entry : checked.borrow_summaries) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const auto& entry : checked.move_rejection_facts) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const auto& entry : checked.destructors) {
        if (entry.second.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const ParamEnvInfo& param_env : checked.param_envs) {
        if (param_env.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    for (const GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        if (info.part_index != SEMA_CHECKED_DUMP_PRIMARY_PART_INDEX) {
            return true;
        }
    }
    return false;
}

void append_part_origin(std::ostringstream& out, const bool show_parts, const base::u32 part_index)
{
    if (show_parts) {
        out << " @part=" << part_index;
    }
}

[[nodiscard]] std::span<const TypeHandle> generic_args_for_type(const TypeTable& types, const TypeHandle type)
{
    if (!is_valid(type) || type.value >= types.size()) {
        return {};
    }
    return types.get(type).generic_args;
}

[[nodiscard]] std::string trait_signature_display_name(const CheckedModule& checked, const TraitSignature& trait)
{
    static_cast<void>(checked);
    std::string display = std::string(trait.name);
    if (!trait.generic_params.empty()) {
        display.push_back('<');
        for (base::usize index = 0; index < trait.generic_params.size(); ++index) {
            if (index > 0) {
                display.append(", ");
            }
            display.push_back('T');
            display += std::to_string(index);
        }
        display.push_back('>');
    }
    return display;
}

[[nodiscard]] std::string trait_impl_display_name(const CheckedModule& checked, const TraitImplInfo& info)
{
    return checked.types.display_name(info.trait_name, info.trait_args);
}

void append_param_info_list(std::ostringstream& out, const CheckedModule& checked,
    const std::span<const FunctionParamInfo> params, const std::span<const TypeHandle> param_types)
{
    for (base::usize index = 0; index < param_types.size(); ++index) {
        if (index != 0) {
            out << ", ";
        }
        if (index < params.size() && !params[index].name.empty()) {
            out << params[index].name << ":";
        }
        out << checked.types.display_name(param_types[index]);
        if (index < params.size() && syntax::is_valid(params[index].default_value)) {
            out << "=e" << params[index].default_value.value;
        }
    }
}

void append_param_type_list(
    std::ostringstream& out, const CheckedModule& checked, const std::span<const TypeHandle> param_types)
{
    for (base::usize index = 0; index < param_types.size(); ++index) {
        if (index != 0) {
            out << ", ";
        }
        out << checked.types.display_name(param_types[index]);
    }
}

void append_ordered_arg_list(std::ostringstream& out, const std::span<const syntax::ExprId> ordered_args)
{
    if (ordered_args.empty()) {
        return;
    }
    out << " ordered_args=[";
    for (base::usize index = 0; index < ordered_args.size(); ++index) {
        if (index != 0) {
            out << ", ";
        }
        out << "e" << ordered_args[index].value;
    }
    out << "]";
}

[[nodiscard]] std::string_view trait_predicate_kind_name(const TraitPredicateKind kind) noexcept
{
    switch (kind) {
        case TraitPredicateKind::builtin:
            return "builtin";
        case TraitPredicateKind::declared_trait:
            return "trait";
    }
    return "trait";
}

[[nodiscard]] std::string_view trait_predicate_origin_name(const TraitPredicateOrigin origin) noexcept
{
    switch (origin) {
        case TraitPredicateOrigin::explicit_where:
            return "where";
        case TraitPredicateOrigin::explicit_impl:
            return "impl";
        case TraitPredicateOrigin::trait_self:
            return "trait_self";
    }
    return "where";
}

[[nodiscard]] std::string_view trait_evidence_kind_name(const TraitEvidenceKind kind) noexcept
{
    switch (kind) {
        case TraitEvidenceKind::param_env:
            return "param_env";
        case TraitEvidenceKind::builtin:
            return "builtin";
        case TraitEvidenceKind::explicit_impl:
            return "impl";
    }
    return "param_env";
}

[[nodiscard]] std::string_view trait_method_dispatch_kind_name(const TraitMethodDispatchKind kind) noexcept
{
    switch (kind) {
        case TraitMethodDispatchKind::param_env:
            return "param_env";
        case TraitMethodDispatchKind::impl_override:
            return "impl_override";
        case TraitMethodDispatchKind::trait_default:
            return "trait_default";
        case TraitMethodDispatchKind::vtable_slot:
            return "vtable_slot";
    }
    return "param_env";
}

[[nodiscard]] std::string_view trait_impl_method_origin_name(const TraitImplMethodOrigin origin) noexcept
{
    switch (origin) {
        case TraitImplMethodOrigin::impl_override:
            return "impl_override";
        case TraitImplMethodOrigin::trait_default:
            return "trait_default";
    }
    return "impl_override";
}

[[nodiscard]] std::string trait_predicate_trait_name(const CheckedModule& checked, const TraitPredicate& predicate)
{
    if (predicate.kind == TraitPredicateKind::builtin) {
        return std::string(capability_name(predicate.builtin_capability));
    }
    return checked.types.display_name(predicate.trait_name, predicate.trait_args);
}

[[nodiscard]] std::string_view checked_borrow_contract_source_name(const FunctionBorrowContractSource source) noexcept
{
    switch (source) {
        case FunctionBorrowContractSource::inferred:
            return "inferred";
        case FunctionBorrowContractSource::declared:
            return "declared";
        case FunctionBorrowContractSource::conservative_unknown:
            return "conservative_unknown";
    }
    return "<invalid>";
}

[[nodiscard]] std::string_view checked_borrow_contract_selector_kind_name(
    const BorrowContractSelectorKind kind) noexcept
{
    switch (kind) {
        case BorrowContractSelectorKind::parameter:
            return "parameter";
        case BorrowContractSelectorKind::self:
            return "self";
        case BorrowContractSelectorKind::static_:
            return "static";
        case BorrowContractSelectorKind::unknown:
            return "unknown";
    }
    return "<invalid>";
}

} // namespace

void populate_type_check_body_borrow_authority(
    query::TypeCheckBodyAuthority& authority, const CheckedModule& checked, const FunctionLookupKey function)
{
    authority.destructor_count = static_cast<base::u64>(checked.destructors.size());
    if (!checked.destructors.empty()) {
        authority.has_destructor_facts = true;
        authority.destructor_fingerprint = checked_destructors_fingerprint(checked);
    }
    authority.trait_object_method_slot_count = static_cast<base::u64>(checked.trait_object_method_slots.size());
    authority.trait_object_callability_count = static_cast<base::u64>(checked.trait_object_callability.size());
    authority.vtable_layout_count = static_cast<base::u64>(checked.vtable_layouts.size());
    authority.trait_object_coercion_count = static_cast<base::u64>(checked.trait_object_coercions.size());
    authority.trait_supertrait_edge_count = static_cast<base::u64>(checked.trait_supertrait_edges.size());
    authority.trait_object_upcast_coercion_count =
        static_cast<base::u64>(checked.trait_object_upcast_coercions.size());
    authority.principal_set_composition_count =
        checked.principal_set_composition_facts.summary.principal_set_count;
    authority.principal_set_composition_principal_count =
        checked.principal_set_composition_facts.summary.principal_count;
    authority.principal_set_composition_projection_count =
        checked.principal_set_composition_facts.summary.projection_count;
    if (authority.trait_object_method_slot_count != 0 || authority.trait_object_callability_count != 0
        || authority.vtable_layout_count != 0 || authority.trait_object_coercion_count != 0
        || authority.trait_supertrait_edge_count != 0 || authority.trait_object_upcast_coercion_count != 0
        || authority.principal_set_composition_count != 0) {
        authority.has_trait_object_facts = true;
        authority.trait_object_fingerprint = trait_object_facts_fingerprint(checked);
    }
    if (authority.principal_set_composition_count != 0) {
        authority.has_principal_set_composition_facts = true;
        authority.principal_set_composition_fingerprint =
            query::principal_set_composition_facts_fingerprint(checked.principal_set_composition_facts);
    }
    if (const auto summary = checked.borrow_summaries.find(function); summary != checked.borrow_summaries.end()) {
        authority.has_borrow_summary = true;
        authority.borrow_summary_fingerprint = summary->second.fingerprint;
        authority.borrow_summary_origin_count = static_cast<base::u64>(summary->second.origins.size());
        authority.borrow_summary_dependency_count = static_cast<base::u64>(summary->second.return_origins.size());
        authority.borrow_summary_storage_escape_count =
            static_cast<base::u64>(summary->second.storage_escapes.size());
        authority.borrow_summary_has_unknown_return_origin = summary->second.has_unknown_return_origin;
        authority.borrow_summary_has_local_return_escape = summary->second.has_local_return_escape;
        authority.borrow_summary_has_storage_escape = !summary->second.storage_escapes.empty();
    }
    if (const auto contract = checked.borrow_contracts.find(function); contract != checked.borrow_contracts.end()) {
        authority.has_borrow_contract = true;
        authority.borrow_contract_fingerprint = contract->second.fingerprint;
        authority.borrow_contract_selector_count = static_cast<base::u64>(contract->second.return_selectors.size());
        authority.borrow_contract_unknown_return_allowed = contract->second.unknown_return_allowed;
        authority.borrow_contract_has_local_return_escape = contract->second.has_local_return_escape;
        authority.borrow_contract_has_mismatch = contract->second.has_contract_mismatch;
    }
    if (const auto move_rejections = checked.move_rejection_facts.find(function);
        move_rejections != checked.move_rejection_facts.end()) {
        authority.has_move_rejection_facts = true;
        authority.move_rejection_fingerprint = function_move_rejection_facts_fingerprint(move_rejections->second);
        authority.move_rejection_count = static_cast<base::u64>(move_rejections->second.rejections.size());
        for (const MoveRejectionFact& rejection : move_rejections->second.rejections) {
            authority.move_rejection_has_emitted_diagnostics =
                authority.move_rejection_has_emitted_diagnostics || rejection.diagnostic_emitted;
            switch (rejection.kind) {
                case MoveRejectionKind::indexed_element:
                    ++authority.move_rejection_indexed_element_count;
                    authority.move_rejection_has_indexed_element = true;
                    break;
                case MoveRejectionKind::pattern_payload:
                    ++authority.move_rejection_pattern_payload_count;
                    authority.move_rejection_has_pattern_payload = true;
                    break;
                case MoveRejectionKind::try_payload:
                    ++authority.move_rejection_try_payload_count;
                    authority.move_rejection_has_try_payload = true;
                    break;
            }
        }
    }
    if (const auto lifetime = checked.lifetime_facts.find(function); lifetime != checked.lifetime_facts.end()) {
        authority.has_lifetime_facts = true;
        authority.lifetime_fingerprint = function_lifetime_facts_fingerprint(lifetime->second);
        authority.lifetime_region_count = static_cast<base::u64>(lifetime->second.regions.size());
        authority.lifetime_outlives_constraint_count =
            static_cast<base::u64>(lifetime->second.outlives_constraints.size());
        authority.lifetime_type_outlives_constraint_count =
            static_cast<base::u64>(lifetime->second.type_outlives_constraints.size());
        authority.lifetime_live_range_count = static_cast<base::u64>(lifetime->second.live_ranges.size());
        authority.lifetime_return_region_count = static_cast<base::u64>(lifetime->second.return_regions.size());
        authority.lifetime_violation_count = static_cast<base::u64>(lifetime->second.violations.size());
        authority.type_lifetime_info_count = static_cast<base::u64>(checked.type_lifetime_infos.size());
        authority.generic_lifetime_predicate_count =
            static_cast<base::u64>(checked.generic_lifetime_predicates.size());
        authority.lifetime_has_emitted_diagnostics =
            std::ranges::any_of(lifetime->second.violations, [](const LifetimeViolation& violation) {
                return violation.diagnostic_emitted;
            });
        authority.lifetime_has_unknown_origin =
            std::ranges::any_of(lifetime->second.violations, [](const LifetimeViolation& violation) {
                return violation.kind == LifetimeViolationKind::unknown_origin;
            });
        authority.lifetime_has_ambiguous_elision =
            std::ranges::any_of(lifetime->second.violations, [](const LifetimeViolation& violation) {
                return violation.kind == LifetimeViolationKind::ambiguous_elision;
            });
        authority.lifetime_has_return_origin_mismatch =
            std::ranges::any_of(lifetime->second.violations, [](const LifetimeViolation& violation) {
                return violation.kind == LifetimeViolationKind::return_origin_outside_type;
            });
        authority.lifetime_has_local_escape =
            std::ranges::any_of(lifetime->second.violations, [](const LifetimeViolation& violation) {
                return violation.kind == LifetimeViolationKind::local_escape;
            });
        authority.lifetime_has_unknown_escape =
            std::ranges::any_of(lifetime->second.violations, [](const LifetimeViolation& violation) {
                return violation.kind == LifetimeViolationKind::unknown_escape;
            });
    }
    if (const auto dropck = checked.dropck_facts.find(function); dropck != checked.dropck_facts.end()) {
        authority.has_dropck_facts = true;
        authority.dropck_fingerprint = function_drop_check_facts_fingerprint(dropck->second);
        authority.dropck_fact_count = static_cast<base::u64>(dropck->second.facts.size());
        authority.dropck_action_count = static_cast<base::u64>(dropck->second.actions.size());
        authority.dropck_required_outlives_count = 0;
        for (const DropCheckFact& fact : dropck->second.facts) {
            authority.dropck_required_outlives_count += static_cast<base::u64>(fact.required_outlives.size());
        }
        authority.dropck_violation_count = static_cast<base::u64>(dropck->second.violations.size());
        authority.dropck_graph_missing = dropck->second.graph_missing;
        authority.dropck_has_emitted_diagnostics =
            std::ranges::any_of(dropck->second.violations, [](const DropCheckViolation& violation) {
                return violation.diagnostic_emitted;
            });
        authority.dropck_has_generic_type_outlives =
            std::ranges::any_of(dropck->second.violations, [](const DropCheckViolation& violation) {
                return violation.kind == DropCheckViolationKind::generic_type_outlives;
            });
        authority.dropck_has_borrowed_drop =
            std::ranges::any_of(dropck->second.violations, [](const DropCheckViolation& violation) {
                return violation.kind == DropCheckViolationKind::borrowed_drop;
            });
        authority.dropck_has_borrowed_field_dangling =
            std::ranges::any_of(dropck->second.violations, [](const DropCheckViolation& violation) {
                return violation.kind == DropCheckViolationKind::borrowed_field_dangling;
            });
        authority.dropck_has_destructor_escape =
            std::ranges::any_of(dropck->second.violations, [](const DropCheckViolation& violation) {
                return violation.kind == DropCheckViolationKind::destructor_escape;
            });
        authority.dropck_has_drop_glue_missing =
            std::ranges::any_of(dropck->second.violations, [](const DropCheckViolation& violation) {
                return violation.kind == DropCheckViolationKind::drop_glue_missing;
            });
    }
    if (const auto place_state = checked.place_state_facts.find(function);
        place_state != checked.place_state_facts.end()) {
        authority.has_place_state_facts = true;
        authority.place_state_fingerprint = function_place_state_facts_fingerprint(place_state->second);
        authority.place_state_place_count = static_cast<base::u64>(place_state->second.places.size());
        authority.place_state_event_count = static_cast<base::u64>(place_state->second.events.size());
        authority.place_state_partial_projection_count =
            static_cast<base::u64>(std::ranges::count_if(place_state->second.places, [](const PlaceStateFact& fact) {
                return fact.has_partial_projection;
            }));
        authority.place_state_drop_place_count =
            static_cast<base::u64>(std::ranges::count_if(place_state->second.places, [](const PlaceStateFact& fact) {
                return fact.drop_count != 0 || fact.cleanup_count != 0 || fact.drop_state == PlaceStateDropState::dropped;
            }));
        authority.place_state_move_candidate_count =
            static_cast<base::u64>(std::ranges::count_if(place_state->second.places, [](const PlaceStateFact& fact) {
                return fact.move_candidate_count != 0;
            }));
        authority.place_state_borrow_event_count =
            static_cast<base::u64>(std::ranges::count_if(place_state->second.events, [](const PlaceStateEvent& event) {
                return event.kind == PlaceStateEventKind::borrow_shared
                    || event.kind == PlaceStateEventKind::borrow_mutable;
            }));
        authority.place_state_partial_move_count =
            static_cast<base::u64>(std::ranges::count_if(place_state->second.places, [](const PlaceStateFact& fact) {
                return fact.partial_move_count != 0 || fact.is_partially_moved;
            }));
        authority.place_state_skipped_drop_count =
            static_cast<base::u64>(std::ranges::count_if(place_state->second.places, [](const PlaceStateFact& fact) {
                return fact.skipped_drop_count != 0;
            }));
        authority.place_state_violation_count = static_cast<base::u64>(place_state->second.violations.size());
        authority.place_state_emitted_diagnostic_count =
            static_cast<base::u64>(std::ranges::count_if(
                place_state->second.violations, [](const PlaceStateViolation& violation) {
                    return violation.diagnostic_emitted;
                }));
        authority.place_state_graph_missing = place_state->second.graph_missing;
        authority.place_state_has_partial_projection = authority.place_state_partial_projection_count != 0;
        authority.place_state_has_drop_action = authority.place_state_drop_place_count != 0;
        authority.place_state_has_move_candidate = authority.place_state_move_candidate_count != 0;
        authority.place_state_has_borrow = authority.place_state_borrow_event_count != 0;
        authority.place_state_has_partial_move = authority.place_state_partial_move_count != 0;
        authority.place_state_has_skipped_drop = authority.place_state_skipped_drop_count != 0;
        authority.place_state_has_violation = authority.place_state_violation_count != 0;
        authority.place_state_has_emitted_diagnostics = authority.place_state_emitted_diagnostic_count != 0;
    }
    if (const auto loan = checked.body_loan_checks.find(function); loan != checked.body_loan_checks.end()) {
        authority.has_body_loan_check = true;
        authority.body_loan_fingerprint = body_loan_check_fingerprint(loan->second);
        authority.body_loan_count = static_cast<base::u64>(loan->second.loans.size());
        authority.body_reborrow_count =
            static_cast<base::u64>(std::ranges::count_if(loan->second.loans, [](const BodyLoan& body_loan) {
                return body_loan.parent_loan != SEMA_BODY_LOAN_INVALID_INDEX;
            }));
        authority.body_two_phase_borrow_count = static_cast<base::u64>(loan->second.two_phase_borrows.size());
        authority.body_loan_conflict_count = static_cast<base::u64>(loan->second.conflicts.size());
        authority.body_loan_graph_missing = loan->second.graph_missing;
        authority.body_loan_has_emitted_diagnostics =
            std::ranges::any_of(loan->second.conflicts, [](const BodyLoanConflict& conflict) {
                return conflict.diagnostic_emitted;
            });
        authority.body_two_phase_has_emitted_diagnostics =
            std::ranges::any_of(loan->second.two_phase_borrows, [](const BodyTwoPhaseBorrow& borrow) {
                return borrow.diagnostic_emitted;
            });
    }
}

std::string struct_display_name(const TypeTable& types, const StructInfo& info)
{
    return types.display_name(info.name.view(), generic_args_for_type(types, info.type));
}

std::string enum_display_name(const TypeTable& types, const EnumCaseInfo& info)
{
    return types.display_name(info.enum_name.view(), generic_args_for_type(types, info.type));
}

std::string enum_case_display_name(const TypeTable& types, const EnumCaseInfo& info)
{
    std::string display = enum_display_name(types, info);
    display += "_";
    display += info.case_name.view();
    return display;
}

std::string_view pointer_mutability_dump_name(const PointerMutability mutability) noexcept
{
    switch (mutability) {
        case PointerMutability::const_:
            return "const";
        case PointerMutability::mut:
            return "mut";
    }
    return "<invalid>";
}

std::string dump_checked_module(const CheckedModule& checked)
{
    std::ostringstream out;
    out << "checked_module\n";
    out << "  expr_types " << checked.expr_types.size() << "\n";
    const ResourceSemanticsClassifier resources(checked);
    out << "  resource_summaries " << checked.types.size() << "\n";
    for (base::usize index = 0; index < checked.types.size(); ++index) {
        const TypeHandle type{static_cast<base::u32>(index)};
        const ResourceSemanticsSummary summary = resources.classify(type);
        out << "    resource #" << index << " " << checked.types.display_name(type) << " "
            << resource_semantics_debug_string(summary)
            << " fingerprint=" << query::debug_string(resource_semantics_fingerprint(summary)) << "\n";
    }
    std::vector<const ForInIterationPlan*> for_in_plans;
    for_in_plans.reserve(checked.for_in_iteration_plans.size());
    for (const auto& entry : checked.for_in_iteration_plans) {
        for_in_plans.push_back(&entry.second);
    }
    std::sort(for_in_plans.begin(), for_in_plans.end(),
        [](const ForInIterationPlan* lhs, const ForInIterationPlan* rhs) {
            return lhs->stmt.value < rhs->stmt.value;
        });
    if (!for_in_plans.empty()) {
        out << "  for_in_iteration_plans " << for_in_plans.size() << "\n";
        for (const ForInIterationPlan* const plan_ptr : for_in_plans) {
            const ForInIterationPlan& plan = *plan_ptr;
            out << "    for_in #" << plan.stmt.value << " " << for_in_iteration_kind_name(plan.kind)
                << " item=" << checked.types.display_name(plan.item_type)
                << " mode=" << for_in_item_mode_name(plan.item_mode);
            if (syntax::is_valid(plan.iterable_expr)) {
                out << " iterable_expr=#" << plan.iterable_expr.value
                    << " iterable_type=" << checked.types.display_name(plan.iterable_type)
                    << " access=" << pointer_mutability_dump_name(plan.element_access);
                if (plan.kind == ForInIterationKind::protocol_iterator) {
                    out << " iterator=" << checked.types.display_name(plan.iterator_type)
                        << " source=" << for_in_protocol_source_kind_name(plan.protocol_source)
                        << " iter_call=" << for_in_protocol_call_kind_name(plan.iter_call.kind)
                        << " has_next_call=" << for_in_protocol_call_kind_name(plan.has_next_call.kind)
                        << " next_call=" << for_in_protocol_call_kind_name(plan.next_call.kind);
                }
            } else {
                out << " range_exprs=(start=";
                if (syntax::is_valid(plan.start_expr)) {
                    out << "#" << plan.start_expr.value;
                } else {
                    out << "<implicit>";
                }
                out << ", end=";
                if (syntax::is_valid(plan.end_expr)) {
                    out << "#" << plan.end_expr.value;
                } else {
                    out << "<invalid>";
                }
                out << ", step=";
                if (syntax::is_valid(plan.step_expr)) {
                    out << "#" << plan.step_expr.value;
                } else {
                    out << "<implicit>";
                }
                out << ")";
            }
            out << " eval_once=" << (plan.evaluates_source_once ? "true" : "false")
                << " consumes=" << (plan.consumes_iterable ? "true" : "false")
                << " copy_item=" << (plan.requires_copy_item ? "true" : "false") << "\n";
        }
    }
    const bool show_parts = checked_has_non_primary_parts(checked);

    std::vector<const GenericTemplateSignatureInfo*> template_names;
    template_names.reserve(checked.generic_template_signatures.size());
    for (const GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        template_names.push_back(&info);
    }
    std::sort(template_names.begin(), template_names.end(),
        [](const GenericTemplateSignatureInfo* lhs, const GenericTemplateSignatureInfo* rhs) {
            if (lhs->name.view() != rhs->name.view()) {
                return lhs->name.view() < rhs->name.view();
            }
            if (lhs->name_space != rhs->name_space) {
                return static_cast<base::u8>(lhs->name_space) < static_cast<base::u8>(rhs->name_space);
            }
            return lhs->param_count < rhs->param_count;
        });
    if (!template_names.empty()) {
        out << "  generic_templates " << template_names.size() << "\n";
        for (const GenericTemplateSignatureInfo* const info_ptr : template_names) {
            const GenericTemplateSignatureInfo& info = *info_ptr;
            out << "    template ";
            if (!syntax::visibility_is_public(info.visibility)) {
                out << syntax::visibility_name(info.visibility) << " ";
            }
            out << generic_template_namespace_name(info.name_space) << " " << info.name
                << " params=" << info.param_count;
            append_part_origin(out, show_parts, info.part_index);
            out << "\n";
        }
    }

    std::vector<const TraitSignature*> trait_names;
    trait_names.reserve(checked.traits.size());
    for (const auto& entry : checked.traits) {
        trait_names.push_back(&entry.second);
    }
    std::sort(trait_names.begin(), trait_names.end(), [&](const TraitSignature* lhs, const TraitSignature* rhs) {
        return trait_signature_display_name(checked, *lhs) < trait_signature_display_name(checked, *rhs);
    });
    out << "  traits " << trait_names.size() << "\n";
    for (const TraitSignature* const trait_ptr : trait_names) {
        const TraitSignature& trait = *trait_ptr;
        out << "    trait ";
        if (!syntax::visibility_is_public(trait.visibility)) {
            out << syntax::visibility_name(trait.visibility) << " ";
        }
        out << trait_signature_display_name(checked, trait) << " params=" << trait.generic_params.size()
            << " associated_types=" << trait.associated_types.size() << " requirements=" << trait.requirements.size();
        append_part_origin(out, show_parts, trait.part_index);
        out << "\n";
        for (const TraitAssociatedTypeRequirement& associated_type : trait.associated_types) {
            out << "      assoc_type " << associated_type.name << "\n";
        }
        for (const TraitMethodRequirement& requirement : trait.requirements) {
            out << "      requirement ";
            if (requirement.is_unsafe) {
                out << "unsafe ";
            }
            out << requirement.name << "(";
            append_param_type_list(out, checked, requirement.param_types);
            out << ") -> " << checked.types.display_name(requirement.return_type);
            if (requirement.is_variadic) {
                out << " variadic";
            }
            if (requirement.has_default_body) {
                out << " default";
            }
            if (requirement.has_borrow_contract) {
                out << " borrow_contract=" << checked_borrow_contract_source_name(requirement.borrow_contract.source)
                    << "/selectors=" << requirement.borrow_contract.return_selectors.size()
                    << "/unknown=" << (requirement.borrow_contract.unknown_return_allowed ? "true" : "false");
            }
            out << "\n";
        }
    }

    std::vector<const TraitImplInfo*> trait_impl_names;
    trait_impl_names.reserve(checked.trait_impls.size());
    for (const auto& entry : checked.trait_impls) {
        trait_impl_names.push_back(&entry.second);
    }
    std::sort(
        trait_impl_names.begin(), trait_impl_names.end(), [&](const TraitImplInfo* lhs, const TraitImplInfo* rhs) {
            const std::string lhs_display =
                trait_impl_display_name(checked, *lhs) + " for " + checked.types.display_name(lhs->self_type);
            const std::string rhs_display =
                trait_impl_display_name(checked, *rhs) + " for " + checked.types.display_name(rhs->self_type);
            return lhs_display < rhs_display;
        });
    out << "  trait_impls " << trait_impl_names.size() << "\n";
    for (const TraitImplInfo* const info_ptr : trait_impl_names) {
        const TraitImplInfo& info = *info_ptr;
        out << "    impl " << trait_impl_display_name(checked, info) << " for "
            << checked.types.display_name(info.self_type) << " associated_types=" << info.associated_types.size()
            << " methods=" << info.methods.size();
        append_part_origin(out, show_parts, info.part_index);
        out << "\n";
        for (const TraitImplAssociatedTypeInfo& associated_type : info.associated_types) {
            out << "      assoc_type " << associated_type.name << " = "
                << checked.types.display_name(associated_type.value_type)
                << " requirement=" << associated_type.requirement_ordinal << "\n";
        }
        for (const TraitImplMethodInfo& method : info.methods) {
            out << "      method " << method.name << " requirement=" << method.requirement_ordinal
                << " origin=" << trait_impl_method_origin_name(method.origin) << "\n";
        }
    }

    std::vector<const TraitPredicate*> trait_predicates;
    trait_predicates.reserve(checked.trait_predicates.size());
    for (const TraitPredicate& predicate : checked.trait_predicates) {
        trait_predicates.push_back(&predicate);
    }
    std::sort(
        trait_predicates.begin(), trait_predicates.end(), [&](const TraitPredicate* lhs, const TraitPredicate* rhs) {
            const std::string lhs_display =
                checked.types.display_name(lhs->subject_type) + ": " + trait_predicate_trait_name(checked, *lhs);
            const std::string rhs_display =
                checked.types.display_name(rhs->subject_type) + ": " + trait_predicate_trait_name(checked, *rhs);
            if (lhs_display != rhs_display) {
                return lhs_display < rhs_display;
            }
            return lhs->index < rhs->index;
        });
    out << "  trait_predicates " << trait_predicates.size() << "\n";
    for (const TraitPredicate* const predicate_ptr : trait_predicates) {
        const TraitPredicate& predicate = *predicate_ptr;
        out << "    predicate #" << predicate.index << " " << trait_predicate_kind_name(predicate.kind) << " "
            << checked.types.display_name(predicate.subject_type) << ": "
            << trait_predicate_trait_name(checked, predicate)
            << " origin=" << trait_predicate_origin_name(predicate.origin);
        append_part_origin(out, show_parts, predicate.part_index);
        out << "\n";
        for (const TraitImplAssociatedTypeInfo& equality : predicate.associated_type_equalities) {
            out << "      assoc_eq " << equality.name << " = " << checked.types.display_name(equality.value_type)
                << "\n";
        }
    }

    out << "  trait_obligations " << checked.trait_obligations.size() << "\n";
    for (base::usize index = 0; index < checked.trait_obligations.size(); ++index) {
        const TraitObligation& obligation = checked.trait_obligations[index];
        out << "    obligation #" << index << " predicate=" << obligation.predicate_index;
        append_part_origin(out, show_parts, obligation.part_index);
        out << "\n";
    }

    out << "  trait_evidence " << checked.trait_evidence.size() << "\n";
    for (base::usize index = 0; index < checked.trait_evidence.size(); ++index) {
        const TraitEvidence& evidence = checked.trait_evidence[index];
        out << "    evidence #" << index << " " << trait_evidence_kind_name(evidence.kind)
            << " predicate=" << evidence.predicate_index;
        append_part_origin(out, show_parts, evidence.part_index);
        out << "\n";
    }

    out << "  trait_method_calls " << checked.trait_method_calls.size() << "\n";
    for (base::usize index = 0; index < checked.trait_method_calls.size(); ++index) {
        const TraitMethodCallBinding& binding = checked.trait_method_calls[index];
        out << "    trait_call #" << index << " " << trait_method_dispatch_kind_name(binding.dispatch) << " "
            << checked.types.display_name(binding.self_type) << ".";
        out << (binding.method_name.empty() ? std::string_view{"<invalid>"} : binding.method_name.view());
        out << " -> " << checked.types.display_name(binding.return_type);
        if (binding.predicate_index != SEMA_TRAIT_PREDICATE_INVALID_INDEX) {
            out << " predicate=" << binding.predicate_index;
        }
        if (binding.requirement_ordinal != SEMA_TRAIT_PREDICATE_INVALID_INDEX) {
            out << " requirement=" << binding.requirement_ordinal;
        }
        if (binding.dispatch == TraitMethodDispatchKind::vtable_slot) {
            out << " slot=" << binding.vtable_slot;
            if (query::is_valid(binding.vtable_layout)) {
                out << " vtable=" << query::debug_string(query::stable_key_fingerprint(binding.vtable_layout));
            }
        }
        out << " receiver_access=" << receiver_access_kind_name(binding.receiver_access)
            << " auto_borrow=" << (binding.receiver_auto_borrow ? "true" : "false")
            << " two_phase=" << (binding.receiver_two_phase_eligible ? "true" : "false");
        append_ordered_arg_list(out, binding.ordered_args);
        if (is_valid(binding.dispatch_receiver_type)
            && binding.dispatch_receiver_type.value != binding.receiver_type.value) {
            out << " dispatch_receiver=" << checked.types.display_name(binding.dispatch_receiver_type);
        }
        append_part_origin(out, show_parts, binding.part_index);
        out << "\n";
    }

    out << "  trait_object_callability " << checked.trait_object_callability.size() << "\n";
    for (base::usize index = 0; index < checked.trait_object_callability.size(); ++index) {
        const TraitObjectCallabilityFact& fact = checked.trait_object_callability[index];
        out << "    trait_object #" << index << " " << checked.types.display_name(fact.object_type)
            << " slots=" << fact.method_slot_count
            << " key=" << query::debug_string(query::stable_key_fingerprint(fact.object_type_key))
            << " schema=" << query::debug_string(fact.slot_schema);
        append_part_origin(out, show_parts, fact.part_index);
        out << "\n";
    }

    out << "  trait_object_method_slots " << checked.trait_object_method_slots.size() << "\n";
    for (base::usize index = 0; index < checked.trait_object_method_slots.size(); ++index) {
        const TraitObjectMethodSlotFact& fact = checked.trait_object_method_slots[index];
        out << "    slot #" << index << " " << checked.types.display_name(fact.object_type) << "."
            << (fact.method_name.empty() ? std::string_view{"<invalid>"} : fact.method_name.view())
            << " slot=" << fact.slot
            << " requirement=" << fact.requirement_ordinal
            << " receiver_access=" << receiver_access_kind_name(fact.receiver_access)
            << " receiver=" << checked.types.display_name(fact.receiver_type)
            << " return=" << checked.types.display_name(fact.return_type);
        append_part_origin(out, show_parts, fact.part_index);
        out << "\n";
    }

    out << "  vtable_layouts " << checked.vtable_layouts.size() << "\n";
    for (base::usize index = 0; index < checked.vtable_layouts.size(); ++index) {
        const VTableLayoutFact& fact = checked.vtable_layouts[index];
        out << "    vtable #" << index << " " << checked.types.display_name(fact.concrete_type) << " as "
            << checked.types.display_name(fact.object_type)
            << " slots=" << fact.method_slot_count
            << " key=" << query::debug_string(query::stable_key_fingerprint(fact.layout_key))
            << " evidence=" << query::debug_string(fact.impl_evidence);
        append_part_origin(out, show_parts, fact.part_index);
        out << "\n";
        for (const VTableMethodSlotFact& slot : fact.method_slots) {
            out << "      vtable_method slot=" << slot.slot
                << " requirement=" << slot.requirement_ordinal
                << " origin=" << (slot.origin == TraitImplMethodOrigin::trait_default ? "trait_default"
                                                                                       : "impl_override")
                << " " << checked.types.display_name(slot.concrete_type) << "."
                << (slot.method_name.empty() ? std::string_view{"<invalid>"} : slot.method_name.view())
                << " fn=(" << slot.function_key.module << "," << slot.function_key.owner_type << ","
                << slot.function_key.name.value << ")"
                << " receiver=" << checked.types.display_name(slot.receiver_type)
                << " return=" << checked.types.display_name(slot.return_type);
            if (!slot.param_types.empty()) {
                out << " params=[";
                for (base::usize param_index = 0; param_index < slot.param_types.size(); ++param_index) {
                    if (param_index != 0) {
                        out << ", ";
                    }
                    out << checked.types.display_name(slot.param_types[param_index]);
                }
                out << "]";
            }
            append_part_origin(out, show_parts, slot.part_index);
            out << "\n";
        }
    }

    out << "  trait_object_coercions " << checked.trait_object_coercions.size() << "\n";
    for (base::usize index = 0; index < checked.trait_object_coercions.size(); ++index) {
        const TraitObjectCoercionFact& fact = checked.trait_object_coercions[index];
        out << "    dyn_coercion #" << index << " expr=e" << fact.expr.value << " "
            << checked.types.display_name(fact.source_reference_type) << " -> "
            << checked.types.display_name(fact.target_reference_type)
            << " source=" << checked.types.display_name(fact.source_type)
            << " object=" << checked.types.display_name(fact.object_type)
            << " borrow=" << (fact.borrow_kind == query::TraitObjectBorrowKindKey::mut ? "mut" : "shared")
            << " key=" << query::debug_string(query::stable_key_fingerprint(fact.coercion_key));
        append_part_origin(out, show_parts, fact.part_index);
        out << "\n";
    }

    out << "  trait_supertrait_edges " << checked.trait_supertrait_edges.size() << "\n";
    for (base::usize index = 0; index < checked.trait_supertrait_edges.size(); ++index) {
        const TraitSupertraitEdgeFact& fact = checked.trait_supertrait_edges[index];
        out << "    supertrait_edge #" << index << " "
            << (fact.child_trait_name.empty() ? std::string_view{"<invalid>"} : fact.child_trait_name.view())
            << " -> "
            << (fact.parent_trait_name.empty() ? std::string_view{"<invalid>"} : fact.parent_trait_name.view())
            << " ordinal=" << fact.direct_edge_ordinal
            << " depth=" << fact.closure_depth
            << " key=" << query::debug_string(fact.edge_fingerprint);
        if (!fact.parent_trait_args.empty()) {
            out << " args=[";
            for (base::usize arg_index = 0; arg_index < fact.parent_trait_args.size(); ++arg_index) {
                if (arg_index != 0) {
                    out << ", ";
                }
                out << checked.types.display_name(fact.parent_trait_args[arg_index]);
            }
            out << "]";
        }
        append_part_origin(out, show_parts, fact.part_index);
        out << "\n";
    }

    out << "  trait_object_upcast_coercions " << checked.trait_object_upcast_coercions.size() << "\n";
    for (base::usize index = 0; index < checked.trait_object_upcast_coercions.size(); ++index) {
        const TraitObjectUpcastCoercionFact& fact = checked.trait_object_upcast_coercions[index];
        out << "    dyn_upcast #" << index << " expr=e" << fact.expr.value << " "
            << checked.types.display_name(fact.source_reference_type) << " -> "
            << checked.types.display_name(fact.target_reference_type)
            << " source_object=" << checked.types.display_name(fact.source_object_type)
            << " target_object=" << checked.types.display_name(fact.target_object_type)
            << " source_layout=" << fact.source_vtable_layout.global_id
            << " target_layout=" << fact.target_vtable_layout.global_id
            << " borrow=" << (fact.borrow_kind == query::TraitObjectBorrowKindKey::mut ? "mut" : "shared")
            << " key=" << query::debug_string(query::stable_key_fingerprint(fact.upcast_key));
        append_part_origin(out, show_parts, fact.part_index);
        out << "\n";
    }

    if (!checked.principal_set_composition_facts.identity_facts.empty()) {
        out << "  principal_set_composition "
            << checked.principal_set_composition_facts.summary.principal_set_count
            << " fingerprint="
            << query::debug_string(
                   query::principal_set_composition_facts_fingerprint(checked.principal_set_composition_facts))
            << "\n";
        out << query::dump_principal_set_composition_facts(checked.principal_set_composition_facts);
    }

    out << "  function_calls " << checked.function_calls.size() << "\n";
    for (base::usize index = 0; index < checked.function_calls.size(); ++index) {
        const FunctionCallBinding& binding = checked.function_calls[index];
        out << "    function_call #" << index << " expr=e" << binding.call_expr.value << " -> "
            << binding.function_key.module << ':' << binding.function_key.owner_type << ':';
        if (syntax::is_valid(binding.function_key.name)) {
            out << '#' << binding.function_key.name.value;
        } else {
            out << '-';
        }
        out << " return=" << checked.types.display_name(binding.return_type)
            << " receiver_args=" << binding.receiver_arg_count
            << " receiver_access=" << receiver_access_kind_name(binding.receiver_access)
            << " auto_borrow=" << (binding.receiver_auto_borrow ? "true" : "false")
            << " two_phase=" << (binding.receiver_two_phase_eligible ? "true" : "false");
        append_ordered_arg_list(out, binding.ordered_args);
        append_part_origin(out, show_parts, binding.part_index);
        out << "\n";
    }

    out << "  borrow_summaries " << checked.borrow_summaries.size() << "\n";
    for (const auto& entry : checked.borrow_summaries) {
        const FunctionBorrowSummary& summary = entry.second;
        out << "    borrow_summary " << entry.first.module << ':' << entry.first.owner_type << ':';
        if (syntax::is_valid(entry.first.name)) {
            out << '#' << entry.first.name.value;
        } else {
            out << '-';
        }
        out << " return=" << checked.types.display_name(summary.return_type) << " origins=" << summary.origins.size()
            << " deps=" << summary.return_origins.size()
            << " storage_escapes=" << summary.storage_escapes.size()
            << " unknown=" << (summary.has_unknown_return_origin ? "true" : "false")
            << " local_escape=" << (summary.has_local_return_escape ? "true" : "false")
            << " fingerprint=" << query::debug_string(summary.fingerprint);
        append_part_origin(out, show_parts, summary.part_index);
        out << "\n";
    }

    out << "  borrow_contracts " << checked.borrow_contracts.size() << "\n";
    for (const auto& entry : checked.borrow_contracts) {
        const FunctionBorrowContract& contract = entry.second;
        out << "    borrow_contract " << entry.first.module << ':' << entry.first.owner_type << ':';
        if (syntax::is_valid(entry.first.name)) {
            out << '#' << entry.first.name.value;
        } else {
            out << '-';
        }
        out << " source=" << checked_borrow_contract_source_name(contract.source)
            << " return=" << checked.types.display_name(contract.return_type)
            << " selectors=" << contract.return_selectors.size()
            << " unknown=" << (contract.unknown_return_allowed ? "true" : "false")
            << " local_escape=" << (contract.has_local_return_escape ? "true" : "false")
            << " mismatch=" << (contract.has_contract_mismatch ? "true" : "false")
            << " fingerprint=" << query::debug_string(contract.fingerprint);
        append_part_origin(out, show_parts, contract.part_index);
        out << "\n";
        for (base::usize selector_index = 0; selector_index < contract.return_selectors.size(); ++selector_index) {
            const BorrowContractSelector& selector = contract.return_selectors[selector_index];
            out << "      selector #" << selector_index << ' '
                << checked_borrow_contract_selector_kind_name(selector.kind) << " param=" << selector.param_index
                << " name=";
            if (syntax::is_valid(selector.name_id)) {
                out << '#' << selector.name_id.value;
            } else {
                out << '-';
            }
            out << "\n";
        }
    }

    std::vector<const FunctionMoveRejectionFacts*> move_rejection_facts;
    move_rejection_facts.reserve(checked.move_rejection_facts.size());
    for (const auto& entry : checked.move_rejection_facts) {
        move_rejection_facts.push_back(&entry.second);
    }
    std::sort(move_rejection_facts.begin(), move_rejection_facts.end(),
        [](const FunctionMoveRejectionFacts* lhs, const FunctionMoveRejectionFacts* rhs) {
            return std::tie(lhs->function.module, lhs->function.owner_type, lhs->function.name.value)
                < std::tie(rhs->function.module, rhs->function.owner_type, rhs->function.name.value);
        });
    out << "  move_rejection_facts " << move_rejection_facts.size() << "\n";
    for (const FunctionMoveRejectionFacts* const facts_ptr : move_rejection_facts) {
        const FunctionMoveRejectionFacts& facts = *facts_ptr;
        out << "    move_rejection_fact " << facts.function.module << ':' << facts.function.owner_type << ':';
        if (syntax::is_valid(facts.function.name)) {
            out << '#' << facts.function.name.value;
        } else {
            out << '-';
        }
        out << " rejections=" << facts.rejections.size()
            << " fingerprint=" << query::debug_string(function_move_rejection_facts_fingerprint(facts));
        append_part_origin(out, show_parts, facts.part_index);
        out << "\n";
        for (base::usize rejection_index = 0; rejection_index < facts.rejections.size(); ++rejection_index) {
            const MoveRejectionFact& rejection = facts.rejections[rejection_index];
            out << "      rejection #" << rejection_index << ' ' << move_rejection_kind_name(rejection.kind)
                << " expr=e" << rejection.expr.value
                << " stmt=s" << rejection.stmt.value
                << " pattern=p" << rejection.pattern.value
                << " tracked_type=" << checked.types.display_name(rejection.tracked_type)
                << " resource=" << query::debug_string(rejection.resource_fingerprint)
                << " emitted=" << (rejection.diagnostic_emitted ? "true" : "false") << "\n";
        }
    }

    std::vector<const DestructorInfo*> destructors;
    destructors.reserve(checked.destructors.size());
    for (const auto& entry : checked.destructors) {
        destructors.push_back(&entry.second);
    }
    std::sort(destructors.begin(), destructors.end(), [](const DestructorInfo* lhs, const DestructorInfo* rhs) {
        return lhs->self_type.value < rhs->self_type.value;
    });
    out << "  destructors " << destructors.size() << "\n";
    for (const DestructorInfo* const info_ptr : destructors) {
        const DestructorInfo& info = *info_ptr;
        out << "    destructor " << checked.types.display_name(info.self_type) << " -> " << info.function_key.module
            << ':' << info.function_key.owner_type << ':';
        if (syntax::is_valid(info.function_key.name)) {
            out << '#' << info.function_key.name.value;
        } else {
            out << '-';
        }
        out << " fingerprint=" << query::debug_string(destructor_info_fingerprint(info));
        append_part_origin(out, show_parts, info.part_index);
        out << "\n";
    }

    out << "  lifetime_origin_params " << checked.lifetime_origin_params.size() << "\n";
    for (base::usize index = 0; index < checked.lifetime_origin_params.size(); ++index) {
        const LifetimeOriginParamInfo& origin = checked.lifetime_origin_params[index];
        out << "    origin_param #" << index << " " << origin.name << " ordinal=" << origin.ordinal;
        append_part_origin(out, show_parts, origin.part_index);
        out << "\n";
    }

    out << "  reference_origin_facts " << checked.reference_origin_facts.size() << "\n";
    for (base::usize index = 0; index < checked.reference_origin_facts.size(); ++index) {
        const ReferenceOriginFact& fact = checked.reference_origin_facts[index];
        out << "    reference_origin #" << index << " t" << fact.syntax_type.value << " "
            << checked.types.display_name(fact.semantic_type) << " origins=";
        if (fact.origin_names.empty()) {
            out << "-";
        }
        for (base::usize origin_index = 0; origin_index < fact.origin_names.size(); ++origin_index) {
            if (origin_index != 0) {
                out << " | ";
            }
            out << fact.origin_names[origin_index];
        }
        append_part_origin(out, show_parts, fact.part_index);
        out << "\n";
    }

    out << "  type_lifetime_infos " << checked.type_lifetime_infos.size() << "\n";
    for (base::usize index = 0; index < checked.type_lifetime_infos.size(); ++index) {
        const TypeLifetimeInfo& info = checked.type_lifetime_infos[index];
        out << "    type_lifetime #" << index << " " << checked.types.display_name(info.type)
            << " can_borrow=" << (info.can_contain_borrow ? "true" : "false")
            << " concrete=" << (info.has_concrete_borrow_surface ? "true" : "false") << " origins=";
        if (info.origin_names.empty()) {
            out << "-";
        }
        for (base::usize origin_index = 0; origin_index < info.origin_names.size(); ++origin_index) {
            if (origin_index != 0) {
                out << " | ";
            }
            out << info.origin_names[origin_index];
        }
        out << " fingerprint=" << query::debug_string(type_lifetime_info_fingerprint(info));
        append_part_origin(out, show_parts, info.part_index);
        out << "\n";
    }

    out << "  generic_lifetime_predicates " << checked.generic_lifetime_predicates.size() << "\n";
    for (base::usize index = 0; index < checked.generic_lifetime_predicates.size(); ++index) {
        const GenericLifetimePredicate& predicate = checked.generic_lifetime_predicates[index];
        out << "    generic_lifetime #" << index << " " << checked.types.display_name(predicate.subject_type)
            << " : " << predicate.origin_name
            << " source=" << generic_lifetime_predicate_source_name(predicate.source)
            << " fingerprint=" << query::debug_string(generic_lifetime_predicate_fingerprint(predicate));
        append_part_origin(out, show_parts, predicate.part_index);
        out << "\n";
    }

    std::vector<const FunctionLifetimeFacts*> lifetime_facts;
    lifetime_facts.reserve(checked.lifetime_facts.size());
    for (const auto& entry : checked.lifetime_facts) {
        lifetime_facts.push_back(&entry.second);
    }
    std::sort(lifetime_facts.begin(), lifetime_facts.end(),
        [](const FunctionLifetimeFacts* lhs, const FunctionLifetimeFacts* rhs) {
            return std::tie(lhs->function.module, lhs->function.owner_type, lhs->function.name.value)
                < std::tie(rhs->function.module, rhs->function.owner_type, rhs->function.name.value);
        });
    out << "  lifetime_facts " << lifetime_facts.size() << "\n";
    for (const FunctionLifetimeFacts* const facts_ptr : lifetime_facts) {
        const FunctionLifetimeFacts& facts = *facts_ptr;
        out << "    lifetime_fact " << facts.function.module << ':' << facts.function.owner_type << ':';
        if (syntax::is_valid(facts.function.name)) {
            out << '#' << facts.function.name.value;
        } else {
            out << '-';
        }
        out << " return=" << checked.types.display_name(facts.return_type) << " regions=" << facts.regions.size()
            << " outlives=" << facts.outlives_constraints.size()
            << " type_outlives=" << facts.type_outlives_constraints.size()
            << " live_ranges=" << facts.live_ranges.size() << " returns=" << facts.return_regions.size()
            << " violations=" << facts.violations.size() << " solved=" << (facts.solved ? "true" : "false")
            << " enforced=" << (facts.diagnostic_mode_enforced ? "true" : "false")
            << " fingerprint=" << query::debug_string(function_lifetime_facts_fingerprint(facts));
        append_part_origin(out, show_parts, facts.part_index);
        out << "\n";
        for (base::usize region_index = 0; region_index < facts.regions.size(); ++region_index) {
            const LifetimeRegion& region = facts.regions[region_index];
            out << "      region #" << region_index << ' ' << lifetime_region_kind_name(region.kind)
                << " param=" << region.param_index << " name=";
            if (!region.name.empty()) {
                out << region.name;
            } else if (syntax::is_valid(region.name_id)) {
                out << '#' << region.name_id.value;
            } else {
                out << '-';
            }
            out << "\n";
        }
        for (base::usize violation_index = 0; violation_index < facts.violations.size(); ++violation_index) {
            const LifetimeViolation& violation = facts.violations[violation_index];
            out << "      violation #" << violation_index << ' ' << lifetime_violation_kind_name(violation.kind)
                << " region=" << violation.region << " related=" << violation.related_region
                << " emitted=" << (violation.diagnostic_emitted ? "true" : "false") << "\n";
        }
    }

    std::vector<const FunctionDropCheckFacts*> dropck_facts;
    dropck_facts.reserve(checked.dropck_facts.size());
    for (const auto& entry : checked.dropck_facts) {
        dropck_facts.push_back(&entry.second);
    }
    std::sort(dropck_facts.begin(), dropck_facts.end(),
        [](const FunctionDropCheckFacts* lhs, const FunctionDropCheckFacts* rhs) {
            return std::tie(lhs->function.module, lhs->function.owner_type, lhs->function.name.value)
                < std::tie(rhs->function.module, rhs->function.owner_type, rhs->function.name.value);
        });
    out << "  dropck_facts " << dropck_facts.size() << "\n";
    for (const FunctionDropCheckFacts* const facts_ptr : dropck_facts) {
        const FunctionDropCheckFacts& facts = *facts_ptr;
        out << "    dropck_fact " << facts.function.module << ':' << facts.function.owner_type << ':';
        if (syntax::is_valid(facts.function.name)) {
            out << '#' << facts.function.name.value;
        } else {
            out << '-';
        }
        base::u64 required_outlives_count = 0;
        for (const DropCheckFact& fact : facts.facts) {
            required_outlives_count += static_cast<base::u64>(fact.required_outlives.size());
        }
        out << " facts=" << facts.facts.size() << " actions=" << facts.actions.size()
            << " required_outlives=" << required_outlives_count << " violations=" << facts.violations.size()
            << " solved=" << (facts.solved ? "true" : "false")
            << " enforced=" << (facts.diagnostic_mode_enforced ? "true" : "false")
            << " graph_missing=" << (facts.graph_missing ? "true" : "false")
            << " fingerprint=" << query::debug_string(function_drop_check_facts_fingerprint(facts));
        append_part_origin(out, show_parts, facts.part_index);
        out << "\n";
        for (base::usize action_index = 0; action_index < facts.actions.size(); ++action_index) {
            const DropActionFact& action = facts.actions[action_index];
            out << "      action #" << action_index << ' ' << drop_check_action_kind_name(action.kind)
                << " flow=" << action.action << " point=" << action.point << " place=" << action.place
                << " type=" << checked.types.display_name(action.type) << "\n";
        }
        for (base::usize violation_index = 0; violation_index < facts.violations.size(); ++violation_index) {
            const DropCheckViolation& violation = facts.violations[violation_index];
            out << "      violation #" << violation_index << ' ' << drop_check_violation_kind_name(violation.kind)
                << " action=" << violation.action << " point=" << violation.point
                << " region=" << violation.region
                << " emitted=" << (violation.diagnostic_emitted ? "true" : "false") << "\n";
        }
    }

    std::vector<const FunctionPlaceStateFacts*> place_state_facts;
    place_state_facts.reserve(checked.place_state_facts.size());
    for (const auto& entry : checked.place_state_facts) {
        place_state_facts.push_back(&entry.second);
    }
    std::sort(place_state_facts.begin(), place_state_facts.end(),
        [](const FunctionPlaceStateFacts* lhs, const FunctionPlaceStateFacts* rhs) {
            return std::tie(lhs->function.module, lhs->function.owner_type, lhs->function.name.value)
                < std::tie(rhs->function.module, rhs->function.owner_type, rhs->function.name.value);
        });
    out << "  place_state_facts " << place_state_facts.size() << "\n";
    for (const FunctionPlaceStateFacts* const facts_ptr : place_state_facts) {
        const FunctionPlaceStateFacts& facts = *facts_ptr;
        out << "    place_state " << facts.function.module << ':' << facts.function.owner_type << ':';
        if (syntax::is_valid(facts.function.name)) {
            out << '#' << facts.function.name.value;
        } else {
            out << '-';
        }
        const base::u64 partial_count =
            static_cast<base::u64>(std::ranges::count_if(facts.places, [](const PlaceStateFact& fact) {
                return fact.has_partial_projection;
            }));
        const base::u64 borrow_event_count =
            static_cast<base::u64>(std::ranges::count_if(facts.events, [](const PlaceStateEvent& event) {
                return event.kind == PlaceStateEventKind::borrow_shared
                    || event.kind == PlaceStateEventKind::borrow_mutable;
            }));
        const base::u64 partial_move_count =
            static_cast<base::u64>(std::ranges::count_if(facts.places, [](const PlaceStateFact& fact) {
                return fact.partial_move_count != 0 || fact.is_partially_moved;
            }));
        const base::u64 emitted_violation_count =
            static_cast<base::u64>(std::ranges::count_if(facts.violations, [](const PlaceStateViolation& violation) {
                return violation.diagnostic_emitted;
            }));
        out << " places=" << facts.places.size() << " events=" << facts.events.size()
            << " partials=" << partial_count << " borrows=" << borrow_event_count
            << " partial_moves=" << partial_move_count
            << " violations=" << facts.violations.size()
            << " diagnostics=" << emitted_violation_count
            << " solved=" << (facts.solved ? "true" : "false")
            << " enforced=" << (facts.diagnostic_mode_enforced ? "true" : "false")
            << " graph_missing=" << (facts.graph_missing ? "true" : "false")
            << " fingerprint=" << query::debug_string(function_place_state_facts_fingerprint(facts));
        append_part_origin(out, show_parts, facts.part_index);
        out << "\n";
        for (base::usize place_index = 0; place_index < facts.places.size(); ++place_index) {
            const PlaceStateFact& fact = facts.places[place_index];
            out << "      place #" << place_index << " id=" << fact.place
                << " root=" << body_flow_place_root_kind_name(fact.root_kind)
                << " type=" << checked.types.display_name(fact.type)
                << " projections=" << fact.projection_count
                << " init=" << place_state_initialization_name(fact.initialization)
                << " move=" << place_state_move_state_name(fact.move_state)
                << " drop=" << place_state_drop_state_name(fact.drop_state)
                << " needs_drop=" << (fact.needs_drop ? "true" : "false")
                << " partial_moved=" << (fact.is_partially_moved ? "true" : "false")
                << " drop_flag_live=" << (fact.drop_flag_live ? "true" : "false")
                << " reads=" << fact.read_count << " writes=" << fact.write_count
                << " reinits=" << fact.reinit_count << " moves=" << fact.move_candidate_count
                << " drops=" << fact.drop_count << " cleanups=" << fact.cleanup_count
                << " borrows=" << fact.borrow_count
                << " partial_moves=" << fact.partial_move_count
                << " skipped_drops=" << fact.skipped_drop_count << "\n";
        }
        for (base::usize event_index = 0; event_index < facts.events.size(); ++event_index) {
            const PlaceStateEvent& event = facts.events[event_index];
            out << "      event #" << event_index << ' ' << place_state_event_kind_name(event.kind)
                << " place=" << event.place << " action=" << event.action << " point=" << event.point
                << " type=" << checked.types.display_name(event.type) << "\n";
        }
        for (base::usize violation_index = 0; violation_index < facts.violations.size(); ++violation_index) {
            const PlaceStateViolation& violation = facts.violations[violation_index];
            out << "      violation #" << violation_index << ' ' << place_state_violation_kind_name(violation.kind)
                << " place=" << violation.place << " action=" << violation.action << " point=" << violation.point
                << " emitted=" << (violation.diagnostic_emitted ? "true" : "false") << "\n";
        }
    }

    std::vector<const BodyLoanCheckResult*> loan_checks;
    loan_checks.reserve(checked.body_loan_checks.size());
    for (const auto& entry : checked.body_loan_checks) {
        loan_checks.push_back(&entry.second);
    }
    std::sort(
        loan_checks.begin(), loan_checks.end(), [](const BodyLoanCheckResult* lhs, const BodyLoanCheckResult* rhs) {
            return std::tie(lhs->function.module, lhs->function.owner_type, lhs->function.name.value)
                < std::tie(rhs->function.module, rhs->function.owner_type, rhs->function.name.value);
        });
    out << "  body_loan_checks " << loan_checks.size() << "\n";
    for (const BodyLoanCheckResult* const result_ptr : loan_checks) {
        const BodyLoanCheckResult& result = *result_ptr;
        out << "    body_loan_check " << result.function.module << ':' << result.function.owner_type << ':';
        if (syntax::is_valid(result.function.name)) {
            out << '#' << result.function.name.value;
        } else {
            out << '-';
        }
        out << " mode=" << body_loan_diagnostic_mode_name(result.diagnostic_mode) << " loans=" << result.loans.size()
            << " conflicts=" << result.conflicts.size()
            << " graph_missing=" << (result.graph_missing ? "true" : "false")
            << " fingerprint=" << query::debug_string(body_loan_check_fingerprint(result)) << "\n";
    }

    out << "  trait_default_method_instances " << checked.trait_default_method_instances.size() << "\n";
    for (base::usize index = 0; index < checked.trait_default_method_instances.size(); ++index) {
        const TraitDefaultMethodInstanceInfo& instance = checked.trait_default_method_instances[index];
        out << "    trait_default_instance #" << index << " "
            << checked.types.display_name(instance.signature.method_owner_type) << "." << instance.signature.name
            << " -> " << checked.types.display_name(instance.signature.return_type)
            << " requirement=" << instance.requirement_ordinal;
        if (!instance.signature.c_name.empty()) {
            out << " @c_name=" << instance.signature.c_name;
        }
        append_part_origin(out, show_parts, instance.signature.part_index);
        out << "\n";
    }

    std::vector<const ParamEnvInfo*> param_envs;
    param_envs.reserve(checked.param_envs.size());
    for (const ParamEnvInfo& param_env : checked.param_envs) {
        param_envs.push_back(&param_env);
    }
    std::sort(param_envs.begin(), param_envs.end(), [](const ParamEnvInfo* lhs, const ParamEnvInfo* rhs) {
        if (lhs->owner_name.view() != rhs->owner_name.view()) {
            return lhs->owner_name.view() < rhs->owner_name.view();
        }
        return lhs->predicate_indices.size() < rhs->predicate_indices.size();
    });
    out << "  param_envs " << param_envs.size() << "\n";
    for (const ParamEnvInfo* const param_env_ptr : param_envs) {
        const ParamEnvInfo& param_env = *param_env_ptr;
        out << "    param_env " << param_env.owner_name << " predicates=" << param_env.predicate_indices.size()
            << " key=" << query::debug_string(param_env.key);
        append_part_origin(out, show_parts, param_env.part_index);
        out << "\n";
    }

    std::vector<const FunctionSignature*> function_names;
    function_names.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        function_names.push_back(&entry.second);
    }
    std::sort(
        function_names.begin(), function_names.end(), [&](const FunctionSignature* lhs, const FunctionSignature* rhs) {
            return function_display_name(checked.types, *lhs) < function_display_name(checked.types, *rhs);
        });
    out << "  functions " << function_names.size() << "\n";
    for (const FunctionSignature* const fn_ptr : function_names) {
        const FunctionSignature& fn = *fn_ptr;
        const std::string display_name = function_display_name(checked.types, fn);
        out << "    fn ";
        if (!syntax::visibility_is_public(fn.visibility)) {
            out << syntax::visibility_name(fn.visibility) << " ";
        }
        if (fn.is_method) {
            out << "method " << checked.types.display_name(fn.method_owner_type) << ".";
        }
        out << display_name << " -> " << checked.types.display_name(fn.return_type);
        if (fn.is_unsafe) {
            out << " unsafe";
        }
        if (fn.c_name != display_name) {
            out << " @c_name=" << fn.c_name;
        }
        if (fn.is_extern_c) {
            out << " extern_c";
        }
        if (fn.is_variadic) {
            out << " variadic";
        }
        if (fn.is_export_c) {
            out << " export_c";
        }
        if (!fn.param_types.empty()) {
            out << " params=[";
            append_param_info_list(out, checked, fn.params, fn.param_types);
            out << "]";
        }
        if (fn.is_destructor) {
            out << " destructor";
        }
        append_part_origin(out, show_parts, fn.part_index);
        out << "\n";
    }

    std::vector<const StructInfo*> struct_names;
    struct_names.reserve(checked.structs.size());
    for (const auto& entry : checked.structs) {
        struct_names.push_back(&entry.second);
    }
    std::sort(struct_names.begin(), struct_names.end(), [&](const StructInfo* lhs, const StructInfo* rhs) {
        return struct_display_name(checked.types, *lhs) < struct_display_name(checked.types, *rhs);
    });
    out << "  structs " << struct_names.size() << "\n";
    for (const StructInfo* const info_ptr : struct_names) {
        const StructInfo& info = *info_ptr;
        out << "    struct ";
        if (!syntax::visibility_is_public(info.visibility)) {
            out << syntax::visibility_name(info.visibility) << " ";
        }
        out << struct_display_name(checked.types, info);
        if (info.is_opaque) {
            out << " opaque";
        }
        if (info.is_generic_placeholder) {
            out << " generic_placeholder";
        }
        if (const auto derived = checked.derived_capabilities_by_type.find(info.type.value);
            derived != checked.derived_capabilities_by_type.end() && !derived->second.empty()) {
            out << " derives=";
            for (base::usize i = 0; i < derived->second.size(); ++i) {
                if (i != 0) {
                    out << ",";
                }
                out << capability_name(derived->second[i].capability);
            }
        }
        append_part_origin(out, show_parts, info.part_index);
        out << " fields=" << info.fields.size() << "\n";
    }

    std::vector<const TypeAliasInfo*> alias_names;
    alias_names.reserve(checked.type_aliases.size());
    for (const auto& entry : checked.type_aliases) {
        alias_names.push_back(&entry.second);
    }
    std::sort(alias_names.begin(), alias_names.end(), [](const TypeAliasInfo* lhs, const TypeAliasInfo* rhs) {
        return lhs->name.view() < rhs->name.view();
    });
    out << "  type_aliases " << alias_names.size() << "\n";
    for (const TypeAliasInfo* const alias_ptr : alias_names) {
        const TypeAliasInfo& alias = *alias_ptr;
        TypeHandle resolved = INVALID_TYPE_HANDLE;
        if (alias.target.value < checked.syntax_type_handles.size()) {
            resolved = checked.syntax_type_handles[alias.target.value];
        }
        out << "    type ";
        if (!syntax::visibility_is_public(alias.visibility)) {
            out << syntax::visibility_name(alias.visibility) << " ";
        }
        out << alias.name << " = " << checked.types.display_name(resolved);
        append_part_origin(out, show_parts, alias.part_index);
        out << "\n";
    }

    out << "  enum_cases " << checked.enum_cases.size() << "\n";
    std::vector<const EnumCaseInfo*> enum_case_names;
    enum_case_names.reserve(checked.enum_cases.size());
    for (const auto& entry : checked.enum_cases) {
        enum_case_names.push_back(&entry.second);
    }
    std::sort(enum_case_names.begin(), enum_case_names.end(), [&](const EnumCaseInfo* lhs, const EnumCaseInfo* rhs) {
        return enum_case_display_name(checked.types, *lhs) < enum_case_display_name(checked.types, *rhs);
    });
    for (const EnumCaseInfo* const info_ptr : enum_case_names) {
        const EnumCaseInfo& info = *info_ptr;
        out << "    case " << enum_case_display_name(checked.types, info) << " : "
            << checked.types.display_name(info.type);
        if (!info.payload_types.empty()) {
            out << "(";
            for (base::usize i = 0; i < info.payload_types.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                out << checked.types.display_name(info.payload_types[i]);
            }
            out << ")";
        } else if (is_valid(info.payload_type)) {
            out << "(" << checked.types.display_name(info.payload_type) << ")";
        }
        if (const auto derived = checked.derived_capabilities_by_type.find(info.type.value);
            derived != checked.derived_capabilities_by_type.end() && !derived->second.empty()) {
            out << " derives=";
            for (base::usize i = 0; i < derived->second.size(); ++i) {
                if (i != 0) {
                    out << ",";
                }
                out << capability_name(derived->second[i].capability);
            }
        }
        out << " @c_name=" << info.c_name;
        append_part_origin(out, show_parts, info.part_index);
        out << "\n";
    }
    return out.str();
}

} // namespace aurex::sema
