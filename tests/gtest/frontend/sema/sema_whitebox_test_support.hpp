#pragma once

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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

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
        messages += std::string(base::severity_name(diagnostic.severity));
        messages += " ";
        messages += std::string(base::diagnostic_category_name(diagnostic.category));
        messages += " ";
        messages += std::string(base::diagnostic_code_name(diagnostic.code));
        messages += ": ";
        messages += diagnostic.message;
        messages.push_back('\n');
        for (const base::DiagnosticLabel& label : diagnostic.labels) {
            messages += "label: ";
            messages += label.message;
            messages.push_back('\n');
        }
        for (const base::DiagnosticChild& child : diagnostic.children) {
            messages += std::string(base::severity_name(child.severity));
            messages += " ";
            messages += std::string(base::diagnostic_category_name(child.category));
            messages += " ";
            messages += std::string(base::diagnostic_code_name(child.code));
            messages += ": ";
            messages += child.message;
            messages.push_back('\n');
        }
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

} // namespace aurex::test

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
