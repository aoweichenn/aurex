#pragma once

#include <aurex/sema/function.hpp>
#include <aurex/sema/storage.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace aurex::sema {

using CNameIdSet = SemaSet<IdentId, IdentIdHash>;
using SemaTypeTable = SemaVector<TypeHandle>;
using SemaIdentTable = SemaVector<IdentId>;
using SemaIndexTable = SemaVector<base::u32>;

inline constexpr base::usize SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX = static_cast<base::usize>(-1);
inline constexpr base::usize SEMA_GENERIC_SIDE_TABLE_BLOCK_BYTES = 1024U;
inline constexpr base::usize SEMA_PATTERN_CASE_NAME_TABLE_BLOCK_BYTES = 1024U;
inline constexpr base::usize SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX = static_cast<base::usize>(-1);

class PatternCaseNameTable final {
public:
    using Map = SemaMap<base::u32, CNameIdSet>;
    using iterator = Map::iterator;
    using const_iterator = Map::const_iterator;

    PatternCaseNameTable();
    PatternCaseNameTable(const PatternCaseNameTable& other);
    PatternCaseNameTable& operator=(const PatternCaseNameTable& other);
    PatternCaseNameTable(PatternCaseNameTable&& other) noexcept;
    PatternCaseNameTable& operator=(PatternCaseNameTable&& other) noexcept;
    ~PatternCaseNameTable() = default;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] bool contains(base::u32 pattern) const;
    void reserve(base::usize pattern_count);
    void clear() noexcept;

    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] iterator end() noexcept;
    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator end() const noexcept;
    [[nodiscard]] const_iterator find(base::u32 pattern) const;
    [[nodiscard]] iterator find(base::u32 pattern);
    [[nodiscard]] CNameIdSet& operator[](base::u32 pattern);
    void insert(base::u32 pattern, IdentId c_name_id);
    void merge(base::u32 pattern, const CNameIdSet& source);

    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;

private:
    [[nodiscard]] CNameIdSet make_bucket();
    void ensure_storage();
    void swap(PatternCaseNameTable& other) noexcept;
    void copy_from(const PatternCaseNameTable& other);

    std::unique_ptr<base::BumpAllocator> arena_;
    Map names_;
};

