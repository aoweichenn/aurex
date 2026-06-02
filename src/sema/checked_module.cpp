#include <aurex/sema/checked_module.hpp>
#include <aurex/sema/resource_semantics.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string_view>
#include <utility>

namespace aurex::sema {

namespace {

constexpr std::size_t SEMA_TRAIT_IMPL_HASH_MIX = 0x9e3779b97f4a7c15ULL;
constexpr base::usize SEMA_TRAIT_IMPL_HASH_LEFT_SHIFT = 6;
constexpr base::usize SEMA_TRAIT_IMPL_HASH_RIGHT_SHIFT = 2;

[[nodiscard]] std::size_t mix_trait_impl_hash(std::size_t hash, const std::uint64_t value) noexcept
{
    hash ^= static_cast<std::size_t>(value) + SEMA_TRAIT_IMPL_HASH_MIX + (hash << SEMA_TRAIT_IMPL_HASH_LEFT_SHIFT)
        + (hash >> SEMA_TRAIT_IMPL_HASH_RIGHT_SHIFT);
    return hash;
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

PatternCaseNameTable::PatternCaseNameTable(PatternCaseNameTable&& other) noexcept
    : arena_(std::move(other.arena_)), names_(std::move(other.names_))
{
    other.names_ = Map{};
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
      sparse_stmt_local_types(make_sema_map<base::u32, TypeHandle>(*this->arena_))
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

GenericSideTables::GenericSideTables(GenericSideTables&& other) noexcept
    : arena_(std::move(other.arena_)), analysis_arena_(std::move(other.analysis_arena_)), sparse(other.sparse),
      local_dense(other.local_dense), expr_span(other.expr_span), pattern_span(other.pattern_span),
      type_span(other.type_span), stmt_span(other.stmt_span), layout(other.layout),
      expr_node_ids(std::move(other.expr_node_ids)), pattern_node_ids(std::move(other.pattern_node_ids)),
      type_node_ids(std::move(other.type_node_ids)), stmt_node_ids(std::move(other.stmt_node_ids)),
      expr_intrinsic_types(std::move(other.expr_intrinsic_types)), expr_types(std::move(other.expr_types)),
      expr_expected_types(std::move(other.expr_expected_types)),
      expr_owned_use_modes(std::move(other.expr_owned_use_modes)), expr_c_name_ids(std::move(other.expr_c_name_ids)),
      pattern_c_name_ids(std::move(other.pattern_c_name_ids)),
      syntax_type_handles(std::move(other.syntax_type_handles)), stmt_local_types(std::move(other.stmt_local_types)),
      sparse_expr_intrinsic_types(std::move(other.sparse_expr_intrinsic_types)),
      sparse_expr_types(std::move(other.sparse_expr_types)),
      sparse_expr_expected_types(std::move(other.sparse_expr_expected_types)),
      sparse_expr_owned_use_modes(std::move(other.sparse_expr_owned_use_modes)),
      sparse_expr_c_name_ids(std::move(other.sparse_expr_c_name_ids)),
      sparse_pattern_c_name_ids(std::move(other.sparse_pattern_c_name_ids)),
      pattern_case_name_ids(std::move(other.pattern_case_name_ids)),
      sparse_syntax_type_handles(std::move(other.sparse_syntax_type_handles)),
      sparse_stmt_local_types(std::move(other.sparse_stmt_local_types)), sparse_fallbacks(other.sparse_fallbacks)
{
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

void GenericSideTables::configure_local_dense(const GenericSideTableLocalLayoutView& layout)
{
    this->sparse = true;
    this->local_dense = true;
    this->expr_span = layout.expr_span;
    this->pattern_span = layout.pattern_span;
    this->type_span = layout.type_span;
    this->stmt_span = layout.stmt_span;
    this->layout = nullptr;
    this->expr_node_ids.assign(layout.expr_node_ids.begin(), layout.expr_node_ids.end());
    this->pattern_node_ids.assign(layout.pattern_node_ids.begin(), layout.pattern_node_ids.end());
    this->type_node_ids.assign(layout.type_node_ids.begin(), layout.type_node_ids.end());
    this->stmt_node_ids.assign(layout.stmt_node_ids.begin(), layout.stmt_node_ids.end());
    const base::usize expr_count = this->expr_node_ids.empty() ? layout.expr_span.count : this->expr_node_ids.size();
    const base::usize pattern_count =
        this->pattern_node_ids.empty() ? layout.pattern_span.count : this->pattern_node_ids.size();
    const base::usize type_count = this->type_node_ids.empty() ? layout.type_span.count : this->type_node_ids.size();
    const base::usize stmt_count = this->stmt_node_ids.empty() ? layout.stmt_span.count : this->stmt_node_ids.size();
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
      item_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      coercions(make_sema_vector<CoercionRecord>(*this->arena_)),
      functions(make_sema_map<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash>(
          *this->arena_, FunctionLookupKeyHash{})),
      structs(make_sema_map<ModuleLookupKey, StructInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash{})),
      enum_cases(
          make_sema_map<ModuleLookupKey, EnumCaseInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash{})),
      type_aliases(
          make_sema_map<ModuleLookupKey, TypeAliasInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash{})),
      traits(make_sema_map<ModuleLookupKey, TraitSignature, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash{})),
      trait_impls(make_sema_map<TraitImplLookupKey, TraitImplInfo, TraitImplLookupKeyHash>(
          *this->arena_, TraitImplLookupKeyHash{})),
      trait_predicates(make_sema_vector<TraitPredicate>(*this->arena_)),
      trait_obligations(make_sema_vector<TraitObligation>(*this->arena_)),
      trait_evidence(make_sema_vector<TraitEvidence>(*this->arena_)),
      trait_method_calls(make_sema_vector<TraitMethodCallBinding>(*this->arena_)),
      body_flow_graphs(make_sema_map<FunctionLookupKey, BodyFlowGraph, FunctionLookupKeyHash>(
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

CheckedModule::CheckedModule(CheckedModule&& other) noexcept
    : arena_(std::move(other.arena_)), analysis_arena_(std::move(other.analysis_arena_)),
      c_names(std::move(other.c_names)), types(std::move(other.types)),
      expr_intrinsic_types(std::move(other.expr_intrinsic_types)), expr_types(std::move(other.expr_types)),
      expr_expected_types(std::move(other.expr_expected_types)),
      expr_owned_use_modes(std::move(other.expr_owned_use_modes)), expr_c_name_ids(std::move(other.expr_c_name_ids)),
      pattern_c_name_ids(std::move(other.pattern_c_name_ids)),
      pattern_case_name_ids(std::move(other.pattern_case_name_ids)),
      syntax_type_handles(std::move(other.syntax_type_handles)), stmt_local_types(std::move(other.stmt_local_types)),
      item_c_name_ids(std::move(other.item_c_name_ids)), coercions(std::move(other.coercions)),
      functions(std::move(other.functions)), structs(std::move(other.structs)), enum_cases(std::move(other.enum_cases)),
      type_aliases(std::move(other.type_aliases)), traits(std::move(other.traits)),
      trait_impls(std::move(other.trait_impls)), trait_predicates(std::move(other.trait_predicates)),
      trait_obligations(std::move(other.trait_obligations)), trait_evidence(std::move(other.trait_evidence)),
      trait_method_calls(std::move(other.trait_method_calls)), body_flow_graphs(std::move(other.body_flow_graphs)),
      param_envs(std::move(other.param_envs)),
      generic_template_signatures(std::move(other.generic_template_signatures)),
      generic_side_table_layouts(std::move(other.generic_side_table_layouts)),
      generic_enum_instances(std::move(other.generic_enum_instances)),
      generic_type_alias_instances(std::move(other.generic_type_alias_instances)),
      generic_function_instances(std::move(other.generic_function_instances)),
      trait_default_method_instances(std::move(other.trait_default_method_instances)),
      normalized_ast(other.normalized_ast)
{
    this->rebind_interned_texts(&other.c_names, this->c_names);
    this->rebind_generic_instance_layouts();
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
    this->item_c_name_ids.swap(other.item_c_name_ids);
    this->coercions.swap(other.coercions);
    this->functions.swap(other.functions);
    this->structs.swap(other.structs);
    this->enum_cases.swap(other.enum_cases);
    this->type_aliases.swap(other.type_aliases);
    this->traits.swap(other.traits);
    this->trait_impls.swap(other.trait_impls);
    this->trait_predicates.swap(other.trait_predicates);
    this->trait_obligations.swap(other.trait_obligations);
    this->trait_evidence.swap(other.trait_evidence);
    this->trait_method_calls.swap(other.trait_method_calls);
    this->body_flow_graphs.swap(other.body_flow_graphs);
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
    this->item_c_name_ids.assign(other.item_c_name_ids.begin(), other.item_c_name_ids.end());
    this->coercions.assign(other.coercions.begin(), other.coercions.end());
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
    this->trait_method_calls.reserve(other.trait_method_calls.size());
    for (const TraitMethodCallBinding& binding : other.trait_method_calls) {
        this->trait_method_calls.push_back(this->clone_trait_method_call_binding(binding));
    }
    this->body_flow_graphs.clear();
    this->body_flow_graphs.reserve(other.body_flow_graphs.size());
    for (const auto& entry : other.body_flow_graphs) {
        this->body_flow_graphs.emplace(entry.first, entry.second);
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
    signature.generic_args = this->make_type_handle_list();
    return signature;
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
    return requirement;
}

TraitAssociatedTypeRequirement CheckedModule::make_trait_associated_type_requirement() const
{
    return {};
}

TraitSignature CheckedModule::make_trait_signature() const
{
    TraitSignature signature;
    signature.generic_params = make_sema_vector<IdentId>(*this->arena_);
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
    return {};
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
    copy.visibility = other.visibility;
    copy.prototype_item = other.prototype_item;
    copy.definition_item = other.definition_item;
    copy.part_index = other.part_index;
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
    copy.visibility = other.visibility;
    copy.stable_key = other.stable_key;
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

TraitMethodCallBinding CheckedModule::clone_trait_method_call_binding(const TraitMethodCallBinding& other)
{
    TraitMethodCallBinding copy = other;
    copy.method_name = this->intern_text(other.method_name);
    return copy;
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

void rebind_trait_signature_texts(
    TraitSignature& signature, const IdentifierInterner* const from, const IdentifierInterner& to) noexcept
{
    rebind_interned_text(signature.name, from, to);
    for (TraitAssociatedTypeRequirement& associated_type : signature.associated_types) {
        rebind_interned_text(associated_type.name, from, to);
    }
    for (TraitMethodRequirement& requirement : signature.requirements) {
        rebind_interned_text(requirement.name, from, to);
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
    for (auto& entry : this->type_aliases) {
        rebind_interned_text(entry.second.name, from, to);
    }
    for (auto& entry : this->traits) {
        rebind_trait_signature_texts(entry.second, from, to);
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
        display.push_back('[');
        for (base::usize index = 0; index < trait.generic_params.size(); ++index) {
            if (index > 0) {
                display.append(", ");
            }
            display.push_back('T');
            display += std::to_string(index);
        }
        display.push_back(']');
    }
    return display;
}

[[nodiscard]] std::string trait_impl_display_name(const CheckedModule& checked, const TraitImplInfo& info)
{
    return checked.types.display_name(info.trait_name, info.trait_args);
}

void append_type_list(std::ostringstream& out, const CheckedModule& checked, std::span<const TypeHandle> types)
{
    for (base::usize index = 0; index < types.size(); ++index) {
        if (index > 0) {
            out << ", ";
        }
        out << checked.types.display_name(types[index]);
    }
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

} // namespace

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
            append_type_list(out, checked, requirement.param_types);
            out << ") -> " << checked.types.display_name(requirement.return_type);
            if (requirement.is_variadic) {
                out << " variadic";
            }
            if (requirement.has_default_body) {
                out << " default";
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
        append_part_origin(out, show_parts, binding.part_index);
        out << "\n";
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
        out << " @c_name=" << info.c_name;
        append_part_origin(out, show_parts, info.part_index);
        out << "\n";
    }
    return out.str();
}

} // namespace aurex::sema
