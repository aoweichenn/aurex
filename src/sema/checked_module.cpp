#include <aurex/sema/checked_module.hpp>

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

namespace aurex::sema {

PatternCaseNameTable::PatternCaseNameTable()
    : names_() {}

PatternCaseNameTable::PatternCaseNameTable(const PatternCaseNameTable& other)
    : PatternCaseNameTable() {
    this->copy_from(other);
}

PatternCaseNameTable& PatternCaseNameTable::operator=(const PatternCaseNameTable& other) {
    if (this == &other) {
        return *this;
    }
    PatternCaseNameTable copy(other);
    *this = std::move(copy);
    return *this;
}

PatternCaseNameTable::PatternCaseNameTable(PatternCaseNameTable&& other) noexcept
    : arena_(std::move(other.arena_)),
      names_(std::move(other.names_)) {
    other.names_ = Map {};
}

PatternCaseNameTable& PatternCaseNameTable::operator=(PatternCaseNameTable&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

bool PatternCaseNameTable::empty() const noexcept {
    return this->names_.empty();
}

base::usize PatternCaseNameTable::size() const noexcept {
    return this->names_.size();
}

bool PatternCaseNameTable::contains(const base::u32 pattern) const {
    return this->names_.contains(pattern);
}

void PatternCaseNameTable::reserve(const base::usize pattern_count) {
    if (pattern_count == 0) {
        return;
    }
    this->ensure_storage();
    this->names_.reserve(pattern_count);
}

void PatternCaseNameTable::clear() noexcept {
    this->names_.clear();
}

PatternCaseNameTable::iterator PatternCaseNameTable::begin() noexcept {
    return this->names_.begin();
}

PatternCaseNameTable::iterator PatternCaseNameTable::end() noexcept {
    return this->names_.end();
}

PatternCaseNameTable::const_iterator PatternCaseNameTable::begin() const noexcept {
    return this->names_.begin();
}

PatternCaseNameTable::const_iterator PatternCaseNameTable::end() const noexcept {
    return this->names_.end();
}

PatternCaseNameTable::const_iterator PatternCaseNameTable::find(const base::u32 pattern) const {
    return this->names_.find(pattern);
}

PatternCaseNameTable::iterator PatternCaseNameTable::find(const base::u32 pattern) {
    return this->names_.find(pattern);
}

CNameIdSet& PatternCaseNameTable::operator[](const base::u32 pattern) {
    this->ensure_storage();
    if (const auto found = this->names_.find(pattern); found != this->names_.end()) {
        return found->second;
    }
    const auto inserted = this->names_.emplace(pattern, this->make_bucket());
    return inserted.first->second;
}

void PatternCaseNameTable::insert(const base::u32 pattern, const IdentId c_name_id) {
    static_cast<void>((*this)[pattern].insert(c_name_id));
}

void PatternCaseNameTable::merge(const base::u32 pattern, const CNameIdSet& source) {
    if (source.empty()) {
        return;
    }
    CNameIdSet& target = (*this)[pattern];
    target.insert(source.begin(), source.end());
}

base::usize PatternCaseNameTable::arena_bytes() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize PatternCaseNameTable::arena_blocks() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

CNameIdSet PatternCaseNameTable::make_bucket() {
    this->ensure_storage();
    return make_sema_set<IdentId, IdentIdHash>(*this->arena_, IdentIdHash {});
}

void PatternCaseNameTable::ensure_storage() {
    if (this->arena_ != nullptr) {
        return;
    }
    this->arena_ = std::make_unique<base::BumpAllocator>(SEMA_PATTERN_CASE_NAME_TABLE_BLOCK_BYTES);
    this->names_ = make_sema_map<base::u32, CNameIdSet>(*this->arena_);
}

void PatternCaseNameTable::swap(PatternCaseNameTable& other) noexcept {
    using std::swap;
    swap(this->arena_, other.arena_);
    this->names_.swap(other.names_);
}

void PatternCaseNameTable::copy_from(const PatternCaseNameTable& other) {
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
      expr_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      pattern_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      syntax_type_handles(make_sema_vector<TypeHandle>(*this->arena_)),
      stmt_local_types(make_sema_vector<TypeHandle>(*this->arena_)),
      sparse_expr_intrinsic_types(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
      sparse_expr_types(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
      sparse_expr_c_name_ids(make_sema_map<base::u32, IdentId>(*this->arena_)),
      sparse_pattern_c_name_ids(make_sema_map<base::u32, IdentId>(*this->arena_)),
      sparse_syntax_type_handles(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
      sparse_stmt_local_types(make_sema_map<base::u32, TypeHandle>(*this->arena_)) {}

GenericSideTables::GenericSideTables(const GenericSideTables& other)
    : GenericSideTables() {
    this->copy_from(other);
}

GenericSideTables& GenericSideTables::operator=(const GenericSideTables& other) {
    if (this == &other) {
        return *this;
    }
    GenericSideTables copy(other);
    *this = std::move(copy);
    return *this;
}

GenericSideTables::GenericSideTables(GenericSideTables&& other) noexcept
    : arena_(std::move(other.arena_)),
      analysis_arena_(std::move(other.analysis_arena_)),
      sparse(other.sparse),
      local_dense(other.local_dense),
      expr_span(other.expr_span),
      pattern_span(other.pattern_span),
      type_span(other.type_span),
      stmt_span(other.stmt_span),
      layout(other.layout),
      expr_node_ids(std::move(other.expr_node_ids)),
      pattern_node_ids(std::move(other.pattern_node_ids)),
      type_node_ids(std::move(other.type_node_ids)),
      stmt_node_ids(std::move(other.stmt_node_ids)),
      expr_intrinsic_types(std::move(other.expr_intrinsic_types)),
      expr_types(std::move(other.expr_types)),
      expr_expected_types(std::move(other.expr_expected_types)),
      expr_c_name_ids(std::move(other.expr_c_name_ids)),
      pattern_c_name_ids(std::move(other.pattern_c_name_ids)),
      syntax_type_handles(std::move(other.syntax_type_handles)),
      stmt_local_types(std::move(other.stmt_local_types)),
      sparse_expr_intrinsic_types(std::move(other.sparse_expr_intrinsic_types)),
      sparse_expr_types(std::move(other.sparse_expr_types)),
      sparse_expr_expected_types(std::move(other.sparse_expr_expected_types)),
      sparse_expr_c_name_ids(std::move(other.sparse_expr_c_name_ids)),
      sparse_pattern_c_name_ids(std::move(other.sparse_pattern_c_name_ids)),
      pattern_case_name_ids(std::move(other.pattern_case_name_ids)),
      sparse_syntax_type_handles(std::move(other.sparse_syntax_type_handles)),
      sparse_stmt_local_types(std::move(other.sparse_stmt_local_types)),
      sparse_fallbacks(other.sparse_fallbacks) {}

GenericSideTables& GenericSideTables::operator=(GenericSideTables&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

base::usize GenericSideTables::arena_bytes() const noexcept {
    return (this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes()) +
           (this->analysis_arena_ == nullptr ? 0 : this->analysis_arena_->allocated_bytes());
}

base::usize GenericSideTables::arena_blocks() const noexcept {
    return (this->arena_ == nullptr ? 0 : this->arena_->block_count()) +
           (this->analysis_arena_ == nullptr ? 0 : this->analysis_arena_->block_count());
}

void GenericSideTables::record_sparse_fallback(const GenericSparseFallbackKind kind) noexcept {
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
    const GenericNodeSpan expr,
    const GenericNodeSpan pattern,
    const GenericNodeSpan type,
    const GenericNodeSpan stmt
) {
    this->configure_local_dense(expr, pattern, type, stmt, {}, {}, {}, {});
}

void GenericSideTables::configure_local_dense(
    const GenericNodeSpan expr,
    const GenericNodeSpan pattern,
    const GenericNodeSpan type,
    const GenericNodeSpan stmt,
    const std::span<const base::u32> expr_ids,
    const std::span<const base::u32> pattern_ids,
    const std::span<const base::u32> type_ids,
    const std::span<const base::u32> stmt_ids
) {
    this->sparse = true;
    this->local_dense = true;
    this->expr_span = expr;
    this->pattern_span = pattern;
    this->type_span = type;
    this->stmt_span = stmt;
    this->layout = nullptr;
    this->expr_node_ids.assign(expr_ids.begin(), expr_ids.end());
    this->pattern_node_ids.assign(pattern_ids.begin(), pattern_ids.end());
    this->type_node_ids.assign(type_ids.begin(), type_ids.end());
    this->stmt_node_ids.assign(stmt_ids.begin(), stmt_ids.end());
    const base::usize expr_count = this->expr_node_ids.empty() ? expr.count : this->expr_node_ids.size();
    const base::usize pattern_count = this->pattern_node_ids.empty() ? pattern.count : this->pattern_node_ids.size();
    const base::usize type_count = this->type_node_ids.empty() ? type.count : this->type_node_ids.size();
    const base::usize stmt_count = this->stmt_node_ids.empty() ? stmt.count : this->stmt_node_ids.size();
    this->expr_intrinsic_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->prepare_analysis_only_storage(expr_count);
    this->expr_expected_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_c_name_ids.assign(expr_count, INVALID_IDENT_ID);
    this->pattern_c_name_ids.assign(pattern_count, INVALID_IDENT_ID);
    this->syntax_type_handles.assign(type_count, INVALID_TYPE_HANDLE);
    this->stmt_local_types.assign(stmt_count, INVALID_TYPE_HANDLE);
    this->sparse_expr_intrinsic_types.clear();
    this->sparse_expr_types.clear();
    this->sparse_expr_c_name_ids.clear();
    this->sparse_pattern_c_name_ids.clear();
    this->sparse_syntax_type_handles.clear();
    this->sparse_stmt_local_types.clear();
    this->sparse_fallbacks = {};
}

void GenericSideTables::configure_local_dense(const GenericSideTableLayout& shared_layout) {
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
    const base::usize expr_count = shared_layout.expr_node_ids.empty()
        ? shared_layout.expr_span.count
        : shared_layout.expr_node_ids.size();
    const base::usize pattern_count = shared_layout.pattern_node_ids.empty()
        ? shared_layout.pattern_span.count
        : shared_layout.pattern_node_ids.size();
    const base::usize type_count = shared_layout.type_node_ids.empty()
        ? shared_layout.type_span.count
        : shared_layout.type_node_ids.size();
    const base::usize stmt_count = shared_layout.stmt_node_ids.empty()
        ? shared_layout.stmt_span.count
        : shared_layout.stmt_node_ids.size();
    this->expr_intrinsic_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->prepare_analysis_only_storage(expr_count);
    this->expr_expected_types.assign(expr_count, INVALID_TYPE_HANDLE);
    this->expr_c_name_ids.assign(expr_count, INVALID_IDENT_ID);
    this->pattern_c_name_ids.assign(pattern_count, INVALID_IDENT_ID);
    this->syntax_type_handles.assign(type_count, INVALID_TYPE_HANDLE);
    this->stmt_local_types.assign(stmt_count, INVALID_TYPE_HANDLE);
    this->sparse_expr_intrinsic_types.clear();
    this->sparse_expr_types.clear();
    this->sparse_expr_c_name_ids.clear();
    this->sparse_pattern_c_name_ids.clear();
    this->sparse_syntax_type_handles.clear();
    this->sparse_stmt_local_types.clear();
    this->sparse_fallbacks = {};
}

void GenericSideTables::bind_local_dense_layout(const GenericSideTableLayout& shared_layout) noexcept {
    this->sparse = true;
    this->local_dense = true;
    this->expr_span = shared_layout.expr_span;
    this->pattern_span = shared_layout.pattern_span;
    this->type_span = shared_layout.type_span;
    this->stmt_span = shared_layout.stmt_span;
    this->layout = &shared_layout;
}

void GenericSideTables::prepare_analysis_only_storage(const base::usize expr_count) {
    this->expr_expected_types = SemaTypeTable {};
    this->sparse_expr_expected_types = SemaMap<base::u32, TypeHandle> {};
    this->analysis_arena_ = std::make_unique<base::BumpAllocator>(SEMA_GENERIC_SIDE_TABLE_BLOCK_BYTES);
    this->expr_expected_types = make_sema_vector<TypeHandle>(*this->analysis_arena_);
    this->sparse_expr_expected_types = make_sema_map<base::u32, TypeHandle>(*this->analysis_arena_);
    this->expr_expected_types.reserve(expr_count);
}

void GenericSideTables::release_analysis_only_storage() {
    this->expr_expected_types = SemaTypeTable {};
    this->sparse_expr_expected_types = SemaMap<base::u32, TypeHandle> {};
    this->analysis_arena_.reset();
    this->pattern_case_name_ids = PatternCaseNameTable {};
}

void GenericSideTables::swap(GenericSideTables& other) noexcept {
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
    this->expr_c_name_ids.swap(other.expr_c_name_ids);
    this->pattern_c_name_ids.swap(other.pattern_c_name_ids);
    this->syntax_type_handles.swap(other.syntax_type_handles);
    this->stmt_local_types.swap(other.stmt_local_types);
    this->sparse_expr_intrinsic_types.swap(other.sparse_expr_intrinsic_types);
    this->sparse_expr_types.swap(other.sparse_expr_types);
    this->sparse_expr_expected_types.swap(other.sparse_expr_expected_types);
    this->sparse_expr_c_name_ids.swap(other.sparse_expr_c_name_ids);
    this->sparse_pattern_c_name_ids.swap(other.sparse_pattern_c_name_ids);
    swap(this->pattern_case_name_ids, other.pattern_case_name_ids);
    this->sparse_syntax_type_handles.swap(other.sparse_syntax_type_handles);
    this->sparse_stmt_local_types.swap(other.sparse_stmt_local_types);
    swap(this->sparse_fallbacks, other.sparse_fallbacks);
    swap(this->arena_, other.arena_);
    swap(this->analysis_arena_, other.analysis_arena_);
}

void GenericSideTables::copy_from(const GenericSideTables& other) {
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
      expr_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      pattern_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      syntax_type_handles(make_sema_vector<TypeHandle>(*this->arena_)),
      stmt_local_types(make_sema_vector<TypeHandle>(*this->arena_)),
      item_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      coercions(make_sema_vector<CoercionRecord>(*this->arena_)),
      functions(make_sema_map<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash>(
          *this->arena_,
          FunctionLookupKeyHash {}
      )),
      structs(make_sema_map<ModuleLookupKey, StructInfo, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      enum_cases(make_sema_map<ModuleLookupKey, EnumCaseInfo, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      type_aliases(make_sema_map<ModuleLookupKey, TypeAliasInfo, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_side_table_layouts(make_sema_deque<GenericSideTableLayout>(*this->arena_)),
      generic_function_instances(make_sema_deque<GenericFunctionInstanceInfo>(*this->arena_)) {}

CheckedModule::CheckedModule(const CheckedModule& other)
    : CheckedModule() {
    this->copy_from(other);
}

CheckedModule& CheckedModule::operator=(const CheckedModule& other) {
    if (this == &other) {
        return *this;
    }
    CheckedModule copy(other);
    *this = std::move(copy);
    return *this;
}

CheckedModule::CheckedModule(CheckedModule&& other) noexcept
    : arena_(std::move(other.arena_)),
      analysis_arena_(std::move(other.analysis_arena_)),
      c_names(std::move(other.c_names)),
      types(std::move(other.types)),
      expr_intrinsic_types(std::move(other.expr_intrinsic_types)),
      expr_types(std::move(other.expr_types)),
      expr_expected_types(std::move(other.expr_expected_types)),
      expr_c_name_ids(std::move(other.expr_c_name_ids)),
      pattern_c_name_ids(std::move(other.pattern_c_name_ids)),
      pattern_case_name_ids(std::move(other.pattern_case_name_ids)),
      syntax_type_handles(std::move(other.syntax_type_handles)),
      stmt_local_types(std::move(other.stmt_local_types)),
      item_c_name_ids(std::move(other.item_c_name_ids)),
      coercions(std::move(other.coercions)),
      functions(std::move(other.functions)),
      structs(std::move(other.structs)),
      enum_cases(std::move(other.enum_cases)),
      type_aliases(std::move(other.type_aliases)),
      generic_side_table_layouts(std::move(other.generic_side_table_layouts)),
      generic_function_instances(std::move(other.generic_function_instances)),
      normalized_ast(other.normalized_ast) {
    this->rebind_interned_texts(&other.c_names, this->c_names);
    this->rebind_generic_instance_layouts();
}

CheckedModule& CheckedModule::operator=(CheckedModule&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

base::usize CheckedModule::arena_bytes() const noexcept {
    return (this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes()) +
           (this->analysis_arena_ == nullptr ? 0 : this->analysis_arena_->allocated_bytes());
}

base::usize CheckedModule::arena_blocks() const noexcept {
    return (this->arena_ == nullptr ? 0 : this->arena_->block_count()) +
           (this->analysis_arena_ == nullptr ? 0 : this->analysis_arena_->block_count());
}

void CheckedModule::swap(CheckedModule& other) noexcept {
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
    this->generic_side_table_layouts.swap(other.generic_side_table_layouts);
    this->generic_function_instances.swap(other.generic_function_instances);
    swap(this->normalized_ast, other.normalized_ast);
    this->rebind_interned_texts(other_c_names, this->c_names);
    other.rebind_interned_texts(this_c_names, other.c_names);
    this->rebind_generic_instance_layouts();
    other.rebind_generic_instance_layouts();
}

void CheckedModule::copy_from(const CheckedModule& other) {
    this->c_names = other.c_names;
    this->types = other.types;
    this->expr_intrinsic_types.assign(other.expr_intrinsic_types.begin(), other.expr_intrinsic_types.end());
    this->expr_types.assign(other.expr_types.begin(), other.expr_types.end());
    if (other.expr_expected_types.empty()) {
        this->expr_expected_types = SemaTypeTable {};
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
        alias.target = entry.second.target;
        alias.range = entry.second.range;
        alias.visibility = entry.second.visibility;
        alias.stable_id = entry.second.stable_id;
        alias.incremental_key = entry.second.incremental_key;
        this->type_aliases.emplace(entry.first, alias);
    }
    this->generic_side_table_layouts.clear();
    for (const GenericSideTableLayout& layout : other.generic_side_table_layouts) {
        this->generic_side_table_layouts.push_back(this->clone_generic_side_table_layout(layout));
    }
    this->generic_function_instances.clear();
    for (const GenericFunctionInstanceInfo& instance : other.generic_function_instances) {
        this->generic_function_instances.push_back(this->clone_generic_function_instance(instance));
    }
    this->rebind_generic_instance_layouts();
    this->normalized_ast = other.normalized_ast;
}

TypeHandleList CheckedModule::make_type_handle_list() const
{
    return make_sema_vector<TypeHandle>(*this->arena_);
}

TypeHandleList CheckedModule::copy_type_handle_list(const std::span<const TypeHandle> values) const {
    TypeHandleList copy = this->make_type_handle_list();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

SemaVector<StructFieldInfo> CheckedModule::make_struct_field_list() const
{
    return make_sema_vector<StructFieldInfo>(*this->arena_);
}

SemaVector<StructFieldInfo> CheckedModule::copy_struct_field_list(
    const std::span<const StructFieldInfo> values
) {
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

SemaIndexTable CheckedModule::make_index_table() const {
    return make_sema_vector<base::u32>(*this->arena_);
}

SemaIndexTable CheckedModule::copy_index_table(const std::span<const base::u32> values) const {
    SemaIndexTable copy = this->make_index_table();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

FunctionSignature CheckedModule::make_function_signature() const {
    FunctionSignature signature;
    signature.param_types = this->make_type_handle_list();
    signature.generic_args = this->make_type_handle_list();
    return signature;
}

StructInfo CheckedModule::make_struct_info() const {
    StructInfo info;
    info.fields = this->make_struct_field_list();
    return info;
}

EnumCaseInfo CheckedModule::make_enum_case_info() const {
    EnumCaseInfo info;
    info.payload_types = this->make_type_handle_list();
    return info;
}

GenericSideTableLayout CheckedModule::make_generic_side_table_layout(
    const GenericNodeSpan expr,
    const GenericNodeSpan pattern,
    const GenericNodeSpan type,
    const GenericNodeSpan stmt,
    const std::span<const base::u32> expr_ids,
    const std::span<const base::u32> pattern_ids,
    const std::span<const base::u32> type_ids,
    const std::span<const base::u32> stmt_ids
) const {
    GenericSideTableLayout layout;
    layout.expr_span = expr;
    layout.pattern_span = pattern;
    layout.type_span = type;
    layout.stmt_span = stmt;
    layout.expr_node_ids = this->copy_index_table(expr_ids);
    layout.pattern_node_ids = this->copy_index_table(pattern_ids);
    layout.type_node_ids = this->copy_index_table(type_ids);
    layout.stmt_node_ids = this->copy_index_table(stmt_ids);
    return layout;
}

base::usize CheckedModule::append_generic_side_table_layout(
    const GenericNodeSpan expr,
    const GenericNodeSpan pattern,
    const GenericNodeSpan type,
    const GenericNodeSpan stmt,
    const std::span<const base::u32> expr_ids,
    const std::span<const base::u32> pattern_ids,
    const std::span<const base::u32> type_ids,
    const std::span<const base::u32> stmt_ids
) {
    const base::usize index = this->generic_side_table_layouts.size();
    this->generic_side_table_layouts.push_back(this->make_generic_side_table_layout(
        expr,
        pattern,
        type,
        stmt,
        expr_ids,
        pattern_ids,
        type_ids,
        stmt_ids
    ));
    return index;
}

const GenericSideTableLayout* CheckedModule::generic_side_table_layout(const base::usize index) const noexcept {
    return index < this->generic_side_table_layouts.size()
        ? &this->generic_side_table_layouts[index]
        : nullptr;
}

FunctionSignature CheckedModule::clone_function_signature(const FunctionSignature& other) {
    FunctionSignature copy = this->make_function_signature();
    copy.name = this->intern_text(other.name);
    copy.name_id = other.name_id;
    copy.semantic_key = other.semantic_key;
    copy.stable_id = other.stable_id;
    copy.incremental_key = other.incremental_key;
    copy.c_name = this->intern_text(other.c_name);
    copy.module = other.module;
    copy.method_owner_type = other.method_owner_type;
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
    copy.visibility = other.visibility;
    copy.prototype_item = other.prototype_item;
    copy.definition_item = other.definition_item;
    return copy;
}

StructInfo CheckedModule::clone_struct_info(const StructInfo& other) {
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
    return copy;
}

EnumCaseInfo CheckedModule::clone_enum_case_info(const EnumCaseInfo& other) {
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
    return copy;
}

GenericSideTableLayout CheckedModule::clone_generic_side_table_layout(
    const GenericSideTableLayout& other
) const {
    return this->make_generic_side_table_layout(
        other.expr_span,
        other.pattern_span,
        other.type_span,
        other.stmt_span,
        other.expr_node_ids,
        other.pattern_node_ids,
        other.type_node_ids,
        other.stmt_node_ids
    );
}

GenericFunctionInstanceInfo CheckedModule::clone_generic_function_instance(
    const GenericFunctionInstanceInfo& other
) {
    GenericFunctionInstanceInfo copy;
    copy.key = other.key;
    copy.item = other.item;
    copy.signature = this->clone_function_signature(other.signature);
    copy.side_table_layout_index = other.side_table_layout_index;
    copy.side_tables = other.side_tables;
    return copy;
}

void CheckedModule::prepare_analysis_only_storage(const base::usize expr_count) {
    this->expr_expected_types = SemaTypeTable {};
    this->analysis_arena_ = std::make_unique<base::BumpAllocator>(SEMA_GENERIC_SIDE_TABLE_BLOCK_BYTES);
    this->expr_expected_types = make_sema_vector<TypeHandle>(*this->analysis_arena_);
    this->expr_expected_types.reserve(expr_count);
}

void CheckedModule::release_analysis_only_storage() {
    this->expr_expected_types = SemaTypeTable {};
    this->analysis_arena_.reset();
    this->pattern_case_name_ids = PatternCaseNameTable {};
    for (GenericFunctionInstanceInfo& instance : this->generic_function_instances) {
        instance.side_tables.release_analysis_only_storage();
    }
}

namespace {

void rebind_function_signature_texts(
    FunctionSignature& signature,
    const IdentifierInterner* const from,
    const IdentifierInterner& to
) noexcept {
    rebind_interned_text(signature.name, from, to);
    rebind_interned_text(signature.c_name, from, to);
}

void rebind_struct_info_texts(
    StructInfo& info,
    const IdentifierInterner* const from,
    const IdentifierInterner& to
) noexcept {
    rebind_interned_text(info.name, from, to);
    rebind_interned_text(info.c_name, from, to);
    for (StructFieldInfo& field : info.fields) {
        rebind_interned_text(field.name, from, to);
        rebind_interned_text(field.c_name, from, to);
    }
}

void rebind_enum_case_info_texts(
    EnumCaseInfo& info,
    const IdentifierInterner* const from,
    const IdentifierInterner& to
) noexcept {
    rebind_interned_text(info.name, from, to);
    rebind_interned_text(info.c_name, from, to);
    rebind_interned_text(info.value_text, from, to);
    rebind_interned_text(info.enum_name, from, to);
    rebind_interned_text(info.case_name, from, to);
}

} // namespace

void CheckedModule::rebind_interned_texts(
    const IdentifierInterner* const from,
    const IdentifierInterner& to
) noexcept {
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
    for (GenericFunctionInstanceInfo& instance : this->generic_function_instances) {
        rebind_function_signature_texts(instance.signature, from, to);
    }
}

void CheckedModule::rebind_generic_instance_layouts() noexcept {
    for (GenericFunctionInstanceInfo& instance : this->generic_function_instances) {
        const GenericSideTableLayout* const layout =
            this->generic_side_table_layout(instance.side_table_layout_index);
        if (layout != nullptr && instance.side_tables.local_dense) {
            instance.side_tables.bind_local_dense_layout(*layout);
        }
    }
}

void CheckedModule::reserve_side_table_storage(
    const base::usize expr_count,
    const base::usize pattern_count,
    const base::usize type_count,
    const base::usize stmt_count,
    const base::usize item_count
) const
{
    const base::usize type_handle_slots = (expr_count * 2U) + type_count + stmt_count;
    const base::usize ident_slots = expr_count + pattern_count + item_count;
    const base::usize bytes =
        type_handle_slots * sizeof(TypeHandle) +
        ident_slots * sizeof(IdentId);
    this->arena_->reserve_touched(bytes);
}

namespace {

[[nodiscard]] std::span<const TypeHandle> generic_args_for_type(const TypeTable& types, const TypeHandle type) {
    if (!is_valid(type) || type.value >= types.size()) {
        return {};
    }
    return types.get(type).generic_args;
}

} // namespace

std::string struct_display_name(const TypeTable& types, const StructInfo& info) {
    return types.display_name(info.name.view(), generic_args_for_type(types, info.type));
}

std::string enum_display_name(const TypeTable& types, const EnumCaseInfo& info) {
    return types.display_name(info.enum_name.view(), generic_args_for_type(types, info.type));
}

std::string enum_case_display_name(const TypeTable& types, const EnumCaseInfo& info) {
    std::string display = enum_display_name(types, info);
    display += "_";
    display += info.case_name.view();
    return display;
}

std::string dump_checked_module(const CheckedModule& checked) {
    std::ostringstream out;
    out << "checked_module\n";
    out << "  expr_types " << checked.expr_types.size() << "\n";

    std::vector<const FunctionSignature*> function_names;
    function_names.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        function_names.push_back(&entry.second);
    }
    std::sort(function_names.begin(), function_names.end(), [&](const FunctionSignature* lhs, const FunctionSignature* rhs) {
        return function_display_name(checked.types, *lhs) < function_display_name(checked.types, *rhs);
    });
    out << "  functions " << function_names.size() << "\n";
    for (const FunctionSignature* const fn_ptr : function_names) {
        const FunctionSignature& fn = *fn_ptr;
        const std::string display_name = function_display_name(checked.types, fn);
        out << "    fn ";
        if (fn.visibility == syntax::Visibility::private_) {
            out << "priv ";
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
        if (info.visibility == syntax::Visibility::private_) {
            out << "priv ";
        }
        out << struct_display_name(checked.types, info);
        if (info.is_opaque) {
            out << " opaque";
        }
        if (info.is_generic_placeholder) {
            out << " generic_placeholder";
        }
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
        if (alias.visibility == syntax::Visibility::private_) {
            out << "priv ";
        }
        out << alias.name << " = " << checked.types.display_name(resolved) << "\n";
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
        out << "    case " << enum_case_display_name(checked.types, info) << " : " << checked.types.display_name(info.type);
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
        out << " @c_name=" << info.c_name << "\n";
    }
    return out.str();
}

} // namespace aurex::sema
