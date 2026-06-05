#include <aurex/frontend/sema/canonical_type_builder.hpp>
#include <aurex/frontend/sema/drop_glue.hpp>
#include <aurex/frontend/sema/resource_semantics.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <frontend/sema/internal/borrow/private/contract.hpp>
#include <frontend/sema/internal/borrow/private/flow_graph.hpp>
#include <frontend/sema/internal/borrow/private/loan_checker.hpp>
#include <frontend/sema/internal/borrow/private/summary.hpp>
#include <frontend/sema/internal/core/private/sema_core.hpp>
#include <frontend/sema/internal/core/private/sema_side_tables.hpp>
#include <frontend/sema/internal/declarations/private/sema_declaration_analyzer.hpp>
#include <frontend/sema/internal/declarations/private/sema_generic_analyzer.hpp>
#include <frontend/sema/internal/diagnostics/private/sema_diagnostics.hpp>
#include <frontend/sema/internal/dropck/private/dropck_analysis.hpp>
#include <frontend/sema/internal/expressions/private/sema_builtin_expression_analyzer.hpp>
#include <frontend/sema/internal/expressions/private/sema_control_expression_analyzer.hpp>
#include <frontend/sema/internal/expressions/private/sema_expression_analyzer.hpp>
#include <frontend/sema/internal/expressions/private/sema_operator_expression_analyzer.hpp>
#include <frontend/sema/internal/expressions/private/sema_projection_aggregate_expression_analyzer.hpp>
#include <frontend/sema/internal/expressions/private/sema_statement_analyzer.hpp>
#include <frontend/sema/internal/lookup/private/sema_lookup_indexer.hpp>
#include <frontend/sema/internal/lookup/private/sema_lookup_resolver.hpp>
#include <frontend/sema/internal/patterns/private/sema_pattern_match_analyzer.hpp>
#include <frontend/sema/internal/pipeline/private/analysis_pipeline.hpp>
#include <frontend/sema/internal/place/private/move_analysis.hpp>
#include <frontend/sema/internal/services/private/sema_services.hpp>
#include <frontend/sema/internal/services/private/sema_type_services.hpp>

namespace aurex::test {

struct TypeTableTestAccess {
    [[nodiscard]] static sema::SemaVector<sema::TypeInfo>& entries(sema::TypeTable& types) noexcept
    {
        return types.types_;
    }

    [[nodiscard]] static const sema::SemaVector<sema::TypeInfo>& entries(const sema::TypeTable& types) noexcept
    {
        return types.types_;
    }

    [[nodiscard]] static sema::TypeHandle push(sema::TypeTable& types, sema::TypeInfo info)
    {
        return types.push(std::move(info));
    }
};

namespace {

using base::DiagnosticCategory;
using base::DiagnosticCode;
using base::u32;
using sema::BuiltinType;
using sema::EnumCaseInfo;
using sema::FunctionSignature;
using sema::IdentId;
using sema::IdentifierInterner;
using sema::INVALID_TYPE_HANDLE;
using sema::is_valid;
using sema::PointerMutability;
using sema::ResourceCleanupKind;
using sema::ResourceCopyKind;
using sema::ResourceDiscardKind;
using sema::ResourceOwnershipKind;
using sema::ResourceSemanticsClassifier;
using sema::ResourceSemanticsSummary;
using sema::StructFieldInfo;
using sema::StructInfo;
using sema::Symbol;
using sema::SymbolKind;
using sema::TypeHandle;
using sema::TypeKind;
using syntax::ExprId;
using syntax::ModuleId;
using syntax::TypeId;

constexpr u32 SEMA_TEST_ROOT_MODULE_INDEX = 0;
constexpr u32 SEMA_TEST_LIB_ONE_MODULE_INDEX = 1;
constexpr u32 SEMA_TEST_MISSING_MODULE_INDEX = 99;
constexpr base::u32 SEMA_TEST_PATTERN_FIRST_INDEX = 0;
constexpr base::u32 SEMA_TEST_PATTERN_SECOND_INDEX = 1;
constexpr base::u32 SEMA_TEST_PATTERN_TRACKED_COUNT = 3;
constexpr base::u64 SEMA_TEST_ABI_INVALID_SIZE = 0;
constexpr base::u64 SEMA_TEST_ABI_MIN_ALIGNMENT = 1;
constexpr base::u64 SEMA_TEST_RECORD_ABI_SIZE = 16;
constexpr base::u64 SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_SIZE = 1;
constexpr base::u64 SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_ALIGN = 0;
constexpr base::u64 SEMA_TEST_NESTED_ARRAY_COUNT = 5;
constexpr base::u64 SEMA_TEST_INVALID_ARRAY_COUNT = 2;
constexpr base::u64 SEMA_TEST_SMALL_ARRAY_COUNT = 3;
constexpr base::u32 SEMA_TEST_U8_DOMAIN_SIZE = 256;
constexpr base::u64 SEMA_TEST_LARGE_ARRAY_MATCH_COUNT = 4097;
constexpr base::usize SEMA_TEST_SLICE_PATTERN_OVERFLOW_ELEMENT_COUNT = 4097;
constexpr base::usize SEMA_TEST_GENERIC_FIRST_PARAM_INDEX = 0;
constexpr base::usize SEMA_TEST_GENERIC_SECOND_PARAM_INDEX = 1;
constexpr int SEMA_TEST_UNKNOWN_EXPR_KIND_VALUE = 255;
constexpr base::u64 SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT = std::numeric_limits<base::u64>::max();
constexpr base::usize SEMA_TEST_LARGE_GENERIC_SPAN_EXPR_COUNT = 70;
constexpr base::u32 SEMA_TEST_STALE_STRUCT_CACHE_OFFSET = 1;
constexpr base::u32 SEMA_TEST_INVALID_SEMA_TYPE_KIND_VALUE = 99;
constexpr base::u32 SEMA_TEST_INVALID_BUILTIN_TYPE_VALUE = 99;
constexpr base::u32 SEMA_TEST_INVALID_CAPABILITY_KIND_VALUE = 99;
constexpr base::u32 SEMA_TEST_INVALID_RESOURCE_KIND_VALUE = 99;
constexpr base::usize SEMA_TEST_RESOURCE_CLASSIFIER_DEEP_CHAIN_LENGTH = 32;
constexpr base::u8 SEMA_TEST_CONTROL_FLOW_CACHE_UNKNOWN = 0;
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_ONE = "1";
constexpr std::string_view SEMA_TEST_BODY_FLOW_MODULE_NAME = "body_flow";
constexpr std::string_view SEMA_TEST_BODY_FLOW_SOURCE_NAME = "source";
constexpr std::string_view SEMA_TEST_BODY_FLOW_FIELD_NAME = "field";
constexpr std::string_view SEMA_TEST_BODY_FLOW_OTHER_FIELD_NAME = "other";
constexpr std::string_view SEMA_TEST_BODY_FLOW_SINK_NAME = "sink";
constexpr std::string_view SEMA_TEST_BODY_FLOW_BYTES_NAME = "bytes";
constexpr std::string_view SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME = "flow_direct";
constexpr std::string_view SEMA_TEST_BODY_FLOW_INTEGRATED_FUNCTION_NAME = "flow_integrated";
constexpr std::string_view SEMA_TEST_BODY_FLOW_DISCARD_FUNCTION_NAME = "flow_discard";
constexpr std::string_view SEMA_TEST_BODY_FLOW_DISCARD_REF_FUNCTION_NAME = "flow_discard_ref";
constexpr std::string_view SEMA_TEST_BODY_LOAN_MODULE_NAME = "body_loans";
constexpr std::string_view SEMA_TEST_BODY_LOAN_VALUE_NAME = "value";
constexpr std::string_view SEMA_TEST_BODY_LOAN_REF_NAME = "ref_value";
constexpr base::usize SEMA_TEST_BODY_LOAN_RANGE_BEGIN = 10;
constexpr base::usize SEMA_TEST_BODY_LOAN_RANGE_END = 15;
constexpr base::usize SEMA_TEST_BODY_LOAN_RANGE_STRIDE = 10;
constexpr base::u32 SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX = 0;
constexpr base::u32 SEMA_TEST_BORROW_CONTRACT_OTHER_PARAM_INDEX = 1;
constexpr base::u32 SEMA_TEST_BORROW_CONTRACT_UNKNOWN_ORIGIN_INDEX = 0;
constexpr base::u32 SEMA_TEST_BORROW_CONTRACT_LOCAL_ORIGIN_INDEX = 1;
constexpr base::u32 SEMA_TEST_BORROW_CONTRACT_PARAM_ORIGIN_INDEX = 2;
constexpr base::u32 SEMA_TEST_BORROW_CONTRACT_TEMPORARY_ORIGIN_INDEX = 3;
constexpr base::u32 SEMA_TEST_BORROW_CONTRACT_INVALID_ORIGIN_INDEX = 99;
constexpr base::u32 SEMA_TEST_BODY_FLOW_OUT_OF_RANGE_NODE = 4096;
constexpr std::string_view SEMA_TEST_IMPORT_ALIAS_ONE = "one";
constexpr std::string_view SEMA_TEST_CONST_VALUE_NAME = "VALUE";
constexpr std::string_view SEMA_TEST_LOCAL_VALUE_NAME = "LOCAL_VALUE";
constexpr std::string_view SEMA_TEST_ENUM_VALUE_NAME = "ENUM_VALUE";
constexpr std::string_view SEMA_TEST_MISSING_VALUE_NAME = "MISSING_VALUE";
constexpr std::string_view SEMA_TEST_ENUM_CASE_C_NAME = "Choice_some";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_HEX_UPPER = "0X2A";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_HEX_LOWER = "0xa";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_BIN_LOWER = "0b1010";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_BIN_UPPER = "0B1010";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_INVALID_BINARY = "0b2";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_INVALID_CHAR = "12g";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_EMPTY = "";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_UNSIGNED_OVERFLOW = "18446744073709551616";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_SIGNED_OVERFLOW = "9223372036854775808";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_U8_SUFFIX = "255u8";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_I8_SUFFIX = "127i8";
constexpr std::string_view SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX = "128i8";
constexpr std::string_view SEMA_TEST_NEGATIVE_I8_OVERFLOW_SUFFIX = "129i8";
constexpr std::string_view SEMA_TEST_NEGATIVE_I8_MISMATCH_SUFFIX = "1i8";
constexpr std::string_view SEMA_TEST_NEGATIVE_I64_MIN_MAGNITUDE = "9223372036854775808";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_INVALID_SUFFIX = "1f32";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_BAD_SUFFIX = "1bad";
constexpr syntax::PrimitiveTypeKind SEMA_TEST_INVALID_PRIMITIVE_KIND = static_cast<syntax::PrimitiveTypeKind>(99);
constexpr std::string_view SEMA_TEST_FLOAT_OVERFLOW_LITERAL = "1e999999999";
constexpr std::string_view SEMA_TEST_FLOAT_WITH_SEPARATOR_LITERAL = "1_2.5";
constexpr std::string_view SEMA_TEST_FLOAT_INVALID_TRAILING_LITERAL = "1.0x";
constexpr std::string_view SEMA_TEST_FLOAT_LITERAL_F32_SUFFIX = ".5f32";
constexpr std::string_view SEMA_TEST_FLOAT_LITERAL_MISMATCH_F32_SUFFIX = "1.0f32";
constexpr std::string_view SEMA_TEST_FLOAT_LITERAL_INVALID_SUFFIX = "1.0u8";
constexpr base::u64 SEMA_TEST_ARRAY_SLICE_LENGTH = 3;
constexpr std::string_view SEMA_TEST_ARRAY_SLICE_OUT_OF_BOUNDS = "4";
constexpr std::string_view SEMA_TEST_ARRAY_SLICE_NEGATIVE_OUT_OF_BOUNDS = "1";
constexpr std::string_view SEMA_TEST_GENERIC_PARAM_NAME = "T";
constexpr syntax::BinaryOp SEMA_TEST_INVALID_BINARY_OP = static_cast<syntax::BinaryOp>(99);
constexpr sema::BuiltinType SEMA_TEST_INVALID_BUILTIN_TYPE = static_cast<sema::BuiltinType>(99);
constexpr sema::TypeKind SEMA_TEST_INVALID_TYPE_KIND = static_cast<sema::TypeKind>(99);
constexpr sema::PointerMutability SEMA_TEST_INVALID_POINTER_MUTABILITY = static_cast<sema::PointerMutability>(99);
constexpr sema::FunctionCallConv SEMA_TEST_INVALID_FUNCTION_CALL_CONV = static_cast<sema::FunctionCallConv>(99);
constexpr std::string_view SEMA_TEST_INVALID_TYPE_DISPLAY = "<invalid>";
constexpr std::string_view SEMA_TEST_UNKNOWN_TYPE_DISPLAY = "<unknown>";
constexpr std::string_view SEMA_TEST_SYMBOL_OUTER_NAME = "outer_value";
constexpr std::string_view SEMA_TEST_SYMBOL_INNER_NAME = "inner_value";
constexpr std::string_view SEMA_TEST_SYMBOL_DUPLICATE_NAME = "duplicate_value";
constexpr std::string_view SEMA_TEST_ROOT_MODULE_NAME = "core";
constexpr std::string_view SEMA_TEST_CHILD_MODULE_NAME = "mem";
constexpr std::string_view SEMA_TEST_LEAF_MODULE_NAME = "io";
constexpr std::string_view SEMA_TEST_ORIGIN_FACT_MODULE_NAME = "origin_facts";
constexpr std::string_view SEMA_TEST_ORIGIN_FACT_FUNCTION_NAME = "view";
constexpr std::string_view SEMA_TEST_ORIGIN_FACT_PARAM_NAME = "data";
constexpr std::string_view SEMA_TEST_ORIGIN_FACT_TYPE_DISPLAY = "&[data] i32";
constexpr base::u32 SEMA_TEST_ORIGIN_FACT_PART_INDEX = 0;

[[nodiscard]] ModuleId module_id(const u32 value) noexcept
{
    return ModuleId{value};
}

[[nodiscard]] syntax::ModulePath module_path(const std::initializer_list<std::string_view> parts)
{
    syntax::ModulePath path;
    path.parts.assign(parts.begin(), parts.end());
    return path;
}

[[nodiscard]] syntax::ModuleInfo module_info(const std::initializer_list<std::string_view> parts)
{
    syntax::ModuleInfo info;
    info.path = module_path(parts);
    return info;
}

[[nodiscard]] query::ModuleKey query_test_module_key()
{
    const std::array<std::string_view, 2> package_parts{"test", "workspace"};
    const std::array<std::string_view, 1> module_parts{"types"};
    return query::module_key(query::package_key(package_parts), module_parts);
}

[[nodiscard]] query::DefKey query_test_def_key(
    const query::ModuleKey module, const query::DefKind kind, const std::string_view name)
{
    const std::array<std::string_view, 1> path{name};
    return query::def_key(module, query::DefNamespace::type, kind, path);
}

class CanonicalTypeBuilderTestResolver final : public sema::CanonicalTypeKeyResolver {
public:
    void bind_nominal(const TypeHandle handle, const query::DefKey key)
    {
        this->nominal_keys_.emplace(handle.value, key);
    }

    void bind_generic(const sema::GenericParamIdentity identity, const query::GenericParamKey key)
    {
        this->generic_param_keys_.emplace(identity.value, key);
    }

    [[nodiscard]] std::optional<query::DefKey> nominal_type_key(
        const TypeHandle handle, const sema::TypeInfo& info) const override
    {
        static_cast<void>(info);
        const auto found = this->nominal_keys_.find(handle.value);
        if (found == this->nominal_keys_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] std::optional<query::GenericParamKey> generic_param_key(
        const TypeHandle handle, const sema::TypeInfo& info) const override
    {
        static_cast<void>(handle);
        const auto found = this->generic_param_keys_.find(info.generic_identity.value);
        if (found == this->generic_param_keys_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

private:
    std::unordered_map<base::u32, query::DefKey> nominal_keys_;
    std::unordered_map<base::u64, query::GenericParamKey> generic_param_keys_;
};

[[nodiscard]] syntax::ResolvedImport resolved_import(const ModuleId module, const std::string_view alias,
    const syntax::Visibility visibility = syntax::Visibility::private_)
{
    syntax::ResolvedImport import;
    import.module = module;
    import.alias = alias;
    import.visibility = visibility;
    return import;
}

[[nodiscard]] syntax::ResolvedUse resolved_use(const ModuleId module, const std::string_view target_name,
    const std::string_view alias, const syntax::Visibility visibility, const IdentId target_name_id,
    const IdentId alias_id)
{
    syntax::ResolvedUse use;
    use.module = module;
    use.target_name = target_name;
    use.alias = alias;
    use.visibility = visibility;
    use.target_name_id = target_name_id;
    use.alias_id = alias_id;
    return use;
}

[[nodiscard]] syntax::TypeNode primitive_node(const syntax::PrimitiveTypeKind kind)
{
    syntax::TypeNode node;
    node.kind = syntax::TypeKind::primitive;
    node.primitive = kind;
    return node;
}

[[nodiscard]] syntax::TypeNode named_node(const std::string_view name)
{
    syntax::TypeNode node;
    node.kind = syntax::TypeKind::named;
    node.name = name;
    return node;
}

[[nodiscard]] syntax::TypeNode reference_node(
    const TypeId pointee, const syntax::PointerMutability mutability = syntax::PointerMutability::const_)
{
    syntax::TypeNode node;
    node.kind = syntax::TypeKind::reference;
    node.pointee = pointee;
    node.pointer_mutability = mutability;
    return node;
}

[[nodiscard]] syntax::TypeNode pointer_node(
    const TypeId pointee, const syntax::PointerMutability mutability = syntax::PointerMutability::const_)
{
    syntax::TypeNode node;
    node.kind = syntax::TypeKind::pointer;
    node.pointee = pointee;
    node.pointer_mutability = mutability;
    return node;
}

[[nodiscard]] ExprId push_integer(syntax::AstModule& module);
[[nodiscard]] syntax::StmtId push_block(syntax::AstModule& module, std::initializer_list<syntax::StmtId> statements);

enum class GenericAggregateSignatureVariant : base::u8 {
    generic_param,
    concrete_i64,
};

struct GenericAggregateSignatureSnapshot {
    query::GenericInstanceKey struct_key;
    sema::IncrementalKey struct_signature;
    query::GenericInstanceKey enum_key;
    sema::IncrementalKey enum_signature;
};

[[nodiscard]] TypeId generic_aggregate_variant_type(
    const GenericAggregateSignatureVariant variant, const TypeId generic_type, const TypeId i64_type) noexcept
{
    switch (variant) {
        case GenericAggregateSignatureVariant::generic_param:
            return generic_type;
        case GenericAggregateSignatureVariant::concrete_i64:
            return i64_type;
    }
    return generic_type;
}

[[nodiscard]] std::optional<GenericAggregateSignatureSnapshot> generic_aggregate_signature_snapshot(
    const GenericAggregateSignatureVariant struct_field_variant,
    const GenericAggregateSignatureVariant enum_payload_variant)
{
    syntax::AstModule module;
    module.modules = {module_info({"generic_signature_sensitivity"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId i64_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i64));
    const TypeId struct_field_type = generic_aggregate_variant_type(struct_field_variant, generic_type, i64_type);
    const TypeId enum_payload_type = generic_aggregate_variant_type(enum_payload_variant, generic_type, i64_type);

    syntax::TypeNode box_i32_type_node = named_node("Box");
    box_i32_type_node.type_args = {i32_type};
    const TypeId box_i32_type = module.push_type(box_i32_type_node);

    syntax::TypeNode maybe_i32_type_node = named_node("Maybe");
    maybe_i32_type_node.type_args = {i32_type};
    const TypeId maybe_i32_type = module.push_type(maybe_i32_type_node);

    syntax::ItemNode box_item;
    box_item.kind = syntax::ItemKind::struct_decl;
    box_item.name = "Box";
    box_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    box_item.fields = {syntax::FieldDecl{"value", struct_field_type, {}}};
    const syntax::ItemId box_item_id = module.push_item(box_item);
    module.item_modules[box_item_id.value] = module_id(0);

    syntax::EnumCaseDecl some_case;
    some_case.name = "some";
    some_case.payload_types = {enum_payload_type};
    syntax::EnumCaseDecl none_case;
    none_case.name = "none";
    syntax::ItemNode maybe_item;
    maybe_item.kind = syntax::ItemKind::enum_decl;
    maybe_item.name = "Maybe";
    maybe_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    maybe_item.enum_cases = {some_case, none_case};
    const syntax::ItemId maybe_item_id = module.push_item(maybe_item);
    module.item_modules[maybe_item_id.value] = module_id(0);

    const ExprId zero = push_integer(module);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode use_item;
    use_item.kind = syntax::ItemKind::fn_decl;
    use_item.name = "use";
    use_item.params = {
        syntax::ParamDecl{"box", box_i32_type, {}},
        syntax::ParamDecl{"maybe", maybe_i32_type, {}},
    };
    use_item.return_type = i32_type;
    use_item.body = body;
    const syntax::ItemId use_item_id = module.push_item(use_item);
    module.item_modules[use_item_id.value] = module_id(0);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    if (!checked_result) {
        ADD_FAILURE() << checked_result.error().message;
        return std::nullopt;
    }
    const sema::CheckedModule& checked = checked_result.value();

    const sema::StructInfo* generic_box = nullptr;
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        if (info.name == "Box" && query::is_valid(info.generic_instance_key)) {
            generic_box = &info;
            break;
        }
    }
    if (generic_box == nullptr || checked.generic_enum_instances.empty()) {
        ADD_FAILURE() << "missing generic aggregate instance";
        return std::nullopt;
    }

    const sema::GenericEnumInstanceInfo& generic_maybe = checked.generic_enum_instances.front();
    return GenericAggregateSignatureSnapshot{
        generic_box->generic_instance_key,
        generic_box->incremental_key,
        generic_maybe.generic_instance_key,
        generic_maybe.incremental_key,
    };
}

[[nodiscard]] FunctionSignature function_signature(const std::string_view name, const ModuleId module,
    const TypeHandle return_type, const IdentId name_id, sema::CheckedModule& checked)
{
    FunctionSignature signature;
    signature.name = checked.intern_text(name);
    signature.name_id = name_id;
    signature.c_name = signature.name;
    signature.module = module;
    signature.return_type = return_type;
    return signature;
}

[[nodiscard]] Symbol symbol(const SymbolKind kind, const std::string_view name, const ModuleId module,
    const TypeHandle type, const bool is_mutable = false,
    const syntax::Visibility visibility = syntax::Visibility::public_, const IdentId name_id = syntax::INVALID_IDENT_ID,
    sema::CheckedModule* checked = nullptr)
{
    const sema::InternedText interned_name = checked == nullptr ? sema::InternedText{} : checked->intern_text(name);
    return Symbol{
        kind,
        interned_name,
        name_id,
        interned_name,
        module,
        type,
        {},
        is_mutable,
        visibility,
        {},
    };
}

[[nodiscard]] IdentId intern_identifier(sema::SemanticAnalyzerCore& analyzer, const std::string_view name)
{
    return analyzer.ctx_.module.intern_identifier(name);
}

[[nodiscard]] sema::InternedText checked_text(sema::CheckedModule& checked, const std::string_view text)
{
    return checked.intern_text(text);
}

[[nodiscard]] StructFieldInfo struct_field_info(sema::SemanticAnalyzerCore& analyzer, const std::string_view name,
    const ModuleId module, const TypeHandle type, const syntax::Visibility visibility = syntax::Visibility::public_)
{
    return StructFieldInfo{
        checked_text(analyzer.state_.checked, name),
        intern_identifier(analyzer, name),
        checked_text(analyzer.state_.checked, name),
        module,
        type,
        {},
        visibility,
        {},
    };
}

[[nodiscard]] sema::ModuleLookupKey semantic_module_key(
    sema::SemanticAnalyzerCore& analyzer, const ModuleId module, const std::string_view name)
{
    return analyzer.module_lookup_key(module, intern_identifier(analyzer, name));
}

[[nodiscard]] sema::FunctionLookupKey semantic_function_key(
    sema::SemanticAnalyzerCore& analyzer, const ModuleId module, const std::string_view name)
{
    return analyzer.function_lookup_key(module, intern_identifier(analyzer, name));
}

[[nodiscard]] base::SourceRange body_loan_test_range(const base::usize index) noexcept
{
    const base::usize offset = index * SEMA_TEST_BODY_LOAN_RANGE_STRIDE;
    return base::SourceRange{
        .source = base::SourceId{SEMA_TEST_ROOT_MODULE_INDEX},
        .begin = SEMA_TEST_BODY_LOAN_RANGE_BEGIN + offset,
        .end = SEMA_TEST_BODY_LOAN_RANGE_END + offset,
    };
}

[[nodiscard]] sema::BodyFlowPlace body_loan_local_place(
    const IdentId name_id, const ExprId expr = syntax::INVALID_EXPR_ID, const base::usize range_index = 0)
{
    return sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = name_id,
        .root_expr = expr,
        .projections = {},
        .range = body_loan_test_range(range_index),
    };
}

[[nodiscard]] sema::BodyFlowPlace body_loan_deref_place(
    const IdentId name_id, const ExprId root_expr, const ExprId deref_expr, const base::usize range_index = 0)
{
    return sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = name_id,
        .root_expr = root_expr,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::dereference,
            .expr = deref_expr,
        }},
        .range = body_loan_test_range(range_index),
    };
}

[[nodiscard]] sema::BodyFlowPlace body_loan_field_place(
    const IdentId name_id, const IdentId field_name_id, const base::usize range_index = 0)
{
    return sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = name_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::field,
            .field_name_id = field_name_id,
        }},
        .range = body_loan_test_range(range_index),
    };
}

[[nodiscard]] sema::FunctionLookupKey semantic_method_key(sema::SemanticAnalyzerCore& analyzer, const ModuleId module,
    const TypeHandle owner_type, const std::string_view name)
{
    return analyzer.method_function_lookup_key(module, owner_type, intern_identifier(analyzer, name));
}

[[nodiscard]] FunctionSignature indexed_function_signature(sema::SemanticAnalyzerCore& analyzer,
    const std::string_view name, const ModuleId module, const TypeHandle return_type)
{
    return function_signature(name, module, return_type, intern_identifier(analyzer, name), analyzer.state_.checked);
}

[[nodiscard]] Symbol indexed_symbol(sema::SemanticAnalyzerCore& analyzer, const SymbolKind kind,
    const std::string_view name, const ModuleId module, const TypeHandle type, const bool is_mutable = false,
    const syntax::Visibility visibility = syntax::Visibility::public_)
{
    return symbol(
        kind, name, module, type, is_mutable, visibility, intern_identifier(analyzer, name), &analyzer.state_.checked);
}

[[nodiscard]] sema::ModuleLookupKey add_named_type(sema::SemanticAnalyzerCore& analyzer, const ModuleId module,
    const std::string_view name, const TypeHandle type,
    const syntax::Visibility visibility = syntax::Visibility::public_)
{
    const IdentId name_id = intern_identifier(analyzer, name);
    const sema::ModuleLookupKey key = analyzer.module_lookup_key(module, name_id);
    analyzer.state_.types.named_types.emplace(key, type);
    analyzer.index_named_type(module, name_id, type, visibility);
    return key;
}

[[nodiscard]] const StructInfo& add_struct_info(sema::SemanticAnalyzerCore& analyzer, const ModuleId module,
    const std::string_view name, const TypeHandle type,
    const syntax::Visibility visibility = syntax::Visibility::public_)
{
    const IdentId name_id = intern_identifier(analyzer, name);
    const sema::ModuleLookupKey key = analyzer.module_lookup_key(module, name_id);
    StructInfo info = analyzer.state_.checked.make_struct_info();
    info.name = analyzer.state_.checked.intern_text(name);
    info.name_id = name_id;
    info.c_name = analyzer.state_.checked.intern_text(name);
    info.module = module;
    info.type = type;
    info.visibility = visibility;
    info.stable_id = analyzer.stable_definition_id(module, sema::StableSymbolKind::type, name_id, name);
    info.incremental_key = analyzer.stable_incremental_key(info.stable_id, name);
    const auto inserted = analyzer.state_.checked.structs.emplace(key, std::move(info));
    analyzer.state_.types.struct_infos_by_type[type.value] = &inserted.first->second;
    static_cast<void>(add_named_type(analyzer, module, name, type, visibility));
    return inserted.first->second;
}

[[nodiscard]] sema::FunctionLookupKey add_function(sema::SemanticAnalyzerCore& analyzer, FunctionSignature signature)
{
    if (!is_valid(signature.name_id)) {
        signature.name_id = intern_identifier(analyzer, signature.name);
    }
    const sema::FunctionLookupKey key = analyzer.function_lookup_key(signature.module, signature.name_id);
    signature.semantic_key = key;
    const auto inserted = analyzer.state_.checked.functions.emplace(key, std::move(signature));
    analyzer.index_function_lookup(inserted.first->second);
    return inserted.first->first;
}

[[nodiscard]] sema::FunctionLookupKey add_method(
    sema::SemanticAnalyzerCore& analyzer, FunctionSignature signature, const TypeHandle owner_type)
{
    if (!is_valid(signature.name_id)) {
        signature.name_id = intern_identifier(analyzer, signature.name);
    }
    signature.is_method = true;
    signature.method_owner_type = owner_type;
    const sema::FunctionLookupKey key =
        analyzer.method_function_lookup_key(signature.module, owner_type, signature.name_id);
    signature.semantic_key = key;
    const auto inserted = analyzer.state_.checked.functions.emplace(key, std::move(signature));
    analyzer.index_function_lookup(inserted.first->second);
    return inserted.first->first;
}

[[nodiscard]] std::pair<sema::FunctionLookupKey, const Symbol*> add_global_value(sema::SemanticAnalyzerCore& analyzer,
    const ModuleId module, const std::string_view name, const TypeHandle type,
    const SymbolKind kind = SymbolKind::const_, const bool is_mutable = false,
    const syntax::Visibility visibility = syntax::Visibility::public_)
{
    const IdentId name_id = intern_identifier(analyzer, name);
    const sema::FunctionLookupKey key = analyzer.function_lookup_key(module, name_id);
    auto inserted = analyzer.state_.functions.global_values.emplace(
        key, symbol(kind, name, module, type, is_mutable, visibility, name_id, &analyzer.state_.checked));
    analyzer.index_global_value(inserted.first->second);
    return {inserted.first->first, &inserted.first->second};
}

[[nodiscard]] sema::SemanticAnalyzerCore::GenericTemplateInfo generic_template_info(
    sema::SemanticAnalyzerCore& analyzer, const ModuleId module, const std::string_view name,
    const syntax::Visibility visibility = syntax::Visibility::public_)
{
    sema::SemanticAnalyzerCore::GenericTemplateInfo info = analyzer.make_generic_template_info();
    info.module = module;
    info.name = analyzer.state_.checked.intern_text(name);
    info.name_id = intern_identifier(analyzer, name);
    info.key = analyzer.module_lookup_key(module, info.name_id);
    info.function_key = analyzer.function_lookup_key(module, info.name_id);
    info.params = {intern_identifier(analyzer, "T")};
    info.visibility = visibility;
    return info;
}

[[nodiscard]] std::pair<sema::ModuleLookupKey, const EnumCaseInfo*> add_enum_case(sema::SemanticAnalyzerCore& analyzer,
    const ModuleId module, const std::string_view name, const std::string_view case_name, const TypeHandle enum_type,
    const TypeHandle payload_type = INVALID_TYPE_HANDLE, std::vector<TypeHandle> payload_types = {})
{
    EnumCaseInfo info = analyzer.state_.checked.make_enum_case_info();
    info.name = analyzer.state_.checked.intern_text(name);
    info.name_id = intern_identifier(analyzer, name);
    info.c_name = analyzer.state_.checked.intern_text(name);
    info.module = module;
    info.type = enum_type;
    info.payload_type = payload_type;
    info.payload_types = analyzer.state_.checked.copy_type_handle_list(payload_types);
    info.case_name = analyzer.state_.checked.intern_text(case_name);
    info.case_name_id = intern_identifier(analyzer, case_name);

    const sema::ModuleLookupKey key = analyzer.module_lookup_key(module, info.name_id);
    auto inserted = analyzer.state_.checked.enum_cases.emplace(key, std::move(info));
    analyzer.index_enum_case(inserted.first->second);
    return {inserted.first->first, &inserted.first->second};
}

[[nodiscard]] ExprId push_name(
    syntax::AstModule& module, const std::string_view text, const std::string_view scope = {})
{
    syntax::NameExprPayload payload;
    payload.text = text;
    payload.scope_name = scope;
    return module.push_name_expr({}, std::move(payload));
}

[[nodiscard]] ExprId push_field(syntax::AstModule& module, const ExprId object, const std::string_view field_name)
{
    syntax::FieldExprPayload payload;
    payload.object = object;
    payload.field_name = field_name;
    return module.push_field_expr({}, payload);
}

[[nodiscard]] ExprId push_generic_apply(
    syntax::AstModule& module, const ExprId callee, const std::initializer_list<TypeId> type_args)
{
    syntax::GenericApplyExprPayload payload;
    payload.callee = callee;
    payload.type_args.assign(type_args.begin(), type_args.end());
    return module.push_generic_apply_expr({}, std::move(payload));
}

[[nodiscard]] ExprId push_unary(syntax::AstModule& module, const syntax::UnaryOp op, const ExprId operand)
{
    return module.push_unary_expr(syntax::ExprKind::unary, {},
        syntax::UnaryExprPayload{
            op,
            operand,
        });
}

[[nodiscard]] ExprId push_binary(
    syntax::AstModule& module, const syntax::BinaryOp op, const ExprId lhs, const ExprId rhs)
{
    return module.push_binary_expr({},
        syntax::BinaryExprPayload{
            op,
            lhs,
            rhs,
        });
}

[[nodiscard]] ExprId push_call(
    syntax::AstModule& module, const ExprId callee, const std::initializer_list<ExprId> args = {})
{
    syntax::CallExprPayload payload;
    payload.callee = callee;
    payload.args.assign(args.begin(), args.end());
    return module.push_call_expr(syntax::ExprKind::call, {}, std::move(payload));
}

[[nodiscard]] ExprId push_integer(syntax::AstModule& module)
{
    return module.push_literal_expr(syntax::ExprKind::integer_literal, {}, SEMA_TEST_INTEGER_LITERAL_ONE);
}

[[nodiscard]] ExprId push_integer_text(syntax::AstModule& module, const std::string_view text)
{
    return module.push_literal_expr(syntax::ExprKind::integer_literal, {}, text);
}

[[nodiscard]] ExprId push_bool(syntax::AstModule& module, const std::string_view text)
{
    return module.push_literal_expr(syntax::ExprKind::bool_literal, {}, text);
}

void prepare_expr_storage(sema::SemanticAnalyzerCore& analyzer, const syntax::AstModule& module)
{
    analyzer.state_.checked.expr_intrinsic_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.prepare_analysis_only_storage(module.exprs.size());
    analyzer.state_.checked.expr_expected_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
}

[[nodiscard]] std::string diagnostic_messages(const base::DiagnosticSink& diagnostics)
{
    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages.push_back('\n');
    }
    return messages;
}

[[nodiscard]] syntax::StmtId push_stmt(syntax::AstModule& module, const syntax::StmtKind kind)
{
    syntax::StmtNode stmt;
    stmt.kind = kind;
    return module.push_stmt(stmt);
}

[[nodiscard]] syntax::StmtId push_block(
    syntax::AstModule& module, const std::initializer_list<syntax::StmtId> statements)
{
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.statements.assign(statements.begin(), statements.end());
    return module.push_stmt(block);
}

[[nodiscard]] syntax::StmtId push_local_stmt(syntax::AstModule& module, const syntax::StmtKind kind,
    const std::string_view name, const TypeId type, const ExprId init = syntax::INVALID_EXPR_ID)
{
    syntax::StmtNode stmt;
    stmt.kind = kind;
    stmt.name = name;
    stmt.name_id = module.intern_identifier(name);
    stmt.declared_type = type;
    stmt.init = init;
    return module.push_stmt(stmt);
}

[[nodiscard]] syntax::StmtId push_assign_stmt(syntax::AstModule& module, const ExprId lhs, const ExprId rhs)
{
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::assign;
    stmt.lhs = lhs;
    stmt.rhs = rhs;
    return module.push_stmt(stmt);
}

[[nodiscard]] syntax::StmtId push_return_stmt(syntax::AstModule& module, const ExprId value)
{
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::return_;
    stmt.return_value = value;
    return module.push_stmt(stmt);
}

[[nodiscard]] syntax::ParamDecl push_param_decl(syntax::AstModule& module, const std::string_view name)
{
    syntax::ParamDecl param;
    param.name = name;
    param.name_id = module.intern_identifier(name);
    return param;
}

} // namespace

TEST(CoreUnit, SemanticWhiteBoxFacadeDelegatesBorrowedAndOwnedModules)
{
    static_assert(std::is_final_v<sema::SemanticAnalysisPipeline>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalysisPipeline>);
    static_assert(std::is_constructible_v<sema::SemanticAnalysisPipeline, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticDiagnosticReporter>);
    static_assert(!std::is_default_constructible_v<sema::SemanticDiagnosticReporter>);
    static_assert(
        std::is_constructible_v<sema::SemanticDiagnosticReporter, base::DiagnosticSink&, const sema::TypeTable&>);
    static_assert(std::is_final_v<sema::SemanticSideTableReader>);
    static_assert(!std::is_default_constructible_v<sema::SemanticSideTableReader>);
    static_assert(std::is_constructible_v<sema::SemanticSideTableReader, const sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticSideTableStore>);
    static_assert(!std::is_default_constructible_v<sema::SemanticSideTableStore>);
    static_assert(std::is_constructible_v<sema::SemanticSideTableStore, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::BuiltinExpressionAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::BuiltinExpressionAnalyzer>);
    static_assert(
        std::is_constructible_v<sema::SemanticAnalyzerCore::BuiltinExpressionAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::BodyFlowAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::BodyFlowAnalyzer>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::BodyFlowAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::BodyLoanChecker>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::BodyLoanChecker>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::BodyLoanChecker, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::DropCheckAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::DropCheckAnalyzer>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::DropCheckAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::BodyMoveAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::BodyMoveAnalyzer>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::BodyMoveAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::ControlExpressionAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::ControlExpressionAnalyzer>);
    static_assert(
        std::is_constructible_v<sema::SemanticAnalyzerCore::ControlExpressionAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::DeclarationAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::DeclarationAnalyzer>);
    static_assert(
        std::is_constructible_v<sema::SemanticAnalyzerCore::DeclarationAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::ExpressionAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::ExpressionAnalyzer>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::ExpressionAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::GenericAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::GenericAnalyzer>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::GenericAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::OperatorExpressionAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::OperatorExpressionAnalyzer>);
    static_assert(
        std::is_constructible_v<sema::SemanticAnalyzerCore::OperatorExpressionAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::PatternMatchAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::PatternMatchAnalyzer>);
    static_assert(
        std::is_constructible_v<sema::SemanticAnalyzerCore::PatternMatchAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer,
        sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::LookupResolver>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::LookupResolver>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::LookupResolver, sema::SemanticAnalyzerCore&>);
    static_assert(
        std::is_constructible_v<sema::SemanticAnalyzerCore::LookupResolver, const sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::LookupIndexer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::LookupIndexer>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::LookupIndexer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAnalyzerCore::StatementAnalyzer>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAnalyzerCore::StatementAnalyzer>);
    static_assert(std::is_constructible_v<sema::SemanticAnalyzerCore::StatementAnalyzer, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticTypeResolver>);
    static_assert(!std::is_default_constructible_v<sema::SemanticTypeResolver>);
    static_assert(std::is_constructible_v<sema::SemanticTypeResolver, sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticTypeValidator>);
    static_assert(!std::is_default_constructible_v<sema::SemanticTypeValidator>);
    static_assert(std::is_constructible_v<sema::SemanticTypeValidator, const sema::SemanticAnalyzerCore&>);
    static_assert(std::is_final_v<sema::SemanticAbiChecker>);
    static_assert(!std::is_default_constructible_v<sema::SemanticAbiChecker>);
    static_assert(std::is_constructible_v<sema::SemanticAbiChecker, const sema::SemanticAnalyzerCore&>);

    {
        syntax::AstModule borrowed_module;
        borrowed_module.module_path = module_path({"facade", "borrowed"});
        base::DiagnosticSink diagnostics;

        sema::SemanticAnalyzer analyzer(borrowed_module, diagnostics);
        auto checked_result = analyzer.analyze();

        ASSERT_TRUE(checked_result) << checked_result.error().message;
        EXPECT_TRUE(diagnostics.diagnostics().empty());
        EXPECT_EQ(borrowed_module.modules.size(), 1U);
        EXPECT_TRUE(checked_result.value().normalized_ast.parser_only_module_contract_added);
    }

    {
        syntax::AstModule owned_module;
        owned_module.module_path = module_path({"facade", "owned"});
        base::DiagnosticSink diagnostics;

        sema::SemanticAnalyzer analyzer(std::move(owned_module), diagnostics);
        auto checked_result = analyzer.analyze();

        ASSERT_TRUE(checked_result) << checked_result.error().message;
        EXPECT_TRUE(diagnostics.diagnostics().empty());
        EXPECT_TRUE(checked_result.value().normalized_ast.parser_only_module_contract_added);
    }
}

TEST(CoreUnit, SemanticWhiteBoxDiagnosticMetadataUsesExplicitKinds)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const base::SourceRange range{{1}, 0, 1};

    analyzer.report_pattern_exhaustiveness(range, "same diagnostic text");
    analyzer.report_unsafe_required(range, "same diagnostic text");
    analyzer.report_capability(range, "same diagnostic text");
    analyzer.report_unsupported(range, "same diagnostic text");
    analyzer.report_internal_contract(range, "same diagnostic text");
    analyzer.report_type_mismatch(range, std::string(sema::SEMA_INITIALIZER_TYPE_MISMATCH),
        analyzer.state_.checked.types.builtin(BuiltinType::bool_),
        analyzer.state_.checked.types.builtin(BuiltinType::i32));
    analyzer.report_note(
        range, sema::SemanticDiagnosticKind::duplicate, sema::sema_previous_declaration_note_message("value"));
    analyzer.report_lookup_suggestion(range, "value");
    analyzer.report(range, sema::SemanticDiagnosticKind::general, "direct semantic kind");
    analyzer.report(range, "direct module diagnostic", DiagnosticCategory::module, DiagnosticCode::module_error);
    analyzer.report_help(range, sema::SemanticDiagnosticKind::lookup, "direct lookup help");

    ASSERT_EQ(diagnostics.diagnostics().size(), 13U);
    EXPECT_EQ(diagnostics.diagnostics()[0].category, DiagnosticCategory::pattern);
    EXPECT_EQ(diagnostics.diagnostics()[0].code, DiagnosticCode::semantic_pattern_exhaustiveness);
    EXPECT_EQ(diagnostics.diagnostics()[1].category, DiagnosticCategory::safety);
    EXPECT_EQ(diagnostics.diagnostics()[1].code, DiagnosticCode::semantic_unsafe_required);
    EXPECT_EQ(diagnostics.diagnostics()[2].category, DiagnosticCategory::capability);
    EXPECT_EQ(diagnostics.diagnostics()[2].code, DiagnosticCode::semantic_capability);
    EXPECT_EQ(diagnostics.diagnostics()[3].category, DiagnosticCategory::unsupported);
    EXPECT_EQ(diagnostics.diagnostics()[3].code, DiagnosticCode::semantic_unsupported);
    EXPECT_EQ(diagnostics.diagnostics()[4].category, DiagnosticCategory::internal);
    EXPECT_EQ(diagnostics.diagnostics()[4].code, DiagnosticCode::internal_contract);
    EXPECT_EQ(diagnostics.diagnostics()[5].category, DiagnosticCategory::type);
    EXPECT_EQ(diagnostics.diagnostics()[5].code, DiagnosticCode::semantic_type_mismatch);
    EXPECT_EQ(diagnostics.diagnostics()[6].category, DiagnosticCategory::type);
    EXPECT_EQ(diagnostics.diagnostics()[6].code, DiagnosticCode::semantic_type_mismatch);
    EXPECT_EQ(diagnostics.diagnostics()[7].category, DiagnosticCategory::type);
    EXPECT_EQ(diagnostics.diagnostics()[7].code, DiagnosticCode::semantic_type_mismatch);
    EXPECT_EQ(diagnostics.diagnostics()[8].category, DiagnosticCategory::name_resolution);
    EXPECT_EQ(diagnostics.diagnostics()[8].code, DiagnosticCode::semantic_duplicate);
    EXPECT_EQ(diagnostics.diagnostics()[9].category, DiagnosticCategory::name_resolution);
    EXPECT_EQ(diagnostics.diagnostics()[9].code, DiagnosticCode::semantic_lookup);
    EXPECT_EQ(diagnostics.diagnostics()[10].category, DiagnosticCategory::semantic);
    EXPECT_EQ(diagnostics.diagnostics()[10].code, DiagnosticCode::semantic_error);
    EXPECT_EQ(diagnostics.diagnostics()[11].category, DiagnosticCategory::module);
    EXPECT_EQ(diagnostics.diagnostics()[11].code, DiagnosticCode::module_error);
    EXPECT_EQ(diagnostics.diagnostics()[12].category, DiagnosticCategory::name_resolution);
    EXPECT_EQ(diagnostics.diagnostics()[12].code, DiagnosticCode::semantic_lookup);
    EXPECT_EQ(diagnostics.diagnostics()[12].severity, base::Severity::help);
    EXPECT_EQ(diagnostics.diagnostics()[0].message, diagnostics.diagnostics()[1].message);
}

TEST(CoreUnit, CanonicalTypeBuilderMapsBuiltinTypesWithoutSessionHandles)
{
    sema::TypeTable types;
    CanonicalTypeBuilderTestResolver resolver;

    const std::array<std::pair<BuiltinType, query::BuiltinTypeKey>, 16> builtins{{
        {BuiltinType::void_, query::BuiltinTypeKey::void_},
        {BuiltinType::bool_, query::BuiltinTypeKey::bool_},
        {BuiltinType::i8, query::BuiltinTypeKey::i8},
        {BuiltinType::u8, query::BuiltinTypeKey::u8},
        {BuiltinType::i16, query::BuiltinTypeKey::i16},
        {BuiltinType::u16, query::BuiltinTypeKey::u16},
        {BuiltinType::i32, query::BuiltinTypeKey::i32},
        {BuiltinType::u32, query::BuiltinTypeKey::u32},
        {BuiltinType::i64, query::BuiltinTypeKey::i64},
        {BuiltinType::u64, query::BuiltinTypeKey::u64},
        {BuiltinType::isize, query::BuiltinTypeKey::isize},
        {BuiltinType::usize, query::BuiltinTypeKey::usize},
        {BuiltinType::f32, query::BuiltinTypeKey::f32},
        {BuiltinType::f64, query::BuiltinTypeKey::f64},
        {BuiltinType::str, query::BuiltinTypeKey::str},
        {BuiltinType::char_, query::BuiltinTypeKey::char_},
    }};

    for (const auto& [source, expected] : builtins) {
        base::Result<query::CanonicalTypeKey> key =
            sema::build_canonical_type_key(types, types.builtin(source), resolver);
        ASSERT_TRUE(key.has_value());
        EXPECT_EQ(key.value(), query::canonical_builtin(expected));
    }
}

TEST(CoreUnit, CanonicalTypeBuilderMapsCompoundNominalAndGenericTypes)
{
    sema::TypeTable types;
    CanonicalTypeBuilderTestResolver resolver;
    const query::ModuleKey module = query_test_module_key();
    const query::DefKey vector_def = query_test_def_key(module, query::DefKind::struct_, "Vec");
    const query::DefKey enum_def = query_test_def_key(module, query::DefKind::enum_, "Choice");
    const query::DefKey opaque_def = query_test_def_key(module, query::DefKind::struct_, "Opaque");
    const query::GenericParamKey type_param_key = query::generic_param_key(vector_def, 0);
    const sema::GenericParamIdentity type_param_identity = sema::generic_param_identity_from_text("types.Vec.T");

    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle type_param = types.generic_param(type_param_identity, "T");
    const TypeHandle vector_type = types.named_struct("Vec", "Vec", false);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");
    const TypeHandle opaque_type = types.opaque_struct("Opaque", "Opaque");
    const std::array<TypeHandle, 1> vector_args{type_param};
    types.set_generic_instance(vector_type, "Vec", vector_args);
    resolver.bind_nominal(vector_type, vector_def);
    resolver.bind_nominal(enum_type, enum_def);
    resolver.bind_nominal(opaque_type, opaque_def);
    resolver.bind_generic(type_param_identity, type_param_key);

    const TypeHandle array_bool = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, bool_type);
    const TypeHandle pointer_vector = types.pointer(PointerMutability::mut, vector_type);
    const TypeHandle slice_i32 = types.slice(PointerMutability::mut, i32);
    const TypeHandle reference_slice = types.reference(PointerMutability::const_, slice_i32);
    const std::array<TypeHandle, 3> tuple_elements{array_bool, pointer_vector, reference_slice};
    const TypeHandle tuple_type = types.tuple(tuple_elements);
    const std::array<TypeHandle, 2> function_params{tuple_type, enum_type};
    const TypeHandle function_type =
        types.function(sema::FunctionCallConv::c, true, true, function_params, opaque_type);
    const TypeHandle empty_function_type =
        types.function(sema::FunctionCallConv::aurex, false, false, std::span<const TypeHandle>{}, i32);

    base::Result<query::CanonicalTypeKey> key = sema::build_canonical_type_key(types, function_type, resolver);
    ASSERT_TRUE(key.has_value());
    base::Result<query::CanonicalTypeKey> empty_function_key =
        sema::build_canonical_type_key(types, empty_function_type, resolver);
    ASSERT_TRUE(empty_function_key.has_value());

    const query::CanonicalTypeKey expected_type_param = query::canonical_generic_param(type_param_key);
    const std::array<query::CanonicalTypeKey, 1> expected_vector_args{expected_type_param};
    const query::CanonicalTypeKey expected_vector = query::canonical_nominal(vector_def, expected_vector_args);
    const query::CanonicalTypeKey expected_array_bool =
        query::canonical_array(SEMA_TEST_SMALL_ARRAY_COUNT, query::canonical_builtin(query::BuiltinTypeKey::bool_));
    const query::CanonicalTypeKey expected_pointer_vector =
        query::canonical_pointer(query::PointerMutabilityKey::mut, expected_vector);
    const query::CanonicalTypeKey expected_slice_i32 =
        query::canonical_slice(query::PointerMutabilityKey::mut, query::canonical_builtin(query::BuiltinTypeKey::i32));
    const query::CanonicalTypeKey expected_reference_slice =
        query::canonical_reference(query::PointerMutabilityKey::const_, expected_slice_i32);
    const std::array<query::CanonicalTypeKey, 3> expected_tuple_elements{
        expected_array_bool,
        expected_pointer_vector,
        expected_reference_slice,
    };
    const query::CanonicalTypeKey expected_tuple = query::canonical_tuple(expected_tuple_elements);
    const query::CanonicalTypeKey expected_enum =
        query::canonical_nominal(enum_def, std::span<const query::CanonicalTypeKey>{});
    const query::CanonicalTypeKey expected_opaque =
        query::canonical_nominal(opaque_def, std::span<const query::CanonicalTypeKey>{});
    const std::array<query::CanonicalTypeKey, 2> expected_params{expected_tuple, expected_enum};
    const query::CanonicalTypeKey expected_function =
        query::canonical_function(query::FunctionCallConvKey::c, true, true, expected_params, expected_opaque);
    const query::CanonicalTypeKey expected_empty_function = query::canonical_function(query::FunctionCallConvKey::aurex,
        false, false, std::span<const query::CanonicalTypeKey>{}, query::canonical_builtin(query::BuiltinTypeKey::i32));

    EXPECT_EQ(key.value(), expected_function);
    EXPECT_EQ(empty_function_key.value(), expected_empty_function);
    EXPECT_EQ(query::stable_key_fingerprint(key.value()), query::stable_key_fingerprint(expected_function));
}

TEST(CoreUnit, CanonicalTypeBuilderMapsAssociatedProjectionTypes)
{
    sema::TypeTable types;
    CanonicalTypeBuilderTestResolver resolver;
    const query::ModuleKey module = query_test_module_key();
    const std::array<std::string_view, 1> source_path{"Source"};
    const query::DefKey source_def =
        query::def_key(module, query::DefNamespace::trait_, query::DefKind::trait_, source_path);
    const query::MemberKey item_member =
        query::member_key(source_def, query::MemberKind::associated_type, "Item", SEMA_TEST_PATTERN_FIRST_INDEX);
    const sema::GenericParamIdentity type_param_identity = sema::generic_param_identity_from_text("types.Source.T");
    const query::GenericParamKey type_param_key = query::generic_param_key(source_def, SEMA_TEST_PATTERN_FIRST_INDEX);

    const TypeHandle type_param = types.generic_param(type_param_identity, "T");
    const TypeHandle projection = types.associated_projection(type_param, item_member, "Item");
    resolver.bind_generic(type_param_identity, type_param_key);

    base::Result<query::CanonicalTypeKey> key = sema::build_canonical_type_key(types, projection, resolver);
    ASSERT_TRUE(key.has_value());
    const query::CanonicalTypeKey expected =
        query::canonical_associated_type_projection(query::canonical_generic_param(type_param_key), item_member);
    EXPECT_EQ(key.value(), expected);

    TypeTableTestAccess::entries(types)[projection.value].associated_member = query::MemberKey{};
    base::Result<query::CanonicalTypeKey> invalid_member = sema::build_canonical_type_key(types, projection, resolver);
    EXPECT_FALSE(invalid_member.has_value());
    EXPECT_NE(invalid_member.error().message.find("unresolved associated type member"), std::string::npos);
}

TEST(CoreUnit, CanonicalTypeBuilderRejectsUnresolvedOrInvalidTypes)
{
    sema::TypeTable types;
    CanonicalTypeBuilderTestResolver resolver;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle unresolved_struct = types.named_struct("Missing", "Missing", false);
    const sema::GenericParamIdentity type_param_identity = sema::generic_param_identity_from_text("types.Missing.T");
    const TypeHandle unresolved_generic = types.generic_param(type_param_identity, "T");
    const TypeHandle array_invalid = types.array(SEMA_TEST_INVALID_ARRAY_COUNT, INVALID_TYPE_HANDLE);
    const TypeHandle array_unknown = types.array(SEMA_TEST_INVALID_ARRAY_COUNT,
        TypeHandle{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)});
    const TypeHandle pointer_i32 = types.pointer(PointerMutability::mut, i32);
    const TypeHandle reference_i32 = types.reference(PointerMutability::const_, i32);
    const TypeHandle slice_i32 = types.slice(PointerMutability::mut, i32);
    const TypeHandle function_i32 =
        types.function(sema::FunctionCallConv::aurex, false, false, std::span<const TypeHandle>{}, i32);

    base::Result<query::CanonicalTypeKey> invalid =
        sema::build_canonical_type_key(types, INVALID_TYPE_HANDLE, resolver);
    EXPECT_FALSE(invalid.has_value());
    EXPECT_NE(invalid.error().message.find("invalid type handle"), std::string::npos);

    base::Result<query::CanonicalTypeKey> unknown = sema::build_canonical_type_key(
        types, TypeHandle{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)}, resolver);
    EXPECT_FALSE(unknown.has_value());
    EXPECT_NE(unknown.error().message.find("unknown type handle"), std::string::npos);

    base::Result<query::CanonicalTypeKey> missing_nominal =
        sema::build_canonical_type_key(types, unresolved_struct, resolver);
    EXPECT_FALSE(missing_nominal.has_value());
    EXPECT_NE(missing_nominal.error().message.find("unresolved nominal type key"), std::string::npos);

    base::Result<query::CanonicalTypeKey> missing_generic =
        sema::build_canonical_type_key(types, unresolved_generic, resolver);
    EXPECT_FALSE(missing_generic.has_value());
    EXPECT_NE(missing_generic.error().message.find("unresolved generic parameter key"), std::string::npos);

    base::Result<query::CanonicalTypeKey> invalid_child =
        sema::build_canonical_type_key(types, array_invalid, resolver);
    EXPECT_FALSE(invalid_child.has_value());
    EXPECT_NE(invalid_child.error().message.find("invalid type handle"), std::string::npos);

    base::Result<query::CanonicalTypeKey> unknown_child =
        sema::build_canonical_type_key(types, array_unknown, resolver);
    EXPECT_FALSE(unknown_child.has_value());
    EXPECT_NE(unknown_child.error().message.find("unknown type handle"), std::string::npos);

    TypeTableTestAccess::entries(types)[i32.value].builtin = SEMA_TEST_INVALID_BUILTIN_TYPE;
    base::Result<query::CanonicalTypeKey> unsupported_builtin = sema::build_canonical_type_key(types, i32, resolver);
    EXPECT_FALSE(unsupported_builtin.has_value());
    EXPECT_NE(unsupported_builtin.error().message.find("unsupported builtin type"), std::string::npos);
    TypeTableTestAccess::entries(types)[i32.value].builtin = BuiltinType::i32;

    TypeTableTestAccess::entries(types)[pointer_i32.value].pointer_mutability = SEMA_TEST_INVALID_POINTER_MUTABILITY;
    base::Result<query::CanonicalTypeKey> unsupported_pointer_mutability =
        sema::build_canonical_type_key(types, pointer_i32, resolver);
    EXPECT_FALSE(unsupported_pointer_mutability.has_value());
    EXPECT_NE(unsupported_pointer_mutability.error().message.find("unsupported pointer mutability"), std::string::npos);

    TypeTableTestAccess::entries(types)[reference_i32.value].pointer_mutability = SEMA_TEST_INVALID_POINTER_MUTABILITY;
    base::Result<query::CanonicalTypeKey> unsupported_reference_mutability =
        sema::build_canonical_type_key(types, reference_i32, resolver);
    EXPECT_FALSE(unsupported_reference_mutability.has_value());
    EXPECT_NE(
        unsupported_reference_mutability.error().message.find("unsupported pointer mutability"), std::string::npos);

    TypeTableTestAccess::entries(types)[slice_i32.value].slice_mutability = SEMA_TEST_INVALID_POINTER_MUTABILITY;
    base::Result<query::CanonicalTypeKey> unsupported_slice_mutability =
        sema::build_canonical_type_key(types, slice_i32, resolver);
    EXPECT_FALSE(unsupported_slice_mutability.has_value());
    EXPECT_NE(unsupported_slice_mutability.error().message.find("unsupported pointer mutability"), std::string::npos);

    TypeTableTestAccess::entries(types)[function_i32.value].function_call_conv = SEMA_TEST_INVALID_FUNCTION_CALL_CONV;
    base::Result<query::CanonicalTypeKey> unsupported_call_conv =
        sema::build_canonical_type_key(types, function_i32, resolver);
    EXPECT_FALSE(unsupported_call_conv.has_value());
    EXPECT_NE(unsupported_call_conv.error().message.find("unsupported function call convention"), std::string::npos);

    TypeTableTestAccess::entries(types)[i32.value].kind = SEMA_TEST_INVALID_TYPE_KIND;
    base::Result<query::CanonicalTypeKey> unsupported = sema::build_canonical_type_key(types, i32, resolver);
    EXPECT_FALSE(unsupported.has_value());
    EXPECT_NE(unsupported.error().message.find("unsupported type kind"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxLayoutPlacesAndModules)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
        module_info({"lib", "reexported"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(1), "one"),
        resolved_import(module_id(2), "two"),
        resolved_import(syntax::INVALID_MODULE_ID, "broken"),
    };
    module.modules[1].imports = {
        resolved_import(module_id(3), "pub", syntax::Visibility::public_),
    };

    const ExprId value_expr = push_name(module, "value");
    const ExprId scoped_value_expr = push_name(module, "shared", "one");
    const ExprId missing_scoped_value_expr = push_name(module, "shared", "missing");
    const ExprId ptr_expr = push_name(module, "ptr");
    const ExprId record_expr = push_name(module, "record");
    const ExprId array_expr = push_name(module, "array");
    const ExprId field_id = push_field(module, ptr_expr, "field");
    const ExprId value_field_id = push_field(module, record_expr, "field");
    const ExprId nested_value_field_id = push_field(module, value_field_id, "field");
    const ExprId index_arg = push_integer(module);
    const ExprId index_id = module.push_index_expr({}, syntax::IndexExprPayload{ptr_expr, index_arg});
    const ExprId value_index_id = module.push_index_expr({}, syntax::IndexExprPayload{array_expr, index_arg});
    const ExprId nested_value_index_id =
        module.push_index_expr({}, syntax::IndexExprPayload{value_index_id, index_arg});
    const ExprId deref_id = push_unary(module, syntax::UnaryOp::dereference, ptr_expr);
    const ExprId invalid_deref_id = push_unary(module, syntax::UnaryOp::dereference, value_expr);
    const ExprId not_id = push_unary(module, syntax::UnaryOp::logical_not, ptr_expr);
    const ExprId array_ref_expr = push_name(module, "array_ref");
    const ExprId array_ref_index_id = module.push_index_expr({}, syntax::IndexExprPayload{array_ref_expr, index_arg});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle i8 = types.builtin(BuiltinType::i8);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle i16 = types.builtin(BuiltinType::i16);
    const TypeHandle u16 = types.builtin(BuiltinType::u16);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle u32 = types.builtin(BuiltinType::u32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);
    const TypeHandle isize = types.builtin(BuiltinType::isize);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle f32 = types.builtin(BuiltinType::f32);
    const TypeHandle f64 = types.builtin(BuiltinType::f64);
    const TypeHandle str = types.builtin(BuiltinType::str);
    const TypeHandle char_type = types.builtin(BuiltinType::char_);
    const TypeHandle ptr_i32 = types.pointer(PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = types.pointer(PointerMutability::const_, i32);
    const TypeHandle ref_i32 = types.reference(PointerMutability::mut, i32);
    const TypeHandle ref_u32 = types.reference(PointerMutability::mut, u32);
    const TypeHandle slice_i32 = types.slice(PointerMutability::mut, i32);
    const TypeHandle slice_u32 = types.slice(PointerMutability::mut, u32);
    const TypeHandle array_i16 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i16);
    const TypeHandle missing_struct = types.named_struct("missing.Struct", "missing_Struct", false);
    const TypeHandle record_type = types.named_struct("lib.one.Record", "lib_one_Record", false);
    const TypeHandle enum_type = types.named_enum("lib.one.Enum", "lib_one_Enum");
    const TypeHandle payload_enum_type = types.named_enum("lib.one.Payload", "lib_one_Payload");
    const TypeHandle opaque_type = types.opaque_struct("lib.one.Opaque", "lib_one_Opaque");
    const TypeHandle nested_array_i16 = types.array(SEMA_TEST_NESTED_ARRAY_COUNT, array_i16);
    const TypeHandle array_void = types.array(SEMA_TEST_INVALID_ARRAY_COUNT, void_type);
    const TypeHandle array_opaque = types.array(SEMA_TEST_INVALID_ARRAY_COUNT, opaque_type);
    const TypeHandle overflowing_array = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, array_i16);
    const TypeHandle place_record_type = types.named_struct("root.PlaceRecord", "root_PlaceRecord", false);
    const TypeHandle ptr_place_record = types.pointer(PointerMutability::mut, place_record_type);
    const TypeHandle ref_nested_array_i16 = types.reference(PointerMutability::mut, nested_array_i16);
    const TypeHandle generic_param = types.generic_param(sema::generic_param_identity_from_text("layout.T"), "T");
    types.set_enum_underlying(enum_type, u16);
    types.set_enum_underlying(payload_enum_type, u8);
    types.set_enum_payload_layout(payload_enum_type, u64, sizeof(std::uint64_t), alignof(std::uint64_t));

    StructInfo record;
    record.name = checked_text(analyzer.state_.checked, "Record");
    record.name_id = intern_identifier(analyzer, "Record");
    record.module = module_id(1);
    record.type = record_type;
    record.fields = {
        struct_field_info(analyzer, "tag", module_id(1), u8),
        struct_field_info(analyzer, "value", module_id(1), u64),
    };
    analyzer.state_.checked.structs.emplace(semantic_module_key(analyzer, module_id(1), "Record"), record);

    StructInfo place_record;
    place_record.name = checked_text(analyzer.state_.checked, "PlaceRecord");
    place_record.name_id = intern_identifier(analyzer, "PlaceRecord");
    place_record.module = module_id(0);
    place_record.type = place_record_type;
    place_record.fields = {
        struct_field_info(analyzer, "field", module_id(0), place_record_type),
    };
    analyzer.state_.checked.structs.emplace(semantic_module_key(analyzer, module_id(0), "PlaceRecord"), place_record);

    EXPECT_EQ(analyzer.abi_size(INVALID_TYPE_HANDLE), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(INVALID_TYPE_HANDLE), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(void_type), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(void_type), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(bool_type), sema::SEMA_DEFAULT_TARGET_BOOL_SIZE);
    EXPECT_EQ(analyzer.abi_align(bool_type), sema::SEMA_DEFAULT_TARGET_BOOL_ALIGN);
    EXPECT_EQ(analyzer.abi_size(i8), sema::SEMA_DEFAULT_TARGET_I8_SIZE);
    EXPECT_EQ(analyzer.abi_size(u8), sema::SEMA_DEFAULT_TARGET_I8_SIZE);
    EXPECT_EQ(analyzer.abi_size(i16), sema::SEMA_DEFAULT_TARGET_I16_SIZE);
    EXPECT_EQ(analyzer.abi_size(u16), sema::SEMA_DEFAULT_TARGET_I16_SIZE);
    EXPECT_EQ(analyzer.abi_size(i32), sema::SEMA_DEFAULT_TARGET_I32_SIZE);
    EXPECT_EQ(analyzer.abi_size(u32), sema::SEMA_DEFAULT_TARGET_I32_SIZE);
    EXPECT_EQ(analyzer.abi_size(i64), sema::SEMA_DEFAULT_TARGET_I64_SIZE);
    EXPECT_EQ(analyzer.abi_size(u64), sema::SEMA_DEFAULT_TARGET_I64_SIZE);
    EXPECT_EQ(analyzer.abi_size(isize), sema::SEMA_DEFAULT_TARGET_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_size(usize), sema::SEMA_DEFAULT_TARGET_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_size(f32), sema::SEMA_DEFAULT_TARGET_F32_SIZE);
    EXPECT_EQ(analyzer.abi_size(f64), sema::SEMA_DEFAULT_TARGET_F64_SIZE);
    EXPECT_EQ(analyzer.abi_size(str), sema::SEMA_DEFAULT_TARGET_POINTER_SIZE + sema::SEMA_DEFAULT_TARGET_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_size(char_type), sema::SEMA_DEFAULT_TARGET_CHAR_SIZE);
    const sema::SemanticAnalyzerCore::TypeAbiLayout i32_layout = analyzer.abi_layout(i32);
    EXPECT_EQ(i32_layout.size, sema::SEMA_DEFAULT_TARGET_I32_SIZE);
    EXPECT_EQ(i32_layout.align, sema::SEMA_DEFAULT_TARGET_I32_ALIGN);
    EXPECT_EQ(analyzer.abi_align(i8), sema::SEMA_DEFAULT_TARGET_I8_ALIGN);
    EXPECT_EQ(analyzer.abi_align(i64), sema::SEMA_DEFAULT_TARGET_I64_ALIGN);
    EXPECT_EQ(analyzer.abi_align(isize), sema::SEMA_DEFAULT_TARGET_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_align(usize), sema::SEMA_DEFAULT_TARGET_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_align(f32), sema::SEMA_DEFAULT_TARGET_F32_ALIGN);
    EXPECT_EQ(analyzer.abi_align(f64), sema::SEMA_DEFAULT_TARGET_F64_ALIGN);
    EXPECT_EQ(analyzer.abi_align(str), sema::SEMA_DEFAULT_TARGET_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_align(char_type), sema::SEMA_DEFAULT_TARGET_CHAR_ALIGN);
    EXPECT_EQ(analyzer.abi_size(ptr_i32), sema::SEMA_DEFAULT_TARGET_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_align(ptr_i32), sema::SEMA_DEFAULT_TARGET_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_size(array_i16), SEMA_TEST_SMALL_ARRAY_COUNT * sema::SEMA_DEFAULT_TARGET_I16_SIZE);
    EXPECT_EQ(analyzer.abi_align(array_i16), sema::SEMA_DEFAULT_TARGET_I16_ALIGN);
    EXPECT_EQ(analyzer.abi_size(missing_struct), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(missing_struct), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(record_type), SEMA_TEST_RECORD_ABI_SIZE);
    EXPECT_EQ(analyzer.abi_align(record_type), alignof(std::uint64_t));
    EXPECT_EQ(analyzer.abi_size(enum_type), sema::SEMA_DEFAULT_TARGET_I16_SIZE);
    EXPECT_EQ(analyzer.abi_align(enum_type), sema::SEMA_DEFAULT_TARGET_I16_ALIGN);
    EXPECT_GT(analyzer.abi_size(payload_enum_type), sema::SEMA_DEFAULT_TARGET_I8_SIZE);
    EXPECT_EQ(analyzer.abi_align(payload_enum_type), sema::SEMA_DEFAULT_TARGET_I64_ALIGN);
    EXPECT_EQ(analyzer.abi_size(opaque_type), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(opaque_type), SEMA_TEST_ABI_MIN_ALIGNMENT);
    static_cast<void>(analyzer.abi_layout(place_record_type));

    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, INVALID_TYPE_HANDLE, i32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, generic_param, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::cast, f64, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::pcast, const_ptr_i32, ptr_i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, i32, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, u32, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, char_type, u32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, u32, char_type));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::bcast, bool_type, i8));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::bcast, str, ptr_i32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::ptr_addr, u32, ptr_i32));
    EXPECT_FALSE(analyzer.is_valid_storage_type(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(analyzer.is_valid_storage_type(void_type));
    EXPECT_FALSE(analyzer.is_valid_storage_type(opaque_type));
    EXPECT_TRUE(analyzer.is_valid_storage_type(char_type));
    EXPECT_TRUE(analyzer.is_valid_storage_type(nested_array_i16));
    EXPECT_FALSE(analyzer.is_valid_storage_type(array_void));
    EXPECT_FALSE(analyzer.is_valid_storage_type(array_opaque));
    EXPECT_FALSE(analyzer.is_valid_storage_type(overflowing_array));
    EXPECT_FALSE(analyzer.is_array_containing_value_type(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(analyzer.is_array_containing_value_type(i32));
    EXPECT_TRUE(analyzer.is_array_containing_value_type(nested_array_i16));
    EXPECT_FALSE(analyzer.can_assign(ref_i32, ref_u32, syntax::INVALID_EXPR_ID));
    EXPECT_FALSE(analyzer.can_assign(slice_i32, slice_u32, syntax::INVALID_EXPR_ID));
    EXPECT_TRUE(analyzer.check_m2_value_abi(INVALID_TYPE_HANDLE, sema::ValueAbiContext::parameter, {}));

    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "value", module_id(0), i32, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "ptr", module_id(0), ptr_place_record, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "record", module_id(0), place_record_type, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "array", module_id(0), nested_array_i16, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "array_ref", module_id(0), ref_nested_array_i16, true),
        diagnostics));
    static_cast<void>(add_global_value(analyzer, module_id(1), "shared", i32, SymbolKind::local, true));

    EXPECT_TRUE(analyzer.is_place_expr(value_expr));
    EXPECT_FALSE(analyzer.is_place_expr(scoped_value_expr));
    EXPECT_FALSE(analyzer.is_place_expr(missing_scoped_value_expr));
    EXPECT_TRUE(analyzer.is_place_expr(field_id));
    EXPECT_TRUE(analyzer.is_place_expr(index_id));
    EXPECT_TRUE(analyzer.is_place_expr(nested_value_field_id));
    EXPECT_TRUE(analyzer.is_place_expr(nested_value_index_id));
    EXPECT_TRUE(analyzer.is_place_expr(deref_id));
    EXPECT_FALSE(analyzer.is_place_expr(not_id));
    EXPECT_FALSE(analyzer.is_place_expr(syntax::INVALID_EXPR_ID));
    EXPECT_TRUE(analyzer.is_writable_place(value_expr));
    EXPECT_FALSE(analyzer.is_writable_place(scoped_value_expr));
    EXPECT_FALSE(analyzer.is_writable_place(missing_scoped_value_expr));
    EXPECT_TRUE(analyzer.is_writable_place(field_id));
    EXPECT_TRUE(analyzer.is_writable_place(index_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_field_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(nested_value_field_id));
    EXPECT_TRUE(analyzer.is_writable_place(nested_value_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(array_ref_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(deref_id));
    EXPECT_FALSE(analyzer.is_writable_place(not_id));
    EXPECT_FALSE(analyzer.is_writable_place(syntax::INVALID_EXPR_ID));
    const sema::SemanticAnalyzerCore::PlaceInfo invalid_deref_place =
        analyzer.analyze_place_info(invalid_deref_id, true);
    EXPECT_FALSE(invalid_deref_place.is_place);
    EXPECT_FALSE(diagnostics.diagnostics().empty());

    analyzer.state_.flow.current_module = syntax::INVALID_MODULE_ID;
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("missing", {})));
    analyzer.state_.flow.current_module = module_id(0);
    analyzer.ctx_.module.modules[0].imports.push_back(resolved_import(module_id(2), "one"));
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("one", {})));
    analyzer.ctx_.module.modules[0].imports.pop_back();
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("missing", {})));

    EXPECT_TRUE(analyzer.visible_modules(syntax::INVALID_MODULE_ID).empty());
    EXPECT_EQ(analyzer.visible_modules(module_id(SEMA_TEST_MISSING_MODULE_INDEX)).size(), 1U);
    EXPECT_TRUE(analyzer.module_export_modules(syntax::INVALID_MODULE_ID).empty());
    EXPECT_EQ(analyzer.module_export_modules(module_id(SEMA_TEST_MISSING_MODULE_INDEX)).size(), 1U);
    const auto& visible = analyzer.visible_modules(module_id(0));
    ASSERT_GE(visible.size(), 4U);
    EXPECT_EQ(analyzer.module_name(syntax::INVALID_MODULE_ID), "<unknown>");
    EXPECT_EQ(analyzer.qualified_name(syntax::INVALID_MODULE_ID, "Name"), "Name");
    EXPECT_EQ(analyzer.c_symbol_name(syntax::INVALID_MODULE_ID, "Name"), "Name");
}

TEST(CoreUnit, SemanticWhiteBoxTargetLayoutControlsPointerSizedIntegerSemantics)
{
    constexpr base::u64 SEMA_TEST_32BIT_POINTER_SIZE = 4;
    constexpr base::u64 SEMA_TEST_32BIT_POINTER_ALIGN = 4;

    syntax::AstModule module;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle isize = types.builtin(BuiltinType::isize);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle ref_i32 = types.reference(PointerMutability::const_, types.builtin(BuiltinType::i32));
    const TypeHandle slice_i32 = types.slice(PointerMutability::const_, types.builtin(BuiltinType::i32));

    analyzer.ctx_.options.target_layout.pointer_size = SEMA_TEST_32BIT_POINTER_SIZE;
    analyzer.ctx_.options.target_layout.pointer_align = SEMA_TEST_32BIT_POINTER_ALIGN;

    EXPECT_EQ(analyzer.abi_size(isize), SEMA_TEST_32BIT_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_size(usize), SEMA_TEST_32BIT_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_align(ref_i32), SEMA_TEST_32BIT_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_size(slice_i32), SEMA_TEST_32BIT_POINTER_SIZE + SEMA_TEST_32BIT_POINTER_SIZE);
    EXPECT_TRUE(analyzer.integer_literal_fits_type(usize, "4294967295usize"));
    EXPECT_FALSE(analyzer.integer_literal_fits_type(usize, "4294967296usize"));
    EXPECT_TRUE(analyzer.negative_integer_literal_fits_type(isize, "2147483648isize"));
    EXPECT_FALSE(analyzer.negative_integer_literal_fits_type(isize, "2147483649isize"));
}

TEST(CoreUnit, SemanticWhiteBoxVisibilityLatticeAccessAndSurfaceLeaks)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::DeclContext root_declaration = analyzer.declaration_context(module_id(SEMA_TEST_ROOT_MODULE_INDEX));
    const sema::DeclContext lib_declaration = analyzer.declaration_context(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX));
    const sema::AccessContext root_access = analyzer.current_access_context();
    const sema::VisibilityPolicy policy;

    EXPECT_TRUE(query::is_valid(root_declaration.module));
    EXPECT_TRUE(query::is_valid(root_access.module));
    EXPECT_TRUE(sema::access_context_same_module(root_declaration, root_access));
    EXPECT_FALSE(sema::access_context_same_module(lib_declaration, root_access));
    EXPECT_TRUE(sema::access_context_same_package(lib_declaration, root_access));
    EXPECT_TRUE(policy.can_access(syntax::Visibility::public_, lib_declaration, root_access));
    EXPECT_TRUE(policy.can_access(syntax::Visibility::package_, lib_declaration, root_access));
    EXPECT_FALSE(policy.can_access(syntax::Visibility::private_, lib_declaration, root_access));
    EXPECT_TRUE(policy.can_expose_type(syntax::Visibility::package_, syntax::Visibility::package_));
    EXPECT_FALSE(policy.can_expose_type(syntax::Visibility::public_, syntax::Visibility::package_));

    const std::array<std::string_view, 1> package_one{"package-one"};
    const std::array<std::string_view, 1> package_two{"package-two"};
    const std::array<std::string_view, 1> package_module{"visible"};
    const query::ModuleKey package_one_module = query::module_key(query::package_key(package_one), package_module);
    const query::ModuleKey package_two_module = query::module_key(query::package_key(package_two), package_module);
    const sema::DeclContext package_one_declaration = sema::decl_context_from_module_key(package_one_module);
    const sema::AccessContext package_two_access = sema::access_context_from_module_key(package_two_module);
    EXPECT_FALSE(sema::access_context_same_package(package_one_declaration, package_two_access));
    EXPECT_FALSE(policy.can_access(syntax::Visibility::package_, package_one_declaration, package_two_access));

    EXPECT_TRUE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::public_));
    EXPECT_TRUE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::package_));
    EXPECT_FALSE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::private_));
    EXPECT_TRUE(analyzer.can_access_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), syntax::Visibility::private_));

    sema::SemanticOptions split_package_options;
    split_package_options.module_packages = {
        query::package_key(package_one),
        query::package_key(package_two),
        query::package_key(package_two),
    };
    sema::SemanticAnalyzerCore split_package_analyzer(module, diagnostics, split_package_options);
    split_package_analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_FALSE(split_package_analyzer.can_access_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::package_));
    EXPECT_NE(split_package_analyzer.query_module_key(module_id(SEMA_TEST_ROOT_MODULE_INDEX)).package,
        split_package_analyzer.query_module_key(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX)).package);

    analyzer.state_.flow.current_module = syntax::INVALID_MODULE_ID;
    EXPECT_TRUE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::public_));
    EXPECT_FALSE(analyzer.can_access_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), syntax::Visibility::package_));
    EXPECT_FALSE(query::is_valid(analyzer.current_access_context().module));

    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_FALSE(analyzer.can_access_module(syntax::INVALID_MODULE_ID, syntax::Visibility::package_));

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle package_type = types.named_struct("root.PackageOnly", "root_PackageOnly", false);
    static_cast<void>(add_struct_info(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "PackageOnly", package_type, syntax::Visibility::package_));

    FunctionSignature public_function =
        indexed_function_signature(analyzer, "leaks_package", module_id(SEMA_TEST_ROOT_MODULE_INDEX), package_type);
    public_function.visibility = syntax::Visibility::public_;
    static_cast<void>(add_function(analyzer, public_function));

    analyzer.validate_exported_signature_surfaces();
    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(
        messages.find("public function `leaks_package` exposes package-visible type `root.PackageOnly`"),
        std::string::npos);

    base::DiagnosticSink package_diagnostics;
    sema::SemanticAnalyzerCore package_analyzer(module, package_diagnostics);
    package_analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    sema::TypeTable& package_types = package_analyzer.state_.checked.types;
    const TypeHandle private_type = package_types.named_struct("root.PrivateOnly", "root_PrivateOnly", false);
    static_cast<void>(add_struct_info(package_analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "PrivateOnly",
        private_type, syntax::Visibility::private_));

    FunctionSignature package_function = indexed_function_signature(
        package_analyzer, "leaks_private", module_id(SEMA_TEST_ROOT_MODULE_INDEX), private_type);
    package_function.visibility = syntax::Visibility::package_;
    static_cast<void>(add_function(package_analyzer, package_function));

    package_analyzer.validate_exported_signature_surfaces();
    const std::string package_messages = diagnostic_messages(package_diagnostics);
    EXPECT_NE(package_messages.find("package-visible function `leaks_private` exposes private type `root.PrivateOnly`"),
        std::string::npos);

    const std::optional<sema::SemanticAnalyzerCore::DeclarationAnalyzer::ExportSurfaceRestrictedType> package_leak =
        sema::SemanticAnalyzerCore::DeclarationAnalyzer(package_analyzer)
            .restricted_type_exposed_by_surface_type(private_type, syntax::Visibility::package_);
    ASSERT_TRUE(package_leak.has_value());
    EXPECT_EQ(package_leak->name, "root.PrivateOnly");
    EXPECT_EQ(package_leak->visibility, syntax::Visibility::private_);
}

TEST(CoreUnit, SemanticWhiteBoxModulePartContextsTrackCurrentItem)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    constexpr base::u32 SEMA_TEST_FRAGMENT_PART_INDEX = 2;
    constexpr base::usize SEMA_TEST_PART_KEY_TABLE_SIZE = SEMA_TEST_FRAGMENT_PART_INDEX + 1;
    const std::array<std::string_view, 1> package_parts{"part-package"};
    const std::array<std::string_view, 1> module_parts{"root"};
    const query::PackageKey package = query::package_key(package_parts);
    const query::ModuleKey module_key = query::module_key_from_stable_id(
        package, sema::stable_module_id(std::span<const std::string_view>{module_parts.data(), module_parts.size()}));
    const query::FileKey primary_file = query::file_key(package, "/workspace/root.ax");
    const query::FileKey part_file = query::file_key(package, "/workspace/root.parts/types.ax");
    const query::ModulePartKey primary_key = query::module_part_key(
        module_key, primary_file, query::ModulePartKind::primary, "<primary>", SEMA_TEST_PRIMARY_PART_INDEX);
    const query::ModulePartKey part_key = query::module_part_key(
        module_key, part_file, query::ModulePartKind::fragment, "types", SEMA_TEST_FRAGMENT_PART_INDEX);

    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    syntax::ItemNode primary_item;
    primary_item.kind = syntax::ItemKind::fn_decl;
    primary_item.name = "primary";
    const syntax::ItemId primary_id =
        module.push_item_for_module(primary_item, module_id(0), SEMA_TEST_PRIMARY_PART_INDEX);
    syntax::ItemNode part_item;
    part_item.kind = syntax::ItemKind::fn_decl;
    part_item.name = "from_part";
    const syntax::ItemId part_id = module.push_item_for_module(part_item, module_id(0), SEMA_TEST_FRAGMENT_PART_INDEX);
    syntax::ItemImportScope part_import_scope;
    part_import_scope.item_begin = part_id.value;
    part_import_scope.item_count = 1;
    part_import_scope.part_index = SEMA_TEST_FRAGMENT_PART_INDEX;
    module.item_import_scopes.push_back(std::move(part_import_scope));

    sema::SemanticOptions options;
    options.module_packages.push_back(package);
    options.module_part_keys.resize(module.modules.size());
    options.module_part_keys[0].resize(SEMA_TEST_PART_KEY_TABLE_SIZE);
    options.module_part_keys[0][SEMA_TEST_PRIMARY_PART_INDEX] = primary_key;
    options.module_part_keys[0][SEMA_TEST_FRAGMENT_PART_INDEX] = part_key;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics, options);
    EXPECT_EQ(analyzer.item_part_index(primary_id), SEMA_TEST_PRIMARY_PART_INDEX);
    EXPECT_EQ(analyzer.item_part_index(part_id), SEMA_TEST_FRAGMENT_PART_INDEX);
    EXPECT_EQ(analyzer.query_module_part_key(primary_id), primary_key);
    EXPECT_EQ(analyzer.query_module_part_key(part_id), part_key);
    const syntax::ItemImportScope* const found_part_scope = analyzer.item_import_scope(part_id);
    ASSERT_NE(found_part_scope, nullptr);
    EXPECT_EQ(found_part_scope->part_index, SEMA_TEST_FRAGMENT_PART_INDEX);

    const sema::DeclContext part_declaration = analyzer.declaration_context(part_id);
    EXPECT_EQ(part_declaration.module, module_key);
    EXPECT_EQ(part_declaration.part, part_key);
    EXPECT_FALSE(query::is_valid(analyzer.declaration_context(module_id(0)).part));

    analyzer.state_.flow.current_module = module_id(0);
    analyzer.state_.flow.current_item = part_id;
    const sema::AccessContext part_access = analyzer.current_access_context();
    EXPECT_EQ(part_access.module, module_key);
    EXPECT_EQ(part_access.part, part_key);

    analyzer.state_.flow.current_item = syntax::INVALID_ITEM_ID;
    const sema::AccessContext module_only_access = analyzer.current_access_context();
    EXPECT_EQ(module_only_access.module, module_key);
    EXPECT_FALSE(query::is_valid(module_only_access.part));
    EXPECT_FALSE(query::is_valid(analyzer.query_module_part_key(syntax::INVALID_ITEM_ID)));
}

TEST(CoreUnit, SemanticWhiteBoxCheckedDumpSurfacesModulePartOrigins)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    constexpr base::u32 SEMA_TEST_FRAGMENT_PART_INDEX = 2;
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));
    const ExprId zero = push_integer_text(module, "0");
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode primary_function;
    primary_function.kind = syntax::ItemKind::fn_decl;
    primary_function.name = "primary";
    primary_function.return_type = i32_type;
    primary_function.body = body;
    static_cast<void>(module.push_item_for_module(
        primary_function, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_PRIMARY_PART_INDEX));

    syntax::ItemNode part_function;
    part_function.kind = syntax::ItemKind::fn_decl;
    part_function.name = "from_part";
    part_function.return_type = i32_type;
    part_function.body = body;
    const syntax::ItemId first_part_item = module.push_item_for_module(
        part_function, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX);

    syntax::ItemNode part_struct;
    part_struct.kind = syntax::ItemKind::struct_decl;
    part_struct.name = "PartRecord";
    static_cast<void>(module.push_item_for_module(
        part_struct, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::ItemNode part_alias;
    part_alias.kind = syntax::ItemKind::type_alias;
    part_alias.name = "PartAlias";
    part_alias.alias_type = i32_type;
    static_cast<void>(
        module.push_item_for_module(part_alias, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::EnumCaseDecl enum_case;
    enum_case.name = "some";
    syntax::ItemNode part_enum;
    part_enum.kind = syntax::ItemKind::enum_decl;
    part_enum.name = "PartChoice";
    part_enum.enum_cases = {enum_case};
    static_cast<void>(
        module.push_item_for_module(part_enum, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::ItemNode generic_struct;
    generic_struct.kind = syntax::ItemKind::struct_decl;
    generic_struct.name = "Box";
    generic_struct.generic_params = {syntax::GenericParamDecl{std::string(SEMA_TEST_GENERIC_PARAM_NAME), {}}};
    generic_struct.fields = {syntax::FieldDecl{"value", generic_type, {}}};
    static_cast<void>(module.push_item_for_module(
        generic_struct, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::ItemImportScope part_scope;
    part_scope.item_begin = first_part_item.value;
    part_scope.item_count = static_cast<base::u32>(module.items.size() - first_part_item.value);
    part_scope.part_index = SEMA_TEST_FRAGMENT_PART_INDEX;
    module.item_import_scopes.push_back(std::move(part_scope));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    const auto function_part_index = [&](const std::string_view name) -> std::optional<base::u32> {
        for (const auto& entry : checked.functions) {
            const FunctionSignature& signature = entry.second;
            if (signature.name == name) {
                return signature.part_index;
            }
        }
        return std::nullopt;
    };
    ASSERT_TRUE(function_part_index("primary").has_value());
    ASSERT_TRUE(function_part_index("from_part").has_value());
    EXPECT_EQ(function_part_index("primary").value(), SEMA_TEST_PRIMARY_PART_INDEX);
    EXPECT_EQ(function_part_index("from_part").value(), SEMA_TEST_FRAGMENT_PART_INDEX);

    bool struct_part_found = false;
    for (const auto& entry : checked.structs) {
        const StructInfo& info = entry.second;
        if (info.name == "PartRecord") {
            struct_part_found = true;
            EXPECT_EQ(info.part_index, SEMA_TEST_FRAGMENT_PART_INDEX);
        }
    }
    EXPECT_TRUE(struct_part_found);

    bool alias_part_found = false;
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& alias = entry.second;
        if (alias.name == "PartAlias") {
            alias_part_found = true;
            EXPECT_EQ(alias.part_index, SEMA_TEST_FRAGMENT_PART_INDEX);
        }
    }
    EXPECT_TRUE(alias_part_found);

    bool enum_part_found = false;
    for (const auto& entry : checked.enum_cases) {
        const EnumCaseInfo& info = entry.second;
        if (info.enum_name == "PartChoice" && info.case_name == "some") {
            enum_part_found = true;
            EXPECT_EQ(info.part_index, SEMA_TEST_FRAGMENT_PART_INDEX);
        }
    }
    EXPECT_TRUE(enum_part_found);

    bool template_part_found = false;
    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        if (info.name == "Box") {
            template_part_found = true;
            EXPECT_EQ(info.part_index, SEMA_TEST_FRAGMENT_PART_INDEX);
        }
    }
    EXPECT_TRUE(template_part_found);

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("template priv type Box params=1 @part=2"), std::string::npos);
    EXPECT_NE(checked_dump.find("fn priv primary -> i32"), std::string::npos);
    EXPECT_NE(checked_dump.find("fn priv from_part -> i32"), std::string::npos);
    EXPECT_NE(checked_dump.find("struct priv PartRecord @part=2 fields=0"), std::string::npos);
    EXPECT_NE(checked_dump.find("type priv PartAlias = i32 @part=2"), std::string::npos);
    EXPECT_NE(checked_dump.find("case PartChoice_some"), std::string::npos);
    EXPECT_NE(checked_dump.find("@part=0"), std::string::npos);
    EXPECT_NE(checked_dump.find("@part=2"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxCheckedDumpCoversPrimaryOnlyAndTemplateNamespaces)
{
    sema::CheckedModule primary_checked;
    const sema::InternedText primary_name = primary_checked.intern_text("primary_only");
    FunctionSignature primary_signature = primary_checked.make_function_signature();
    primary_signature.name = primary_name;
    primary_signature.name_id = primary_name.id;
    primary_signature.c_name = primary_name;
    primary_signature.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    primary_signature.return_type = primary_checked.types.builtin(BuiltinType::i32);
    primary_signature.semantic_key = sema::FunctionLookupKey{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        primary_name.id,
    };
    primary_checked.functions.emplace(primary_signature.semantic_key, std::move(primary_signature));

    const std::string primary_dump = sema::dump_checked_module(primary_checked);
    EXPECT_NE(primary_dump.find("fn primary_only -> i32"), std::string::npos);
    EXPECT_EQ(primary_dump.find("@part="), std::string::npos);

    sema::CheckedModule template_checked;
    constexpr base::u32 SEMA_TEST_TEMPLATE_PART_INDEX = 1;
    const std::array<query::DefNamespace, 5> namespaces{
        query::DefNamespace::value,
        query::DefNamespace::member,
        query::DefNamespace::trait_,
        query::DefNamespace::impl_,
        query::DefNamespace::synthetic,
    };
    for (base::usize index = 0; index < namespaces.size(); ++index) {
        sema::GenericTemplateSignatureInfo info;
        info.name = template_checked.intern_text("Template" + std::to_string(index));
        info.name_id = info.name.id;
        info.name_space = namespaces[index];
        info.param_count = static_cast<base::u32>(index);
        info.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
        template_checked.generic_template_signatures.push_back(info);
    }
    sema::GenericTemplateSignatureInfo unknown_template;
    unknown_template.name = template_checked.intern_text("TemplateUnknown");
    unknown_template.name_id = unknown_template.name.id;
    unknown_template.name_space = static_cast<query::DefNamespace>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    unknown_template.param_count = namespaces.size();
    unknown_template.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    template_checked.generic_template_signatures.push_back(unknown_template);

    const std::string template_dump = sema::dump_checked_module(template_checked);
    EXPECT_NE(template_dump.find("template value Template0 params=0 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template member Template1 params=1 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template trait Template2 params=2 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template impl Template3 params=3 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template synthetic Template4 params=4 @part=1"), std::string::npos);
    EXPECT_NE(template_dump.find("template unknown TemplateUnknown params=5 @part=1"), std::string::npos);

    sema::CheckedModule trait_checked;
    const TypeHandle trait_i32 = trait_checked.types.builtin(BuiltinType::i32);
    const sema::InternedText reader_name = trait_checked.intern_text("Reader");
    const sema::InternedText item_name = trait_checked.intern_text("Item");
    const sema::InternedText read_name = trait_checked.intern_text("read");
    const sema::InternedText fallback_name = trait_checked.intern_text("fallback");

    sema::TraitSignature trait = trait_checked.make_trait_signature();
    trait.name = reader_name;
    trait.name_id = reader_name.id;
    trait.visibility = syntax::Visibility::private_;
    trait.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    trait.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    trait.generic_params.push_back(reader_name.id);
    trait.generic_params.push_back(item_name.id);

    sema::TraitAssociatedTypeRequirement associated_type =
        trait_checked.make_trait_associated_type_requirement();
    associated_type.name = item_name;
    associated_type.name_id = item_name.id;
    trait.associated_types.push_back(associated_type);

    sema::TraitMethodRequirement requirement = trait_checked.make_trait_method_requirement();
    requirement.name = read_name;
    requirement.name_id = read_name.id;
    requirement.return_type = trait_i32;
    requirement.param_types.push_back(trait_i32);
    requirement.is_unsafe = true;
    requirement.is_variadic = true;
    requirement.has_default_body = true;
    requirement.has_borrow_contract = true;
    requirement.borrow_contract.source = sema::FunctionBorrowContractSource::declared;
    requirement.borrow_contract.unknown_return_allowed = true;
    requirement.borrow_contract.return_selectors.push_back(sema::BorrowContractSelector{
        .kind = sema::BorrowContractSelectorKind::parameter,
        .param_index = 0U,
        .name_id = read_name.id,
        .range = {},
    });
    trait.requirements.push_back(requirement);

    const sema::ModuleLookupKey reader_key{SEMA_TEST_ROOT_MODULE_INDEX, reader_name.id};
    trait_checked.traits.emplace(reader_key, std::move(trait));

    sema::TraitImplInfo trait_impl = trait_checked.make_trait_impl_info();
    trait_impl.trait_name = reader_name;
    trait_impl.trait_name_id = reader_name.id;
    trait_impl.trait_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    trait_impl.self_type = trait_i32;
    trait_impl.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    trait_impl.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    trait_impl.key = sema::TraitImplLookupKey{SEMA_TEST_ROOT_MODULE_INDEX, reader_name.id, trait_i32.value, {}};

    sema::TraitImplAssociatedTypeInfo associated_impl = trait_checked.make_trait_impl_associated_type_info();
    associated_impl.name = item_name;
    associated_impl.name_id = item_name.id;
    associated_impl.value_type = trait_i32;
    associated_impl.requirement_ordinal = 0U;
    trait_impl.associated_types.push_back(associated_impl);

    sema::TraitImplMethodInfo override_method = trait_checked.make_trait_impl_method_info();
    override_method.name = read_name;
    override_method.name_id = read_name.id;
    override_method.requirement_ordinal = 0U;
    override_method.origin = sema::TraitImplMethodOrigin::impl_override;
    trait_impl.methods.push_back(override_method);

    sema::TraitImplMethodInfo default_method = trait_checked.make_trait_impl_method_info();
    default_method.name = fallback_name;
    default_method.name_id = fallback_name.id;
    default_method.requirement_ordinal = 1U;
    default_method.origin = sema::TraitImplMethodOrigin::trait_default;
    trait_impl.methods.push_back(default_method);

    sema::TraitImplMethodInfo invalid_origin_method = trait_checked.make_trait_impl_method_info();
    invalid_origin_method.name = trait_checked.intern_text("invalid_origin");
    invalid_origin_method.name_id = invalid_origin_method.name.id;
    invalid_origin_method.requirement_ordinal = 2U;
    invalid_origin_method.origin =
        static_cast<sema::TraitImplMethodOrigin>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    trait_impl.methods.push_back(invalid_origin_method);

    trait_checked.trait_impls.emplace(trait_impl.key, std::move(trait_impl));

    const std::string trait_dump = sema::dump_checked_module(trait_checked);
    EXPECT_NE(trait_dump.find("trait priv Reader[T0, T1] params=2 associated_types=1 requirements=1 @part=1"),
        std::string::npos);
    EXPECT_NE(trait_dump.find("assoc_type Item"), std::string::npos);
    EXPECT_NE(trait_dump.find(
                  "requirement unsafe read(i32) -> i32 variadic default borrow_contract=declared/selectors=1/unknown=true"),
        std::string::npos);
    EXPECT_NE(trait_dump.find("impl Reader for i32 associated_types=1 methods=3 @part=1"), std::string::npos);
    EXPECT_NE(trait_dump.find("assoc_type Item = i32 requirement=0"), std::string::npos);
    EXPECT_NE(trait_dump.find("method read requirement=0 origin=impl_override"), std::string::npos);
    EXPECT_NE(trait_dump.find("method fallback requirement=1 origin=trait_default"), std::string::npos);
    EXPECT_NE(trait_dump.find("method invalid_origin requirement=2 origin=impl_override"), std::string::npos);

    sema::CheckedModule fallback_checked;
    const TypeHandle fallback_i32 = fallback_checked.types.builtin(BuiltinType::i32);
    const TypeHandle fallback_struct_type =
        fallback_checked.types.named_struct("DumpStruct", "DumpStruct", false);
    const TypeHandle fallback_enum_type = fallback_checked.types.named_enum("DumpEnum", "DumpEnum");
    const sema::InternedText predicate_trait = fallback_checked.intern_text("DumpTrait");
    const sema::InternedText function_name = fallback_checked.intern_text("dump_function");
    const std::string invalid_lookup_key = std::to_string(SEMA_TEST_ROOT_MODULE_INDEX) + ":"
        + std::to_string(sema::SEMA_LOOKUP_INVALID_KEY_PART) + ":-";
    const sema::FunctionLookupKey invalid_function_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        sema::INVALID_IDENT_ID,
    };

    sema::TraitPredicate predicate = fallback_checked.make_trait_predicate();
    predicate.index = 0U;
    predicate.kind = static_cast<sema::TraitPredicateKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    predicate.origin = static_cast<sema::TraitPredicateOrigin>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    predicate.subject_type = fallback_i32;
    predicate.trait_name = predicate_trait;
    predicate.trait_name_id = predicate_trait.id;
    predicate.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.trait_predicates.push_back(predicate);

    sema::TraitEvidence evidence = fallback_checked.make_trait_evidence();
    evidence.kind = static_cast<sema::TraitEvidenceKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    evidence.predicate_index = 0U;
    fallback_checked.trait_evidence.push_back(evidence);

    sema::TraitMethodCallBinding trait_call = fallback_checked.make_trait_method_call_binding();
    trait_call.dispatch = static_cast<sema::TraitMethodDispatchKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    trait_call.self_type = fallback_i32;
    trait_call.return_type = fallback_i32;
    trait_call.receiver_access = static_cast<sema::ReceiverAccessKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    trait_call.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.append_trait_method_call_binding(trait_call);

    sema::FunctionCallBinding function_call = fallback_checked.make_function_call_binding();
    function_call.call_expr = ExprId{SEMA_TEST_PATTERN_FIRST_INDEX};
    function_call.function_key = invalid_function_key;
    function_call.return_type = fallback_i32;
    function_call.receiver_access = sema::ReceiverAccessKind::shared;
    function_call.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.append_function_call_binding(function_call);

    sema::FunctionBorrowSummary borrow_summary;
    borrow_summary.function = invalid_function_key;
    borrow_summary.return_type = fallback_i32;
    borrow_summary.has_unknown_return_origin = true;
    borrow_summary.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.borrow_summaries.emplace(invalid_function_key, borrow_summary);

    sema::FunctionBorrowContract borrow_contract;
    borrow_contract.function = invalid_function_key;
    borrow_contract.return_type = fallback_i32;
    borrow_contract.source =
        static_cast<sema::FunctionBorrowContractSource>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE);
    borrow_contract.unknown_return_allowed = true;
    borrow_contract.has_contract_mismatch = true;
    borrow_contract.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    borrow_contract.return_selectors.push_back(sema::BorrowContractSelector{
        .kind = static_cast<sema::BorrowContractSelectorKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE),
        .param_index = SEMA_TEST_PATTERN_FIRST_INDEX,
        .name_id = sema::INVALID_IDENT_ID,
        .range = {},
    });
    fallback_checked.borrow_contracts.emplace(invalid_function_key, borrow_contract);

    sema::ReferenceOriginFact reference_origin;
    reference_origin.syntax_type = TypeId{SEMA_TEST_PATTERN_FIRST_INDEX};
    reference_origin.semantic_type = fallback_i32;
    reference_origin.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.reference_origin_facts.push_back(reference_origin);

    sema::TypeLifetimeInfo type_lifetime;
    type_lifetime.type = fallback_i32;
    type_lifetime.can_contain_borrow = true;
    type_lifetime.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.type_lifetime_infos.push_back(type_lifetime);

    sema::FunctionLifetimeFacts lifetime_facts;
    lifetime_facts.function = invalid_function_key;
    lifetime_facts.return_type = fallback_i32;
    lifetime_facts.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    lifetime_facts.regions.push_back(sema::LifetimeRegion{
        .kind = sema::LifetimeRegionKind::inferred,
        .name_id = function_name.id,
        .name = {},
        .param_index = SEMA_TEST_PATTERN_FIRST_INDEX,
        .range = {},
    });
    lifetime_facts.regions.push_back(sema::LifetimeRegion{
        .kind = sema::LifetimeRegionKind::unknown,
        .name_id = sema::INVALID_IDENT_ID,
        .name = {},
        .param_index = SEMA_TEST_PATTERN_SECOND_INDEX,
        .range = {},
    });
    fallback_checked.lifetime_facts.emplace(invalid_function_key, lifetime_facts);

    sema::FunctionDropCheckFacts dropck_facts;
    dropck_facts.function = invalid_function_key;
    dropck_facts.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    dropck_facts.graph_missing = true;
    fallback_checked.dropck_facts.emplace(invalid_function_key, dropck_facts);

    sema::BodyLoanCheckResult loan_result;
    loan_result.function = invalid_function_key;
    loan_result.diagnostic_mode = sema::BodyLoanDiagnosticMode::enforced;
    loan_result.graph_missing = true;
    fallback_checked.body_loan_checks.emplace(invalid_function_key, loan_result);

    sema::ParamEnvInfo param_env = fallback_checked.make_param_env_info();
    param_env.owner_name = fallback_checked.intern_text("DumpParamEnv");
    param_env.owner_name_id = param_env.owner_name.id;
    param_env.predicate_indices.push_back(SEMA_TEST_PATTERN_FIRST_INDEX);
    param_env.part_index = SEMA_TEST_TEMPLATE_PART_INDEX;
    fallback_checked.param_envs.push_back(param_env);

    FunctionSignature export_signature = fallback_checked.make_function_signature();
    export_signature.name = function_name;
    export_signature.name_id = function_name.id;
    export_signature.c_name = function_name;
    export_signature.return_type = fallback_i32;
    export_signature.is_export_c = true;
    export_signature.semantic_key = sema::FunctionLookupKey{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_name.id,
    };
    fallback_checked.functions.emplace(export_signature.semantic_key, std::move(export_signature));

    StructInfo opaque_struct = fallback_checked.make_struct_info();
    opaque_struct.name = fallback_checked.intern_text("DumpStruct");
    opaque_struct.name_id = opaque_struct.name.id;
    opaque_struct.c_name = opaque_struct.name;
    opaque_struct.type = fallback_struct_type;
    opaque_struct.is_opaque = true;
    fallback_checked.structs.emplace(
        sema::ModuleLookupKey{SEMA_TEST_ROOT_MODULE_INDEX, opaque_struct.name_id}, std::move(opaque_struct));

    EnumCaseInfo payload_case = fallback_checked.make_enum_case_info();
    payload_case.enum_name = fallback_checked.intern_text("DumpEnum");
    payload_case.case_name = fallback_checked.intern_text("single");
    payload_case.name = payload_case.case_name;
    payload_case.name_id = payload_case.case_name.id;
    payload_case.c_name = fallback_checked.intern_text("DumpEnum_single");
    payload_case.type = fallback_enum_type;
    payload_case.payload_type = fallback_i32;
    fallback_checked.enum_cases.emplace(
        sema::ModuleLookupKey{SEMA_TEST_ROOT_MODULE_INDEX, payload_case.name_id}, std::move(payload_case));

    const std::string fallback_dump = sema::dump_checked_module(fallback_checked);
    EXPECT_NE(fallback_dump.find("predicate #0 trait i32: DumpTrait origin=where @part=1"), std::string::npos);
    EXPECT_NE(fallback_dump.find("evidence #0 param_env predicate=0"), std::string::npos);
    EXPECT_NE(fallback_dump.find("trait_call #0 param_env i32.<invalid> -> i32"), std::string::npos);
    EXPECT_NE(fallback_dump.find("receiver_access=<invalid>"), std::string::npos);
    EXPECT_NE(fallback_dump.find("function_call #0 expr=e0 -> " + invalid_lookup_key), std::string::npos);
    EXPECT_NE(fallback_dump.find("borrow_summary " + invalid_lookup_key), std::string::npos);
    EXPECT_NE(fallback_dump.find("borrow_contract " + invalid_lookup_key + " source=<invalid>"),
        std::string::npos);
    EXPECT_NE(fallback_dump.find("selector #0 <invalid> param=0 name=-"), std::string::npos);
    EXPECT_NE(fallback_dump.find("reference_origin #0 t0 i32 origins=- @part=1"), std::string::npos);
    EXPECT_NE(fallback_dump.find("type_lifetime #0 i32 can_borrow=true concrete=false origins=-"),
        std::string::npos);
    EXPECT_NE(fallback_dump.find("lifetime_fact " + invalid_lookup_key), std::string::npos);
    EXPECT_NE(fallback_dump.find("region #0 inferred param=0 name=#"), std::string::npos);
    EXPECT_NE(fallback_dump.find("region #1 unknown param=1 name=-"), std::string::npos);
    EXPECT_NE(fallback_dump.find("dropck_fact " + invalid_lookup_key), std::string::npos);
    EXPECT_NE(fallback_dump.find("body_loan_check " + invalid_lookup_key + " mode=enforced"),
        std::string::npos);
    EXPECT_NE(fallback_dump.find("param_env DumpParamEnv predicates=1"), std::string::npos);
    EXPECT_NE(fallback_dump.find("fn dump_function -> i32 export_c"), std::string::npos);
    EXPECT_NE(fallback_dump.find("struct DumpStruct opaque @part=0 fields=0"), std::string::npos);
    EXPECT_NE(fallback_dump.find("case DumpEnum_single : DumpEnum(i32)"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxRecordsOriginParamsAndReferenceFacts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_ORIGIN_FACT_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    syntax::TypeNode reference_type = reference_node(i32_type);
    reference_type.reference_origin.explicit_ = true;
    reference_type.reference_origin.names = {SEMA_TEST_ORIGIN_FACT_PARAM_NAME};
    const TypeId explicit_origin_ref = module.push_type(std::move(reference_type));

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_ORIGIN_FACT_FUNCTION_NAME;
    function.generic_params = {syntax::GenericParamDecl{
        SEMA_TEST_ORIGIN_FACT_PARAM_NAME,
        {},
        syntax::INVALID_IDENT_ID,
        syntax::GenericParamKind::origin,
    }};
    function.return_type = explicit_origin_ref;
    function.is_prototype = true;
    function.is_extern_c = true;
    const syntax::ItemId function_id =
        module.push_item_for_module(function, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_ORIGIN_FACT_PART_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    ASSERT_EQ(checked.lifetime_origin_params.size(), 1U);
    const sema::LifetimeOriginParamInfo& origin_param = checked.lifetime_origin_params.front();
    EXPECT_EQ(origin_param.module.value, SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_EQ(origin_param.item.value, function_id.value);
    EXPECT_EQ(origin_param.name, SEMA_TEST_ORIGIN_FACT_PARAM_NAME);
    EXPECT_TRUE(sema::is_valid(origin_param.name_id));
    EXPECT_EQ(origin_param.ordinal, 0U);
    EXPECT_EQ(origin_param.part_index, SEMA_TEST_ORIGIN_FACT_PART_INDEX);

    ASSERT_EQ(checked.reference_origin_facts.size(), 1U);
    const sema::ReferenceOriginFact& origin_fact = checked.reference_origin_facts.front();
    EXPECT_EQ(origin_fact.module.value, SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_EQ(origin_fact.item.value, function_id.value);
    EXPECT_EQ(origin_fact.syntax_type.value, explicit_origin_ref.value);
    EXPECT_TRUE(sema::is_valid(origin_fact.semantic_type));
    EXPECT_EQ(checked.types.display_name(origin_fact.semantic_type), SEMA_TEST_ORIGIN_FACT_TYPE_DISPLAY);
    ASSERT_EQ(origin_fact.origin_names.size(), 1U);
    EXPECT_EQ(origin_fact.origin_names.front(), SEMA_TEST_ORIGIN_FACT_PARAM_NAME);
    ASSERT_EQ(origin_fact.origin_name_ids.size(), 1U);
    EXPECT_EQ(origin_fact.origin_name_ids.front(), origin_param.name_id);
    EXPECT_EQ(origin_fact.part_index, SEMA_TEST_ORIGIN_FACT_PART_INDEX);

    EXPECT_GT(checked.type_lifetime_infos.size(), 0U);
    EXPECT_GT(checked.generic_lifetime_predicates.size(), 0U);
    const bool has_explicit_predicate = std::ranges::any_of(
        checked.generic_lifetime_predicates, [](const sema::GenericLifetimePredicate& predicate) {
            return predicate.source == sema::GenericLifetimePredicateSource::explicit_origin
                && predicate.origin_name.view() == SEMA_TEST_ORIGIN_FACT_PARAM_NAME;
        });
    EXPECT_TRUE(has_explicit_predicate);

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("lifetime_origin_params 1"), std::string::npos);
    EXPECT_NE(checked_dump.find("origin_param #0 data ordinal=0"), std::string::npos);
    EXPECT_NE(checked_dump.find("reference_origin_facts 1"), std::string::npos);
    EXPECT_NE(checked_dump.find("reference_origin #0"), std::string::npos);
    EXPECT_NE(checked_dump.find("origins=data"), std::string::npos);
    EXPECT_NE(checked_dump.find("type_lifetime_infos"), std::string::npos);
    EXPECT_NE(checked_dump.find("generic_lifetime_predicates"), std::string::npos);
    EXPECT_NE(checked_dump.find("source=explicit_origin"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxLookupsAndMethodReceivers)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(1), "one"),
        resolved_import(module_id(2), "two"),
    };
    const ExprId value_expr = push_name(module, "value");
    const ExprId literal_expr = push_integer(module);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle array_record = types.named_struct("ArrayRecord", "ArrayRecord", true);
    types.set_record_contains_array(array_record, true);
    const TypeHandle ptr_record = types.pointer(PointerMutability::mut, record_type);
    const TypeHandle const_ptr_record = types.pointer(PointerMutability::const_, record_type);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");

    const IdentId shared_id = intern_identifier(analyzer, "Shared");
    const IdentId private_id = intern_identifier(analyzer, "Private");
    const IdentId ambiguous_id = intern_identifier(analyzer, "ambiguous");
    const IdentId hidden_id = intern_identifier(analyzer, "hidden");
    const IdentId missing_id = intern_identifier(analyzer, "missing");
    const IdentId same_id = intern_identifier(analyzer, "same");
    const IdentId private_value_id = intern_identifier(analyzer, "private_value");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId static_only_id = intern_identifier(analyzer, "static_only");
    const IdentId free_id = intern_identifier(analyzer, "free");

    static_cast<void>(add_global_value(analyzer, module_id(0), "value", record_type, SymbolKind::local, true));

    static_cast<void>(add_named_type(analyzer, module_id(1), "Shared", i32));
    static_cast<void>(add_named_type(analyzer, module_id(2), "Shared", bool_type));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_visible_modules(shared_id, "Shared", {}, false)));

    static_cast<void>(add_named_type(analyzer, module_id(1), "Private", record_type, syntax::Visibility::private_));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(1), private_id, "Private", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(syntax::INVALID_MODULE_ID, missing_id, "Missing", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(1), missing_id, "Missing", {}, false)));

    FunctionSignature one = indexed_function_signature(analyzer, "ambiguous", module_id(1), i32);
    FunctionSignature two = indexed_function_signature(analyzer, "ambiguous", module_id(2), i32);
    static_cast<void>(add_function(analyzer, one));
    static_cast<void>(add_function(analyzer, two));
    EXPECT_EQ(analyzer.find_function_in_visible_modules(ambiguous_id, "ambiguous", {}), nullptr);
    FunctionSignature private_function = indexed_function_signature(analyzer, "hidden", module_id(1), i32);
    private_function.visibility = syntax::Visibility::private_;
    static_cast<void>(add_function(analyzer, private_function));
    EXPECT_EQ(analyzer.find_function_in_module(module_id(1), hidden_id, "hidden", {}), nullptr);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(1), missing_id, "missing", {}), nullptr);
    EXPECT_EQ(analyzer.find_function_in_module(syntax::INVALID_MODULE_ID, missing_id, "missing", {}), nullptr);

    EnumCaseInfo one_case;
    one_case.name = checked_text(analyzer.state_.checked, "same");
    one_case.name_id = same_id;
    one_case.case_name = checked_text(analyzer.state_.checked, "same");
    one_case.case_name_id = same_id;
    one_case.module = module_id(1);
    one_case.type = enum_type;
    EnumCaseInfo two_case = one_case;
    two_case.module = module_id(2);
    auto one_case_inserted =
        analyzer.state_.checked.enum_cases.emplace(semantic_module_key(analyzer, module_id(1), "same"), one_case);
    auto two_case_inserted =
        analyzer.state_.checked.enum_cases.emplace(semantic_module_key(analyzer, module_id(2), "same"), two_case);
    analyzer.index_enum_case(one_case_inserted.first->second);
    analyzer.index_enum_case(two_case_inserted.first->second);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(same_id, "same", {}), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(missing_id, "missing", {}), nullptr);

    static_cast<void>(add_named_type(analyzer, module_id(0), "Record", record_type));
    static_cast<void>(add_named_type(analyzer, module_id(0), "Choice", enum_type));
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
                  intern_identifier(analyzer, "Record"), "Record", missing_id, "missing", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
                  intern_identifier(analyzer, "Choice"), "Choice", missing_id, "missing", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_enum_constructor(syntax::INVALID_EXPR_ID, true), nullptr);

    static_cast<void>(add_global_value(analyzer, module_id(1), "ambiguous", i32));
    static_cast<void>(add_global_value(analyzer, module_id(2), "ambiguous", i32));
    EXPECT_EQ(analyzer.find_symbol(ambiguous_id, "ambiguous", {}), nullptr);
    EXPECT_EQ(analyzer.find_symbol(missing_id, "missing", {}), nullptr);
    static_cast<void>(add_global_value(
        analyzer, module_id(1), "private_value", i32, SymbolKind::const_, false, syntax::Visibility::private_));
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(1), missing_id, "missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(1), private_value_id, "private_value", {}, true), nullptr);

    FunctionSignature no_self;
    EXPECT_FALSE(analyzer.method_receiver_matches(no_self, record_type, value_expr));
    FunctionSignature by_value;
    by_value.has_self_param = true;
    by_value.param_types = {array_record};
    EXPECT_FALSE(analyzer.method_receiver_matches(by_value, array_record, value_expr));
    FunctionSignature plain_self;
    plain_self.has_self_param = true;
    plain_self.param_types = {record_type};
    EXPECT_TRUE(analyzer.method_receiver_matches(plain_self, record_type, value_expr));
    FunctionSignature pointer_self;
    pointer_self.has_self_param = true;
    pointer_self.param_types = {ptr_record};
    EXPECT_TRUE(analyzer.method_receiver_matches(pointer_self, ptr_record, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, const_ptr_record, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, record_type, literal_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(plain_self, bool_type, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, bool_type, value_expr));

    FunctionSignature method = indexed_function_signature(analyzer, "run", module_id(1), i32);
    method.is_method = true;
    method.has_self_param = true;
    method.method_owner_type = record_type;
    FunctionSignature other_method = method;
    other_method.module = module_id(2);
    static_cast<void>(add_method(analyzer, method, record_type));
    static_cast<void>(add_method(analyzer, other_method, record_type));
    FunctionSignature static_method = indexed_function_signature(analyzer, "static_only", module_id(1), i32);
    static_method.is_method = true;
    static_method.method_owner_type = record_type;
    static_cast<void>(add_method(analyzer, static_method, record_type));
    FunctionSignature not_method = indexed_function_signature(analyzer, "free", module_id(1), i32);
    not_method.method_owner_type = record_type;
    analyzer.state_.checked.functions.emplace(
        semantic_method_key(analyzer, module_id(1), record_type, "free"), not_method);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, run_id, "run", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, static_only_id, "static_only", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, free_id, "free", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, missing_id, "missing", {}, true), nullptr);
}

TEST(CoreUnit, SemanticWhiteBoxModuleVisibilityResolverEdges)
{
    constexpr base::u32 SEMA_TEST_CORE_MODULE_INDEX = 3;
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
        module_info({"core"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
        resolved_import(module_id(2), ""),
        resolved_import(module_id(SEMA_TEST_MISSING_MODULE_INDEX), "ghost", syntax::Visibility::public_),
    };
    module.modules[SEMA_TEST_LIB_ONE_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_CORE_MODULE_INDEX), "core", syntax::Visibility::package_),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    EXPECT_EQ(analyzer.resolve_import_alias("lib", {}).value, SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_EQ(analyzer.resolve_import_alias("ghost", {}).value, SEMA_TEST_MISSING_MODULE_INDEX);
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("libs", {})));
    EXPECT_FALSE(analyzer.module_alias_visible("missing"));
    EXPECT_FALSE(syntax::is_valid(analyzer.find_visible_module_path({})));

    analyzer.state_.modules.visible_modules_cache.clear();
    const auto& visible = analyzer.visible_modules(module_id(SEMA_TEST_ROOT_MODULE_INDEX));
    EXPECT_NE(std::find_if(visible.begin(), visible.end(),
                  [](const syntax::ModuleId candidate_module) {
                      return candidate_module.value == SEMA_TEST_MISSING_MODULE_INDEX;
                  }),
        visible.end());
    EXPECT_NE(std::find_if(visible.begin(), visible.end(),
                  [](const syntax::ModuleId candidate_module) {
                      return candidate_module.value == SEMA_TEST_CORE_MODULE_INDEX;
                  }),
        visible.end());
    const auto& public_exports = analyzer.module_export_modules(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX));
    EXPECT_EQ(std::find_if(public_exports.begin(), public_exports.end(),
                  [](const syntax::ModuleId candidate_module) {
                      return candidate_module.value == SEMA_TEST_CORE_MODULE_INDEX;
                  }),
        public_exports.end());
    const auto accessible_exports =
        analyzer.accessible_module_export_modules(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX));
    EXPECT_NE(std::find_if(accessible_exports.begin(), accessible_exports.end(),
                  [](const syntax::ModuleId candidate_module) {
                      return candidate_module.value == SEMA_TEST_CORE_MODULE_INDEX;
                  }),
        accessible_exports.end());
    EXPECT_FALSE(analyzer.visible_root_module_name_exists("ghost"));
    EXPECT_FALSE(syntax::is_valid(analyzer.find_visible_module_path({"ghost", "child"})));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({"ghost", "child", "leaf"}));

    analyzer.state_.flow.current_module = module_id(SEMA_TEST_MISSING_MODULE_INDEX);
    EXPECT_FALSE(analyzer.module_alias_visible("ghost"));
    EXPECT_FALSE(analyzer.visible_root_module_name_exists("root"));
    EXPECT_FALSE(syntax::is_valid(analyzer.find_visible_module_path({"root"})));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({"root", "child"}));
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("ghost", {}, false)));
}

TEST(CoreUnit, SemanticWhiteBoxDotOnlyModuleSelectorAndShadowingEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"app"}),
        module_info({"lib"}),
        module_info({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME}),
        module_info({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME, SEMA_TEST_LEAF_MODULE_NAME}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
        resolved_import(module_id(2), "mem"),
        resolved_import(module_id(3), "io"),
    };

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::fn_decl;
    item.name = "owned";
    const syntax::ItemId item_id = module.push_item(item);
    module.item_modules[item_id.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle local_type = types.named_struct("LocalType", "LocalType", false);
    const IdentId local_type_id = intern_identifier(analyzer, "LocalType");
    const IdentId global_id = intern_identifier(analyzer, "global");
    const IdentId lib_id = intern_identifier(analyzer, "lib");
    const IdentId root_module_id = intern_identifier(analyzer, SEMA_TEST_ROOT_MODULE_NAME);
    const IdentId fresh_id = intern_identifier(analyzer, "fresh");
    const IdentId t_id = intern_identifier(analyzer, "T");
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "LocalType", local_type));
    static_cast<void>(add_global_value(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "global", i32));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "local", module_id(SEMA_TEST_ROOT_MODULE_INDEX), i32),
        diagnostics));

    const ExprId alias_expr = push_name(analyzer.ctx_.module, "lib");
    const ExprId root_expr = push_name(analyzer.ctx_.module, SEMA_TEST_ROOT_MODULE_NAME);
    const ExprId core_mem = push_field(analyzer.ctx_.module, root_expr, SEMA_TEST_CHILD_MODULE_NAME);
    const ExprId core_mem_io = push_field(analyzer.ctx_.module, core_mem, SEMA_TEST_LEAF_MODULE_NAME);
    const ExprId invalid_expr = push_name(analyzer.ctx_.module, "missing");
    const ExprId missing_root_child =
        push_field(analyzer.ctx_.module, push_name(analyzer.ctx_.module, SEMA_TEST_ROOT_MODULE_NAME), "missing");
    const ExprId core_mem_file = push_field(analyzer.ctx_.module, core_mem, "File");
    const ExprId local_member = push_field(analyzer.ctx_.module, push_name(analyzer.ctx_.module, "local"), "member");
    const ExprId alias_member = push_field(analyzer.ctx_.module, push_name(analyzer.ctx_.module, "mem"), "File");
    const ExprId empty_field = push_field(analyzer.ctx_.module, push_name(analyzer.ctx_.module, "lib"), {});
    const ExprId scoped_name = push_name(analyzer.ctx_.module, "Name", "scope");
    const ExprId not_selector = push_integer(analyzer.ctx_.module);
    analyzer.state_.checked.expr_types.assign(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(analyzer.ctx_.module.exprs.size(), sema::INVALID_IDENT_ID);

    const sema::SemanticAnalyzerCore::ModuleSelectorPath invalid_path =
        analyzer.expr_selector_path(syntax::INVALID_EXPR_ID);
    EXPECT_TRUE(invalid_path.parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(empty_field).parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(scoped_name).parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(not_selector).parts.empty());

    const sema::SemanticAnalyzerCore::ModuleSelectorPath core_mem_io_path = analyzer.expr_selector_path(core_mem_io);
    ASSERT_EQ(core_mem_io_path.parts.size(), 3U);
    EXPECT_EQ(core_mem_io_path.parts[0], SEMA_TEST_ROOT_MODULE_NAME);
    EXPECT_EQ(core_mem_io_path.parts[1], SEMA_TEST_CHILD_MODULE_NAME);
    EXPECT_EQ(core_mem_io_path.parts[2], SEMA_TEST_LEAF_MODULE_NAME);

    const sema::SemanticAnalyzerCore::ModuleSelector alias_selector =
        analyzer.resolve_module_selector(alias_expr, true);
    EXPECT_EQ(alias_selector.module.value, SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_FALSE(alias_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector root_selector = analyzer.resolve_module_selector(core_mem, true);
    EXPECT_EQ(root_selector.module.value, 2U);
    EXPECT_FALSE(root_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector leaf_selector =
        analyzer.resolve_module_selector(core_mem_io, true);
    EXPECT_EQ(leaf_selector.module.value, 3U);
    EXPECT_FALSE(leaf_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector unknown_alias_selector =
        analyzer.resolve_module_selector(invalid_expr, true);
    EXPECT_FALSE(syntax::is_valid(unknown_alias_selector.module));
    EXPECT_TRUE(unknown_alias_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector missing_child_selector =
        analyzer.resolve_module_selector(missing_root_child, true);
    EXPECT_FALSE(syntax::is_valid(missing_child_selector.module));
    EXPECT_TRUE(missing_child_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector prefix_member_selector =
        analyzer.resolve_module_selector(core_mem_file, true);
    EXPECT_FALSE(syntax::is_valid(prefix_member_selector.module));
    EXPECT_FALSE(prefix_member_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector local_selector =
        analyzer.resolve_module_selector(local_member, true);
    EXPECT_FALSE(syntax::is_valid(local_selector.module));
    EXPECT_FALSE(local_selector.failed_as_module_selector);

    const sema::SemanticAnalyzerCore::ModuleSelector alias_member_selector =
        analyzer.resolve_module_selector(alias_member, true);
    EXPECT_FALSE(syntax::is_valid(alias_member_selector.module));
    EXPECT_FALSE(alias_member_selector.failed_as_module_selector);

    EXPECT_TRUE(analyzer.visible_root_module_name_exists(SEMA_TEST_ROOT_MODULE_NAME));
    EXPECT_FALSE(analyzer.visible_root_module_name_exists({}));
    EXPECT_TRUE(
        analyzer.visible_module_path_prefix_exists({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME, "File"}));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({SEMA_TEST_ROOT_MODULE_NAME}));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({"missing", "path"}));

    EXPECT_FALSE(analyzer.can_define_local_name(lib_id, "lib", {}));
    EXPECT_FALSE(analyzer.can_define_local_name(root_module_id, SEMA_TEST_ROOT_MODULE_NAME, {}));
    EXPECT_FALSE(analyzer.can_define_local_name(local_type_id, "LocalType", {}));
    EXPECT_TRUE(analyzer.can_define_local_name(fresh_id, "fresh", {}));

    sema::SemanticAnalyzerCore::GenericContext generic_context;
    generic_context.params.emplace(t_id, bool_type);
    analyzer.state_.flow.current_generic_context = &generic_context;
    EXPECT_TRUE(analyzer.current_generic_param_exists(t_id, "T"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(t_id, "T"));
    EXPECT_FALSE(analyzer.can_define_local_name(t_id, "T", {}));
    analyzer.state_.flow.current_generic_context = nullptr;

    const IdentId missing_id = intern_identifier(analyzer, "missing");
    EXPECT_TRUE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), global_id, "global"));
    EXPECT_FALSE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), missing_id, "missing"));
    EXPECT_TRUE(
        analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), local_type_id, "LocalType"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), global_id, "global"));
    EXPECT_FALSE(
        analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), missing_id, "missing"));

    EXPECT_EQ(analyzer.item_module(item_id).value, SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_FALSE(syntax::is_valid(analyzer.item_module(syntax::ItemId{99})));
    analyzer.ctx_.module.item_modules.clear();
    EXPECT_FALSE(syntax::is_valid(analyzer.item_module(item_id)));
}

TEST(CoreUnit, SemanticWhiteBoxTypedLookupRejectsAmbiguousPublicReexports)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(1), "one", syntax::Visibility::public_),
        resolved_import(module_id(2), "two", syntax::Visibility::public_),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle one_type = types.named_struct("lib.one.Shared", "lib_one_Shared", false);
    const TypeHandle two_type = types.named_struct("lib.two.Shared", "lib_two_Shared", false);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const IdentId shared_id = intern_identifier(analyzer, "Shared");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId value_id = intern_identifier(analyzer, "VALUE");

    static_cast<void>(add_named_type(analyzer, module_id(1), "Shared", one_type));
    static_cast<void>(add_named_type(analyzer, module_id(2), "Shared", two_type));
    EXPECT_FALSE(
        is_valid(analyzer.find_type_in_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), shared_id, "Shared", {}, false)));

    static_cast<void>(add_function(analyzer, indexed_function_signature(analyzer, "run", module_id(1), i32)));
    static_cast<void>(add_function(analyzer, indexed_function_signature(analyzer, "run", module_id(2), i32)));
    EXPECT_EQ(analyzer.find_function_in_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), run_id, "run", {}), nullptr);

    static_cast<void>(add_global_value(analyzer, module_id(1), "VALUE", i32));
    static_cast<void>(add_global_value(analyzer, module_id(2), "VALUE", i32));
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), value_id, "VALUE", {}), nullptr);
}

TEST(CoreUnit, SemanticWhiteBoxFunctionAndEnumLookupFallbackEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"owner"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "owner"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle array_i32 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i32);
    const TypeHandle fn_type = types.function(sema::FunctionCallConv::aurex, false, false, {i32}, bool_type);
    const TypeHandle unsafe_fn_type = types.function(sema::FunctionCallConv::aurex, true, false, {}, bool_type);

    FunctionSignature callable =
        indexed_function_signature(analyzer, "callable", module_id(SEMA_TEST_ROOT_MODULE_INDEX), bool_type);
    callable.param_types = {i32};
    static_cast<void>(add_function(analyzer, callable));
    const Symbol callable_symbol = indexed_symbol(
        analyzer, SymbolKind::function, "callable", module_id(SEMA_TEST_ROOT_MODULE_INDEX), INVALID_TYPE_HANDLE);
    EXPECT_TRUE(types.is_function(analyzer.function_type_from_symbol(callable_symbol, {})));

    const Symbol fallback_symbol =
        indexed_symbol(analyzer, SymbolKind::local, "local_fn", syntax::INVALID_MODULE_ID, fn_type);
    EXPECT_TRUE(types.same(analyzer.function_type_from_symbol(fallback_symbol, {}), fn_type));

    analyzer.validate_unsafe_function_value_call(i32, {});
    analyzer.validate_unsafe_function_value_call(unsafe_fn_type, {});
    EXPECT_TRUE(diagnostics.has_error());
    analyzer.state_.flow.unsafe_context_depth += 1;
    analyzer.validate_unsafe_function_value_call(unsafe_fn_type, {});
    analyzer.state_.flow.unsafe_context_depth -= 1;

    EXPECT_FALSE(analyzer.find_enum_cases_by_type(INVALID_TYPE_HANDLE));
    EnumCaseInfo invalid_case;
    invalid_case.name = checked_text(analyzer.state_.checked, "invalid");
    invalid_case.case_name = checked_text(analyzer.state_.checked, "invalid");
    invalid_case.type = INVALID_TYPE_HANDLE;
    analyzer.index_enum_case(invalid_case);
    EXPECT_TRUE(analyzer.state_.names.enum_cases_by_type.empty());

    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");
    types.set_enum_underlying(enum_type, i32);
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Record", record_type));
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Choice", enum_type));
    const EnumCaseInfo* const yes_case = add_enum_case(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Choice_yes",
        "yes", enum_type, INVALID_TYPE_HANDLE, {array_i32})
                                             .second;
    ASSERT_NE(yes_case, nullptr);
    ASSERT_NE(analyzer.find_enum_cases_by_type(enum_type), nullptr);
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId case_id = intern_identifier(analyzer, "case");
    const IdentId record_id = intern_identifier(analyzer, "Record");
    const IdentId missing_id = intern_identifier(analyzer, "missing");
    const IdentId missing_enum_id = intern_identifier(analyzer, "Missing");
    const base::usize interned_cases = analyzer.ctx_.module.identifiers.size();
    const EnumCaseInfo* indexed_yes = analyzer.find_enum_case_by_type_and_case(enum_type, yes_id, "yes");
    ASSERT_NE(indexed_yes, nullptr);
    EXPECT_EQ(indexed_yes->case_name, "yes");
    EXPECT_EQ(analyzer.find_enum_case_by_type_and_case(record_type, yes_id, "yes"), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_type_and_case(enum_type, missing_id, "missing"), nullptr);
    EXPECT_EQ(analyzer.ctx_.module.identifiers.size(), interned_cases);
    EXPECT_EQ(analyzer.state_.names.enum_cases_by_type_and_case.size(), 1U);
    EXPECT_TRUE(sema::is_valid(analyzer.ctx_.module.find_identifier("yes")));

    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(missing_enum_id, "Missing", case_id, "case", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(record_id, "Record", case_id, "case", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
                  intern_identifier(analyzer, "Choice"), "Choice", yes_id, "yes", {}, true),
        yes_case);

    const ExprId record_name = push_name(analyzer.ctx_.module, "Record");
    const ExprId record_case = push_field(analyzer.ctx_.module, record_name, "case");
    const ExprId choice_name = push_name(analyzer.ctx_.module, "Choice");
    const ExprId choice_missing = push_field(analyzer.ctx_.module, choice_name, "missing");
    const ExprId choice_yes = push_field(analyzer.ctx_.module, choice_name, "yes");
    const ExprId payload_value = push_name(analyzer.ctx_.module, "payload");
    const ExprId enum_call_id = push_call(analyzer.ctx_.module, choice_yes, {payload_value});

    analyzer.state_.checked.expr_types.assign(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(analyzer.ctx_.module.exprs.size(), sema::INVALID_IDENT_ID);
    static_cast<void>(
        add_global_value(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "payload", array_i32, SymbolKind::local));

    EXPECT_EQ(analyzer.find_enum_constructor(record_case, true), nullptr);
    EXPECT_EQ(analyzer.find_enum_constructor(choice_missing, true), nullptr);
    EXPECT_NE(analyzer.find_enum_constructor(choice_yes, true), nullptr);
    EXPECT_TRUE(types.same(
        analyzer.analyze_enum_constructor_call(enum_call_id, analyzer.expr_view(enum_call_id), *yes_case), enum_type));

    const ExprId argument_call_id = push_call(analyzer.ctx_.module, syntax::INVALID_EXPR_ID, {payload_value});
    const std::vector<TypeHandle> array_param_types{array_i32};
    analyzer.validate_call_arguments(analyzer.expr_view(argument_call_id), "array_arg", array_param_types, 0, false);
    const ExprId variadic_argument_call_id =
        push_call(analyzer.ctx_.module, syntax::INVALID_EXPR_ID, {payload_value, payload_value});
    const std::vector<TypeHandle> no_param_types;
    analyzer.validate_call_arguments(
        analyzer.expr_view(variadic_argument_call_id), "array_vararg", no_param_types, 0, true);
}

TEST(CoreUnit, SemanticWhiteBoxTypedLookupIndexesCoverHotPaths)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle record_type = types.named_struct("lib.Record", "lib_Record", false);
    const TypeHandle enum_type = types.named_enum("lib.Choice", "lib_Choice");
    types.set_enum_underlying(enum_type, i32);

    const IdentId record_id = intern_identifier(analyzer, "Record");
    const IdentId alias_id = intern_identifier(analyzer, "Alias");
    const IdentId box_id = intern_identifier(analyzer, "Box");
    const IdentId make_id = intern_identifier(analyzer, "make");
    const IdentId generic_method_id = intern_identifier(analyzer, "generic_method");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId method_id = intern_identifier(analyzer, "method");
    const IdentId value_id = intern_identifier(analyzer, "VALUE");
    const IdentId choice_yes_id = intern_identifier(analyzer, "Choice_yes");
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId missing_id = intern_identifier(analyzer, "missing");

    const sema::ModuleLookupKey record_key =
        add_named_type(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Record", record_type);
    StructInfo record_info;
    record_info.name = checked_text(analyzer.state_.checked, "Record");
    record_info.name_id = record_id;
    record_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    record_info.type = record_type;
    const auto record_inserted = analyzer.state_.checked.structs.emplace(record_key, record_info);
    ASSERT_TRUE(record_inserted.second);
    analyzer.state_.types.struct_infos_by_type[record_type.value] = &record_inserted.first->second;

    const TypeId i32_type_id = analyzer.ctx_.module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    sema::TypeAliasInfo alias;
    alias.name = checked_text(analyzer.state_.checked, "Alias");
    alias.name_id = alias_id;
    alias.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    alias.target = i32_type_id;
    alias.visibility = syntax::Visibility::public_;
    const auto alias_inserted = analyzer.state_.checked.type_aliases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Alias"), alias);
    ASSERT_TRUE(alias_inserted.second);
    analyzer.index_type_alias(alias_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo generic_type;
    generic_type.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_type.name = checked_text(analyzer.state_.checked, "Box");
    generic_type.name_id = box_id;
    generic_type.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Box");
    generic_type.visibility = syntax::Visibility::public_;
    const auto generic_type_inserted =
        analyzer.state_.generics.struct_templates.emplace(generic_type.key, generic_type);
    ASSERT_TRUE(generic_type_inserted.second);
    analyzer.index_generic_struct_template(generic_type_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo generic_function;
    generic_function.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_function.name = checked_text(analyzer.state_.checked, "make");
    generic_function.name_id = make_id;
    generic_function.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "make");
    generic_function.visibility = syntax::Visibility::public_;
    const auto generic_function_inserted =
        analyzer.state_.generics.function_templates.emplace(generic_function.key, generic_function);
    ASSERT_TRUE(generic_function_inserted.second);
    analyzer.index_generic_function_template(generic_function_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo generic_method;
    generic_method.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_method.name = checked_text(analyzer.state_.checked, "generic_method");
    generic_method.name_id = generic_method_id;
    generic_method.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "generic_method");
    generic_method.function_key =
        semantic_method_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, "generic_method");
    generic_method.impl_type_pattern = record_type;
    generic_method.visibility = syntax::Visibility::public_;
    const auto generic_method_inserted =
        analyzer.state_.generics.method_templates.emplace(generic_method.function_key, generic_method);
    ASSERT_TRUE(generic_method_inserted.second);
    analyzer.index_generic_method_template(generic_method_inserted.first->second);

    FunctionSignature run = indexed_function_signature(analyzer, "run", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    run.visibility = syntax::Visibility::public_;
    run.semantic_key = semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "run");
    const auto run_inserted = analyzer.state_.checked.functions.emplace(run.semantic_key, run);
    ASSERT_TRUE(run_inserted.second);
    analyzer.index_function_lookup(run_inserted.first->second);

    FunctionSignature method =
        indexed_function_signature(analyzer, "method", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    method.is_method = true;
    method.has_self_param = true;
    method.method_owner_type = record_type;
    method.visibility = syntax::Visibility::public_;
    method.semantic_key =
        semantic_method_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, "method");
    const auto method_inserted = analyzer.state_.checked.functions.emplace(method.semantic_key, method);
    ASSERT_TRUE(method_inserted.second);
    analyzer.index_method_lookup(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, method_id, method_inserted.first->second);

    const auto value_inserted = analyzer.state_.functions.global_values.emplace(
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "VALUE"),
        indexed_symbol(analyzer, SymbolKind::const_, "VALUE", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32));
    ASSERT_TRUE(value_inserted.second);
    analyzer.index_global_value(value_inserted.first->second);

    EnumCaseInfo case_info;
    case_info.name = checked_text(analyzer.state_.checked, "Choice_yes");
    case_info.name_id = choice_yes_id;
    case_info.case_name = checked_text(analyzer.state_.checked, "yes");
    case_info.case_name_id = yes_id;
    case_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    case_info.type = enum_type;
    const auto case_inserted = analyzer.state_.checked.enum_cases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Choice_yes"), case_info);
    ASSERT_TRUE(case_inserted.second);
    analyzer.index_enum_case(case_inserted.first->second);

    EXPECT_TRUE(analyzer.named_type_lookup_complete());
    EXPECT_TRUE(analyzer.type_alias_lookup_complete());
    EXPECT_TRUE(analyzer.generic_struct_lookup_complete());
    EXPECT_TRUE(analyzer.generic_function_lookup_complete());
    EXPECT_TRUE(analyzer.generic_method_lookup_complete());
    EXPECT_TRUE(analyzer.function_lookup_complete());
    EXPECT_TRUE(analyzer.global_value_lookup_complete());
    EXPECT_TRUE(analyzer.enum_case_module_lookup_complete());

    EXPECT_TRUE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE"));
    EXPECT_TRUE(
        analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_id, "Record"));
    EXPECT_TRUE(
        analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(record_id, "Record"));
    EXPECT_TRUE(types.same(
        analyzer.find_type_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_id, "Record", {}, false),
        record_type));
    EXPECT_TRUE(types.same(
        analyzer.find_type_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), alias_id, "Alias", {}, false), i32));
    EXPECT_EQ(
        analyzer.find_any_generic_type_template_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), box_id, "Box"),
        &generic_type_inserted.first->second);
    EXPECT_EQ(analyzer.find_generic_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), make_id, "make", {}),
        &generic_function_inserted.first->second);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), run_id, "run", {}),
        &run_inserted.first->second);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, method_id, "method", {}, true),
        &method_inserted.first->second);
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE", {}),
        &value_inserted.first->second);

    analyzer.state_.flow.current_module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_EQ(
        analyzer.find_enum_case_in_visible_modules(choice_yes_id, "Choice_yes", {}), &case_inserted.first->second);
    const base::usize interned_before_miss = analyzer.ctx_.module.identifiers.size();
    EXPECT_EQ(
        analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false),
        nullptr);
    EXPECT_EQ(
        analyzer.find_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false),
        nullptr);
    EXPECT_EQ(analyzer.ctx_.module.identifiers.size(), interned_before_miss);
}

TEST(CoreUnit, SemanticWhiteBoxGenericLookupUsesSelectiveReexports)
{
    constexpr u32 SEMA_TEST_SELECTIVE_FACADE_MODULE_INDEX = 1;
    constexpr u32 SEMA_TEST_SELECTIVE_INNER_MODULE_INDEX = 2;

    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"facade"}),
        module_info({"inner"}),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const ModuleId facade_module = module_id(SEMA_TEST_SELECTIVE_FACADE_MODULE_INDEX);
    const ModuleId inner_module = module_id(SEMA_TEST_SELECTIVE_INNER_MODULE_INDEX);
    const IdentId box_id = intern_identifier(analyzer, "Box");
    const IdentId re_box_id = intern_identifier(analyzer, "ReBox");
    const IdentId choice_id = intern_identifier(analyzer, "Choice");
    const IdentId re_choice_id = intern_identifier(analyzer, "ReChoice");
    const IdentId alias_box_id = intern_identifier(analyzer, "AliasBox");
    const IdentId re_alias_box_id = intern_identifier(analyzer, "ReAliasBox");
    const IdentId make_id = intern_identifier(analyzer, "make");
    const IdentId re_make_id = intern_identifier(analyzer, "re_make");

    analyzer.ctx_.module.modules[SEMA_TEST_SELECTIVE_FACADE_MODULE_INDEX].reexports = {
        resolved_use(inner_module, "Box", "ReBox", syntax::Visibility::public_, box_id, re_box_id),
        resolved_use(inner_module, "Choice", "ReChoice", syntax::Visibility::public_, choice_id, re_choice_id),
        resolved_use(
            inner_module, "AliasBox", "ReAliasBox", syntax::Visibility::public_, alias_box_id, re_alias_box_id),
        resolved_use(inner_module, "make", "re_make", syntax::Visibility::public_, make_id, re_make_id),
    };

    sema::SemanticAnalyzerCore::GenericTemplateInfo box = generic_template_info(analyzer, inner_module, "Box");
    const auto box_inserted = analyzer.state_.generics.struct_templates.emplace(box.key, std::move(box));
    ASSERT_TRUE(box_inserted.second);
    analyzer.index_generic_struct_template(box_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo choice = generic_template_info(analyzer, inner_module, "Choice");
    const auto choice_inserted = analyzer.state_.generics.enum_templates.emplace(choice.key, std::move(choice));
    ASSERT_TRUE(choice_inserted.second);
    analyzer.index_generic_enum_template(choice_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo alias_box =
        generic_template_info(analyzer, inner_module, "AliasBox");
    const auto alias_box_inserted =
        analyzer.state_.generics.type_alias_templates.emplace(alias_box.key, std::move(alias_box));
    ASSERT_TRUE(alias_box_inserted.second);
    analyzer.index_generic_type_alias_template(alias_box_inserted.first->second);

    sema::SemanticAnalyzerCore::GenericTemplateInfo make = generic_template_info(analyzer, inner_module, "make");
    const auto make_inserted = analyzer.state_.generics.function_templates.emplace(make.key, std::move(make));
    ASSERT_TRUE(make_inserted.second);
    analyzer.index_generic_function_template(make_inserted.first->second);

    EXPECT_EQ(analyzer.find_generic_struct_in_module(facade_module, re_box_id, "ReBox", {}, false),
        &box_inserted.first->second);
    EXPECT_EQ(analyzer.find_generic_enum_in_module(facade_module, re_choice_id, "ReChoice", {}, false),
        &choice_inserted.first->second);
    EXPECT_EQ(analyzer.find_generic_type_alias_in_module(facade_module, re_alias_box_id, "ReAliasBox", {}, false),
        &alias_box_inserted.first->second);
    EXPECT_TRUE(analyzer.generic_type_template_exists_in_module(facade_module, re_box_id, "ReBox"));
    EXPECT_EQ(analyzer.find_generic_function_in_module(facade_module, re_make_id, "re_make", {}, false),
        &make_inserted.first->second);

    const base::usize diagnostic_count = diagnostics.diagnostics().size();
    analyzer.report_generic_type_template_in_module(facade_module, re_box_id, "ReBox", {});
    ASSERT_EQ(diagnostics.diagnostics().size(), diagnostic_count + 1U);
    EXPECT_EQ(diagnostics.diagnostics()[diagnostic_count].message, "generic type ReBox requires type arguments");
}

TEST(CoreUnit, SemanticWhiteBoxTypedLookupRejectsUnindexedLegacyMaps)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle record_type = types.named_struct("lib.LegacyRecord", "lib_LegacyRecord", false);
    const TypeHandle enum_type = types.named_enum("lib.LegacyChoice", "lib_LegacyChoice");
    types.set_enum_underlying(enum_type, i32);

    const IdentId legacy_record_id = intern_identifier(analyzer, "LegacyRecord");
    const IdentId legacy_alias_id = intern_identifier(analyzer, "LegacyAlias");
    const IdentId legacy_box_id = intern_identifier(analyzer, "LegacyBox");
    const IdentId private_box_id = intern_identifier(analyzer, "PrivateBox");
    const IdentId legacy_enum_id = intern_identifier(analyzer, "LegacyEnum");
    const IdentId legacy_alias_box_id = intern_identifier(analyzer, "LegacyAliasBox");
    const IdentId legacy_make_id = intern_identifier(analyzer, "LegacyMake");
    const IdentId legacy_fn_id = intern_identifier(analyzer, "legacy_fn");
    const IdentId owner_method_id = intern_identifier(analyzer, "owner_method");
    const IdentId legacy_value_id = intern_identifier(analyzer, "LEGACY_VALUE");
    const IdentId legacy_choice_yes_id = intern_identifier(analyzer, "LegacyChoice_yes");
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId missing_id = intern_identifier(analyzer, "missing");

    const sema::ModuleLookupKey record_key =
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyRecord");
    analyzer.state_.types.named_types.emplace(record_key, record_type);

    StructInfo record_info;
    record_info.name = checked_text(analyzer.state_.checked, "LegacyRecord");
    record_info.name_id = legacy_record_id;
    record_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    record_info.type = record_type;
    const auto record_inserted = analyzer.state_.checked.structs.emplace(record_key, record_info);
    ASSERT_TRUE(record_inserted.second);
    analyzer.state_.types.struct_infos_by_type[record_type.value] = &record_inserted.first->second;

    const TypeId i32_type_id = analyzer.ctx_.module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    sema::TypeAliasInfo alias;
    alias.name = checked_text(analyzer.state_.checked, "LegacyAlias");
    alias.name_id = legacy_alias_id;
    alias.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    alias.target = i32_type_id;
    alias.visibility = syntax::Visibility::public_;
    analyzer.state_.checked.type_aliases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAlias"), alias);

    analyzer.state_.generics.struct_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox"));
    analyzer.state_.generics.struct_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "PrivateBox"),
        generic_template_info(
            analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "PrivateBox", syntax::Visibility::private_));
    analyzer.state_.generics.enum_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum"));
    analyzer.state_.generics.type_alias_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox"));
    analyzer.state_.generics.function_templates.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake"));

    FunctionSignature fallback_function =
        indexed_function_signature(analyzer, "legacy_fn", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    fallback_function.visibility = syntax::Visibility::public_;
    fallback_function.semantic_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn");
    analyzer.state_.checked.functions.emplace(fallback_function.semantic_key, fallback_function);

    FunctionSignature owner_method =
        indexed_function_signature(analyzer, "owner_method", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    owner_method.is_method = true;
    owner_method.has_self_param = true;
    owner_method.method_owner_type = record_type;
    owner_method.visibility = syntax::Visibility::public_;
    owner_method.param_types = {record_type};
    owner_method.semantic_key =
        semantic_method_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, "owner_method");
    const auto owner_method_inserted =
        analyzer.state_.checked.functions.emplace(owner_method.semantic_key, owner_method);
    ASSERT_TRUE(owner_method_inserted.second);

    const auto value_inserted = analyzer.state_.functions.global_values.emplace(
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LEGACY_VALUE"),
        indexed_symbol(analyzer, SymbolKind::const_, "LEGACY_VALUE", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32));
    ASSERT_TRUE(value_inserted.second);

    EnumCaseInfo enum_case;
    enum_case.name = checked_text(analyzer.state_.checked, "LegacyChoice_yes");
    enum_case.name_id = legacy_choice_yes_id;
    enum_case.case_name = checked_text(analyzer.state_.checked, "yes");
    enum_case.case_name_id = yes_id;
    enum_case.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    enum_case.type = enum_type;
    const auto enum_case_inserted = analyzer.state_.checked.enum_cases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyChoice_yes"), enum_case);
    ASSERT_TRUE(enum_case_inserted.second);

    EXPECT_FALSE(analyzer.named_type_lookup_complete());
    EXPECT_FALSE(analyzer.type_alias_lookup_complete());
    EXPECT_FALSE(analyzer.generic_struct_lookup_complete());
    EXPECT_FALSE(analyzer.generic_enum_lookup_complete());
    EXPECT_FALSE(analyzer.generic_type_alias_lookup_complete());
    EXPECT_FALSE(analyzer.generic_function_lookup_complete());
    EXPECT_FALSE(analyzer.function_lookup_complete());
    EXPECT_FALSE(analyzer.global_value_lookup_complete());
    EXPECT_FALSE(analyzer.enum_case_module_lookup_complete());

    EXPECT_FALSE(analyzer.top_level_value_name_exists(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE"));
    EXPECT_FALSE(analyzer.module_type_or_value_name_exists(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, "LegacyRecord"));
    EXPECT_FALSE(analyzer.visible_type_name_exists(legacy_alias_id, "LegacyAlias"));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_alias_id, "LegacyAlias", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, "LegacyRecord", {}, false)));

    analyzer.state_.flow.current_module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_EQ(analyzer.find_generic_struct_in_visible_modules(legacy_box_id, "LegacyBox", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_generic_enum_in_visible_modules(legacy_enum_id, "LegacyEnum", {}, false), nullptr);
    EXPECT_EQ(
        analyzer.find_generic_type_alias_in_visible_modules(legacy_alias_box_id, "LegacyAliasBox", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_generic_function_in_visible_modules(legacy_make_id, "LegacyMake", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_function_in_visible_modules(legacy_fn_id, "legacy_fn", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_method_in_owner_module(record_type, owner_method_id, "owner_method", true), nullptr);
    EXPECT_EQ(analyzer.find_symbol_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE", {}, false),
        nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(legacy_choice_yes_id, "LegacyChoice_yes", {}, false), nullptr);

    analyzer.index_named_type(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, record_type, syntax::Visibility::public_);
    analyzer.index_type_alias(analyzer.state_.checked.type_aliases
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAlias"))
            ->second);
    analyzer.index_generic_struct_template(analyzer.state_.generics.struct_templates
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox"))
            ->second);
    analyzer.index_generic_enum_template(analyzer.state_.generics.enum_templates
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum"))
            ->second);
    analyzer.index_generic_type_alias_template(analyzer.state_.generics.type_alias_templates
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox"))
            ->second);
    analyzer.index_generic_function_template(analyzer.state_.generics.function_templates
            .find(semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake"))
            ->second);
    analyzer.index_function_lookup(analyzer.state_.checked.functions
            .find(semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn"))
            ->second);
    analyzer.index_method_lookup(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, owner_method_id, owner_method_inserted.first->second);
    analyzer.index_global_value(value_inserted.first->second);
    analyzer.index_enum_case(enum_case_inserted.first->second);

    EXPECT_TRUE(analyzer.top_level_value_name_exists(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, "LegacyRecord"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(legacy_alias_id, "LegacyAlias"));
    EXPECT_TRUE(types.same(analyzer.find_type_in_module(
                               module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_alias_id, "LegacyAlias", {}, false),
        i32));
    EXPECT_NE(analyzer.find_generic_struct_in_visible_modules(legacy_box_id, "LegacyBox", {}, false), nullptr);
    EXPECT_NE(analyzer.find_generic_enum_in_visible_modules(legacy_enum_id, "LegacyEnum", {}, false), nullptr);
    EXPECT_NE(
        analyzer.find_generic_type_alias_in_visible_modules(legacy_alias_box_id, "LegacyAliasBox", {}, false), nullptr);
    EXPECT_NE(analyzer.find_generic_function_in_visible_modules(legacy_make_id, "LegacyMake", {}, false), nullptr);
    EXPECT_TRUE(analyzer.generic_type_template_exists_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_box_id, "LegacyBox"));
    EXPECT_TRUE(analyzer.report_generic_type_requires_args_if_visible(legacy_box_id, "LegacyBox", {}));
    analyzer.report_generic_type_template_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_box_id, "LegacyBox", {});
    analyzer.report_generic_type_template_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingGeneric", {});
    EXPECT_EQ(analyzer.find_generic_struct_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingStruct", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_struct_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "MissingStruct", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_struct_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), private_box_id, "PrivateBox", {}, false),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_enum_in_visible_modules(missing_id, "MissingEnum", {}, true), nullptr);
    EXPECT_EQ(
        analyzer.find_generic_enum_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingEnum", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_generic_enum_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "MissingEnum", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_type_alias_in_visible_modules(missing_id, "MissingAlias", {}, true), nullptr);
    EXPECT_EQ(
        analyzer.find_generic_type_alias_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingAlias", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_type_alias_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "MissingAlias", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_function_in_visible_modules(missing_id, "missing_function", {}, true), nullptr);
    EXPECT_EQ(
        analyzer.find_generic_function_in_module(syntax::INVALID_MODULE_ID, missing_id, "missing_function", {}, true),
        nullptr);
    EXPECT_EQ(analyzer.find_generic_function_in_module(
                  module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false),
        nullptr);
    EXPECT_EQ(analyzer.find_function_in_visible_modules(legacy_fn_id, "legacy_fn", {}, false),
        &analyzer.state_.checked.functions
            .find(semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn"))
            ->second);
    EXPECT_EQ(analyzer.find_method_in_owner_module(record_type, owner_method_id, "owner_method", true),
        &owner_method_inserted.first->second);
    EXPECT_EQ(analyzer.find_method_in_owner_module(record_type, missing_id, "missing", true), nullptr);
    EXPECT_EQ(
        analyzer.find_method_in_owner_module(INVALID_TYPE_HANDLE, owner_method_id, "owner_method", true), nullptr);
    EXPECT_EQ(
        analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE", {}),
        &value_inserted.first->second);
    EXPECT_EQ(analyzer.find_symbol_in_module(syntax::INVALID_MODULE_ID, missing_id, "missing", {}), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(legacy_choice_yes_id, "LegacyChoice_yes", {}),
        &enum_case_inserted.first->second);
}

TEST(CoreUnit, SemanticWhiteBoxBodyInferenceEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const TypeId plain_type_id = module.push_type(named_node("Plain"));

    syntax::StmtNode expr_stmt;
    expr_stmt.kind = syntax::StmtKind::expr;
    const syntax::StmtId expr_stmt_id = module.push_stmt(expr_stmt);
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "infer";
    function.body = expr_stmt_id;
    const syntax::ItemId function_item = module.push_item(function);
    module.item_modules[function_item.value] = module_id(0);

    const ExprId INVALID_EXPR_ID = module.push_invalid_expr({});

    syntax::StmtNode empty_return_stmt;
    empty_return_stmt.kind = syntax::StmtKind::return_;
    const syntax::StmtId empty_return_stmt_id = module.push_stmt(empty_return_stmt);

    syntax::StmtNode invalid_expr_return_stmt;
    invalid_expr_return_stmt.kind = syntax::StmtKind::return_;
    invalid_expr_return_stmt.return_value = INVALID_EXPR_ID;
    const syntax::StmtId invalid_expr_return_stmt_id = module.push_stmt(invalid_expr_return_stmt);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.stmt_local_types.assign(module.stmts.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle ptr_i32 = analyzer.state_.checked.types.pointer(PointerMutability::const_, i32);
    const TypeHandle plain_type = analyzer.state_.checked.types.named_struct("Plain", "Plain", false);
    static_cast<void>(add_named_type(analyzer, module_id(0), "Plain", plain_type));

    const sema::FunctionLookupKey infer_key = semantic_function_key(analyzer, module_id(0), "infer");
    FunctionSignature conflict_signature =
        indexed_function_signature(analyzer, "infer", module_id(0), INVALID_TYPE_HANDLE);
    conflict_signature.semantic_key = infer_key;
    sema::SemanticAnalyzerCore::FunctionBodyState state = sema::SemanticAnalyzerCore::FunctionBodyState::analyzing;
    analyzer.analyze_function_body_with_signature(function, infer_key, conflict_signature, state);
    state = sema::SemanticAnalyzerCore::FunctionBodyState::analyzed;
    analyzer.analyze_function_body_with_signature(function, infer_key, conflict_signature, state);
    state = sema::SemanticAnalyzerCore::FunctionBodyState::not_started;
    conflict_signature.has_conflict = true;
    analyzer.analyze_function_body_with_signature(function, infer_key, conflict_signature, state);

    analyzer.analyze_block(syntax::INVALID_STMT_ID, i32, nullptr);
    analyzer.analyze_stmt(syntax::INVALID_STMT_ID, i32, nullptr);
    sema::SemanticAnalyzerCore::ReturnTypeInference inference;
    analyzer.finalize_inferred_return(function, infer_key, inference);

    sema::SemanticAnalyzerCore::ReturnTypeInference invalid_pending_null_return;
    invalid_pending_null_return.inferred_type = ptr_i32;
    invalid_pending_null_return.pending_null_returns.push_back(syntax::INVALID_STMT_ID);
    analyzer.resolve_pending_null_returns(invalid_pending_null_return);

    sema::SemanticAnalyzerCore::ReturnTypeInference empty_pending_null_return;
    empty_pending_null_return.inferred_type = ptr_i32;
    empty_pending_null_return.pending_null_returns.push_back(empty_return_stmt_id);
    analyzer.resolve_pending_null_returns(empty_pending_null_return);

    sema::SemanticAnalyzerCore::ReturnTypeInference invalid_expr_pending_null_return;
    invalid_expr_pending_null_return.inferred_type = ptr_i32;
    invalid_expr_pending_null_return.pending_null_returns.push_back(invalid_expr_return_stmt_id);
    analyzer.resolve_pending_null_returns(invalid_expr_pending_null_return);
    analyzer.report_return_inference_diagnostic(syntax::INVALID_STMT_ID, "ignored diagnostic");

    conflict_signature.has_conflict = false;
    analyzer.ensure_function_return_known(conflict_signature, {});
    EXPECT_FALSE(is_valid(analyzer.analyze_expr(syntax::INVALID_EXPR_ID)));
    EXPECT_FALSE(is_valid(analyzer.analyze_expr(INVALID_EXPR_ID)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(syntax::INVALID_TYPE_ID)));

    analyzer.state_.checked.syntax_type_handles[plain_type_id.value] = plain_type;
    EXPECT_TRUE(analyzer.state_.checked.types.same(analyzer.resolve_type(plain_type_id), plain_type));
    const TypeHandle opaque = analyzer.state_.checked.types.opaque_struct("Opaque", "Opaque");
    analyzer.state_.checked.syntax_type_handles[plain_type_id.value] = opaque;
    EXPECT_TRUE(analyzer.state_.checked.types.same(analyzer.resolve_type(plain_type_id), opaque));
    analyzer.state_.checked.syntax_type_handles[plain_type_id.value] = INVALID_TYPE_HANDLE;
}

TEST(CoreUnit, SemanticWhiteBoxGenericTemplateNodeSpansTrackReachableAstOnly)
{
    syntax::AstModule module;
    module.modules = {module_info({"generic_span"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    const TypeId unused_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::f64));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));

    syntax::TypeNode pointer_type;
    pointer_type.kind = syntax::TypeKind::pointer;
    pointer_type.pointee = generic_type;
    const TypeId pointer_generic_type = module.push_type(pointer_type);

    syntax::TypeNode array_type;
    array_type.kind = syntax::TypeKind::array;
    array_type.array_count = SEMA_TEST_SMALL_ARRAY_COUNT;
    array_type.array_element = i32_type;
    const TypeId array_i32_type = module.push_type(array_type);

    syntax::TypeNode slice_type;
    slice_type.kind = syntax::TypeKind::slice;
    slice_type.slice_element = generic_type;
    const TypeId slice_generic_type = module.push_type(slice_type);

    syntax::TypeNode tuple_type;
    tuple_type.kind = syntax::TypeKind::tuple;
    tuple_type.tuple_elements = {i32_type, generic_type};
    const TypeId tuple_i32_generic_type = module.push_type(tuple_type);

    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_params = {pointer_generic_type, slice_generic_type, tuple_i32_generic_type};
    function_type.function_return = array_i32_type;
    const TypeId function_handle_type = module.push_type(function_type);

    syntax::TypeNode box_type_node = named_node("Box");
    box_type_node.type_args = {function_handle_type};
    const TypeId box_function_type = module.push_type(box_type_node);

    syntax::PatternNode binding_pattern;
    binding_pattern.kind = syntax::PatternKind::binding;
    binding_pattern.binding_name = "value";
    const syntax::PatternId binding_pattern_id = module.push_pattern(binding_pattern);

    syntax::PatternNode unused_pattern;
    unused_pattern.kind = syntax::PatternKind::wildcard;
    const syntax::PatternId unused_pattern_id = module.push_pattern(unused_pattern);

    syntax::PatternNode enum_pattern;
    enum_pattern.kind = syntax::PatternKind::enum_case;
    enum_pattern.enum_type = box_function_type;
    enum_pattern.payload_patterns = {binding_pattern_id};
    const syntax::PatternId enum_pattern_id = module.push_pattern(enum_pattern);

    syntax::PatternNode tuple_pattern;
    tuple_pattern.kind = syntax::PatternKind::tuple;
    tuple_pattern.elements = {binding_pattern_id, enum_pattern_id};
    const syntax::PatternId tuple_pattern_id = module.push_pattern(tuple_pattern);

    syntax::PatternNode slice_pattern;
    slice_pattern.kind = syntax::PatternKind::slice;
    slice_pattern.elements = {tuple_pattern_id};
    slice_pattern.has_slice_rest = true;
    const syntax::PatternId slice_pattern_id = module.push_pattern(slice_pattern);

    syntax::PatternNode struct_pattern;
    struct_pattern.kind = syntax::PatternKind::struct_;
    struct_pattern.struct_name = "Box";
    struct_pattern.field_patterns = {syntax::FieldPattern{"field", slice_pattern_id, {}}};
    const syntax::PatternId struct_pattern_id = module.push_pattern(struct_pattern);

    syntax::PatternNode literal_pattern;
    literal_pattern.kind = syntax::PatternKind::literal;
    literal_pattern.case_name = "1";
    const syntax::PatternId literal_pattern_id = module.push_pattern(literal_pattern);

    syntax::PatternNode or_pattern;
    or_pattern.kind = syntax::PatternKind::or_pattern;
    or_pattern.alternatives = {struct_pattern_id, literal_pattern_id};
    const syntax::PatternId or_pattern_id = module.push_pattern(or_pattern);

    const ExprId name_with_type_arg = module.push_name_expr({}, "value", std::vector<TypeId>{generic_type});
    const ExprId unused_expr = push_integer_text(module, "99");
    const ExprId callee = module.push_name_expr({}, "callee", std::vector<TypeId>{function_handle_type});
    const ExprId generic_apply = push_generic_apply(module, callee, {i32_type});
    const ExprId try_expr = module.push_try_expr({}, generic_apply);
    const ExprId bool_expr = push_bool(module, "true");
    const ExprId binary_expr = push_binary(module, syntax::BinaryOp::add, name_with_type_arg, generic_apply);
    const ExprId call_expr = push_call(module, callee, {binary_expr, try_expr});

    syntax::CallExprPayload string_call_payload;
    string_call_payload.callee = callee;
    string_call_payload.args.assign({name_with_type_arg, generic_apply});
    const ExprId string_call_expr =
        module.push_call_expr(syntax::ExprKind::str_from_bytes_unchecked, {}, std::move(string_call_payload));

    const ExprId if_expr = module.push_if_expr({}, bool_expr, or_pattern_id, call_expr, string_call_expr);

    syntax::StmtNode inner_expr_stmt;
    inner_expr_stmt.kind = syntax::StmtKind::expr;
    inner_expr_stmt.init = binary_expr;
    const syntax::StmtId inner_expr_stmt_id = module.push_stmt(inner_expr_stmt);
    const syntax::StmtId inner_block_id = push_block(module, {inner_expr_stmt_id});
    const ExprId block_expr = module.push_block_expr(syntax::ExprKind::block_expr, {}, inner_block_id, if_expr);
    const ExprId unsafe_block_expr =
        module.push_block_expr(syntax::ExprKind::unsafe_block, {}, inner_block_id, block_expr);

    const ExprId match_guard_expr = push_bool(module, "false");
    const ExprId match_value_expr = push_integer(module);
    const ExprId match_expr = module.push_match_expr({}, name_with_type_arg,
        std::vector<syntax::MatchArm>{
            syntax::MatchArm{or_pattern_id, match_guard_expr, match_value_expr, {}},
        });

    const ExprId array_expr = module.push_array_expr(
        {}, std::vector<ExprId>{block_expr, unsafe_block_expr}, match_value_expr, name_with_type_arg);
    const ExprId tuple_expr = module.push_tuple_expr({}, std::vector<ExprId>{array_expr, match_expr});

    const ExprId tuple_generic = push_generic_apply(module, tuple_expr, {box_function_type});
    const ExprId tuple_call = push_call(module, tuple_generic, {call_expr});
    const ExprId tuple_slice =
        module.push_slice_expr({}, syntax::SliceExprPayload{tuple_call, match_guard_expr, match_value_expr});
    const ExprId postfix_expr = module.push_struct_literal_expr({}, tuple_slice, {}, {}, {}, std::vector<TypeId>{},
        std::vector<syntax::FieldInit>{syntax::FieldInit{"field", tuple_expr, {}}}, syntax::INVALID_IDENT_ID,
        syntax::INVALID_IDENT_ID);
    const ExprId field_expr = push_field(module, postfix_expr, "field");
    const ExprId index_expr = module.push_index_expr({}, syntax::IndexExprPayload{field_expr, match_value_expr});
    const ExprId slice_expr =
        module.push_slice_expr({}, syntax::SliceExprPayload{index_expr, match_guard_expr, match_value_expr});
    const ExprId struct_literal_expr = module.push_struct_literal_expr({}, push_name(module, "Box"), {}, {}, "Box",
        std::vector<TypeId>{box_function_type},
        std::vector<syntax::FieldInit>{syntax::FieldInit{"field", slice_expr, {}}}, syntax::INVALID_IDENT_ID,
        syntax::INVALID_IDENT_ID);
    const ExprId cast_expr =
        module.push_cast_like_expr(syntax::ExprKind::cast, {}, syntax::CastExprPayload{i32_type, struct_literal_expr});

    syntax::StmtNode unused_stmt;
    unused_stmt.kind = syntax::StmtKind::expr;
    unused_stmt.init = unused_expr;
    const syntax::StmtId unused_stmt_id = module.push_stmt(unused_stmt);

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = if_expr;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);

    const syntax::StmtId then_block_id = push_block(module, {inner_expr_stmt_id});
    const syntax::StmtId else_block_id = push_block(module, {return_stmt_id});

    syntax::StmtNode else_if_stmt;
    else_if_stmt.kind = syntax::StmtKind::if_;
    else_if_stmt.condition = bool_expr;
    else_if_stmt.then_block = then_block_id;
    const syntax::StmtId else_if_stmt_id = module.push_stmt(else_if_stmt);

    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = call_expr;
    if_stmt.pattern = or_pattern_id;
    if_stmt.then_block = then_block_id;
    if_stmt.else_block = else_block_id;
    if_stmt.else_if = else_if_stmt_id;
    const syntax::StmtId if_stmt_id = module.push_stmt(if_stmt);

    syntax::StmtNode let_stmt;
    let_stmt.kind = syntax::StmtKind::let;
    let_stmt.pattern = struct_pattern_id;
    let_stmt.declared_type = box_function_type;
    let_stmt.init = struct_literal_expr;
    let_stmt.else_block = else_block_id;
    const syntax::StmtId let_stmt_id = module.push_stmt(let_stmt);

    syntax::StmtNode assign_stmt;
    assign_stmt.kind = syntax::StmtKind::assign;
    assign_stmt.lhs = field_expr;
    assign_stmt.rhs = cast_expr;
    const syntax::StmtId assign_stmt_id = module.push_stmt(assign_stmt);

    syntax::StmtNode for_init_stmt;
    for_init_stmt.kind = syntax::StmtKind::expr;
    for_init_stmt.init = array_expr;
    const syntax::StmtId for_init_stmt_id = module.push_stmt(for_init_stmt);

    syntax::StmtNode for_update_stmt;
    for_update_stmt.kind = syntax::StmtKind::expr;
    for_update_stmt.init = tuple_expr;
    const syntax::StmtId for_update_stmt_id = module.push_stmt(for_update_stmt);
    const syntax::StmtId loop_body_id = push_block(module, {assign_stmt_id});

    syntax::StmtNode for_stmt;
    for_stmt.kind = syntax::StmtKind::for_;
    for_stmt.for_init = for_init_stmt_id;
    for_stmt.condition = bool_expr;
    for_stmt.for_update = for_update_stmt_id;
    for_stmt.body = loop_body_id;
    const syntax::StmtId for_stmt_id = module.push_stmt(for_stmt);

    syntax::StmtNode for_range_stmt;
    for_range_stmt.kind = syntax::StmtKind::for_range;
    for_range_stmt.range_start = name_with_type_arg;
    for_range_stmt.range_end = match_value_expr;
    for_range_stmt.range_step = match_guard_expr;
    for_range_stmt.body = loop_body_id;
    const syntax::StmtId for_range_stmt_id = module.push_stmt(for_range_stmt);

    syntax::StmtNode while_stmt;
    while_stmt.kind = syntax::StmtKind::while_;
    while_stmt.condition = bool_expr;
    while_stmt.pattern = or_pattern_id;
    while_stmt.body = loop_body_id;
    const syntax::StmtId while_stmt_id = module.push_stmt(while_stmt);

    syntax::StmtNode defer_stmt;
    defer_stmt.kind = syntax::StmtKind::defer;
    defer_stmt.init = match_expr;
    const syntax::StmtId defer_stmt_id = module.push_stmt(defer_stmt);

    const syntax::StmtId body_id = push_block(module,
        {
            let_stmt_id,
            assign_stmt_id,
            if_stmt_id,
            for_stmt_id,
            for_range_stmt_id,
            while_stmt_id,
            defer_stmt_id,
            return_stmt_id,
        });

    syntax::ItemNode generic_function;
    generic_function.kind = syntax::ItemKind::fn_decl;
    generic_function.name = "span";
    generic_function.generic_params = {syntax::GenericParamDecl{"T", {}}};
    generic_function.params = {syntax::ParamDecl{"param", box_function_type, {}}};
    generic_function.return_type = function_handle_type;
    generic_function.impl_type = pointer_generic_type;
    generic_function.body = body_id;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::SemanticAnalyzerCore::GenericTemplateInfo info = analyzer.make_generic_template_info();
    analyzer.populate_generic_template_node_spans(info, generic_function);

    const auto contains_id = [](const sema::SemaIndexTable& ids, const base::u32 value) {
        return std::ranges::find(ids, value) != ids.end();
    };
    EXPECT_TRUE(info.expr_span.contains(cast_expr.value));
    EXPECT_TRUE(info.pattern_span.contains(or_pattern_id.value));
    EXPECT_TRUE(info.type_span.contains(box_function_type.value));
    EXPECT_TRUE(info.stmt_span.contains(body_id.value));
    ASSERT_FALSE(info.expr_node_ids.empty());
    ASSERT_FALSE(info.pattern_node_ids.empty());
    ASSERT_FALSE(info.type_node_ids.empty());
    ASSERT_FALSE(info.stmt_node_ids.empty());
    EXPECT_FALSE(contains_id(info.expr_node_ids, unused_expr.value));
    EXPECT_FALSE(contains_id(info.pattern_node_ids, unused_pattern_id.value));
    EXPECT_FALSE(contains_id(info.type_node_ids, unused_type.value));
    EXPECT_FALSE(contains_id(info.stmt_node_ids, unused_stmt_id.value));

    sema::GenericSideTables side_tables;
    side_tables.configure_local_dense(sema::GenericSideTableLocalLayoutView{
        info.expr_span,
        info.pattern_span,
        info.type_span,
        info.stmt_span,
        info.expr_node_ids,
        info.pattern_node_ids,
        info.type_node_ids,
        info.stmt_node_ids,
    });
    EXPECT_EQ(side_tables.local_expr_index(unused_expr), sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_EQ(side_tables.local_pattern_index(unused_pattern_id), sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_EQ(side_tables.local_type_index(unused_type), sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_EQ(side_tables.local_stmt_index(unused_stmt_id), sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_EQ(side_tables.expr_types.size(), info.expr_node_ids.size());
    EXPECT_EQ(side_tables.pattern_c_name_ids.size(), info.pattern_node_ids.size());
    EXPECT_EQ(side_tables.syntax_type_handles.size(), info.type_node_ids.size());
    EXPECT_EQ(side_tables.stmt_local_types.size(), info.stmt_node_ids.size());
}

TEST(CoreUnit, SemanticWhiteBoxGenericTemplateNodeSpansSwitchToHashDedupForLargeBodies)
{
    syntax::AstModule module;
    module.modules = {module_info({"generic_large_span"})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    std::vector<ExprId> values;
    values.reserve(SEMA_TEST_LARGE_GENERIC_SPAN_EXPR_COUNT);
    for (base::usize index = 0; index < SEMA_TEST_LARGE_GENERIC_SPAN_EXPR_COUNT; ++index) {
        values.push_back(push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_ONE));
    }
    values.push_back(values.front());

    const ExprId tuple_expr = module.push_tuple_expr({}, values);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = tuple_expr;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body_id = push_block(module, {return_stmt_id});

    syntax::ItemNode generic_function;
    generic_function.kind = syntax::ItemKind::fn_decl;
    generic_function.name = "large_span";
    generic_function.generic_params = {syntax::GenericParamDecl{"T", {}}};
    generic_function.return_type = i32_type;
    generic_function.body = body_id;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::SemanticAnalyzerCore::GenericTemplateInfo info = analyzer.make_generic_template_info();
    analyzer.populate_generic_template_node_spans(info, generic_function);

    EXPECT_TRUE(info.expr_span.contains(tuple_expr.value));
    EXPECT_GE(info.expr_span.count, SEMA_TEST_LARGE_GENERIC_SPAN_EXPR_COUNT);
    EXPECT_TRUE(info.expr_node_ids.empty());
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowCollectsProjectionBorrowAndStableDump)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const IdentId source_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_SOURCE_NAME);
    const IdentId field_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const ExprId source = push_name(module, SEMA_TEST_BODY_FLOW_SOURCE_NAME);
    const ExprId source_field = push_field(module, source, SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const ExprId mutable_borrow = push_unary(module, syntax::UnaryOp::address_of_mut, source_field);
    const ExprId sink = push_name(module, SEMA_TEST_BODY_FLOW_SINK_NAME);
    const ExprId call = push_call(module, sink, {mutable_borrow});

    syntax::StmtNode defer_stmt;
    defer_stmt.kind = syntax::StmtKind::defer;
    defer_stmt.init = call;
    const syntax::StmtId defer_stmt_id = module.push_stmt(defer_stmt);

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = source;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {defer_stmt_id, return_stmt_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME;
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key = semantic_function_key(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME);
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    const auto found = analyzer.state_.checked.body_flow_graphs.find(key);
    ASSERT_NE(found, analyzer.state_.checked.body_flow_graphs.end());
    const sema::BodyFlowGraph& graph = found->second;
    EXPECT_TRUE(graph.collect_only);
    EXPECT_EQ(graph.body.value, body.value);
    EXPECT_TRUE(std::ranges::any_of(graph.points, [](const sema::BodyFlowPoint& point) {
        return point.kind == sema::BodyFlowPointKind::sequence;
    }));

    const auto has_action = [&graph](const sema::BodyFlowActionKind kind) {
        return std::ranges::any_of(graph.actions, [kind](const sema::BodyFlowAction& action) {
            return action.kind == kind;
        });
    };
    EXPECT_TRUE(has_action(sema::BodyFlowActionKind::borrow_mutable));
    EXPECT_TRUE(has_action(sema::BodyFlowActionKind::call));
    EXPECT_TRUE(has_action(sema::BodyFlowActionKind::cleanup_scope));
    EXPECT_TRUE(has_action(sema::BodyFlowActionKind::return_));
    EXPECT_TRUE(has_action(sema::BodyFlowActionKind::read));
    EXPECT_TRUE(has_action(sema::BodyFlowActionKind::move_candidate));

    const auto borrow = std::ranges::find_if(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_mutable;
    });
    ASSERT_NE(borrow, graph.actions.end());
    ASSERT_NE(borrow->place, sema::SEMA_BODY_FLOW_INVALID_INDEX);
    ASSERT_LT(borrow->place, graph.places.size());
    const sema::BodyFlowPlace& borrow_place = graph.places[borrow->place];
    EXPECT_EQ(borrow_place.root_kind, sema::BodyFlowPlaceRootKind::local);
    EXPECT_EQ(borrow_place.root_name_id, source_id);
    ASSERT_EQ(borrow_place.projections.size(), 1U);
    EXPECT_EQ(borrow_place.projections.front().kind, sema::BodyFlowPlaceProjectionKind::field);
    EXPECT_EQ(borrow_place.projections.front().field_name_id, field_id);

    const std::string dump = sema::dump_body_flow_graph(graph);
    EXPECT_NE(dump.find("collect_only=true"), std::string::npos);
    EXPECT_NE(dump.find("borrow_mutable"), std::string::npos);
    EXPECT_NE(dump.find("cleanup_scope"), std::string::npos);
    EXPECT_NE(dump.find("return"), std::string::npos);
    EXPECT_NE(dump.find("field("), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowRecordsDeclaredBorrowContracts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const ExprId param = push_name(module, "param");
    const ExprId other = push_name(module, "other");
    const ExprId callee = push_name(module, "borrowed_view");
    const ExprId call = push_call(module, callee, {param, other});
    const syntax::StmtId call_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "out", syntax::INVALID_TYPE_ID, call);
    const syntax::StmtId body = push_block(module, {call_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "contract_flow";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "contract_flow");
    const IdentId callee_id = module.intern_identifier("borrowed_view");
    const sema::FunctionLookupKey callee_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        callee_id,
    };
    sema::FunctionCallBinding binding = analyzer.state_.checked.make_function_call_binding();
    binding.call_expr = call;
    binding.callee_expr = callee;
    binding.function_key = callee_key;
    binding.return_type = analyzer.state_.checked.types.reference(
        PointerMutability::const_, analyzer.state_.checked.types.builtin(BuiltinType::i32));
    analyzer.state_.checked.append_function_call_binding(binding);

    sema::FunctionBorrowContract contract;
    contract.function = callee_key;
    contract.source = sema::FunctionBorrowContractSource::declared;
    contract.return_type = binding.return_type;
    contract.return_type_can_contain_borrow = true;
    contract.return_selectors = {
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::parameter,
            .param_index = 0U,
            .name_id = module.intern_identifier("param"),
            .range = body_loan_test_range(0),
        },
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::static_,
            .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
            .range = body_loan_test_range(1),
        },
    };
    analyzer.state_.checked.borrow_contracts[callee_key] = contract;

    prepare_expr_storage(analyzer, module);
    analyzer.state_.checked.expr_types[call.value] = binding.return_type;
    analyzer.state_.checked.expr_types[param.value] = binding.return_type;
    analyzer.state_.checked.expr_types[other.value] = analyzer.state_.checked.types.builtin(BuiltinType::i32);

    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);
    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared && action.place < graph.places.size()
            && graph.places[action.place].root_kind == sema::BodyFlowPlaceRootKind::local;
    }));
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowRecordsUnknownBorrowContracts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const ExprId value = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId borrowed = push_unary(module, syntax::UnaryOp::address_of, value);
    const ExprId callee = push_name(module, "unknown_view");
    const ExprId call = push_call(module, callee, {borrowed, value});
    const syntax::StmtId call_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "out", syntax::INVALID_TYPE_ID, call);
    const syntax::StmtId body = push_block(module, {call_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "unknown_contract_flow";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "unknown_contract_flow");
    const IdentId callee_id = module.intern_identifier("unknown_view");
    const sema::FunctionLookupKey callee_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        callee_id,
    };
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = analyzer.state_.checked.types.reference(PointerMutability::const_, i32);
    sema::FunctionCallBinding binding = analyzer.state_.checked.make_function_call_binding();
    binding.call_expr = call;
    binding.callee_expr = callee;
    binding.function_key = callee_key;
    binding.return_type = ref_i32;
    analyzer.state_.checked.append_function_call_binding(binding);

    sema::FunctionBorrowContract contract;
    contract.function = callee_key;
    contract.source = sema::FunctionBorrowContractSource::declared;
    contract.return_type = ref_i32;
    contract.return_type_can_contain_borrow = true;
    contract.unknown_return_allowed = true;
    contract.return_selectors = {
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::unknown,
            .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
            .range = body_loan_test_range(0),
        },
    };
    analyzer.state_.checked.borrow_contracts[callee_key] = contract;

    prepare_expr_storage(analyzer, module);
    analyzer.state_.checked.expr_types[call.value] = ref_i32;
    analyzer.state_.checked.expr_types[borrowed.value] = ref_i32;
    analyzer.state_.checked.expr_types[value.value] = i32;

    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);
    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared && action.place < graph.places.size()
            && graph.places[action.place].root_kind == sema::BodyFlowPlaceRootKind::unknown;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared && action.place < graph.places.size()
            && graph.places[action.place].root_kind == sema::BodyFlowPlaceRootKind::local;
    }));
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowTreatsNonStaticSummaryOriginsAsUnknown)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const ExprId callee = push_name(module, "local_view");
    const ExprId call = push_call(module, callee, {});
    const syntax::StmtId call_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "out", syntax::INVALID_TYPE_ID, call);
    const syntax::StmtId body = push_block(module, {call_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "summary_local_origin_flow";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "summary_local_origin_flow");
    const IdentId callee_id = module.intern_identifier("local_view");
    const sema::FunctionLookupKey callee_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        callee_id,
    };
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = analyzer.state_.checked.types.reference(PointerMutability::const_, i32);
    sema::FunctionCallBinding binding = analyzer.state_.checked.make_function_call_binding();
    binding.call_expr = call;
    binding.callee_expr = callee;
    binding.function_key = callee_key;
    binding.return_type = ref_i32;
    analyzer.state_.checked.append_function_call_binding(binding);

    sema::FunctionBorrowSummary summary;
    summary.function = callee_key;
    summary.return_type = ref_i32;
    summary.return_type_can_contain_borrow = true;
    summary.origins = {
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::local,
            .name_id = module.intern_identifier("local"),
            .range = body_loan_test_range(0),
        },
    };
    summary.return_origins = {sema::FunctionBorrowReturnOrigin{.origin_index = 0U}};
    analyzer.state_.checked.borrow_summaries[callee_key] = summary;

    prepare_expr_storage(analyzer, module);
    analyzer.state_.checked.expr_types[call.value] = ref_i32;

    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);
    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared && action.place < graph.places.size()
            && graph.places[action.place].root_kind == sema::BodyFlowPlaceRootKind::unknown;
    }));
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowCoversEdgeFactsAndKindNames)
{
    EXPECT_EQ(sema::body_flow_point_kind_name(sema::BodyFlowPointKind::entry), "entry");
    EXPECT_EQ(sema::body_flow_point_kind_name(sema::BodyFlowPointKind::exit), "exit");
    EXPECT_EQ(sema::body_flow_point_kind_name(sema::BodyFlowPointKind::statement_entry), "statement_entry");
    EXPECT_EQ(sema::body_flow_point_kind_name(sema::BodyFlowPointKind::statement_exit), "statement_exit");
    EXPECT_EQ(sema::body_flow_point_kind_name(sema::BodyFlowPointKind::expression_entry), "expression_entry");
    EXPECT_EQ(sema::body_flow_point_kind_name(sema::BodyFlowPointKind::expression_exit), "expression_exit");
    EXPECT_EQ(sema::body_flow_point_kind_name(sema::BodyFlowPointKind::sequence), "sequence");
    EXPECT_EQ(sema::body_flow_point_kind_name(sema::BodyFlowPointKind::cleanup_scope), "cleanup_scope");
    EXPECT_EQ(
        sema::body_flow_point_kind_name(static_cast<sema::BodyFlowPointKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::read), "read");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::write), "write");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::reinit), "reinit");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::move_candidate), "move_candidate");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::drop), "drop");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::borrow_shared), "borrow_shared");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::borrow_mutable), "borrow_mutable");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::call), "call");
    EXPECT_EQ(
        sema::body_flow_action_kind_name(sema::BodyFlowActionKind::call_receiver_reserve), "call_receiver_reserve");
    EXPECT_EQ(
        sema::body_flow_action_kind_name(sema::BodyFlowActionKind::call_receiver_activate), "call_receiver_activate");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::return_), "return");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::branch), "branch");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::cleanup_scope), "cleanup_scope");
    EXPECT_EQ(sema::body_flow_action_kind_name(sema::BodyFlowActionKind::cleanup_storage), "cleanup_storage");
    EXPECT_EQ(
        sema::body_flow_action_kind_name(static_cast<sema::BodyFlowActionKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::body_flow_place_root_kind_name(sema::BodyFlowPlaceRootKind::none), "none");
    EXPECT_EQ(sema::body_flow_place_root_kind_name(sema::BodyFlowPlaceRootKind::local), "local");
    EXPECT_EQ(sema::body_flow_place_root_kind_name(sema::BodyFlowPlaceRootKind::temporary), "temporary");
    EXPECT_EQ(sema::body_flow_place_root_kind_name(sema::BodyFlowPlaceRootKind::unknown), "unknown");
    EXPECT_EQ(sema::body_flow_place_root_kind_name(
                  static_cast<sema::BodyFlowPlaceRootKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::body_flow_place_projection_kind_name(sema::BodyFlowPlaceProjectionKind::field), "field");
    EXPECT_EQ(sema::body_flow_place_projection_kind_name(sema::BodyFlowPlaceProjectionKind::index), "index");
    EXPECT_EQ(
        sema::body_flow_place_projection_kind_name(sema::BodyFlowPlaceProjectionKind::dereference), "dereference");
    EXPECT_EQ(sema::body_flow_place_projection_kind_name(sema::BodyFlowPlaceProjectionKind::slice), "slice");
    EXPECT_EQ(sema::body_flow_place_projection_kind_name(
                  static_cast<sema::BodyFlowPlaceProjectionKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");

    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const ExprId invalid_expr = module.push_invalid_expr({});
    const ExprId bool_expr = push_bool(module, "true");
    const ExprId integer = push_integer(module);
    const ExprId shared_borrow_temporary = push_unary(module, syntax::UnaryOp::address_of, integer);
    const ExprId shared_borrow_invalid = push_unary(module, syntax::UnaryOp::address_of, syntax::INVALID_EXPR_ID);
    const ExprId source = push_name(module, SEMA_TEST_BODY_FLOW_SOURCE_NAME);
    const ExprId logical_not_source = push_unary(module, syntax::UnaryOp::logical_not, source);
    const ExprId shared_borrow_non_deref_unary = push_unary(module, syntax::UnaryOp::address_of, logical_not_source);
    const ExprId scoped_name =
        module.push_name_expr({}, SEMA_TEST_BODY_FLOW_MODULE_NAME, {}, SEMA_TEST_BODY_FLOW_SOURCE_NAME);

    syntax::StmtNode local_stmt;
    local_stmt.kind = syntax::StmtKind::let;
    local_stmt.name = "edge_local";
    local_stmt.name_id = module.intern_identifier(local_stmt.name);
    const syntax::StmtId local_stmt_id = module.push_stmt(local_stmt);

    syntax::StmtNode empty_if_stmt;
    empty_if_stmt.kind = syntax::StmtKind::if_;
    empty_if_stmt.condition = bool_expr;
    const syntax::StmtId empty_if_stmt_id = module.push_stmt(empty_if_stmt);

    syntax::StmtNode invalid_expr_stmt;
    invalid_expr_stmt.kind = syntax::StmtKind::expr;
    invalid_expr_stmt.init = invalid_expr;
    const syntax::StmtId invalid_expr_stmt_id = module.push_stmt(invalid_expr_stmt);

    syntax::StmtNode shared_borrow_stmt;
    shared_borrow_stmt.kind = syntax::StmtKind::expr;
    shared_borrow_stmt.init = shared_borrow_temporary;
    const syntax::StmtId shared_borrow_stmt_id = module.push_stmt(shared_borrow_stmt);

    syntax::StmtNode invalid_borrow_stmt;
    invalid_borrow_stmt.kind = syntax::StmtKind::expr;
    invalid_borrow_stmt.init = shared_borrow_invalid;
    const syntax::StmtId invalid_borrow_stmt_id = module.push_stmt(invalid_borrow_stmt);

    syntax::StmtNode logical_not_stmt;
    logical_not_stmt.kind = syntax::StmtKind::expr;
    logical_not_stmt.init = logical_not_source;
    const syntax::StmtId logical_not_stmt_id = module.push_stmt(logical_not_stmt);

    syntax::StmtNode non_deref_borrow_stmt;
    non_deref_borrow_stmt.kind = syntax::StmtKind::expr;
    non_deref_borrow_stmt.init = shared_borrow_non_deref_unary;
    const syntax::StmtId non_deref_borrow_stmt_id = module.push_stmt(non_deref_borrow_stmt);

    syntax::StmtNode scoped_return_stmt;
    scoped_return_stmt.kind = syntax::StmtKind::return_;
    scoped_return_stmt.return_value = scoped_name;
    const syntax::StmtId scoped_return_stmt_id = module.push_stmt(scoped_return_stmt);

    const syntax::StmtId body = push_block(module,
        {
            local_stmt_id,
            empty_if_stmt_id,
            invalid_expr_stmt_id,
            shared_borrow_stmt_id,
            invalid_borrow_stmt_id,
            logical_not_stmt_id,
            non_deref_borrow_stmt_id,
            scoped_return_stmt_id,
        });

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME;
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key = semantic_function_key(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME);
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    const auto found = analyzer.state_.checked.body_flow_graphs.find(key);
    ASSERT_NE(found, analyzer.state_.checked.body_flow_graphs.end());
    const sema::BodyFlowGraph& graph = found->second;
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::branch;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.places, [](const sema::BodyFlowPlace& place) {
        return place.root_kind == sema::BodyFlowPlaceRootKind::temporary;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.places, [](const sema::BodyFlowPlace& place) {
        return place.root_kind == sema::BodyFlowPlaceRootKind::unknown;
    }));

    const std::string dump = sema::dump_body_flow_graph(graph);
    EXPECT_NE(dump.find("borrow_shared"), std::string::npos);
    EXPECT_NE(dump.find("branch"), std::string::npos);
    EXPECT_NE(dump.find("temporary"), std::string::npos);
    EXPECT_NE(dump.find("unknown"), std::string::npos);

    sema::BodyFlowGraph manual_graph;
    manual_graph.collect_only = false;
    const std::string manual_dump = sema::dump_body_flow_graph(manual_graph);
    EXPECT_NE(manual_dump.find("collect_only=false"), std::string::npos);
    EXPECT_NE(manual_dump.find("function=4294967295:4294967295:-"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowCollectsCastLikeStringOperand)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const IdentId bytes_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_BYTES_NAME);
    const ExprId bytes = push_name(module, SEMA_TEST_BODY_FLOW_BYTES_NAME);
    const ExprId checked_utf8 =
        module.push_cast_like_expr(syntax::ExprKind::str_from_utf8_checked, {}, syntax::INVALID_TYPE_ID, bytes);

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = checked_utf8;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME;
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key = semantic_function_key(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME);
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    const auto found = analyzer.state_.checked.body_flow_graphs.find(key);
    ASSERT_NE(found, analyzer.state_.checked.body_flow_graphs.end());
    const sema::BodyFlowGraph& graph = found->second;
    EXPECT_TRUE(graph.collect_only);
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph, bytes_id](const sema::BodyFlowAction& action) {
        if (action.kind != sema::BodyFlowActionKind::read || action.place == sema::SEMA_BODY_FLOW_INVALID_INDEX
            || action.place >= graph.places.size()) {
            return false;
        }
        const sema::BodyFlowPlace& place = graph.places[action.place];
        return place.root_kind == sema::BodyFlowPlaceRootKind::local && place.root_name_id == bytes_id;
    }));
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowHandlesSparseAstReferencesCollectOnly)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const syntax::ExprId out_of_range_expr{SEMA_TEST_BODY_FLOW_OUT_OF_RANGE_NODE};
    const syntax::StmtId out_of_range_stmt{SEMA_TEST_BODY_FLOW_OUT_OF_RANGE_NODE};
    const ExprId shared_borrow_out_of_range = push_unary(module, syntax::UnaryOp::address_of, out_of_range_expr);

    syntax::StmtNode shared_borrow_stmt;
    shared_borrow_stmt.kind = syntax::StmtKind::expr;
    shared_borrow_stmt.init = shared_borrow_out_of_range;
    const syntax::StmtId shared_borrow_stmt_id = module.push_stmt(shared_borrow_stmt);
    const syntax::StmtId body = push_block(module, {out_of_range_stmt, shared_borrow_stmt_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME;
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key = semantic_function_key(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME);
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    const auto found = analyzer.state_.checked.body_flow_graphs.find(key);
    ASSERT_NE(found, analyzer.state_.checked.body_flow_graphs.end());
    const sema::BodyFlowGraph& graph = found->second;
    EXPECT_TRUE(graph.collect_only);
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.places, [](const sema::BodyFlowPlace& place) {
        return place.root_kind == sema::BodyFlowPlaceRootKind::unknown;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowHandlesOutOfRangeBodyCollectOnly)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const syntax::StmtId out_of_range_body{SEMA_TEST_BODY_FLOW_OUT_OF_RANGE_NODE};
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME;
    function.body = out_of_range_body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key = semantic_function_key(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME);
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    const auto found = analyzer.state_.checked.body_flow_graphs.find(key);
    ASSERT_NE(found, analyzer.state_.checked.body_flow_graphs.end());
    const sema::BodyFlowGraph& graph = found->second;
    EXPECT_TRUE(graph.collect_only);
    EXPECT_EQ(graph.points.size(), 2U);
    EXPECT_EQ(graph.edges.size(), 1U);
    EXPECT_TRUE(graph.actions.empty());
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxAnalyzePopulatesCollectOnlyBodyFlowFacts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId one = push_integer(module);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = one;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    const IdentId function_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_INTEGRATED_FUNCTION_NAME);
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_INTEGRATED_FUNCTION_NAME;
    function.name_id = function_id;
    function.return_type = i32_type;
    function.body = body;
    const syntax::ItemId item = module.push_item(function);
    module.item_modules[item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const sema::CheckedModule& checked = checked_result.value();
    const auto found = checked.body_flow_graphs.find(key);
    ASSERT_NE(found, checked.body_flow_graphs.end());
    const sema::BodyFlowGraph& graph = found->second;
    EXPECT_TRUE(graph.collect_only);
    EXPECT_EQ(graph.body.value, body.value);
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::return_;
    }));
}

TEST(CoreUnit, SemanticWhiteBoxAnalyzeCanDiscardRetainedBodyFlowGraphs)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId one = push_integer(module);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = one;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);

    const IdentId function_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_DISCARD_FUNCTION_NAME);
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DISCARD_FUNCTION_NAME;
    function.name_id = function_id;
    function.return_type = i32_type;
    function.body = push_block(module, {return_stmt_id});
    const syntax::ItemId item = module.push_item(function);
    module.item_modules[item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    sema::SemanticOptions options;
    options.retain_body_flow_graphs = false;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics, options);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const sema::CheckedModule& checked = checked_result.value();
    EXPECT_FALSE(checked.body_flow_graphs.contains(key));
    EXPECT_TRUE(checked.body_loan_checks.contains(key));
    EXPECT_TRUE(checked.borrow_summaries.contains(key));
}

TEST(CoreUnit, SemanticWhiteBoxDiscardedBodyFlowStillChecksReferenceLoans)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId function_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_DISCARD_REF_FUNCTION_NAME);
    const ExprId one = push_integer(module);
    const syntax::StmtId value_stmt =
        push_local_stmt(module, syntax::StmtKind::var, SEMA_TEST_BODY_LOAN_VALUE_NAME, i32_type, one);
    const ExprId value_name = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, value_name);
    const syntax::StmtId ref_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, ref_i32_type, shared_borrow);
    const ExprId ref_name = push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME);
    const ExprId deref_ref = push_unary(module, syntax::UnaryOp::dereference, ref_name);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = deref_ref;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DISCARD_REF_FUNCTION_NAME;
    function.name_id = function_id;
    function.return_type = i32_type;
    function.body = push_block(module, {value_stmt, ref_stmt, return_stmt_id});
    const syntax::ItemId item = module.push_item(function);
    module.item_modules[item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    sema::SemanticOptions options;
    options.retain_body_flow_graphs = false;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics, options);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const sema::CheckedModule& checked = checked_result.value();
    EXPECT_FALSE(checked.body_flow_graphs.contains(key));
    ASSERT_TRUE(checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = checked.body_loan_checks.at(key);
    EXPECT_EQ(result.diagnostic_mode, sema::BodyLoanDiagnosticMode::enforced);
    EXPECT_EQ(result.loans.size(), 1U);
    EXPECT_TRUE(result.conflicts.empty());
    EXPECT_TRUE(checked.borrow_summaries.contains(key));
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanKindNamesAndMissingGraphFacts)
{
    EXPECT_EQ(sema::body_loan_kind_name(sema::BodyLoanKind::shared), "shared");
    EXPECT_EQ(sema::body_loan_kind_name(sema::BodyLoanKind::mutable_), "mutable");
    EXPECT_EQ(
        sema::body_loan_kind_name(static_cast<sema::BodyLoanKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)), "<invalid>");
    EXPECT_EQ(sema::body_loan_origin_kind_name(sema::BodyLoanOriginKind::none), "none");
    EXPECT_EQ(sema::body_loan_origin_kind_name(sema::BodyLoanOriginKind::local), "local");
    EXPECT_EQ(sema::body_loan_origin_kind_name(sema::BodyLoanOriginKind::temporary), "temporary");
    EXPECT_EQ(sema::body_loan_origin_kind_name(sema::BodyLoanOriginKind::unknown), "unknown");
    EXPECT_EQ(
        sema::body_loan_origin_kind_name(static_cast<sema::BodyLoanOriginKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::body_loan_diagnostic_mode_name(sema::BodyLoanDiagnosticMode::shadow), "shadow");
    EXPECT_EQ(sema::body_loan_diagnostic_mode_name(sema::BodyLoanDiagnosticMode::enforced), "enforced");
    EXPECT_EQ(sema::body_loan_diagnostic_mode_name(
                  static_cast<sema::BodyLoanDiagnosticMode>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::read), "read");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::write), "write");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::reinit), "reinit");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::move), "move");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::drop), "drop");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::shared_borrow), "shared_borrow");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::mutable_borrow), "mutable_borrow");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::cleanup), "cleanup");
    EXPECT_EQ(
        sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::two_phase_reservation), "two_phase_reservation");
    EXPECT_EQ(
        sema::body_loan_conflict_kind_name(sema::BodyLoanConflictKind::two_phase_activation), "two_phase_activation");
    EXPECT_EQ(sema::body_loan_conflict_kind_name(
                  static_cast<sema::BodyLoanConflictKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");

    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key = semantic_function_key(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);

    const auto found = analyzer.state_.checked.body_loan_checks.find(key);
    ASSERT_NE(found, analyzer.state_.checked.body_loan_checks.end());
    EXPECT_TRUE(found->second.graph_missing);
    const std::string dump = sema::dump_body_loan_check_result(found->second);
    EXPECT_NE(dump.find("graph_missing=true"), std::string::npos);

    sema::BodyLoanCheckResult manual_result;
    manual_result.diagnostic_mode = sema::BodyLoanDiagnosticMode::enforced;
    manual_result.origins.push_back(sema::BodyLoanOrigin{});
    manual_result.loans.push_back(sema::BodyLoan{});
    manual_result.conflicts.push_back(sema::BodyLoanConflict{});
    manual_result.conflicts.front().diagnostic_emitted = true;
    const std::string manual_dump = sema::dump_body_loan_check_result(manual_result);
    EXPECT_NE(manual_dump.find("function=4294967295:4294967295:-"), std::string::npos);
    EXPECT_NE(manual_dump.find("mode=enforced"), std::string::npos);
    EXPECT_NE(manual_dump.find("expr=-"), std::string::npos);
    EXPECT_NE(manual_dump.find("stmt=-"), std::string::npos);
    EXPECT_NE(manual_dump.find("emitted=true"), std::string::npos);
    EXPECT_NE(manual_dump.find("later_use=p4294967295"), std::string::npos);
    EXPECT_NE(manual_dump.find("fingerprint="), std::string::npos);
    sema::BodyLoanCheckResult range_changed_manual_result = manual_result;
    range_changed_manual_result.conflicts.front().range = body_loan_test_range(1);
    range_changed_manual_result.conflicts.front().later_use_range = body_loan_test_range(2);
    EXPECT_EQ(sema::body_loan_check_fingerprint(manual_result),
        sema::body_loan_check_fingerprint(range_changed_manual_result));
    sema::BodyLoanCheckResult later_point_changed_manual_result = manual_result;
    later_point_changed_manual_result.conflicts.front().later_use_point = 1;
    EXPECT_NE(sema::body_loan_check_fingerprint(manual_result),
        sema::body_loan_check_fingerprint(later_point_changed_manual_result));
    sema::BodyLoanCheckResult changed_manual_result = manual_result;
    changed_manual_result.conflicts.front().kind = sema::BodyLoanConflictKind::read;
    EXPECT_NE(
        sema::body_loan_check_fingerprint(manual_result), sema::body_loan_check_fingerprint(changed_manual_result));
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanTracksReborrowParentUseConflict)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId parent_id = module.intern_identifier("parent_ref");
    const IdentId child_id = module.intern_identifier("child_ref");
    const ExprId value_expr = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId parent_borrow = push_unary(module, syntax::UnaryOp::address_of_mut, value_expr);
    const syntax::StmtId parent_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "parent_ref", ref_i32_type, parent_borrow);
    const ExprId parent_expr = push_name(module, "parent_ref");
    const ExprId deref_parent = push_unary(module, syntax::UnaryOp::dereference, parent_expr);
    const ExprId child_borrow = push_unary(module, syntax::UnaryOp::address_of, deref_parent);
    const syntax::StmtId child_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "child_ref", ref_i32_type, child_borrow);
    const ExprId parent_use = push_name(module, "parent_ref");
    const ExprId child_use = push_name(module, "child_ref");
    const syntax::StmtId body = push_block(module, {parent_stmt, child_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "reborrow_parent_use";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "reborrow_parent_use");

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.body = body;
    for (base::usize index = 0; index < 4; ++index) {
        graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(index)});
    }
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0, .to = 1});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 2});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 2, .to = 3});
    graph.places.push_back(body_loan_local_place(value_id, value_expr, 0));
    graph.places.push_back(body_loan_local_place(parent_id, parent_expr, 1));
    graph.places.push_back(body_loan_deref_place(parent_id, parent_expr, deref_parent, 2));
    graph.places.push_back(body_loan_local_place(child_id, child_use, 3));
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_mutable,
        .point = 0,
        .place = 0,
        .stmt = parent_stmt,
        .expr = value_expr,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 1,
        .stmt = parent_stmt,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 1,
        .place = 2,
        .stmt = child_stmt,
        .expr = deref_parent,
        .range = body_loan_test_range(2),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 1,
        .place = 3,
        .stmt = child_stmt,
        .range = body_loan_test_range(3),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 2,
        .place = 1,
        .expr = parent_use,
        .range = body_loan_test_range(4),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 3,
        .place = 3,
        .expr = child_use,
        .range = body_loan_test_range(5),
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::enforced);
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    ASSERT_GE(result.loans.size(), 2U);
    const auto child = std::ranges::find_if(result.loans, [](const sema::BodyLoan& loan) {
        return loan.parent_loan != sema::SEMA_BODY_LOAN_INVALID_INDEX;
    });
    ASSERT_NE(child, result.loans.end());
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::reborrow_parent_use && conflict.diagnostic_emitted;
    }));
    const std::string dump = sema::dump_body_loan_check_result(result);
    EXPECT_NE(dump.find("parent=l"), std::string::npos);
    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_REBORROW_PARENT_USE_CONFLICT), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_REBORROW_CHILD_CREATED), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanReborrowEffectivePlaceKeepsDisjointFieldsSeparate)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type, syntax::PointerMutability::mut));
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId parent_id = module.intern_identifier("parent_ref");
    const IdentId child_id = module.intern_identifier("child_ref");
    const IdentId field_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const IdentId other_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_OTHER_FIELD_NAME);
    const ExprId value_expr = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId parent_borrow = push_unary(module, syntax::UnaryOp::address_of_mut, value_expr);
    const syntax::StmtId parent_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "parent_ref", ref_i32_type, parent_borrow);
    const ExprId parent_expr = push_name(module, "parent_ref");
    const ExprId deref_parent = push_unary(module, syntax::UnaryOp::dereference, parent_expr);
    const ExprId field_expr = push_field(module, deref_parent, SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const ExprId child_borrow = push_unary(module, syntax::UnaryOp::address_of_mut, field_expr);
    const syntax::StmtId child_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "child_ref", ref_i32_type, child_borrow);
    const ExprId child_use = push_name(module, "child_ref");
    const syntax::StmtId body = push_block(module, {parent_stmt, child_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "reborrow_disjoint_field";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "reborrow_disjoint_field");

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.body = body;
    for (base::usize index = 0; index < 4; ++index) {
        graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(index)});
    }
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0, .to = 1});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 2});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 2, .to = 3});
    graph.places.push_back(body_loan_local_place(value_id, value_expr, 0));
    graph.places.push_back(body_loan_local_place(parent_id, parent_expr, 1));
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = parent_id,
        .root_expr = parent_expr,
        .projections =
            {
                sema::BodyFlowPlaceProjection{
                    .kind = sema::BodyFlowPlaceProjectionKind::dereference,
                    .expr = deref_parent,
                },
                sema::BodyFlowPlaceProjection{
                    .kind = sema::BodyFlowPlaceProjectionKind::field,
                    .field_name_id = field_id,
                    .expr = field_expr,
                },
            },
        .range = body_loan_test_range(2),
    });
    graph.places.push_back(body_loan_local_place(child_id, child_use, 3));
    graph.places.push_back(body_loan_field_place(value_id, other_id, 4));
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_mutable,
        .point = 0,
        .place = 0,
        .stmt = parent_stmt,
        .expr = value_expr,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 1,
        .stmt = parent_stmt,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_mutable,
        .point = 1,
        .place = 2,
        .stmt = child_stmt,
        .expr = field_expr,
        .range = body_loan_test_range(2),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 1,
        .place = 3,
        .stmt = child_stmt,
        .range = body_loan_test_range(3),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 2,
        .place = 4,
        .range = body_loan_test_range(4),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 3,
        .place = 3,
        .expr = child_use,
        .range = body_loan_test_range(5),
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    ASSERT_TRUE(std::ranges::any_of(result.loans, [](const sema::BodyLoan& loan) {
        return loan.parent_loan != sema::SEMA_BODY_LOAN_INVALID_INDEX;
    }));
    EXPECT_TRUE(result.conflicts.empty()) << sema::dump_body_loan_check_result(result);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanTwoPhaseReservationAndActivationConflicts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId value_expr = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId call_expr = push_call(module, push_field(module, value_expr, "push"), {push_integer(module)});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "two_phase_manual";
    const sema::FunctionLookupKey read_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "two_phase_read");
    const sema::FunctionLookupKey write_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "two_phase_write");

    const auto make_two_phase_graph = [&](const sema::BodyFlowActionKind middle_kind) {
        sema::BodyFlowGraph graph;
        graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(0)});
        graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(1)});
        graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(2)});
        graph.edges.push_back(sema::BodyFlowEdge{.from = 0, .to = 1});
        graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 2});
        graph.places.push_back(body_loan_local_place(value_id, value_expr, 0));
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = sema::BodyFlowActionKind::call_receiver_reserve,
            .point = 0,
            .place = 0,
            .expr = call_expr,
            .range = body_loan_test_range(0),
        });
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = middle_kind,
            .point = 1,
            .place = 0,
            .expr = value_expr,
            .range = body_loan_test_range(1),
        });
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = sema::BodyFlowActionKind::call_receiver_activate,
            .point = 2,
            .place = 0,
            .expr = call_expr,
            .range = body_loan_test_range(2),
        });
        return graph;
    };

    sema::BodyFlowGraph read_graph = make_two_phase_graph(sema::BodyFlowActionKind::read);
    read_graph.function = read_key;
    analyzer.state_.checked.body_flow_graphs[read_key] = std::move(read_graph);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(
        function, read_key, sema::BodyLoanDiagnosticMode::shadow);
    const sema::BodyLoanCheckResult& read_result = analyzer.state_.checked.body_loan_checks.at(read_key);
    EXPECT_EQ(read_result.two_phase_borrows.size(), 1U);
    EXPECT_TRUE(read_result.conflicts.empty());

    sema::BodyFlowGraph write_graph = make_two_phase_graph(sema::BodyFlowActionKind::write);
    write_graph.function = write_key;
    analyzer.state_.checked.body_flow_graphs[write_key] = std::move(write_graph);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(
        function, write_key, sema::BodyLoanDiagnosticMode::shadow);
    const sema::BodyLoanCheckResult& write_result = analyzer.state_.checked.body_loan_checks.at(write_key);
    EXPECT_EQ(write_result.two_phase_borrows.size(), 1U);
    EXPECT_TRUE(std::ranges::any_of(write_result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::two_phase_reservation;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanTwoPhaseActivationConflictsWithActiveLoan)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId ref_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_REF_NAME);
    const ExprId value_expr = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, value_expr);
    const syntax::StmtId ref_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, ref_i32_type, shared_borrow);
    const ExprId ref_use = push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME);
    const ExprId call_expr = push_call(module, push_field(module, value_expr, "push"), {push_integer(module)});
    const syntax::StmtId body = push_block(module, {ref_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "two_phase_activation";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "two_phase_activation");

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.body = body;
    for (base::usize index = 0; index < 4; ++index) {
        graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(index)});
    }
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0, .to = 1});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 2});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 2, .to = 3});
    graph.places.push_back(body_loan_local_place(value_id, value_expr, 0));
    graph.places.push_back(body_loan_local_place(ref_id, ref_use, 1));
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 0,
        .stmt = ref_stmt,
        .expr = value_expr,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 1,
        .stmt = ref_stmt,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::call_receiver_reserve,
        .point = 1,
        .place = 0,
        .expr = call_expr,
        .range = body_loan_test_range(2),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::call_receiver_activate,
        .point = 2,
        .place = 0,
        .expr = call_expr,
        .range = body_loan_test_range(3),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 3,
        .place = 1,
        .expr = ref_use,
        .range = body_loan_test_range(4),
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    EXPECT_EQ(result.two_phase_borrows.size(), 1U);
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::two_phase_activation;
    }));
    const std::string dump = sema::dump_body_loan_check_result(result);
    EXPECT_NE(dump.find("two_phase_borrows:"), std::string::npos);
    EXPECT_NE(dump.find("reserve=a"), std::string::npos);
    EXPECT_NE(dump.find("activate=a"), std::string::npos);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanSharedReborrowParentReadAndWriteAccess)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId parent_id = module.intern_identifier("parent_ref");
    const IdentId child_id = module.intern_identifier("child_ref");
    const ExprId value_expr = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId parent_borrow = push_unary(module, syntax::UnaryOp::address_of, value_expr);
    const syntax::StmtId parent_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "parent_ref", ref_i32_type, parent_borrow);
    const ExprId parent_expr = push_name(module, "parent_ref");
    const ExprId deref_parent = push_unary(module, syntax::UnaryOp::dereference, parent_expr);
    const ExprId child_borrow = push_unary(module, syntax::UnaryOp::address_of, deref_parent);
    const syntax::StmtId child_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "child_ref", ref_i32_type, child_borrow);
    const ExprId parent_use = push_name(module, "parent_ref");
    const ExprId child_use = push_name(module, "child_ref");
    const syntax::StmtId body = push_block(module, {parent_stmt, child_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "shared_reborrow_parent_access";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const auto make_graph = [&](const sema::FunctionLookupKey& key, const sema::BodyFlowActionKind parent_access) {
        sema::BodyFlowGraph graph;
        graph.function = key;
        graph.body = body;
        for (base::usize index = 0; index < 4; ++index) {
            graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(index)});
        }
        graph.edges.push_back(sema::BodyFlowEdge{.from = 0, .to = 1});
        graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 2});
        graph.edges.push_back(sema::BodyFlowEdge{.from = 2, .to = 3});
        graph.places.push_back(body_loan_local_place(value_id, value_expr, 0));
        graph.places.push_back(body_loan_local_place(parent_id, parent_expr, 1));
        graph.places.push_back(body_loan_deref_place(parent_id, parent_expr, deref_parent, 2));
        graph.places.push_back(body_loan_local_place(child_id, child_use, 3));
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = sema::BodyFlowActionKind::borrow_shared,
            .point = 0,
            .place = 0,
            .stmt = parent_stmt,
            .expr = value_expr,
            .range = body_loan_test_range(0),
        });
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = sema::BodyFlowActionKind::write,
            .point = 0,
            .place = 1,
            .stmt = parent_stmt,
            .range = body_loan_test_range(1),
        });
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = sema::BodyFlowActionKind::borrow_shared,
            .point = 1,
            .place = 2,
            .stmt = child_stmt,
            .expr = deref_parent,
            .range = body_loan_test_range(2),
        });
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = sema::BodyFlowActionKind::write,
            .point = 1,
            .place = 3,
            .stmt = child_stmt,
            .range = body_loan_test_range(3),
        });
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = parent_access,
            .point = 2,
            .place = 1,
            .expr = parent_use,
            .range = body_loan_test_range(4),
        });
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = sema::BodyFlowActionKind::read,
            .point = 3,
            .place = 3,
            .expr = child_use,
            .range = body_loan_test_range(5),
        });
        return graph;
    };

    const sema::FunctionLookupKey read_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "shared_reborrow_parent_read");
    analyzer.state_.checked.body_flow_graphs[read_key] = make_graph(read_key, sema::BodyFlowActionKind::read);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(
        function, read_key, sema::BodyLoanDiagnosticMode::shadow);
    const sema::BodyLoanCheckResult& read_result = analyzer.state_.checked.body_loan_checks.at(read_key);
    ASSERT_TRUE(std::ranges::any_of(read_result.loans, [](const sema::BodyLoan& loan) {
        return loan.parent_loan != sema::SEMA_BODY_LOAN_INVALID_INDEX;
    }));
    EXPECT_TRUE(read_result.conflicts.empty()) << sema::dump_body_loan_check_result(read_result);

    const sema::FunctionLookupKey write_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "shared_reborrow_parent_write");
    analyzer.state_.checked.body_flow_graphs[write_key] = make_graph(write_key, sema::BodyFlowActionKind::write);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(
        function, write_key, sema::BodyLoanDiagnosticMode::shadow);
    const sema::BodyLoanCheckResult& write_result = analyzer.state_.checked.body_loan_checks.at(write_key);
    EXPECT_TRUE(std::ranges::any_of(write_result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::reborrow_parent_use;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanTwoPhaseSkipsReserveWithoutFreshActivation)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId value_expr = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId call_expr = push_call(module, push_field(module, value_expr, "push"), {push_integer(module)});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "two_phase_stale_activation");

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(0)});
    graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(1)});
    graph.points.push_back(sema::BodyFlowPoint{.range = body_loan_test_range(2)});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0, .to = 1});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 2});
    graph.places.push_back(body_loan_local_place(value_id, value_expr, 0));
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::call_receiver_reserve,
        .point = 0,
        .place = 0,
        .expr = call_expr,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::call_receiver_activate,
        .point = 1,
        .place = 0,
        .expr = call_expr,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::call_receiver_reserve,
        .point = 2,
        .place = 0,
        .expr = call_expr,
        .range = body_loan_test_range(2),
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "two_phase_stale_activation";
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    EXPECT_EQ(result.two_phase_borrows.size(), 1U);
    EXPECT_TRUE(result.conflicts.empty()) << sema::dump_body_loan_check_result(result);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanShadowRecordsActiveSharedWriteConflict)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId borrowed_value = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, borrowed_value);
    const syntax::StmtId borrow_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, i32_type, shared_borrow);
    const syntax::StmtId write_stmt =
        push_assign_stmt(module, push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME), push_integer(module));
    syntax::StmtNode use_stmt;
    use_stmt.kind = syntax::StmtKind::expr;
    use_stmt.init = push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME);
    const syntax::StmtId use_stmt_id = module.push_stmt(use_stmt);
    const syntax::StmtId body = push_block(module, {borrow_stmt, write_stmt, use_stmt_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME;
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key = semantic_function_key(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME);
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);

    const auto found = analyzer.state_.checked.body_loan_checks.find(key);
    ASSERT_NE(found, analyzer.state_.checked.body_loan_checks.end());
    const sema::BodyLoanCheckResult& result = found->second;
    EXPECT_EQ(result.diagnostic_mode, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_EQ(result.loans.size(), 1U);
    EXPECT_EQ(result.loans.front().kind, sema::BodyLoanKind::shared);
    EXPECT_TRUE(syntax::is_valid(result.loans.front().carrier_name_id));
    ASSERT_FALSE(result.conflicts.empty());
    EXPECT_EQ(result.conflicts.front().kind, sema::BodyLoanConflictKind::reinit);
    EXPECT_FALSE(result.conflicts.front().diagnostic_emitted);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const std::string dump = sema::dump_body_loan_check_result(result);
    EXPECT_NE(dump.find("mode=shadow"), std::string::npos);
    EXPECT_NE(dump.find("shared"), std::string::npos);
    EXPECT_NE(dump.find("reinit"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanAllowsLastUseAndDisjointFieldWrite)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId field_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const IdentId other_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_OTHER_FIELD_NAME);
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId borrowed_field =
        push_field(module, push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME), SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, borrowed_field);
    const syntax::StmtId borrow_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, i32_type, shared_borrow);
    syntax::StmtNode use_stmt;
    use_stmt.kind = syntax::StmtKind::expr;
    use_stmt.init = push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME);
    const syntax::StmtId use_stmt_id = module.push_stmt(use_stmt);
    const syntax::StmtId whole_write_stmt =
        push_assign_stmt(module, push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME), push_integer(module));
    const syntax::StmtId disjoint_field_write_stmt = push_assign_stmt(module,
        push_field(module, push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME), SEMA_TEST_BODY_FLOW_OTHER_FIELD_NAME),
        push_integer(module));

    syntax::ItemNode last_use_function;
    last_use_function.kind = syntax::ItemKind::fn_decl;
    last_use_function.name = SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME;
    last_use_function.body = push_block(module, {borrow_stmt, use_stmt_id, whole_write_stmt});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey last_use_key = semantic_function_key(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_BODY_FLOW_DIRECT_FUNCTION_NAME);
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(last_use_function, last_use_key);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(
        last_use_function, last_use_key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(last_use_key));
    EXPECT_TRUE(analyzer.state_.checked.body_loan_checks.at(last_use_key).conflicts.empty());

    syntax::ItemNode disjoint_function;
    disjoint_function.kind = syntax::ItemKind::fn_decl;
    disjoint_function.name = "flow_disjoint";
    disjoint_function.body = push_block(module, {borrow_stmt, disjoint_field_write_stmt, use_stmt_id});
    const sema::FunctionLookupKey disjoint_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_disjoint");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(disjoint_function, disjoint_key);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(
        disjoint_function, disjoint_key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(disjoint_key));
    const sema::BodyLoanCheckResult& disjoint_result = analyzer.state_.checked.body_loan_checks.at(disjoint_key);
    ASSERT_EQ(disjoint_result.loans.size(), 1U);
    ASSERT_LT(
        disjoint_result.loans.front().place, analyzer.state_.checked.body_flow_graphs.at(disjoint_key).places.size());
    const sema::BodyFlowPlace& place =
        analyzer.state_.checked.body_flow_graphs.at(disjoint_key).places[disjoint_result.loans.front().place];
    EXPECT_EQ(place.root_name_id, value_id);
    ASSERT_EQ(place.projections.size(), 1U);
    EXPECT_EQ(place.projections.front().field_name_id, field_id);
    EXPECT_NE(field_id, other_id);
    EXPECT_TRUE(disjoint_result.conflicts.empty());
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanBindsCarrierThroughValueResultShapes)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const ExprId field_source = push_name(module, "field_source");
    const ExprId field_init = push_field(module, field_source, SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const syntax::StmtId field_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "field_carrier", ref_i32_type, field_init);

    const ExprId index_source = push_name(module, "index_source");
    const ExprId index_init = module.push_index_expr({}, syntax::IndexExprPayload{index_source, push_integer(module)});
    const syntax::StmtId index_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "index_carrier", ref_i32_type, index_init);

    const ExprId try_source = push_name(module, "try_source");
    const ExprId try_init = module.push_try_expr({}, try_source);
    const syntax::StmtId try_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "try_carrier", ref_i32_type, try_init);

    syntax::PatternNode wildcard;
    wildcard.kind = syntax::PatternKind::wildcard;
    const syntax::PatternId wildcard_id = module.push_pattern(wildcard);
    const ExprId match_source = push_name(module, "match_source");
    const ExprId match_init = module.push_match_expr({}, push_integer(module),
        std::vector<syntax::MatchArm>{syntax::MatchArm{wildcard_id, syntax::INVALID_EXPR_ID, match_source, {}}});
    const syntax::StmtId match_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "match_carrier", ref_i32_type, match_init);

    const ExprId index_assign_source = push_name(module, "index_assign_source");
    const ExprId index_assign_lhs = module.push_index_expr(
        {}, syntax::IndexExprPayload{push_name(module, "index_assign_carrier"), push_integer(module)});
    const syntax::StmtId index_assign_stmt = push_assign_stmt(module, index_assign_lhs, index_assign_source);

    const ExprId slice_assign_source = push_name(module, "slice_assign_source");
    const ExprId slice_assign_lhs = module.push_slice_expr({},
        syntax::SliceExprPayload{
            push_name(module, "slice_assign_carrier"), push_integer(module), push_integer(module)});
    const syntax::StmtId slice_assign_stmt = push_assign_stmt(module, slice_assign_lhs, slice_assign_source);

    const ExprId deref_assign_source = push_name(module, "deref_assign_source");
    const ExprId deref_assign_lhs =
        push_unary(module, syntax::UnaryOp::dereference, push_name(module, "deref_assign_carrier"));
    const syntax::StmtId deref_assign_stmt = push_assign_stmt(module, deref_assign_lhs, deref_assign_source);

    const syntax::StmtId body = push_block(module,
        {field_stmt, index_stmt, try_stmt, match_stmt, index_assign_stmt, slice_assign_stmt, deref_assign_stmt});
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "value_result_carriers";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = analyzer.state_.checked.types.reference(PointerMutability::const_, i32);
    const auto mark_ref = [&analyzer, ref_i32](const ExprId expr) {
        analyzer.state_.checked.expr_types[expr.value] = ref_i32;
    };
    for (const ExprId expr : {field_source, field_init, index_source, index_init, try_source, try_init, match_source,
             match_init, index_assign_source, slice_assign_source, deref_assign_source}) {
        mark_ref(expr);
    }

    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "value_result_carriers");
    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.body = body;
    graph.points.push_back(sema::BodyFlowPoint{.stmt = body, .range = body_loan_test_range(0)});
    const auto add_loan = [&graph](const IdentId source_id, const ExprId source, const syntax::StmtId stmt,
                              const base::usize range_index) {
        const base::u32 place = base::checked_u32(graph.places.size(), "test body loan place");
        graph.places.push_back(body_loan_local_place(source_id, source, range_index));
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = sema::BodyFlowActionKind::borrow_shared,
            .point = 0,
            .place = place,
            .stmt = stmt,
            .expr = source,
            .range = body_loan_test_range(range_index),
        });
    };
    add_loan(module.intern_identifier("field_source"), field_source, field_stmt, 1);
    add_loan(module.intern_identifier("index_source"), index_source, index_stmt, 2);
    add_loan(module.intern_identifier("try_source"), try_source, try_stmt, 3);
    add_loan(module.intern_identifier("match_source"), match_source, match_stmt, 4);
    add_loan(module.intern_identifier("index_assign_source"), index_assign_source, index_assign_stmt, 5);
    add_loan(module.intern_identifier("slice_assign_source"), slice_assign_source, slice_assign_stmt, 6);
    add_loan(module.intern_identifier("deref_assign_source"), deref_assign_source, deref_assign_stmt, 7);
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    ASSERT_EQ(result.loans.size(), 7U);
    EXPECT_TRUE(std::ranges::all_of(result.loans, [](const sema::BodyLoan& loan) {
        return syntax::is_valid(loan.carrier_name_id);
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanShadowRecordsBorrowKindConflicts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId shared_operand = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, shared_operand);
    const syntax::StmtId shared_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "shared_ref", i32_type, shared_borrow);
    const ExprId mutable_operand = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId mutable_borrow = push_unary(module, syntax::UnaryOp::address_of_mut, mutable_operand);
    const syntax::StmtId mutable_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "mutable_ref", i32_type, mutable_borrow);
    syntax::StmtNode shared_use_stmt;
    shared_use_stmt.kind = syntax::StmtKind::expr;
    shared_use_stmt.init = push_name(module, "shared_ref");
    const syntax::StmtId shared_use_stmt_id = module.push_stmt(shared_use_stmt);

    syntax::ItemNode mutable_after_shared;
    mutable_after_shared.kind = syntax::ItemKind::fn_decl;
    mutable_after_shared.name = "mutable_after_shared";
    mutable_after_shared.body = push_block(module, {shared_stmt, mutable_stmt, shared_use_stmt_id});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey mutable_after_shared_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "mutable_after_shared");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(mutable_after_shared, mutable_after_shared_key);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(
        mutable_after_shared, mutable_after_shared_key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(mutable_after_shared_key));
    const sema::BodyLoanCheckResult& mutable_after_shared_result =
        analyzer.state_.checked.body_loan_checks.at(mutable_after_shared_key);
    EXPECT_TRUE(std::ranges::any_of(mutable_after_shared_result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::mutable_borrow;
    }));

    syntax::StmtNode mutable_use_stmt;
    mutable_use_stmt.kind = syntax::StmtKind::expr;
    mutable_use_stmt.init = push_name(module, "mutable_ref");
    const syntax::StmtId mutable_use_stmt_id = module.push_stmt(mutable_use_stmt);
    syntax::ItemNode shared_after_mutable;
    shared_after_mutable.kind = syntax::ItemKind::fn_decl;
    shared_after_mutable.name = "shared_after_mutable";
    shared_after_mutable.body = push_block(module, {mutable_stmt, shared_stmt, mutable_use_stmt_id});

    const sema::FunctionLookupKey shared_after_mutable_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "shared_after_mutable");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(shared_after_mutable, shared_after_mutable_key);
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(
        shared_after_mutable, shared_after_mutable_key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(shared_after_mutable_key));
    const sema::BodyLoanCheckResult& shared_after_mutable_result =
        analyzer.state_.checked.body_loan_checks.at(shared_after_mutable_key);
    EXPECT_TRUE(std::ranges::any_of(shared_after_mutable_result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::shared_borrow;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanManualGraphCoversConservativeRoots)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "manual_roots");

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.points.push_back(sema::BodyFlowPoint{});
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::unknown,
        .root_expr = syntax::ExprId{1},
        .projections = {},
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME),
        .root_expr = syntax::ExprId{2},
        .projections = {},
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::temporary,
        .root_expr = syntax::ExprId{3},
        .projections = {},
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::temporary,
        .root_expr = syntax::ExprId{3},
        .projections = {},
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::none,
        .projections = {},
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 0,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 1,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::reinit,
        .point = 0,
        .place = 0,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::drop,
        .point = 0,
        .place = 0,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 0,
        .place = 0,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_mutable,
        .point = 0,
        .place = 2,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 0,
        .place = 3,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 4,
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "manual_roots";
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    ASSERT_GE(result.loans.size(), 3U);
    EXPECT_TRUE(std::ranges::any_of(result.origins, [](const sema::BodyLoanOrigin& origin) {
        return origin.kind == sema::BodyLoanOriginKind::unknown;
    }));
    EXPECT_TRUE(std::ranges::any_of(result.origins, [](const sema::BodyLoanOrigin& origin) {
        return origin.kind == sema::BodyLoanOriginKind::temporary;
    }));
    EXPECT_TRUE(std::ranges::any_of(result.origins, [](const sema::BodyLoanOrigin& origin) {
        return origin.kind == sema::BodyLoanOriginKind::none;
    }));
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::write;
    }));
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::reinit;
    }));
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::drop;
    }));
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::cleanup;
    }));
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::read;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanManualGraphCoversProjectionAndStaleEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "manual_projection_edges");
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId field_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_FIELD_NAME);

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.points.push_back(sema::BodyFlowPoint{
        .range = body_loan_test_range(0),
    });
    graph.edges.push_back(sema::BodyFlowEdge{
        .from = 0,
        .to = sema::SEMA_BODY_FLOW_INVALID_INDEX,
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = value_id,
        .root_expr = syntax::ExprId{1},
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::field,
            .field_name_id = field_id,
        }},
        .range = body_loan_test_range(0),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = value_id,
        .root_expr = syntax::ExprId{1},
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::index,
            .expr = syntax::ExprId{2},
        }},
        .range = body_loan_test_range(1),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::temporary,
        .root_expr = syntax::ExprId{3},
        .projections = {},
        .range = body_loan_test_range(2),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::temporary,
        .root_expr = syntax::ExprId{4},
        .projections = {},
        .range = body_loan_test_range(3),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::none,
        .projections = {},
        .range = body_loan_test_range(4),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = static_cast<sema::BodyFlowActionKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE),
        .point = 0,
        .place = 0,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = sema::SEMA_BODY_FLOW_INVALID_INDEX,
        .place = 0,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 0,
        .range = body_loan_test_range(2),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 1,
        .range = body_loan_test_range(3),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 1,
        .range = body_loan_test_range(4),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 1,
        .range = body_loan_test_range(5),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_mutable,
        .point = 0,
        .place = 2,
        .range = body_loan_test_range(6),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 0,
        .place = 3,
        .range = body_loan_test_range(7),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 4,
        .range = body_loan_test_range(8),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 4,
        .range = body_loan_test_range(9),
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "manual_projection_edges";
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::write && conflict.place == 1;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanLaterUseSearchHandlesRedefinitionAndPriorUse)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId ref_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_REF_NAME);
    const ExprId borrowed_value = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, borrowed_value);
    const syntax::StmtId borrow_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, i32_type, shared_borrow);
    const syntax::StmtId body = push_block(module, {borrow_stmt});
    const ExprId direct_ref_use = push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME);
    const ExprId redefined_ref_use = push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "later_use_redefinition";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "later_use_redefinition");

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.body = body;
    for (base::usize index = 0; index < 5; ++index) {
        graph.points.push_back(sema::BodyFlowPoint{
            .stmt = borrow_stmt,
            .range = body_loan_test_range(index),
        });
    }
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0, .to = 1});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 2});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 3});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 3});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 3, .to = 4});
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = ref_id,
        .projections = {},
        .range = body_loan_test_range(0),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = value_id,
        .root_expr = borrowed_value,
        .projections = {},
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 0,
        .stmt = borrow_stmt,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 1,
        .stmt = borrow_stmt,
        .expr = borrowed_value,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 1,
        .place = 1,
        .range = body_loan_test_range(2),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 3,
        .place = 0,
        .range = body_loan_test_range(3),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 2,
        .place = 0,
        .expr = direct_ref_use,
        .range = body_loan_test_range(4),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 4,
        .place = 0,
        .expr = redefined_ref_use,
        .range = body_loan_test_range(5),
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    ASSERT_FALSE(result.conflicts.empty());
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.later_use_point == 2 && conflict.later_use_range.well_formed();
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanLaterUseSearchSkipsSamePointPriorUse)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId ref_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_REF_NAME);
    const ExprId borrowed_value = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, borrowed_value);
    const syntax::StmtId borrow_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, i32_type, shared_borrow);
    const syntax::StmtId body = push_block(module, {borrow_stmt});
    const ExprId prior_ref_use = push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "same_point_prior_use";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "same_point_prior_use");

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.body = body;
    graph.points.push_back(sema::BodyFlowPoint{.stmt = borrow_stmt, .range = body_loan_test_range(0)});
    graph.points.push_back(sema::BodyFlowPoint{.stmt = borrow_stmt, .range = body_loan_test_range(1)});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0, .to = 1});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1, .to = 1});
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = ref_id,
        .projections = {},
        .range = body_loan_test_range(0),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = value_id,
        .root_expr = borrowed_value,
        .projections = {},
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 0,
        .stmt = borrow_stmt,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 1,
        .stmt = borrow_stmt,
        .expr = borrowed_value,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 1,
        .place = 0,
        .expr = prior_ref_use,
        .range = body_loan_test_range(2),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 1,
        .place = 1,
        .range = body_loan_test_range(3),
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    ASSERT_FALSE(result.conflicts.empty());
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.later_use_point == sema::SEMA_BODY_FLOW_INVALID_INDEX;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanRecordsLexicalCleanupStorageConflict)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const syntax::StmtId carrier_decl =
        push_local_stmt(module, syntax::StmtKind::var, SEMA_TEST_BODY_LOAN_REF_NAME, ref_i32_type);
    const syntax::StmtId local_decl =
        push_local_stmt(module, syntax::StmtKind::var, SEMA_TEST_BODY_LOAN_VALUE_NAME, i32_type, push_integer(module));
    const ExprId borrow_local =
        push_unary(module, syntax::UnaryOp::address_of, push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME));
    const syntax::StmtId assign_carrier =
        push_assign_stmt(module, push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME), borrow_local);
    const syntax::StmtId inner_block = push_block(module, {local_decl, assign_carrier});
    syntax::StmtNode carrier_use_stmt;
    carrier_use_stmt.kind = syntax::StmtKind::expr;
    carrier_use_stmt.init = push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME);
    const syntax::StmtId carrier_use = module.push_stmt(carrier_use_stmt);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "cleanup_escape";
    function.body = push_block(module, {carrier_decl, inner_block, carrier_use});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "cleanup_escape");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);
    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::cleanup_storage;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::reinit;
    }));

    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::shadow);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::cleanup;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyLoanEnforcedDeduplicatesDiagnosticSite)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dedup_sites");
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const base::SourceRange range{
        .source = base::SourceId{SEMA_TEST_ROOT_MODULE_INDEX},
        .begin = SEMA_TEST_BODY_LOAN_RANGE_BEGIN,
        .end = SEMA_TEST_BODY_LOAN_RANGE_END,
    };

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.points.push_back(sema::BodyFlowPoint{
        .range = range,
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = value_id,
        .projections = {},
        .range = range,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_mutable,
        .point = 0,
        .place = 0,
        .range = range,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 0,
        .place = 0,
        .range = range,
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 0,
        .place = 0,
        .range = range,
    });
    analyzer.state_.checked.body_flow_graphs[key] = std::move(graph);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dedup_sites";
    sema::SemanticAnalyzerCore::BodyLoanChecker(analyzer).check(function, key, sema::BodyLoanDiagnosticMode::enforced);

    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    EXPECT_EQ(result.diagnostic_mode, sema::BodyLoanDiagnosticMode::enforced);
    ASSERT_EQ(result.conflicts.size(), 2U);
    const base::usize emitted_count =
        static_cast<base::usize>(std::ranges::count_if(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
            return conflict.diagnostic_emitted;
        }));
    EXPECT_EQ(emitted_count, 1U);
    EXPECT_EQ(diagnostics.diagnostics().size(), 3U);
    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_ACTIVE_BORROW_CONFLICT), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_ACTIVE_BORROW_CREATED), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_ACTIVE_BORROW_INVALIDATING_ACTION), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxAnalyzeEnforcesBodyLoanConflictDiagnostics)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId void_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::void_));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId sink_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_SINK_NAME);
    const IdentId function_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_INTEGRATED_FUNCTION_NAME);

    syntax::ItemNode sink_function;
    sink_function.kind = syntax::ItemKind::fn_decl;
    sink_function.name = SEMA_TEST_BODY_FLOW_SINK_NAME;
    sink_function.name_id = sink_id;
    sink_function.params = {syntax::ParamDecl{SEMA_TEST_BODY_LOAN_REF_NAME, ref_i32_type, {}}};
    sink_function.return_type = void_type;
    sink_function.body = push_block(module, {});
    const syntax::ItemId sink_item = module.push_item(sink_function);
    module.item_modules[sink_item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const ExprId value_init = push_integer(module);
    const syntax::StmtId value_stmt =
        push_local_stmt(module, syntax::StmtKind::var, SEMA_TEST_BODY_LOAN_VALUE_NAME, i32_type, value_init);
    const ExprId borrowed_value = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, borrowed_value);
    const syntax::StmtId borrow_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, ref_i32_type, shared_borrow);
    const syntax::StmtId write_stmt =
        push_assign_stmt(module, push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME), push_integer(module));
    const ExprId call = push_call(
        module, push_name(module, SEMA_TEST_BODY_FLOW_SINK_NAME), {push_name(module, SEMA_TEST_BODY_LOAN_REF_NAME)});
    syntax::StmtNode call_stmt;
    call_stmt.kind = syntax::StmtKind::expr;
    call_stmt.init = call;
    const syntax::StmtId call_stmt_id = module.push_stmt(call_stmt);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = SEMA_TEST_BODY_FLOW_INTEGRATED_FUNCTION_NAME;
    function.name_id = function_id;
    function.return_type = void_type;
    function.body = push_block(module, {value_stmt, borrow_stmt, write_stmt, call_stmt_id});
    const syntax::ItemId item = module.push_item(function);
    module.item_modules[item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_ACTIVE_BORROW_CONFLICT), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_ACTIVE_BORROW_CREATED), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_ACTIVE_BORROW_INVALIDATING_ACTION), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_ACTIVE_BORROW_LATER_CARRIER_USE), std::string::npos);
    ASSERT_TRUE(analyzer.state_.checked.body_loan_checks.contains(key));
    const sema::BodyLoanCheckResult& result = analyzer.state_.checked.body_loan_checks.at(key);
    EXPECT_EQ(result.diagnostic_mode, sema::BodyLoanDiagnosticMode::enforced);
    ASSERT_FALSE(result.conflicts.empty());
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.diagnostic_emitted;
    }));
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.later_use_point != sema::SEMA_BODY_FLOW_INVALID_INDEX && conflict.later_use_range.well_formed();
    }));
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryRecordsParameterReturnDependency)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId function_id = module.intern_identifier("id_ref");
    const ExprId value = push_name(module, "value");
    const syntax::StmtId body = push_block(module, {push_return_stmt(module, value)});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "id_ref";
    function.name_id = function_id;
    function.params = {syntax::ParamDecl{"value", ref_i32_type, {}}};
    function.return_type = ref_i32_type;
    function.body = body;
    const syntax::ItemId item = module.push_item(function);
    module.item_modules[item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const sema::CheckedModule& checked = checked_result.value();
    ASSERT_TRUE(checked.borrow_summaries.contains(key));
    const sema::FunctionBorrowSummary& summary = checked.borrow_summaries.at(key);
    EXPECT_TRUE(summary.return_type_can_contain_borrow);
    EXPECT_FALSE(summary.has_unknown_return_origin);
    EXPECT_FALSE(summary.has_local_return_escape);
    ASSERT_EQ(summary.return_origins.size(), 1U);
    ASSERT_LT(summary.return_origins.front().origin_index, summary.origins.size());
    const sema::BorrowSummaryOrigin& origin = summary.origins[summary.return_origins.front().origin_index];
    EXPECT_EQ(origin.kind, sema::BorrowSummaryOriginKind::parameter);
    EXPECT_EQ(origin.param_index, 0U);
    EXPECT_NE(summary.fingerprint.byte_count, 0U);

    const std::string dump = sema::dump_function_borrow_summary(summary);
    EXPECT_NE(dump.find("parameter"), std::string::npos);
    EXPECT_NE(dump.find("unknown=false"), std::string::npos);
    EXPECT_EQ(sema::borrow_summary_origin_kind_name(sema::BorrowSummaryOriginKind::parameter), "parameter");
    EXPECT_EQ(sema::borrow_summary_origin_kind_name(sema::BorrowSummaryOriginKind::static_), "static");
    EXPECT_EQ(sema::borrow_summary_origin_kind_name(
                  static_cast<sema::BorrowSummaryOriginKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
}

TEST(CoreUnit, SemanticWhiteBoxBorrowContractHelpersCoverSelectorsAndDump)
{
    syntax::AstModule module;
    module.modules = {module_info({"borrow_contract_helpers"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);

    const IdentId function_id = intern_identifier(analyzer, "contract_fn");
    const IdentId value_id = intern_identifier(analyzer, "value");
    const IdentId other_id = intern_identifier(analyzer, "other");
    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const auto selector = [](const sema::BorrowContractSelectorKind kind, const base::u32 param_index,
                              const IdentId name_id) {
        return sema::BorrowContractSelector{
            .kind = kind,
            .param_index = param_index,
            .name_id = name_id,
            .range = body_loan_test_range(param_index),
        };
    };
    const auto contract = [&](std::vector<sema::BorrowContractSelector> selectors) {
        sema::FunctionBorrowContract result;
        result.function = key;
        result.return_type = i32;
        result.return_selectors = std::move(selectors);
        result.source = sema::FunctionBorrowContractSource::declared;
        result.return_type_can_contain_borrow = true;
        result.range = body_loan_test_range(0);
        result.fingerprint = sema::function_borrow_contract_fingerprint(result);
        return result;
    };

    sema::SemanticAnalyzerCore::BorrowContractAnalyzer contract_analyzer(analyzer);
    sema::FunctionBorrowContract wildcard_boundary =
        contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id)});
    wildcard_boundary.unknown_return_allowed = true;
    EXPECT_TRUE(contract_analyzer.contract_is_subset(
        contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id)}), wildcard_boundary));

    sema::FunctionBorrowContract escaped = contract({});
    escaped.has_local_return_escape = true;
    EXPECT_FALSE(contract_analyzer.contract_is_subset(escaped, wildcard_boundary));

    sema::FunctionBorrowContract mismatched = contract({});
    mismatched.has_contract_mismatch = true;
    EXPECT_FALSE(contract_analyzer.contract_is_subset(mismatched, wildcard_boundary));

    sema::FunctionBorrowContract narrowed_unknown = contract({selector(
        sema::BorrowContractSelectorKind::unknown, sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX, sema::INVALID_IDENT_ID)});
    narrowed_unknown.unknown_return_allowed = true;
    EXPECT_FALSE(contract_analyzer.contract_is_subset(
        narrowed_unknown, contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id)})));

    EXPECT_TRUE(
        contract_analyzer.contract_is_subset(contract({selector(sema::BorrowContractSelectorKind::unknown,
                                                 sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX, sema::INVALID_IDENT_ID)}),
            contract({selector(sema::BorrowContractSelectorKind::unknown, sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX,
                sema::INVALID_IDENT_ID)})));
    EXPECT_FALSE(
        contract_analyzer.contract_is_subset(contract({selector(sema::BorrowContractSelectorKind::static_,
                                                 sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX, sema::INVALID_IDENT_ID)}),
            contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id)})));
    EXPECT_TRUE(
        contract_analyzer.contract_is_subset(contract({selector(sema::BorrowContractSelectorKind::static_,
                                                 sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX, sema::INVALID_IDENT_ID)}),
            contract({selector(sema::BorrowContractSelectorKind::static_, sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX,
                sema::INVALID_IDENT_ID)})));
    EXPECT_TRUE(
        contract_analyzer.contract_is_subset(contract({selector(sema::BorrowContractSelectorKind::self, 0U, value_id)}),
            contract({selector(sema::BorrowContractSelectorKind::self, 0U, value_id)})));
    EXPECT_FALSE(
        contract_analyzer.contract_is_subset(contract({selector(sema::BorrowContractSelectorKind::self, 0U, value_id)}),
            contract({selector(sema::BorrowContractSelectorKind::self, 1U, value_id)})));
    EXPECT_TRUE(contract_analyzer.contract_is_subset(
        contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id)}),
        contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id)})));
    EXPECT_TRUE(contract_analyzer.contract_is_subset(
        contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id)}),
        contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, other_id)})));
    EXPECT_FALSE(contract_analyzer.contract_is_subset(
        contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id)}),
        contract({selector(sema::BorrowContractSelectorKind::parameter, 1U, value_id)})));

    sema::FunctionBorrowContract dump_contract =
        contract({selector(sema::BorrowContractSelectorKind::parameter, 0U, value_id),
            selector(sema::BorrowContractSelectorKind::self, 0U, value_id),
            selector(sema::BorrowContractSelectorKind::static_, sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX,
                sema::INVALID_IDENT_ID),
            selector(sema::BorrowContractSelectorKind::unknown, sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX,
                sema::INVALID_IDENT_ID)});
    dump_contract.unknown_return_allowed = true;
    dump_contract.has_local_return_escape = true;
    dump_contract.has_contract_mismatch = true;
    dump_contract.fingerprint = sema::function_borrow_contract_fingerprint(dump_contract);
    const std::string dump = sema::dump_function_borrow_contract(dump_contract);
    EXPECT_NE(dump.find("source=declared"), std::string::npos);
    EXPECT_NE(dump.find("can_borrow=true"), std::string::npos);
    EXPECT_NE(dump.find("unknown=true"), std::string::npos);
    EXPECT_NE(dump.find("local_escape=true"), std::string::npos);
    EXPECT_NE(dump.find("mismatch=true"), std::string::npos);
    EXPECT_NE(dump.find("parameter"), std::string::npos);
    EXPECT_NE(dump.find("self"), std::string::npos);
    EXPECT_NE(dump.find("static"), std::string::npos);
    EXPECT_NE(dump.find("unknown"), std::string::npos);
    EXPECT_NE(dump.find("name=#"), std::string::npos);
    EXPECT_NE(dump.find("fingerprint="), std::string::npos);

    sema::FunctionBorrowContract false_flag_dump_contract = contract({});
    false_flag_dump_contract.return_type_can_contain_borrow = false;
    false_flag_dump_contract.unknown_return_allowed = false;
    false_flag_dump_contract.has_local_return_escape = false;
    false_flag_dump_contract.has_contract_mismatch = false;
    false_flag_dump_contract.fingerprint = sema::function_borrow_contract_fingerprint(false_flag_dump_contract);
    const std::string false_flag_dump = sema::dump_function_borrow_contract(false_flag_dump_contract);
    EXPECT_NE(false_flag_dump.find("can_borrow=false"), std::string::npos);
    EXPECT_NE(false_flag_dump.find("unknown=false"), std::string::npos);
    EXPECT_NE(false_flag_dump.find("local_escape=false"), std::string::npos);
    EXPECT_NE(false_flag_dump.find("mismatch=false"), std::string::npos);

    EXPECT_NE(sema::function_borrow_contract_fingerprint(dump_contract).byte_count, 0U);
    EXPECT_EQ(sema::borrow_contract_selector_kind_name(sema::BorrowContractSelectorKind::parameter), "parameter");
    EXPECT_EQ(sema::borrow_contract_selector_kind_name(sema::BorrowContractSelectorKind::self), "self");
    EXPECT_EQ(sema::borrow_contract_selector_kind_name(sema::BorrowContractSelectorKind::static_), "static");
    EXPECT_EQ(sema::borrow_contract_selector_kind_name(sema::BorrowContractSelectorKind::unknown), "unknown");
    EXPECT_EQ(sema::borrow_contract_selector_kind_name(
                  static_cast<sema::BorrowContractSelectorKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::function_borrow_contract_source_name(sema::FunctionBorrowContractSource::inferred), "inferred");
    EXPECT_EQ(sema::function_borrow_contract_source_name(sema::FunctionBorrowContractSource::declared), "declared");
    EXPECT_EQ(sema::function_borrow_contract_source_name(sema::FunctionBorrowContractSource::conservative_unknown),
        "conservative_unknown");
    EXPECT_EQ(sema::function_borrow_contract_source_name(
                  static_cast<sema::FunctionBorrowContractSource>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
}

TEST(CoreUnit, SemanticWhiteBoxBorrowContractReportsDeclaredMismatch)
{
    syntax::AstModule module;
    module.modules = {module_info({"borrow_contract_declared_mismatch"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);

    const IdentId function_id = intern_identifier(analyzer, "declared_contract");
    const IdentId value_id = intern_identifier(analyzer, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId other_id = intern_identifier(analyzer, "other");
    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    const TypeHandle str = analyzer.state_.checked.types.builtin(BuiltinType::str);

    syntax::ParamDecl value_param;
    value_param.name = SEMA_TEST_BODY_LOAN_VALUE_NAME;
    value_param.name_id = value_id;
    syntax::BorrowContractSelectorDecl value_selector;
    value_selector.kind = syntax::BorrowContractSelectorKind::parameter;
    value_selector.name = SEMA_TEST_BODY_LOAN_VALUE_NAME;
    value_selector.name_id = value_id;
    value_selector.range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX);
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "declared_contract";
    function.name_id = function_id;
    function.params = {value_param};
    function.range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX);
    function.borrow_contract.present = true;
    function.borrow_contract.range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX);
    function.borrow_contract.return_selectors = {value_selector};

    FunctionSignature signature = function_signature(
        "declared_contract", module_id(SEMA_TEST_ROOT_MODULE_INDEX), str, function_id, analyzer.state_.checked);
    signature.semantic_key = key;
    signature.param_types = {str};

    sema::FunctionBorrowContract previous_contract;
    previous_contract.function = key;
    previous_contract.return_type = str;
    previous_contract.source = sema::FunctionBorrowContractSource::declared;
    previous_contract.return_type_can_contain_borrow = true;
    previous_contract.range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_OTHER_PARAM_INDEX);
    previous_contract.return_selectors.push_back(sema::BorrowContractSelector{
        .kind = sema::BorrowContractSelectorKind::parameter,
        .param_index = SEMA_TEST_BORROW_CONTRACT_OTHER_PARAM_INDEX,
        .name_id = other_id,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_OTHER_PARAM_INDEX),
    });
    previous_contract.fingerprint = sema::function_borrow_contract_fingerprint(previous_contract);
    analyzer.state_.checked.borrow_contracts[key] = previous_contract;

    analyzer.record_declared_borrow_contract(function, key, signature);

    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_BORROW_CONTRACT_MISMATCH), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_BORROW_CONTRACT_DECLARED_HERE), std::string::npos);
    ASSERT_TRUE(analyzer.state_.checked.borrow_contracts.contains(key));
    const sema::FunctionBorrowContract& recorded = analyzer.state_.checked.borrow_contracts.at(key);
    ASSERT_EQ(recorded.return_selectors.size(), 1U);
    EXPECT_EQ(recorded.return_selectors.front().param_index, SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX);
    EXPECT_EQ(recorded.return_selectors.front().name_id, value_id);
}

TEST(CoreUnit, SemanticWhiteBoxBorrowContractCheckSkipsMissingSummary)
{
    syntax::AstModule module;
    module.modules = {module_info({"borrow_contract_missing_summary"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);

    const IdentId function_id = intern_identifier(analyzer, "missing_summary_contract");
    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    const TypeHandle str = analyzer.state_.checked.types.builtin(BuiltinType::str);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "missing_summary_contract";
    function.name_id = function_id;
    function.range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX);

    FunctionSignature signature = function_signature(
        "missing_summary_contract", module_id(SEMA_TEST_ROOT_MODULE_INDEX), str, function_id, analyzer.state_.checked);
    signature.semantic_key = key;

    sema::FunctionBorrowContract previous_contract;
    previous_contract.function = key;
    previous_contract.return_type = str;
    previous_contract.source = sema::FunctionBorrowContractSource::declared;
    previous_contract.return_type_can_contain_borrow = true;
    previous_contract.range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX);
    previous_contract.fingerprint = sema::function_borrow_contract_fingerprint(previous_contract);
    const query::StableFingerprint128 previous_fingerprint = previous_contract.fingerprint;
    analyzer.state_.checked.borrow_contracts[key] = previous_contract;

    ASSERT_FALSE(analyzer.state_.checked.borrow_summaries.contains(key));

    analyzer.check_borrow_contract(function, key, signature);

    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
    ASSERT_TRUE(analyzer.state_.checked.borrow_contracts.contains(key));
    const sema::FunctionBorrowContract& checked = analyzer.state_.checked.borrow_contracts.at(key);
    EXPECT_EQ(checked.source, sema::FunctionBorrowContractSource::declared);
    EXPECT_EQ(checked.fingerprint, previous_fingerprint);
    EXPECT_FALSE(checked.has_contract_mismatch);
}

TEST(CoreUnit, SemanticWhiteBoxBorrowContractChecksMalformedInferredOrigins)
{
    syntax::AstModule module;
    module.modules = {module_info({"borrow_contract_malformed_inferred"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);

    const IdentId function_id = intern_identifier(analyzer, "checked_contract");
    const IdentId value_id = intern_identifier(analyzer, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    const TypeHandle str = analyzer.state_.checked.types.builtin(BuiltinType::str);

    syntax::ParamDecl value_param;
    value_param.name = SEMA_TEST_BODY_LOAN_VALUE_NAME;
    value_param.name_id = value_id;
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "checked_contract";
    function.name_id = function_id;
    function.params = {value_param};
    function.range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX);

    FunctionSignature signature = function_signature(
        "checked_contract", module_id(SEMA_TEST_ROOT_MODULE_INDEX), str, function_id, analyzer.state_.checked);
    signature.semantic_key = key;
    signature.param_types = {str};

    sema::FunctionBorrowContract declared_contract;
    declared_contract.function = key;
    declared_contract.return_type = str;
    declared_contract.source = sema::FunctionBorrowContractSource::declared;
    declared_contract.return_type_can_contain_borrow = true;
    declared_contract.range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX);
    declared_contract.fingerprint = sema::function_borrow_contract_fingerprint(declared_contract);
    analyzer.state_.checked.borrow_contracts[key] = declared_contract;

    sema::FunctionBorrowSummary summary;
    summary.function = key;
    summary.return_type = str;
    summary.return_type_can_contain_borrow = true;
    summary.origins.push_back(sema::BorrowSummaryOrigin{
        .kind = sema::BorrowSummaryOriginKind::unknown,
        .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
        .name_id = sema::INVALID_IDENT_ID,
        .expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX),
    });
    summary.origins.push_back(sema::BorrowSummaryOrigin{
        .kind = sema::BorrowSummaryOriginKind::local,
        .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
        .name_id = value_id,
        .expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_OTHER_PARAM_INDEX),
    });
    summary.origins.push_back(sema::BorrowSummaryOrigin{
        .kind = sema::BorrowSummaryOriginKind::parameter,
        .param_index = SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX,
        .name_id = value_id,
        .expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX),
    });
    summary.origins.push_back(sema::BorrowSummaryOrigin{
        .kind = sema::BorrowSummaryOriginKind::temporary,
        .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
        .name_id = sema::INVALID_IDENT_ID,
        .expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_TEMPORARY_ORIGIN_INDEX),
    });
    summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{
        .origin_index = SEMA_TEST_BORROW_CONTRACT_INVALID_ORIGIN_INDEX,
        .return_expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_INVALID_ORIGIN_INDEX),
    });
    summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{
        .origin_index = SEMA_TEST_BORROW_CONTRACT_UNKNOWN_ORIGIN_INDEX,
        .return_expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_VALUE_PARAM_INDEX),
    });
    summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{
        .origin_index = SEMA_TEST_BORROW_CONTRACT_LOCAL_ORIGIN_INDEX,
        .return_expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_OTHER_PARAM_INDEX),
    });
    summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{
        .origin_index = SEMA_TEST_BORROW_CONTRACT_PARAM_ORIGIN_INDEX,
        .return_expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_PARAM_ORIGIN_INDEX),
    });
    summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{
        .origin_index = SEMA_TEST_BORROW_CONTRACT_TEMPORARY_ORIGIN_INDEX,
        .return_expr = syntax::INVALID_EXPR_ID,
        .range = body_loan_test_range(SEMA_TEST_BORROW_CONTRACT_TEMPORARY_ORIGIN_INDEX),
    });
    analyzer.state_.checked.borrow_summaries[key] = summary;

    analyzer.check_borrow_contract(function, key, signature);

    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_BORROW_CONTRACT_MISMATCH), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_BORROW_CONTRACT_DECLARED_HERE), std::string::npos);
    ASSERT_TRUE(analyzer.state_.checked.borrow_contracts.contains(key));
    const sema::FunctionBorrowContract& checked = analyzer.state_.checked.borrow_contracts.at(key);
    EXPECT_TRUE(checked.has_local_return_escape);
    EXPECT_TRUE(checked.has_contract_mismatch);
    EXPECT_FALSE(checked.unknown_return_allowed);
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryKeepsNonBorrowReturnSummaryCompact)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId function_id = module.intern_identifier("non_borrow_summary");
    const ExprId one = push_integer(module);
    const syntax::StmtId value_stmt =
        push_local_stmt(module, syntax::StmtKind::var, SEMA_TEST_BODY_LOAN_VALUE_NAME, i32_type, one);
    const ExprId value_name = push_name(module, SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const ExprId shared_borrow = push_unary(module, syntax::UnaryOp::address_of, value_name);
    const syntax::StmtId ref_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, ref_i32_type, shared_borrow);
    const syntax::StmtId return_stmt = push_return_stmt(module, push_integer(module));

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "non_borrow_summary";
    function.name_id = function_id;
    function.return_type = i32_type;
    function.body = push_block(module, {value_stmt, ref_stmt, return_stmt});
    const syntax::ItemId item = module.push_item(function);
    module.item_modules[item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    sema::SemanticOptions options;
    options.retain_body_flow_graphs = false;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics, options);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const sema::CheckedModule& checked = checked_result.value();
    ASSERT_TRUE(checked.borrow_summaries.contains(key));
    const sema::FunctionBorrowSummary& summary = checked.borrow_summaries.at(key);
    EXPECT_FALSE(summary.return_type_can_contain_borrow);
    EXPECT_FALSE(summary.has_unknown_return_origin);
    EXPECT_FALSE(summary.has_local_return_escape);
    EXPECT_TRUE(summary.origins.empty());
    EXPECT_TRUE(summary.return_origins.empty());
    EXPECT_NE(summary.fingerprint.byte_count, 0U);

    ASSERT_TRUE(checked.body_loan_checks.contains(key));
    EXPECT_EQ(checked.body_loan_checks.at(key).loans.size(), 1U);
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryRecordsStorageEscapeForNonBorrowReturn)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const IdentId function_id = module.intern_identifier("non_borrow_storage_escape");
    const syntax::StmtId local_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "local", i32_type, push_integer(module));
    const ExprId slot_name = push_name(module, "slot");
    const ExprId slot_field = push_field(module, slot_name, SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const ExprId local_name = push_name(module, "local");
    const ExprId borrow_local = push_unary(module, syntax::UnaryOp::address_of, local_name);
    const syntax::StmtId assign_stmt = push_assign_stmt(module, slot_field, borrow_local);
    const syntax::StmtId return_stmt = push_return_stmt(module, push_integer(module));

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "non_borrow_storage_escape";
    function.name_id = function_id;
    function.params = {push_param_decl(module, "slot")};
    function.return_type = i32_type;
    function.body = push_block(module, {local_stmt, assign_stmt, return_stmt});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    prepare_expr_storage(analyzer, analyzer.ctx_.module);
    analyzer.state_.checked.stmt_local_types.assign(analyzer.ctx_.module.stmts.size(), INVALID_TYPE_HANDLE);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = types.reference(PointerMutability::const_, i32);
    static_cast<void>(analyzer.record_expr_type(slot_name, ref_i32));
    static_cast<void>(analyzer.record_expr_type(slot_field, ref_i32));
    static_cast<void>(analyzer.record_expr_type(local_name, i32));
    static_cast<void>(analyzer.record_expr_type(borrow_local, ref_i32));
    analyzer.record_stmt_local_type(local_stmt, i32);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    FunctionSignature signature;
    signature.name = analyzer.state_.checked.intern_text("non_borrow_storage_escape");
    signature.name_id = function_id;
    signature.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    signature.semantic_key = key;
    signature.param_types = {ref_i32};
    signature.return_type = i32;

    sema::SemanticAnalyzerCore::BorrowSummaryBuilder(analyzer).build(function, key, signature);

    ASSERT_TRUE(analyzer.state_.checked.borrow_summaries.contains(key));
    const sema::FunctionBorrowSummary& summary = analyzer.state_.checked.borrow_summaries.at(key);
    EXPECT_FALSE(summary.return_type_can_contain_borrow);
    EXPECT_TRUE(summary.return_origins.empty());
    ASSERT_EQ(summary.storage_escapes.size(), 1U);
    ASSERT_LT(summary.storage_escapes.front().origin_index, summary.origins.size());
    EXPECT_EQ(summary.origins[summary.storage_escapes.front().origin_index].kind,
        sema::BorrowSummaryOriginKind::local);
    EXPECT_TRUE(summary.origins[summary.storage_escapes.front().origin_index].storage_slot);
    EXPECT_NE(summary.fingerprint.byte_count, 0U);

    const std::string dump = sema::dump_function_borrow_summary(summary);
    EXPECT_NE(dump.find("can_borrow=false"), std::string::npos) << dump;
    EXPECT_NE(dump.find("storage_escapes:"), std::string::npos) << dump;
    EXPECT_NE(dump.find("s0 origin=o"), std::string::npos) << dump;
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryPropagatesDirectCallWrapper)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId id_ref_id = module.intern_identifier("id_ref");
    const IdentId wrap_id = module.intern_identifier("wrap_ref");

    syntax::ItemNode id_function;
    id_function.kind = syntax::ItemKind::fn_decl;
    id_function.name = "id_ref";
    id_function.name_id = id_ref_id;
    id_function.params = {syntax::ParamDecl{"value", ref_i32_type, {}}};
    id_function.return_type = ref_i32_type;
    id_function.body = push_block(module, {push_return_stmt(module, push_name(module, "value"))});
    const syntax::ItemId id_item = module.push_item(id_function);
    module.item_modules[id_item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const ExprId call = push_call(module, push_name(module, "id_ref"), {push_name(module, "input")});
    syntax::ItemNode wrapper;
    wrapper.kind = syntax::ItemKind::fn_decl;
    wrapper.name = "wrap_ref";
    wrapper.name_id = wrap_id;
    wrapper.params = {syntax::ParamDecl{"input", ref_i32_type, {}}};
    wrapper.return_type = ref_i32_type;
    wrapper.body = push_block(module, {push_return_stmt(module, call)});
    const syntax::ItemId wrapper_item = module.push_item(wrapper);
    module.item_modules[wrapper_item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey id_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        id_ref_id,
    };
    const sema::FunctionLookupKey wrapper_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        wrap_id,
    };
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const sema::CheckedModule& checked = checked_result.value();
    EXPECT_TRUE(checked.borrow_summaries.contains(id_key));
    ASSERT_TRUE(checked.borrow_summaries.contains(wrapper_key));
    EXPECT_TRUE(std::ranges::any_of(checked.function_calls, [id_key](const sema::FunctionCallBinding& binding) {
        return binding.function_key == id_key;
    }));
    const sema::FunctionBorrowSummary& summary = checked.borrow_summaries.at(wrapper_key);
    ASSERT_EQ(summary.return_origins.size(), 1U);
    ASSERT_LT(summary.return_origins.front().origin_index, summary.origins.size());
    const sema::BorrowSummaryOrigin& origin = summary.origins[summary.return_origins.front().origin_index];
    EXPECT_EQ(origin.kind, sema::BorrowSummaryOriginKind::parameter);
    EXPECT_EQ(origin.param_index, 0U);
    EXPECT_FALSE(summary.has_unknown_return_origin);
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryMapsDeclaredCallContracts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId wrapper_id = module.intern_identifier("wrap_declared_contract");
    const IdentId callee_id = module.intern_identifier("declared_contract");
    const ExprId value_arg = push_name(module, "value");
    const ExprId callee_expr = push_name(module, "declared_contract");
    const ExprId call = push_call(module, callee_expr, {value_arg});

    syntax::ItemNode wrapper;
    wrapper.kind = syntax::ItemKind::fn_decl;
    wrapper.name = "wrap_declared_contract";
    wrapper.name_id = wrapper_id;
    wrapper.params = {syntax::ParamDecl{"value", ref_i32_type, {}}};
    wrapper.return_type = ref_i32_type;
    wrapper.body = push_block(module, {push_return_stmt(module, call)});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = analyzer.state_.checked.types.reference(PointerMutability::const_, i32);
    analyzer.state_.checked.expr_types[value_arg.value] = ref_i32;
    analyzer.state_.checked.expr_types[call.value] = ref_i32;

    const sema::FunctionLookupKey wrapper_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        wrapper_id,
    };
    const sema::FunctionLookupKey callee_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        callee_id,
    };
    sema::FunctionCallBinding binding = analyzer.state_.checked.make_function_call_binding();
    binding.call_expr = call;
    binding.callee_expr = callee_expr;
    binding.function_key = callee_key;
    binding.return_type = ref_i32;
    analyzer.state_.checked.append_function_call_binding(binding);

    sema::FunctionBorrowContract contract;
    contract.function = callee_key;
    contract.source = sema::FunctionBorrowContractSource::declared;
    contract.return_type = ref_i32;
    contract.return_type_can_contain_borrow = true;
    contract.unknown_return_allowed = true;
    contract.return_selectors = {
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::parameter,
            .param_index = 0U,
            .name_id = module.intern_identifier("value"),
            .range = body_loan_test_range(0),
        },
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::parameter,
            .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
            .range = body_loan_test_range(1),
        },
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::static_,
            .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
            .range = body_loan_test_range(2),
        },
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::unknown,
            .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
            .range = body_loan_test_range(3),
        },
    };
    analyzer.state_.checked.borrow_contracts[callee_key] = contract;

    sema::FunctionSignature signature = analyzer.state_.checked.make_function_signature();
    signature.semantic_key = wrapper_key;
    signature.return_type = ref_i32;
    signature.param_types = {ref_i32};
    sema::SemanticAnalyzerCore::BorrowSummaryBuilder(analyzer).build(wrapper, wrapper_key, signature);

    ASSERT_TRUE(analyzer.state_.checked.borrow_summaries.contains(wrapper_key));
    const sema::FunctionBorrowSummary& summary = analyzer.state_.checked.borrow_summaries.at(wrapper_key);
    EXPECT_TRUE(summary.has_unknown_return_origin);
    EXPECT_TRUE(std::ranges::any_of(summary.origins, [](const sema::BorrowSummaryOrigin& origin) {
        return origin.kind == sema::BorrowSummaryOriginKind::parameter;
    }));
    EXPECT_TRUE(std::ranges::any_of(summary.origins, [](const sema::BorrowSummaryOrigin& origin) {
        return origin.kind == sema::BorrowSummaryOriginKind::static_;
    }));
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryPropagatesGenericParamWrapper)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId generic_type = module.push_type(named_node("T"));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId id_ref_id = module.intern_identifier("id_generic_ref");
    const IdentId wrap_id = module.intern_identifier("wrap_generic_ref");

    syntax::ItemNode id_function;
    id_function.kind = syntax::ItemKind::fn_decl;
    id_function.name = "id_generic_ref";
    id_function.name_id = id_ref_id;
    id_function.generic_params = {syntax::GenericParamDecl{"T", {}}};
    id_function.params = {syntax::ParamDecl{"value", generic_type, {}}};
    id_function.return_type = generic_type;
    id_function.body = push_block(module, {push_return_stmt(module, push_name(module, "value"))});
    const syntax::ItemId id_item = module.push_item(id_function);
    module.item_modules[id_item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const ExprId generic_callee = push_generic_apply(module, push_name(module, "id_generic_ref"), {ref_i32_type});
    const ExprId call = push_call(module, generic_callee, {push_name(module, "input")});
    syntax::ItemNode wrapper;
    wrapper.kind = syntax::ItemKind::fn_decl;
    wrapper.name = "wrap_generic_ref";
    wrapper.name_id = wrap_id;
    wrapper.params = {syntax::ParamDecl{"input", ref_i32_type, {}}};
    wrapper.return_type = ref_i32_type;
    wrapper.body = push_block(module, {push_return_stmt(module, call)});
    const syntax::ItemId wrapper_item = module.push_item(wrapper);
    module.item_modules[wrapper_item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey id_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        id_ref_id,
    };
    const sema::FunctionLookupKey wrapper_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        wrap_id,
    };
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const sema::CheckedModule& checked = checked_result.value();
    ASSERT_TRUE(checked.borrow_summaries.contains(id_key));
    const sema::FunctionBorrowSummary& generic_summary = checked.borrow_summaries.at(id_key);
    EXPECT_TRUE(generic_summary.return_type_can_contain_borrow);
    ASSERT_EQ(generic_summary.return_origins.size(), 1U);
    ASSERT_LT(generic_summary.return_origins.front().origin_index, generic_summary.origins.size());
    EXPECT_EQ(generic_summary.origins[generic_summary.return_origins.front().origin_index].kind,
        sema::BorrowSummaryOriginKind::parameter);

    ASSERT_TRUE(checked.borrow_summaries.contains(wrapper_key));
    const sema::FunctionBorrowSummary& wrapper_summary = checked.borrow_summaries.at(wrapper_key);
    ASSERT_EQ(wrapper_summary.return_origins.size(), 1U);
    ASSERT_LT(wrapper_summary.return_origins.front().origin_index, wrapper_summary.origins.size());
    const sema::BorrowSummaryOrigin& origin =
        wrapper_summary.origins[wrapper_summary.return_origins.front().origin_index];
    EXPECT_EQ(origin.kind, sema::BorrowSummaryOriginKind::parameter);
    EXPECT_EQ(origin.param_index, 0U);
    EXPECT_FALSE(wrapper_summary.has_unknown_return_origin);
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryTreatsAssociatedProjectionAsBorrowCarrier)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const IdentId function_id = module.intern_identifier("associated_projection_ref");
    const ExprId value = push_name(module, "value");
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "associated_projection_ref";
    function.name_id = function_id;
    function.params = {push_param_decl(module, "value")};
    function.body = push_block(module, {push_return_stmt(module, value)});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle generic =
        types.generic_param(sema::generic_param_identity_from_text("borrow_summary.Assoc.T"), "T");
    const TypeHandle associated_projection = types.associated_projection(generic, query::MemberKey{}, "Item");

    analyzer.state_.checked.expr_types.assign(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.prepare_analysis_only_storage(analyzer.ctx_.module.exprs.size());
    static_cast<void>(analyzer.record_expr_type(value, associated_projection));

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    FunctionSignature signature;
    signature.name = analyzer.state_.checked.intern_text("associated_projection_ref");
    signature.name_id = function_id;
    signature.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    signature.semantic_key = key;
    signature.param_types = {associated_projection};
    signature.return_type = associated_projection;

    sema::SemanticAnalyzerCore::BorrowSummaryBuilder(analyzer).build(function, key, signature);
    ASSERT_TRUE(analyzer.state_.checked.borrow_summaries.contains(key));
    const sema::FunctionBorrowSummary& summary = analyzer.state_.checked.borrow_summaries.at(key);
    EXPECT_TRUE(summary.return_type_can_contain_borrow);
    ASSERT_EQ(summary.return_origins.size(), 1U);
    ASSERT_LT(summary.return_origins.front().origin_index, summary.origins.size());
    const sema::BorrowSummaryOrigin& origin = summary.origins[summary.return_origins.front().origin_index];
    EXPECT_EQ(origin.kind, sema::BorrowSummaryOriginKind::parameter);
    EXPECT_EQ(origin.param_index, 0U);
    EXPECT_FALSE(summary.has_unknown_return_origin);
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryRecordsMultiBranchAndRawUnknown)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId bool_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::bool_));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId u8_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::u8));
    const TypeId usize_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::usize));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const TypeId const_u8_ptr_type = module.push_type(pointer_node(u8_type));
    const TypeId str_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::str));

    const IdentId choose_id = module.intern_identifier("choose_ref");
    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = push_name(module, "flag");
    if_stmt.then_block = push_block(module, {push_return_stmt(module, push_name(module, "a"))});
    if_stmt.else_block = push_block(module, {push_return_stmt(module, push_name(module, "b"))});
    const syntax::StmtId if_stmt_id = module.push_stmt(if_stmt);
    syntax::ItemNode choose;
    choose.kind = syntax::ItemKind::fn_decl;
    choose.name = "choose_ref";
    choose.name_id = choose_id;
    choose.params = {
        syntax::ParamDecl{"a", ref_i32_type, {}},
        syntax::ParamDecl{"b", ref_i32_type, {}},
        syntax::ParamDecl{"flag", bool_type, {}},
    };
    choose.return_type = ref_i32_type;
    choose.body = push_block(module, {if_stmt_id});
    const syntax::ItemId choose_item = module.push_item(choose);
    module.item_modules[choose_item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const IdentId raw_id = module.intern_identifier("raw_str");
    const ExprId raw_call = module.push_call_expr(syntax::ExprKind::str_from_bytes_unchecked, {},
        syntax::CallExprPayload{syntax::INVALID_EXPR_ID, {push_name(module, "data"), push_name(module, "len")}});
    syntax::ItemNode raw_function;
    raw_function.kind = syntax::ItemKind::fn_decl;
    raw_function.name = "raw_str";
    raw_function.name_id = raw_id;
    raw_function.params = {
        syntax::ParamDecl{"data", const_u8_ptr_type, {}},
        syntax::ParamDecl{"len", usize_type, {}},
    };
    raw_function.return_type = str_type;
    raw_function.body = push_block(module, {push_return_stmt(module, raw_call)});
    raw_function.is_unsafe = true;
    const syntax::ItemId raw_item = module.push_item(raw_function);
    module.item_modules[raw_item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey choose_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        choose_id,
    };
    const sema::FunctionLookupKey raw_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        raw_id,
    };
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);

    const sema::CheckedModule& checked = checked_result.value();
    ASSERT_TRUE(checked.borrow_summaries.contains(choose_key));
    const sema::FunctionBorrowSummary& choose_summary = checked.borrow_summaries.at(choose_key);
    ASSERT_EQ(choose_summary.return_origins.size(), 2U);
    std::vector<base::u32> params;
    for (const sema::FunctionBorrowReturnOrigin& dependency : choose_summary.return_origins) {
        ASSERT_LT(dependency.origin_index, choose_summary.origins.size());
        params.push_back(choose_summary.origins[dependency.origin_index].param_index);
    }
    std::ranges::sort(params);
    EXPECT_EQ(params, (std::vector<base::u32>{0U, 1U}));

    ASSERT_TRUE(checked.borrow_summaries.contains(raw_key));
    const sema::FunctionBorrowSummary& raw_summary = checked.borrow_summaries.at(raw_key);
    EXPECT_TRUE(raw_summary.has_unknown_return_origin);
    EXPECT_TRUE(raw_summary.return_origins.empty());
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryRecordsLocalEscapeFacts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId ref_i32_type = module.push_type(reference_node(i32_type));
    const IdentId function_id = module.intern_identifier("leak_ref");
    const syntax::StmtId local_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "local", i32_type, push_integer(module));
    const ExprId borrow_local = push_unary(module, syntax::UnaryOp::address_of, push_name(module, "local"));
    const syntax::StmtId body = push_block(module, {local_stmt, push_return_stmt(module, borrow_local)});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "leak_ref";
    function.name_id = function_id;
    function.return_type = ref_i32_type;
    function.body = body;
    const syntax::ItemId item = module.push_item(function);
    module.item_modules[item.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_BORROWED_LOCAL_ESCAPE), std::string::npos);

    ASSERT_TRUE(analyzer.state_.checked.borrow_summaries.contains(key));
    const sema::FunctionBorrowSummary& summary = analyzer.state_.checked.borrow_summaries.at(key);
    EXPECT_TRUE(summary.has_local_return_escape);
    ASSERT_EQ(summary.return_origins.size(), 1U);
    ASSERT_LT(summary.return_origins.front().origin_index, summary.origins.size());
    EXPECT_EQ(summary.origins[summary.return_origins.front().origin_index].kind, sema::BorrowSummaryOriginKind::local);
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryCoversPatternProjectionAndDumpEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    syntax::PatternNode left_pattern;
    left_pattern.kind = syntax::PatternKind::binding;
    left_pattern.binding_name = "left";
    const syntax::PatternId left_pattern_id = module.push_pattern(left_pattern);

    syntax::PatternNode right_pattern;
    right_pattern.kind = syntax::PatternKind::binding;
    right_pattern.binding_name = "right";
    const syntax::PatternId right_pattern_id = module.push_pattern(right_pattern);

    syntax::PatternNode tuple_pattern;
    tuple_pattern.kind = syntax::PatternKind::tuple;
    tuple_pattern.elements = {left_pattern_id, right_pattern_id};
    const syntax::PatternId tuple_pattern_id = module.push_pattern(tuple_pattern);

    const ExprId tuple_left = push_name(module, "a");
    const ExprId tuple_right = push_name(module, "b");
    const ExprId tuple_source = module.push_tuple_expr({}, std::vector<ExprId>{tuple_left, tuple_right});
    syntax::StmtNode tuple_decl;
    tuple_decl.kind = syntax::StmtKind::let;
    tuple_decl.pattern = tuple_pattern_id;
    tuple_decl.init = tuple_source;
    const syntax::StmtId tuple_decl_id = module.push_stmt(tuple_decl);

    const ExprId right_return = push_name(module, "right");
    const syntax::StmtId right_return_id = push_return_stmt(module, right_return);

    const syntax::StmtId holder_decl =
        push_local_stmt(module, syntax::StmtKind::let, "holder", syntax::INVALID_TYPE_ID);
    const ExprId holder_name = push_name(module, "holder");
    const ExprId holder_field = push_field(module, holder_name, SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const ExprId borrowed_holder_field = push_unary(module, syntax::UnaryOp::address_of, holder_field);
    const syntax::StmtId holder_return_id = push_return_stmt(module, borrowed_holder_field);

    const ExprId array_name = push_name(module, "arr");
    const ExprId array_slice = module.push_slice_expr(
        {}, syntax::SliceExprPayload{array_name, syntax::INVALID_EXPR_ID, syntax::INVALID_EXPR_ID});
    const ExprId borrowed_slice = push_unary(module, syntax::UnaryOp::address_of, array_slice);
    const syntax::StmtId slice_return_id = push_return_stmt(module, borrowed_slice);

    const ExprId raw_name = push_name(module, "raw");
    const ExprId dereferenced_raw = push_unary(module, syntax::UnaryOp::dereference, raw_name);
    const ExprId borrowed_raw = push_unary(module, syntax::UnaryOp::address_of, dereferenced_raw);
    const syntax::StmtId raw_return_id = push_return_stmt(module, borrowed_raw);

    const ExprId temporary_left = push_name(module, "a");
    const ExprId temporary_right = push_name(module, "b");
    const ExprId temporary_tuple = module.push_tuple_expr({}, std::vector<ExprId>{temporary_left, temporary_right});
    const ExprId borrowed_temporary = push_unary(module, syntax::UnaryOp::address_of, temporary_tuple);
    const syntax::StmtId temporary_return_id = push_return_stmt(module, borrowed_temporary);

    const syntax::StmtId body = push_block(module,
        {tuple_decl_id, right_return_id, holder_decl, holder_return_id, slice_return_id, raw_return_id,
            temporary_return_id});
    const IdentId function_id = module.intern_identifier("borrow_summary_edges");
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "borrow_summary_edges";
    function.name_id = function_id;
    function.params = {
        push_param_decl(module, "a"),
        push_param_decl(module, "b"),
        push_param_decl(module, "arr"),
        push_param_decl(module, "raw"),
    };
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = types.reference(PointerMutability::const_, i32);
    const TypeHandle pointer_i32 = types.pointer(PointerMutability::const_, i32);
    const TypeHandle array_i32 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i32);
    const TypeHandle slice_i32 = types.slice(PointerMutability::const_, i32);
    const TypeHandle tuple_refs = types.tuple({ref_i32, ref_i32});

    analyzer.state_.checked.expr_types.assign(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.prepare_analysis_only_storage(analyzer.ctx_.module.exprs.size());
    analyzer.state_.checked.stmt_local_types.assign(analyzer.ctx_.module.stmts.size(), INVALID_TYPE_HANDLE);
    static_cast<void>(analyzer.record_expr_type(tuple_left, ref_i32));
    static_cast<void>(analyzer.record_expr_type(tuple_right, ref_i32));
    static_cast<void>(analyzer.record_expr_type(tuple_source, tuple_refs));
    static_cast<void>(analyzer.record_expr_type(right_return, ref_i32));
    static_cast<void>(analyzer.record_expr_type(holder_name, ref_i32));
    static_cast<void>(analyzer.record_expr_type(holder_field, i32));
    static_cast<void>(analyzer.record_expr_type(borrowed_holder_field, ref_i32));
    static_cast<void>(analyzer.record_expr_type(array_name, array_i32));
    static_cast<void>(analyzer.record_expr_type(array_slice, slice_i32));
    static_cast<void>(analyzer.record_expr_type(borrowed_slice, ref_i32));
    static_cast<void>(analyzer.record_expr_type(raw_name, pointer_i32));
    static_cast<void>(analyzer.record_expr_type(dereferenced_raw, i32));
    static_cast<void>(analyzer.record_expr_type(borrowed_raw, ref_i32));
    static_cast<void>(analyzer.record_expr_type(temporary_left, ref_i32));
    static_cast<void>(analyzer.record_expr_type(temporary_right, ref_i32));
    static_cast<void>(analyzer.record_expr_type(temporary_tuple, tuple_refs));
    static_cast<void>(analyzer.record_expr_type(borrowed_temporary, ref_i32));
    analyzer.record_stmt_local_type(tuple_decl_id, tuple_refs);
    analyzer.record_stmt_local_type(holder_decl, ref_i32);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    FunctionSignature signature;
    signature.name = analyzer.state_.checked.intern_text("borrow_summary_edges");
    signature.name_id = function_id;
    signature.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    signature.semantic_key = key;
    signature.param_types = {ref_i32, ref_i32, array_i32, pointer_i32};
    signature.return_type = ref_i32;

    sema::SemanticAnalyzerCore::BorrowSummaryBuilder(analyzer).build(function, key, signature);
    ASSERT_TRUE(analyzer.state_.checked.borrow_summaries.contains(key));
    const sema::FunctionBorrowSummary& summary = analyzer.state_.checked.borrow_summaries.at(key);
    EXPECT_TRUE(summary.has_unknown_return_origin);
    EXPECT_TRUE(summary.has_local_return_escape);
    EXPECT_TRUE(std::ranges::any_of(summary.return_origins, [&summary](const sema::FunctionBorrowReturnOrigin& origin) {
        return origin.origin_index < summary.origins.size()
            && summary.origins[origin.origin_index].kind == sema::BorrowSummaryOriginKind::parameter
            && summary.origins[origin.origin_index].param_index == 1U;
    }));
    EXPECT_TRUE(std::ranges::any_of(summary.return_origins, [&summary](const sema::FunctionBorrowReturnOrigin& origin) {
        return origin.origin_index < summary.origins.size()
            && summary.origins[origin.origin_index].kind == sema::BorrowSummaryOriginKind::parameter
            && summary.origins[origin.origin_index].param_index == 2U;
    }));
    EXPECT_TRUE(std::ranges::any_of(summary.return_origins, [&summary](const sema::FunctionBorrowReturnOrigin& origin) {
        return origin.origin_index < summary.origins.size()
            && summary.origins[origin.origin_index].kind == sema::BorrowSummaryOriginKind::temporary;
    }));

    sema::FunctionBorrowSummary dump_summary;
    dump_summary.return_type = i32;
    dump_summary.has_unknown_return_origin = true;
    dump_summary.has_local_return_escape = true;
    dump_summary.origins = {
        sema::BorrowSummaryOrigin{.kind = sema::BorrowSummaryOriginKind::none},
        sema::BorrowSummaryOrigin{.kind = sema::BorrowSummaryOriginKind::static_},
        sema::BorrowSummaryOrigin{.kind = sema::BorrowSummaryOriginKind::local},
        sema::BorrowSummaryOrigin{.kind = sema::BorrowSummaryOriginKind::temporary},
        sema::BorrowSummaryOrigin{.kind = sema::BorrowSummaryOriginKind::unknown},
    };
    dump_summary.return_origins = {sema::FunctionBorrowReturnOrigin{.origin_index = 3U}};
    const std::string dump = sema::dump_function_borrow_summary(dump_summary);
    EXPECT_NE(dump.find("can_borrow=false"), std::string::npos);
    EXPECT_NE(dump.find("unknown=true"), std::string::npos);
    EXPECT_NE(dump.find("local_escape=true"), std::string::npos);
    EXPECT_NE(dump.find("none"), std::string::npos);
    EXPECT_NE(dump.find("static"), std::string::npos);
    EXPECT_NE(dump.find("local"), std::string::npos);
    EXPECT_NE(dump.find("temporary"), std::string::npos);
    EXPECT_NE(dump.find("unknown"), std::string::npos);
    EXPECT_EQ(sema::borrow_summary_origin_kind_name(sema::BorrowSummaryOriginKind::none), "none");
    EXPECT_EQ(sema::borrow_summary_origin_kind_name(sema::BorrowSummaryOriginKind::static_), "static");
    EXPECT_EQ(sema::borrow_summary_origin_kind_name(sema::BorrowSummaryOriginKind::local), "local");
    EXPECT_EQ(sema::borrow_summary_origin_kind_name(sema::BorrowSummaryOriginKind::temporary), "temporary");
    EXPECT_EQ(sema::borrow_summary_origin_kind_name(sema::BorrowSummaryOriginKind::unknown), "unknown");
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryCoversTraitAndConservativeCallEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    const ExprId receiver_for_trait = push_name(module, "receiver");
    const ExprId trait_callee = push_field(module, receiver_for_trait, "borrow");
    const ExprId trait_call = push_call(module, trait_callee);
    const syntax::StmtId trait_return = push_return_stmt(module, trait_call);

    const ExprId receiver_for_missing_trait = push_name(module, "receiver");
    const ExprId missing_trait_callee = push_field(module, receiver_for_missing_trait, "missing_borrow");
    const ExprId missing_trait_call = push_call(module, missing_trait_callee);
    const syntax::StmtId missing_trait_return = push_return_stmt(module, missing_trait_call);

    const ExprId missing_trait_value_arg = push_name(module, "receiver");
    const ExprId missing_trait_value_callee = push_name(module, "missing_trait_value");
    const ExprId missing_trait_value_call = push_call(module, missing_trait_value_callee, {missing_trait_value_arg});
    const syntax::StmtId missing_trait_value_return = push_return_stmt(module, missing_trait_value_call);

    const ExprId direct_missing_arg_callee = push_name(module, "takes_ref");
    const ExprId direct_missing_arg_call = push_call(module, direct_missing_arg_callee);
    const syntax::StmtId direct_missing_arg_return = push_return_stmt(module, direct_missing_arg_call);

    const ExprId direct_local_callee = push_name(module, "returns_local");
    const ExprId direct_local_call = push_call(module, direct_local_callee, {push_name(module, "receiver")});
    const syntax::StmtId direct_local_return = push_return_stmt(module, direct_local_call);

    const ExprId direct_bad_index_callee = push_name(module, "bad_index");
    const ExprId direct_bad_index_call = push_call(module, direct_bad_index_callee, {push_name(module, "receiver")});
    const syntax::StmtId direct_bad_index_return = push_return_stmt(module, direct_bad_index_call);

    const ExprId direct_stale_callee = push_name(module, "stale_nonborrow");
    const ExprId direct_stale_call = push_call(module, direct_stale_callee, {push_name(module, "receiver")});
    const syntax::StmtId direct_stale_return = push_return_stmt(module, direct_stale_call);

    const syntax::StmtId body = push_block(module,
        {trait_return, missing_trait_return, missing_trait_value_return, direct_missing_arg_return, direct_local_return,
            direct_bad_index_return, direct_stale_return});
    const IdentId function_id = module.intern_identifier("borrow_summary_calls");
    const IdentId trait_target_id = module.intern_identifier("trait_target");
    const IdentId missing_trait_target_id = module.intern_identifier("missing_trait_target");
    const IdentId missing_trait_value_target_id = module.intern_identifier("missing_trait_value_target");
    const IdentId direct_param_target_id = module.intern_identifier("direct_param_target");
    const IdentId direct_local_target_id = module.intern_identifier("direct_local_target");
    const IdentId direct_bad_index_target_id = module.intern_identifier("direct_bad_index_target");
    const IdentId direct_stale_target_id = module.intern_identifier("direct_stale_target");

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "borrow_summary_calls";
    function.name_id = function_id;
    function.params = {push_param_decl(module, "receiver")};
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = types.reference(PointerMutability::const_, i32);

    analyzer.state_.checked.expr_types.assign(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.prepare_analysis_only_storage(analyzer.ctx_.module.exprs.size());
    analyzer.state_.checked.stmt_local_types.assign(analyzer.ctx_.module.stmts.size(), INVALID_TYPE_HANDLE);
    static_cast<void>(analyzer.record_expr_type(receiver_for_trait, ref_i32));
    static_cast<void>(analyzer.record_expr_type(trait_callee, ref_i32));
    static_cast<void>(analyzer.record_expr_type(trait_call, ref_i32));
    static_cast<void>(analyzer.record_expr_type(receiver_for_missing_trait, ref_i32));
    static_cast<void>(analyzer.record_expr_type(missing_trait_callee, ref_i32));
    static_cast<void>(analyzer.record_expr_type(missing_trait_call, ref_i32));
    static_cast<void>(analyzer.record_expr_type(missing_trait_value_arg, ref_i32));
    static_cast<void>(analyzer.record_expr_type(missing_trait_value_callee, ref_i32));
    static_cast<void>(analyzer.record_expr_type(missing_trait_value_call, ref_i32));
    static_cast<void>(analyzer.record_expr_type(direct_missing_arg_callee, ref_i32));
    static_cast<void>(analyzer.record_expr_type(direct_missing_arg_call, ref_i32));
    static_cast<void>(analyzer.record_expr_type(direct_local_callee, ref_i32));
    static_cast<void>(analyzer.record_expr_type(direct_local_call, ref_i32));
    static_cast<void>(analyzer.record_expr_type(direct_bad_index_callee, ref_i32));
    static_cast<void>(analyzer.record_expr_type(direct_bad_index_call, ref_i32));
    static_cast<void>(analyzer.record_expr_type(direct_stale_callee, ref_i32));
    static_cast<void>(analyzer.record_expr_type(direct_stale_call, ref_i32));

    const sema::FunctionLookupKey trait_target{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        trait_target_id,
    };
    const sema::FunctionLookupKey missing_trait_target{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        missing_trait_target_id,
    };
    const sema::FunctionLookupKey missing_trait_value_target{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        missing_trait_value_target_id,
    };
    const sema::FunctionLookupKey direct_param_target{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        direct_param_target_id,
    };
    const sema::FunctionLookupKey direct_local_target{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        direct_local_target_id,
    };
    const sema::FunctionLookupKey direct_bad_index_target{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        direct_bad_index_target_id,
    };
    const sema::FunctionLookupKey direct_stale_target{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        direct_stale_target_id,
    };

    auto parameter_summary = [&](const sema::FunctionLookupKey key) {
        sema::FunctionBorrowSummary summary;
        summary.function = key;
        summary.return_type = ref_i32;
        summary.return_type_can_contain_borrow = true;
        summary.origins.push_back(sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::parameter,
            .param_index = 0U,
            .name_id = analyzer.ctx_.module.intern_identifier("callee_param"),
        });
        summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{.origin_index = 0U});
        return summary;
    };
    analyzer.state_.checked.borrow_summaries[trait_target] = parameter_summary(trait_target);
    analyzer.state_.checked.borrow_summaries[direct_param_target] = parameter_summary(direct_param_target);

    sema::FunctionBorrowSummary local_summary;
    local_summary.function = direct_local_target;
    local_summary.return_type = ref_i32;
    local_summary.return_type_can_contain_borrow = true;
    local_summary.origins.push_back(sema::BorrowSummaryOrigin{.kind = sema::BorrowSummaryOriginKind::local});
    local_summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{.origin_index = 0U});
    analyzer.state_.checked.borrow_summaries[direct_local_target] = std::move(local_summary);

    sema::FunctionBorrowSummary bad_index_summary;
    bad_index_summary.function = direct_bad_index_target;
    bad_index_summary.return_type = ref_i32;
    bad_index_summary.return_type_can_contain_borrow = true;
    bad_index_summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{.origin_index = 7U});
    analyzer.state_.checked.borrow_summaries[direct_bad_index_target] = std::move(bad_index_summary);

    sema::TraitMethodCallBinding trait_binding;
    trait_binding.call_expr = trait_call;
    trait_binding.callee_expr = trait_callee;
    trait_binding.function_key = trait_target;
    trait_binding.receiver_type = ref_i32;
    trait_binding.return_type = ref_i32;
    analyzer.state_.checked.trait_method_calls.push_back(trait_binding);

    sema::TraitMethodCallBinding missing_trait_binding;
    missing_trait_binding.call_expr = missing_trait_call;
    missing_trait_binding.callee_expr = missing_trait_callee;
    missing_trait_binding.function_key = missing_trait_target;
    missing_trait_binding.receiver_type = ref_i32;
    missing_trait_binding.return_type = ref_i32;
    analyzer.state_.checked.trait_method_calls.push_back(missing_trait_binding);

    sema::TraitMethodCallBinding missing_trait_value_binding;
    missing_trait_value_binding.call_expr = missing_trait_value_call;
    missing_trait_value_binding.callee_expr = missing_trait_value_callee;
    missing_trait_value_binding.function_key = missing_trait_value_target;
    missing_trait_value_binding.receiver_type = INVALID_TYPE_HANDLE;
    missing_trait_value_binding.return_type = i32;
    analyzer.state_.checked.trait_method_calls.push_back(missing_trait_value_binding);
    analyzer.state_.checked.function_calls.push_back(sema::FunctionCallBinding{
        .call_expr = direct_missing_arg_call,
        .callee_expr = direct_missing_arg_callee,
        .function_key = direct_param_target,
        .return_type = ref_i32,
    });
    analyzer.state_.checked.function_calls.push_back(sema::FunctionCallBinding{
        .call_expr = direct_local_call,
        .callee_expr = direct_local_callee,
        .function_key = direct_local_target,
        .return_type = ref_i32,
    });
    analyzer.state_.checked.function_calls.push_back(sema::FunctionCallBinding{
        .call_expr = direct_bad_index_call,
        .callee_expr = direct_bad_index_callee,
        .function_key = direct_bad_index_target,
        .return_type = ref_i32,
    });
    analyzer.state_.checked.function_calls.push_back(sema::FunctionCallBinding{
        .call_expr = direct_stale_call,
        .callee_expr = direct_stale_callee,
        .function_key = direct_stale_target,
        .return_type = i32,
    });

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    FunctionSignature signature;
    signature.name = analyzer.state_.checked.intern_text("borrow_summary_calls");
    signature.name_id = function_id;
    signature.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    signature.semantic_key = key;
    signature.param_types = {ref_i32};
    signature.return_type = ref_i32;

    sema::SemanticAnalyzerCore::BorrowSummaryBuilder(analyzer).build(function, key, signature);
    ASSERT_TRUE(analyzer.state_.checked.borrow_summaries.contains(key));
    const sema::FunctionBorrowSummary& summary = analyzer.state_.checked.borrow_summaries.at(key);
    EXPECT_TRUE(summary.has_unknown_return_origin);
    EXPECT_TRUE(std::ranges::any_of(summary.return_origins, [&summary](const sema::FunctionBorrowReturnOrigin& origin) {
        return origin.origin_index < summary.origins.size()
            && summary.origins[origin.origin_index].kind == sema::BorrowSummaryOriginKind::parameter
            && summary.origins[origin.origin_index].param_index == 0U;
    }));
}

TEST(CoreUnit, SemanticWhiteBoxBorrowSummaryCoversPatternFallbackEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    syntax::PatternNode struct_binding;
    struct_binding.kind = syntax::PatternKind::binding;
    struct_binding.binding_name = "field_value";
    const syntax::PatternId struct_binding_id = module.push_pattern(struct_binding);

    syntax::PatternNode struct_pattern;
    struct_pattern.kind = syntax::PatternKind::struct_;
    struct_pattern.struct_name = "MissingStruct";
    struct_pattern.field_patterns = {syntax::FieldPattern{"wanted", struct_binding_id, {}}};
    const syntax::PatternId struct_pattern_id = module.push_pattern(struct_pattern);

    syntax::StructLiteralExprPayload struct_payload;
    struct_payload.name = "MissingStruct";
    struct_payload.field_inits = {syntax::FieldInit{"other", push_name(module, "source"), {}}};
    const ExprId struct_source = module.push_struct_literal_expr({}, std::move(struct_payload));
    syntax::StmtNode struct_decl;
    struct_decl.kind = syntax::StmtKind::let;
    struct_decl.pattern = struct_pattern_id;
    struct_decl.init = struct_source;
    const syntax::StmtId struct_decl_id = module.push_stmt(struct_decl);

    syntax::PatternNode missing_enum_binding;
    missing_enum_binding.kind = syntax::PatternKind::binding;
    missing_enum_binding.binding_name = "missing_payload";
    const syntax::PatternId missing_enum_binding_id = module.push_pattern(missing_enum_binding);

    syntax::PatternNode missing_enum_pattern;
    missing_enum_pattern.kind = syntax::PatternKind::enum_case;
    missing_enum_pattern.case_name = "missing";
    missing_enum_pattern.payload_patterns = {missing_enum_binding_id};
    const syntax::PatternId missing_enum_pattern_id = module.push_pattern(missing_enum_pattern);
    syntax::StmtNode missing_enum_decl;
    missing_enum_decl.kind = syntax::StmtKind::let;
    missing_enum_decl.pattern = missing_enum_pattern_id;
    missing_enum_decl.init = push_name(module, "missing_enum_value");
    const syntax::StmtId missing_enum_decl_id = module.push_stmt(missing_enum_decl);

    syntax::PatternNode short_enum_binding;
    short_enum_binding.kind = syntax::PatternKind::binding;
    short_enum_binding.binding_name = "short_payload";
    const syntax::PatternId short_enum_binding_id = module.push_pattern(short_enum_binding);

    syntax::PatternNode short_enum_pattern;
    short_enum_pattern.kind = syntax::PatternKind::enum_case;
    short_enum_pattern.case_name = "some";
    short_enum_pattern.payload_patterns = {short_enum_binding_id};
    const syntax::PatternId short_enum_pattern_id = module.push_pattern(short_enum_pattern);
    syntax::StmtNode short_enum_decl;
    short_enum_decl.kind = syntax::StmtKind::let;
    short_enum_decl.pattern = short_enum_pattern_id;
    short_enum_decl.init = push_name(module, "short_enum_value");
    const syntax::StmtId short_enum_decl_id = module.push_stmt(short_enum_decl);

    const syntax::StmtId body = push_block(module, {struct_decl_id, missing_enum_decl_id, short_enum_decl_id});
    const IdentId function_id = module.intern_identifier("borrow_summary_pattern_fallbacks");
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "borrow_summary_pattern_fallbacks";
    function.name_id = function_id;
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = types.reference(PointerMutability::const_, i32);
    const TypeHandle struct_type = types.named_struct("body_loans.MissingStruct", "MissingStruct", false);
    const TypeHandle enum_type = types.named_enum("body_loans.ShortEnum", "ShortEnum");
    const StructInfo& struct_info =
        add_struct_info(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "MissingStruct", struct_type);
    const_cast<StructInfo&>(struct_info)
        .fields.push_back(struct_field_info(analyzer, "other", module_id(SEMA_TEST_ROOT_MODULE_INDEX), ref_i32));
    static_cast<void>(add_enum_case(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "ShortEnum_some", "some",
        enum_type, INVALID_TYPE_HANDLE, {}));

    analyzer.state_.checked.expr_types.assign(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.prepare_analysis_only_storage(analyzer.ctx_.module.exprs.size());
    analyzer.state_.checked.stmt_local_types.assign(analyzer.ctx_.module.stmts.size(), INVALID_TYPE_HANDLE);
    analyzer.record_stmt_local_type(struct_decl_id, struct_type);
    analyzer.record_stmt_local_type(missing_enum_decl_id, INVALID_TYPE_HANDLE);
    analyzer.record_stmt_local_type(short_enum_decl_id, enum_type);

    const sema::FunctionLookupKey key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    FunctionSignature signature;
    signature.name = analyzer.state_.checked.intern_text("borrow_summary_pattern_fallbacks");
    signature.name_id = function_id;
    signature.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    signature.semantic_key = key;
    signature.return_type = ref_i32;

    sema::SemanticAnalyzerCore::BorrowSummaryBuilder(analyzer).build(function, key, signature);
    ASSERT_TRUE(analyzer.state_.checked.borrow_summaries.contains(key));
    const sema::FunctionBorrowSummary& summary = analyzer.state_.checked.borrow_summaries.at(key);
    EXPECT_TRUE(summary.return_type_can_contain_borrow);
    EXPECT_TRUE(summary.return_origins.empty());
    EXPECT_FALSE(summary.has_unknown_return_origin);
}

TEST(CoreUnit, SemanticWhiteBoxGenericInstancesUseLocalDenseSideTables)
{
    syntax::AstModule module;
    module.modules = {module_info({"generic_sparse"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    syntax::TypeNode i32_type_node = primitive_node(syntax::PrimitiveTypeKind::i32);
    const TypeId i32_type = module.push_type(i32_type_node);

    const ExprId value = push_name(module, "value");

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = value;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode id_function;
    id_function.kind = syntax::ItemKind::fn_decl;
    id_function.name = "id";
    id_function.generic_params = {syntax::GenericParamDecl{"T", {}}};
    id_function.params = {syntax::ParamDecl{"value", generic_type, {}}};
    id_function.return_type = generic_type;
    id_function.body = body;
    const syntax::ItemId id_item = module.push_item(id_function);
    module.item_modules[id_item.value] = module_id(0);

    const ExprId call_callee = push_name(module, "id");
    const ExprId call_arg = push_integer(module);
    const ExprId call = push_call(module, call_callee, {call_arg});

    syntax::StmtNode main_return;
    main_return.kind = syntax::StmtKind::return_;
    main_return.return_value = call;
    const syntax::StmtId main_return_id = module.push_stmt(main_return);
    const syntax::StmtId main_body = push_block(module, {main_return_id});

    syntax::ItemNode main_function;
    main_function.kind = syntax::ItemKind::fn_decl;
    main_function.name = "main";
    main_function.return_type = i32_type;
    main_function.body = main_body;
    const syntax::ItemId main_item = module.push_item(main_function);
    module.item_modules[main_item.value] = module_id(0);

    syntax::AstModule discard_side_tables_module = module;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();
    ASSERT_EQ(checked.generic_function_instances.size(), 1U);
    const sema::GenericFunctionInstanceInfo& instance = checked.generic_function_instances.front();
    EXPECT_TRUE(query::is_valid(instance.generic_instance_key));
    EXPECT_EQ(instance.body.value, body.value);
    ASSERT_EQ(instance.generic_instance_key.type_args.size(), 1U);
    EXPECT_EQ(instance.generic_instance_key.type_args.front(), query::canonical_builtin(query::BuiltinTypeKey::i32));
    EXPECT_EQ(instance.generic_instance_key.param_env.predicate_count, 0U);

    const sema::FunctionSignature& signature = instance.signature;
    EXPECT_EQ(signature.generic_instance_key, instance.generic_instance_key);
    EXPECT_EQ(signature.name, "id");
    EXPECT_TRUE(sema::is_valid(signature.semantic_key));
    ASSERT_EQ(signature.generic_args.size(), 1U);
    EXPECT_TRUE(checked.types.same(signature.generic_args.front(), checked.types.builtin(sema::BuiltinType::i32)));
    EXPECT_EQ(sema::function_display_name(checked.types, signature), "id[i32]");
    EXPECT_TRUE(checked.functions.contains(signature.semantic_key));
    EXPECT_EQ(checked.functions.at(signature.semantic_key).generic_instance_key, instance.generic_instance_key);
    EXPECT_NE(sema::dump_checked_module(checked).find("id[i32]"), std::string::npos);
    const sema::GenericFunctionInstanceBodyView body_view =
        checked.generic_function_instance_body_view(analyzer.ctx_.module, 0);
    EXPECT_TRUE(sema::is_valid(body_view));
    EXPECT_EQ(body_view.instance, &instance);
    EXPECT_EQ(body_view.signature, &instance.signature);
    EXPECT_EQ(body_view.side_tables, &instance.side_tables);
    ASSERT_NE(body_view.item, nullptr);
    EXPECT_EQ(body_view.item->name, "id");
    EXPECT_EQ(body_view.body.value, body.value);
    EXPECT_FALSE(sema::is_valid(checked.generic_function_instance_body_view(analyzer.ctx_.module, 1)));

    const sema::GenericSideTables& side_tables = checked.generic_function_instances.front().side_tables;
    EXPECT_TRUE(side_tables.sparse);
    EXPECT_TRUE(side_tables.local_dense);
    EXPECT_TRUE(side_tables.expr_span.contains(value.value));
    EXPECT_TRUE(side_tables.stmt_span.contains(return_stmt_id.value));
    EXPECT_TRUE(checked.generic_side_table_layouts.empty());
    EXPECT_EQ(checked.generic_function_instances.front().side_table_layout_index,
        sema::SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX);
    EXPECT_EQ(side_tables.layout, nullptr);
    EXPECT_EQ(side_tables.expr_intrinsic_types.size(), side_tables.expr_span.count);
    EXPECT_EQ(side_tables.expr_types.size(), side_tables.expr_span.count);
    EXPECT_TRUE(side_tables.expr_expected_types.empty());
    EXPECT_EQ(side_tables.expr_c_name_ids.size(), side_tables.expr_span.count);
    EXPECT_EQ(side_tables.pattern_c_name_ids.size(), side_tables.pattern_span.count);
    EXPECT_TRUE(side_tables.pattern_case_name_ids.empty());
    EXPECT_EQ(side_tables.syntax_type_handles.size(), side_tables.type_span.count);
    EXPECT_EQ(side_tables.stmt_local_types.size(), side_tables.stmt_span.count);
    const base::usize value_local = side_tables.local_expr_index(value);
    ASSERT_NE(value_local, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    ASSERT_LT(value_local, side_tables.expr_types.size());
    EXPECT_TRUE(sema::is_valid(side_tables.expr_intrinsic_types[value_local]));
    EXPECT_TRUE(sema::is_valid(side_tables.expr_types[value_local]));
    EXPECT_TRUE(side_tables.sparse_expr_intrinsic_types.empty());
    EXPECT_TRUE(side_tables.sparse_expr_types.empty());
    EXPECT_TRUE(side_tables.sparse_expr_expected_types.empty());
    EXPECT_TRUE(side_tables.sparse_fallbacks.empty());
    EXPECT_TRUE(checked.expr_expected_types.empty());
    EXPECT_TRUE(checked.pattern_case_name_ids.empty());

    sema::SemanticOptions discard_options;
    discard_options.retain_generic_side_tables = false;
    base::DiagnosticSink discard_diagnostics;
    sema::SemanticAnalyzerCore discard_analyzer(
        std::move(discard_side_tables_module), discard_diagnostics, discard_options);
    auto discard_result = discard_analyzer.analyze();
    ASSERT_TRUE(discard_result) << discard_result.error().message;
    const sema::CheckedModule& discard_checked = discard_result.value();
    EXPECT_TRUE(discard_checked.generic_function_instances.empty());
    ASSERT_TRUE(discard_checked.functions.contains(signature.semantic_key));
    EXPECT_TRUE(query::is_valid(discard_checked.functions.at(signature.semantic_key).generic_instance_key));
    EXPECT_EQ(discard_checked.functions.at(signature.semantic_key).generic_instance_key, instance.generic_instance_key);
}

TEST(CoreUnit, SemanticWhiteBoxGenericInstanceQueryKeysIgnoreSessionTypeHandles)
{
    syntax::AstModule first_module;
    first_module.modules = {module_info({"stable_generic"})};
    base::DiagnosticSink first_diagnostics;
    sema::SemanticAnalyzerCore first_analyzer(first_module, first_diagnostics);
    const TypeHandle first_payload =
        first_analyzer.state_.checked.types.named_struct("stable_generic.Payload", "Payload", false);
    static_cast<void>(add_struct_info(first_analyzer, module_id(0), "Payload", first_payload));
    sema::SemanticAnalyzerCore::GenericTemplateInfo first_template =
        generic_template_info(first_analyzer, module_id(0), "Wrap");
    sema::SemanticAnalyzerCore::CapabilitySet first_constraints = first_analyzer.make_capability_set();
    first_constraints.insert(sema::CapabilityKind::eq);
    first_template.constraints.emplace(first_template.params.front(), std::move(first_constraints));
    const std::array<TypeHandle, 1> first_args{first_payload};

    syntax::AstModule second_module;
    second_module.modules = {module_info({"stable_generic"})};
    base::DiagnosticSink second_diagnostics;
    sema::SemanticAnalyzerCore second_analyzer(second_module, second_diagnostics);
    const TypeHandle extra_payload =
        second_analyzer.state_.checked.types.named_struct("stable_generic.Extra", "Extra", false);
    static_cast<void>(add_struct_info(second_analyzer, module_id(0), "Extra", extra_payload));
    const TypeHandle second_payload =
        second_analyzer.state_.checked.types.named_struct("stable_generic.Payload", "Payload", false);
    static_cast<void>(add_struct_info(second_analyzer, module_id(0), "Payload", second_payload));
    sema::SemanticAnalyzerCore::GenericTemplateInfo second_template =
        generic_template_info(second_analyzer, module_id(0), "Wrap");
    sema::SemanticAnalyzerCore::CapabilitySet second_constraints = second_analyzer.make_capability_set();
    second_constraints.insert(sema::CapabilityKind::eq);
    second_template.constraints.emplace(second_template.params.front(), std::move(second_constraints));
    const std::array<TypeHandle, 1> second_args{second_payload};

    ASSERT_NE(first_payload.value, second_payload.value);
    EXPECT_NE(first_analyzer.generic_instance_key_suffix({first_payload}),
        second_analyzer.generic_instance_key_suffix({second_payload}));

    base::Result<sema::SemanticAnalyzerCore::GenericInstanceIdentity> first_identity =
        first_analyzer.generic_instance_identity(first_template, first_args, query::DefNamespace::type);
    base::Result<sema::SemanticAnalyzerCore::GenericInstanceIdentity> second_identity =
        second_analyzer.generic_instance_identity(second_template, second_args, query::DefNamespace::type);
    ASSERT_TRUE(first_identity) << first_identity.error().message;
    ASSERT_TRUE(second_identity) << second_identity.error().message;

    EXPECT_EQ(first_identity.value().fingerprint_text, second_identity.value().fingerprint_text);
    EXPECT_EQ(first_identity.value().key, second_identity.value().key);
    EXPECT_EQ(query::stable_key_fingerprint(first_identity.value().key),
        query::stable_key_fingerprint(second_identity.value().key));
    EXPECT_EQ(first_analyzer.generic_instance_abi_suffix(first_identity.value().key),
        second_analyzer.generic_instance_abi_suffix(second_identity.value().key));
    EXPECT_NE(
        first_analyzer.generic_instance_abi_suffix(first_identity.value().key).find("__aurexg_k"), std::string::npos);
    EXPECT_EQ(first_identity.value().key.param_env.predicate_count, 1U);
    ASSERT_EQ(first_identity.value().key.type_args.size(), 1U);
    ASSERT_EQ(second_identity.value().key.type_args.size(), 1U);
    EXPECT_EQ(first_identity.value().key.type_args.front(), second_identity.value().key.type_args.front());
}

TEST(CoreUnit, SemanticWhiteBoxGenericInstanceResolverCoversIdentityEdges)
{
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_MODULE = "identity_edges";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_OWNER = "Owner";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_INVALID_INDEX = "InvalidIndex";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_OUTER = "Outer";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_INNER = "Inner";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_PAYLOAD = "Payload";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_GENERIC_PAYLOAD = "GenericPayload";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_GENERIC_PAYLOAD_ORIGIN = "0:GenericPayload";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_MODE = "Mode";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_CHOICE = "Choice";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_CHOICE_CASE = "Choice_some";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_CHOICE_CASE_NAME = "some";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_MALFORMED_ORIGIN = "BadOrigin";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_NON_NUMERIC_ORIGIN = "x:BadOrigin";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_OUT_OF_RANGE_ORIGIN = "99:BadOrigin";
    constexpr std::string_view SEMA_TEST_QUERY_EDGE_MISSING_GENERIC_IDENTITY = "identity_edges.Missing.T";

    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_QUERY_EDGE_MODULE})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);

    const query::PackageKey default_package = query::package_key(std::span<const std::string_view>{});
    const query::ModuleKey empty_module =
        query::module_key_from_stable_id(default_package, sema::stable_module_id(std::span<const std::string_view>{}));
    EXPECT_EQ(analyzer.query_module_key(syntax::INVALID_MODULE_ID), empty_module);
    EXPECT_EQ(analyzer.query_module_key(module_id(SEMA_TEST_MISSING_MODULE_INDEX)), empty_module);

    sema::SemanticAnalyzerCore::GenericTemplateInfo owner =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_QUERY_EDGE_OWNER);
    analyzer.populate_generic_param_identities(owner);
    const query::DefKey owner_key = analyzer.generic_template_query_key(owner, query::DefNamespace::type);

    sema::SemanticAnalyzerCore::GenericTemplateInfo invalid_index_owner =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_QUERY_EDGE_INVALID_INDEX);
    invalid_index_owner.param_identities.push_back(sema::INVALID_GENERIC_PARAM_IDENTITY);
    analyzer.index_generic_param_query_keys(invalid_index_owner, query::DefNamespace::type);

    sema::TypeInfo invalid_generic_info;
    EXPECT_FALSE(analyzer.canonical_generic_param_query_key(owner, owner_key, invalid_generic_info).has_value());

    const TypeHandle owner_param =
        analyzer.state_.checked.types.generic_param(owner.param_identities.front(), SEMA_TEST_GENERIC_PARAM_NAME);
    const std::optional<query::GenericParamKey> owner_param_key =
        analyzer.canonical_generic_param_query_key(owner, owner_key, analyzer.state_.checked.types.get(owner_param));
    ASSERT_TRUE(owner_param_key.has_value());
    EXPECT_EQ(owner_param_key.value(), query::generic_param_key(owner_key, 0));

    sema::SemanticAnalyzerCore::GenericTemplateInfo outer =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_QUERY_EDGE_OUTER);
    analyzer.populate_generic_param_identities(outer);
    analyzer.index_generic_param_query_keys(outer, query::DefNamespace::value);
    sema::SemanticAnalyzerCore::GenericTemplateInfo inner =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_QUERY_EDGE_INNER);
    analyzer.populate_generic_param_identities(inner);
    const query::DefKey inner_key = analyzer.generic_template_query_key(inner, query::DefNamespace::value);
    const TypeHandle outer_param =
        analyzer.state_.checked.types.generic_param(outer.param_identities.front(), SEMA_TEST_GENERIC_PARAM_NAME);
    const std::optional<query::GenericParamKey> fallback_param_key =
        analyzer.canonical_generic_param_query_key(inner, inner_key, analyzer.state_.checked.types.get(outer_param));
    ASSERT_TRUE(fallback_param_key.has_value());
    EXPECT_EQ(fallback_param_key.value(),
        query::generic_param_key(analyzer.generic_template_query_key(outer, query::DefNamespace::value), 0));

    sema::TypeInfo missing_generic_info;
    missing_generic_info.generic_identity =
        sema::generic_param_identity_from_text(SEMA_TEST_QUERY_EDGE_MISSING_GENERIC_IDENTITY);
    EXPECT_FALSE(analyzer.canonical_generic_param_query_key(owner, owner_key, missing_generic_info).has_value());

    const TypeHandle payload =
        analyzer.state_.checked.types.named_struct("identity_edges.Payload", SEMA_TEST_QUERY_EDGE_PAYLOAD, false);
    static_cast<void>(add_struct_info(analyzer, module_id(0), SEMA_TEST_QUERY_EDGE_PAYLOAD, payload));
    EXPECT_TRUE(
        analyzer.canonical_nominal_type_query_key(payload, analyzer.state_.checked.types.get(payload)).has_value());

    const TypeHandle generic_payload = analyzer.state_.checked.types.named_struct(
        "identity_edges.GenericPayload", SEMA_TEST_QUERY_EDGE_GENERIC_PAYLOAD, false);
    analyzer.state_.checked.types.set_generic_instance(
        generic_payload, SEMA_TEST_QUERY_EDGE_GENERIC_PAYLOAD_ORIGIN, {});
    const std::optional<query::DefKey> generic_payload_key =
        analyzer.canonical_nominal_type_query_key(generic_payload, analyzer.state_.checked.types.get(generic_payload));
    ASSERT_TRUE(generic_payload_key.has_value());
    EXPECT_EQ(generic_payload_key->kind, query::DefKind::generic_template);

    analyzer.state_.types.struct_infos_by_type[payload.value] = nullptr;
    EXPECT_FALSE(
        analyzer.canonical_nominal_type_query_key(payload, analyzer.state_.checked.types.get(payload)).has_value());

    const TypeHandle malformed_origin =
        analyzer.state_.checked.types.named_struct("identity_edges.Malformed", "Malformed", false);
    analyzer.state_.checked.types.set_generic_instance(malformed_origin, SEMA_TEST_QUERY_EDGE_MALFORMED_ORIGIN, {});
    EXPECT_FALSE(
        analyzer.canonical_nominal_type_query_key(malformed_origin, analyzer.state_.checked.types.get(malformed_origin))
            .has_value());

    const TypeHandle non_numeric_origin =
        analyzer.state_.checked.types.named_struct("identity_edges.NonNumeric", "NonNumeric", false);
    analyzer.state_.checked.types.set_generic_instance(non_numeric_origin, SEMA_TEST_QUERY_EDGE_NON_NUMERIC_ORIGIN, {});
    EXPECT_FALSE(analyzer
            .canonical_nominal_type_query_key(non_numeric_origin, analyzer.state_.checked.types.get(non_numeric_origin))
            .has_value());

    const TypeHandle out_of_range_origin =
        analyzer.state_.checked.types.named_struct("identity_edges.OutOfRange", "OutOfRange", false);
    analyzer.state_.checked.types.set_generic_instance(
        out_of_range_origin, SEMA_TEST_QUERY_EDGE_OUT_OF_RANGE_ORIGIN, {});
    EXPECT_FALSE(analyzer
            .canonical_nominal_type_query_key(
                out_of_range_origin, analyzer.state_.checked.types.get(out_of_range_origin))
            .has_value());

    const TypeHandle named_enum =
        analyzer.state_.checked.types.named_enum("identity_edges.Mode", SEMA_TEST_QUERY_EDGE_MODE);
    static_cast<void>(add_named_type(analyzer, module_id(0), SEMA_TEST_QUERY_EDGE_MODE, named_enum));
    const std::optional<query::DefKey> named_enum_key =
        analyzer.canonical_nominal_type_query_key(named_enum, analyzer.state_.checked.types.get(named_enum));
    ASSERT_TRUE(named_enum_key.has_value());
    EXPECT_EQ(named_enum_key->kind, query::DefKind::enum_);

    const TypeHandle case_only_enum =
        analyzer.state_.checked.types.named_enum("identity_edges.Choice", SEMA_TEST_QUERY_EDGE_CHOICE);
    EnumCaseInfo case_info = analyzer.state_.checked.make_enum_case_info();
    case_info.name = checked_text(analyzer.state_.checked, SEMA_TEST_QUERY_EDGE_CHOICE_CASE);
    case_info.name_id = intern_identifier(analyzer, SEMA_TEST_QUERY_EDGE_CHOICE_CASE);
    case_info.module = module_id(0);
    case_info.type = case_only_enum;
    case_info.enum_name = checked_text(analyzer.state_.checked, SEMA_TEST_QUERY_EDGE_CHOICE);
    case_info.case_name = checked_text(analyzer.state_.checked, SEMA_TEST_QUERY_EDGE_CHOICE_CASE_NAME);
    analyzer.state_.checked.enum_cases.emplace(
        semantic_module_key(analyzer, module_id(0), SEMA_TEST_QUERY_EDGE_CHOICE_CASE), std::move(case_info));
    const std::optional<query::DefKey> case_only_enum_key =
        analyzer.canonical_nominal_type_query_key(case_only_enum, analyzer.state_.checked.types.get(case_only_enum));
    ASSERT_TRUE(case_only_enum_key.has_value());
    EXPECT_EQ(case_only_enum_key->kind, query::DefKind::enum_);
}

TEST(CoreUnit, SemanticWhiteBoxGenericParamEnvKeySortsPredicates)
{
    constexpr std::string_view SEMA_TEST_QUERY_ENV_MODULE = "identity_env";
    constexpr std::string_view SEMA_TEST_QUERY_ENV_TEMPLATE = "Constrained";
    constexpr std::string_view SEMA_TEST_QUERY_ENV_SECOND_PARAM = "U";

    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_QUERY_ENV_MODULE})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);

    sema::SemanticAnalyzerCore::GenericTemplateInfo first =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_QUERY_ENV_TEMPLATE);
    first.params.push_back(intern_identifier(analyzer, SEMA_TEST_QUERY_ENV_SECOND_PARAM));
    sema::SemanticAnalyzerCore::CapabilitySet first_constraints = analyzer.make_capability_set();
    first_constraints.insert(sema::CapabilityKind::hash);
    first_constraints.insert(sema::CapabilityKind::sized);
    first_constraints.insert(sema::CapabilityKind::eq);
    first.constraints.emplace(first.params.front(), std::move(first_constraints));

    sema::SemanticAnalyzerCore::GenericTemplateInfo second =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_QUERY_ENV_TEMPLATE);
    second.params.push_back(intern_identifier(analyzer, SEMA_TEST_QUERY_ENV_SECOND_PARAM));
    sema::SemanticAnalyzerCore::CapabilitySet second_constraints = analyzer.make_capability_set();
    second_constraints.insert(sema::CapabilityKind::eq);
    second_constraints.insert(sema::CapabilityKind::hash);
    second_constraints.insert(sema::CapabilityKind::sized);
    second.constraints.emplace(second.params.front(), std::move(second_constraints));

    const query::ParamEnvKey first_key = analyzer.generic_param_env_key(first);
    const query::ParamEnvKey second_key = analyzer.generic_param_env_key(second);
    EXPECT_EQ(first_key, second_key);
    EXPECT_EQ(first_key.predicate_count, 3U);
    EXPECT_TRUE(query::is_valid(first_key));

    sema::SemanticAnalyzerCore::GenericTemplateInfo unconstrained =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_QUERY_ENV_TEMPLATE);
    const query::ParamEnvKey unconstrained_key = analyzer.generic_param_env_key(unconstrained);
    EXPECT_EQ(unconstrained_key.predicate_count, 0U);
    EXPECT_TRUE(query::is_valid(unconstrained_key));
}

TEST(CoreUnit, SemanticWhiteBoxGenericCapabilityAndParameterFallbackEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"generic_fallback"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle enum_type = types.named_enum("generic_fallback.Flag", "Flag");
    const IdentId param_id = intern_identifier(analyzer, SEMA_TEST_GENERIC_PARAM_NAME);
    const sema::GenericParamIdentity param_identity = sema::generic_param_identity_from_text("generic_fallback.T");
    const TypeHandle param_type = types.generic_param(param_identity, SEMA_TEST_GENERIC_PARAM_NAME);

    EXPECT_EQ(
        sema::capability_name(static_cast<sema::CapabilityKind>(SEMA_TEST_INVALID_CAPABILITY_KIND_VALUE)), "<invalid>");
    EXPECT_FALSE(analyzer.generic_param_has_capability(SEMA_TEST_GENERIC_PARAM_NAME, sema::CapabilityKind::eq));
    EXPECT_FALSE(analyzer.generic_param_has_capability(INVALID_TYPE_HANDLE, sema::CapabilityKind::eq));
    EXPECT_FALSE(analyzer.type_satisfies_capability(INVALID_TYPE_HANDLE, sema::CapabilityKind::sized));
    EXPECT_FALSE(analyzer.type_satisfies_equality_capability(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(analyzer.type_supports_equality_operator(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(analyzer.type_supports_hash_capability(INVALID_TYPE_HANDLE));

    sema::SemanticAnalyzerCore::GenericContext context = analyzer.make_generic_context();
    context.param_identities.emplace(param_id, param_identity);
    sema::SemanticAnalyzerCore::CapabilitySet identity_capabilities = analyzer.make_capability_set();
    identity_capabilities.insert(sema::CapabilityKind::eq);
    context.constraints_by_identity.emplace(param_identity, std::move(identity_capabilities));
    analyzer.state_.flow.current_generic_context = &context;
    EXPECT_TRUE(analyzer.generic_param_has_capability(SEMA_TEST_GENERIC_PARAM_NAME, sema::CapabilityKind::eq));
    EXPECT_TRUE(analyzer.generic_param_has_capability(param_type, sema::CapabilityKind::eq));
    EXPECT_FALSE(analyzer.generic_param_has_capability(i32, sema::CapabilityKind::eq));
    EXPECT_TRUE(analyzer.type_satisfies_capability(param_type, sema::CapabilityKind::eq));

    context.constraints_by_identity.clear();
    sema::SemanticAnalyzerCore::CapabilitySet named_capabilities = analyzer.make_capability_set();
    named_capabilities.insert(sema::CapabilityKind::hash);
    context.constraints.emplace(param_id, std::move(named_capabilities));
    EXPECT_TRUE(analyzer.generic_param_has_capability(SEMA_TEST_GENERIC_PARAM_NAME, sema::CapabilityKind::hash));
    EXPECT_TRUE(analyzer.type_satisfies_capability(i32, sema::CapabilityKind::sized));
    EXPECT_TRUE(analyzer.type_satisfies_capability(i32, sema::CapabilityKind::eq));
    EXPECT_TRUE(analyzer.type_satisfies_capability(i32, sema::CapabilityKind::ord));
    EXPECT_TRUE(analyzer.type_satisfies_capability(i32, sema::CapabilityKind::hash));
    EXPECT_TRUE(analyzer.type_satisfies_capability(i32, sema::CapabilityKind::copy));
    EXPECT_FALSE(analyzer.type_satisfies_capability(
        i32, static_cast<sema::CapabilityKind>(SEMA_TEST_INVALID_CAPABILITY_KIND_VALUE)));
    EXPECT_TRUE(analyzer.type_satisfies_equality_capability(enum_type));
    EXPECT_TRUE(analyzer.type_supports_equality_operator(enum_type));

    sema::TraitPredicate reader_predicate = analyzer.state_.checked.make_trait_predicate();
    reader_predicate.kind = sema::TraitPredicateKind::declared_trait;
    reader_predicate.trait_name = checked_text(analyzer.state_.checked, "Reader");
    reader_predicate.trait_name_id = intern_identifier(analyzer, "Reader");
    const std::array<std::string_view, 1> reader_path{"Reader"};
    const query::DefKey reader_key =
        query::def_key(query_test_module_key(), query::DefNamespace::trait_, query::DefKind::trait_, reader_path);
    sema::TraitImplAssociatedTypeInfo item_equality = analyzer.state_.checked.make_trait_impl_associated_type_info();
    item_equality.name = checked_text(analyzer.state_.checked, "Item");
    item_equality.name_id = intern_identifier(analyzer, "Item");
    item_equality.member_key =
        query::member_key(reader_key, query::MemberKind::associated_type, "Item", SEMA_TEST_GENERIC_FIRST_PARAM_INDEX);
    item_equality.value_type = i32;
    reader_predicate.associated_type_equalities.push_back(item_equality);

    sema::SemanticAnalyzerCore::GenericAnalyzer generic(analyzer);
    EXPECT_FALSE(generic.generic_param_has_trait_predicate(INVALID_TYPE_HANDLE, reader_predicate));
    EXPECT_FALSE(generic.generic_param_has_trait_predicate(i32, reader_predicate));
    EXPECT_FALSE(generic.type_satisfies_trait_predicate(INVALID_TYPE_HANDLE, reader_predicate, {}));

    context.predicate_indices.push_back(sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX);
    EXPECT_FALSE(generic.generic_param_has_trait_predicate(param_type, reader_predicate));
    EXPECT_FALSE(generic.type_satisfies_trait_predicate(param_type, reader_predicate, {}));
    context.predicate_indices.clear();

    sema::TraitPredicate builtin_predicate = analyzer.state_.checked.make_trait_predicate();
    builtin_predicate.kind = sema::TraitPredicateKind::builtin;
    builtin_predicate.subject_param_index = 0;
    const base::u32 builtin_predicate_index = static_cast<base::u32>(analyzer.state_.checked.trait_predicates.size());
    analyzer.state_.checked.trait_predicates.push_back(std::move(builtin_predicate));

    sema::SemanticAnalyzerCore::GenericTemplateInfo predicate_info =
        generic_template_info(analyzer, module_id(0), "Predicated");
    predicate_info.predicate_indices.push_back(sema::SEMA_TRAIT_PREDICATE_INVALID_INDEX);
    predicate_info.predicate_indices.push_back(builtin_predicate_index);
    const std::vector<TypeHandle> predicate_args{i32};
    EXPECT_TRUE(analyzer.validate_generic_arguments(predicate_info, predicate_args, {}));
    syntax::ItemNode predicate_item;
    predicate_item.kind = syntax::ItemKind::fn_decl;
    predicate_item.name = "Predicated";
    EXPECT_NE(analyzer.generic_template_incremental_fingerprint(predicate_item, predicate_info).find("Predicated"),
        std::string::npos);

    sema::TraitPredicate declared_predicate = analyzer.state_.checked.make_trait_predicate();
    declared_predicate.kind = sema::TraitPredicateKind::declared_trait;
    declared_predicate.subject_param_identity = param_identity;
    declared_predicate.trait_stable_id = reader_predicate.trait_stable_id;
    declared_predicate.associated_type_equalities.push_back(item_equality);
    const base::u32 declared_predicate_index = static_cast<base::u32>(analyzer.state_.checked.trait_predicates.size());
    analyzer.state_.checked.trait_predicates.push_back(std::move(declared_predicate));
    context.predicate_indices.push_back(declared_predicate_index);
    EXPECT_TRUE(generic.generic_param_has_trait_predicate(param_type, reader_predicate));
    EXPECT_TRUE(generic.type_satisfies_trait_predicate(param_type, reader_predicate, {}));

    sema::TraitPredicate mismatched_predicate = analyzer.state_.checked.make_trait_predicate();
    mismatched_predicate.kind = sema::TraitPredicateKind::declared_trait;
    mismatched_predicate.trait_name = reader_predicate.trait_name;
    mismatched_predicate.trait_name_id = reader_predicate.trait_name_id;
    mismatched_predicate.trait_stable_id = reader_predicate.trait_stable_id;
    sema::TraitImplAssociatedTypeInfo mismatched_equality = item_equality;
    mismatched_equality.value_type = types.builtin(BuiltinType::u8);
    mismatched_predicate.associated_type_equalities.push_back(mismatched_equality);
    EXPECT_FALSE(generic.generic_param_has_trait_predicate(param_type, mismatched_predicate));

    analyzer.state_.flow.current_generic_context = nullptr;

    sema::SemanticAnalyzerCore::GenericTemplateInfo fingerprint_info =
        generic_template_info(analyzer, module_id(0), "FingerprintFallback");
    fingerprint_info.param_identities.clear();
    const query::StableFingerprint128 missing_identity_fingerprint =
        analyzer.generic_trait_predicate_fingerprint(fingerprint_info, SEMA_TEST_GENERIC_SECOND_PARAM_INDEX,
            sema::TraitPredicateKind::builtin, sema::CapabilityKind::eq, nullptr);
    EXPECT_NE(query::debug_string(missing_identity_fingerprint), query::debug_string(query::StableFingerprint128{}));

    syntax::GenericConstraintDecl record_constraint;
    record_constraint.param_name = SEMA_TEST_GENERIC_PARAM_NAME;
    record_constraint.param_name_id = param_id;
    record_constraint.capability_names = {"Eq"};
    sema::SemanticAnalyzerCore::GenericTemplateInfo recorded_predicate_info =
        generic_template_info(analyzer, module_id(0), "RecordedPredicateFallback");
    recorded_predicate_info.param_identities.clear();
    const base::u32 recorded_predicate_index = generic.record_generic_trait_predicate(recorded_predicate_info,
        record_constraint, SEMA_TEST_GENERIC_FIRST_PARAM_INDEX, SEMA_TEST_GENERIC_FIRST_PARAM_INDEX,
        sema::TraitPredicateKind::builtin, sema::CapabilityKind::eq, nullptr);
    ASSERT_LT(recorded_predicate_index, analyzer.state_.checked.trait_predicates.size());
    EXPECT_EQ(analyzer.state_.checked.trait_predicates[recorded_predicate_index].subject_param_identity,
        sema::INVALID_GENERIC_PARAM_IDENTITY);

    syntax::ItemNode constraint_item;
    constraint_item.kind = syntax::ItemKind::fn_decl;
    constraint_item.name = "ConstraintFallback";
    constraint_item.name_id = intern_identifier(analyzer, constraint_item.name);
    constraint_item.generic_params = {syntax::GenericParamDecl{SEMA_TEST_GENERIC_PARAM_NAME, {}, param_id}};
    syntax::GenericConstraintDecl fallback_constraint;
    fallback_constraint.param_name = SEMA_TEST_GENERIC_PARAM_NAME;
    fallback_constraint.param_name_id = param_id;
    fallback_constraint.capability_names = {"Eq", "Drop"};
    constraint_item.where_constraints = {fallback_constraint};
    sema::SemanticAnalyzerCore::GenericTemplateInfo constraint_info =
        generic_template_info(analyzer, module_id(0), "ConstraintFallback");
    generic.validate_generic_constraints(constraint_item, constraint_info);
    EXPECT_TRUE(diagnostics.has_error());
    EXPECT_FALSE(constraint_info.predicate_indices.empty());

    sema::SemanticAnalyzerCore::GenericTemplateInfo info = generic_template_info(analyzer, module_id(0), "Fallback");
    EXPECT_TRUE(analyzer.generic_param_name(info, info.params.size()).empty());
    EXPECT_FALSE(is_valid(analyzer.generic_param_placeholder(info, info.params.size())));
    EXPECT_TRUE(is_valid(analyzer.generic_param_identity(info, info.param_identities.size())));
    sema::TypeInfo fallback_info;
    fallback_info.name = checked_text(analyzer.state_.checked, "FallbackParam");
    EXPECT_TRUE(is_valid(analyzer.generic_param_identity(fallback_info)));
}

TEST(CoreUnit, SemanticWhiteBoxResourceSemanticsClassifiesStructuralTypesAndFingerprints)
{
    syntax::AstModule module;
    module.modules = {module_info({"resource_semantics"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle str = types.builtin(BuiltinType::str);
    const TypeHandle pointer = types.pointer(PointerMutability::mut, i32);
    const TypeHandle reference = types.reference(PointerMutability::const_, i32);
    const TypeHandle slice = types.slice(PointerMutability::const_, i32);
    const TypeHandle array = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i32);
    const TypeHandle tuple = types.tuple({i32, reference});
    const TypeHandle function =
        types.function(sema::FunctionCallConv::aurex, false, false, std::span<const TypeHandle>{}, i32);
    const sema::GenericParamIdentity param_identity = sema::generic_param_identity_from_text("resource_semantics.T");
    const TypeHandle param = types.generic_param(param_identity, SEMA_TEST_GENERIC_PARAM_NAME);
    const query::MemberKey item_member = query::member_key(
        query::DefKey{}, query::MemberKind::associated_type, "Item", SEMA_TEST_GENERIC_FIRST_PARAM_INDEX);
    const TypeHandle projection = types.associated_projection(param, item_member, "Item");
    const TypeHandle generic_tuple = types.tuple({i32, param});
    const TypeHandle invalid_tuple = types.tuple({i32, INVALID_TYPE_HANDLE});
    const TypeHandle empty_tuple = types.tuple(std::span<const TypeHandle>{});
    const TypeHandle record = types.named_struct("resource_semantics.Record", "resource_semantics_Record", false);
    const TypeHandle empty_record =
        types.named_struct("resource_semantics.EmptyRecord", "resource_semantics_EmptyRecord", false);
    const TypeHandle missing_record =
        types.named_struct("resource_semantics.MissingRecord", "resource_semantics_MissingRecord", false);
    const TypeHandle recursive_record =
        types.named_struct("resource_semantics.RecursiveRecord", "resource_semantics_RecursiveRecord", false);
    const TypeHandle mutual_record_a =
        types.named_struct("resource_semantics.MutualRecordA", "resource_semantics_MutualRecordA", false);
    const TypeHandle mutual_record_b =
        types.named_struct("resource_semantics.MutualRecordB", "resource_semantics_MutualRecordB", false);
    const TypeHandle opaque = types.opaque_struct("resource_semantics.Opaque", "resource_semantics_Opaque");
    const TypeHandle choice = types.named_enum("resource_semantics.Choice", "resource_semantics_Choice");
    const TypeHandle empty_choice =
        types.named_enum("resource_semantics.EmptyChoice", "resource_semantics_EmptyChoice");
    const TypeHandle missing_choice =
        types.named_enum("resource_semantics.MissingChoice", "resource_semantics_MissingChoice");
    types.set_enum_underlying(choice, i32);
    types.set_enum_underlying(empty_choice, i32);
    TypeHandle deep_tuple = i32;
    for (base::usize depth = 0; depth < SEMA_TEST_RESOURCE_CLASSIFIER_DEEP_CHAIN_LENGTH; ++depth) {
        const std::array<TypeHandle, 1> nested{deep_tuple};
        deep_tuple = types.tuple(nested);
    }

    const StructInfo& record_info = add_struct_info(analyzer, module_id(0), "Record", record);
    const_cast<StructInfo&>(record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("value"),
            intern_identifier(analyzer, "value"),
            checked.intern_text("resource_semantics_Record_value"),
            module_id(0),
            generic_tuple,
            {},
            syntax::Visibility::public_,
            {},
        });
    static_cast<void>(add_struct_info(analyzer, module_id(0), "EmptyRecord", empty_record));
    const StructInfo& recursive_record_info =
        add_struct_info(analyzer, module_id(0), "RecursiveRecord", recursive_record);
    const_cast<StructInfo&>(recursive_record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("next"),
            intern_identifier(analyzer, "next"),
            checked.intern_text("resource_semantics_RecursiveRecord_next"),
            module_id(0),
            recursive_record,
            {},
            syntax::Visibility::public_,
            {},
        });
    const StructInfo& mutual_record_a_info = add_struct_info(analyzer, module_id(0), "MutualRecordA", mutual_record_a);
    const_cast<StructInfo&>(mutual_record_a_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("b"),
            intern_identifier(analyzer, "b"),
            checked.intern_text("resource_semantics_MutualRecordA_b"),
            module_id(0),
            mutual_record_b,
            {},
            syntax::Visibility::public_,
            {},
        });
    const StructInfo& mutual_record_b_info = add_struct_info(analyzer, module_id(0), "MutualRecordB", mutual_record_b);
    const_cast<StructInfo&>(mutual_record_b_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("a"),
            intern_identifier(analyzer, "a"),
            checked.intern_text("resource_semantics_MutualRecordB_a"),
            module_id(0),
            mutual_record_a,
            {},
            syntax::Visibility::public_,
            {},
        });
    static_cast<void>(add_enum_case(analyzer, module_id(0), "Choice_value", "value", choice, record, {record}));
    EnumCaseInfo invalid_case = checked.make_enum_case_info();
    invalid_case.name = checked.intern_text("InvalidCase");
    invalid_case.name_id = intern_identifier(analyzer, "InvalidCase");
    invalid_case.case_name = checked.intern_text("invalid");
    invalid_case.case_name_id = intern_identifier(analyzer, "invalid");
    const std::array invalid_payload_types{record};
    invalid_case.payload_types = checked.copy_type_handle_list(invalid_payload_types);
    checked.enum_cases.emplace(semantic_module_key(analyzer, module_id(0), "InvalidCase"), invalid_case);

    const ResourceSemanticsSummary copy_owned{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::owned_value,
    };
    const ResourceSemanticsSummary borrowed{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::borrowed_view,
    };
    const ResourceSemanticsSummary raw{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::trivial,
        ResourceOwnershipKind::raw_pointer,
    };
    const ResourceSemanticsSummary conservative{
        ResourceCopyKind::move_only,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::needs_drop,
        ResourceOwnershipKind::owned_value,
    };
    const ResourceSemanticsSummary copy_needs_drop{
        ResourceCopyKind::copy,
        ResourceDiscardKind::discard,
        ResourceCleanupKind::needs_drop,
        ResourceOwnershipKind::owned_value,
    };
    const ResourceSemanticsSummary shared_must_consume{
        ResourceCopyKind::copy,
        ResourceDiscardKind::must_consume,
        ResourceCleanupKind::needs_drop,
        ResourceOwnershipKind::shared_managed,
    };

    const ResourceSemanticsClassifier resources(checked);
    EXPECT_EQ(resources.classify(i32), copy_owned);
    EXPECT_EQ(resources.classify(str), borrowed);
    EXPECT_EQ(resources.classify(pointer), raw);
    EXPECT_EQ(resources.classify(reference), borrowed);
    EXPECT_EQ(resources.classify(slice), borrowed);
    EXPECT_EQ(resources.classify(function), copy_owned);
    EXPECT_EQ(resources.classify(array), copy_owned);
    EXPECT_EQ(resources.classify(tuple), copy_owned);
    EXPECT_EQ(resources.classify(empty_tuple), copy_owned);
    EXPECT_EQ(resources.classify(deep_tuple), copy_owned);
    EXPECT_EQ(resources.classify(param), conservative);
    EXPECT_EQ(resources.classify(projection), conservative);
    EXPECT_EQ(resources.classify(generic_tuple), conservative);
    EXPECT_EQ(resources.classify(invalid_tuple), conservative);
    EXPECT_EQ(resources.classify(record), conservative);
    EXPECT_EQ(resources.classify(empty_record), copy_owned);
    EXPECT_EQ(resources.classify(missing_record), conservative);
    EXPECT_EQ(resources.classify(recursive_record), conservative);
    EXPECT_EQ(resources.classify(mutual_record_a), conservative);
    EXPECT_EQ(resources.classify(mutual_record_b), conservative);
    EXPECT_EQ(resources.classify(opaque), conservative);
    EXPECT_EQ(resources.classify(choice), conservative);
    EXPECT_EQ(resources.classify(empty_choice), copy_owned);
    EXPECT_EQ(resources.classify(missing_choice), conservative);
    EXPECT_EQ(resources.classify(INVALID_TYPE_HANDLE), conservative);
    EXPECT_EQ(resources.classify(TypeHandle{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)}),
        conservative);

    const ResourceSemanticsClassifier copy_resources(checked, [param](const TypeHandle candidate) {
        return candidate.value == param.value;
    });
    EXPECT_EQ(copy_resources.classify(generic_tuple), copy_needs_drop);
    EXPECT_EQ(copy_resources.classify(record), copy_needs_drop);
    EXPECT_EQ(copy_resources.classify(choice), copy_needs_drop);
    const ResourceSemanticsClassifier missing_provider_resources(
        checked, {}, [](const TypeHandle) -> std::optional<std::vector<TypeHandle>> {
            return std::nullopt;
        });
    const ResourceSemanticsClassifier empty_provider_resources(
        checked, {}, [](const TypeHandle) -> std::optional<std::vector<TypeHandle>> {
            return std::vector<TypeHandle>{};
        });
    EXPECT_EQ(missing_provider_resources.classify(tuple), conservative);
    EXPECT_EQ(empty_provider_resources.classify(tuple), copy_owned);
    EXPECT_EQ(sema::resource_semantics_debug_string(copy_owned), "Copy/Discard/Trivial/OwnedValue");
    EXPECT_EQ(sema::resource_semantics_debug_string(shared_must_consume), "Copy/MustConsume/NeedsDrop/SharedManaged");
    EXPECT_TRUE(sema::resource_is_copy(copy_owned));
    EXPECT_FALSE(sema::resource_is_copy(conservative));
    EXPECT_FALSE(sema::resource_needs_drop(copy_owned));
    EXPECT_TRUE(sema::resource_needs_drop(conservative));
    EXPECT_EQ(sema::resource_copy_kind_name(static_cast<ResourceCopyKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::resource_discard_kind_name(static_cast<ResourceDiscardKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::resource_cleanup_kind_name(static_cast<ResourceCleanupKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(
        sema::resource_ownership_kind_name(static_cast<ResourceOwnershipKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::resource_semantics_fingerprint(copy_owned), sema::resource_semantics_fingerprint(copy_owned));
    EXPECT_NE(sema::resource_semantics_fingerprint(copy_owned), sema::resource_semantics_fingerprint(conservative));
    EXPECT_NE(sema::dump_checked_module(checked).find("resource_summaries"), std::string::npos);
    EXPECT_NE(sema::dump_checked_module(checked).find("Copy/Discard/Trivial/OwnedValue"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxDropGluePlansStructuralGenericAndOpaqueCleanup)
{
    syntax::AstModule module;
    module.modules = {module_info({"drop_glue"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const sema::GenericParamIdentity param_identity = sema::generic_param_identity_from_text("drop_glue.T");
    const TypeHandle param = types.generic_param(param_identity, SEMA_TEST_GENERIC_PARAM_NAME);
    const TypeHandle tuple = types.tuple({i32, param});
    const TypeHandle array = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, tuple);
    const TypeHandle record = types.named_struct("drop_glue.Record", "drop_glue_Record", false);
    const TypeHandle choice = types.named_enum("drop_glue.Choice", "drop_glue_Choice");
    const TypeHandle opaque = types.opaque_struct("drop_glue.Opaque", "drop_glue_Opaque");
    types.set_enum_underlying(choice, i32);

    const StructInfo& record_info = add_struct_info(analyzer, module_id(0), "Record", record);
    const_cast<StructInfo&>(record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("copy"),
            intern_identifier(analyzer, "copy"),
            checked.intern_text("drop_glue_Record_copy"),
            module_id(0),
            i32,
            {},
            syntax::Visibility::public_,
            {},
        });
    const_cast<StructInfo&>(record_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("owned"),
            intern_identifier(analyzer, "owned"),
            checked.intern_text("drop_glue_Record_owned"),
            module_id(0),
            array,
            {},
            syntax::Visibility::public_,
            {},
        });
    static_cast<void>(add_enum_case(analyzer, module_id(0), "Choice_record", "record", choice, record, {record}));

    base::Result<sema::DropGluePlan> record_plan = sema::build_drop_glue_plan(checked, record);
    ASSERT_TRUE(record_plan) << record_plan.error().message;
    EXPECT_TRUE(sema::drop_glue_plan_needs_drop(record_plan.value()));
    ASSERT_EQ(record_plan.value().steps.size(), 4U);
    EXPECT_EQ(record_plan.value().steps[0].kind, sema::DropGlueStepKind::struct_field);
    EXPECT_EQ(record_plan.value().steps[0].ordinal, 1U);
    EXPECT_EQ(record_plan.value().steps[1].kind, sema::DropGlueStepKind::array_element);
    EXPECT_EQ(record_plan.value().steps[2].kind, sema::DropGlueStepKind::tuple_element);
    EXPECT_EQ(record_plan.value().steps[2].ordinal, 1U);
    EXPECT_EQ(record_plan.value().steps[3].kind, sema::DropGlueStepKind::generic_value);
    EXPECT_NE(record_plan.value().fingerprint.byte_count, 0U);
    EXPECT_EQ(sema::drop_glue_step_kind_name(record_plan.value().steps[0].kind), "struct_field");

    base::Result<sema::DropGluePlan> enum_plan = sema::build_drop_glue_plan(checked, choice);
    ASSERT_TRUE(enum_plan) << enum_plan.error().message;
    ASSERT_FALSE(enum_plan.value().steps.empty());
    EXPECT_EQ(enum_plan.value().steps.front().kind, sema::DropGlueStepKind::enum_payload);

    base::Result<sema::DropGluePlan> opaque_plan = sema::build_drop_glue_plan(checked, opaque);
    ASSERT_TRUE(opaque_plan) << opaque_plan.error().message;
    ASSERT_EQ(opaque_plan.value().steps.size(), 1U);
    EXPECT_EQ(opaque_plan.value().steps.front().kind, sema::DropGlueStepKind::opaque_value);

    base::Result<sema::DropGluePlan> trivial_plan = sema::build_drop_glue_plan(checked, i32);
    ASSERT_TRUE(trivial_plan) << trivial_plan.error().message;
    EXPECT_FALSE(sema::drop_glue_plan_needs_drop(trivial_plan.value()));
    EXPECT_TRUE(trivial_plan.value().steps.empty());
    EXPECT_FALSE(sema::build_drop_glue_plan(checked, INVALID_TYPE_HANDLE));
}

TEST(CoreUnit, SemanticWhiteBoxDropGlueCoversMissingRecursiveEnumAndInvalidEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"drop_glue_edges"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle missing_record = types.named_struct("drop_glue_edges.Missing", "drop_glue_edges_Missing", false);
    const TypeHandle recursive_record =
        types.named_struct("drop_glue_edges.Recursive", "drop_glue_edges_Recursive", false);
    const TypeHandle empty_choice = types.named_enum("drop_glue_edges.EmptyChoice", "drop_glue_edges_EmptyChoice");
    const TypeHandle missing_choice =
        types.named_enum("drop_glue_edges.MissingChoice", "drop_glue_edges_MissingChoice");
    const TypeHandle mixed_choice = types.named_enum("drop_glue_edges.MixedChoice", "drop_glue_edges_MixedChoice");
    const query::MemberKey associated_member = query::member_key(
        query::DefKey{}, query::MemberKind::associated_type, "Item", SEMA_TEST_GENERIC_FIRST_PARAM_INDEX);
    const TypeHandle associated_projection = types.associated_projection(recursive_record, associated_member, "Item");
    types.set_enum_underlying(empty_choice, i32);
    types.set_enum_underlying(mixed_choice, i32);

    const StructInfo& recursive_info = add_struct_info(analyzer, module_id(0), "Recursive", recursive_record);
    const_cast<StructInfo&>(recursive_info)
        .fields.push_back(StructFieldInfo{
            checked.intern_text("next"),
            intern_identifier(analyzer, "next"),
            checked.intern_text("drop_glue_edges_Recursive_next"),
            module_id(0),
            recursive_record,
            {},
            syntax::Visibility::public_,
            {},
        });
    static_cast<void>(add_enum_case(analyzer, module_id(0), "Mixed_i32", "i32", mixed_choice, i32, {i32}));
    static_cast<void>(add_enum_case(
        analyzer, module_id(0), "Mixed_recursive", "recursive", mixed_choice, recursive_record, {recursive_record}));

    base::Result<sema::DropGluePlan> missing_record_plan = sema::build_drop_glue_plan(checked, missing_record);
    ASSERT_TRUE(missing_record_plan) << missing_record_plan.error().message;
    ASSERT_EQ(missing_record_plan.value().steps.size(), 1U);
    EXPECT_EQ(missing_record_plan.value().steps.front().kind, sema::DropGlueStepKind::opaque_value);

    base::Result<sema::DropGluePlan> recursive_plan = sema::build_drop_glue_plan(checked, recursive_record);
    ASSERT_TRUE(recursive_plan) << recursive_plan.error().message;
    ASSERT_EQ(recursive_plan.value().steps.size(), 2U);
    EXPECT_EQ(recursive_plan.value().steps[0].kind, sema::DropGlueStepKind::struct_field);
    EXPECT_EQ(recursive_plan.value().steps[1].kind, sema::DropGlueStepKind::generic_value);

    base::Result<sema::DropGluePlan> empty_enum_plan = sema::build_drop_glue_plan(checked, empty_choice);
    ASSERT_TRUE(empty_enum_plan) << empty_enum_plan.error().message;
    EXPECT_FALSE(sema::drop_glue_plan_needs_drop(empty_enum_plan.value()));
    EXPECT_TRUE(empty_enum_plan.value().steps.empty());

    base::Result<sema::DropGluePlan> missing_enum_plan = sema::build_drop_glue_plan(checked, missing_choice);
    ASSERT_TRUE(missing_enum_plan) << missing_enum_plan.error().message;
    EXPECT_TRUE(sema::drop_glue_plan_needs_drop(missing_enum_plan.value()));
    EXPECT_TRUE(missing_enum_plan.value().steps.empty());

    base::Result<sema::DropGluePlan> mixed_enum_plan = sema::build_drop_glue_plan(checked, mixed_choice);
    ASSERT_TRUE(mixed_enum_plan) << mixed_enum_plan.error().message;
    ASSERT_EQ(mixed_enum_plan.value().steps.size(), 3U);
    EXPECT_EQ(mixed_enum_plan.value().steps.front().kind, sema::DropGlueStepKind::enum_payload);

    base::Result<sema::DropGluePlan> associated_plan = sema::build_drop_glue_plan(checked, associated_projection);
    ASSERT_TRUE(associated_plan) << associated_plan.error().message;
    ASSERT_EQ(associated_plan.value().steps.size(), 1U);
    EXPECT_EQ(associated_plan.value().steps.front().kind, sema::DropGlueStepKind::generic_value);

    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::custom_destructor), "custom_destructor");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::tuple_element), "tuple_element");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::array_element), "array_element");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::enum_payload), "enum_payload");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::generic_value), "generic_value");
    EXPECT_EQ(sema::drop_glue_step_kind_name(sema::DropGlueStepKind::opaque_value), "opaque_value");
    EXPECT_EQ(
        sema::drop_glue_step_kind_name(static_cast<sema::DropGlueStepKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "invalid");
    EXPECT_FALSE(sema::build_drop_glue_plan(
        checked, TypeHandle{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)}));
}

TEST(CoreUnit, SemanticWhiteBoxDropCheckFactsNamesFingerprintsAuthorityAndDump)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle param =
        types.generic_param(sema::generic_param_identity_from_text("dropck.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_manual");
    const sema::FunctionLookupKey destructor =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_destructor");
    const query::StableFingerprint128 drop_glue_fingerprint = query::stable_fingerprint("dropck.manual.glue");

    sema::DropCheckFact fact;
    fact.type = param;
    fact.destructor_function = destructor;
    fact.required_outlives.push_back(sema::DropCheckRequiredOutlives{
        .type = param,
        .region = 0U,
        .reason = sema::LifetimeConstraintReason::dropck,
        .range = body_loan_test_range(0),
    });
    fact.drop_glue_fingerprint = drop_glue_fingerprint;
    fact.may_observe_fields = true;
    fact.may_move_fields = false;
    fact.fingerprint = sema::drop_check_fact_fingerprint(fact);

    sema::DropActionFact action;
    action.kind = sema::DropCheckActionKind::lexical_cleanup;
    action.action = 0U;
    action.point = 1U;
    action.place = 2U;
    action.type = param;
    action.destructor_key = drop_glue_fingerprint;
    action.range = body_loan_test_range(1);

    sema::DropCheckViolation borrowed_drop;
    borrowed_drop.kind = sema::DropCheckViolationKind::borrowed_drop;
    borrowed_drop.action = action.action;
    borrowed_drop.point = action.point;
    borrowed_drop.place = action.place;
    borrowed_drop.type = param;
    borrowed_drop.region = sema::SEMA_LIFETIME_INVALID_INDEX;
    borrowed_drop.diagnostic_emitted = true;
    borrowed_drop.range = body_loan_test_range(2);

    sema::DropCheckViolation generic_outlives = borrowed_drop;
    generic_outlives.kind = sema::DropCheckViolationKind::generic_type_outlives;
    generic_outlives.region = 0U;
    generic_outlives.diagnostic_emitted = false;

    sema::DropCheckViolation glue_missing = borrowed_drop;
    glue_missing.kind = sema::DropCheckViolationKind::drop_glue_missing;
    glue_missing.action = 1U;
    glue_missing.point = 3U;
    glue_missing.diagnostic_emitted = false;

    sema::FunctionDropCheckFacts facts;
    facts.function = key;
    facts.facts.push_back(fact);
    facts.actions.push_back(action);
    facts.violations.push_back(borrowed_drop);
    facts.violations.push_back(generic_outlives);
    facts.violations.push_back(glue_missing);
    facts.solved = true;
    facts.diagnostic_mode_enforced = true;
    facts.graph_missing = true;
    facts.part_index = 1U;
    facts.fingerprint = sema::function_drop_check_facts_fingerprint(facts);
    checked.dropck_facts[key] = facts;

    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::lexical_cleanup), "lexical_cleanup");
    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::overwrite), "overwrite");
    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::early_exit), "early_exit");
    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::explicit_drop), "explicit_drop");
    EXPECT_EQ(sema::drop_check_action_kind_name(sema::DropCheckActionKind::defer_cleanup), "defer_cleanup");
    EXPECT_EQ(
        sema::drop_check_action_kind_name(static_cast<sema::DropCheckActionKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::borrowed_drop), "borrowed_drop");
    EXPECT_EQ(
        sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::borrowed_field_dangling),
        "borrowed_field_dangling");
    EXPECT_EQ(
        sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::generic_type_outlives),
        "generic_type_outlives");
    EXPECT_EQ(
        sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::destructor_escape), "destructor_escape");
    EXPECT_EQ(
        sema::drop_check_violation_kind_name(sema::DropCheckViolationKind::drop_glue_missing), "drop_glue_missing");
    EXPECT_EQ(sema::drop_check_violation_kind_name(
                  static_cast<sema::DropCheckViolationKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::borrowed_drop),
        "drop check rejected dropping storage while it is still borrowed");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::borrowed_field_dangling),
        "drop check rejected destructor access to a borrowed field that may dangle");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::generic_type_outlives),
        "drop check rejected generic drop glue because borrowed contents may not outlive the drop");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::destructor_escape),
        "drop check rejected destructor body because it may leak borrowed storage");
    EXPECT_EQ(sema::drop_check_violation_message(sema::DropCheckViolationKind::drop_glue_missing),
        "drop check could not build drop glue for this value");
    EXPECT_EQ(
        sema::drop_check_violation_message(
            static_cast<sema::DropCheckViolationKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "drop check rejected generic drop glue because borrowed contents may not outlive the drop");

    const query::StableFingerprint128 baseline = sema::function_drop_check_facts_fingerprint(facts);
    sema::FunctionDropCheckFacts range_changed = facts;
    range_changed.facts.front().required_outlives.front().range = body_loan_test_range(4);
    range_changed.actions.front().range = body_loan_test_range(5);
    range_changed.violations.front().range = body_loan_test_range(6);
    EXPECT_EQ(baseline, sema::function_drop_check_facts_fingerprint(range_changed));
    sema::FunctionDropCheckFacts action_changed = facts;
    action_changed.actions.front().kind = sema::DropCheckActionKind::overwrite;
    EXPECT_NE(baseline, sema::function_drop_check_facts_fingerprint(action_changed));
    sema::FunctionDropCheckFacts violation_changed = facts;
    violation_changed.violations.front().kind = sema::DropCheckViolationKind::destructor_escape;
    EXPECT_NE(baseline, sema::function_drop_check_facts_fingerprint(violation_changed));

    const std::string dump = sema::dump_function_drop_check_facts(facts);
    EXPECT_NE(dump.find("dropck_facts"), std::string::npos) << dump;
    EXPECT_NE(dump.find("required=1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("reason=dropck"), std::string::npos) << dump;
    EXPECT_NE(dump.find("borrowed_drop"), std::string::npos) << dump;
    EXPECT_NE(dump.find("drop_glue_missing"), std::string::npos) << dump;
    EXPECT_NE(dump.find("fingerprint="), std::string::npos) << dump;

    sema::FunctionDropCheckFacts sparse_facts;
    sparse_facts.function = sema::FunctionLookupKey{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        sema::INVALID_IDENT_ID,
    };
    sema::DropCheckFact sparse_fact = fact;
    sparse_fact.required_outlives.clear();
    sparse_fact.may_observe_fields = false;
    sparse_fact.may_move_fields = true;
    sparse_facts.facts.push_back(sparse_fact);
    sparse_facts.fingerprint = sema::function_drop_check_facts_fingerprint(sparse_facts);
    const std::string sparse_dump = sema::dump_function_drop_check_facts(sparse_facts);
    const std::string invalid_function_label =
        "function=0:" + std::to_string(sema::SEMA_LOOKUP_INVALID_KEY_PART) + ":-";
    EXPECT_NE(sparse_dump.find(invalid_function_label), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("solved=false"), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("enforced=false"), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("graph_missing=false"), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("observe=false"), std::string::npos) << sparse_dump;
    EXPECT_NE(sparse_dump.find("move=true"), std::string::npos) << sparse_dump;

    sema::CheckedModule copy = checked;
    ASSERT_TRUE(copy.dropck_facts.contains(key));
    EXPECT_EQ(copy.dropck_facts.at(key).facts.size(), 1U);
    EXPECT_EQ(copy.dropck_facts.at(key).violations.size(), 3U);
    const std::string checked_dump = sema::dump_checked_module(copy);
    EXPECT_NE(checked_dump.find("dropck_facts"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("dropck_fact"), std::string::npos) << checked_dump;

    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("dropck.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("dropck.body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("dropck.signature"));
    sema::populate_type_check_body_borrow_authority(authority, checked, key);

    EXPECT_TRUE(authority.has_dropck_facts);
    EXPECT_EQ(authority.dropck_fact_count, 1U);
    EXPECT_EQ(authority.dropck_action_count, 1U);
    EXPECT_EQ(authority.dropck_required_outlives_count, 1U);
    EXPECT_EQ(authority.dropck_violation_count, 3U);
    EXPECT_TRUE(authority.dropck_graph_missing);
    EXPECT_TRUE(authority.dropck_has_emitted_diagnostics);
    EXPECT_TRUE(authority.dropck_has_generic_type_outlives);
    EXPECT_TRUE(authority.dropck_has_borrowed_drop);
    EXPECT_TRUE(authority.dropck_has_drop_glue_missing);
    EXPECT_TRUE(query::is_valid(query::type_check_body_result_fingerprint(authority)));
}

TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerRecordsActionsOutlivesAndBorrowedDrop)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_analyze"})};
    const IdentId value_id = module.intern_identifier(SEMA_TEST_BODY_LOAN_VALUE_NAME);
    const IdentId function_id = module.intern_identifier("dropck_value");
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));

    syntax::ParamDecl param_decl;
    param_decl.name = SEMA_TEST_BODY_LOAN_VALUE_NAME;
    param_decl.type = generic_type;
    param_decl.range = body_loan_test_range(0);
    param_decl.name_id = value_id;
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_value";
    function.name_id = function_id;
    function.params = {param_decl};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle param =
        types.generic_param(sema::generic_param_identity_from_text("dropck_analyze.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_value");

    FunctionSignature signature =
        function_signature("dropck_value", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.param_types = {param};
    signature.has_definition = true;
    signature.part_index = 2U;

    sema::BodyFlowGraph graph;
    graph.function = key;
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::entry,
        .range = body_loan_test_range(0),
    });
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::cleanup_scope,
        .range = body_loan_test_range(1),
    });
    graph.points.push_back(sema::BodyFlowPoint{
        .kind = sema::BodyFlowPointKind::exit,
        .range = body_loan_test_range(2),
    });
    graph.edges.push_back(sema::BodyFlowEdge{.from = 0U, .to = 1U});
    graph.edges.push_back(sema::BodyFlowEdge{.from = 1U, .to = 2U});
    graph.places.push_back(body_loan_local_place(value_id, syntax::INVALID_EXPR_ID, 0));
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 0U,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 0U,
        .range = body_loan_test_range(2),
    });
    checked.body_flow_graphs[key] = std::move(graph);

    sema::FunctionLifetimeFacts lifetime;
    lifetime.function = key;
    lifetime.return_type = void_type;
    sema::LifetimeRegion region;
    region.kind = sema::LifetimeRegionKind::inferred;
    region.range = body_loan_test_range(0);
    lifetime.regions.push_back(region);
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 0U,
        .last_point = 2U,
        .point_count = 3U,
        .range = body_loan_test_range(0),
    });
    checked.lifetime_facts[key] = std::move(lifetime);

    sema::BodyLoanCheckResult loan;
    loan.function = key;
    loan.diagnostic_mode = sema::BodyLoanDiagnosticMode::enforced;
    loan.conflicts.push_back(sema::BodyLoanConflict{
        .kind = sema::BodyLoanConflictKind::cleanup,
        .action = 0U,
        .point = 1U,
        .place = 0U,
        .diagnostic_emitted = false,
        .range = body_loan_test_range(1),
    });
    loan.conflicts.push_back(sema::BodyLoanConflict{
        .kind = sema::BodyLoanConflictKind::drop,
        .action = 0U,
        .point = 1U,
        .place = 0U,
        .diagnostic_emitted = false,
        .range = body_loan_test_range(1),
    });
    loan.conflicts.push_back(sema::BodyLoanConflict{
        .kind = sema::BodyLoanConflictKind::cleanup,
        .action = 1U,
        .point = 1U,
        .place = 0U,
        .diagnostic_emitted = false,
        .range = body_loan_test_range(2),
    });
    checked.body_loan_checks[key] = std::move(loan);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_TRUE(facts.solved);
    EXPECT_TRUE(facts.diagnostic_mode_enforced);
    EXPECT_FALSE(facts.graph_missing);
    ASSERT_EQ(facts.actions.size(), 2U);
    EXPECT_EQ(facts.actions.front().kind, sema::DropCheckActionKind::lexical_cleanup);
    EXPECT_EQ(facts.actions.front().point, 1U);
    ASSERT_EQ(facts.facts.size(), 1U);
    EXPECT_EQ(facts.facts.front().type.value, param.value);
    ASSERT_EQ(facts.facts.front().required_outlives.size(), 1U);
    EXPECT_EQ(facts.facts.front().required_outlives.front().type.value, param.value);
    EXPECT_EQ(facts.facts.front().required_outlives.front().region, 0U);
    EXPECT_EQ(facts.facts.front().required_outlives.front().reason, sema::LifetimeConstraintReason::dropck);
    ASSERT_EQ(facts.violations.size(), 2U);
    EXPECT_TRUE(std::ranges::all_of(facts.violations, [](const sema::DropCheckViolation& violation) {
        return violation.kind == sema::DropCheckViolationKind::borrowed_drop && violation.diagnostic_emitted;
    }));

    ASSERT_TRUE(checked.lifetime_facts.contains(key));
    const sema::FunctionLifetimeFacts& lifetime_after = checked.lifetime_facts.at(key);
    ASSERT_EQ(lifetime_after.type_outlives_constraints.size(), 1U);
    EXPECT_EQ(lifetime_after.type_outlives_constraints.front().type.value, param.value);
    EXPECT_EQ(lifetime_after.type_outlives_constraints.front().region, 0U);
    EXPECT_EQ(lifetime_after.type_outlives_constraints.front().reason, sema::LifetimeConstraintReason::dropck);
    EXPECT_NE(facts.fingerprint.byte_count, 0U);
    EXPECT_NE(lifetime_after.fingerprint.byte_count, 0U);
    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find("drop check rejected dropping storage"), std::string::npos);
    EXPECT_EQ(messages.find("drop check rejected generic drop glue"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerRecordsMissingGraphForLoanConflict)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_missing_graph"})};
    const IdentId function_id = module.intern_identifier("dropck_missing_graph");

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_missing_graph";
    function.name_id = function_id;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_missing_graph");

    FunctionSignature signature = function_signature(
        "dropck_missing_graph", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.has_definition = true;

    sema::BodyLoanCheckResult loan;
    loan.function = key;
    loan.diagnostic_mode = sema::BodyLoanDiagnosticMode::enforced;
    loan.conflicts.push_back(sema::BodyLoanConflict{
        .kind = sema::BodyLoanConflictKind::reinit,
        .action = 0U,
        .point = 1U,
        .place = 0U,
        .range = body_loan_test_range(0),
    });
    checked.body_loan_checks[key] = std::move(loan);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_TRUE(facts.graph_missing);
    EXPECT_TRUE(facts.solved);
    EXPECT_TRUE(facts.actions.empty());
    EXPECT_TRUE(facts.violations.empty());
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerCoversMalformedPatternsAndProjectionEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_edges"})};
    const IdentId function_id = module.intern_identifier("dropck_edges");
    const IdentId owned_id = module.intern_identifier("owned");
    const IdentId slice_id = module.intern_identifier("slice_value");
    const IdentId ptr_id = module.intern_identifier("ptr_value");
    const IdentId missing_id = module.intern_identifier("missing_value");
    const IdentId field_id = module.intern_identifier("field");
    const IdentId missing_field_id = module.intern_identifier("missing_field");
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));
    const ExprId temporary_expr = push_name(module, "temporary");

    syntax::PatternNode field_binding;
    field_binding.kind = syntax::PatternKind::binding;
    field_binding.binding_name = "field_binding";
    field_binding.binding_name_id = module.intern_identifier("field_binding");
    const syntax::PatternId field_binding_id = module.push_pattern(field_binding);

    syntax::PatternNode non_struct_pattern;
    non_struct_pattern.kind = syntax::PatternKind::struct_;
    non_struct_pattern.struct_name = "NotAStruct";
    non_struct_pattern.field_patterns = {syntax::FieldPattern{"field", field_binding_id, body_loan_test_range(0),
        field_id}};
    const syntax::PatternId non_struct_pattern_id = module.push_pattern(non_struct_pattern);

    syntax::PatternNode missing_field_pattern;
    missing_field_pattern.kind = syntax::PatternKind::struct_;
    missing_field_pattern.struct_name = "Record";
    missing_field_pattern.field_patterns = {
        syntax::FieldPattern{"missing_field", field_binding_id, body_loan_test_range(1), missing_field_id}};
    const syntax::PatternId missing_field_pattern_id = module.push_pattern(missing_field_pattern);

    syntax::StmtNode invalid_pattern_decl;
    invalid_pattern_decl.kind = syntax::StmtKind::let;
    invalid_pattern_decl.pattern = syntax::PatternId{SEMA_TEST_BODY_FLOW_OUT_OF_RANGE_NODE};
    const syntax::StmtId invalid_pattern_decl_id = module.push_stmt(invalid_pattern_decl);

    syntax::StmtNode non_struct_decl;
    non_struct_decl.kind = syntax::StmtKind::let;
    non_struct_decl.pattern = non_struct_pattern_id;
    const syntax::StmtId non_struct_decl_id = module.push_stmt(non_struct_decl);

    syntax::StmtNode missing_field_decl;
    missing_field_decl.kind = syntax::StmtKind::let;
    missing_field_decl.pattern = missing_field_pattern_id;
    const syntax::StmtId missing_field_decl_id = module.push_stmt(missing_field_decl);

    const syntax::StmtId body = push_block(module,
        {invalid_pattern_decl_id, syntax::StmtId{SEMA_TEST_BODY_FLOW_OUT_OF_RANGE_NODE}, non_struct_decl_id,
            missing_field_decl_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_edges";
    function.name_id = function_id;
    function.params = {
        syntax::ParamDecl{"owned", generic_type, {}, owned_id},
        syntax::ParamDecl{"slice_value", generic_type, {}, slice_id},
        syntax::ParamDecl{"ptr_value", generic_type, {}, ptr_id},
    };
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle param =
        types.generic_param(sema::generic_param_identity_from_text("dropck_edges.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    const TypeHandle record = types.named_struct("dropck_edges.Record", "dropck_edges_Record", false);
    const TypeHandle slice_param = types.slice(PointerMutability::const_, param);
    const TypeHandle pointer_param = types.pointer(PointerMutability::const_, param);
    const StructInfo& record_info =
        add_struct_info(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Record", record);
    const_cast<StructInfo&>(record_info).fields.push_back(
        struct_field_info(analyzer, "field", module_id(SEMA_TEST_ROOT_MODULE_INDEX), param));
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_edges");
    FunctionSignature signature =
        function_signature("dropck_edges", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.param_types = {param, slice_param, pointer_param};
    signature.has_definition = true;

    checked.stmt_local_types.assign(analyzer.ctx_.module.stmts.size(), INVALID_TYPE_HANDLE);
    analyzer.record_stmt_local_type(invalid_pattern_decl_id, i32);
    analyzer.record_stmt_local_type(non_struct_decl_id, i32);
    analyzer.record_stmt_local_type(missing_field_decl_id, record);
    prepare_expr_storage(analyzer, analyzer.ctx_.module);
    static_cast<void>(analyzer.record_expr_type(temporary_expr, param));

    sema::BodyFlowGraph graph;
    graph.function = key;
    for (base::u32 point = 0U; point < 4U; ++point) {
        graph.points.push_back(sema::BodyFlowPoint{
            .kind = point == 0U ? sema::BodyFlowPointKind::entry : sema::BodyFlowPointKind::sequence,
            .range = body_loan_test_range(point),
        });
        if (point > 0U) {
            graph.edges.push_back(sema::BodyFlowEdge{.from = point - 1U, .to = point});
        }
    }
    graph.places.push_back(body_loan_local_place(owned_id, syntax::INVALID_EXPR_ID, 0));
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::temporary,
        .root_expr = temporary_expr,
        .projections = {},
        .range = body_loan_test_range(1),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = slice_id,
        .projections = {sema::BodyFlowPlaceProjection{.kind = sema::BodyFlowPlaceProjectionKind::index}},
        .range = body_loan_test_range(2),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = slice_id,
        .projections = {sema::BodyFlowPlaceProjection{.kind = sema::BodyFlowPlaceProjectionKind::slice}},
        .range = body_loan_test_range(3),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = ptr_id,
        .projections = {sema::BodyFlowPlaceProjection{.kind = sema::BodyFlowPlaceProjectionKind::dereference}},
        .range = body_loan_test_range(4),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = missing_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::field,
            .field_name_id = field_id,
        }},
        .range = body_loan_test_range(5),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = static_cast<sema::BodyFlowActionKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE),
        .point = 0U,
        .place = 0U,
        .range = body_loan_test_range(0),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = sema::SEMA_BODY_FLOW_INVALID_INDEX,
        .place = 0U,
        .range = body_loan_test_range(1),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 0U,
        .place = 1U,
        .range = body_loan_test_range(2),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 2U,
        .range = body_loan_test_range(3),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 2U,
        .place = 3U,
        .range = body_loan_test_range(4),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 3U,
        .place = 4U,
        .range = body_loan_test_range(5),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::cleanup_storage,
        .point = 1U,
        .place = 5U,
        .range = body_loan_test_range(6),
    });
    checked.body_flow_graphs[key] = std::move(graph);

    sema::FunctionLifetimeFacts lifetime;
    lifetime.function = key;
    lifetime.return_type = void_type;
    sema::LifetimeRegion region;
    region.kind = sema::LifetimeRegionKind::inferred;
    lifetime.regions.push_back(region);
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 0U,
        .last_point = 3U,
        .point_count = 4U,
        .range = body_loan_test_range(0),
    });
    checked.lifetime_facts[key] = std::move(lifetime);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_TRUE(facts.solved);
    EXPECT_FALSE(facts.graph_missing);
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.point == sema::SEMA_BODY_FLOW_INVALID_INDEX;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.place == 1U;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.place == 3U;
    }));
    ASSERT_TRUE(checked.lifetime_facts.contains(key));
    EXPECT_TRUE(std::ranges::any_of(checked.lifetime_facts.at(key).type_outlives_constraints,
        [param](const sema::LifetimeTypeOutlivesConstraint& constraint) {
            return constraint.type.value == param.value && constraint.reason == sema::LifetimeConstraintReason::dropck;
        }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxDropCheckAnalyzerCoversProjectedPlacesAndTypeTraversal)
{
    syntax::AstModule module;
    module.modules = {module_info({"dropck_projected"})};
    const IdentId function_id = module.intern_identifier("dropck_projected");
    const IdentId record_id = module.intern_identifier("record");
    const IdentId items_id = module.intern_identifier("items");
    const IdentId ref_id = module.intern_identifier("ref_value");
    const IdentId choice_id = module.intern_identifier("choice");
    const IdentId associated_id = module.intern_identifier("associated");
    const IdentId opaque_id = module.intern_identifier("opaque");
    const IdentId field_id = module.intern_identifier("field");
    const IdentId missing_field_id = module.intern_identifier("missing_field");
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));

    const auto param_decl = [&module, generic_type](const std::string_view name) {
        syntax::ParamDecl param;
        param.name = name;
        param.type = generic_type;
        param.name_id = module.intern_identifier(name);
        return param;
    };

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dropck_projected";
    function.name_id = function_id;
    function.params = {
        param_decl("record"),
        param_decl("items"),
        param_decl("ref_value"),
        param_decl("choice"),
        param_decl("associated"),
        param_decl("opaque"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::CheckedModule& checked = analyzer.state_.checked;
    sema::TypeTable& types = checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle param =
        types.generic_param(sema::generic_param_identity_from_text("dropck_projected.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    const TypeHandle record = types.named_struct("dropck_projected.Record", "dropck_projected_Record", false);
    const TypeHandle array = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, param);
    const TypeHandle ref_param = types.reference(PointerMutability::const_, param);
    const TypeHandle choice = types.named_enum("dropck_projected.Choice", "dropck_projected_Choice");
    const query::MemberKey associated_member = query::member_key(
        query::DefKey{}, query::MemberKind::associated_type, "Item", SEMA_TEST_GENERIC_FIRST_PARAM_INDEX);
    const TypeHandle associated_projection = types.associated_projection(record, associated_member, "Item");
    const TypeHandle opaque = types.opaque_struct("dropck_projected.Opaque", "dropck_projected_Opaque");
    const StructInfo& record_info =
        add_struct_info(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Record", record);
    const_cast<StructInfo&>(record_info).fields.push_back(
        struct_field_info(analyzer, "field", module_id(SEMA_TEST_ROOT_MODULE_INDEX), param));
    static_cast<void>(add_enum_case(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Choice_payload", "payload", choice, param, {param}));

    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "dropck_projected");
    FunctionSignature signature =
        function_signature("dropck_projected", module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, function_id, checked);
    signature.semantic_key = key;
    signature.param_types = {record, array, ref_param, choice, associated_projection, opaque};
    signature.has_definition = true;
    signature.part_index = 3U;

    sema::BodyFlowGraph graph;
    graph.function = key;
    for (base::u32 point = 0U; point < 10U; ++point) {
        graph.points.push_back(sema::BodyFlowPoint{
            .kind = point == 0U ? sema::BodyFlowPointKind::entry : sema::BodyFlowPointKind::sequence,
            .range = body_loan_test_range(point),
        });
        if (point > 0U) {
            graph.edges.push_back(sema::BodyFlowEdge{.from = point - 1U, .to = point});
        }
    }
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = record_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::field,
            .field_name_id = field_id,
        }},
        .range = body_loan_test_range(0),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = items_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::index,
        }},
        .range = body_loan_test_range(1),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = ref_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::dereference,
        }},
        .range = body_loan_test_range(2),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = items_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::slice,
        }},
        .range = body_loan_test_range(3),
    });
    graph.places.push_back(body_loan_local_place(items_id, syntax::INVALID_EXPR_ID, 4));
    graph.places.push_back(body_loan_local_place(choice_id, syntax::INVALID_EXPR_ID, 5));
    graph.places.push_back(body_loan_local_place(associated_id, syntax::INVALID_EXPR_ID, 6));
    graph.places.push_back(body_loan_local_place(opaque_id, syntax::INVALID_EXPR_ID, 7));
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::unknown,
        .projections = {},
        .range = body_loan_test_range(8),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = record_id,
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::field,
            .field_name_id = missing_field_id,
        }},
        .range = body_loan_test_range(9),
    });

    const auto push_action = [&graph](const sema::BodyFlowActionKind kind, const base::u32 point,
                                 const base::u32 place, const base::usize range_index) {
        graph.actions.push_back(sema::BodyFlowAction{
            .kind = kind,
            .point = point,
            .place = place,
            .range = body_loan_test_range(range_index),
        });
    };
    push_action(sema::BodyFlowActionKind::drop, 1U, 0U, 0);
    push_action(sema::BodyFlowActionKind::reinit, 2U, 1U, 1);
    push_action(sema::BodyFlowActionKind::cleanup_scope, 3U, 2U, 2);
    push_action(sema::BodyFlowActionKind::return_, 4U, 3U, 3);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 5U, 4U, 4);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 6U, 5U, 5);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 7U, 6U, 6);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 8U, 7U, 7);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 8U, 8U, 8);
    push_action(sema::BodyFlowActionKind::cleanup_storage, 8U, 9U, 9);
    checked.body_flow_graphs[key] = std::move(graph);

    sema::FunctionLifetimeFacts lifetime;
    lifetime.function = key;
    lifetime.return_type = void_type;
    lifetime.regions.push_back(sema::LifetimeRegion{.kind = sema::LifetimeRegionKind::inferred, .name = {}});
    lifetime.regions.push_back(sema::LifetimeRegion{.kind = sema::LifetimeRegionKind::local, .name = {}});
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 0U,
        .last_point = 4U,
        .point_count = 5U,
        .range = body_loan_test_range(0),
    });
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 0U,
        .last_point = 4U,
        .point_count = 5U,
        .range = body_loan_test_range(1),
    });
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 1U,
        .first_point = 1U,
        .last_point = 8U,
        .point_count = 8U,
        .range = body_loan_test_range(2),
    });
    lifetime.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = sema::SEMA_LIFETIME_INVALID_INDEX,
        .first_point = 1U,
        .last_point = 0U,
        .point_count = 0U,
        .range = body_loan_test_range(3),
    });
    checked.lifetime_facts[key] = std::move(lifetime);

    sema::SemanticAnalyzerCore::DropCheckAnalyzer(analyzer).analyze(function, key, signature);

    ASSERT_TRUE(checked.dropck_facts.contains(key));
    const sema::FunctionDropCheckFacts& facts = checked.dropck_facts.at(key);
    EXPECT_FALSE(facts.graph_missing);
    EXPECT_TRUE(facts.violations.empty());
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.kind == sema::DropCheckActionKind::explicit_drop;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.kind == sema::DropCheckActionKind::overwrite;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.kind == sema::DropCheckActionKind::defer_cleanup;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.actions, [](const sema::DropActionFact& action) {
        return action.kind == sema::DropCheckActionKind::early_exit;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts.facts, [opaque](const sema::DropCheckFact& fact) {
        return fact.type.value == opaque.value && fact.required_outlives.empty();
    }));
    ASSERT_TRUE(checked.lifetime_facts.contains(key));
    const sema::FunctionLifetimeFacts& lifetime_after = checked.lifetime_facts.at(key);
    EXPECT_TRUE(std::ranges::any_of(lifetime_after.type_outlives_constraints, [array](const auto& constraint) {
        return constraint.type.value == array.value && constraint.reason == sema::LifetimeConstraintReason::dropck;
    }));
    EXPECT_TRUE(std::ranges::any_of(lifetime_after.type_outlives_constraints, [choice](const auto& constraint) {
        return constraint.type.value == choice.value && constraint.reason == sema::LifetimeConstraintReason::dropck;
    }));
    EXPECT_TRUE(std::ranges::any_of(
        lifetime_after.type_outlives_constraints, [associated_projection](const auto& constraint) {
            return constraint.type.value == associated_projection.value
                && constraint.reason == sema::LifetimeConstraintReason::dropck;
        }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxGenericInstanceIdentityReportsCanonicalizationErrors)
{
    constexpr std::string_view SEMA_TEST_QUERY_ERROR_MODULE = "identity_errors";
    constexpr std::string_view SEMA_TEST_QUERY_ERROR_TEMPLATE = "Failing";

    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_QUERY_ERROR_MODULE})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::SemanticAnalyzerCore::GenericTemplateInfo info =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_QUERY_ERROR_TEMPLATE);
    analyzer.populate_generic_param_identities(info);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle unresolved_nominal = types.named_struct("identity_errors.Missing", "Missing", false);
    const std::array<TypeHandle, 1> unresolved_nominal_args{unresolved_nominal};
    base::Result<sema::SemanticAnalyzerCore::GenericInstanceIdentity> unresolved_nominal_identity =
        analyzer.generic_instance_identity(info, unresolved_nominal_args, query::DefNamespace::type);
    ASSERT_FALSE(unresolved_nominal_identity.has_value());
    EXPECT_NE(unresolved_nominal_identity.error().message.find("unresolved nominal type key"), std::string::npos);

    const sema::GenericParamIdentity unregistered_identity =
        sema::generic_param_identity_from_text("identity_errors.Unregistered.T");
    const TypeHandle unresolved_generic = types.generic_param(unregistered_identity, SEMA_TEST_GENERIC_PARAM_NAME);
    const std::array<TypeHandle, 1> unresolved_generic_args{unresolved_generic};
    base::Result<sema::SemanticAnalyzerCore::GenericInstanceIdentity> unresolved_generic_identity =
        analyzer.generic_instance_identity(info, unresolved_generic_args, query::DefNamespace::type);
    ASSERT_FALSE(unresolved_generic_identity.has_value());
    EXPECT_NE(unresolved_generic_identity.error().message.find("unresolved generic parameter key"), std::string::npos);

    base::Result<sema::SemanticAnalyzerCore::GenericInstanceIdentity> identity =
        analyzer.generic_instance_identity(info, std::span<const TypeHandle>{}, query::DefNamespace::value);
    ASSERT_TRUE(identity.has_value()) << identity.error().message;

    const TypeHandle unknown_type{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)};
    base::Result<std::string> bad_return_signature = analyzer.generic_instance_signature_fingerprint(
        info, identity.value(), unknown_type, std::span<const TypeHandle>{}, false, false);
    ASSERT_FALSE(bad_return_signature.has_value());
    EXPECT_NE(bad_return_signature.error().message.find("unknown type handle"), std::string::npos);

    const std::array<TypeHandle, 1> bad_param_types{unknown_type};
    base::Result<std::string> bad_param_signature =
        analyzer.generic_instance_signature_fingerprint(info, identity.value(), i32, bad_param_types, true, true);
    ASSERT_FALSE(bad_param_signature.has_value());
    EXPECT_NE(bad_param_signature.error().message.find("unknown type handle"), std::string::npos);

    base::Result<std::string> invalid_return_signature = analyzer.generic_instance_signature_fingerprint(
        info, identity.value(), INVALID_TYPE_HANDLE, std::span<const TypeHandle>{}, false, false);
    ASSERT_TRUE(invalid_return_signature.has_value()) << invalid_return_signature.error().message;
}

TEST(CoreUnit, SemanticWhiteBoxGenericTypeDisplaysAreLazy)
{
    syntax::AstModule module;
    module.modules = {module_info({"generic_type_display"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));

    syntax::TypeNode box_i32_type_node = named_node("Box");
    box_i32_type_node.type_args = {i32_type};
    const TypeId box_i32_type = module.push_type(box_i32_type_node);

    syntax::TypeNode box_generic_type_node = named_node("Box");
    box_generic_type_node.type_args = {generic_type};
    const TypeId box_generic_type = module.push_type(box_generic_type_node);

    syntax::TypeNode maybe_i32_type_node = named_node("Maybe");
    maybe_i32_type_node.type_args = {i32_type};
    const TypeId maybe_i32_type = module.push_type(maybe_i32_type_node);

    syntax::TypeNode alias_i32_type_node = named_node("AliasBox");
    alias_i32_type_node.type_args = {i32_type};
    const TypeId alias_i32_type = module.push_type(alias_i32_type_node);

    syntax::ItemNode box_item;
    box_item.kind = syntax::ItemKind::struct_decl;
    box_item.name = "Box";
    box_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    box_item.fields = {syntax::FieldDecl{"value", generic_type, {}}};
    const syntax::ItemId box_item_id = module.push_item(box_item);
    module.item_modules[box_item_id.value] = module_id(0);

    syntax::EnumCaseDecl some_case;
    some_case.name = "some";
    some_case.payload_types = {generic_type};
    syntax::EnumCaseDecl none_case;
    none_case.name = "none";
    syntax::ItemNode maybe_item;
    maybe_item.kind = syntax::ItemKind::enum_decl;
    maybe_item.name = "Maybe";
    maybe_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    maybe_item.enum_cases = {some_case, none_case};
    const syntax::ItemId maybe_item_id = module.push_item(maybe_item);
    module.item_modules[maybe_item_id.value] = module_id(0);

    syntax::ItemNode alias_item;
    alias_item.kind = syntax::ItemKind::type_alias;
    alias_item.name = "AliasBox";
    alias_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    alias_item.alias_type = box_generic_type;
    const syntax::ItemId alias_item_id = module.push_item(alias_item);
    module.item_modules[alias_item_id.value] = module_id(0);

    const ExprId zero = push_integer(module);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode use_item;
    use_item.kind = syntax::ItemKind::fn_decl;
    use_item.name = "use";
    use_item.params = {
        syntax::ParamDecl{"box", box_i32_type, {}},
        syntax::ParamDecl{"maybe", maybe_i32_type, {}},
        syntax::ParamDecl{"alias", alias_i32_type, {}},
    };
    use_item.return_type = i32_type;
    use_item.body = body;
    const syntax::ItemId use_item_id = module.push_item(use_item);
    module.item_modules[use_item_id.value] = module_id(0);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    const sema::StructInfo* generic_box = nullptr;
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        if (info.name == "Box" && !checked.types.get(info.type).generic_args.empty()) {
            generic_box = &info;
            break;
        }
    }
    ASSERT_NE(generic_box, nullptr);
    EXPECT_EQ(generic_box->name, "Box");
    EXPECT_TRUE(query::is_valid(generic_box->generic_instance_key));
    ASSERT_EQ(generic_box->generic_instance_key.type_args.size(), 1U);
    EXPECT_EQ(
        generic_box->generic_instance_key.type_args.front(), query::canonical_builtin(query::BuiltinTypeKey::i32));
    EXPECT_EQ(checked.types.get(generic_box->type).name, "generic_type_display.Box");
    EXPECT_EQ(checked.types.display_name(generic_box->type), "generic_type_display.Box[i32]");

    ASSERT_EQ(checked.generic_enum_instances.size(), 1U);
    const sema::GenericEnumInstanceInfo& generic_maybe_instance = checked.generic_enum_instances.front();
    EXPECT_TRUE(query::is_valid(generic_maybe_instance.generic_instance_key));
    ASSERT_EQ(generic_maybe_instance.generic_instance_key.type_args.size(), 1U);
    EXPECT_EQ(generic_maybe_instance.generic_instance_key.type_args.front(),
        query::canonical_builtin(query::BuiltinTypeKey::i32));
    EXPECT_EQ(checked.types.display_name(generic_maybe_instance.type), "generic_type_display.Maybe[i32]");

    const sema::EnumCaseInfo* generic_some = nullptr;
    const sema::EnumCaseInfo* generic_none = nullptr;
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        if (info.enum_name == "Maybe" && info.case_name == "some") {
            generic_some = &info;
        } else if (info.enum_name == "Maybe" && info.case_name == "none") {
            generic_none = &info;
        }
    }
    ASSERT_NE(generic_some, nullptr);
    ASSERT_NE(generic_none, nullptr);
    EXPECT_EQ(generic_some->enum_name, "Maybe");
    EXPECT_EQ(generic_some->generic_instance_key, generic_maybe_instance.generic_instance_key);
    EXPECT_EQ(generic_none->generic_instance_key, generic_maybe_instance.generic_instance_key);
    EXPECT_EQ(checked.types.display_name(generic_some->type), "generic_type_display.Maybe[i32]");
    EXPECT_EQ(generic_some->name.view().find("[i32]"), std::string::npos);

    ASSERT_EQ(checked.generic_type_alias_instances.size(), 1U);
    const sema::GenericTypeAliasInstanceInfo& alias_instance = checked.generic_type_alias_instances.front();
    EXPECT_TRUE(query::is_valid(alias_instance.generic_instance_key));
    ASSERT_EQ(alias_instance.generic_instance_key.type_args.size(), 1U);
    EXPECT_EQ(
        alias_instance.generic_instance_key.type_args.front(), query::canonical_builtin(query::BuiltinTypeKey::i32));
    EXPECT_EQ(checked.types.display_name(alias_instance.resolved_type), "generic_type_display.Box[i32]");

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("struct priv Box[i32]"), std::string::npos);
    EXPECT_NE(checked_dump.find("case Maybe[i32]_some"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxGenericAggregateInstanceSignaturesTrackResolvedShape)
{
    const std::optional<GenericAggregateSignatureSnapshot> generic_shape = generic_aggregate_signature_snapshot(
        GenericAggregateSignatureVariant::generic_param, GenericAggregateSignatureVariant::generic_param);
    ASSERT_TRUE(generic_shape.has_value());

    const std::optional<GenericAggregateSignatureSnapshot> changed_struct_shape = generic_aggregate_signature_snapshot(
        GenericAggregateSignatureVariant::concrete_i64, GenericAggregateSignatureVariant::generic_param);
    ASSERT_TRUE(changed_struct_shape.has_value());
    EXPECT_EQ(generic_shape->struct_key, changed_struct_shape->struct_key);
    EXPECT_NE(generic_shape->struct_signature, changed_struct_shape->struct_signature);
    EXPECT_EQ(generic_shape->enum_key, changed_struct_shape->enum_key);
    EXPECT_EQ(generic_shape->enum_signature, changed_struct_shape->enum_signature);

    const std::optional<GenericAggregateSignatureSnapshot> changed_enum_shape = generic_aggregate_signature_snapshot(
        GenericAggregateSignatureVariant::generic_param, GenericAggregateSignatureVariant::concrete_i64);
    ASSERT_TRUE(changed_enum_shape.has_value());
    EXPECT_EQ(generic_shape->enum_key, changed_enum_shape->enum_key);
    EXPECT_NE(generic_shape->enum_signature, changed_enum_shape->enum_signature);
    EXPECT_EQ(generic_shape->struct_key, changed_enum_shape->struct_key);
    EXPECT_EQ(generic_shape->struct_signature, changed_enum_shape->struct_signature);
}

TEST(CoreUnit, SemanticWhiteBoxGenericAggregateSignatureFailuresReportInternalContract)
{
    {
        syntax::AstModule module;
        module.modules = {module_info({"generic_struct_signature_failure"})};
        const TypeId external_type = module.push_type(named_node("External"));
        syntax::ItemNode box_item;
        box_item.kind = syntax::ItemKind::struct_decl;
        box_item.name = "Box";
        box_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
        box_item.fields = {syntax::FieldDecl{"value", external_type, {}}};
        const syntax::ItemId box_item_id = module.push_item(box_item);
        module.item_modules[box_item_id.value] = module_id(0);

        base::DiagnosticSink diagnostics;
        sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
        const TypeHandle external_handle =
            analyzer.state_.checked.types.named_struct("generic_struct_signature_failure.External", "External", false);
        static_cast<void>(add_named_type(analyzer, module_id(0), "External", external_handle));

        sema::SemanticAnalyzerCore::GenericTemplateInfo box_info = generic_template_info(analyzer, module_id(0), "Box");
        box_info.item = box_item_id;
        box_info.stable_id = analyzer.stable_definition_id(
            module_id(0), sema::StableSymbolKind::generic_template, box_info.name_id, box_info.name);
        analyzer.populate_generic_param_identities(box_info);

        syntax::TypeNode use_type = named_node("Box");
        const std::vector<TypeHandle> args{analyzer.state_.checked.types.builtin(BuiltinType::i32)};
        sema::SemanticAnalyzerCore::GenericAnalyzer generic(analyzer);
        EXPECT_FALSE(is_valid(generic.instantiate_generic_struct(box_info, use_type, syntax::INVALID_TYPE_ID, args)));
        EXPECT_NE(
            diagnostic_messages(diagnostics).find("generic struct instance signature fingerprint"), std::string::npos);
    }

    {
        syntax::AstModule module;
        module.modules = {module_info({"generic_enum_signature_failure"})};
        const TypeId external_type = module.push_type(named_node("External"));
        syntax::EnumCaseDecl some_case;
        some_case.name = "some";
        some_case.payload_types = {external_type};
        syntax::ItemNode maybe_item;
        maybe_item.kind = syntax::ItemKind::enum_decl;
        maybe_item.name = "Maybe";
        maybe_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
        maybe_item.enum_cases = {some_case};
        const syntax::ItemId maybe_item_id = module.push_item(maybe_item);
        module.item_modules[maybe_item_id.value] = module_id(0);

        base::DiagnosticSink diagnostics;
        sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
        const TypeHandle external_handle =
            analyzer.state_.checked.types.named_struct("generic_enum_signature_failure.External", "External", false);
        static_cast<void>(add_named_type(analyzer, module_id(0), "External", external_handle));

        sema::SemanticAnalyzerCore::GenericTemplateInfo maybe_info =
            generic_template_info(analyzer, module_id(0), "Maybe");
        maybe_info.item = maybe_item_id;
        maybe_info.stable_id = analyzer.stable_definition_id(
            module_id(0), sema::StableSymbolKind::generic_template, maybe_info.name_id, maybe_info.name);
        analyzer.populate_generic_param_identities(maybe_info);

        syntax::TypeNode use_type = named_node("Maybe");
        const std::vector<TypeHandle> args{analyzer.state_.checked.types.builtin(BuiltinType::i32)};
        sema::SemanticAnalyzerCore::GenericAnalyzer generic(analyzer);
        EXPECT_FALSE(is_valid(generic.instantiate_generic_enum(maybe_info, use_type, syntax::INVALID_TYPE_ID, args)));
        EXPECT_NE(
            diagnostic_messages(diagnostics).find("generic enum instance signature fingerprint"), std::string::npos);
    }
}

TEST(CoreUnit, SemanticWhiteBoxRecordSideTableDenseAndSparseEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const ExprId expr_id = push_integer(module);
    const ExprId missing_expr_id{expr_id.value + 100U};

    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::binding;
    pattern.binding_name = "value";
    const syntax::PatternId pattern_id = module.push_pattern(pattern);
    const syntax::PatternId alternative_pattern_id = module.push_pattern(pattern);
    const syntax::PatternId missing_pattern_id{alternative_pattern_id.value + 100U};

    const TypeId type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId missing_type_id{type_id.value + 100U};

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::expr;
    stmt.init = expr_id;
    const syntax::StmtId stmt_id = module.push_stmt(stmt);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);

    sema::GenericSideTables dense_side_tables;
    analyzer.state_.flow.current_side_tables.side_tables = &dense_side_tables;
    static_cast<void>(analyzer.record_expr_intrinsic_type(expr_id, i32));
    static_cast<void>(analyzer.record_expr_type(expr_id, i32));
    analyzer.record_expr_expected_type(expr_id, i64);
    analyzer.record_expr_owned_use_mode(expr_id, sema::OwnedUseMode::owned_copy);
    analyzer.record_expr_c_name(expr_id, "dense_expr");
    analyzer.record_pattern_c_name(pattern_id, "dense_pattern");
    analyzer.record_pattern_case_name(pattern_id, "DenseCase");
    analyzer.record_pattern_case_name(alternative_pattern_id, "DenseAlternative");
    analyzer.merge_pattern_case_names(pattern_id, alternative_pattern_id);
    analyzer.record_syntax_type_handle(type_id, i32);
    analyzer.record_stmt_local_type(stmt_id, i64);

    EXPECT_TRUE(types.same(analyzer.cached_expr_intrinsic_type(expr_id), i32));
    EXPECT_TRUE(types.same(analyzer.cached_expr_type(expr_id), i32));
    EXPECT_TRUE(types.same(analyzer.cached_expr_expected_type(expr_id), i64));
    EXPECT_EQ(analyzer.cached_expr_owned_use_mode(expr_id), sema::OwnedUseMode::owned_copy);
    EXPECT_TRUE(types.same(analyzer.cached_syntax_type(type_id), i32));
    EXPECT_EQ(analyzer.cached_expr_c_name(expr_id), "dense_expr");
    EXPECT_EQ(analyzer.cached_pattern_c_name(pattern_id), "dense_pattern");
    ASSERT_TRUE(dense_side_tables.pattern_case_name_ids.contains(pattern_id.value));
    EXPECT_TRUE(dense_side_tables.pattern_case_name_ids[pattern_id.value].contains(
        analyzer.state_.checked.intern_c_name("DenseAlternative")));
    EXPECT_TRUE(types.same(dense_side_tables.stmt_local_types[stmt_id.value], i64));
    EXPECT_TRUE(types.same(analyzer.cached_stmt_local_type(stmt_id), i64));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_intrinsic_type(missing_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_type(missing_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_expected_type(missing_expr_id)));
    EXPECT_EQ(analyzer.cached_expr_owned_use_mode(missing_expr_id), sema::OwnedUseMode::none);
    EXPECT_FALSE(is_valid(analyzer.cached_stmt_local_type(syntax::INVALID_STMT_ID)));
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(missing_type_id)));
    EXPECT_TRUE(analyzer.cached_expr_c_name(missing_expr_id).empty());
    EXPECT_TRUE(analyzer.cached_pattern_c_name(missing_pattern_id).empty());
    EXPECT_EQ(&analyzer.active_expr_intrinsic_types(), &dense_side_tables.expr_intrinsic_types);
    EXPECT_EQ(&analyzer.active_expr_types(), &dense_side_tables.expr_types);
    EXPECT_EQ(&analyzer.active_expr_expected_types(), &dense_side_tables.expr_expected_types);
    EXPECT_EQ(&analyzer.active_expr_owned_use_modes(), &dense_side_tables.expr_owned_use_modes);
    EXPECT_EQ(&analyzer.active_expr_c_name_ids(), &dense_side_tables.expr_c_name_ids);
    EXPECT_EQ(&analyzer.active_pattern_c_name_ids(), &dense_side_tables.pattern_c_name_ids);
    EXPECT_EQ(&analyzer.active_pattern_case_name_ids(), &dense_side_tables.pattern_case_name_ids);
    EXPECT_EQ(&analyzer.active_syntax_type_handles(), &dense_side_tables.syntax_type_handles);
    EXPECT_EQ(&analyzer.active_stmt_local_types(), &dense_side_tables.stmt_local_types);
    EXPECT_GT(dense_side_tables.arena_bytes(), 0U);
    EXPECT_GT(dense_side_tables.arena_blocks(), 0U);
    EXPECT_GT(dense_side_tables.pattern_case_name_ids.arena_bytes(), 0U);

    sema::GenericSideTables sparse_side_tables;
    sparse_side_tables.sparse = true;
    analyzer.state_.flow.current_side_tables.side_tables = &sparse_side_tables;
    analyzer.state_.flow.current_side_tables.cache_syntax_types = true;
    static_cast<void>(analyzer.record_expr_intrinsic_type(expr_id, i64));
    static_cast<void>(analyzer.record_expr_type(expr_id, i64));
    static_cast<void>(analyzer.record_expr_type(syntax::INVALID_EXPR_ID, i64));
    analyzer.record_expr_expected_type(expr_id, i32);
    analyzer.record_expr_owned_use_mode(expr_id, sema::OwnedUseMode::owned_consume);
    analyzer.record_expr_owned_use_mode(syntax::INVALID_EXPR_ID, sema::OwnedUseMode::owned_copy);
    analyzer.record_expr_expected_type(syntax::INVALID_EXPR_ID, i32);
    analyzer.record_expr_c_name(expr_id, "sparse_expr");
    analyzer.record_pattern_c_name(pattern_id, "sparse_pattern");
    analyzer.record_pattern_case_name(alternative_pattern_id, "SparseAlternative");
    analyzer.merge_pattern_case_names(pattern_id, alternative_pattern_id);
    analyzer.record_syntax_type_handle(type_id, i64);
    analyzer.record_syntax_type_handle(syntax::INVALID_TYPE_ID, i32);
    analyzer.record_stmt_local_type(stmt_id, i32);

    EXPECT_TRUE(types.same(analyzer.cached_expr_intrinsic_type(expr_id), i64));
    EXPECT_TRUE(types.same(analyzer.cached_expr_type(expr_id), i64));
    EXPECT_TRUE(types.same(analyzer.cached_expr_expected_type(expr_id), i32));
    EXPECT_EQ(analyzer.cached_expr_owned_use_mode(expr_id), sema::OwnedUseMode::owned_consume);
    EXPECT_TRUE(types.same(analyzer.cached_syntax_type(type_id), i64));
    EXPECT_EQ(analyzer.cached_expr_c_name(expr_id), "sparse_expr");
    EXPECT_EQ(analyzer.cached_pattern_c_name(pattern_id), "sparse_pattern");
    ASSERT_TRUE(sparse_side_tables.pattern_case_name_ids.contains(pattern_id.value));
    EXPECT_TRUE(sparse_side_tables.pattern_case_name_ids[pattern_id.value].contains(
        analyzer.state_.checked.intern_c_name("SparseAlternative")));
    EXPECT_TRUE(types.same(sparse_side_tables.sparse_stmt_local_types[stmt_id.value], i32));
    EXPECT_TRUE(types.same(analyzer.cached_stmt_local_type(stmt_id), i32));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_intrinsic_type(syntax::INVALID_EXPR_ID)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_type(syntax::INVALID_EXPR_ID)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_expected_type(syntax::INVALID_EXPR_ID)));
    EXPECT_EQ(analyzer.cached_expr_owned_use_mode(syntax::INVALID_EXPR_ID), sema::OwnedUseMode::none);
    EXPECT_FALSE(is_valid(analyzer.cached_expr_intrinsic_type(missing_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_type(missing_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_expected_type(missing_expr_id)));
    EXPECT_EQ(analyzer.cached_expr_owned_use_mode(missing_expr_id), sema::OwnedUseMode::none);
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(syntax::INVALID_TYPE_ID)));
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(missing_type_id)));
    EXPECT_TRUE(analyzer.cached_expr_c_name(syntax::INVALID_EXPR_ID).empty());
    EXPECT_TRUE(analyzer.cached_expr_c_name(missing_expr_id).empty());
    EXPECT_TRUE(analyzer.cached_pattern_c_name(syntax::INVALID_PATTERN_ID).empty());
    EXPECT_TRUE(analyzer.cached_pattern_c_name(missing_pattern_id).empty());
    EXPECT_GT(sparse_side_tables.arena_bytes(), 0U);
    EXPECT_GT(sparse_side_tables.arena_blocks(), 0U);
    EXPECT_GT(sparse_side_tables.pattern_case_name_ids.arena_bytes(), 0U);

    sema::GenericSideTables local_side_tables;
    local_side_tables.configure_local_dense(sema::GenericNodeSpan{expr_id.value, 1U},
        sema::GenericNodeSpan{pattern_id.value, 1U}, sema::GenericNodeSpan{type_id.value, 1U},
        sema::GenericNodeSpan{stmt_id.value, 1U});
    analyzer.state_.flow.current_side_tables.side_tables = &local_side_tables;
    analyzer.state_.flow.current_side_tables.cache_syntax_types = true;
    static_cast<void>(analyzer.record_expr_intrinsic_type(expr_id, i32));
    static_cast<void>(analyzer.record_expr_type(expr_id, i32));
    analyzer.record_expr_expected_type(expr_id, i64);
    analyzer.record_expr_owned_use_mode(expr_id, sema::OwnedUseMode::shared_borrow);
    analyzer.record_expr_c_name(expr_id, "local_expr");
    analyzer.record_pattern_c_name(pattern_id, "local_pattern");
    analyzer.record_syntax_type_handle(type_id, i32);
    analyzer.record_stmt_local_type(stmt_id, i64);

    const base::usize local_expr = local_side_tables.local_expr_index(expr_id);
    const base::usize local_pattern = local_side_tables.local_pattern_index(pattern_id);
    const base::usize local_type = local_side_tables.local_type_index(type_id);
    const base::usize local_stmt = local_side_tables.local_stmt_index(stmt_id);
    ASSERT_NE(local_expr, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    ASSERT_NE(local_pattern, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    ASSERT_NE(local_type, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    ASSERT_NE(local_stmt, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_TRUE(types.same(local_side_tables.expr_intrinsic_types[local_expr], i32));
    EXPECT_TRUE(types.same(local_side_tables.expr_types[local_expr], i32));
    EXPECT_TRUE(types.same(local_side_tables.expr_expected_types[local_expr], i64));
    EXPECT_TRUE(local_side_tables.expr_owned_use_modes.empty());
    EXPECT_EQ(local_side_tables.sparse_expr_owned_use_modes.at(expr_id.value), sema::OwnedUseMode::shared_borrow);
    EXPECT_EQ(analyzer.cached_expr_owned_use_mode(expr_id), sema::OwnedUseMode::shared_borrow);
    EXPECT_EQ(analyzer.state_.checked.c_name_text(local_side_tables.expr_c_name_ids[local_expr]), "local_expr");
    EXPECT_EQ(
        analyzer.state_.checked.c_name_text(local_side_tables.pattern_c_name_ids[local_pattern]), "local_pattern");
    EXPECT_TRUE(types.same(local_side_tables.syntax_type_handles[local_type], i32));
    EXPECT_TRUE(types.same(local_side_tables.stmt_local_types[local_stmt], i64));
    EXPECT_TRUE(local_side_tables.sparse_expr_intrinsic_types.empty());
    EXPECT_TRUE(local_side_tables.sparse_expr_types.empty());
    EXPECT_TRUE(local_side_tables.sparse_expr_expected_types.empty());
    EXPECT_TRUE(local_side_tables.sparse_expr_c_name_ids.empty());
    EXPECT_TRUE(local_side_tables.sparse_pattern_c_name_ids.empty());
    EXPECT_TRUE(local_side_tables.sparse_syntax_type_handles.empty());
    EXPECT_TRUE(local_side_tables.sparse_stmt_local_types.empty());
    EXPECT_EQ(local_side_tables.sparse_fallbacks.expr_owned_use_modes, 0U);
    EXPECT_EQ(local_side_tables.sparse_fallbacks.total(), 0U);

    sema::GenericSideTables sparse_local_side_tables;
    constexpr base::u32 SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN = 10U;
    constexpr base::u32 SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT = 91U;
    const std::array<base::u32, 2> sparse_expr_ids{
        SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN,
        SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN + SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT - 1U,
    };
    const std::array<base::u32, 2> sparse_pattern_ids = sparse_expr_ids;
    const std::array<base::u32, 2> sparse_type_ids = sparse_expr_ids;
    const std::array<base::u32, 2> sparse_stmt_ids = sparse_expr_ids;
    sparse_local_side_tables.configure_local_dense(sema::GenericSideTableLocalLayoutView{
        sema::GenericNodeSpan{SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN, SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT},
        sema::GenericNodeSpan{SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN, SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT},
        sema::GenericNodeSpan{SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN, SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT},
        sema::GenericNodeSpan{SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN, SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT},
        sparse_expr_ids,
        sparse_pattern_ids,
        sparse_type_ids,
        sparse_stmt_ids,
    });
    EXPECT_EQ(sparse_local_side_tables.expr_intrinsic_types.size(), sparse_expr_ids.size());
    EXPECT_EQ(sparse_local_side_tables.expr_types.size(), sparse_expr_ids.size());
    EXPECT_EQ(sparse_local_side_tables.pattern_c_name_ids.size(), sparse_pattern_ids.size());
    EXPECT_EQ(sparse_local_side_tables.syntax_type_handles.size(), sparse_type_ids.size());
    EXPECT_EQ(sparse_local_side_tables.stmt_local_types.size(), sparse_stmt_ids.size());
    EXPECT_EQ(sparse_local_side_tables.local_expr_index(ExprId{sparse_expr_ids.front()}), 0U);
    EXPECT_EQ(sparse_local_side_tables.local_expr_index(ExprId{sparse_expr_ids.back()}), 1U);
    EXPECT_EQ(sparse_local_side_tables.local_expr_index(ExprId{sparse_expr_ids.front() + 1U}),
        sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    analyzer.state_.flow.current_side_tables.side_tables = &sparse_local_side_tables;
    analyzer.state_.flow.current_side_tables.cache_syntax_types = true;
    static_cast<void>(analyzer.record_expr_intrinsic_type(ExprId{sparse_expr_ids.front() + 1U}, i32));
    static_cast<void>(analyzer.record_expr_type(ExprId{sparse_expr_ids.front() + 1U}, i32));
    analyzer.record_expr_expected_type(ExprId{sparse_expr_ids.front() + 1U}, i64);
    analyzer.record_expr_owned_use_mode(ExprId{sparse_expr_ids.front() + 1U}, sema::OwnedUseMode::mutable_borrow);
    analyzer.record_expr_c_name(ExprId{sparse_expr_ids.front() + 1U}, "sparse_local_expr");
    analyzer.record_pattern_c_name(syntax::PatternId{sparse_pattern_ids.front() + 1U}, "sparse_local_pattern");
    analyzer.record_pattern_case_name(syntax::PatternId{sparse_pattern_ids.front()}, "SparseLocalAlternative");
    analyzer.record_pattern_case_name(syntax::PatternId{sparse_pattern_ids.front() + 1U}, "SparseLocalCase");
    analyzer.merge_pattern_case_names(
        syntax::PatternId{sparse_pattern_ids.front() + 1U}, syntax::PatternId{sparse_pattern_ids.front()});
    analyzer.merge_pattern_case_names(
        syntax::PatternId{sparse_pattern_ids.front()}, syntax::PatternId{sparse_pattern_ids.front() + 1U});
    analyzer.record_syntax_type_handle(TypeId{sparse_type_ids.front() + 1U}, i32);
    analyzer.record_stmt_local_type(syntax::StmtId{sparse_stmt_ids.front() + 1U}, i64);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.expr_intrinsic_types, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.expr_types, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.expr_expected_types, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.expr_owned_use_modes, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.expr_c_name_ids, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.pattern_c_name_ids, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.pattern_case_name_ids, 3U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.syntax_type_handles, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.stmt_local_types, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.total(), 11U);
    EXPECT_EQ(
        analyzer.cached_expr_owned_use_mode(ExprId{sparse_expr_ids.front() + 1U}), sema::OwnedUseMode::mutable_borrow);
    EXPECT_EQ(sema::owned_use_mode_name(sema::OwnedUseMode::none), "none");
    EXPECT_EQ(sema::owned_use_mode_name(sema::OwnedUseMode::owned_copy), "owned_copy");
    EXPECT_EQ(sema::owned_use_mode_name(sema::OwnedUseMode::owned_consume), "owned_consume");
    EXPECT_EQ(sema::owned_use_mode_name(sema::OwnedUseMode::shared_borrow), "shared_borrow");
    EXPECT_EQ(sema::owned_use_mode_name(sema::OwnedUseMode::mutable_borrow), "mutable_borrow");
    EXPECT_EQ(sema::owned_use_mode_name(sema::OwnedUseMode::place_only), "place_only");
    EXPECT_EQ(sema::owned_use_mode_name(static_cast<sema::OwnedUseMode>(99)), "<invalid>");
    EXPECT_EQ(sema::receiver_access_kind_name(sema::ReceiverAccessKind::none), "none");
    EXPECT_EQ(sema::receiver_access_kind_name(sema::ReceiverAccessKind::shared), "shared");
    EXPECT_EQ(sema::receiver_access_kind_name(sema::ReceiverAccessKind::mutable_), "mutable");
    EXPECT_EQ(sema::receiver_access_kind_name(sema::ReceiverAccessKind::consuming), "consuming");
    EXPECT_EQ(sema::receiver_access_kind_name(static_cast<sema::ReceiverAccessKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
}

TEST(CoreUnit, SemanticWhiteBoxBodyMoveAnalysisRecordsOwnedUseModes)
{
    syntax::AstModule module;
    module.modules = {module_info({"owned_use_modes"})};
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId returned_value = push_name(module, "value");

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = returned_value;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode identity;
    identity.kind = syntax::ItemKind::fn_decl;
    identity.name = "identity";
    identity.params = {syntax::ParamDecl{"value", i32_type, {}}};
    identity.return_type = i32_type;
    identity.body = body;
    const syntax::ItemId identity_id = module.push_item(identity);
    module.item_modules[identity_id.value] = module_id(0);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    const auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();
    ASSERT_LT(returned_value.value, checked.expr_owned_use_modes.size());
    EXPECT_EQ(checked.expr_owned_use_modes[returned_value.value], sema::OwnedUseMode::owned_copy);
}

TEST(CoreUnit, SemanticWhiteBoxArenaBackedSemaStorageCopiesAndMoves)
{
    const IdentId alpha_id{1};
    const IdentId beta_id{2};
    const TypeHandle i32{static_cast<base::u32>(BuiltinType::i32)};
    const TypeHandle i64{static_cast<base::u32>(BuiltinType::i64)};

    sema::PatternCaseNameTable empty_pattern_names;
    const sema::PatternCaseNameTable& const_empty_pattern_names = empty_pattern_names;
    EXPECT_EQ(empty_pattern_names.begin(), empty_pattern_names.end());
    EXPECT_EQ(const_empty_pattern_names.find(0), const_empty_pattern_names.end());
    EXPECT_EQ(empty_pattern_names.arena_blocks(), 0U);
    empty_pattern_names.reserve(0);
    sema::PatternCaseNameTable source_empty_pattern_names;
    const sema::CNameIdSet& empty_pattern_bucket = source_empty_pattern_names[0];
    empty_pattern_names.merge(0, empty_pattern_bucket);
    EXPECT_FALSE(empty_pattern_names.contains(0));

    sema::PatternCaseNameTable pattern_names;
    pattern_names.reserve(2);
    pattern_names.insert(10, alpha_id);
    pattern_names.insert(20, beta_id);
    pattern_names.merge(10, pattern_names[20]);
    sema::PatternCaseNameTable* const pattern_self = &pattern_names;
    pattern_names = *pattern_self;
    pattern_names = std::move(*pattern_self);
    ASSERT_TRUE(pattern_names.contains(10));
    EXPECT_NE(pattern_names.find(10), pattern_names.end());
    EXPECT_TRUE(pattern_names[10].contains(alpha_id));
    EXPECT_TRUE(pattern_names[10].contains(beta_id));

    sema::PatternCaseNameTable pattern_copy(pattern_names);
    EXPECT_TRUE(pattern_copy[10].contains(beta_id));
    sema::PatternCaseNameTable pattern_assigned;
    pattern_assigned = pattern_names;
    EXPECT_TRUE(pattern_assigned[20].contains(beta_id));
    sema::PatternCaseNameTable pattern_moved(std::move(pattern_copy));
    EXPECT_TRUE(pattern_moved[10].contains(alpha_id));
    sema::PatternCaseNameTable pattern_move_assigned;
    pattern_move_assigned = std::move(pattern_assigned);
    EXPECT_TRUE(pattern_move_assigned[20].contains(beta_id));
    pattern_move_assigned.clear();
    EXPECT_TRUE(pattern_move_assigned.empty());

    sema::GenericSideTables side_tables;
    side_tables.sparse = true;
    side_tables.expr_intrinsic_types.push_back(i32);
    side_tables.expr_types.push_back(i32);
    side_tables.prepare_analysis_only_storage(1U);
    side_tables.expr_expected_types.push_back(i64);
    side_tables.expr_owned_use_modes.push_back(sema::OwnedUseMode::owned_consume);
    side_tables.expr_c_name_ids.push_back(alpha_id);
    side_tables.pattern_c_name_ids.push_back(beta_id);
    side_tables.syntax_type_handles.push_back(i32);
    side_tables.stmt_local_types.push_back(i64);
    side_tables.sparse_expr_intrinsic_types.emplace(3U, i32);
    side_tables.sparse_expr_types.emplace(4U, i32);
    side_tables.sparse_expr_expected_types.emplace(5U, i64);
    side_tables.sparse_expr_owned_use_modes.emplace(5U, sema::OwnedUseMode::shared_borrow);
    side_tables.sparse_expr_c_name_ids.emplace(6U, alpha_id);
    side_tables.sparse_pattern_c_name_ids.emplace(7U, beta_id);
    side_tables.sparse_syntax_type_handles.emplace(8U, i32);
    side_tables.sparse_stmt_local_types.emplace(9U, i64);
    side_tables.pattern_case_name_ids.insert(10, alpha_id);
    side_tables.record_sparse_fallback(sema::GenericSparseFallbackKind::expr_intrinsic_type);
    side_tables.record_sparse_fallback(sema::GenericSparseFallbackKind::expr_type);
    side_tables.record_sparse_fallback(sema::GenericSparseFallbackKind::expr_owned_use_mode);
    side_tables.record_sparse_fallback(sema::GenericSparseFallbackKind::stmt_local_type);
    sema::GenericSideTables* const side_self = &side_tables;
    side_tables = *side_self;
    side_tables = std::move(*side_self);
    side_tables.release_analysis_only_storage();
    EXPECT_TRUE(side_tables.expr_expected_types.empty());
    EXPECT_TRUE(side_tables.sparse_expr_expected_types.empty());
    EXPECT_TRUE(side_tables.pattern_case_name_ids.empty());
    side_tables.prepare_analysis_only_storage(1U);
    side_tables.expr_expected_types.push_back(i64);
    side_tables.sparse_expr_expected_types.emplace(5U, i64);
    side_tables.pattern_case_name_ids.insert(10, alpha_id);

    sema::GenericSideTables side_copy(side_tables);
    EXPECT_TRUE(side_copy.sparse);
    EXPECT_EQ(side_copy.expr_intrinsic_types.front().value, i32.value);
    EXPECT_EQ(side_copy.expr_types.front().value, i32.value);
    EXPECT_EQ(side_copy.sparse_expr_intrinsic_types.at(3U).value, i32.value);
    EXPECT_EQ(side_copy.sparse_expr_c_name_ids.at(6U).value, alpha_id.value);
    EXPECT_EQ(side_copy.expr_owned_use_modes.front(), sema::OwnedUseMode::owned_consume);
    EXPECT_EQ(side_copy.sparse_expr_owned_use_modes.at(5U), sema::OwnedUseMode::shared_borrow);
    EXPECT_EQ(side_copy.sparse_fallbacks.total(), 4U);
    sema::GenericSideTables side_assigned;
    side_assigned = side_tables;
    EXPECT_EQ(side_assigned.sparse_stmt_local_types.at(9U).value, i64.value);
    EXPECT_EQ(side_assigned.sparse_fallbacks.expr_intrinsic_types, 1U);
    EXPECT_EQ(side_assigned.sparse_fallbacks.expr_types, 1U);
    EXPECT_EQ(side_assigned.sparse_fallbacks.expr_owned_use_modes, 1U);
    sema::GenericSideTables side_moved(std::move(side_copy));
    EXPECT_TRUE(side_moved.pattern_case_name_ids[10].contains(alpha_id));
    EXPECT_EQ(side_moved.sparse_fallbacks.stmt_local_types, 1U);
    sema::GenericSideTables side_move_assigned;
    side_move_assigned = std::move(side_assigned);
    EXPECT_EQ(side_move_assigned.syntax_type_handles.front().value, i32.value);
    EXPECT_EQ(side_move_assigned.sparse_fallbacks.total(), 4U);

    sema::CheckedModule checked;
    constexpr base::u32 SEMA_TEST_CHECKED_COPY_PART_INDEX = 4;
    const IdentId checked_c_name = checked.intern_c_name("m0_test");
    const std::array<std::string_view, 1> checked_module_parts{"checked_copy"};
    const sema::StableModuleId checked_stable_module = sema::stable_module_id(checked_module_parts);
    const sema::StableDefId checked_template_stable_id =
        sema::stable_definition_id(checked_stable_module, sema::StableSymbolKind::generic_template, "CopyGeneric");
    const query::DefKey checked_template_key =
        query::def_key_from_stable_id(query::package_key(std::span<const std::string_view>{}),
            checked_template_stable_id, query::DefNamespace::value, query::DefKind::generic_template);
    const std::array<query::CanonicalTypeKey, 1> checked_generic_args{
        query::canonical_builtin(query::BuiltinTypeKey::i32),
    };
    const query::GenericInstanceKey checked_generic_instance_key = query::generic_instance_key(checked_template_key,
        checked_generic_args, std::span<const query::StableFingerprint128>{}, query::param_env_key({}));
    checked.expr_intrinsic_types.push_back(i32);
    checked.expr_types.push_back(i32);
    checked.prepare_analysis_only_storage(1U);
    checked.expr_expected_types.push_back(i64);
    checked.expr_owned_use_modes.push_back(sema::OwnedUseMode::owned_copy);
    checked.expr_c_name_ids.push_back(checked_c_name);
    checked.pattern_c_name_ids.push_back(checked_c_name);
    checked.pattern_case_name_ids.insert(3, checked_c_name);
    checked.syntax_type_handles.push_back(i32);
    checked.stmt_local_types.push_back(i64);
    checked.item_c_name_ids.push_back(checked_c_name);
    checked.coercions.push_back(sema::CoercionRecord{
        ExprId{0},
        i32,
        i64,
        sema::CoercionKind::contextual_integer_literal,
    });

    FunctionSignature signature = checked.make_function_signature();
    signature.name = checked.intern_text("f");
    signature.name_id = alpha_id;
    signature.semantic_key = sema::FunctionLookupKey{
        module_id(0).value,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        alpha_id,
    };
    signature.c_name = checked.intern_text("m0_f");
    signature.generic_instance_key = checked_generic_instance_key;
    signature.param_types.push_back(i32);
    signature.generic_args.push_back(i64);
    signature.return_type = i32;
    signature.part_index = SEMA_TEST_CHECKED_COPY_PART_INDEX;
    checked.functions.emplace(signature.semantic_key, signature);
    StructInfo struct_info = checked.make_struct_info();
    struct_info.name = checked.intern_text("S");
    struct_info.name_id = alpha_id;
    struct_info.c_name = checked.intern_text("m0_S");
    struct_info.module = module_id(0);
    struct_info.type = i32;
    struct_info.part_index = SEMA_TEST_CHECKED_COPY_PART_INDEX;
    struct_info.generic_instance_key = checked_generic_instance_key;
    struct_info.fields.push_back(StructFieldInfo{
        checked.intern_text("field"),
        beta_id,
        checked.intern_text("m0_S_field"),
        module_id(0),
        i32,
        {},
        syntax::Visibility::public_,
        {},
    });
    const sema::ModuleLookupKey struct_key{module_id(0).value, alpha_id};
    checked.structs.emplace(struct_key, struct_info);
    EnumCaseInfo enum_case = checked.make_enum_case_info();
    enum_case.name = checked.intern_text("E_case");
    enum_case.name_id = beta_id;
    enum_case.case_name = checked.intern_text("case");
    enum_case.case_name_id = beta_id;
    enum_case.c_name = checked.intern_text("m0_E_case");
    enum_case.type = i64;
    enum_case.part_index = SEMA_TEST_CHECKED_COPY_PART_INDEX;
    enum_case.generic_instance_key = checked_generic_instance_key;
    enum_case.payload_types.push_back(i32);
    const sema::ModuleLookupKey enum_case_key{module_id(0).value, beta_id};
    checked.enum_cases.emplace(enum_case_key, enum_case);
    sema::TypeAliasInfo alias_info;
    alias_info.name = checked.intern_text("Alias");
    alias_info.name_id = alpha_id;
    alias_info.module = module_id(0);
    alias_info.target = TypeId{0};
    alias_info.part_index = SEMA_TEST_CHECKED_COPY_PART_INDEX;
    const sema::ModuleLookupKey alias_key{module_id(0).value, alpha_id};
    checked.type_aliases.emplace(alias_key, alias_info);

    sema::GenericTemplateSignatureInfo template_info;
    template_info.name = checked.intern_text("G");
    template_info.name_id = alpha_id;
    template_info.part_index = SEMA_TEST_CHECKED_COPY_PART_INDEX;
    checked.generic_template_signatures.push_back(template_info);

    sema::GenericEnumInstanceInfo enum_instance;
    enum_instance.key = enum_case_key;
    enum_instance.item = syntax::ItemId{0};
    enum_instance.generic_instance_key = checked_generic_instance_key;
    enum_instance.type = i64;
    enum_instance.stable_id = enum_case.stable_id;
    enum_instance.incremental_key = enum_case.incremental_key;
    enum_instance.part_index = SEMA_TEST_CHECKED_COPY_PART_INDEX;
    checked.generic_enum_instances.push_back(enum_instance);

    sema::GenericTypeAliasInstanceInfo alias_instance;
    alias_instance.key = alias_key;
    alias_instance.item = syntax::ItemId{0};
    alias_instance.generic_instance_key = checked_generic_instance_key;
    alias_instance.resolved_type = i32;
    alias_instance.stable_id = alias_info.stable_id;
    alias_instance.incremental_key = alias_info.incremental_key;
    alias_instance.part_index = SEMA_TEST_CHECKED_COPY_PART_INDEX;
    checked.generic_type_alias_instances.push_back(alias_instance);

    sema::GenericFunctionInstanceInfo instance;
    instance.key = sema::FunctionLookupKey{
        module_id(0).value,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        checked.intern_c_name("f[i32]"),
    };
    instance.item = syntax::ItemId{0};
    instance.body = syntax::StmtId{2};
    instance.generic_instance_key = checked_generic_instance_key;
    instance.signature = checked.clone_function_signature(signature);
    instance.side_table_layout_index = checked.append_generic_side_table_layout(sema::GenericSideTableLocalLayoutView{
        sema::GenericNodeSpan{4U, 3U},
        {},
        {},
        {},
    });
    ASSERT_EQ(instance.side_table_layout_index, 0U);
    const sema::GenericSideTableLayout* const instance_layout =
        checked.generic_side_table_layout(instance.side_table_layout_index);
    ASSERT_NE(instance_layout, nullptr);
    instance.side_tables.configure_local_dense(*instance_layout);
    instance.side_tables.expr_intrinsic_types.front() = i32;
    instance.side_tables.expr_types.front() = i32;
    instance.side_tables.expr_expected_types.front() = i64;
    checked.generic_function_instances.push_back(std::move(instance));
    sema::CheckedModule* const checked_self = &checked;
    checked = *checked_self;
    checked = std::move(*checked_self);

    sema::CheckedModule checked_copy(checked);
    ASSERT_EQ(checked_copy.functions.size(), 1U);
    EXPECT_EQ(checked_copy.expr_intrinsic_types.front().value, i32.value);
    EXPECT_EQ(checked_copy.c_name_text(checked_copy.expr_c_name_ids.front()), "m0_test");
    EXPECT_EQ(checked_copy.functions.at(signature.semantic_key).generic_instance_key, checked_generic_instance_key);
    EXPECT_EQ(checked_copy.generic_function_instances.front().signature.name, "f");
    EXPECT_EQ(checked_copy.generic_function_instances.front().body.value, 2U);
    EXPECT_EQ(checked_copy.generic_function_instances.front().generic_instance_key, checked_generic_instance_key);
    EXPECT_EQ(
        checked_copy.generic_function_instances.front().signature.generic_instance_key, checked_generic_instance_key);
    EXPECT_EQ(checked_copy.generic_function_instances.front().signature.part_index, SEMA_TEST_CHECKED_COPY_PART_INDEX);
    ASSERT_EQ(checked_copy.generic_template_signatures.size(), 1U);
    EXPECT_EQ(checked_copy.generic_template_signatures.front().part_index, SEMA_TEST_CHECKED_COPY_PART_INDEX);
    EXPECT_EQ(checked_copy.generic_side_table_layouts.size(), 1U);
    ASSERT_EQ(checked_copy.generic_enum_instances.size(), 1U);
    EXPECT_EQ(checked_copy.generic_enum_instances.front().generic_instance_key, checked_generic_instance_key);
    ASSERT_EQ(checked_copy.generic_type_alias_instances.size(), 1U);
    EXPECT_EQ(checked_copy.generic_type_alias_instances.front().generic_instance_key, checked_generic_instance_key);
    EXPECT_EQ(checked_copy.generic_function_instances.front().side_tables.layout,
        &checked_copy.generic_side_table_layouts.front());
    EXPECT_EQ(checked_copy.generic_function_instances.front().side_tables.local_expr_index(ExprId{4U}), 0U);
    checked.release_analysis_only_storage();
    EXPECT_TRUE(checked.expr_expected_types.empty());
    EXPECT_TRUE(checked.pattern_case_name_ids.empty());
    ASSERT_FALSE(checked.generic_function_instances.empty());
    EXPECT_TRUE(checked.generic_function_instances.front().side_tables.expr_expected_types.empty());
    EXPECT_TRUE(checked.generic_function_instances.front().side_tables.pattern_case_name_ids.empty());
    EXPECT_EQ(
        checked.generic_function_instances.front().side_tables.layout, &checked.generic_side_table_layouts.front());
    sema::CheckedModule checked_assigned;
    checked_assigned = checked;
    EXPECT_EQ(checked_assigned.enum_cases.at(enum_case_key).payload_types.front().value, i32.value);
    EXPECT_EQ(checked_assigned.enum_cases.at(enum_case_key).generic_instance_key, checked_generic_instance_key);
    EXPECT_EQ(checked_assigned.enum_cases.at(enum_case_key).part_index, SEMA_TEST_CHECKED_COPY_PART_INDEX);
    sema::CheckedModule checked_moved(std::move(checked_copy));
    EXPECT_EQ(checked_moved.structs.at(struct_key).name, "S");
    EXPECT_EQ(checked_moved.structs.at(struct_key).generic_instance_key, checked_generic_instance_key);
    EXPECT_EQ(checked_moved.structs.at(struct_key).part_index, SEMA_TEST_CHECKED_COPY_PART_INDEX);
    ASSERT_EQ(checked_moved.structs.at(struct_key).fields.size(), 1U);
    EXPECT_EQ(checked_moved.structs.at(struct_key).fields.front().name, "field");
    EXPECT_EQ(checked_moved.structs.at(struct_key).fields.front().c_name, "m0_S_field");
    sema::CheckedModule checked_move_assigned;
    checked_move_assigned = std::move(checked_assigned);
    EXPECT_EQ(checked_move_assigned.type_aliases.at(alias_key).name, "Alias");
    EXPECT_EQ(checked_move_assigned.type_aliases.at(alias_key).part_index, SEMA_TEST_CHECKED_COPY_PART_INDEX);
    ASSERT_EQ(checked_move_assigned.generic_enum_instances.size(), 1U);
    EXPECT_EQ(checked_move_assigned.generic_enum_instances.front().generic_instance_key, checked_generic_instance_key);
    ASSERT_EQ(checked_move_assigned.generic_type_alias_instances.size(), 1U);
    EXPECT_EQ(
        checked_move_assigned.generic_type_alias_instances.front().generic_instance_key, checked_generic_instance_key);
}

TEST(CoreUnit, SemanticWhiteBoxCheckedModuleIndexesCallBindingsByExpr)
{
    constexpr base::u32 SEMA_TEST_INDEXED_FUNCTION_CALL_EXPR = 7U;
    constexpr base::u32 SEMA_TEST_INDEXED_TRAIT_CALL_EXPR = 11U;
    constexpr base::u32 SEMA_TEST_MANUAL_FUNCTION_CALL_EXPR = 13U;
    constexpr base::u32 SEMA_TEST_MANUAL_TRAIT_CALL_EXPR = 17U;
    constexpr base::u32 SEMA_TEST_MISSING_FUNCTION_CALL_EXPR = 19U;
    constexpr base::u32 SEMA_TEST_MISSING_TRAIT_CALL_EXPR = 23U;

    sema::CheckedModule checked;
    sema::FunctionCallBinding function_binding = checked.make_function_call_binding();
    function_binding.call_expr = ExprId{SEMA_TEST_INDEXED_FUNCTION_CALL_EXPR};
    checked.append_function_call_binding(function_binding);
    sema::TraitMethodCallBinding trait_binding = checked.make_trait_method_call_binding();
    trait_binding.call_expr = ExprId{SEMA_TEST_INDEXED_TRAIT_CALL_EXPR};
    checked.append_trait_method_call_binding(trait_binding);

    ASSERT_NE(checked.function_call_binding_for_expr(ExprId{SEMA_TEST_INDEXED_FUNCTION_CALL_EXPR}), nullptr);
    EXPECT_EQ(checked.function_call_binding_for_expr(ExprId{SEMA_TEST_INDEXED_FUNCTION_CALL_EXPR})->call_expr.value,
        SEMA_TEST_INDEXED_FUNCTION_CALL_EXPR);
    ASSERT_NE(checked.trait_method_call_binding_for_expr(ExprId{SEMA_TEST_INDEXED_TRAIT_CALL_EXPR}), nullptr);
    EXPECT_EQ(checked.trait_method_call_binding_for_expr(ExprId{SEMA_TEST_INDEXED_TRAIT_CALL_EXPR})->call_expr.value,
        SEMA_TEST_INDEXED_TRAIT_CALL_EXPR);
    EXPECT_EQ(checked.function_call_binding_for_expr(syntax::INVALID_EXPR_ID), nullptr);
    EXPECT_EQ(checked.trait_method_call_binding_for_expr(syntax::INVALID_EXPR_ID), nullptr);
    EXPECT_EQ(checked.function_call_binding_for_expr(ExprId{SEMA_TEST_MISSING_FUNCTION_CALL_EXPR}), nullptr);
    EXPECT_EQ(checked.trait_method_call_binding_for_expr(ExprId{SEMA_TEST_MISSING_TRAIT_CALL_EXPR}), nullptr);

    sema::FunctionCallBinding invalid_function_binding = checked.make_function_call_binding();
    invalid_function_binding.call_expr = syntax::INVALID_EXPR_ID;
    checked.append_function_call_binding(invalid_function_binding);
    sema::TraitMethodCallBinding invalid_trait_binding = checked.make_trait_method_call_binding();
    invalid_trait_binding.call_expr = syntax::INVALID_EXPR_ID;
    checked.append_trait_method_call_binding(invalid_trait_binding);

    sema::FunctionCallBinding manual_function_binding = checked.make_function_call_binding();
    manual_function_binding.call_expr = ExprId{SEMA_TEST_MANUAL_FUNCTION_CALL_EXPR};
    checked.function_calls.push_back(manual_function_binding);
    sema::TraitMethodCallBinding manual_trait_binding = checked.make_trait_method_call_binding();
    manual_trait_binding.call_expr = ExprId{SEMA_TEST_MANUAL_TRAIT_CALL_EXPR};
    checked.trait_method_calls.push_back(manual_trait_binding);

    ASSERT_NE(checked.function_call_binding_for_expr(ExprId{SEMA_TEST_MANUAL_FUNCTION_CALL_EXPR}), nullptr);
    EXPECT_EQ(checked.function_call_binding_for_expr(ExprId{SEMA_TEST_MANUAL_FUNCTION_CALL_EXPR})->call_expr.value,
        SEMA_TEST_MANUAL_FUNCTION_CALL_EXPR);
    ASSERT_NE(checked.trait_method_call_binding_for_expr(ExprId{SEMA_TEST_MANUAL_TRAIT_CALL_EXPR}), nullptr);
    EXPECT_EQ(checked.trait_method_call_binding_for_expr(ExprId{SEMA_TEST_MANUAL_TRAIT_CALL_EXPR})->call_expr.value,
        SEMA_TEST_MANUAL_TRAIT_CALL_EXPR);
    EXPECT_EQ(checked.function_call_binding_for_expr(ExprId{SEMA_TEST_MISSING_FUNCTION_CALL_EXPR}), nullptr);
    EXPECT_EQ(checked.trait_method_call_binding_for_expr(ExprId{SEMA_TEST_MISSING_TRAIT_CALL_EXPR}), nullptr);

    sema::CheckedModule copied(checked);
    ASSERT_NE(copied.function_call_binding_for_expr(ExprId{SEMA_TEST_MANUAL_FUNCTION_CALL_EXPR}), nullptr);
    ASSERT_NE(copied.trait_method_call_binding_for_expr(ExprId{SEMA_TEST_MANUAL_TRAIT_CALL_EXPR}), nullptr);
}

TEST(CoreUnit, SemanticWhiteBoxFunctionCallBindingHandlesInvalidAndFallbackReceiverFacts)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_LOAN_MODULE_NAME})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    FunctionSignature invalid_signature = analyzer.state_.checked.make_function_signature();
    analyzer.record_function_call_binding(
        syntax::INVALID_EXPR_ID, syntax::INVALID_EXPR_ID, invalid_signature, 0, body_loan_test_range(0));
    EXPECT_TRUE(analyzer.state_.checked.function_calls.empty());

    const IdentId function_id = module.intern_identifier("target");
    const ExprId call_expr = push_call(module, syntax::INVALID_EXPR_ID);
    FunctionSignature signature = analyzer.state_.checked.make_function_signature();
    signature.name = analyzer.state_.checked.intern_text("target");
    signature.name_id = function_id;
    signature.semantic_key = sema::FunctionLookupKey{
        module_id(SEMA_TEST_ROOT_MODULE_INDEX).value,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        function_id,
    };
    signature.return_type = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    signature.param_types = {INVALID_TYPE_HANDLE};

    analyzer.record_function_call_binding(call_expr, syntax::INVALID_EXPR_ID, signature, 1, body_loan_test_range(1));
    ASSERT_EQ(analyzer.state_.checked.function_calls.size(), 1U);
    const sema::FunctionCallBinding& binding = analyzer.state_.checked.function_calls.front();
    EXPECT_EQ(binding.call_expr.value, call_expr.value);
    EXPECT_EQ(binding.receiver_arg_count, 1U);
    EXPECT_EQ(binding.receiver_access, sema::ReceiverAccessKind::none);
    EXPECT_FALSE(binding.receiver_auto_borrow);
    EXPECT_FALSE(binding.receiver_two_phase_eligible);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxSourceNamesBorrowAstInternerAcrossCheckedModuleMoves)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const IdentId name_id = module.intern_identifier("borrowed");
    module.finalize_identifiers();

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::InternedText source_name = analyzer.source_name_text(name_id, "borrowed");
    ASSERT_EQ(source_name.interner, &module.identifiers);
    EXPECT_EQ(source_name, "borrowed");

    sema::CheckedModule checked;
    FunctionSignature signature = checked.make_function_signature();
    signature.name = source_name;
    signature.name_id = name_id;
    signature.semantic_key = sema::FunctionLookupKey{
        module_id(0).value,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        name_id,
    };
    signature.c_name = checked.intern_text("m0_borrowed");
    checked.functions.emplace(signature.semantic_key, signature);

    sema::CheckedModule moved(std::move(checked));
    ASSERT_EQ(moved.functions.size(), 1U);
    const FunctionSignature& moved_signature = moved.functions.begin()->second;
    EXPECT_EQ(moved_signature.name.interner, &module.identifiers);
    EXPECT_EQ(moved_signature.c_name.interner, &moved.c_names);
    EXPECT_EQ(moved_signature.name, "borrowed");
    EXPECT_EQ(moved_signature.c_name, "m0_borrowed");

    sema::CheckedModule copied(moved);
    ASSERT_EQ(copied.functions.size(), 1U);
    const FunctionSignature& copied_signature = copied.functions.begin()->second;
    EXPECT_EQ(copied_signature.name.interner, &copied.c_names);
    EXPECT_EQ(copied_signature.c_name.interner, &copied.c_names);
    EXPECT_EQ(copied_signature.name, "borrowed");
    EXPECT_EQ(copied_signature.c_name, "m0_borrowed");
}

TEST(CoreUnit, SemanticWhiteBoxParserOnlyModuleContractIsNormalized)
{
    using CheckedExprCNameTable = decltype(sema::CheckedModule{}.expr_c_name_ids);
    using CheckedPatternCNameTable = decltype(sema::CheckedModule{}.pattern_c_name_ids);
    using CheckedItemCNameTable = decltype(sema::CheckedModule{}.item_c_name_ids);
    using CheckedGenericLayoutTable = decltype(sema::CheckedModule{}.generic_side_table_layouts);
    using CheckedGenericInstanceTable = decltype(sema::CheckedModule{}.generic_function_instances);
    using AnalyzerGenericMethodIndex =
        decltype(std::declval<sema::SemanticAnalyzerCore&>().state_.names.generic_method_templates_by_name);
    using AnalyzerEnumCaseTypeIndex =
        decltype(std::declval<sema::SemanticAnalyzerCore&>().state_.names.enum_cases_by_type);
    using AnalyzerVisibleModuleCache =
        decltype(std::declval<sema::SemanticAnalyzerCore&>().state_.modules.visible_modules_cache);
    using GenericTemplateParams = decltype(std::declval<sema::SemanticAnalyzerCore::GenericTemplateInfo&>().params);
    using GenericTemplateConstraints =
        decltype(std::declval<sema::SemanticAnalyzerCore::GenericTemplateInfo&>().constraints);
    using FunctionParamTypes = decltype(std::declval<sema::FunctionSignature&>().param_types);
    using FunctionGenericArgs = decltype(std::declval<sema::FunctionSignature&>().generic_args);
    using StructFields = decltype(std::declval<sema::StructInfo&>().fields);
    using EnumPayloadTypes = decltype(std::declval<sema::EnumCaseInfo&>().payload_types);
    using TypeTupleElements = decltype(std::declval<sema::TypeInfo&>().tuple_elements);
    using TypeFunctionParams = decltype(std::declval<sema::TypeInfo&>().function_params);
    using TypeGenericArgs = decltype(std::declval<sema::TypeInfo&>().generic_args);

    static_assert(std::is_same_v<CheckedExprCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedPatternCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedItemCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedGenericLayoutTable, sema::SemaDeque<sema::GenericSideTableLayout>>);
    static_assert(std::is_same_v<CheckedGenericInstanceTable, sema::SemaDeque<sema::GenericFunctionInstanceInfo>>);
    static_assert(
        std::is_same_v<AnalyzerGenericMethodIndex::mapped_type, sema::SemanticAnalyzerCore::GenericTemplateList>);
    static_assert(std::is_same_v<AnalyzerEnumCaseTypeIndex::mapped_type, sema::SemanticAnalyzerCore::EnumCaseList>);
    static_assert(std::is_same_v<AnalyzerVisibleModuleCache::mapped_type, sema::SemanticAnalyzerCore::ModuleIdList>);
    static_assert(std::is_same_v<GenericTemplateParams, sema::SemaVector<sema::IdentId>>);
    static_assert(std::is_same_v<GenericTemplateConstraints, sema::SemanticAnalyzerCore::CapabilityMap>);
    static_assert(std::is_same_v<FunctionParamTypes, sema::TypeHandleList>);
    static_assert(std::is_same_v<FunctionGenericArgs, sema::TypeHandleList>);
    static_assert(std::is_same_v<StructFields, sema::SemaVector<sema::StructFieldInfo>>);
    static_assert(std::is_same_v<EnumPayloadTypes, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeTupleElements, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeFunctionParams, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeGenericArgs, sema::TypeHandleList>);
    static_assert(std::is_same_v<decltype(sema::CheckedModule{}.normalized_ast), sema::NormalizedAstOverlay>);
    static_assert(std::is_same_v<decltype(sema::NormalizedAstOverlay{}.original_expr_count), base::u64>);
    static_assert(std::is_same_v<decltype(sema::NormalizedAstOverlay{}.final_type_count), base::u64>);
    static_assert(sizeof(sema::NormalizedAstOverlay) < sizeof(syntax::AstModule));

    syntax::AstModule module;
    module.module_path = module_path({"parser_only"});

    syntax::TypeNode i32_type_node = primitive_node(syntax::PrimitiveTypeKind::i32);
    const TypeId i32_type = module.push_type(i32_type_node);

    const ExprId zero = push_integer_text(module, "0");

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode main_function;
    main_function.kind = syntax::ItemKind::fn_decl;
    main_function.name = "main";
    main_function.return_type = i32_type;
    main_function.body = body;
    static_cast<void>(module.push_item(main_function));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_GT(checked_result.value().arena_bytes(), 0U);
    EXPECT_GT(checked_result.value().arena_blocks(), 0U);
    EXPECT_TRUE(checked_result.value().normalized_ast.parser_only_module_contract_added);
    EXPECT_EQ(checked_result.value().normalized_ast.original_expr_count, 1U);
    EXPECT_EQ(checked_result.value().normalized_ast.final_expr_count, module.exprs.size());
    EXPECT_EQ(checked_result.value().normalized_ast.original_type_count, 1U);
    EXPECT_EQ(checked_result.value().normalized_ast.final_type_count, module.types.size());
    EXPECT_EQ(module.modules.size(), 1U);
    EXPECT_EQ(module.item_modules.size(), 1U);
    EXPECT_EQ(module.item_modules.front().value, 0U);
    EXPECT_EQ(module.item_part_indices.size(), 1U);
    EXPECT_EQ(module.item_part_indices.front(), 0U);

    syntax::AstModule discard_module;
    discard_module.modules = {module_info({"root"})};
    const TypeId discard_i32_type = discard_module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId discard_zero = push_integer(discard_module);
    syntax::StmtNode discard_return_stmt;
    discard_return_stmt.kind = syntax::StmtKind::return_;
    discard_return_stmt.return_value = discard_zero;
    const syntax::StmtId discard_return_stmt_id = discard_module.push_stmt(discard_return_stmt);
    const syntax::StmtId discard_body = push_block(discard_module, {discard_return_stmt_id});

    syntax::ItemNode discard_main_function;
    discard_main_function.kind = syntax::ItemKind::fn_decl;
    discard_main_function.name = "main";
    discard_main_function.return_type = discard_i32_type;
    discard_main_function.body = discard_body;
    const syntax::ItemId discard_main_item = discard_module.push_item(discard_main_function);
    discard_module.item_modules[discard_main_item.value] = module_id(0);

    base::DiagnosticSink snapshot_diagnostics;
    sema::SemanticAnalyzerCore snapshot_analyzer(std::move(discard_module), snapshot_diagnostics);
    auto snapshot_result = snapshot_analyzer.analyze();
    ASSERT_TRUE(snapshot_result) << snapshot_result.error().message;
    EXPECT_FALSE(snapshot_result.value().normalized_ast.parser_only_module_contract_added);
    EXPECT_EQ(snapshot_result.value().normalized_ast.original_expr_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.final_expr_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.original_type_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.final_type_count, 1U);
}

TEST(CoreUnit, SemanticWhiteBoxParserAstRequiresItemModulesWhenModulesExist)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::TypeNode i32_type_node = primitive_node(syntax::PrimitiveTypeKind::i32);
    const TypeId i32_type = module.push_type(i32_type_node);

    syntax::ItemNode main_function;
    main_function.kind = syntax::ItemKind::fn_decl;
    main_function.name = "main";
    main_function.return_type = i32_type;
    static_cast<void>(module.push_item(main_function));
    module.item_modules.clear();

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found
            || diagnostic.message.find("item_modules must contain one module owner per item") != std::string::npos;
    }
    EXPECT_TRUE(found);
}

TEST(CoreUnit, SemanticWhiteBoxItemImportScopeRangeContractIsValidated)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    constexpr base::u32 SEMA_TEST_TOO_WIDE_SCOPE_ITEM_COUNT = 2;
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "main";
    const syntax::ItemId item = module.push_item_for_module(function, module_id(0), SEMA_TEST_PRIMARY_PART_INDEX);

    syntax::ItemImportScope out_of_range_scope;
    out_of_range_scope.item_begin = item.value;
    out_of_range_scope.item_count = SEMA_TEST_TOO_WIDE_SCOPE_ITEM_COUNT;
    out_of_range_scope.part_index = SEMA_TEST_PRIMARY_PART_INDEX;
    module.item_import_scopes.push_back(std::move(out_of_range_scope));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    constexpr std::string_view SEMA_TEST_EXPECTED_MESSAGE = sema::SEMA_AST_ITEM_MODULE_CONTRACT;
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find(SEMA_TEST_EXPECTED_MESSAGE) != std::string::npos;
    }
    EXPECT_TRUE(found);
}

TEST(CoreUnit, SemanticWhiteBoxItemImportScopeModuleContractIsValidated)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"dep"}),
    };

    syntax::ItemNode root_function;
    root_function.kind = syntax::ItemKind::fn_decl;
    root_function.name = "root_fn";
    const syntax::ItemId root_item =
        module.push_item_for_module(root_function, module_id(0), SEMA_TEST_PRIMARY_PART_INDEX);

    syntax::ItemNode dep_function;
    dep_function.kind = syntax::ItemKind::fn_decl;
    dep_function.name = "dep_fn";
    static_cast<void>(module.push_item_for_module(dep_function, module_id(1), SEMA_TEST_PRIMARY_PART_INDEX));

    syntax::ItemImportScope cross_module_scope;
    cross_module_scope.item_begin = root_item.value;
    cross_module_scope.item_count = 2;
    cross_module_scope.part_index = SEMA_TEST_PRIMARY_PART_INDEX;
    module.item_import_scopes.push_back(std::move(cross_module_scope));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    constexpr std::string_view SEMA_TEST_EXPECTED_MESSAGE = sema::SEMA_AST_ITEM_IMPORT_SCOPE_MODULE_INVALID;
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find(SEMA_TEST_EXPECTED_MESSAGE) != std::string::npos;
    }
    EXPECT_TRUE(found);
}

TEST(CoreUnit, SemanticWhiteBoxItemImportScopePartContractIsValidated)
{
    constexpr base::u32 SEMA_TEST_PRIMARY_PART_INDEX = 0;
    constexpr base::u32 SEMA_TEST_FRAGMENT_PART_INDEX = 1;
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::ItemNode primary_function;
    primary_function.kind = syntax::ItemKind::fn_decl;
    primary_function.name = "primary";
    const syntax::ItemId primary_item =
        module.push_item_for_module(primary_function, module_id(0), SEMA_TEST_PRIMARY_PART_INDEX);

    syntax::ItemNode part_function;
    part_function.kind = syntax::ItemKind::fn_decl;
    part_function.name = "from_part";
    static_cast<void>(module.push_item_for_module(part_function, module_id(0), SEMA_TEST_FRAGMENT_PART_INDEX));

    syntax::ItemImportScope cross_part_scope;
    cross_part_scope.item_begin = primary_item.value;
    cross_part_scope.item_count = 2;
    cross_part_scope.part_index = SEMA_TEST_PRIMARY_PART_INDEX;
    module.item_import_scopes.push_back(std::move(cross_part_scope));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    constexpr std::string_view SEMA_TEST_EXPECTED_MESSAGE = sema::SEMA_AST_ITEM_IMPORT_SCOPE_PART_INVALID;
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find(SEMA_TEST_EXPECTED_MESSAGE) != std::string::npos;
    }
    EXPECT_TRUE(found);
}

TEST(CoreUnit, SemanticWhiteBoxItemImportScopePartKeyContractIsValidated)
{
    constexpr base::u32 SEMA_TEST_FRAGMENT_PART_INDEX = 1;
    constexpr base::usize SEMA_TEST_PART_KEY_TABLE_SIZE = SEMA_TEST_FRAGMENT_PART_INDEX + 1;
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::ItemNode part_function;
    part_function.kind = syntax::ItemKind::fn_decl;
    part_function.name = "from_part";
    const syntax::ItemId part_item =
        module.push_item_for_module(part_function, module_id(0), SEMA_TEST_FRAGMENT_PART_INDEX);

    syntax::ItemImportScope part_scope;
    part_scope.item_begin = part_item.value;
    part_scope.item_count = 1;
    part_scope.part_index = SEMA_TEST_FRAGMENT_PART_INDEX;
    module.item_import_scopes.push_back(std::move(part_scope));

    sema::SemanticOptions options;
    options.module_part_keys.resize(module.modules.size());
    options.module_part_keys[0].resize(SEMA_TEST_PART_KEY_TABLE_SIZE);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics, options);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    constexpr std::string_view SEMA_TEST_EXPECTED_MESSAGE = sema::SEMA_AST_ITEM_IMPORT_SCOPE_PART_INVALID;
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find(SEMA_TEST_EXPECTED_MESSAGE) != std::string::npos;
    }
    EXPECT_TRUE(found);
}

TEST(CoreUnit, SemanticWhiteBoxSyntaxTypeCacheDisabledDoesNotRead)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const TypeId bool_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::bool_));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    const TypeHandle bool_type = analyzer.state_.checked.types.builtin(BuiltinType::bool_);
    analyzer.state_.checked.syntax_type_handles[bool_type_id.value] = bool_type;

    EXPECT_EQ(analyzer.cached_syntax_type(bool_type_id).value, bool_type.value);
    analyzer.state_.flow.current_side_tables.cache_syntax_types = false;
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(bool_type_id)));
}

TEST(CoreUnit, SemanticWhiteBoxStringBuiltinExpressions)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::TypeNode u8_type;
    u8_type.kind = syntax::TypeKind::primitive;
    u8_type.primitive = syntax::PrimitiveTypeKind::u8;
    const TypeId u8_type_id = module.push_type(u8_type);
    syntax::TypeNode const_u8_ptr_type;
    const_u8_ptr_type.kind = syntax::TypeKind::pointer;
    const_u8_ptr_type.pointer_mutability = syntax::PointerMutability::const_;
    const_u8_ptr_type.pointee = u8_type_id;
    const TypeId const_u8_ptr_type_id = module.push_type(const_u8_ptr_type);

    const ExprId str_value = push_name(module, "text");
    const ExprId data_value = push_name(module, "data");
    const ExprId length_value = push_name(module, "len");
    const ExprId str_data_id = module.push_cast_like_expr(
        syntax::ExprKind::str_data, {}, syntax::CastExprPayload{syntax::INVALID_TYPE_ID, str_value});
    const ExprId str_byte_len_id = module.push_cast_like_expr(
        syntax::ExprKind::str_byte_len, {}, syntax::CastExprPayload{syntax::INVALID_TYPE_ID, str_value});
    const ExprId str_from_bytes_id = module.push_call_expr(syntax::ExprKind::str_from_bytes_unchecked, {},
        syntax::CallExprPayload{syntax::INVALID_EXPR_ID, {data_value, length_value}});
    const ExprId malformed_id = module.push_call_expr(
        syntax::ExprKind::str_from_bytes_unchecked, {}, syntax::CallExprPayload{syntax::INVALID_EXPR_ID, {data_value}});
    const ExprId raw_literal_id = module.push_literal_expr(syntax::ExprKind::raw_string_literal, {}, "r\"C:\\tmp\\a\"");
    const ExprId byte_string_literal_id =
        module.push_literal_expr(syntax::ExprKind::byte_string_literal, {}, "b\"a\\n\\0\"");
    const ExprId invalid_byte_string_literal_id =
        module.push_literal_expr(syntax::ExprKind::byte_string_literal, {}, "b\"\\u{41}\"");
    const ExprId char_literal_id = module.push_literal_expr(syntax::ExprKind::char_literal, {}, "'\\u{03BB}'");

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle str = types.builtin(BuiltinType::str);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle const_u8_ptr = types.pointer(PointerMutability::const_, u8);
    analyzer.state_.checked.syntax_type_handles[u8_type_id.value] = u8;
    analyzer.state_.checked.syntax_type_handles[const_u8_ptr_type_id.value] = const_u8_ptr;
    const sema::FunctionLookupKey text_key =
        add_global_value(analyzer, module_id(0), "text", str, SymbolKind::local).first;
    const sema::FunctionLookupKey data_key =
        add_global_value(analyzer, module_id(0), "data", const_u8_ptr, SymbolKind::local).first;
    const sema::FunctionLookupKey len_key =
        add_global_value(analyzer, module_id(0), "len", usize, SymbolKind::local).first;

    EXPECT_TRUE(
        types.same(analyzer.analyze_str_projection_expr(str_data_id, analyzer.expr_view(str_data_id)), const_u8_ptr));
    EXPECT_TRUE(
        types.same(analyzer.analyze_str_projection_expr(str_byte_len_id, analyzer.expr_view(str_byte_len_id)), usize));
    EXPECT_TRUE(types.same(
        analyzer.analyze_str_from_bytes_unchecked_expr(str_from_bytes_id, analyzer.expr_view(str_from_bytes_id)), str));
    EXPECT_TRUE(types.same(
        analyzer.analyze_str_from_bytes_unchecked_expr(malformed_id, analyzer.expr_view(malformed_id)), str));
    EXPECT_TRUE(types.same(analyzer.analyze_expr(raw_literal_id), str));
    const TypeHandle byte_string_type = analyzer.analyze_expr(byte_string_literal_id);
    ASSERT_TRUE(types.is_array(byte_string_type));
    EXPECT_EQ(types.get(byte_string_type).array_count, 3U);
    EXPECT_TRUE(types.same(types.get(byte_string_type).array_element, u8));
    const base::usize diagnostics_before_invalid_byte_string = diagnostics.diagnostics().size();
    const TypeHandle invalid_byte_string_type = analyzer.analyze_expr(invalid_byte_string_literal_id);
    ASSERT_TRUE(types.is_array(invalid_byte_string_type));
    EXPECT_TRUE(types.same(types.get(invalid_byte_string_type).array_element, u8));
    EXPECT_GT(diagnostics.diagnostics().size(), diagnostics_before_invalid_byte_string);
    EXPECT_TRUE(types.is_char(analyzer.analyze_expr(char_literal_id)));

    analyzer.state_.functions.global_values[text_key].type = usize;
    analyzer.state_.functions.global_values[data_key].type = usize;
    analyzer.state_.functions.global_values[len_key].type = str;
    static_cast<void>(analyzer.analyze_str_projection_expr(str_data_id, analyzer.expr_view(str_data_id)));
    static_cast<void>(analyzer.analyze_str_projection_expr(str_byte_len_id, analyzer.expr_view(str_byte_len_id)));
    static_cast<void>(
        analyzer.analyze_str_from_bytes_unchecked_expr(str_from_bytes_id, analyzer.expr_view(str_from_bytes_id)));

    EXPECT_GT(diagnostics.diagnostics().size(), 0U);
}

TEST(CoreUnit, SemanticWhiteBoxSliceBuiltinExpressions)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId slice_value = push_name(module, "values");
    const ExprId data_id = module.push_cast_like_expr(
        syntax::ExprKind::slice_data, {}, syntax::CastExprPayload{syntax::INVALID_TYPE_ID, slice_value});
    const ExprId len_id = module.push_cast_like_expr(
        syntax::ExprKind::slice_len, {}, syntax::CastExprPayload{syntax::INVALID_TYPE_ID, slice_value});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle slice = types.slice(PointerMutability::const_, u8);
    const TypeHandle const_u8_ptr = types.pointer(PointerMutability::const_, u8);
    const sema::FunctionLookupKey value_key =
        add_global_value(analyzer, module_id(0), "values", slice, SymbolKind::local).first;

    EXPECT_TRUE(types.same(analyzer.analyze_slice_projection_expr(data_id, analyzer.expr_view(data_id)), const_u8_ptr));
    EXPECT_TRUE(types.same(analyzer.analyze_slice_projection_expr(len_id, analyzer.expr_view(len_id)), usize));

    analyzer.state_.functions.global_values[value_key].type = usize;
    const base::usize diagnostics_before_invalid_slice = diagnostics.diagnostics().size();
    analyzer.state_.checked.expr_types[slice_value.value] = INVALID_TYPE_HANDLE;
    analyzer.state_.checked.expr_types[data_id.value] = INVALID_TYPE_HANDLE;
    analyzer.state_.checked.expr_types[len_id.value] = INVALID_TYPE_HANDLE;
    static_cast<void>(analyzer.analyze_slice_projection_expr(data_id, analyzer.expr_view(data_id)));
    static_cast<void>(analyzer.analyze_slice_projection_expr(len_id, analyzer.expr_view(len_id)));
    EXPECT_GT(diagnostics.diagnostics().size(), diagnostics_before_invalid_slice);
}

TEST(CoreUnit, SemanticWhiteBoxArrayLiteralEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId repeat_value = push_integer(module);
    const ExprId repeat_literal_id = module.push_array_expr({},
        syntax::ArrayExprPayload{
            {},
            repeat_value,
            syntax::INVALID_EXPR_ID,
        });

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle expected_array = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i32);
    EXPECT_TRUE(types.same(
        analyzer.analyze_array_literal_expr(repeat_literal_id, analyzer.expr_view(repeat_literal_id), expected_array),
        expected_array));
    EXPECT_TRUE(diagnostics.has_error());
}

TEST(CoreUnit, SemanticWhiteBoxExpectedTypeSensitiveExprCache)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId integer_literal = push_integer_text(module, "2147483648");
    const ExprId small_integer_literal = push_integer_text(module, "7");
    const ExprId null_literal_id = module.push_literal_expr(syntax::ExprKind::null_literal, {}, "null");

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_intrinsic_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.prepare_analysis_only_storage(module.exprs.size());
    analyzer.state_.checked.expr_expected_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle ptr_i32 = types.pointer(PointerMutability::const_, i32);

    EXPECT_TRUE(types.same(analyzer.analyze_expr(integer_literal), i32));
    EXPECT_TRUE(types.same(analyzer.analyze_expr(integer_literal, i64), i64));
    EXPECT_TRUE(types.same(analyzer.state_.checked.expr_intrinsic_types[integer_literal.value], i32));
    EXPECT_TRUE(types.same(analyzer.state_.checked.expr_types[integer_literal.value], i64));
    EXPECT_TRUE(types.same(analyzer.state_.checked.expr_expected_types[integer_literal.value], i64));
    EXPECT_TRUE(types.same(analyzer.analyze_expr(integer_literal), i32));
    EXPECT_TRUE(types.same(analyzer.state_.checked.expr_intrinsic_types[integer_literal.value], i32));
    EXPECT_TRUE(types.same(analyzer.state_.checked.expr_types[integer_literal.value], i32));
    EXPECT_FALSE(is_valid(analyzer.state_.checked.expr_expected_types[integer_literal.value]));

    EXPECT_TRUE(types.same(analyzer.analyze_expr(small_integer_literal, i64), i64));
    ASSERT_FALSE(analyzer.state_.checked.coercions.empty());
    const sema::CoercionRecord& coercion = analyzer.state_.checked.coercions.back();
    EXPECT_EQ(coercion.expr.value, small_integer_literal.value);
    EXPECT_TRUE(types.same(coercion.from_type, i32));
    EXPECT_TRUE(types.same(coercion.to_type, i64));
    EXPECT_EQ(coercion.kind, sema::CoercionKind::contextual_integer_literal);

    EXPECT_FALSE(is_valid(analyzer.analyze_expr(null_literal_id)));
    const std::size_t null_coercion_index = analyzer.state_.checked.coercions.size();
    EXPECT_TRUE(types.same(analyzer.analyze_expr(null_literal_id, ptr_i32), ptr_i32));
    EXPECT_TRUE(types.same(analyzer.state_.checked.expr_types[null_literal_id.value], ptr_i32));
    EXPECT_TRUE(types.same(analyzer.state_.checked.expr_expected_types[null_literal_id.value], ptr_i32));
    ASSERT_GT(analyzer.state_.checked.coercions.size(), null_coercion_index);
    const sema::CoercionRecord& null_coercion = analyzer.state_.checked.coercions.back();
    EXPECT_EQ(null_coercion.expr.value, null_literal_id.value);
    EXPECT_FALSE(is_valid(null_coercion.from_type));
    EXPECT_TRUE(types.same(null_coercion.to_type, ptr_i32));
    EXPECT_EQ(null_coercion.kind, sema::CoercionKind::null_to_pointer);
}

TEST(CoreUnit, SemanticWhiteBoxLiteralSuffixCapabilityAndStorageEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    const ExprId negative_valid = push_integer_text(module, SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX);
    const ExprId negative_invalid_suffix = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_BAD_SUFFIX);
    const ExprId negative_mismatch = push_integer_text(module, SEMA_TEST_NEGATIVE_I8_MISMATCH_SUFFIX);
    const ExprId negative_overflow = push_integer_text(module, SEMA_TEST_NEGATIVE_I8_OVERFLOW_SUFFIX);
    const ExprId float_mismatch =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_LITERAL_MISMATCH_F32_SUFFIX);
    const ExprId void_value = push_name(module, "void_value");
    const ExprId shared_void_ref = push_unary(module, syntax::UnaryOp::address_of, void_value);
    const ExprId mut_void_ref = push_unary(module, syntax::UnaryOp::address_of_mut, void_value);
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));
    const ExprId size_of_generic = module.push_cast_like_expr(
        syntax::ExprKind::size_of, {}, syntax::CastExprPayload{generic_type, syntax::INVALID_EXPR_ID});
    const ExprId lib_alias = push_name(module, "lib");
    const ExprId lib_type_member = push_field(module, lib_alias, "Thing");
    const ExprId lib_template_member = push_field(module, lib_alias, "Box");

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle i8 = types.builtin(BuiltinType::i8);
    const TypeHandle i16 = types.builtin(BuiltinType::i16);
    const TypeHandle f64 = types.builtin(BuiltinType::f64);
    const TypeHandle usize = types.builtin(BuiltinType::usize);

    EXPECT_TRUE(analyzer.negative_integer_literal_fits_type(i8, SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX));
    EXPECT_FALSE(analyzer.negative_integer_literal_fits_type(i16, SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX));
    EXPECT_FALSE(analyzer.negative_integer_literal_fits_type(i8, SEMA_TEST_INTEGER_LITERAL_BAD_SUFFIX));
    EXPECT_TRUE(types.same(analyzer.analyze_negative_integer_literal(
                               negative_valid, SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX, {}, INVALID_TYPE_HANDLE),
        i8));
    EXPECT_TRUE(types.same(analyzer.analyze_negative_integer_literal(
                               negative_invalid_suffix, SEMA_TEST_INTEGER_LITERAL_BAD_SUFFIX, {}, INVALID_TYPE_HANDLE),
        types.builtin(BuiltinType::i32)));
    EXPECT_TRUE(types.same(
        analyzer.analyze_negative_integer_literal(negative_mismatch, SEMA_TEST_NEGATIVE_I8_MISMATCH_SUFFIX, {}, i16),
        i8));
    EXPECT_TRUE(types.same(analyzer.analyze_negative_integer_literal(
                               negative_overflow, SEMA_TEST_NEGATIVE_I8_OVERFLOW_SUFFIX, {}, INVALID_TYPE_HANDLE),
        i8));
    EXPECT_TRUE(
        types.same(analyzer.analyze_float_literal(float_mismatch, SEMA_TEST_FLOAT_LITERAL_MISMATCH_F32_SUFFIX, {}, f64),
            types.builtin(BuiltinType::f32)));

    const TypeHandle generic =
        types.generic_param(sema::generic_param_identity_from_text("test.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    sema::SemanticAnalyzerCore::GenericContext generic_context = analyzer.make_generic_context();
    const IdentId generic_id = intern_identifier(analyzer, SEMA_TEST_GENERIC_PARAM_NAME);
    generic_context.params.emplace(generic_id, generic);
    generic_context.param_identities.emplace(generic_id, sema::generic_param_identity_from_text("test.T"));
    analyzer.state_.flow.current_generic_context = &generic_context;
    EXPECT_FALSE(analyzer.generic_param_has_capability(generic, sema::CapabilityKind::ord));
    sema::SemanticAnalyzerCore::ExprView ordering_expr;
    ordering_expr.kind = syntax::ExprKind::binary;
    ordering_expr.binary_op = syntax::BinaryOp::less;
    EXPECT_TRUE(types.is_bool(analyzer.record_ordering_binary_expr(syntax::INVALID_EXPR_ID, ordering_expr, generic)));
    EXPECT_TRUE(
        types.same(analyzer.analyze_size_or_align_expr(size_of_generic, analyzer.expr_view(size_of_generic)), usize));
    analyzer.state_.flow.current_generic_context = nullptr;

    EXPECT_TRUE(analyzer.state_.names.symbols.insert(indexed_symbol(analyzer, SymbolKind::local, "void_value",
                                                         module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, true),
        diagnostics));
    EXPECT_TRUE(types.is_reference(
        analyzer.analyze_unary_expr(shared_void_ref, analyzer.expr_view(shared_void_ref), INVALID_TYPE_HANDLE)));
    EXPECT_TRUE(types.is_reference(
        analyzer.analyze_unary_expr(mut_void_ref, analyzer.expr_view(mut_void_ref), INVALID_TYPE_HANDLE)));

    const TypeHandle lib_thing = types.named_struct("lib.Thing", "lib_Thing", false);
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Thing", lib_thing));
    sema::SemanticAnalyzerCore::GenericTemplateInfo generic_template =
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Box");
    const auto inserted_template =
        analyzer.state_.generics.struct_templates.emplace(generic_template.key, generic_template);
    ASSERT_TRUE(inserted_template.second);
    analyzer.index_generic_struct_template(inserted_template.first->second);
    EXPECT_FALSE(is_valid(analyzer.analyze_module_member_expr(
        lib_type_member, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), analyzer.expr_view(lib_type_member))));
    EXPECT_FALSE(is_valid(analyzer.analyze_module_member_expr(
        lib_template_member, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), analyzer.expr_view(lib_template_member))));

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages.push_back('\n');
    }
    EXPECT_NE(messages.find("invalid integer literal suffix"), std::string::npos);
    EXPECT_NE(messages.find("integer literal suffix type"), std::string::npos);
    EXPECT_NE(messages.find("integer literal out of range"), std::string::npos);
    EXPECT_NE(messages.find("float literal suffix type"), std::string::npos);
    EXPECT_NE(messages.find("generic type parameter cannot be queried by sizeof or alignof"), std::string::npos);
    EXPECT_NE(messages.find("reference requires a valid storage type"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxSliceStructAndMatchFocusedEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId array_value = push_name(module, "values");
    const ExprId slice_start = push_integer_text(module, SEMA_TEST_ARRAY_SLICE_OUT_OF_BOUNDS);
    const ExprId slice_end_value = push_integer_text(module, SEMA_TEST_ARRAY_SLICE_NEGATIVE_OUT_OF_BOUNDS);
    const ExprId slice_end = push_unary(module, syntax::UnaryOp::numeric_negate, slice_end_value);
    const ExprId array_slice =
        module.push_slice_expr({}, syntax::SliceExprPayload{array_value, slice_start, slice_end});
    const ExprId missing_field_value = push_integer(module);
    const ExprId missing_struct_literal =
        module.push_struct_literal_expr({}, syntax::INVALID_EXPR_ID, {}, {}, "Missing", std::vector<TypeId>{},
            std::vector<syntax::FieldInit>{syntax::FieldInit{"value", missing_field_value, {}}},
            syntax::INVALID_IDENT_ID, syntax::INVALID_IDENT_ID);
    const ExprId ghost_struct_literal = module.push_struct_literal_expr({}, syntax::INVALID_EXPR_ID, {}, {}, "Ghost",
        std::vector<TypeId>{}, std::vector<syntax::FieldInit>{}, syntax::INVALID_IDENT_ID, syntax::INVALID_IDENT_ID);
    const ExprId opaque_struct_literal = module.push_struct_literal_expr({}, syntax::INVALID_EXPR_ID, {}, {}, "Hidden",
        std::vector<TypeId>{}, std::vector<syntax::FieldInit>{}, syntax::INVALID_IDENT_ID, syntax::INVALID_IDENT_ID);

    const ExprId slice_subject = push_name(module, "items");
    const ExprId slice_result = push_integer(module);
    syntax::PatternNode true_pattern;
    true_pattern.kind = syntax::PatternKind::literal;
    true_pattern.case_name = "true";
    const syntax::PatternId true_pattern_id = module.push_pattern(true_pattern);
    syntax::PatternNode slice_pattern;
    slice_pattern.kind = syntax::PatternKind::slice;
    slice_pattern.elements = {true_pattern_id};
    const syntax::PatternId slice_pattern_id = module.push_pattern(slice_pattern);
    const ExprId slice_match = module.push_match_expr({},
        syntax::MatchExprPayload{
            slice_subject,
            {syntax::MatchArm{slice_pattern_id, syntax::INVALID_EXPR_ID, slice_result, {}}},
        });

    const ExprId large_array_subject = push_name(module, "wide");
    const ExprId large_array_result = push_integer(module);
    syntax::PatternNode large_true_pattern;
    large_true_pattern.kind = syntax::PatternKind::literal;
    large_true_pattern.case_name = "true";
    const syntax::PatternId large_true_pattern_id = module.push_pattern(large_true_pattern);
    syntax::PatternNode large_slice_pattern;
    large_slice_pattern.kind = syntax::PatternKind::slice;
    large_slice_pattern.elements = {large_true_pattern_id};
    large_slice_pattern.has_slice_rest = true;
    large_slice_pattern.slice_rest_index = 1;
    const syntax::PatternId large_slice_pattern_id = module.push_pattern(large_slice_pattern);
    const ExprId large_array_match = module.push_match_expr({},
        syntax::MatchExprPayload{
            large_array_subject,
            {syntax::MatchArm{large_slice_pattern_id, syntax::INVALID_EXPR_ID, large_array_result, {}}},
        });

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    analyzer.state_.checked.pattern_c_name_ids.assign(module.patterns.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle array_i32 = types.array(SEMA_TEST_ARRAY_SLICE_LENGTH, i32);
    const TypeHandle bool_slice = types.slice(PointerMutability::const_, bool_type);
    const TypeHandle large_bool_array = types.array(SEMA_TEST_LARGE_ARRAY_MATCH_COUNT, bool_type);
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "values", module_id(0), array_i32, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "items", module_id(0), bool_slice), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "wide", module_id(0), large_bool_array), diagnostics));

    const TypeHandle ghost_type = types.named_struct("Ghost", "Ghost", false);
    static_cast<void>(add_named_type(analyzer, module_id(0), "Ghost", ghost_type));
    const TypeHandle hidden_type = types.opaque_struct("Hidden", "Hidden");
    static_cast<void>(add_named_type(analyzer, module_id(0), "Hidden", hidden_type));

    EXPECT_TRUE(
        types.is_slice(analyzer.analyze_slice_expr(array_slice, analyzer.expr_view(array_slice), INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_struct_literal_expr(
        missing_struct_literal, analyzer.expr_view(missing_struct_literal), INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_struct_literal_expr(
        ghost_struct_literal, analyzer.expr_view(ghost_struct_literal), INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_struct_literal_expr(
        opaque_struct_literal, analyzer.expr_view(opaque_struct_literal), INVALID_TYPE_HANDLE)));
    EXPECT_TRUE(types.is_integer(
        analyzer.analyze_match_expr(slice_match, analyzer.expr_view(slice_match), INVALID_TYPE_HANDLE)));
    EXPECT_TRUE(types.is_integer(
        analyzer.analyze_match_expr(large_array_match, analyzer.expr_view(large_array_match), INVALID_TYPE_HANDLE)));

    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_ARRAY_SLICE_BOUND_OUT_OF_BOUNDS), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_STRUCT_LITERAL_TYPE), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_OPAQUE_POINTER_ONLY), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_MATCH_DYNAMIC_SLICE_WITNESS), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_MATCH_LARGE_ARRAY_IRREFUTABLE), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxContextualExprKeepsIntrinsicAndFinalTypesSeparate)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId binary_lhs = push_integer_text(module, "1");
    const ExprId binary_rhs = push_integer_text(module, "2");
    const ExprId binary =
        module.push_binary_expr({}, syntax::BinaryExprPayload{syntax::BinaryOp::add, binary_lhs, binary_rhs});

    const ExprId array_first = push_integer_text(module, "3");
    const ExprId array_second = push_integer_text(module, "4");
    const ExprId array = module.push_array_expr({}, std::vector<ExprId>{array_first, array_second});

    const ExprId tuple_first = push_integer_text(module, "5");
    const ExprId tuple_second = push_integer_text(module, "6");
    const ExprId tuple = module.push_tuple_expr({}, std::vector<ExprId>{tuple_first, tuple_second});

    const ExprId condition = push_bool(module, "true");
    const ExprId then_value = push_integer_text(module, "7");
    const ExprId else_value = push_integer_text(module, "8");
    const ExprId if_expr =
        module.push_if_expr({}, syntax::IfExprPayload{condition, syntax::INVALID_PATTERN_ID, then_value, else_value});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_intrinsic_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.prepare_analysis_only_storage(module.exprs.size());
    analyzer.state_.checked.expr_expected_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle array_i64 = types.array(2, i64);
    const TypeHandle array_i32 = types.array(2, i32);
    const TypeHandle tuple_i64 = types.tuple(std::vector<TypeHandle>{i64, i64});
    const TypeHandle tuple_i32 = types.tuple(std::vector<TypeHandle>{i32, i32});

    EXPECT_TRUE(types.same(analyzer.analyze_expr(binary, i64), i64));
    EXPECT_TRUE(types.same(analyzer.cached_expr_intrinsic_type(binary), i32));
    EXPECT_TRUE(types.same(analyzer.cached_expr_type(binary), i64));

    EXPECT_TRUE(types.same(analyzer.analyze_expr(array, array_i64), array_i64));
    EXPECT_TRUE(types.same(analyzer.cached_expr_intrinsic_type(array), array_i32));
    EXPECT_TRUE(types.same(analyzer.cached_expr_type(array), array_i64));

    EXPECT_TRUE(types.same(analyzer.analyze_expr(tuple, tuple_i64), tuple_i64));
    EXPECT_TRUE(types.same(analyzer.cached_expr_intrinsic_type(tuple), tuple_i32));
    EXPECT_TRUE(types.same(analyzer.cached_expr_type(tuple), tuple_i64));

    EXPECT_TRUE(types.same(analyzer.analyze_expr(if_expr, i64), i64));
    EXPECT_TRUE(types.same(analyzer.cached_expr_intrinsic_type(if_expr), i32));
    EXPECT_TRUE(types.same(analyzer.cached_expr_type(if_expr), i64));
}

TEST(CoreUnit, SemanticWhiteBoxControlExprDiagnosticsCoverVoidAndInvalidResults)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId condition = push_bool(module, "true");
    const ExprId void_then = push_name(module, "void_value");
    const ExprId void_else = push_name(module, "void_value");
    const ExprId void_if =
        module.push_if_expr({}, syntax::IfExprPayload{condition, syntax::INVALID_PATTERN_ID, void_then, void_else});

    const ExprId missing_block_result = push_name(module, "missing_value");
    const syntax::StmtId empty_block = push_block(module, {});
    const ExprId invalid_block =
        module.push_block_expr(syntax::ExprKind::block_expr, {}, empty_block, missing_block_result);

    const ExprId void_block_result = push_name(module, "void_value");
    const ExprId void_block = module.push_block_expr(syntax::ExprKind::block_expr, {}, empty_block, void_block_result);

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId returning_block = push_block(module, {return_stmt_id});
    const ExprId unsafe_unreachable_result = push_integer(module);
    const ExprId unsafe_unreachable_block =
        module.push_block_expr(syntax::ExprKind::unsafe_block, {}, returning_block, unsafe_unreachable_result);

    const ExprId unsafe_invalid_result = push_name(module, "missing_value");
    const ExprId unsafe_invalid_block =
        module.push_block_expr(syntax::ExprKind::unsafe_block, {}, empty_block, unsafe_invalid_result);

    const ExprId unsafe_void_result = push_name(module, "void_value");
    const ExprId unsafe_void_block =
        module.push_block_expr(syntax::ExprKind::unsafe_block, {}, empty_block, unsafe_void_result);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    analyzer.state_.checked.stmt_local_types.assign(module.stmts.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    static_cast<void>(add_global_value(analyzer, module_id(0), "void_value", void_type, SymbolKind::local));

    EXPECT_FALSE(is_valid(analyzer.analyze_if_expr(void_if, analyzer.expr_view(void_if), INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(
        is_valid(analyzer.analyze_block_expr(invalid_block, analyzer.expr_view(invalid_block), INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(
        is_valid(analyzer.analyze_block_expr(void_block, analyzer.expr_view(void_block), INVALID_TYPE_HANDLE)));
    EXPECT_TRUE(types.is_integer(analyzer.analyze_unsafe_block_expr(
        unsafe_unreachable_block, analyzer.expr_view(unsafe_unreachable_block), INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_unsafe_block_expr(
        unsafe_invalid_block, analyzer.expr_view(unsafe_invalid_block), INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_unsafe_block_expr(
        unsafe_void_block, analyzer.expr_view(unsafe_void_block), INVALID_TYPE_HANDLE)));

    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_IF_EXPR_VOID), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_BLOCK_EXPR_UNREACHABLE), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_BLOCK_EXPR_VOID), std::string::npos);
    EXPECT_NE(messages.find("unknown name: missing_value"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxTryExprReportsConstInitializerAndOptionReturnMismatch)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId option_value = push_name(module, "option_value");
    const ExprId try_option = module.push_try_expr({}, option_value);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle option_type = types.named_enum("OptionI32", "OptionI32");
    types.set_enum_underlying(option_type, types.builtin(BuiltinType::u8));
    static_cast<void>(add_enum_case(analyzer, module_id(0), "OptionI32_some", "some", option_type, i32, {i32}));
    static_cast<void>(add_enum_case(analyzer, module_id(0), "OptionI32_none", "none", option_type));
    static_cast<void>(add_global_value(analyzer, module_id(0), "option_value", option_type, SymbolKind::local));

    analyzer.state_.flow.current_function_return_type = i32;
    analyzer.state_.flow.in_const_initializer = true;
    EXPECT_TRUE(types.same(analyzer.analyze_try_expr(try_option, analyzer.expr_view(try_option)), i32));

    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_TRY_CONST_INITIALIZER), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_TRY_OPTION_RETURN), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxExpressionCategoryHelpersRejectMismatchedViews)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(0);

    sema::SemanticAnalyzerCore::ExprView name_view;
    name_view.kind = syntax::ExprKind::name;
    EXPECT_FALSE(is_valid(analyzer.analyze_literal_expr(syntax::INVALID_EXPR_ID, name_view, INVALID_TYPE_HANDLE)));

    sema::SemanticAnalyzerCore::ExprView integer_view;
    integer_view.kind = syntax::ExprKind::integer_literal;
    EXPECT_FALSE(is_valid(analyzer.analyze_value_expr(syntax::INVALID_EXPR_ID, integer_view, INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_control_expr(syntax::INVALID_EXPR_ID, integer_view, INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_aggregate_expr(syntax::INVALID_EXPR_ID, integer_view, INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(
        is_valid(analyzer.analyze_projection_expr(syntax::INVALID_EXPR_ID, integer_view, INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_operator_expr(syntax::INVALID_EXPR_ID, integer_view, INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(is_valid(analyzer.analyze_builtin_expr(syntax::INVALID_EXPR_ID, integer_view)));

    sema::SemanticAnalyzerCore::ExprView unknown_view;
    unknown_view.kind = static_cast<syntax::ExprKind>(SEMA_TEST_UNKNOWN_EXPR_KIND_VALUE);
    EXPECT_FALSE(is_valid(analyzer.analyze_expr(syntax::INVALID_EXPR_ID, unknown_view, INVALID_TYPE_HANDLE)));
}

TEST(CoreUnit, SemanticWhiteBoxBinaryOperatorSplitCoversGenericIntegerPath)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(0);

    sema::SemanticAnalyzerCore::ExprView expr;
    expr.kind = syntax::ExprKind::binary;
    expr.binary_op = syntax::BinaryOp::bit_and;
    const TypeHandle generic =
        analyzer.state_.checked.types.generic_param(sema::generic_param_identity_from_text("test.T"), "T");

    EXPECT_TRUE(analyzer.state_.checked.types.same(
        analyzer.record_integer_binary_expr(syntax::INVALID_EXPR_ID, expr, generic, generic), generic));
    EXPECT_TRUE(diagnostics.has_error());
}

TEST(CoreUnit, SemanticWhiteBoxBinaryOperatorReportsI64OverflowAndRecoversReversedNullComparison)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId min_magnitude = push_integer_text(module, SEMA_TEST_NEGATIVE_I64_MIN_MAGNITUDE);
    const ExprId negative_min = push_unary(module, syntax::UnaryOp::numeric_negate, min_magnitude);
    const ExprId one = push_integer(module);
    const ExprId negative_one = push_unary(module, syntax::UnaryOp::numeric_negate, one);
    const ExprId overflowing_division = push_binary(module, syntax::BinaryOp::div, negative_min, negative_one);
    const ExprId null_literal = module.push_literal_expr(syntax::ExprKind::null_literal, {}, "null");
    const ExprId pointer_value = push_name(module, "pointer_value");
    const ExprId reversed_null_comparison = push_binary(module, syntax::BinaryOp::equal, null_literal, pointer_value);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle pointer_i32 = types.pointer(PointerMutability::const_, i32);
    static_cast<void>(add_global_value(analyzer, module_id(0), "pointer_value", pointer_i32, SymbolKind::local));

    EXPECT_TRUE(types.same(
        analyzer.analyze_binary_expr(overflowing_division, analyzer.expr_view(overflowing_division), i64), i64));
    EXPECT_TRUE(types.is_bool(analyzer.analyze_binary_expr(
        reversed_null_comparison, analyzer.expr_view(reversed_null_comparison), INVALID_TYPE_HANDLE)));

    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find(sema::SEMA_SIGNED_DIVISION_OVERFLOW), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxStatementControlFlowQueries)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const syntax::StmtId return_stmt = push_stmt(module, syntax::StmtKind::return_);
    const syntax::StmtId expr_stmt = push_stmt(module, syntax::StmtKind::expr);
    const syntax::StmtId break_stmt = push_stmt(module, syntax::StmtKind::break_);
    const syntax::StmtId continue_stmt = push_stmt(module, syntax::StmtKind::continue_);

    const syntax::StmtId nested_return_block = push_block(module, {return_stmt});
    const syntax::StmtId nested_fallthrough_block = push_block(module, {expr_stmt});
    const syntax::StmtId mixed_block = push_block(module, {expr_stmt, nested_return_block});

    syntax::StmtNode full_if;
    full_if.kind = syntax::StmtKind::if_;
    full_if.then_block = nested_return_block;
    full_if.else_block = push_block(module, {return_stmt});
    const syntax::StmtId full_if_stmt = module.push_stmt(full_if);

    syntax::StmtNode partial_if = full_if;
    partial_if.else_block = nested_fallthrough_block;
    const syntax::StmtId partial_if_stmt = module.push_stmt(partial_if);

    syntax::StmtNode else_if_leaf;
    else_if_leaf.kind = syntax::StmtKind::if_;
    else_if_leaf.then_block = nested_return_block;
    else_if_leaf.else_block = push_block(module, {return_stmt});
    const syntax::StmtId else_if_leaf_stmt = module.push_stmt(else_if_leaf);

    syntax::StmtNode else_if_root;
    else_if_root.kind = syntax::StmtKind::if_;
    else_if_root.then_block = nested_return_block;
    else_if_root.else_if = else_if_leaf_stmt;
    const syntax::StmtId else_if_root_stmt = module.push_stmt(else_if_root);

    syntax::StmtNode missing_else_if_root = else_if_root;
    missing_else_if_root.else_if = syntax::INVALID_STMT_ID;
    const syntax::StmtId missing_else_if_stmt = module.push_stmt(missing_else_if_root);

    const syntax::StmtId fallthrough_block = push_block(module, {expr_stmt, partial_if_stmt});
    const syntax::StmtId non_fallthrough_block = push_block(module, {expr_stmt, full_if_stmt, expr_stmt});
    const syntax::StmtId abrupt_block = push_block(module, {continue_stmt, expr_stmt});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);

    EXPECT_FALSE(analyzer.block_guarantees_return(syntax::INVALID_STMT_ID));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(syntax::INVALID_STMT_ID));
    EXPECT_TRUE(analyzer.block_may_fallthrough(syntax::INVALID_STMT_ID));
    EXPECT_TRUE(analyzer.stmt_may_fallthrough(syntax::INVALID_STMT_ID));

    EXPECT_TRUE(analyzer.block_guarantees_return(return_stmt));
    EXPECT_TRUE(analyzer.block_guarantees_return(mixed_block));
    EXPECT_TRUE(analyzer.stmt_guarantees_return(full_if_stmt));
    EXPECT_TRUE(analyzer.stmt_guarantees_return(else_if_root_stmt));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(partial_if_stmt));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(missing_else_if_stmt));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(expr_stmt));

    EXPECT_FALSE(analyzer.stmt_may_fallthrough(return_stmt));
    EXPECT_FALSE(analyzer.stmt_may_fallthrough(break_stmt));
    EXPECT_FALSE(analyzer.stmt_may_fallthrough(continue_stmt));
    EXPECT_FALSE(analyzer.stmt_may_fallthrough(full_if_stmt));
    EXPECT_FALSE(analyzer.stmt_may_fallthrough(else_if_root_stmt));
    EXPECT_FALSE(analyzer.block_may_fallthrough(non_fallthrough_block));
    EXPECT_FALSE(analyzer.block_may_fallthrough(abrupt_block));
    EXPECT_TRUE(analyzer.stmt_may_fallthrough(partial_if_stmt));
    EXPECT_TRUE(analyzer.stmt_may_fallthrough(missing_else_if_stmt));
    EXPECT_TRUE(analyzer.block_may_fallthrough(fallthrough_block));

    const auto cached_value = [](const std::vector<base::u8>& cache, const syntax::StmtId stmt) {
        return stmt.value < cache.size() ? cache[stmt.value] : SEMA_TEST_CONTROL_FLOW_CACHE_UNKNOWN;
    };
    EXPECT_EQ(analyzer.state_.control_flow_queries.block_guarantees_return.size(), module.stmts.size());
    EXPECT_EQ(analyzer.state_.control_flow_queries.stmt_guarantees_return.size(), module.stmts.size());
    EXPECT_EQ(analyzer.state_.control_flow_queries.block_may_fallthrough.size(), module.stmts.size());
    EXPECT_EQ(analyzer.state_.control_flow_queries.stmt_may_fallthrough.size(), module.stmts.size());
    EXPECT_NE(cached_value(analyzer.state_.control_flow_queries.block_guarantees_return, mixed_block),
        SEMA_TEST_CONTROL_FLOW_CACHE_UNKNOWN);
    EXPECT_NE(cached_value(analyzer.state_.control_flow_queries.stmt_guarantees_return, full_if_stmt),
        SEMA_TEST_CONTROL_FLOW_CACHE_UNKNOWN);
    EXPECT_NE(cached_value(analyzer.state_.control_flow_queries.block_may_fallthrough, fallthrough_block),
        SEMA_TEST_CONTROL_FLOW_CACHE_UNKNOWN);
    EXPECT_NE(cached_value(analyzer.state_.control_flow_queries.stmt_may_fallthrough, partial_if_stmt),
        SEMA_TEST_CONTROL_FLOW_CACHE_UNKNOWN);

    const base::usize guarantee_cache_size = analyzer.state_.control_flow_queries.stmt_guarantees_return.size();
    const base::usize fallthrough_cache_size = analyzer.state_.control_flow_queries.block_may_fallthrough.size();
    EXPECT_TRUE(analyzer.stmt_guarantees_return(full_if_stmt));
    EXPECT_TRUE(analyzer.block_may_fallthrough(fallthrough_block));
    EXPECT_EQ(analyzer.state_.control_flow_queries.stmt_guarantees_return.size(), guarantee_cache_size);
    EXPECT_EQ(analyzer.state_.control_flow_queries.block_may_fallthrough.size(), fallthrough_cache_size);
}

TEST(CoreUnit, SemanticWhiteBoxIterativeTypeLayoutEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);

    const TypeHandle max_array_u8 = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, u8);
    EXPECT_EQ(analyzer.abi_size(max_array_u8), SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT);
    EXPECT_EQ(analyzer.abi_align(max_array_u8), alignof(std::uint8_t));

    const TypeHandle overflow_array_u64 = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, u64);
    EXPECT_EQ(analyzer.abi_size(overflow_array_u64), SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT);
    EXPECT_EQ(analyzer.abi_align(overflow_array_u64), alignof(std::uint64_t));

    const TypeHandle overflow_struct_type = types.named_struct("OverflowStruct", "OverflowStruct", true);
    const TypeHandle empty_struct_type = types.named_struct("EmptyStruct", "EmptyStruct", false);
    StructInfo empty_struct;
    empty_struct.name = analyzer.state_.checked.intern_text("EmptyStruct");
    empty_struct.name_id = intern_identifier(analyzer, "EmptyStruct");
    empty_struct.module = module_id(0);
    empty_struct.type = empty_struct_type;
    analyzer.state_.checked.structs.emplace(semantic_module_key(analyzer, module_id(0), "EmptyStruct"), empty_struct);

    StructInfo overflow_struct;
    overflow_struct.name = analyzer.state_.checked.intern_text("OverflowStruct");
    overflow_struct.name_id = intern_identifier(analyzer, "OverflowStruct");
    overflow_struct.module = module_id(0);
    overflow_struct.type = overflow_struct_type;
    overflow_struct.fields = {
        struct_field_info(analyzer, "huge", module_id(0), max_array_u8),
        struct_field_info(analyzer, "tail", module_id(0), u64),
    };
    analyzer.state_.checked.structs.emplace(
        semantic_module_key(analyzer, module_id(0), "OverflowStruct"), overflow_struct);

    const TypeHandle overflow_enum_type = types.named_enum("OverflowEnum", "OverflowEnum");
    types.set_enum_underlying(overflow_enum_type, max_array_u8);
    EnumCaseInfo overflow_case;
    overflow_case.name = analyzer.state_.checked.intern_text("payload");
    overflow_case.name_id = intern_identifier(analyzer, "payload");
    overflow_case.case_name = analyzer.state_.checked.intern_text("payload");
    overflow_case.case_name_id = intern_identifier(analyzer, "payload");
    overflow_case.module = module_id(0);
    overflow_case.type = overflow_enum_type;
    overflow_case.payload_type = u64;
    analyzer.state_.checked.enum_cases.emplace(semantic_module_key(analyzer, module_id(0), "payload"), overflow_case);
    static_cast<void>(add_named_type(analyzer, module_id(0), "OverflowEnum", overflow_enum_type));

    analyzer.validate_type_layouts();
    EXPECT_GT(diagnostics.diagnostics().size(), 0U);
}

TEST(CoreUnit, SemanticWhiteBoxTypeResolverAndAbiFocusedEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    const TypeId void_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::void_));
    const TypeId i32_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_is_variadic = true;
    function_type.function_params = {void_type_id};
    function_type.function_return = void_type_id;
    const TypeId invalid_function_type_id = module.push_type(function_type);

    syntax::TypeNode generic_arg_type = named_node(SEMA_TEST_GENERIC_PARAM_NAME);
    generic_arg_type.type_args = {i32_type_id};
    const TypeId generic_arg_type_id = module.push_type(generic_arg_type);

    syntax::TypeNode scoped_template_type = named_node("Box");
    scoped_template_type.scope_name = "lib";
    scoped_template_type.scope_name_id = module.intern_identifier("lib");
    scoped_template_type.name_id = module.intern_identifier("Box");
    const TypeId scoped_template_type_id = module.push_type(scoped_template_type);
    const TypeId plain_type_id = module.push_type(named_node("Plain"));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);

    sema::SemanticAnalyzerCore::GenericContext generic_context = analyzer.make_generic_context();
    const IdentId generic_id = intern_identifier(analyzer, SEMA_TEST_GENERIC_PARAM_NAME);
    generic_context.params.emplace(generic_id,
        types.generic_param(sema::generic_param_identity_from_text("test.T"), SEMA_TEST_GENERIC_PARAM_NAME));
    analyzer.state_.flow.current_generic_context = &generic_context;
    EXPECT_FALSE(is_valid(analyzer.resolve_type(generic_arg_type_id)));
    analyzer.state_.flow.current_generic_context = nullptr;

    sema::SemanticAnalyzerCore::GenericTemplateInfo box_template =
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Box");
    const auto inserted_box = analyzer.state_.generics.struct_templates.emplace(box_template.key, box_template);
    analyzer.index_generic_struct_template(inserted_box.first->second);

    EXPECT_TRUE(types.is_function(analyzer.resolve_type(invalid_function_type_id)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(scoped_template_type_id)));
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Plain", i32));
    EXPECT_TRUE(types.same(
        analyzer.resolve_named_type(syntax::INVALID_TYPE_ID, module.types[plain_type_id.value], false), i32));

    sema::TypeInfo invalid_builtin_info;
    invalid_builtin_info.kind = TypeKind::builtin;
    invalid_builtin_info.builtin = static_cast<BuiltinType>(SEMA_TEST_INVALID_BUILTIN_TYPE_VALUE);
    const TypeHandle invalid_builtin = TypeTableTestAccess::push(types, invalid_builtin_info);
    EXPECT_FALSE(analyzer.integer_literal_fits_type(invalid_builtin, SEMA_TEST_INTEGER_LITERAL_ONE));
    const sema::SemanticAnalyzerCore::TypeAbiLayout invalid_builtin_layout = analyzer.abi_layout(invalid_builtin);
    EXPECT_EQ(invalid_builtin_layout.size, SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(invalid_builtin_layout.align, SEMA_TEST_ABI_MIN_ALIGNMENT);

    sema::TypeInfo invalid_kind_info;
    invalid_kind_info.kind = static_cast<TypeKind>(SEMA_TEST_INVALID_SEMA_TYPE_KIND_VALUE);
    const TypeHandle invalid_kind = TypeTableTestAccess::push(types, invalid_kind_info);
    EXPECT_EQ(analyzer.abi_size(invalid_kind), SEMA_TEST_ABI_INVALID_SIZE);

    const TypeHandle stale_struct_type = types.named_struct("StaleStruct", "StaleStruct", false);
    StructInfo stale_struct;
    stale_struct.name = checked_text(analyzer.state_.checked, "StaleStruct");
    stale_struct.name_id = intern_identifier(analyzer, "StaleStruct");
    stale_struct.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    stale_struct.type = TypeHandle{stale_struct_type.value + SEMA_TEST_STALE_STRUCT_CACHE_OFFSET};
    analyzer.state_.types.struct_infos_by_type[stale_struct_type.value] = &stale_struct;
    EXPECT_EQ(analyzer.find_struct(stale_struct_type), nullptr);
    EXPECT_EQ(analyzer.abi_size(stale_struct_type), SEMA_TEST_ABI_INVALID_SIZE);

    const TypeHandle builtin_tuple = types.tuple({i32, u64});
    const TypeHandle builtin_struct_type = types.named_struct("BuiltinStruct", "BuiltinStruct", false);
    StructInfo builtin_struct;
    builtin_struct.name = checked_text(analyzer.state_.checked, "BuiltinStruct");
    builtin_struct.name_id = intern_identifier(analyzer, "BuiltinStruct");
    builtin_struct.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    builtin_struct.type = builtin_struct_type;
    builtin_struct.fields = {struct_field_info(analyzer, "value", module_id(0), i32)};
    analyzer.state_.checked.structs.emplace(
        semantic_module_key(analyzer, module_id(0), "BuiltinStruct"), builtin_struct);
    static_cast<void>(analyzer.abi_size(builtin_tuple));
    static_cast<void>(analyzer.abi_size(builtin_struct_type));

    const TypeHandle array_i32 = types.array(SEMA_TEST_ARRAY_SLICE_LENGTH, i32);
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::parameter, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::function_type_parameter, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::function_type_return, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::return_value, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::assignment, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::enum_payload, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::enum_payload_argument, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::argument, {}));

    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find("generic type parameter cannot take type arguments"), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_FUNCTION_TYPE_PARAMETER_STORAGE), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_VARIADIC_FUNCTION_TYPE_EXTERN_C_ONLY), std::string::npos);
    EXPECT_NE(messages.find("generic type Box requires type arguments"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxRecordTypeAndAssociatedOwnerEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "one")};

    const TypeId u8_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::u8));
    const TypeId i32_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_UPPER);
    const ExprId lower_hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_LOWER);
    const ExprId upper_hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_UPPER);
    const ExprId bin_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_BIN_LOWER);
    const ExprId upper_bin_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_BIN_UPPER);
    const ExprId invalid_digit_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_BINARY);
    const ExprId invalid_char_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_CHAR);
    const ExprId empty_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_EMPTY);
    const ExprId overflow_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_UNSIGNED_OVERFLOW);
    const ExprId signed_overflow_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_SIGNED_OVERFLOW);
    const ExprId suffixed_u8_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_U8_SUFFIX);
    const ExprId suffixed_i8_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_I8_SUFFIX);
    const ExprId invalid_suffix_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_SUFFIX);
    const ExprId float_overflow_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_OVERFLOW_LITERAL);
    const ExprId separated_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_WITH_SEPARATOR_LITERAL);
    const ExprId invalid_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_INVALID_TRAILING_LITERAL);
    const ExprId suffixed_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_LITERAL_F32_SUFFIX);
    const ExprId invalid_suffix_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_LITERAL_INVALID_SUFFIX);

    syntax::TypeNode scoped_missing_alias_type = named_node("Missing");
    scoped_missing_alias_type.scope_name = "missing";
    const TypeId scoped_missing_alias_type_id = module.push_type(scoped_missing_alias_type);
    syntax::TypeNode scoped_choice_missing_args_type = named_node("Choice");
    scoped_choice_missing_args_type.scope_name = "one";
    const TypeId scoped_choice_missing_args_type_id = module.push_type(scoped_choice_missing_args_type);
    syntax::TypeNode invalid_primitive_type;
    invalid_primitive_type.kind = syntax::TypeKind::primitive;
    invalid_primitive_type.primitive = SEMA_TEST_INVALID_PRIMITIVE_KIND;
    const TypeId invalid_primitive_type_id = module.push_type(invalid_primitive_type);
    syntax::TypeNode scoped_opaque_type = named_node("ScopedOpaque");
    scoped_opaque_type.scope_name = "one";
    const TypeId scoped_opaque_type_id = module.push_type(scoped_opaque_type);

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "Choice";
    enum_item.enum_base_type = u8_type_id;
    enum_item.enum_cases = {
        syntax::EnumCaseDecl{"none", syntax::INVALID_TYPE_ID, {}, SEMA_TEST_INTEGER_LITERAL_ONE, {}},
    };
    const syntax::ItemId enum_item_id = module.push_item(enum_item);
    module.item_modules[enum_item_id.value] = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.checked.pattern_c_name_ids.assign(SEMA_TEST_PATTERN_TRACKED_COUNT, sema::INVALID_IDENT_ID);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle u16 = types.builtin(BuiltinType::u16);
    const TypeHandle u32 = types.builtin(BuiltinType::u32);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);
    const TypeHandle i8 = types.builtin(BuiltinType::i8);
    const TypeHandle i16 = types.builtin(BuiltinType::i16);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle isize = types.builtin(BuiltinType::isize);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle scoped_opaque = types.opaque_struct("one.ScopedOpaque", "one_ScopedOpaque");

    EXPECT_TRUE(analyzer.can_assign(u8, u8, hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u8, u8, lower_hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u16, u16, upper_hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u32, u32, bin_expr));
    EXPECT_TRUE(analyzer.can_assign(u64, u64, upper_bin_expr));
    EXPECT_TRUE(analyzer.can_assign(i8, i8, hex_expr));
    EXPECT_TRUE(analyzer.can_assign(i16, i16, bin_expr));
    EXPECT_TRUE(analyzer.can_assign(isize, isize, bin_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, invalid_digit_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, invalid_char_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, empty_expr));
    EXPECT_FALSE(analyzer.can_assign(u64, u64, overflow_expr));
    EXPECT_FALSE(analyzer.can_assign(i64, i64, signed_overflow_expr));
    EXPECT_TRUE(analyzer.can_assign(u8, u8, suffixed_u8_expr));
    EXPECT_FALSE(analyzer.can_assign(u16, u16, suffixed_u8_expr));
    EXPECT_FALSE(analyzer.can_assign(bool_type, u8, hex_expr));
    const auto analyze_integer_literal_id = [&](const ExprId expr, const TypeHandle expected) {
        const sema::SemanticAnalyzerCore::ExprView view = analyzer.expr_view(expr);
        return analyzer.analyze_integer_literal(expr, view.text, view.range, expected);
    };
    const auto analyze_float_literal_id = [&](const ExprId expr, const TypeHandle expected) {
        const sema::SemanticAnalyzerCore::ExprView view = analyzer.expr_view(expr);
        return analyzer.analyze_float_literal(expr, view.text, view.range, expected);
    };
    EXPECT_TRUE(types.same(analyze_integer_literal_id(suffixed_i8_expr, INVALID_TYPE_HANDLE), i8));
    EXPECT_TRUE(types.same(
        analyze_integer_literal_id(invalid_suffix_expr, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::i32)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(float_overflow_expr_id, types.builtin(BuiltinType::f32)),
        types.builtin(BuiltinType::f32)));
    EXPECT_TRUE(types.same(
        analyze_float_literal_id(separated_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(invalid_float_expr_id, types.builtin(BuiltinType::f64)),
        types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.same(
        analyze_float_literal_id(suffixed_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f32)));
    EXPECT_TRUE(types.same(
        analyze_float_literal_id(invalid_suffix_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.is_str(types.builtin(BuiltinType::str)));

    const TypeHandle zero_align_enum = types.named_enum("ZeroAlign", "ZeroAlign");
    types.set_enum_underlying(zero_align_enum, u8);
    types.set_enum_payload_layout(
        zero_align_enum, u8, SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_SIZE, SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_ALIGN);
    EXPECT_GE(analyzer.abi_size(zero_align_enum), SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_SIZE);

    analyzer.record_pattern_c_name(syntax::INVALID_PATTERN_ID, "ignored");
    analyzer.record_pattern_c_name(syntax::PatternId{SEMA_TEST_PATTERN_FIRST_INDEX}, {});
    analyzer.record_pattern_case_name(syntax::INVALID_PATTERN_ID, "ignored");
    analyzer.record_pattern_case_name(syntax::PatternId{SEMA_TEST_PATTERN_FIRST_INDEX}, {});
    analyzer.merge_pattern_case_names(syntax::INVALID_PATTERN_ID, syntax::PatternId{SEMA_TEST_PATTERN_FIRST_INDEX});
    analyzer.state_.checked.pattern_case_name_ids[SEMA_TEST_PATTERN_SECOND_INDEX].insert(
        analyzer.state_.checked.intern_c_name("from_checked"));
    analyzer.merge_pattern_case_names(
        syntax::PatternId{SEMA_TEST_PATTERN_FIRST_INDEX}, syntax::PatternId{SEMA_TEST_PATTERN_SECOND_INDEX});
    EXPECT_TRUE(analyzer.state_.checked.pattern_case_name_ids[SEMA_TEST_PATTERN_FIRST_INDEX].contains(
        analyzer.state_.checked.intern_c_name("from_checked")));

    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(syntax::INVALID_EXPR_ID, false)));

    const TypeHandle choice_type = types.named_enum("lib.one.Choice", "lib_one_Choice");
    types.set_enum_underlying(choice_type, u8);
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Choice", choice_type));
    static_cast<void>(
        add_named_type(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "ScopedOpaque", scoped_opaque));
    static_cast<void>(
        add_enum_case(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Choice_none", "none", choice_type));

    const ExprId import_alias_expr = push_name(analyzer.ctx_.module, "one");
    const ExprId scoped_enum_id = push_field(analyzer.ctx_.module, import_alias_expr, "Choice");
    analyzer.state_.checked.expr_types.resize(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.resize(analyzer.ctx_.module.exprs.size(), sema::INVALID_IDENT_ID);
    const TypeHandle choice_i32 = analyzer.resolve_associated_type_owner(scoped_enum_id, false);
    EXPECT_TRUE(is_valid(choice_i32));

    const ExprId scoped_missing_type_id = push_field(analyzer.ctx_.module, import_alias_expr, "MissingScoped");
    analyzer.state_.checked.expr_types.resize(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.resize(analyzer.ctx_.module.exprs.size(), sema::INVALID_IDENT_ID);
    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(scoped_missing_type_id, false)));

    const TypeHandle opaque = types.opaque_struct("Opaque", "Opaque");
    analyzer.state_.checked.syntax_type_handles[i32_type_id.value] = opaque;
    EXPECT_TRUE(analyzer.state_.checked.types.same(analyzer.resolve_type(i32_type_id), opaque));
    analyzer.state_.checked.syntax_type_handles[i32_type_id.value] = INVALID_TYPE_HANDLE;
    EXPECT_TRUE(types.is_void(analyzer.resolve_type(invalid_primitive_type_id)));
    EXPECT_TRUE(types.same(analyzer.resolve_type(scoped_opaque_type_id), scoped_opaque));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(scoped_missing_alias_type_id)));
    EXPECT_TRUE(types.same(analyzer.resolve_type(scoped_choice_missing_args_type_id), choice_type));

    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle array_record = types.named_struct("ArrayRecord", "ArrayRecord", true);
    types.set_record_contains_array(array_record, true);
    static_cast<void>(add_named_type(analyzer, module_id(0), "Record", record_type));
    static_cast<void>(add_global_value(analyzer, module_id(0), "array_value", array_record, SymbolKind::local));

    FunctionSignature static_method = indexed_function_signature(analyzer, "needs", module_id(0), bool_type);
    static_method.is_method = true;
    static_method.method_owner_type = record_type;
    static_cast<void>(add_method(analyzer, static_method, record_type));
    FunctionSignature needs_arg = indexed_function_signature(analyzer, "needs_arg", module_id(0), bool_type);
    needs_arg.is_method = true;
    needs_arg.method_owner_type = record_type;
    needs_arg.param_types = {u8};
    static_cast<void>(add_method(analyzer, needs_arg, record_type));
    FunctionSignature takes_array = indexed_function_signature(analyzer, "takes_array", module_id(0), bool_type);
    takes_array.is_method = true;
    takes_array.method_owner_type = record_type;
    takes_array.param_types = {array_record};
    static_cast<void>(add_method(analyzer, takes_array, record_type));

    const ExprId record_name = push_name(module, "Record");
    const ExprId static_method_field_id = push_field(module, record_name, "needs");
    const ExprId static_method_call_id = push_call(module, static_method_field_id);

    const ExprId missing_arg_field_id = push_field(module, record_name, "needs_arg");
    const ExprId missing_arg_call_id = push_call(module, missing_arg_field_id);

    const ExprId array_value = push_name(module, "array_value");
    const ExprId array_arg_field_id = push_field(module, record_name, "takes_array");
    const ExprId array_arg_call_id = push_call(module, array_arg_field_id, {array_value});

    const ExprId choice_name = push_name(module, "Choice");
    const ExprId none_field_id = push_field(module, choice_name, "none");
    const ExprId none_call_id = push_call(module, none_field_id);

    analyzer.state_.checked.expr_types.resize(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.resize(module.exprs.size(), sema::INVALID_IDENT_ID);
    static_cast<void>(analyzer.analyze_call_expr(
        static_method_call_id, analyzer.expr_view(static_method_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(
        analyzer.analyze_call_expr(missing_arg_call_id, analyzer.expr_view(missing_arg_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(
        analyzer.analyze_call_expr(array_arg_call_id, analyzer.expr_view(array_arg_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(analyzer.analyze_call_expr(none_call_id, analyzer.expr_view(none_call_id), choice_i32));
}

TEST(CoreUnit, SemanticWhiteBoxMatchEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId choice_value = push_name(module, "choice");
    const ExprId int_guard = push_integer(module);
    const ExprId bool_result = push_bool(module, "true");
    const ExprId bool_subject = push_bool(module, "false");
    const ExprId int_result = push_integer(module);
    const ExprId void_value = push_name(module, "void_value");

    syntax::PatternNode payload_pattern;
    payload_pattern.kind = syntax::PatternKind::enum_case;
    payload_pattern.scoped = true;
    payload_pattern.case_name = "some";
    payload_pattern.binding_names = {"payload"};
    const syntax::PatternId payload_pattern_id = module.push_pattern(payload_pattern);

    syntax::PatternNode true_binding_pattern;
    true_binding_pattern.kind = syntax::PatternKind::literal;
    true_binding_pattern.case_name = "true";
    true_binding_pattern.binding_names = {"flag"};
    const syntax::PatternId true_binding_pattern_id = module.push_pattern(true_binding_pattern);

    syntax::PatternNode wildcard_pattern;
    wildcard_pattern.kind = syntax::PatternKind::wildcard;
    const syntax::PatternId wildcard_pattern_id = module.push_pattern(wildcard_pattern);

    syntax::PatternNode unsupported_literal_pattern;
    unsupported_literal_pattern.kind = syntax::PatternKind::literal;
    unsupported_literal_pattern.case_name = "1";
    const syntax::PatternId unsupported_literal_pattern_id = module.push_pattern(unsupported_literal_pattern);

    syntax::PatternNode missing_const_pattern;
    missing_const_pattern.kind = syntax::PatternKind::const_;
    missing_const_pattern.binding_name = "MISSING";
    const syntax::PatternId missing_const_pattern_id = module.push_pattern(missing_const_pattern);

    const ExprId enum_match_id = module.push_match_expr({},
        syntax::MatchExprPayload{
            choice_value,
            {syntax::MatchArm{payload_pattern_id, int_guard, bool_result, {}}},
        });

    const ExprId binding_value_match_id = module.push_match_expr({},
        syntax::MatchExprPayload{
            bool_subject,
            {
                syntax::MatchArm{true_binding_pattern_id, syntax::INVALID_EXPR_ID, int_result, {}},
                syntax::MatchArm{wildcard_pattern_id, syntax::INVALID_EXPR_ID, int_result, {}},
            },
        });

    const ExprId void_match_id = module.push_match_expr({},
        syntax::MatchExprPayload{
            bool_subject,
            {syntax::MatchArm{wildcard_pattern_id, syntax::INVALID_EXPR_ID, void_value, {}}},
        });

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.checked.pattern_c_name_ids.assign(module.patterns.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle choice_type = types.named_enum("Choice", "Choice");
    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    types.set_enum_underlying(choice_type, u8);

    const EnumCaseInfo* const some_case =
        add_enum_case(analyzer, module_id(0), "some", "some", choice_type, i32, {i32}).second;
    ASSERT_NE(some_case, nullptr);
    static_cast<void>(add_named_type(analyzer, module_id(0), "Choice", choice_type));
    static_cast<void>(add_global_value(analyzer, module_id(0), "choice", choice_type, SymbolKind::local));
    static_cast<void>(add_global_value(analyzer, module_id(0), "void_value", void_type, SymbolKind::local));

    std::vector<sema::SemanticAnalyzerCore::PatternBinding> invalid_bindings;
    EXPECT_FALSE(analyzer.analyze_pattern(syntax::INVALID_PATTERN_ID, choice_type, invalid_bindings));
    EXPECT_FALSE(analyzer.pattern_is_irrefutable(syntax::INVALID_PATTERN_ID, choice_type));
    std::vector<sema::SemanticAnalyzerCore::PatternBinding> missing_const_bindings;
    EXPECT_FALSE(analyzer.analyze_pattern(missing_const_pattern_id, i32, missing_const_bindings));

    syntax::PatternNode scoped_name_pattern;
    scoped_name_pattern.kind = syntax::PatternKind::enum_case;
    scoped_name_pattern.scoped = true;
    scoped_name_pattern.enum_name = "Choice";
    scoped_name_pattern.case_name = "some";
    const syntax::PatternId scoped_name_pattern_id = module.push_pattern(scoped_name_pattern);
    syntax::PatternNode missing_payload_pattern;
    missing_payload_pattern.kind = syntax::PatternKind::enum_case;
    missing_payload_pattern.case_name = "missing";
    missing_payload_pattern.payload_patterns = {wildcard_pattern_id};
    const syntax::PatternId missing_payload_pattern_id = module.push_pattern(missing_payload_pattern);

    analyzer.state_.checked.pattern_c_name_ids.resize(module.patterns.size(), sema::INVALID_IDENT_ID);
    std::vector<sema::SemanticAnalyzerCore::PatternBinding> scoped_name_bindings;
    EXPECT_FALSE(analyzer.analyze_pattern(scoped_name_pattern_id, choice_type, scoped_name_bindings));
    std::vector<sema::SemanticAnalyzerCore::PatternBinding> missing_payload_bindings;
    EXPECT_FALSE(analyzer.analyze_pattern(missing_payload_pattern_id, choice_type, missing_payload_bindings));

    EXPECT_TRUE(types.is_bool(
        analyzer.analyze_match_expr(enum_match_id, analyzer.expr_view(enum_match_id), INVALID_TYPE_HANDLE)));
    EXPECT_TRUE(types.is_integer(analyzer.analyze_match_expr(
        binding_value_match_id, analyzer.expr_view(binding_value_match_id), INVALID_TYPE_HANDLE)));
    EXPECT_FALSE(
        is_valid(analyzer.analyze_match_expr(void_match_id, analyzer.expr_view(void_match_id), INVALID_TYPE_HANDLE)));

    bool covered_true = false;
    bool covered_false = false;
    bool value_saw_wildcard = false;
    analyzer.analyze_single_value_pattern(
        syntax::INVALID_PATTERN_ID, types.builtin(BuiltinType::bool_), covered_true, covered_false, value_saw_wildcard);
    analyzer.analyze_single_value_pattern(
        unsupported_literal_pattern_id, record_type, covered_true, covered_false, value_saw_wildcard);
    value_saw_wildcard = true;
    analyzer.analyze_single_value_pattern(unsupported_literal_pattern_id, types.builtin(BuiltinType::bool_),
        covered_true, covered_false, value_saw_wildcard);
    value_saw_wildcard = false;
    analyzer.analyze_single_value_pattern(
        wildcard_pattern_id, types.builtin(BuiltinType::bool_), covered_true, covered_false, value_saw_wildcard);
    EXPECT_TRUE(value_saw_wildcard);
    analyzer.analyze_single_value_pattern(
        payload_pattern_id, types.builtin(BuiltinType::bool_), covered_true, covered_false, value_saw_wildcard);
}

TEST(CoreUnit, SemanticWhiteBoxMatchGuardTruthAndU8FiniteDomain)
{
    {
        syntax::AstModule module;
        module.modules = {module_info({"root"})};
        const ExprId subject = push_name(module, "flag");
        const ExprId one = push_integer(module);
        const ExprId zero = push_integer_text(module, "0");
        const ExprId true_guard = push_bool(module, "true");

        syntax::PatternNode true_pattern;
        true_pattern.kind = syntax::PatternKind::literal;
        true_pattern.case_name = "true";
        const syntax::PatternId true_pattern_id = module.push_pattern(true_pattern);

        syntax::PatternNode false_pattern;
        false_pattern.kind = syntax::PatternKind::literal;
        false_pattern.case_name = "false";
        const syntax::PatternId false_pattern_id = module.push_pattern(false_pattern);

        const ExprId match_id = module.push_match_expr({},
            syntax::MatchExprPayload{
                subject,
                {
                    syntax::MatchArm{true_pattern_id, true_guard, one, {}},
                    syntax::MatchArm{false_pattern_id, syntax::INVALID_EXPR_ID, zero, {}},
                },
            });

        base::DiagnosticSink diagnostics;
        sema::SemanticAnalyzerCore analyzer(module, diagnostics);
        analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
        analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
        analyzer.state_.flow.current_module = module_id(0);
        const TypeHandle bool_type = analyzer.state_.checked.types.builtin(BuiltinType::bool_);
        EXPECT_TRUE(analyzer.state_.names.symbols.insert(
            indexed_symbol(analyzer, SymbolKind::local, "flag", module_id(0), bool_type), diagnostics));

        EXPECT_TRUE(is_valid(analyzer.analyze_match_expr(match_id, analyzer.expr_view(match_id), INVALID_TYPE_HANDLE)));
        EXPECT_FALSE(diagnostics.has_error());
    }

    {
        syntax::AstModule module;
        module.modules = {module_info({"root"})};
        const ExprId subject = push_name(module, "flag");
        const ExprId one = push_integer(module);
        const ExprId zero = push_integer_text(module, "0");
        const ExprId false_guard = push_bool(module, "false");

        syntax::PatternNode true_pattern;
        true_pattern.kind = syntax::PatternKind::literal;
        true_pattern.case_name = "true";
        const syntax::PatternId true_pattern_id = module.push_pattern(true_pattern);

        syntax::PatternNode false_pattern;
        false_pattern.kind = syntax::PatternKind::literal;
        false_pattern.case_name = "false";
        const syntax::PatternId false_pattern_id = module.push_pattern(false_pattern);

        const ExprId match_id = module.push_match_expr({},
            syntax::MatchExprPayload{
                subject,
                {
                    syntax::MatchArm{true_pattern_id, false_guard, one, {}},
                    syntax::MatchArm{false_pattern_id, syntax::INVALID_EXPR_ID, zero, {}},
                },
            });

        base::DiagnosticSink diagnostics;
        sema::SemanticAnalyzerCore analyzer(module, diagnostics);
        analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
        analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
        analyzer.state_.flow.current_module = module_id(0);
        const TypeHandle bool_type = analyzer.state_.checked.types.builtin(BuiltinType::bool_);
        EXPECT_TRUE(analyzer.state_.names.symbols.insert(
            indexed_symbol(analyzer, SymbolKind::local, "flag", module_id(0), bool_type), diagnostics));

        static_cast<void>(analyzer.analyze_match_expr(match_id, analyzer.expr_view(match_id), INVALID_TYPE_HANDLE));
        ASSERT_TRUE(diagnostics.has_error());
        std::string messages;
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            messages += diagnostic.message;
            messages += '\n';
        }
        EXPECT_NE(messages.find("match expression over integer or bool requires a wildcard arm"), std::string::npos);
    }

    {
        syntax::AstModule module;
        module.modules = {module_info({"root"})};
        const ExprId subject = push_name(module, "byte");
        const ExprId result = push_integer(module);
        std::vector<syntax::MatchArm> arms;
        std::vector<std::string> pattern_names;
        arms.reserve(SEMA_TEST_U8_DOMAIN_SIZE);
        pattern_names.reserve(SEMA_TEST_U8_DOMAIN_SIZE);
        for (base::u32 value = 0; value < SEMA_TEST_U8_DOMAIN_SIZE; ++value) {
            pattern_names.push_back(std::to_string(value));
            syntax::PatternNode pattern;
            pattern.kind = syntax::PatternKind::literal;
            pattern.case_name = pattern_names.back();
            const syntax::PatternId pattern_id = module.push_pattern(pattern);
            arms.push_back(syntax::MatchArm{pattern_id, syntax::INVALID_EXPR_ID, result, {}});
        }

        const ExprId match_id = module.push_match_expr({}, subject, std::move(arms));

        base::DiagnosticSink diagnostics;
        sema::SemanticAnalyzerCore analyzer(module, diagnostics);
        analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
        analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
        analyzer.state_.flow.current_module = module_id(0);
        const TypeHandle u8 = analyzer.state_.checked.types.builtin(BuiltinType::u8);
        EXPECT_TRUE(analyzer.state_.names.symbols.insert(
            indexed_symbol(analyzer, SymbolKind::local, "byte", module_id(0), u8), diagnostics));

        EXPECT_TRUE(is_valid(analyzer.analyze_match_expr(match_id, analyzer.expr_view(match_id), INVALID_TYPE_HANDLE)));
        EXPECT_FALSE(diagnostics.has_error());
    }
}

TEST(CoreUnit, SemanticWhiteBoxMatchUsefulnessFocusedEdges)
{
    {
        syntax::AstModule module;
        module.modules = {module_info({"root"})};
        const ExprId subject = push_name(module, "items");
        const ExprId result = push_integer(module);

        syntax::PatternNode false_pattern;
        false_pattern.kind = syntax::PatternKind::literal;
        false_pattern.case_name = "false";
        const syntax::PatternId false_pattern_id = module.push_pattern(false_pattern);

        syntax::PatternNode huge_slice_pattern;
        huge_slice_pattern.kind = syntax::PatternKind::slice;
        huge_slice_pattern.elements.assign(SEMA_TEST_SLICE_PATTERN_OVERFLOW_ELEMENT_COUNT, false_pattern_id);
        const syntax::PatternId huge_slice_pattern_id = module.push_pattern(huge_slice_pattern);

        const ExprId match_id = module.push_match_expr({},
            syntax::MatchExprPayload{
                subject,
                {syntax::MatchArm{huge_slice_pattern_id, syntax::INVALID_EXPR_ID, result, {}}},
            });

        base::DiagnosticSink diagnostics;
        sema::SemanticAnalyzerCore analyzer(module, diagnostics);
        prepare_expr_storage(analyzer, module);
        analyzer.state_.flow.current_module = module_id(0);
        const TypeHandle bool_type = analyzer.state_.checked.types.builtin(BuiltinType::bool_);
        const TypeHandle slice_type = analyzer.state_.checked.types.slice(PointerMutability::const_, bool_type);
        EXPECT_TRUE(analyzer.state_.names.symbols.insert(
            indexed_symbol(analyzer, SymbolKind::local, "items", module_id(0), slice_type), diagnostics));

        EXPECT_TRUE(is_valid(analyzer.analyze_match_expr(match_id, analyzer.expr_view(match_id), INVALID_TYPE_HANDLE)));
        EXPECT_TRUE(diagnostics.has_error());
    }

    {
        syntax::AstModule module;
        module.modules = {module_info({"root"})};
        const ExprId subject = push_name(module, "flag");
        const ExprId result = push_integer(module);

        syntax::PatternNode true_pattern;
        true_pattern.kind = syntax::PatternKind::literal;
        true_pattern.case_name = "true";
        const syntax::PatternId true_pattern_id = module.push_pattern(true_pattern);

        syntax::PatternNode false_pattern;
        false_pattern.kind = syntax::PatternKind::literal;
        false_pattern.case_name = "false";
        const syntax::PatternId false_pattern_id = module.push_pattern(false_pattern);

        syntax::PatternNode or_pattern;
        or_pattern.kind = syntax::PatternKind::or_pattern;
        or_pattern.alternatives = {true_pattern_id, false_pattern_id};
        const syntax::PatternId or_pattern_id = module.push_pattern(or_pattern);

        const ExprId match_id = module.push_match_expr({},
            syntax::MatchExprPayload{
                subject,
                {syntax::MatchArm{or_pattern_id, syntax::INVALID_EXPR_ID, result, {}}},
            });

        base::DiagnosticSink diagnostics;
        sema::SemanticAnalyzerCore analyzer(module, diagnostics);
        prepare_expr_storage(analyzer, module);
        analyzer.state_.checked.pattern_c_name_ids.assign(module.patterns.size(), sema::INVALID_IDENT_ID);
        analyzer.state_.flow.current_module = module_id(0);
        const TypeHandle bool_type = analyzer.state_.checked.types.builtin(BuiltinType::bool_);
        EXPECT_TRUE(analyzer.state_.names.symbols.insert(
            indexed_symbol(analyzer, SymbolKind::local, "flag", module_id(0), bool_type), diagnostics));

        EXPECT_TRUE(is_valid(analyzer.analyze_match_expr(match_id, analyzer.expr_view(match_id), INVALID_TYPE_HANDLE)));
        EXPECT_FALSE(diagnostics.has_error());
    }

    {
        syntax::AstModule module;
        module.modules = {module_info({"root"})};
        const ExprId subject = push_name(module, "choice");
        const ExprId result = push_integer(module);

        syntax::PatternNode left_pattern;
        left_pattern.kind = syntax::PatternKind::enum_case;
        left_pattern.case_name = "left";
        const syntax::PatternId left_pattern_id = module.push_pattern(left_pattern);

        const ExprId match_id = module.push_match_expr({},
            syntax::MatchExprPayload{
                subject,
                {syntax::MatchArm{left_pattern_id, syntax::INVALID_EXPR_ID, result, {}}},
            });

        base::DiagnosticSink diagnostics;
        sema::SemanticAnalyzerCore analyzer(module, diagnostics);
        prepare_expr_storage(analyzer, module);
        analyzer.state_.checked.pattern_c_name_ids.assign(module.patterns.size(), sema::INVALID_IDENT_ID);
        analyzer.state_.flow.current_module = module_id(0);
        const TypeHandle u8 = analyzer.state_.checked.types.builtin(BuiltinType::u8);
        const TypeHandle choice_type = analyzer.state_.checked.types.named_enum("Choice", "Choice");
        analyzer.state_.checked.types.set_enum_underlying(choice_type, u8);
        const EnumCaseInfo* const left_case =
            add_enum_case(analyzer, module_id(0), "Choice_left", "left", choice_type).second;
        const EnumCaseInfo* const right_case =
            add_enum_case(analyzer, module_id(0), "Choice_right", "right", choice_type).second;
        ASSERT_NE(left_case, nullptr);
        ASSERT_NE(right_case, nullptr);
        analyzer.state_.names.enum_cases_by_type.clear();
        static_cast<void>(add_global_value(analyzer, module_id(0), "choice", choice_type, SymbolKind::local));

        EXPECT_TRUE(is_valid(analyzer.analyze_match_expr(match_id, analyzer.expr_view(match_id), INVALID_TYPE_HANDLE)));
        const std::string messages = diagnostic_messages(diagnostics);
        EXPECT_NE(messages.find("not exhaustive for enum case"), std::string::npos);
    }
}

TEST(CoreUnit, SemanticWhiteBoxConstEvaluationTraversal)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), SEMA_TEST_IMPORT_ALIAS_ONE),
    };

    const ExprId scoped_value_expr = push_name(module, SEMA_TEST_CONST_VALUE_NAME, SEMA_TEST_IMPORT_ALIAS_ONE);

    const ExprId field_expr_id = module.push_field_expr({}, syntax::FieldExprPayload{});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.checked.enum_cases.clear();
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const Symbol* const_value_symbol = add_global_value(
        analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), SEMA_TEST_CONST_VALUE_NAME, i32, SymbolKind::const_)
                                           .second;
    ASSERT_NE(const_value_symbol, nullptr);
    const sema::ModuleLookupKey const_value_key{
        const_value_symbol->module.value,
        const_value_symbol->name_id,
    };

    sema::SemaSet<sema::ModuleLookupKey, sema::ModuleLookupKeyHash> dependencies =
        sema::make_sema_set<sema::ModuleLookupKey, sema::ModuleLookupKeyHash>(
            *analyzer.state_.arena, sema::ModuleLookupKeyHash{});
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(scoped_value_expr, dependencies));
    EXPECT_EQ(dependencies.count(const_value_key), 1u);

    sema::EnumCaseInfo case_info;
    case_info.c_name = analyzer.state_.checked.intern_text(SEMA_TEST_ENUM_CASE_C_NAME);
    case_info.name = analyzer.state_.checked.intern_text(SEMA_TEST_ENUM_CASE_C_NAME);
    case_info.name_id = intern_identifier(analyzer, SEMA_TEST_ENUM_CASE_C_NAME);
    case_info.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    analyzer.state_.checked.enum_cases.emplace(
        sema::ModuleLookupKey{case_info.module.value, case_info.name_id}, case_info);
    analyzer.state_.checked.expr_c_name_ids[field_expr_id.value] =
        analyzer.state_.checked.intern_c_name(SEMA_TEST_ENUM_CASE_C_NAME);

    dependencies.clear();
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(field_expr_id, dependencies));
    EXPECT_TRUE(dependencies.empty());
}

TEST(CoreUnit, SemanticWhiteBoxConstEvaluationRejectsUnsupportedShapes)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
    };

    const ExprId missing_name = push_name(module, SEMA_TEST_MISSING_VALUE_NAME);
    const ExprId local_name = push_name(module, SEMA_TEST_LOCAL_VALUE_NAME);
    const ExprId enum_name = push_name(module, SEMA_TEST_ENUM_VALUE_NAME);
    const ExprId integer_literal = push_integer(module);

    const ExprId unsupported_unary_id = push_unary(module, syntax::UnaryOp::address_of, integer_literal);
    const ExprId invalid_child_cast_id = module.push_cast_like_expr(
        syntax::ExprKind::cast, {}, syntax::CastExprPayload{syntax::INVALID_TYPE_ID, syntax::INVALID_EXPR_ID});
    const ExprId empty_struct_literal_id = module.push_struct_literal_expr({}, syntax::StructLiteralExprPayload{});
    const ExprId invalid_binary_id = push_binary(module, SEMA_TEST_INVALID_BINARY_OP, integer_literal, integer_literal);
    const ExprId plain_field_id = module.push_field_expr({}, syntax::FieldExprPayload{});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    static_cast<void>(add_global_value(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_LOCAL_VALUE_NAME, i32, SymbolKind::local));
    static_cast<void>(add_global_value(
        analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_ENUM_VALUE_NAME, i32, SymbolKind::enum_case));

    sema::SemaSet<sema::ModuleLookupKey, sema::ModuleLookupKeyHash> dependencies =
        sema::make_sema_set<sema::ModuleLookupKey, sema::ModuleLookupKeyHash>(
            *analyzer.state_.arena, sema::ModuleLookupKeyHash{});
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(syntax::INVALID_EXPR_ID, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(missing_name, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(local_name, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(enum_name, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(unsupported_unary_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(invalid_child_cast_id, dependencies));
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(empty_struct_literal_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(invalid_binary_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(plain_field_id, dependencies));
    EXPECT_TRUE(dependencies.empty());
}

TEST(CoreUnit, SemanticWhiteBoxTypeTableUnknownDisplayFallbacks)
{
    sema::TypeTable builtin_table;
    const TypeHandle builtin_type = builtin_table.builtin(BuiltinType::i32);
    ASSERT_LT(builtin_type.value, TypeTableTestAccess::entries(builtin_table).size());
    TypeTableTestAccess::entries(builtin_table)[builtin_type.value].builtin = SEMA_TEST_INVALID_BUILTIN_TYPE;
    EXPECT_EQ(builtin_table.display_name(builtin_type), SEMA_TEST_UNKNOWN_TYPE_DISPLAY);

    sema::TypeTable kind_table;
    const TypeHandle kind_type = kind_table.builtin(BuiltinType::i32);
    ASSERT_LT(kind_type.value, TypeTableTestAccess::entries(kind_table).size());
    TypeTableTestAccess::entries(kind_table)[kind_type.value].kind = SEMA_TEST_INVALID_TYPE_KIND;
    EXPECT_EQ(kind_table.display_name(kind_type), SEMA_TEST_UNKNOWN_TYPE_DISPLAY);

    const TypeHandle out_of_range_type{static_cast<base::u32>(TypeTableTestAccess::entries(kind_table).size())};
    EXPECT_FALSE(kind_table.is_integer(out_of_range_type));
    EXPECT_FALSE(kind_table.is_float(out_of_range_type));
    EXPECT_FALSE(kind_table.is_bool(out_of_range_type));
    EXPECT_FALSE(kind_table.is_str(out_of_range_type));
    EXPECT_FALSE(kind_table.is_char(out_of_range_type));
    EXPECT_FALSE(kind_table.is_void(out_of_range_type));
    EXPECT_FALSE(kind_table.is_pointer(out_of_range_type));
    EXPECT_FALSE(kind_table.is_reference(out_of_range_type));
    EXPECT_FALSE(kind_table.is_array(out_of_range_type));
    EXPECT_FALSE(kind_table.is_slice(out_of_range_type));
    EXPECT_FALSE(kind_table.is_tuple(out_of_range_type));
    EXPECT_FALSE(kind_table.is_function(out_of_range_type));
    EXPECT_FALSE(kind_table.contains_array(out_of_range_type));
    EXPECT_EQ(kind_table.display_name(out_of_range_type), SEMA_TEST_INVALID_TYPE_DISPLAY);
    EXPECT_EQ(kind_table.c_name(out_of_range_type), "void");

    EXPECT_FALSE(kind_table.is_integer(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_float(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_bool(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_str(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_char(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_void(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_pointer(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_reference(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_array(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_slice(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_tuple(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_function(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.contains_array(INVALID_TYPE_HANDLE));

    EXPECT_FALSE(kind_table.is_integer(builtin_type));
    EXPECT_FALSE(kind_table.is_float(builtin_type));
    EXPECT_FALSE(kind_table.is_bool(builtin_type));
    EXPECT_FALSE(kind_table.is_str(builtin_type));
    EXPECT_FALSE(kind_table.is_char(builtin_type));
    EXPECT_FALSE(kind_table.is_void(builtin_type));
    EXPECT_FALSE(kind_table.is_pointer(builtin_type));
    EXPECT_FALSE(kind_table.is_reference(builtin_type));
    EXPECT_FALSE(kind_table.is_array(builtin_type));
    EXPECT_FALSE(kind_table.is_slice(builtin_type));
    EXPECT_FALSE(kind_table.is_tuple(builtin_type));
    EXPECT_FALSE(kind_table.is_function(builtin_type));
    EXPECT_FALSE(kind_table.contains_array(builtin_type));

    sema::TypeTable storage_table;
    const TypeHandle i32 = storage_table.builtin(BuiltinType::i32);
    const TypeHandle bool_type = storage_table.builtin(BuiltinType::bool_);
    const TypeHandle pointer = storage_table.pointer(PointerMutability::const_, i32);
    const TypeHandle reference = storage_table.reference(PointerMutability::mut, bool_type);
    const std::array<std::string_view, 2> empty_origin_names{"", ""};
    const TypeHandle empty_origin_reference =
        storage_table.reference(PointerMutability::const_, i32, empty_origin_names);
    const TypeHandle plain_reference = storage_table.reference(PointerMutability::const_, i32);
    const TypeHandle array = storage_table.array(4, i32);
    const TypeHandle slice = storage_table.slice(PointerMutability::const_, i32);
    const TypeHandle tuple = storage_table.tuple({i32, bool_type});
    const TypeHandle function = storage_table.function(sema::FunctionCallConv::aurex, false, {i32, pointer}, bool_type);
    const TypeHandle unnamed_struct = storage_table.named_struct("NoCName", "", false);
    const std::array<TypeHandle, 1> span_function_params{i32};
    const TypeHandle span_function = storage_table.function(
        sema::FunctionCallConv::aurex, true, std::span<const TypeHandle>(span_function_params), bool_type);
    const TypeHandle generic = storage_table.generic_param(sema::generic_param_identity_from_text("test.T"), "T");
    EXPECT_FALSE(is_valid(storage_table.generic_param(sema::INVALID_GENERIC_PARAM_IDENTITY, "Invalid")));
    sema::TypeInfo invalid_generic_info;
    invalid_generic_info.kind = TypeKind::generic_param;
    invalid_generic_info.generic_identity = sema::INVALID_GENERIC_PARAM_IDENTITY;
    const TypeHandle invalid_generic = TypeTableTestAccess::push(storage_table, invalid_generic_info);
    EXPECT_LT(invalid_generic.value, TypeTableTestAccess::entries(storage_table).size());
    storage_table.set_generic_instance(tuple, "tuple.origin", {generic});
    storage_table.set_record_contains_array(tuple, true);
    storage_table.set_enum_underlying(function, i32);
    storage_table.set_enum_payload_layout(function, array, 8, 4);
    EXPECT_EQ(empty_origin_reference.value, plain_reference.value);
    EXPECT_EQ(storage_table.display_name("Plain", std::span<const TypeHandle>{}), "Plain");
    EXPECT_EQ(storage_table.c_name(unnamed_struct), "NoCName");

    sema::TypeTable* const storage_table_alias = &storage_table;
    storage_table = *storage_table_alias;
    EXPECT_TRUE(storage_table.is_tuple(tuple));
    storage_table = std::move(*storage_table_alias);
    EXPECT_TRUE(storage_table.is_function(function));

    sema::TypeTable copied_table(storage_table);
    EXPECT_EQ(copied_table.display_name(pointer), "*const i32");
    EXPECT_TRUE(copied_table.is_reference(reference));
    EXPECT_TRUE(copied_table.is_slice(slice));
    EXPECT_TRUE(copied_table.is_function(span_function));
    EXPECT_LT(invalid_generic.value, TypeTableTestAccess::entries(copied_table).size());
    sema::TypeTable assigned_table;
    assigned_table = storage_table;
    EXPECT_TRUE(assigned_table.contains_array(tuple));
    sema::TypeTable moved_table(std::move(copied_table));
    EXPECT_EQ(moved_table.get(function).enum_payload_align, 4U);
    sema::TypeTable move_assigned_table;
    move_assigned_table = std::move(assigned_table);
    EXPECT_TRUE(move_assigned_table.is_array(array));
}

TEST(CoreUnit, IdentifierInternerStableIdsAndNonAllocatingMisses)
{
    IdentifierInterner interner;
    interner.reserve(2);
    EXPECT_GT(interner.arena_blocks(), 0U);
    EXPECT_GT(interner.arena_bytes(), 0U);

    EXPECT_FALSE(sema::is_valid(interner.find("alpha")));
    const IdentId alpha = interner.intern("alpha");
    const IdentId beta = interner.intern("beta");

    EXPECT_TRUE(sema::is_valid(alpha));
    EXPECT_TRUE(sema::is_valid(beta));
    EXPECT_NE(alpha, beta);
    EXPECT_EQ(interner.intern("alpha"), alpha);
    EXPECT_EQ(interner.find("alpha"), alpha);
    EXPECT_EQ(interner.find("beta"), beta);
    EXPECT_EQ(interner.text(alpha), "alpha");
    EXPECT_EQ(interner.text(beta), "beta");
    EXPECT_EQ(interner.stable_hash(alpha), syntax::stable_hash_text("alpha"));
    EXPECT_NE(interner.stable_hash(alpha), interner.stable_hash(beta));
    EXPECT_EQ(interner.text(sema::INVALID_IDENT_ID), "");
    EXPECT_EQ(interner.stable_hash(sema::INVALID_IDENT_ID), syntax::stable_hash_text(""));
    EXPECT_EQ(interner.text(IdentId{IdentId::INVALID_VALUE - 1}), "");
    EXPECT_EQ(interner.find("missing"), sema::INVALID_IDENT_ID);
    EXPECT_EQ(interner.size(), 2U);
    EXPECT_GT(interner.arena_blocks(), 0U);
    EXPECT_GT(interner.arena_bytes(), 0U);

    IdentifierInterner copied = interner;
    EXPECT_EQ(copied.find("alpha"), alpha);
    EXPECT_EQ(copied.text(beta), "beta");
    EXPECT_GT(copied.arena_blocks(), 0U);

    IdentifierInterner assigned;
    assigned = copied;
    EXPECT_EQ(assigned.find("alpha"), alpha);
    EXPECT_EQ(assigned.text(beta), "beta");
    IdentifierInterner* const assigned_ref = &assigned;
    assigned = *assigned_ref;
    EXPECT_EQ(assigned.size(), 2U);
    EXPECT_EQ(assigned.find("beta"), beta);

    IdentifierInterner moved(std::move(assigned));
    EXPECT_EQ(moved.find("alpha"), alpha);
    EXPECT_EQ(moved.text(beta), "beta");
    EXPECT_EQ(assigned.size(), 0U);
    EXPECT_EQ(assigned.find("alpha"), sema::INVALID_IDENT_ID);
    EXPECT_EQ(assigned.arena_blocks(), 0U);

    const IdentId gamma = assigned.intern("gamma");
    EXPECT_EQ(gamma.value, 0U);
    EXPECT_EQ(assigned.find("gamma"), gamma);
    EXPECT_EQ(assigned.text(gamma), "gamma");
    EXPECT_GT(assigned.arena_blocks(), 0U);

    IdentifierInterner move_assigned;
    static_cast<void>(move_assigned.intern("stale"));
    move_assigned = std::move(moved);
    EXPECT_EQ(move_assigned.find("alpha"), alpha);
    EXPECT_EQ(move_assigned.find("stale"), sema::INVALID_IDENT_ID);
    EXPECT_EQ(move_assigned.text(beta), "beta");
    EXPECT_EQ(moved.size(), 0U);
    EXPECT_EQ(moved.arena_blocks(), 0U);
    moved.reserve(1);
    EXPECT_GT(moved.arena_blocks(), 0U);
    EXPECT_EQ(move_assigned.intern(""), sema::INVALID_IDENT_ID);

    IdentifierInterner* const move_assigned_ref = &move_assigned;
    move_assigned = std::move(*move_assigned_ref);
    EXPECT_EQ(move_assigned.find("alpha"), alpha);
}

TEST(CoreUnit, StableSemanticIdsSeparateModulesMembersAndIncrementalKeys)
{
    const std::array<std::string_view, 2> dotted_path{"a", "b_c"};
    const std::array<std::string_view, 2> underscore_path{"a_b", "c"};
    const sema::StableModuleId empty_module = sema::stable_module_id(std::span<const std::string_view>{});
    const sema::StableModuleId dotted_module = sema::stable_module_id(dotted_path);
    const sema::StableModuleId repeated_dotted_module = sema::stable_module_id(dotted_path);
    const sema::StableModuleId underscore_module = sema::stable_module_id(underscore_path);

    EXPECT_EQ(empty_module.part_count, 0U);
    EXPECT_NE(empty_module.global_id, 0U);
    EXPECT_EQ(dotted_module, repeated_dotted_module);
    EXPECT_NE(dotted_module, underscore_module);
    EXPECT_NE(dotted_module.global_id, underscore_module.global_id);

    const sema::StableFingerprint128 empty_text = sema::stable_fingerprint("");
    const sema::StableFingerprint128 non_empty_text = sema::stable_fingerprint("compute");
    EXPECT_EQ(empty_text.byte_count, 0U);
    EXPECT_GT(non_empty_text.byte_count, 0U);
    EXPECT_NE(empty_text, non_empty_text);

    const sema::StableDefId function_id =
        sema::stable_definition_id(dotted_module, sema::StableSymbolKind::function, "compute");
    const sema::StableDefId overloaded_function_id =
        sema::stable_definition_id(dotted_module, sema::StableSymbolKind::function, "compute", 1);
    const sema::StableDefId value_id =
        sema::stable_definition_id(dotted_module, sema::StableSymbolKind::value, "compute");
    EXPECT_NE(function_id, value_id);
    EXPECT_NE(function_id, overloaded_function_id);
    EXPECT_NE(function_id.global_id, value_id.global_id);

    const sema::StableMemberKey x_field =
        sema::stable_member_key(function_id, sema::StableSymbolKind::struct_field, "x");
    const sema::StableMemberKey y_field =
        sema::stable_member_key(function_id, sema::StableSymbolKind::struct_field, "y");
    EXPECT_NE(x_field, y_field);
    EXPECT_NE(x_field.global_id, y_field.global_id);

    const sema::IncrementalKey first_fingerprint = sema::stable_incremental_key(function_id, "i32(i32)");
    const sema::IncrementalKey same_fingerprint = sema::stable_incremental_key(function_id, "i32(i32)");
    const sema::IncrementalKey changed_fingerprint = sema::stable_incremental_key(function_id, "i64(i32)");
    EXPECT_EQ(first_fingerprint, same_fingerprint);
    EXPECT_NE(first_fingerprint, changed_fingerprint);
    EXPECT_EQ(first_fingerprint.definition, function_id);
}

TEST(CoreUnit, SemanticServicesExposeExplicitBoundaryDomains)
{
    syntax::AstModule module;
    module.modules = {module_info({"service_boundary"})};
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);

    sema::SemanticServiceBundle services = analyzer.services();
    EXPECT_EQ(&services.context().module, &analyzer.ctx_.module);
    EXPECT_EQ(&services.context().diagnostics, &diagnostics);

    sema::SemanticLookupService lookup = services.lookup();
    sema::SemanticTypeService type = services.type();
    sema::SemanticGenericService generic = services.generic();
    sema::SemanticBodyCheckService body_check = services.body_check();

    EXPECT_EQ(analyzer.lookup_service().boundary().domain, sema::SemanticServiceDomain::lookup);
    EXPECT_EQ(analyzer.type_service().boundary().domain, sema::SemanticServiceDomain::type);
    EXPECT_EQ(analyzer.generic_service().boundary().domain, sema::SemanticServiceDomain::generic);
    EXPECT_EQ(analyzer.body_check_service().boundary().domain, sema::SemanticServiceDomain::body_check);

    EXPECT_EQ(lookup.boundary().domain, sema::SemanticServiceDomain::lookup);
    EXPECT_EQ(lookup.boundary().name, "lookup");
    EXPECT_FALSE(lookup.boundary().owns_analyzer_state);
    EXPECT_TRUE(lookup.accepts_authority(query::QueryKind::module_exports));
    EXPECT_FALSE(lookup.accepts_authority(query::QueryKind::lower_function_ir));

    EXPECT_EQ(type.boundary().domain, sema::SemanticServiceDomain::type);
    EXPECT_EQ(type.boundary().name, "type");
    EXPECT_TRUE(type.accepts_authority(query::QueryKind::item_signature));
    EXPECT_TRUE(type.accepts_authority(query::QueryKind::type_check_body));

    EXPECT_EQ(generic.boundary().domain, sema::SemanticServiceDomain::generic);
    EXPECT_EQ(generic.boundary().name, "generic");
    EXPECT_TRUE(generic.accepts_authority(query::QueryKind::generic_template_signature));
    EXPECT_TRUE(generic.accepts_authority(query::QueryKind::generic_instance_body));

    EXPECT_EQ(body_check.boundary().domain, sema::SemanticServiceDomain::body_check);
    EXPECT_EQ(body_check.boundary().name, "body_check");
    EXPECT_TRUE(body_check.accepts_authority(query::QueryKind::function_body_syntax));
    EXPECT_TRUE(body_check.accepts_authority(query::QueryKind::type_check_body));

    syntax::ItemNode nongeneric_item;
    EXPECT_FALSE(generic.has_generic_params(nongeneric_item));
    syntax::ItemNode generic_item;
    generic_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    EXPECT_TRUE(generic.has_generic_params(generic_item));

    analyzer.state_.flow.current_module = module_id(0);
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeId i32_type_id = analyzer.ctx_.module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    analyzer.state_.checked.syntax_type_handles.assign(analyzer.ctx_.module.types.size(), INVALID_TYPE_HANDLE);
    EXPECT_TRUE(analyzer.state_.checked.types.same(type.resolve_type(i32_type_id), i32));
    EXPECT_TRUE(type.can_assign(i32, i32, syntax::INVALID_EXPR_ID));
    EXPECT_TRUE(type.is_valid_storage_type(i32));
    type.validate_type_layouts();

    const sema::ModuleLookupKey known_type_key = add_named_type(analyzer, module_id(0), "KnownServiceType", i32);
    const IdentId known_type_id = known_type_key.name;
    EXPECT_TRUE(analyzer.state_.checked.types.same(
        lookup.find_type_in_visible_modules(known_type_id, "KnownServiceType", {}, false, false), i32));

    const sema::FunctionLookupKey known_function_key =
        add_function(analyzer, indexed_function_signature(analyzer, "known_service_fn", module_id(0), i32));
    const FunctionSignature* const known_function =
        lookup.find_function_in_visible_modules(known_function_key.name, "known_service_fn", {}, false);
    ASSERT_NE(known_function, nullptr);
    EXPECT_EQ(known_function->semantic_key, known_function_key);

    const sema::SemanticLookupService& const_lookup = lookup;
    EXPECT_TRUE(const_lookup.visible_modules(syntax::INVALID_MODULE_ID).empty());
    static_cast<void>(type.resolver());
    static_cast<void>(type.validator());
    static_cast<void>(type.abi_checker());
    static_cast<void>(generic.analyzer());
    static_cast<void>(body_check.expression_analyzer());
    static_cast<void>(body_check.statement_analyzer());

    syntax::ItemNode missing_body_function;
    missing_body_function.kind = syntax::ItemKind::fn_decl;
    missing_body_function.name = "missing_body_function";
    missing_body_function.name_id = intern_identifier(analyzer, missing_body_function.name);
    body_check.analyze_function_body(missing_body_function, syntax::INVALID_ITEM_ID);
    sema::SemanticAnalyzerCore::FunctionBodyState analyzed_state =
        sema::SemanticAnalyzerCore::FunctionBodyState::analyzed;
    body_check.analyze_function_body_with_signature(
        missing_body_function, known_function_key, *known_function, analyzed_state);
    EXPECT_EQ(analyzed_state, sema::SemanticAnalyzerCore::FunctionBodyState::analyzed);
}

TEST(CoreUnit, SymbolTableCoversLookupsScopeRemovalAndInvalidIds)
{
    base::DiagnosticSink diagnostics;
    sema::SymbolTable symbols;
    sema::CheckedModule checked;
    IdentifierInterner identifiers;
    const IdentId outer_id = identifiers.intern(SEMA_TEST_SYMBOL_OUTER_NAME);
    const IdentId inner_id = identifiers.intern(SEMA_TEST_SYMBOL_INNER_NAME);
    const IdentId duplicate_id = identifiers.intern(SEMA_TEST_SYMBOL_DUPLICATE_NAME);
    const IdentId missing_id = identifiers.intern("missing_symbol");
    EXPECT_EQ(symbols.find(syntax::INVALID_IDENT_ID), nullptr);

    const auto outer_inserted =
        symbols.insert(symbol(SymbolKind::local, SEMA_TEST_SYMBOL_OUTER_NAME, module_id(0), INVALID_TYPE_HANDLE, false,
                           syntax::Visibility::public_, outer_id, &checked),
            diagnostics);
    ASSERT_TRUE(outer_inserted) << outer_inserted.error().message;
    ASSERT_NE(symbols.find(outer_id), nullptr);
    EXPECT_EQ(symbols.find(missing_id), nullptr);
    EXPECT_EQ(symbols.get(sema::INVALID_SYMBOL_ID), nullptr);
    EXPECT_EQ(symbols.get(sema::SymbolId{1}), nullptr);

    symbols.push_scope();
    const auto inner_inserted =
        symbols.insert(symbol(SymbolKind::local, SEMA_TEST_SYMBOL_INNER_NAME, module_id(0), INVALID_TYPE_HANDLE, false,
                           syntax::Visibility::public_, inner_id, &checked),
            diagnostics);
    ASSERT_TRUE(inner_inserted) << inner_inserted.error().message;
    EXPECT_NE(symbols.find(inner_id), nullptr);
    EXPECT_NE(symbols.find(outer_id), nullptr);
    const auto shadowed_outer_inserted =
        symbols.insert(symbol(SymbolKind::local, SEMA_TEST_SYMBOL_OUTER_NAME, module_id(0), INVALID_TYPE_HANDLE, false,
                           syntax::Visibility::public_, outer_id, &checked),
            diagnostics);
    ASSERT_TRUE(shadowed_outer_inserted) << shadowed_outer_inserted.error().message;
    EXPECT_NE(symbols.find(outer_id), nullptr);
    const IdentId empty_name_id = identifiers.intern("empty_symbol");
    const auto empty_name_inserted =
        symbols.insert(symbol(SymbolKind::local, "empty_symbol", module_id(0), INVALID_TYPE_HANDLE, false,
                           syntax::Visibility::public_, empty_name_id),
            diagnostics);
    ASSERT_TRUE(empty_name_inserted) << empty_name_inserted.error().message;
    std::vector<std::string_view> visible_names;
    symbols.append_visible_names(visible_names);
    EXPECT_EQ(
        std::count(visible_names.begin(), visible_names.end(), SEMA_TEST_SYMBOL_OUTER_NAME), static_cast<std::ptrdiff_t>(1));
    EXPECT_EQ(std::find(visible_names.begin(), visible_names.end(), "empty_symbol"), visible_names.end());

    sema::SymbolTable copied_symbols(symbols);
    EXPECT_NE(copied_symbols.find(inner_id), nullptr);
    EXPECT_NE(copied_symbols.find(outer_id), nullptr);
    sema::SymbolTable& symbols_ref = symbols;
    symbols = symbols_ref;
    EXPECT_NE(symbols.find(inner_id), nullptr);
    sema::SymbolTable assigned_symbols;
    assigned_symbols = symbols;
    EXPECT_NE(assigned_symbols.find(inner_id), nullptr);
    sema::SymbolTable moved_symbols(std::move(copied_symbols));
    EXPECT_NE(moved_symbols.find(outer_id), nullptr);
    sema::SymbolTable move_assigned_symbols;
    move_assigned_symbols = std::move(assigned_symbols);
    EXPECT_NE(move_assigned_symbols.find(inner_id), nullptr);
    sema::SymbolTable& move_assigned_symbols_ref = move_assigned_symbols;
    move_assigned_symbols = std::move(move_assigned_symbols_ref);
    EXPECT_NE(move_assigned_symbols.find(inner_id), nullptr);

    const auto duplicate_name_inserted =
        symbols.insert(symbol(SymbolKind::local, SEMA_TEST_SYMBOL_DUPLICATE_NAME, module_id(0), INVALID_TYPE_HANDLE,
                           false, syntax::Visibility::public_, duplicate_id, &checked),
            diagnostics);
    ASSERT_TRUE(duplicate_name_inserted) << duplicate_name_inserted.error().message;
    const auto duplicate_shadow =
        symbols.insert(symbol(SymbolKind::local, SEMA_TEST_SYMBOL_DUPLICATE_NAME, module_id(0), INVALID_TYPE_HANDLE,
                           false, syntax::Visibility::public_, duplicate_id, &checked),
            diagnostics);
    ASSERT_FALSE(duplicate_shadow);
    EXPECT_EQ(duplicate_shadow.error().code, base::ErrorCode::sema_error);
    EXPECT_TRUE(diagnostics.has_error());

    symbols.pop_scope();
    EXPECT_EQ(symbols.find(inner_id), nullptr);
    ASSERT_NE(symbols.find(outer_id), nullptr);
}

} // namespace aurex::test
