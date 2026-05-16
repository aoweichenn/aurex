#include <aurex/sema/checked_module.hpp>

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

namespace aurex::sema {

PatternCaseNameTable::PatternCaseNameTable()
    : arena_(std::make_unique<base::BumpAllocator>()),
      names_(make_sema_map<base::u32, CNameIdSet>(*this->arena_)) {}

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
    auto inserted = this->names_.emplace(pattern, this->make_bucket());
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
    this->arena_ = std::make_unique<base::BumpAllocator>();
    this->names_ = make_sema_map<base::u32, CNameIdSet>(*this->arena_);
}

void PatternCaseNameTable::swap(PatternCaseNameTable& other) noexcept {
    using std::swap;
    swap(this->arena_, other.arena_);
    this->names_.swap(other.names_);
}

void PatternCaseNameTable::copy_from(const PatternCaseNameTable& other) {
    this->reserve(other.size());
    for (const auto& entry : other) {
        CNameIdSet& bucket = (*this)[entry.first];
        bucket.reserve(entry.second.size());
        bucket.insert(entry.second.begin(), entry.second.end());
    }
}

GenericSideTables::GenericSideTables()
    : arena_(std::make_unique<base::BumpAllocator>()),
      expr_types(make_sema_vector<TypeHandle>(*this->arena_)),
      expr_expected_types(make_sema_vector<TypeHandle>(*this->arena_)),
      expr_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      pattern_c_name_ids(make_sema_vector<IdentId>(*this->arena_)),
      syntax_type_handles(make_sema_vector<TypeHandle>(*this->arena_)),
      stmt_local_types(make_sema_vector<TypeHandle>(*this->arena_)),
      sparse_expr_types(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
      sparse_expr_expected_types(make_sema_map<base::u32, TypeHandle>(*this->arena_)),
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
      sparse(other.sparse),
      expr_types(std::move(other.expr_types)),
      expr_expected_types(std::move(other.expr_expected_types)),
      expr_c_name_ids(std::move(other.expr_c_name_ids)),
      pattern_c_name_ids(std::move(other.pattern_c_name_ids)),
      syntax_type_handles(std::move(other.syntax_type_handles)),
      stmt_local_types(std::move(other.stmt_local_types)),
      sparse_expr_types(std::move(other.sparse_expr_types)),
      sparse_expr_expected_types(std::move(other.sparse_expr_expected_types)),
      sparse_expr_c_name_ids(std::move(other.sparse_expr_c_name_ids)),
      sparse_pattern_c_name_ids(std::move(other.sparse_pattern_c_name_ids)),
      pattern_case_name_ids(std::move(other.pattern_case_name_ids)),
      sparse_syntax_type_handles(std::move(other.sparse_syntax_type_handles)),
      sparse_stmt_local_types(std::move(other.sparse_stmt_local_types)) {}

GenericSideTables& GenericSideTables::operator=(GenericSideTables&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

base::usize GenericSideTables::arena_bytes() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize GenericSideTables::arena_blocks() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

void GenericSideTables::swap(GenericSideTables& other) noexcept {
    using std::swap;
    swap(this->sparse, other.sparse);
    this->expr_types.swap(other.expr_types);
    this->expr_expected_types.swap(other.expr_expected_types);
    this->expr_c_name_ids.swap(other.expr_c_name_ids);
    this->pattern_c_name_ids.swap(other.pattern_c_name_ids);
    this->syntax_type_handles.swap(other.syntax_type_handles);
    this->stmt_local_types.swap(other.stmt_local_types);
    this->sparse_expr_types.swap(other.sparse_expr_types);
    this->sparse_expr_expected_types.swap(other.sparse_expr_expected_types);
    this->sparse_expr_c_name_ids.swap(other.sparse_expr_c_name_ids);
    this->sparse_pattern_c_name_ids.swap(other.sparse_pattern_c_name_ids);
    swap(this->pattern_case_name_ids, other.pattern_case_name_ids);
    this->sparse_syntax_type_handles.swap(other.sparse_syntax_type_handles);
    this->sparse_stmt_local_types.swap(other.sparse_stmt_local_types);
    swap(this->arena_, other.arena_);
}

void GenericSideTables::copy_from(const GenericSideTables& other) {
    this->sparse = other.sparse;
    this->expr_types.assign(other.expr_types.begin(), other.expr_types.end());
    this->expr_expected_types.assign(other.expr_expected_types.begin(), other.expr_expected_types.end());
    this->expr_c_name_ids.assign(other.expr_c_name_ids.begin(), other.expr_c_name_ids.end());
    this->pattern_c_name_ids.assign(other.pattern_c_name_ids.begin(), other.pattern_c_name_ids.end());
    this->syntax_type_handles.assign(other.syntax_type_handles.begin(), other.syntax_type_handles.end());
    this->stmt_local_types.assign(other.stmt_local_types.begin(), other.stmt_local_types.end());
    this->sparse_expr_types = other.sparse_expr_types;
    this->sparse_expr_expected_types = other.sparse_expr_expected_types;
    this->sparse_expr_c_name_ids = other.sparse_expr_c_name_ids;
    this->sparse_pattern_c_name_ids = other.sparse_pattern_c_name_ids;
    this->pattern_case_name_ids = other.pattern_case_name_ids;
    this->sparse_syntax_type_handles = other.sparse_syntax_type_handles;
    this->sparse_stmt_local_types = other.sparse_stmt_local_types;
}

CheckedModule::CheckedModule()
    : arena_(std::make_unique<base::BumpAllocator>()),
      expr_types(make_sema_vector<TypeHandle>(*this->arena_)),
      expr_expected_types(make_sema_vector<TypeHandle>(*this->arena_)),
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
      c_names(std::move(other.c_names)),
      types(std::move(other.types)),
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
      generic_function_instances(std::move(other.generic_function_instances)),
      normalized_ast(other.normalized_ast) {}

CheckedModule& CheckedModule::operator=(CheckedModule&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

base::usize CheckedModule::arena_bytes() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize CheckedModule::arena_blocks() const noexcept {
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

void CheckedModule::swap(CheckedModule& other) noexcept {
    using std::swap;
    swap(this->arena_, other.arena_);
    swap(this->c_names, other.c_names);
    swap(this->types, other.types);
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
    this->generic_function_instances.swap(other.generic_function_instances);
    swap(this->normalized_ast, other.normalized_ast);
}

void CheckedModule::copy_from(const CheckedModule& other) {
    this->c_names = other.c_names;
    this->types = other.types;
    this->expr_types.assign(other.expr_types.begin(), other.expr_types.end());
    this->expr_expected_types.assign(other.expr_expected_types.begin(), other.expr_expected_types.end());
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
    this->type_aliases = other.type_aliases;
    this->generic_function_instances.clear();
    for (const GenericFunctionInstanceInfo& instance : other.generic_function_instances) {
        this->generic_function_instances.push_back(this->clone_generic_function_instance(instance));
    }
    this->normalized_ast = other.normalized_ast;
}

TypeHandleList CheckedModule::make_type_handle_list() const
{
    return make_sema_vector<TypeHandle>(*this->arena_);
}

TypeHandleList CheckedModule::copy_type_handle_list(const std::span<const TypeHandle> values) {
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
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

FunctionSignature CheckedModule::make_function_signature() {
    FunctionSignature signature;
    signature.param_types = this->make_type_handle_list();
    signature.generic_args = this->make_type_handle_list();
    return signature;
}

StructInfo CheckedModule::make_struct_info() {
    StructInfo info;
    info.fields = this->make_struct_field_list();
    return info;
}

EnumCaseInfo CheckedModule::make_enum_case_info() {
    EnumCaseInfo info;
    info.payload_types = this->make_type_handle_list();
    return info;
}

FunctionSignature CheckedModule::clone_function_signature(const FunctionSignature& other) {
    FunctionSignature copy = this->make_function_signature();
    copy.name = other.name;
    copy.name_id = other.name_id;
    copy.semantic_key = other.semantic_key;
    copy.c_name = other.c_name;
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
    copy.name = other.name;
    copy.name_id = other.name_id;
    copy.c_name = other.c_name;
    copy.module = other.module;
    copy.type = other.type;
    copy.fields = this->copy_struct_field_list(other.fields);
    copy.is_opaque = other.is_opaque;
    copy.is_generic_placeholder = other.is_generic_placeholder;
    copy.visibility = other.visibility;
    return copy;
}

EnumCaseInfo CheckedModule::clone_enum_case_info(const EnumCaseInfo& other) {
    EnumCaseInfo copy = this->make_enum_case_info();
    copy.name = other.name;
    copy.name_id = other.name_id;
    copy.c_name = other.c_name;
    copy.module = other.module;
    copy.type = other.type;
    copy.payload_type = other.payload_type;
    copy.payload_types = this->copy_type_handle_list(other.payload_types);
    copy.value_text = other.value_text;
    copy.range = other.range;
    copy.enum_name = other.enum_name;
    copy.case_name = other.case_name;
    copy.case_name_id = other.case_name_id;
    copy.visibility = other.visibility;
    return copy;
}

GenericFunctionInstanceInfo CheckedModule::clone_generic_function_instance(
    const GenericFunctionInstanceInfo& other
) {
    GenericFunctionInstanceInfo copy;
    copy.key = other.key;
    copy.item = other.item;
    copy.signature = this->clone_function_signature(other.signature);
    copy.side_tables = other.side_tables;
    return copy;
}

void CheckedModule::reserve_side_table_storage(
    const base::usize expr_count,
    const base::usize pattern_count,
    const base::usize type_count,
    const base::usize stmt_count,
    const base::usize item_count
) const
{
    const base::usize type_handle_slots = expr_count + expr_count + type_count + stmt_count;
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
    return types.display_name(info.name, generic_args_for_type(types, info.type));
}

std::string enum_display_name(const TypeTable& types, const EnumCaseInfo& info) {
    return types.display_name(info.enum_name, generic_args_for_type(types, info.type));
}

std::string enum_case_display_name(const TypeTable& types, const EnumCaseInfo& info) {
    std::string display = enum_display_name(types, info);
    display += "_";
    display += info.case_name;
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
        return lhs->name < rhs->name;
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