struct StructFieldInfo {
    std::string name;
    IdentId name_id = INVALID_IDENT_ID;
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct StructInfo {
    std::string name;
    IdentId name_id = INVALID_IDENT_ID;
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    SemaVector<StructFieldInfo> fields;
    bool is_opaque = false;
    bool is_generic_placeholder = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct EnumCaseInfo {
    std::string name;
    IdentId name_id = INVALID_IDENT_ID;
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    TypeHandle payload_type = INVALID_TYPE_HANDLE;
    TypeHandleList payload_types;
    std::string value_text;
    base::SourceRange range {};
    std::string enum_name;
    std::string case_name;
    IdentId case_name_id = INVALID_IDENT_ID;
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct TypeAliasInfo {
    std::string name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::TypeId target = syntax::INVALID_TYPE_ID;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

using CheckedFunctionMap = SemaMap<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash>;
using CheckedModuleInfoMap = SemaMap<ModuleLookupKey, StructInfo, ModuleLookupKeyHash>;
using CheckedEnumCaseMap = SemaMap<ModuleLookupKey, EnumCaseInfo, ModuleLookupKeyHash>;
using CheckedTypeAliasMap = SemaMap<ModuleLookupKey, TypeAliasInfo, ModuleLookupKeyHash>;

enum class CoercionKind {
    contextual_integer_literal,
    contextual_float_literal,
    null_to_pointer,
    slice_to_expected_slice,
};

struct CoercionRecord {
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    TypeHandle from_type = INVALID_TYPE_HANDLE;
    TypeHandle to_type = INVALID_TYPE_HANDLE;
    CoercionKind kind = CoercionKind::contextual_integer_literal;
};

struct GenericNodeSpan {
    base::u32 begin = 0;
    base::u32 count = 0;

    [[nodiscard]] bool empty() const noexcept {
        return this->count == 0;
    }

    [[nodiscard]] bool contains(const base::u32 value) const noexcept {
        return value >= this->begin && value - this->begin < this->count;
    }

    [[nodiscard]] base::usize local_index(const base::u32 value) const noexcept {
        return this->contains(value)
            ? static_cast<base::usize>(value - this->begin)
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }
};

enum class GenericSparseFallbackKind {
    expr_type,
    expr_expected_type,
    expr_c_name,
    pattern_c_name,
    pattern_case_name,
    syntax_type,
    stmt_local_type,
};

struct GenericSparseFallbackStats {
    base::usize expr_types = 0;
    base::usize expr_expected_types = 0;
    base::usize expr_c_name_ids = 0;
    base::usize pattern_c_name_ids = 0;
    base::usize pattern_case_name_ids = 0;
    base::usize syntax_type_handles = 0;
    base::usize stmt_local_types = 0;

    [[nodiscard]] base::usize total() const noexcept {
        return this->expr_types +
               this->expr_expected_types +
               this->expr_c_name_ids +
               this->pattern_c_name_ids +
               this->pattern_case_name_ids +
               this->syntax_type_handles +
               this->stmt_local_types;
    }

    [[nodiscard]] bool empty() const noexcept {
        return this->total() == 0;
    }
};

struct GenericSideTableLayout {
    GenericNodeSpan expr_span;
    GenericNodeSpan pattern_span;
    GenericNodeSpan type_span;
    GenericNodeSpan stmt_span;
    SemaIndexTable expr_node_ids;
    SemaIndexTable pattern_node_ids;
    SemaIndexTable type_node_ids;
    SemaIndexTable stmt_node_ids;
};

struct GenericSideTables {
private:
    std::unique_ptr<base::BumpAllocator> arena_;
    std::unique_ptr<base::BumpAllocator> analysis_arena_;

public:
    GenericSideTables();
    GenericSideTables(const GenericSideTables& other);
    GenericSideTables& operator=(const GenericSideTables& other);
    GenericSideTables(GenericSideTables&& other) noexcept;
    GenericSideTables& operator=(GenericSideTables&& other) noexcept;
    ~GenericSideTables() = default;

    bool sparse = false;
    bool local_dense = false;
    GenericNodeSpan expr_span;
    GenericNodeSpan pattern_span;
    GenericNodeSpan type_span;
    GenericNodeSpan stmt_span;
    const GenericSideTableLayout* layout = nullptr;
    SemaIndexTable expr_node_ids;
    SemaIndexTable pattern_node_ids;
    SemaIndexTable type_node_ids;
    SemaIndexTable stmt_node_ids;
    SemaTypeTable expr_types;
    SemaTypeTable expr_expected_types;
    SemaIdentTable expr_c_name_ids;
    SemaIdentTable pattern_c_name_ids;
    SemaTypeTable syntax_type_handles;
    SemaTypeTable stmt_local_types;
    SemaMap<base::u32, TypeHandle> sparse_expr_types;
    SemaMap<base::u32, TypeHandle> sparse_expr_expected_types;
    SemaMap<base::u32, IdentId> sparse_expr_c_name_ids;
    SemaMap<base::u32, IdentId> sparse_pattern_c_name_ids;
    PatternCaseNameTable pattern_case_name_ids;
    SemaMap<base::u32, TypeHandle> sparse_syntax_type_handles;
    SemaMap<base::u32, TypeHandle> sparse_stmt_local_types;
    GenericSparseFallbackStats sparse_fallbacks;

    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;
    void record_sparse_fallback(GenericSparseFallbackKind kind) noexcept;
    void configure_local_dense(
        GenericNodeSpan expr,
        GenericNodeSpan pattern,
        GenericNodeSpan type,
        GenericNodeSpan stmt
    );
    void configure_local_dense(
        GenericNodeSpan expr,
        GenericNodeSpan pattern,
        GenericNodeSpan type,
        GenericNodeSpan stmt,
        std::span<const base::u32> expr_ids,
        std::span<const base::u32> pattern_ids,
        std::span<const base::u32> type_ids,
        std::span<const base::u32> stmt_ids
    );
    void configure_local_dense(const GenericSideTableLayout& shared_layout);
    void bind_local_dense_layout(const GenericSideTableLayout& shared_layout) noexcept;
    void prepare_analysis_only_storage(base::usize expr_count);
    void release_analysis_only_storage();

    [[nodiscard]] base::usize local_expr_index(syntax::ExprId expr) const noexcept {
        return syntax::is_valid(expr) && this->local_dense
            ? this->local_index(expr.value, this->active_expr_span(), this->active_expr_node_ids())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

    [[nodiscard]] base::usize local_pattern_index(syntax::PatternId pattern) const noexcept {
        return syntax::is_valid(pattern) && this->local_dense
            ? this->local_index(pattern.value, this->active_pattern_span(), this->active_pattern_node_ids())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

    [[nodiscard]] base::usize local_type_index(syntax::TypeId type) const noexcept {
        return syntax::is_valid(type) && this->local_dense
            ? this->local_index(type.value, this->active_type_span(), this->active_type_node_ids())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

    [[nodiscard]] base::usize local_stmt_index(syntax::StmtId stmt) const noexcept {
        return syntax::is_valid(stmt) && this->local_dense
            ? this->local_index(stmt.value, this->active_stmt_span(), this->active_stmt_node_ids())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

private:
    [[nodiscard]] GenericNodeSpan active_expr_span() const noexcept {
        return this->layout == nullptr ? this->expr_span : this->layout->expr_span;
    }

    [[nodiscard]] GenericNodeSpan active_pattern_span() const noexcept {
        return this->layout == nullptr ? this->pattern_span : this->layout->pattern_span;
    }

    [[nodiscard]] GenericNodeSpan active_type_span() const noexcept {
        return this->layout == nullptr ? this->type_span : this->layout->type_span;
    }

    [[nodiscard]] GenericNodeSpan active_stmt_span() const noexcept {
        return this->layout == nullptr ? this->stmt_span : this->layout->stmt_span;
    }

    [[nodiscard]] const SemaIndexTable& active_expr_node_ids() const noexcept {
        return this->layout == nullptr ? this->expr_node_ids : this->layout->expr_node_ids;
    }

    [[nodiscard]] const SemaIndexTable& active_pattern_node_ids() const noexcept {
        return this->layout == nullptr ? this->pattern_node_ids : this->layout->pattern_node_ids;
    }

    [[nodiscard]] const SemaIndexTable& active_type_node_ids() const noexcept {
        return this->layout == nullptr ? this->type_node_ids : this->layout->type_node_ids;
    }

    [[nodiscard]] const SemaIndexTable& active_stmt_node_ids() const noexcept {
        return this->layout == nullptr ? this->stmt_node_ids : this->layout->stmt_node_ids;
    }

    [[nodiscard]] static base::usize local_index(
        base::u32 value,
        GenericNodeSpan span,
        const SemaIndexTable& ids
    ) noexcept {
        if (ids.empty()) {
            return span.local_index(value);
        }
        const auto found = std::ranges::lower_bound(ids, value);
        return found != ids.end() && *found == value
            ? static_cast<base::usize>(found - ids.begin())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

    void swap(GenericSideTables& other) noexcept;
    void copy_from(const GenericSideTables& other);
};

struct GenericFunctionInstanceInfo {
    FunctionLookupKey key;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    FunctionSignature signature;
    base::usize side_table_layout_index = SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX;
    GenericSideTables side_tables;
};

struct NormalizedAstOverlay {
    // Sema normalizes the caller-owned AST in place. The checked module keeps
    // only normalization bounds/flags, never an owning AST snapshot.
    base::usize original_expr_count = 0;
    base::usize original_type_count = 0;
    base::usize final_expr_count = 0;
    base::usize final_type_count = 0;
    bool parser_only_module_contract_added = false;

    [[nodiscard]] bool added_syntax_nodes() const noexcept {
        return final_expr_count > original_expr_count || final_type_count > original_type_count;
    }
};

struct CheckedModule {
private:
    std::unique_ptr<base::BumpAllocator> arena_;
    std::unique_ptr<base::BumpAllocator> analysis_arena_;

public:
    CheckedModule();
    CheckedModule(const CheckedModule& other);
    CheckedModule& operator=(const CheckedModule& other);
    CheckedModule(CheckedModule&& other) noexcept;
    CheckedModule& operator=(CheckedModule&& other) noexcept;
    ~CheckedModule() = default;

    // CheckedModule is the bridge between syntax and codegen. It is deliberately
    // side-table based so AST nodes remain parse-only data.
    IdentifierInterner c_names;
    TypeTable types;
    SemaTypeTable expr_types;
    SemaTypeTable expr_expected_types;
    SemaIdentTable expr_c_name_ids;
    SemaIdentTable pattern_c_name_ids;
    PatternCaseNameTable pattern_case_name_ids;
    SemaTypeTable syntax_type_handles;
    SemaTypeTable stmt_local_types;
    SemaIdentTable item_c_name_ids;
    SemaVector<CoercionRecord> coercions;
    CheckedFunctionMap functions;
    CheckedModuleInfoMap structs;
    CheckedEnumCaseMap enum_cases;
    CheckedTypeAliasMap type_aliases;
    SemaDeque<GenericSideTableLayout> generic_side_table_layouts;
    SemaDeque<GenericFunctionInstanceInfo> generic_function_instances;
    NormalizedAstOverlay normalized_ast;

    [[nodiscard]] IdentId intern_c_name(const std::string_view c_name) {
        return this->c_names.intern(c_name);
    }

    [[nodiscard]] std::string_view c_name_text(const IdentId id) const noexcept {
        return this->c_names.text(id);
    }

    [[nodiscard]] TypeHandleList make_type_handle_list() const;
    [[nodiscard]] TypeHandleList copy_type_handle_list(std::span<const TypeHandle> values);
    [[nodiscard]] SemaVector<StructFieldInfo> make_struct_field_list() const;
    [[nodiscard]] SemaVector<StructFieldInfo> copy_struct_field_list(std::span<const StructFieldInfo> values);
    [[nodiscard]] SemaIndexTable make_index_table() const;
    [[nodiscard]] SemaIndexTable copy_index_table(std::span<const base::u32> values) const;
    [[nodiscard]] FunctionSignature make_function_signature();
    [[nodiscard]] StructInfo make_struct_info();
    [[nodiscard]] EnumCaseInfo make_enum_case_info();
    [[nodiscard]] GenericSideTableLayout make_generic_side_table_layout(
        GenericNodeSpan expr,
        GenericNodeSpan pattern,
        GenericNodeSpan type,
        GenericNodeSpan stmt,
        std::span<const base::u32> expr_ids,
        std::span<const base::u32> pattern_ids,
        std::span<const base::u32> type_ids,
        std::span<const base::u32> stmt_ids
    ) const;
    [[nodiscard]] base::usize append_generic_side_table_layout(
        GenericNodeSpan expr,
        GenericNodeSpan pattern,
        GenericNodeSpan type,
        GenericNodeSpan stmt,
        std::span<const base::u32> expr_ids,
        std::span<const base::u32> pattern_ids,
        std::span<const base::u32> type_ids,
        std::span<const base::u32> stmt_ids
    );
    [[nodiscard]] const GenericSideTableLayout* generic_side_table_layout(base::usize index) const noexcept;
    [[nodiscard]] FunctionSignature clone_function_signature(const FunctionSignature& other);
    [[nodiscard]] StructInfo clone_struct_info(const StructInfo& other);
    [[nodiscard]] EnumCaseInfo clone_enum_case_info(const EnumCaseInfo& other);
    [[nodiscard]] GenericSideTableLayout clone_generic_side_table_layout(const GenericSideTableLayout& other) const;
    [[nodiscard]] GenericFunctionInstanceInfo clone_generic_function_instance(const GenericFunctionInstanceInfo& other);
    void prepare_analysis_only_storage(base::usize expr_count);
    void release_analysis_only_storage();
    void reserve_side_table_storage(
        base::usize expr_count,
        base::usize pattern_count,
        base::usize type_count,
        base::usize stmt_count,
        base::usize item_count
    ) const;

    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;

private:
    void swap(CheckedModule& other) noexcept;
    void copy_from(const CheckedModule& other);
    void rebind_generic_instance_layouts() noexcept;
};

[[nodiscard]] std::string dump_checked_module(const CheckedModule& checked);
[[nodiscard]] std::string struct_display_name(const TypeTable& types, const StructInfo& info);
[[nodiscard]] std::string enum_display_name(const TypeTable& types, const EnumCaseInfo& info);
[[nodiscard]] std::string enum_case_display_name(const TypeTable& types, const EnumCaseInfo& info);

} // namespace aurex::sema
