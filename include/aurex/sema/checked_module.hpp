#pragma once

#include <aurex/sema/function.hpp>
#include <aurex/sema/storage.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace aurex::sema {

using CNameIdSet = SemaSet<IdentId, IdentIdHash>;
using SemaTypeTable = SemaVector<TypeHandle>;
using SemaIdentTable = SemaVector<IdentId>;

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

struct GenericSideTables {
private:
    std::unique_ptr<base::BumpAllocator> arena_;

public:
    GenericSideTables();
    GenericSideTables(const GenericSideTables& other);
    GenericSideTables& operator=(const GenericSideTables& other);
    GenericSideTables(GenericSideTables&& other) noexcept;
    GenericSideTables& operator=(GenericSideTables&& other) noexcept;
    ~GenericSideTables() = default;

    bool sparse = false;
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

    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;

private:
    void swap(GenericSideTables& other) noexcept;
    void copy_from(const GenericSideTables& other);
};

struct GenericFunctionInstanceInfo {
    std::string key;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    FunctionSignature signature;
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
    SemaMap<std::string, FunctionSignature> functions;
    SemaMap<std::string, StructInfo> structs;
    SemaMap<std::string, EnumCaseInfo> enum_cases;
    SemaMap<std::string, TypeAliasInfo> type_aliases;
    SemaDeque<GenericFunctionInstanceInfo> generic_function_instances;
    NormalizedAstOverlay normalized_ast;

    [[nodiscard]] IdentId intern_c_name(const std::string_view c_name) {
        return this->c_names.intern(c_name);
    }

    [[nodiscard]] std::string_view c_name_text(const IdentId id) const noexcept {
        return this->c_names.text(id);
    }

    [[nodiscard]] TypeHandleList make_type_handle_list();
    [[nodiscard]] TypeHandleList copy_type_handle_list(std::span<const TypeHandle> values);
    [[nodiscard]] SemaVector<StructFieldInfo> make_struct_field_list();
    [[nodiscard]] SemaVector<StructFieldInfo> copy_struct_field_list(std::span<const StructFieldInfo> values);
    [[nodiscard]] FunctionSignature make_function_signature();
    [[nodiscard]] StructInfo make_struct_info();
    [[nodiscard]] EnumCaseInfo make_enum_case_info();
    [[nodiscard]] FunctionSignature clone_function_signature(const FunctionSignature& other);
    [[nodiscard]] StructInfo clone_struct_info(const StructInfo& other);
    [[nodiscard]] EnumCaseInfo clone_enum_case_info(const EnumCaseInfo& other);
    [[nodiscard]] GenericFunctionInstanceInfo clone_generic_function_instance(const GenericFunctionInstanceInfo& other);
    void reserve_side_table_storage(
        base::usize expr_count,
        base::usize pattern_count,
        base::usize type_count,
        base::usize stmt_count,
        base::usize item_count
    );

    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;

private:
    void swap(CheckedModule& other) noexcept;
    void copy_from(const CheckedModule& other);
};

[[nodiscard]] std::string dump_checked_module(const CheckedModule& checked);
[[nodiscard]] std::string struct_display_name(const TypeTable& types, const StructInfo& info);
[[nodiscard]] std::string enum_display_name(const TypeTable& types, const EnumCaseInfo& info);
[[nodiscard]] std::string enum_case_display_name(const TypeTable& types, const EnumCaseInfo& info);

} // namespace aurex::sema
