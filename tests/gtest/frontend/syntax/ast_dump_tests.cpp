#include <aurex/frontend/syntax/core/ast_dump.hpp>
#include <aurex/frontend/syntax/core/module.hpp>
#include <aurex/frontend/syntax/core/token.hpp>
#include <aurex/infrastructure/base/abi.hpp>

#include <support/frontend_test_support.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::test {
namespace {

using syntax::Token;
using syntax::TokenKind;

constexpr std::size_t AST_COMPACT_HEADER_MAX_BYTES = 32;
constexpr std::size_t AST_FAT_NODE_HEADER_RATIO = 4;

[[nodiscard]] syntax::TypeId push_primitive_type(syntax::AstModule& module, const syntax::PrimitiveTypeKind kind)
{
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.primitive = kind;
    return module.push_type(type);
}

[[nodiscard]] syntax::StmtId push_expr_stmt(syntax::AstModule& module, const syntax::ExprId expr)
{
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::expr;
    stmt.init = expr;
    return module.push_stmt(stmt);
}

[[nodiscard]] syntax::ExprId push_name_expr(syntax::AstModule& module, const std::string_view text,
    const std::string_view scope_name = {}, std::vector<syntax::TypeId> type_args = {})
{
    return module.push_name_expr({}, scope_name, {}, text, std::move(type_args));
}

[[nodiscard]] syntax::ExprId push_float_literal(syntax::AstModule& module, const std::string_view text)
{
    return module.push_literal_expr(syntax::ExprKind::float_literal, {}, text);
}

} // namespace

TEST(CoreUnit, AstStorageUsesCompactHeaders)
{
    EXPECT_LE(sizeof(syntax::TypeNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LE(sizeof(syntax::ExprNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LE(sizeof(syntax::PatternNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LE(sizeof(syntax::StmtNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LE(sizeof(syntax::ItemNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LT(sizeof(syntax::TypeNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::TypeNode));
    EXPECT_LT(sizeof(syntax::PatternNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::PatternNode));
    EXPECT_LT(sizeof(syntax::StmtNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::StmtNode));
    EXPECT_LT(sizeof(syntax::ItemNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::ItemNode));
}

TEST(CoreUnit, SyntaxModuleHelpersCoverPathMismatchAndManglePunctuation)
{
    syntax::ModulePath empty_path;
    EXPECT_EQ(syntax::module_path_to_string(empty_path), "");
    EXPECT_EQ(syntax::module_path_to_relative_file(empty_path).generic_string(), ".ax");

    syntax::ModulePath core_path;
    core_path.parts = {"core", "mem"};
    syntax::ModulePath same_core_path;
    same_core_path.parts = {"core", "mem"};
    syntax::ModulePath short_core_path;
    short_core_path.parts = {"core"};
    syntax::ModulePath mismatched_core_path;
    mismatched_core_path.parts = {"core", "io"};

    EXPECT_EQ(syntax::module_path_to_string(core_path), "core.mem");
    EXPECT_EQ(syntax::module_path_to_relative_file(core_path).generic_string(), "core/mem.ax");
    EXPECT_TRUE(syntax::module_paths_equal(core_path, same_core_path));
    EXPECT_FALSE(syntax::module_paths_equal(core_path, short_core_path));
    EXPECT_FALSE(syntax::module_paths_equal(core_path, mismatched_core_path));

    syntax::ModulePath punctuation_path;
    punctuation_path.parts = {"core", "Mem9", "io!"};
    const std::string expected_mangled =
        std::string(base::abi::AUREX_INTERNAL_SYMBOL_PREFIX) + "_core_Mem9_io__run_A9_";
    EXPECT_EQ(syntax::mangle_c_symbol(punctuation_path, "run-A9{"), expected_mangled);
}

TEST(CoreUnit, AstItemNodeListCopiesAndMovesEveryPayloadKind)
{
    syntax::ItemNodeList items;
    items.reserve(10U);
    const base::SourceRange range{base::SourceId{1U}, 10U, 20U};

    auto make_constraint = [&items, range](const std::string_view param_name) {
        syntax::GenericConstraintDecl constraint;
        constraint.param_name = param_name;
        constraint.param_range = range;
        constraint.range = range;
        constraint.param_name_id = syntax::IdentId{1U};
        constraint.capability_names = items.make_list<std::string_view>();
        constraint.capability_names.push_back("Reader");
        constraint.capability_ranges = items.make_list<base::SourceRange>();
        constraint.capability_ranges.push_back(range);
        syntax::AssociatedTypeConstraintDecl associated;
        associated.name = "Item";
        associated.name_range = range;
        associated.value_type = syntax::TypeId{2U};
        associated.range = range;
        associated.name_id = syntax::IdentId{2U};
        constraint.capability_associated_constraints.push_back({associated});
        constraint.capability_name_ids = items.make_list<syntax::IdentId>();
        constraint.capability_name_ids.push_back(syntax::IdentId{3U});
        return constraint;
    };

    auto make_enum_case = [&items, range](const std::string_view name) {
        syntax::EnumCaseDecl enum_case;
        enum_case.name = name;
        enum_case.payload_type = syntax::TypeId{3U};
        enum_case.payload_types = items.make_list<syntax::TypeId>();
        enum_case.payload_types.push_back(syntax::TypeId{4U});
        enum_case.value_text = "7";
        enum_case.range = range;
        enum_case.name_id = syntax::IdentId{4U};
        return enum_case;
    };

    syntax::ItemNode const_item;
    const_item.kind = syntax::ItemKind::const_decl;
    const_item.name = "ANSWER";
    const_item.name_id = syntax::IdentId{5U};
    const_item.const_type = syntax::TypeId{5U};
    const_item.const_value = syntax::ExprId{6U};
    const_item.range = range;
    const syntax::ItemId const_id = items.append(const_item);

    syntax::ItemNode alias_item;
    alias_item.kind = syntax::ItemKind::type_alias;
    alias_item.name = "Alias";
    alias_item.generic_params.push_back(syntax::GenericParamDecl{"T", range, syntax::IdentId{6U}});
    alias_item.where_constraints.push_back(make_constraint("T"));
    alias_item.alias_type = syntax::TypeId{7U};
    alias_item.impl_type = syntax::TypeId{8U};
    alias_item.trait_type = syntax::TypeId{9U};
    alias_item.range = range;
    const syntax::ItemId alias_id = items.append(alias_item);

    syntax::ItemNode struct_item;
    struct_item.kind = syntax::ItemKind::struct_decl;
    struct_item.name = "Box";
    struct_item.derives.push_back(syntax::DeriveDecl{"Copy", syntax::IdentId{20U}, range});
    struct_item.derives.push_back(syntax::DeriveDecl{"Eq", syntax::IdentId{21U}, range});
    struct_item.generic_params.push_back(syntax::GenericParamDecl{"T", range, syntax::IdentId{7U}});
    struct_item.where_constraints.push_back(make_constraint("T"));
    struct_item.fields.push_back(syntax::FieldDecl{"value", syntax::TypeId{10U}, range, syntax::Visibility::public_});
    struct_item.range = range;
    const syntax::ItemId struct_id = items.append(struct_item);

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "Choice";
    enum_item.generic_params.push_back(syntax::GenericParamDecl{"T", range, syntax::IdentId{8U}});
    enum_item.where_constraints.push_back(make_constraint("T"));
    enum_item.enum_base_type = syntax::TypeId{11U};
    enum_item.enum_cases.push_back(make_enum_case("some"));
    enum_item.range = range;
    const syntax::ItemId enum_id = items.append(enum_item);

    syntax::ItemNode opaque_item;
    opaque_item.kind = syntax::ItemKind::opaque_struct_decl;
    opaque_item.name = "Handle";
    opaque_item.name_id = syntax::IdentId{9U};
    opaque_item.range = range;
    const syntax::ItemId opaque_id = items.append(opaque_item);

    syntax::ItemNode trait_item;
    trait_item.kind = syntax::ItemKind::trait_decl;
    trait_item.name = "Reader";
    trait_item.generic_params.push_back(syntax::GenericParamDecl{"T", range, syntax::IdentId{10U}});
    trait_item.where_constraints.push_back(make_constraint("T"));
    trait_item.trait_items.push_back(const_id);
    trait_item.range = range;
    const syntax::ItemId trait_id = items.append(trait_item);

    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "read";
    function_item.generic_params.push_back(syntax::GenericParamDecl{"T", range, syntax::IdentId{11U}});
    function_item.where_constraints.push_back(make_constraint("T"));
    function_item.params.push_back(syntax::ParamDecl{"value", syntax::TypeId{12U}, range, syntax::IdentId{12U}});
    function_item.return_type = syntax::TypeId{13U};
    function_item.body = syntax::StmtId{14U};
    function_item.impl_type = syntax::TypeId{15U};
    function_item.trait_type = syntax::TypeId{16U};
    function_item.abi_name = "abi_read";
    function_item.borrow_contract.present = true;
    function_item.borrow_contract.range = range;
    function_item.borrow_contract.return_selectors.push_back(syntax::BorrowContractSelectorDecl{
        syntax::BorrowContractSelectorKind::parameter,
        "value",
        syntax::IdentId{13U},
        range,
    });
    function_item.is_export_c = true;
    function_item.is_extern_c = true;
    function_item.is_unsafe = true;
    function_item.is_variadic = true;
    function_item.is_prototype = true;
    function_item.is_trait_default_method = true;
    function_item.range = range;
    const syntax::ItemId function_id = items.append(function_item);

    syntax::ItemNode extern_item;
    extern_item.kind = syntax::ItemKind::extern_block;
    extern_item.extern_items.push_back(function_id);
    extern_item.range = range;
    const syntax::ItemId extern_id = items.append(extern_item);

    syntax::ItemNode impl_item;
    impl_item.kind = syntax::ItemKind::impl_block;
    impl_item.generic_params.push_back(syntax::GenericParamDecl{"T", range, syntax::IdentId{14U}});
    impl_item.where_constraints.push_back(make_constraint("T"));
    impl_item.impl_type = syntax::TypeId{17U};
    impl_item.trait_type = syntax::TypeId{18U};
    impl_item.impl_items.push_back(function_id);
    impl_item.range = range;
    const syntax::ItemId impl_id = items.append(impl_item);

    syntax::ItemNode unknown_item;
    unknown_item.kind = static_cast<syntax::ItemKind>(99);
    unknown_item.name = "unknown";
    unknown_item.range = range;
    const syntax::ItemId unknown_id = items.append(unknown_item);

    ASSERT_EQ(items.size(), 10U);
    const syntax::ItemNodeList copied(items);
    EXPECT_EQ(copied[const_id.value].name, "ANSWER");
    EXPECT_EQ(copied[alias_id.value].where_constraints.front().capability_names.front(), "Reader");
    ASSERT_EQ(copied[struct_id.value].derives.size(), 2U);
    EXPECT_EQ(copied[struct_id.value].derives.front().name, "Copy");
    EXPECT_EQ(copied[struct_id.value].fields.front().name, "value");
    EXPECT_EQ(copied[enum_id.value].enum_cases.front().payload_types.front().value, 4U);
    EXPECT_EQ(copied[opaque_id.value].name, "Handle");
    EXPECT_EQ(copied[trait_id.value].trait_items.front().value, const_id.value);
    EXPECT_TRUE(copied[function_id.value].borrow_contract.present);
    EXPECT_EQ(copied[extern_id.value].extern_items.front().value, function_id.value);
    EXPECT_EQ(copied[impl_id.value].impl_items.front().value, function_id.value);
    EXPECT_EQ(copied[unknown_id.value].name, "unknown");

    const syntax::ItemNode* const materialized_opaque = copied.ptr(opaque_id.value);
    ASSERT_NE(materialized_opaque, nullptr);
    EXPECT_EQ(materialized_opaque->name, "Handle");
    EXPECT_EQ(copied.ptr(copied.size()), nullptr);

    syntax::ItemNodeList moved(copied);
    EXPECT_EQ(moved.take(const_id.value).name, "ANSWER");
    const base::SourceRange moved_constraint_range =
        moved.take(alias_id.value).where_constraints.front().capability_ranges.front();
    EXPECT_EQ(moved_constraint_range.source.value, range.source.value);
    EXPECT_EQ(moved_constraint_range.begin, range.begin);
    EXPECT_EQ(moved_constraint_range.end, range.end);
    const syntax::ItemNode moved_struct = moved.take(struct_id.value);
    ASSERT_EQ(moved_struct.derives.size(), 2U);
    EXPECT_EQ(moved_struct.derives.back().name, "Eq");
    EXPECT_EQ(moved_struct.fields.front().visibility, syntax::Visibility::public_);
    EXPECT_EQ(moved.take(enum_id.value).enum_cases.front().name, "some");
    EXPECT_EQ(moved.take(opaque_id.value).name_id, syntax::IdentId{9U});
    EXPECT_EQ(moved.take(trait_id.value).trait_items.front().value, const_id.value);
    EXPECT_TRUE(moved.take(function_id.value).is_trait_default_method);
    EXPECT_EQ(moved.take(extern_id.value).extern_items.front().value, function_id.value);
    EXPECT_EQ(moved.take(impl_id.value).impl_type.value, 17U);
    EXPECT_EQ(moved.take(unknown_id.value).name, "unknown");
}

TEST(CoreUnit, AstTypeNodeListCopiesAndMovesEveryPayloadKind)
{
    syntax::TypeNodeList types;
    types.reserve(10U);
    const base::SourceRange range{base::SourceId{2U}, 30U, 40U};

    syntax::TypeNode primitive_type;
    primitive_type.kind = syntax::TypeKind::primitive;
    primitive_type.primitive = syntax::PrimitiveTypeKind::i64;
    primitive_type.range = range;
    const syntax::TypeId primitive_id = types.append(primitive_type);

    syntax::TypeNode named_type;
    named_type.kind = syntax::TypeKind::named;
    named_type.scope_name = "core";
    named_type.scope_range = range;
    named_type.scope_parts = {"core", "mem"};
    named_type.name = "Buffer";
    named_type.scope_name_id = syntax::IdentId{1U};
    named_type.scope_part_ids = {syntax::IdentId{1U}, syntax::IdentId{2U}};
    named_type.name_id = syntax::IdentId{3U};
    named_type.type_args = {syntax::TypeId{1U}, syntax::TypeId{2U}};
    named_type.range = range;
    const syntax::TypeId named_id = types.append(named_type);

    syntax::TypeNode pointer_type;
    pointer_type.kind = syntax::TypeKind::pointer;
    pointer_type.pointer_mutability = syntax::PointerMutability::mut;
    pointer_type.pointee = named_id;
    pointer_type.range = range;
    const syntax::TypeId pointer_id = types.append(pointer_type);

    syntax::TypeNode reference_type;
    reference_type.kind = syntax::TypeKind::reference;
    reference_type.pointer_mutability = syntax::PointerMutability::const_;
    reference_type.pointee = named_id;
    reference_type.reference_origin.names = {"caller", "value"};
    reference_type.reference_origin.name_ids = {syntax::IdentId{4U}, syntax::IdentId{5U}};
    reference_type.reference_origin.ranges = {range, range};
    reference_type.reference_origin.range = range;
    reference_type.reference_origin.explicit_ = true;
    reference_type.range = range;
    const syntax::TypeId reference_id = types.append(reference_type);

    syntax::TypeNode array_type;
    array_type.kind = syntax::TypeKind::array;
    array_type.array_count = 64U;
    array_type.array_element = primitive_id;
    array_type.range = range;
    const syntax::TypeId array_id = types.append(array_type);

    syntax::TypeNode slice_type;
    slice_type.kind = syntax::TypeKind::slice;
    slice_type.slice_mutability = syntax::PointerMutability::mut;
    slice_type.slice_element = primitive_id;
    slice_type.range = range;
    const syntax::TypeId slice_id = types.append(slice_type);

    syntax::TypeNode tuple_type;
    tuple_type.kind = syntax::TypeKind::tuple;
    tuple_type.tuple_elements = {primitive_id, named_id, pointer_id};
    tuple_type.range = range;
    const syntax::TypeId tuple_id = types.append(tuple_type);

    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_call_conv = syntax::FunctionCallConv::c;
    function_type.function_is_unsafe = true;
    function_type.function_is_variadic = true;
    function_type.function_params = {pointer_id, reference_id};
    function_type.function_return = array_id;
    function_type.range = range;
    const syntax::TypeId function_id = types.append(function_type);

    syntax::TypeNode dyn_composition_type;
    dyn_composition_type.kind = syntax::TypeKind::dyn_trait;
    dyn_composition_type.dyn_trait_principals.push_back(syntax::DynTraitPrincipalDecl{named_id, range});
    dyn_composition_type.dyn_trait_principals.push_back(syntax::DynTraitPrincipalDecl{function_id, range});
    dyn_composition_type.range = range;
    const syntax::TypeId dyn_composition_id = types.append(dyn_composition_type);

    syntax::TypeNode unknown_type;
    unknown_type.kind = static_cast<syntax::TypeKind>(99);
    unknown_type.range = range;
    const syntax::TypeId unknown_id = types.append(unknown_type);

    ASSERT_EQ(types.size(), 10U);
    const syntax::TypeNodeList copied(types);
    EXPECT_EQ(copied[primitive_id.value].primitive, syntax::PrimitiveTypeKind::i64);
    EXPECT_EQ(copied[named_id.value].scope_parts.back(), "mem");
    EXPECT_EQ(copied[named_id.value].type_args.back().value, 2U);
    EXPECT_EQ(copied[pointer_id.value].pointer_mutability, syntax::PointerMutability::mut);
    EXPECT_EQ(copied[pointer_id.value].pointee.value, named_id.value);
    EXPECT_TRUE(copied[reference_id.value].reference_origin.explicit_);
    EXPECT_EQ(copied[reference_id.value].reference_origin.names.front(), "caller");
    EXPECT_EQ(copied[array_id.value].array_count, 64U);
    EXPECT_EQ(copied[slice_id.value].slice_mutability, syntax::PointerMutability::mut);
    EXPECT_EQ(copied[tuple_id.value].tuple_elements.back().value, pointer_id.value);
    EXPECT_TRUE(copied[function_id.value].function_is_unsafe);
    EXPECT_TRUE(copied[function_id.value].function_is_variadic);
    EXPECT_EQ(copied[function_id.value].function_params.front().value, pointer_id.value);
    ASSERT_EQ(copied[dyn_composition_id.value].dyn_trait_principals.size(), 2U);
    EXPECT_EQ(copied[dyn_composition_id.value].dyn_trait_principals.front().trait_type.value, named_id.value);
    EXPECT_EQ(copied[dyn_composition_id.value].dyn_trait_principals.back().trait_type.value, function_id.value);
    EXPECT_EQ(copied[unknown_id.value].kind, static_cast<syntax::TypeKind>(99));
    EXPECT_EQ(copied[unknown_id.value].range.begin, range.begin);

    syntax::TypeNodeList assigned;
    assigned = types;
    EXPECT_EQ(assigned.size(), types.size());
    EXPECT_EQ(assigned[function_id.value].function_return.value, array_id.value);

    syntax::TypeNodeList moved(copied);
    EXPECT_EQ(moved.take(primitive_id.value).primitive, syntax::PrimitiveTypeKind::i64);
    EXPECT_EQ(moved.take(named_id.value).scope_name_id.value, 1U);
    EXPECT_EQ(moved.take(pointer_id.value).pointee.value, named_id.value);
    EXPECT_EQ(moved.take(reference_id.value).reference_origin.name_ids.back().value, 5U);
    EXPECT_EQ(moved.take(array_id.value).array_element.value, primitive_id.value);
    EXPECT_EQ(moved.take(slice_id.value).slice_element.value, primitive_id.value);
    EXPECT_EQ(moved.take(tuple_id.value).tuple_elements.front().value, primitive_id.value);
    EXPECT_EQ(moved.take(function_id.value).function_call_conv, syntax::FunctionCallConv::c);
    EXPECT_EQ(moved.take(dyn_composition_id.value).dyn_trait_principals.back().trait_type.value, function_id.value);
    EXPECT_EQ(moved.take(unknown_id.value).kind, static_cast<syntax::TypeKind>(99));

    syntax::TypeNode replacement_type;
    replacement_type.kind = syntax::TypeKind::pointer;
    replacement_type.pointer_mutability = syntax::PointerMutability::const_;
    replacement_type.pointee = function_id;
    assigned.set(primitive_id.value, replacement_type);
    EXPECT_EQ(assigned[primitive_id.value].kind, syntax::TypeKind::pointer);
    EXPECT_EQ(assigned[primitive_id.value].pointee.value, function_id.value);
}

TEST(CoreUnit, AstStmtNodeListCopiesAndMovesLoopAndFallbackPayloads)
{
    syntax::StmtNodeList stmts;
    stmts.reserve(14U);
    const base::SourceRange range{base::SourceId{3U}, 50U, 60U};

    syntax::StmtNode let_stmt;
    let_stmt.kind = syntax::StmtKind::let;
    let_stmt.name = "value";
    let_stmt.name_id = syntax::IdentId{1U};
    let_stmt.pattern = syntax::PatternId{2U};
    let_stmt.declared_type = syntax::TypeId{3U};
    let_stmt.init = syntax::ExprId{4U};
    let_stmt.else_block = syntax::StmtId{5U};
    let_stmt.range = range;
    const syntax::StmtId let_id = stmts.append(let_stmt);

    syntax::StmtNode var_stmt = let_stmt;
    var_stmt.kind = syntax::StmtKind::var;
    var_stmt.name = "slot";
    var_stmt.name_id = syntax::IdentId{6U};
    const syntax::StmtId var_id = stmts.append(var_stmt);

    syntax::StmtNode assign_stmt;
    assign_stmt.kind = syntax::StmtKind::assign;
    assign_stmt.assign_op = syntax::AssignOp::add;
    assign_stmt.lhs = syntax::ExprId{7U};
    assign_stmt.rhs = syntax::ExprId{8U};
    assign_stmt.range = range;
    const syntax::StmtId assign_id = stmts.append(assign_stmt);

    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = syntax::ExprId{9U};
    if_stmt.pattern = syntax::PatternId{10U};
    if_stmt.then_block = syntax::StmtId{11U};
    if_stmt.else_block = syntax::StmtId{12U};
    if_stmt.else_if = syntax::StmtId{13U};
    if_stmt.range = range;
    const syntax::StmtId if_id = stmts.append(if_stmt);

    syntax::StmtNode for_stmt;
    for_stmt.kind = syntax::StmtKind::for_;
    for_stmt.for_init = syntax::StmtId{14U};
    for_stmt.condition = syntax::ExprId{15U};
    for_stmt.for_update = syntax::StmtId{16U};
    for_stmt.body = syntax::StmtId{17U};
    for_stmt.range = range;
    const syntax::StmtId for_id = stmts.append(for_stmt);

    syntax::StmtNode for_range_stmt;
    for_range_stmt.kind = syntax::StmtKind::for_range;
    for_range_stmt.name = "item";
    for_range_stmt.name_id = syntax::IdentId{18U};
    for_range_stmt.range_start = syntax::ExprId{19U};
    for_range_stmt.range_end = syntax::ExprId{20U};
    for_range_stmt.range_step = syntax::ExprId{21U};
    for_range_stmt.body = syntax::StmtId{22U};
    for_range_stmt.range = range;
    const syntax::StmtId for_range_id = stmts.append(for_range_stmt);

    syntax::StmtNode while_stmt;
    while_stmt.kind = syntax::StmtKind::while_;
    while_stmt.condition = syntax::ExprId{23U};
    while_stmt.pattern = syntax::PatternId{24U};
    while_stmt.body = syntax::StmtId{25U};
    while_stmt.range = range;
    const syntax::StmtId while_id = stmts.append(while_stmt);

    syntax::StmtNode break_stmt;
    break_stmt.kind = syntax::StmtKind::break_;
    break_stmt.range = range;
    const syntax::StmtId break_id = stmts.append(break_stmt);

    syntax::StmtNode continue_stmt;
    continue_stmt.kind = syntax::StmtKind::continue_;
    continue_stmt.range = range;
    const syntax::StmtId continue_id = stmts.append(continue_stmt);

    syntax::StmtNode defer_stmt;
    defer_stmt.kind = syntax::StmtKind::defer;
    defer_stmt.init = syntax::ExprId{26U};
    defer_stmt.range = range;
    const syntax::StmtId defer_id = stmts.append(defer_stmt);

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = syntax::ExprId{27U};
    return_stmt.range = range;
    const syntax::StmtId return_id = stmts.append(return_stmt);

    syntax::StmtNode expr_stmt;
    expr_stmt.kind = syntax::StmtKind::expr;
    expr_stmt.init = syntax::ExprId{28U};
    expr_stmt.range = range;
    const syntax::StmtId expr_id = stmts.append(expr_stmt);

    syntax::StmtNode block_stmt;
    block_stmt.kind = syntax::StmtKind::block;
    block_stmt.statements = {let_id, var_id, expr_id};
    block_stmt.range = range;
    const syntax::StmtId block_id = stmts.append(block_stmt);

    syntax::StmtNode unknown_stmt;
    unknown_stmt.kind = static_cast<syntax::StmtKind>(99);
    unknown_stmt.name = "unknown";
    unknown_stmt.init = syntax::ExprId{29U};
    unknown_stmt.range = range;
    const syntax::StmtId unknown_id = stmts.append(unknown_stmt);

    ASSERT_EQ(stmts.size(), 14U);
    EXPECT_EQ(stmts.block_statements(expr_id.value), nullptr);
    ASSERT_NE(stmts.block_statements(block_id.value), nullptr);
    EXPECT_EQ(stmts.block_statements(block_id.value)->back().value, expr_id.value);

    const syntax::StmtNodeList copied(stmts);
    EXPECT_EQ(copied[let_id.value].name, "value");
    EXPECT_EQ(copied[var_id.value].name_id.value, 6U);
    EXPECT_EQ(copied[assign_id.value].assign_op, syntax::AssignOp::add);
    EXPECT_EQ(copied[if_id.value].else_if.value, 13U);
    EXPECT_EQ(copied[for_id.value].for_update.value, 16U);
    EXPECT_EQ(copied[for_range_id.value].range_step.value, 21U);
    EXPECT_EQ(copied[while_id.value].body.value, 25U);
    EXPECT_EQ(copied[break_id.value].kind, syntax::StmtKind::break_);
    EXPECT_EQ(copied[continue_id.value].kind, syntax::StmtKind::continue_);
    EXPECT_EQ(copied[defer_id.value].init.value, 26U);
    EXPECT_EQ(copied[return_id.value].return_value.value, 27U);
    EXPECT_EQ(copied[expr_id.value].init.value, 28U);
    EXPECT_EQ(copied[block_id.value].statements.front().value, let_id.value);
    EXPECT_EQ(copied[unknown_id.value].name, "unknown");
    EXPECT_EQ(copied[unknown_id.value].kind, static_cast<syntax::StmtKind>(99));

    syntax::StmtNodeList assigned;
    assigned = stmts;
    EXPECT_EQ(assigned.size(), stmts.size());
    EXPECT_EQ(assigned[for_id.value].body.value, 17U);

    syntax::StmtNodeList moved(copied);
    EXPECT_EQ(moved.take(let_id.value).declared_type.value, 3U);
    EXPECT_EQ(moved.take(var_id.value).name, "slot");
    EXPECT_EQ(moved.take(assign_id.value).lhs.value, 7U);
    EXPECT_EQ(moved.take(if_id.value).then_block.value, 11U);
    EXPECT_EQ(moved.take(for_id.value).for_init.value, 14U);
    EXPECT_EQ(moved.take(for_range_id.value).name_id.value, 18U);
    EXPECT_EQ(moved.take(while_id.value).pattern.value, 24U);
    EXPECT_EQ(moved.take(break_id.value).kind, syntax::StmtKind::break_);
    EXPECT_EQ(moved.take(continue_id.value).kind, syntax::StmtKind::continue_);
    EXPECT_EQ(moved.take(defer_id.value).init.value, 26U);
    EXPECT_EQ(moved.take(return_id.value).return_value.value, 27U);
    EXPECT_EQ(moved.take(expr_id.value).init.value, 28U);
    EXPECT_EQ(moved.take(block_id.value).statements.back().value, expr_id.value);
    EXPECT_EQ(moved.take(unknown_id.value).name, "unknown");

    const base::SourceRange updated_range{base::SourceId{4U}, 70U, 90U};
    assigned.set_range(let_id.value, updated_range);
    EXPECT_EQ(assigned.range(let_id.value).begin, updated_range.begin);

    syntax::StmtNode replacement_stmt;
    replacement_stmt.kind = syntax::StmtKind::return_;
    replacement_stmt.return_value = syntax::ExprId{30U};
    assigned.set(var_id.value, replacement_stmt);
    EXPECT_EQ(assigned[var_id.value].kind, syntax::StmtKind::return_);
    EXPECT_EQ(assigned[var_id.value].return_value.value, 30U);
}

TEST(CoreUnit, AstModuleInternsNativeIdentifierIdsAcrossNodesAndMetadata)
{
    syntax::AstModule module;
    module.module_path.parts = {"app", "main"};
    module.file_kind = syntax::ModuleFileKind::part;
    module.part_header = syntax::ModulePartHeader{"parser", {}};
    module.part_declarations.push_back(syntax::ModulePartDecl{"parser", {}});
    module.part_declarations.push_back(syntax::ModulePartDecl{"emitter", {}});
    syntax::ImportDecl import;
    import.path.parts = {"core", "mem"};
    import.alias = "mem";
    module.imports.push_back(import);
    syntax::ModuleInfo module_info;
    module_info.path.parts = {"lib", "math"};
    syntax::ResolvedImport resolved_import;
    resolved_import.module = syntax::ModuleId{0};
    resolved_import.alias = "math";
    resolved_import.visibility = syntax::Visibility::public_;
    module_info.imports.push_back(resolved_import);
    module.modules.push_back(module_info);

    syntax::TypeNode named_type;
    named_type.kind = syntax::TypeKind::named;
    named_type.scope_name = "core";
    named_type.scope_parts = {"core", "mem"};
    named_type.name = "Buffer";
    const syntax::TypeId type_id = module.push_type(named_type);

    const syntax::ExprId name_expr_id = push_name_expr(module, "value", "mem");

    syntax::FieldExprPayload field_expr;
    field_expr.object = name_expr_id;
    field_expr.field_name = "len";
    const syntax::ExprId field_expr_id = module.push_field_expr({}, field_expr);

    syntax::StructLiteralExprPayload struct_literal;
    struct_literal.scope_name = "core";
    struct_literal.name = "Buffer";
    syntax::FieldInit count_init;
    count_init.name = "count";
    count_init.value = name_expr_id;
    struct_literal.field_inits = {count_init};
    const syntax::ExprId struct_literal_id = module.push_struct_literal_expr({}, std::move(struct_literal));

    const syntax::ExprId next_field_expr_id = module.push_field_expr({}, name_expr_id, "next");

    syntax::PatternNode binding_pattern;
    binding_pattern.kind = syntax::PatternKind::binding;
    binding_pattern.binding_name = "value";
    const syntax::PatternId pattern_id = module.push_pattern(binding_pattern);

    syntax::PatternNode literal_pattern;
    literal_pattern.kind = syntax::PatternKind::literal;
    literal_pattern.case_name = "true";
    literal_pattern.binding_names = {"value"};
    const syntax::PatternId literal_pattern_id = module.push_pattern(literal_pattern);

    syntax::PatternNode enum_pattern;
    enum_pattern.kind = syntax::PatternKind::enum_case;
    enum_pattern.enum_name = "Choice";
    enum_pattern.case_name = "Some";
    enum_pattern.binding_names = {"payload"};
    const syntax::PatternId enum_pattern_id = module.push_pattern(enum_pattern);

    syntax::FieldPattern item_field;
    item_field.name = "item";
    item_field.pattern = pattern_id;
    syntax::PatternNode struct_pattern;
    struct_pattern.kind = syntax::PatternKind::struct_;
    struct_pattern.struct_name = "Record";
    struct_pattern.field_patterns = {item_field};
    const syntax::PatternId struct_pattern_id = module.push_pattern(struct_pattern);

    syntax::StmtNode local_stmt;
    local_stmt.kind = syntax::StmtKind::let;
    local_stmt.name = "value";
    local_stmt.pattern = pattern_id;
    local_stmt.init = field_expr_id;
    const syntax::StmtId stmt_id = module.push_stmt(local_stmt);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "value";
    function.generic_params = {syntax::GenericParamDecl{"T", {}}};
    syntax::GenericConstraintDecl copy_constraint;
    copy_constraint.param_name = "T";
    copy_constraint.capability_names = {"Copy"};
    function.where_constraints = {copy_constraint};
    function.params = {syntax::ParamDecl{"value", type_id, {}}};
    function.body = stmt_id;
    const syntax::ItemId item_id = module.push_item(function);

    syntax::ItemNode struct_item;
    struct_item.kind = syntax::ItemKind::struct_decl;
    struct_item.name = "Record";
    syntax::FieldDecl field_decl;
    field_decl.name = "item";
    field_decl.type = type_id;
    struct_item.fields = {field_decl};
    const syntax::ItemId struct_item_id = module.push_item(struct_item);

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "Choice";
    syntax::EnumCaseDecl enum_case;
    enum_case.name = "Some";
    enum_case.payload_type = type_id;
    enum_item.enum_cases = {enum_case};
    const syntax::ItemId enum_item_id = module.push_item(enum_item);

    syntax::ItemNode trait_item;
    trait_item.kind = syntax::ItemKind::trait_decl;
    trait_item.name = "Readable";
    trait_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    syntax::GenericConstraintDecl trait_constraint;
    trait_constraint.param_name = "T";
    trait_constraint.capability_names = {"Copy"};
    trait_item.where_constraints = {trait_constraint};
    trait_item.trait_items = {item_id};
    const syntax::ItemId trait_item_id = module.push_item(trait_item);

    module.finalize_identifiers();

    const syntax::IdentId value_id = module.find_identifier("value");
    ASSERT_TRUE(syntax::is_valid(value_id));
    EXPECT_EQ(module.identifier_text(value_id), "value");
    EXPECT_GT(module.identifiers.arena_blocks(), 0U);
    EXPECT_GT(module.identifiers.arena_bytes(), 0U);

    const syntax::TypeNode stored_type = module.types[type_id.value];
    EXPECT_EQ(stored_type.name_id, module.find_identifier("Buffer"));
    ASSERT_EQ(stored_type.scope_part_ids.size(), 2U);
    EXPECT_EQ(stored_type.scope_part_ids.front(), module.find_identifier("core"));
    EXPECT_EQ(stored_type.scope_part_ids.back(), module.find_identifier("mem"));

    const syntax::NameExprPayload* const name_payload = module.exprs.name_payload(name_expr_id.value);
    ASSERT_NE(name_payload, nullptr);
    EXPECT_EQ(name_payload->scope_name_id, module.find_identifier("mem"));
    EXPECT_EQ(name_payload->text_id, value_id);

    const syntax::FieldExprPayload* const field_payload = module.exprs.field_payload(field_expr_id.value);
    ASSERT_NE(field_payload, nullptr);
    EXPECT_EQ(field_payload->field_name_id, module.find_identifier("len"));

    const syntax::StructLiteralExprPayload* const struct_payload =
        module.exprs.struct_literal_payload(struct_literal_id.value);
    ASSERT_NE(struct_payload, nullptr);
    EXPECT_EQ(struct_payload->scope_name_id, module.find_identifier("core"));
    EXPECT_EQ(struct_payload->name_id, module.find_identifier("Buffer"));
    ASSERT_EQ(struct_payload->field_inits.size(), 1U);
    EXPECT_EQ(struct_payload->field_inits.front().name_id, module.find_identifier("count"));

    const syntax::FieldExprPayload* const next_field_payload = module.exprs.field_payload(next_field_expr_id.value);
    ASSERT_NE(next_field_payload, nullptr);
    EXPECT_EQ(next_field_payload->field_name_id, module.find_identifier("next"));

    EXPECT_EQ(module.patterns[pattern_id.value].binding_name_id, value_id);
    const syntax::PatternNode stored_literal = module.patterns[literal_pattern_id.value];
    EXPECT_EQ(stored_literal.case_name_id, module.find_identifier("true"));
    ASSERT_EQ(stored_literal.binding_name_ids.size(), 1U);
    EXPECT_EQ(stored_literal.binding_name_ids.front(), value_id);
    const syntax::PatternNode stored_enum_pattern = module.patterns[enum_pattern_id.value];
    EXPECT_EQ(stored_enum_pattern.enum_name_id, module.find_identifier("Choice"));
    EXPECT_EQ(stored_enum_pattern.case_name_id, module.find_identifier("Some"));
    ASSERT_EQ(stored_enum_pattern.binding_name_ids.size(), 1U);
    EXPECT_EQ(stored_enum_pattern.binding_name_ids.front(), module.find_identifier("payload"));
    const syntax::PatternNode stored_struct_pattern = module.patterns[struct_pattern_id.value];
    EXPECT_EQ(stored_struct_pattern.struct_name_id, module.find_identifier("Record"));
    ASSERT_EQ(stored_struct_pattern.field_patterns.size(), 1U);
    EXPECT_EQ(stored_struct_pattern.field_patterns.front().name_id, module.find_identifier("item"));
    EXPECT_EQ(module.stmts[stmt_id.value].name_id, value_id);
    const syntax::ItemNode stored_item = module.items[item_id.value];
    EXPECT_EQ(stored_item.name_id, value_id);
    ASSERT_EQ(stored_item.generic_params.size(), 1U);
    EXPECT_EQ(stored_item.generic_params.front().name_id, module.find_identifier("T"));
    ASSERT_EQ(stored_item.where_constraints.size(), 1U);
    EXPECT_EQ(stored_item.where_constraints.front().param_name_id, module.find_identifier("T"));
    ASSERT_EQ(stored_item.where_constraints.front().capability_name_ids.size(), 1U);
    EXPECT_EQ(stored_item.where_constraints.front().capability_name_ids.front(), module.find_identifier("Copy"));
    ASSERT_EQ(stored_item.params.size(), 1U);
    EXPECT_EQ(stored_item.params.front().name_id, value_id);
    const syntax::ItemNode stored_struct_item = module.items[struct_item_id.value];
    EXPECT_EQ(stored_struct_item.name_id, module.find_identifier("Record"));
    ASSERT_EQ(stored_struct_item.fields.size(), 1U);
    EXPECT_EQ(stored_struct_item.fields.front().name_id, module.find_identifier("item"));
    const syntax::ItemNode stored_enum_item = module.items[enum_item_id.value];
    EXPECT_EQ(stored_enum_item.name_id, module.find_identifier("Choice"));
    ASSERT_EQ(stored_enum_item.enum_cases.size(), 1U);
    EXPECT_EQ(stored_enum_item.enum_cases.front().name_id, module.find_identifier("Some"));
    const syntax::ItemNode stored_trait_item = module.items[trait_item_id.value];
    EXPECT_EQ(stored_trait_item.name_id, module.find_identifier("Readable"));
    ASSERT_EQ(stored_trait_item.generic_params.size(), 1U);
    EXPECT_EQ(stored_trait_item.generic_params.front().name_id, module.find_identifier("T"));
    ASSERT_EQ(stored_trait_item.where_constraints.size(), 1U);
    EXPECT_EQ(stored_trait_item.where_constraints.front().param_name_id, module.find_identifier("T"));
    ASSERT_EQ(stored_trait_item.where_constraints.front().capability_name_ids.size(), 1U);
    EXPECT_EQ(stored_trait_item.where_constraints.front().capability_name_ids.front(), module.find_identifier("Copy"));
    ASSERT_EQ(stored_trait_item.trait_items.size(), 1U);
    EXPECT_EQ(stored_trait_item.trait_items.front().value, item_id.value);

    ASSERT_EQ(module.module_path.part_ids.size(), 2U);
    EXPECT_EQ(module.module_path.part_ids.front(), module.find_identifier("app"));
    EXPECT_EQ(module.part_header.name_id, module.find_identifier("parser"));
    ASSERT_EQ(module.part_declarations.size(), 2U);
    EXPECT_EQ(module.part_declarations.front().name_id, module.find_identifier("parser"));
    EXPECT_EQ(module.part_declarations.back().name_id, module.find_identifier("emitter"));
    ASSERT_EQ(module.imports.size(), 1U);
    EXPECT_EQ(module.imports.front().alias_id, module.find_identifier("mem"));
    ASSERT_EQ(module.imports.front().path.part_ids.size(), 2U);
    EXPECT_EQ(module.imports.front().path.part_ids.back(), module.find_identifier("mem"));
    ASSERT_EQ(module.modules.size(), 1U);
    ASSERT_EQ(module.modules.front().path.part_ids.size(), 2U);
    EXPECT_EQ(module.modules.front().path.part_ids.back(), module.find_identifier("math"));
    ASSERT_EQ(module.modules.front().imports.size(), 1U);
    EXPECT_EQ(module.modules.front().imports.front().alias_id, module.find_identifier("math"));

    const syntax::AstModule copied = module;
    const syntax::NameExprPayload* const copied_name = copied.exprs.name_payload(name_expr_id.value);
    ASSERT_NE(copied_name, nullptr);
    EXPECT_EQ(copied_name->text_id, copied.find_identifier("value"));
    EXPECT_EQ(copied.identifier_text(copied_name->text_id), "value");
    EXPECT_EQ(copied.file_kind, syntax::ModuleFileKind::part);
    EXPECT_EQ(copied.part_header.name_id, copied.find_identifier("parser"));
    ASSERT_EQ(copied.part_declarations.size(), 2U);
    EXPECT_EQ(copied.part_declarations.front().name_id, copied.find_identifier("parser"));

    syntax::AstModule assigned;
    assigned = module;
    EXPECT_EQ(assigned.file_kind, syntax::ModuleFileKind::part);
    EXPECT_EQ(assigned.part_header.name_id, assigned.find_identifier("parser"));
    ASSERT_EQ(assigned.part_declarations.size(), 2U);
    EXPECT_EQ(assigned.part_declarations.back().name_id, assigned.find_identifier("emitter"));
}

TEST(CoreUnit, AstModuleIdentifierInterningIsIdempotentAndRehomesCopies)
{
    syntax::AstModule module;
    module.module_path.parts = {"app"};

    syntax::TypeNode named_type;
    named_type.kind = syntax::TypeKind::named;
    named_type.scope_name = "core";
    named_type.scope_parts = {"core", "mem"};
    named_type.name = "Buffer";
    const syntax::TypeId type_id = module.push_type(named_type);

    const syntax::ExprId name_expr = push_name_expr(module, "value", "mem");
    static_cast<void>(module.push_field_expr({}, name_expr, "len"));

    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::binding;
    pattern.binding_name = "value";
    static_cast<void>(module.push_pattern(pattern));

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::let;
    stmt.name = "value";
    stmt.init = name_expr;
    const syntax::StmtId stmt_id = module.push_stmt(stmt);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "value";
    function.generic_params = {syntax::GenericParamDecl{"T", {}}};
    function.params = {syntax::ParamDecl{"value", type_id, {}}};
    function.body = stmt_id;
    static_cast<void>(module.push_item(function));

    module.intern_identifiers();
    ASSERT_TRUE(module.identifiers_ready());
    const base::usize identifier_count = module.identifiers.size();
    const base::usize identifier_arena_bytes = module.identifiers.arena_bytes();
    const base::usize type_used_bytes = module.types.arena_used_bytes();
    const base::usize expr_used_bytes = module.exprs.arena_used_bytes();
    const base::usize pattern_used_bytes = module.patterns.arena_used_bytes();
    const base::usize stmt_used_bytes = module.stmts.arena_used_bytes();
    const base::usize item_used_bytes = module.items.arena_used_bytes();

    module.intern_identifiers();
    EXPECT_EQ(module.identifiers.size(), identifier_count);
    EXPECT_EQ(module.identifiers.arena_bytes(), identifier_arena_bytes);
    EXPECT_EQ(module.types.arena_used_bytes(), type_used_bytes);
    EXPECT_EQ(module.exprs.arena_used_bytes(), expr_used_bytes);
    EXPECT_EQ(module.patterns.arena_used_bytes(), pattern_used_bytes);
    EXPECT_EQ(module.stmts.arena_used_bytes(), stmt_used_bytes);
    EXPECT_EQ(module.items.arena_used_bytes(), item_used_bytes);

    const syntax::AstModule copied = module;
    ASSERT_TRUE(copied.identifiers_ready());
    const syntax::IdentId copied_buffer_id = copied.find_identifier("Buffer");
    ASSERT_TRUE(syntax::is_valid(copied_buffer_id));
    const std::string_view copied_buffer_text = copied.identifier_text(copied_buffer_id);
    const syntax::TypeNode copied_type = copied.types[type_id.value];
    EXPECT_EQ(copied_type.name, copied_buffer_text);
    EXPECT_EQ(copied_type.name_id, copied_buffer_id);
    EXPECT_EQ(copied_type.name.data(), copied_buffer_text.data());
}

TEST(CoreUnit, CompactAstStorageRoundTripsAndMovesPayloads)
{
    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_is_unsafe = true;
    function_type.function_params = {syntax::TypeId{1}, syntax::TypeId{2}};
    function_type.function_return = syntax::TypeId{3};

    syntax::TypeNodeList types;
    types.push_back(function_type);
    EXPECT_GT(types.arena_blocks(), 0U);
    EXPECT_GT(types.arena_bytes(), 0U);
    EXPECT_TRUE(types[0].function_is_unsafe);
    EXPECT_EQ(types[0].function_params.size(), 2U);
    syntax::TypeNode moved_type = types.take(0);
    EXPECT_TRUE(moved_type.function_is_unsafe);
    EXPECT_EQ(moved_type.function_params.back().value, 2U);

    syntax::CallExprPayload call_expr;
    call_expr.callee = syntax::ExprId{4};
    call_expr.args = {syntax::ExprId{5}, syntax::ExprId{6}};

    syntax::ExprNodeList exprs;
    const syntax::ExprId call_id = exprs.append_call(syntax::ExprKind::call, {}, call_expr);
    EXPECT_GT(exprs.arena_blocks(), 0U);
    EXPECT_GT(exprs.arena_bytes(), 0U);
    const syntax::CallExprPayload* const stored_call = exprs.call_payload(call_id.value);
    ASSERT_NE(stored_call, nullptr);
    EXPECT_EQ(stored_call->args.size(), 2U);
    EXPECT_EQ(stored_call->callee.value, 4U);
    EXPECT_EQ(stored_call->args.back().value, 6U);

    syntax::NameExprPayload typed_name;
    typed_name.text = "make";
    typed_name.type_args = {syntax::TypeId{8}, syntax::TypeId{9}};
    syntax::ExprNodeList names;
    const syntax::ExprId typed_name_id = names.append_name({}, typed_name);
    const syntax::NameExprPayload* const moved_name = names.name_payload(typed_name_id.value);
    ASSERT_NE(moved_name, nullptr);
    EXPECT_EQ(moved_name->type_args.back().value, 9U);
    EXPECT_EQ(moved_name->type_args.front().value, 8U);

    syntax::ExprNodeList literals;
    const syntax::ExprId bool_id = literals.append_literal(syntax::ExprKind::bool_literal, {}, "true");
    const syntax::ExprId null_id = literals.append_literal(syntax::ExprKind::null_literal, {}, "null");
    ASSERT_NE(literals.literal_payload(bool_id.value), nullptr);
    ASSERT_NE(literals.literal_payload(null_id.value), nullptr);
    EXPECT_EQ(literals.literal_payload(bool_id.value)->text, "true");
    EXPECT_EQ(literals.literal_payload(null_id.value)->text, "null");

    syntax::PatternNode enum_pattern;
    enum_pattern.kind = syntax::PatternKind::enum_case;
    enum_pattern.scoped = true;
    enum_pattern.case_name = "some";
    enum_pattern.payload_patterns = {syntax::PatternId{7}};
    enum_pattern.binding_names = {"value"};

    syntax::PatternNodeList patterns;
    patterns.push_back(enum_pattern);
    EXPECT_GT(patterns.arena_blocks(), 0U);
    EXPECT_GT(patterns.arena_bytes(), 0U);
    EXPECT_EQ(patterns[0].binding_names.front(), "value");
    syntax::PatternNode moved_pattern = patterns.take(0);
    EXPECT_TRUE(moved_pattern.scoped);
    EXPECT_EQ(moved_pattern.payload_patterns.front().value, 7U);
    EXPECT_EQ(moved_pattern.binding_names.front(), "value");

    syntax::StmtNode block_stmt;
    block_stmt.kind = syntax::StmtKind::block;
    block_stmt.statements = {syntax::StmtId{10}, syntax::StmtId{11}, syntax::StmtId{12}};
    syntax::StmtNodeList stmts;
    stmts.push_back(block_stmt);
    EXPECT_GT(stmts.arena_blocks(), 0U);
    EXPECT_GT(stmts.arena_bytes(), 0U);
    EXPECT_EQ(stmts[0].statements.size(), 3U);
    syntax::StmtNode moved_stmt = stmts.take(0);
    EXPECT_EQ(moved_stmt.statements.back().value, 12U);

    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "map";
    function_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    syntax::GenericConstraintDecl copy_constraint;
    copy_constraint.param_name = "T";
    copy_constraint.capability_names = {"Copy"};
    function_item.where_constraints = {copy_constraint};
    function_item.params = {syntax::ParamDecl{"value", syntax::TypeId{13}, {}}};
    function_item.return_type = syntax::TypeId{14};
    function_item.body = syntax::StmtId{15};
    function_item.impl_type = syntax::TypeId{16};
    function_item.trait_type = syntax::TypeId{17};
    function_item.is_unsafe = true;
    function_item.is_trait_default_method = true;
    function_item.abi_name = "aurex_map";

    syntax::ItemNodeList items;
    items.push_back(function_item);
    EXPECT_GT(items.arena_blocks(), 0U);
    EXPECT_GT(items.arena_bytes(), 0U);
    items.set_visibility(0, syntax::Visibility::public_);
    EXPECT_EQ(items[0].visibility, syntax::Visibility::public_);
    EXPECT_TRUE(items[0].is_unsafe);
    EXPECT_TRUE(items[0].is_trait_default_method);
    EXPECT_EQ(items[0].where_constraints.front().capability_names.front(), "Copy");
    syntax::ItemNode moved_item = items.take(0);
    EXPECT_EQ(moved_item.params.front().type.value, 13U);
    EXPECT_EQ(moved_item.impl_type.value, 16U);
    EXPECT_EQ(moved_item.trait_type.value, 17U);
    EXPECT_TRUE(moved_item.is_trait_default_method);
    EXPECT_EQ(moved_item.abi_name, "aurex_map");

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "Option";
    enum_item.enum_base_type = syntax::TypeId{18};
    enum_item.enum_cases = {
        syntax::EnumCaseDecl{"some", syntax::TypeId{19}, {syntax::TypeId{19}}, {}, {}},
        syntax::EnumCaseDecl{"none", syntax::INVALID_TYPE_ID, {}, "0", {}},
    };

    syntax::ItemNodeList enum_items;
    enum_items.push_back(enum_item);
    EXPECT_EQ(enum_items[0].enum_cases.front().payload_types.front().value, 19U);
    syntax::ItemNode moved_enum = enum_items.take(0);
    EXPECT_EQ(moved_enum.enum_base_type.value, 18U);
    EXPECT_EQ(moved_enum.enum_cases.back().value_text, "0");

    syntax::ItemNode trait_item;
    trait_item.kind = syntax::ItemKind::trait_decl;
    trait_item.name = "Reader";
    trait_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    trait_item.trait_items = {syntax::ItemId{20}, syntax::ItemId{21}};
    syntax::ItemNodeList trait_items;
    trait_items.push_back(trait_item);
    ASSERT_EQ(trait_items[0].trait_items.size(), 2U);
    EXPECT_EQ(trait_items[0].trait_items.back().value, 21U);
    syntax::ItemNode moved_trait = trait_items.take(0);
    EXPECT_EQ(moved_trait.kind, syntax::ItemKind::trait_decl);
    EXPECT_EQ(moved_trait.generic_params.front().name, "T");
    EXPECT_EQ(moved_trait.trait_items.front().value, 20U);
}

TEST(CoreUnit, VisibilityLatticeOrdersNamesAndRoundTripsPackageLevel)
{
    EXPECT_EQ(syntax::visibility_rank(syntax::Visibility::private_), syntax::VISIBILITY_RANK_PRIVATE);
    EXPECT_EQ(syntax::visibility_rank(syntax::Visibility::package_), syntax::VISIBILITY_RANK_PACKAGE);
    EXPECT_EQ(syntax::visibility_rank(syntax::Visibility::public_), syntax::VISIBILITY_RANK_PUBLIC);
    EXPECT_LT(
        syntax::visibility_rank(syntax::Visibility::private_), syntax::visibility_rank(syntax::Visibility::package_));
    EXPECT_LT(
        syntax::visibility_rank(syntax::Visibility::package_), syntax::visibility_rank(syntax::Visibility::public_));

    EXPECT_TRUE(syntax::visibility_at_least(syntax::Visibility::public_, syntax::Visibility::private_));
    EXPECT_TRUE(syntax::visibility_at_least(syntax::Visibility::package_, syntax::Visibility::private_));
    EXPECT_TRUE(syntax::visibility_at_least(syntax::Visibility::public_, syntax::Visibility::package_));
    EXPECT_FALSE(syntax::visibility_at_least(syntax::Visibility::package_, syntax::Visibility::public_));
    EXPECT_FALSE(syntax::visibility_at_least(syntax::Visibility::private_, syntax::Visibility::package_));

    EXPECT_EQ(syntax::effective_visibility(syntax::Visibility::public_, syntax::Visibility::package_),
        syntax::Visibility::package_);
    EXPECT_EQ(syntax::effective_visibility(syntax::Visibility::package_, syntax::Visibility::public_),
        syntax::Visibility::package_);
    EXPECT_EQ(syntax::effective_visibility(syntax::Visibility::private_, syntax::Visibility::public_),
        syntax::Visibility::private_);

    EXPECT_EQ(syntax::visibility_name(syntax::Visibility::private_), "priv");
    EXPECT_EQ(syntax::visibility_name(syntax::Visibility::package_), "pub(package)");
    EXPECT_EQ(syntax::visibility_name(syntax::Visibility::public_), "pub");
    EXPECT_TRUE(syntax::visibility_is_public(syntax::Visibility::public_));
    EXPECT_FALSE(syntax::visibility_is_public(syntax::Visibility::package_));
    EXPECT_TRUE(syntax::visibility_is_module_private(syntax::Visibility::private_));

    syntax::ItemNode item;
    item.name = "internal_api";
    item.visibility = syntax::Visibility::package_;
    syntax::ItemNodeList items;
    items.push_back(item);
    EXPECT_EQ(items.visibility(0), syntax::Visibility::package_);
    EXPECT_EQ(items[0].visibility, syntax::Visibility::package_);
}

TEST(CoreUnit, CompactAstStorageMoveAssignmentTransfersArenaBackedPayloads)
{
    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_params = {syntax::TypeId{1}, syntax::TypeId{2}};
    function_type.function_return = syntax::TypeId{3};
    syntax::TypeNodeList source_types;
    source_types.push_back(function_type);
    syntax::TypeNodeList target_types;
    target_types.push_back(syntax::TypeNode{});
    target_types = std::move(source_types);
    EXPECT_GT(target_types.arena_blocks(), 0U);
    EXPECT_EQ(target_types[0].function_params.back().value, 2U);

    syntax::CallExprPayload call;
    call.callee = syntax::ExprId{4};
    call.args = {syntax::ExprId{5}, syntax::ExprId{6}};
    syntax::ExprNodeList source_exprs;
    const syntax::ExprId source_call = source_exprs.append_call(syntax::ExprKind::call, {}, call);
    syntax::ExprNodeList target_exprs;
    static_cast<void>(target_exprs.append_literal(syntax::ExprKind::integer_literal, {}, "0"));
    target_exprs = std::move(source_exprs);
    const syntax::CallExprPayload* const moved_call = target_exprs.call_payload(source_call.value);
    ASSERT_NE(moved_call, nullptr);
    EXPECT_EQ(moved_call->args.back().value, 6U);

    syntax::PatternNode enum_pattern;
    enum_pattern.kind = syntax::PatternKind::enum_case;
    enum_pattern.case_name = "some";
    enum_pattern.payload_patterns = {syntax::PatternId{7}};
    enum_pattern.binding_names = {"value"};
    syntax::PatternNodeList source_patterns;
    source_patterns.push_back(enum_pattern);
    syntax::PatternNodeList target_patterns;
    target_patterns.push_back(syntax::PatternNode{});
    target_patterns = std::move(source_patterns);
    EXPECT_EQ(target_patterns[0].payload_patterns.front().value, 7U);
    EXPECT_EQ(target_patterns[0].binding_names.front(), "value");

    syntax::StmtNode block_stmt;
    block_stmt.kind = syntax::StmtKind::block;
    block_stmt.statements = {syntax::StmtId{8}, syntax::StmtId{9}};
    syntax::StmtNodeList source_stmts;
    source_stmts.push_back(block_stmt);
    syntax::StmtNodeList target_stmts;
    target_stmts.push_back(syntax::StmtNode{});
    target_stmts = std::move(source_stmts);
    EXPECT_EQ(target_stmts[0].statements.back().value, 9U);

    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "map";
    function_item.generic_params = {syntax::GenericParamDecl{"T", {}}};
    function_item.params = {syntax::ParamDecl{"value", syntax::TypeId{10}, {}}};
    function_item.return_type = syntax::TypeId{11};
    function_item.trait_type = syntax::TypeId{12};
    function_item.is_trait_default_method = true;
    syntax::ItemNodeList source_items;
    source_items.push_back(function_item);
    syntax::ItemNodeList target_items;
    target_items.push_back(syntax::ItemNode{});
    target_items = std::move(source_items);
    EXPECT_EQ(target_items[0].generic_params.front().name, "T");
    EXPECT_EQ(target_items[0].params.front().type.value, 10U);
    EXPECT_EQ(target_items[0].trait_type.value, 12U);
    EXPECT_TRUE(target_items[0].is_trait_default_method);
}

TEST(CoreUnit, TypeNodeListSelfAssignmentAndMovedFromArenaQueriesAreStable)
{
    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_params = {syntax::TypeId{1}, syntax::TypeId{2}};
    function_type.function_return = syntax::TypeId{3};

    syntax::TypeNodeList types;
    types.push_back(function_type);
    syntax::TypeNodeList* const types_alias = &types;
    types = *types_alias;
    ASSERT_EQ(types.size(), 1U);
    EXPECT_EQ(types.kind(0), syntax::TypeKind::function);
    EXPECT_EQ(types[0].function_params.back().value, 2U);

    types = std::move(*types_alias);
    ASSERT_EQ(types.size(), 1U);
    EXPECT_EQ(types[0].function_return.value, 3U);

    syntax::TypeNodeList moved_types(std::move(types));
    EXPECT_EQ(types.arena_bytes(), 0U);
    EXPECT_EQ(types.arena_used_bytes(), 0U);
    EXPECT_EQ(types.arena_blocks(), 0U);
    ASSERT_EQ(moved_types.size(), 1U);
    EXPECT_EQ(moved_types[0].function_params.front().value, 1U);
}

TEST(CoreUnit, ItemNodeListSelfAssignmentAndMovedFromArenaQueriesAreStable)
{
    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "stable";
    function_item.visibility = syntax::Visibility::public_;
    function_item.params = {syntax::ParamDecl{"value", syntax::TypeId{1}, {}}};
    function_item.return_type = syntax::TypeId{2};

    syntax::ItemNodeList items;
    items.push_back(function_item);
    syntax::ItemNodeList* const items_alias = &items;
    items = *items_alias;
    ASSERT_EQ(items.size(), 1U);
    EXPECT_EQ(items.kind(0), syntax::ItemKind::fn_decl);
    EXPECT_EQ(items.visibility(0), syntax::Visibility::public_);

    items = std::move(*items_alias);
    ASSERT_EQ(items.size(), 1U);
    EXPECT_EQ(items[0].params.front().type.value, 1U);

    syntax::ItemNodeList moved_items(std::move(items));
    EXPECT_EQ(items.arena_bytes(), 0U);
    EXPECT_EQ(items.arena_used_bytes(), 0U);
    EXPECT_EQ(items.arena_blocks(), 0U);
    ASSERT_EQ(moved_items.size(), 1U);
    EXPECT_EQ(moved_items[0].return_type.value, 2U);
}

TEST(CoreUnit, ExprPatternAndStmtNodeListsSelfAssignmentAndMovedFromArenaQueriesAreStable)
{
    syntax::NameExprPayload name;
    name.text = "stable";
    name.type_args = {syntax::TypeId{1}, syntax::TypeId{2}};
    syntax::ExprNodeList exprs;
    const syntax::ExprId name_id = exprs.append_name({}, name);
    syntax::ExprNodeList* const exprs_alias = &exprs;
    exprs = *exprs_alias;
    ASSERT_EQ(exprs.size(), 1U);
    EXPECT_EQ(exprs.kind(name_id.value), syntax::ExprKind::name);
    ASSERT_NE(exprs.name_payload(name_id.value), nullptr);
    EXPECT_EQ(exprs.name_payload(name_id.value)->type_args.back().value, 2U);

    exprs = std::move(*exprs_alias);
    ASSERT_EQ(exprs.size(), 1U);
    ASSERT_NE(exprs.name_payload(name_id.value), nullptr);
    EXPECT_EQ(exprs.name_payload(name_id.value)->text, "stable");

    syntax::ExprNodeList moved_exprs(std::move(exprs));
    EXPECT_EQ(exprs.arena_bytes(), 0U);
    EXPECT_EQ(exprs.arena_used_bytes(), 0U);
    EXPECT_EQ(exprs.arena_blocks(), 0U);
    ASSERT_EQ(moved_exprs.size(), 1U);
    ASSERT_NE(moved_exprs.name_payload(name_id.value), nullptr);
    EXPECT_EQ(moved_exprs.name_payload(name_id.value)->type_args.front().value, 1U);

    syntax::PatternNode tuple_pattern;
    tuple_pattern.kind = syntax::PatternKind::tuple;
    tuple_pattern.elements = {syntax::PatternId{3}, syntax::PatternId{4}};
    syntax::PatternNodeList patterns;
    const syntax::PatternId tuple_pattern_id = patterns.append(tuple_pattern);
    syntax::PatternNodeList* const patterns_alias = &patterns;
    patterns = *patterns_alias;
    ASSERT_EQ(patterns.size(), 1U);
    EXPECT_EQ(patterns.kind(tuple_pattern_id.value), syntax::PatternKind::tuple);
    EXPECT_EQ(patterns[tuple_pattern_id.value].elements.back().value, 4U);
    EXPECT_EQ(patterns.ptr(patterns.size()), nullptr);

    patterns = std::move(*patterns_alias);
    ASSERT_EQ(patterns.size(), 1U);
    EXPECT_EQ(patterns[tuple_pattern_id.value].elements.front().value, 3U);

    syntax::PatternNodeList moved_patterns(std::move(patterns));
    EXPECT_EQ(patterns.arena_bytes(), 0U);
    EXPECT_EQ(patterns.arena_used_bytes(), 0U);
    EXPECT_EQ(patterns.arena_blocks(), 0U);
    ASSERT_EQ(moved_patterns.size(), 1U);
    EXPECT_EQ(moved_patterns[tuple_pattern_id.value].kind, syntax::PatternKind::tuple);

    syntax::StmtNode block_stmt;
    block_stmt.kind = syntax::StmtKind::block;
    block_stmt.statements = {syntax::StmtId{5}, syntax::StmtId{6}};
    syntax::StmtNodeList stmts;
    const syntax::StmtId block_stmt_id = stmts.append(block_stmt);
    syntax::StmtNodeList* const stmts_alias = &stmts;
    stmts = *stmts_alias;
    ASSERT_EQ(stmts.size(), 1U);
    EXPECT_EQ(stmts.kind(block_stmt_id.value), syntax::StmtKind::block);
    ASSERT_NE(stmts.block_statements(block_stmt_id.value), nullptr);
    EXPECT_EQ(stmts.block_statements(block_stmt_id.value)->back().value, 6U);

    stmts = std::move(*stmts_alias);
    ASSERT_EQ(stmts.size(), 1U);
    EXPECT_EQ(stmts[block_stmt_id.value].statements.front().value, 5U);

    syntax::StmtNodeList moved_stmts(std::move(stmts));
    EXPECT_EQ(stmts.arena_bytes(), 0U);
    EXPECT_EQ(stmts.arena_used_bytes(), 0U);
    EXPECT_EQ(stmts.arena_blocks(), 0U);
    ASSERT_EQ(moved_stmts.size(), 1U);
    EXPECT_EQ(moved_stmts[block_stmt_id.value].kind, syntax::StmtKind::block);
}

TEST(CoreUnit, AstModuleReserveEstimatePreTouchesExpressionArena)
{
    constexpr base::usize ESTIMATED_EXPR_COUNT = 128;

    syntax::AstModule module;
    syntax::AstReserveEstimate estimate;
    estimate.statements = ESTIMATED_EXPR_COUNT / syntax::SYNTAX_AST_RESERVE_EXPRS_PER_STATEMENT;
    module.reserve_for_estimate(estimate);

    const base::usize blocks_after_reserve = module.exprs.arena_blocks();
    EXPECT_GT(blocks_after_reserve, 0U);
    EXPECT_GT(module.exprs.arena_bytes(), 0U);

    for (base::usize index = 0; index < ESTIMATED_EXPR_COUNT; ++index) {
        static_cast<void>(module.push_binary_expr({}, syntax::BinaryOp::add, syntax::ExprId{0}, syntax::ExprId{0}));
    }

    EXPECT_EQ(module.exprs.arena_blocks(), blocks_after_reserve);
}

TEST(CoreUnit, ExprNodeListPayloadAccessorsExposeCompactPayloads)
{
    constexpr base::usize MISSING_EXPR_INDEX = 999;

    syntax::ExprNodeList exprs;

    const syntax::ExprId literal_id = exprs.append_literal(syntax::ExprKind::integer_literal, {}, "42");

    syntax::NameExprPayload name;
    name.scope_name = "math";
    name.text = "add";
    name.type_args = {syntax::TypeId{1}, syntax::TypeId{2}};
    const syntax::ExprId name_id = exprs.append_name({}, name);

    syntax::GenericApplyExprPayload generic;
    generic.callee = name_id;
    generic.type_args = {syntax::TypeId{3}};
    const syntax::ExprId generic_id = exprs.append_generic_apply({}, generic);

    const syntax::ExprId unary_id = exprs.append_unary(syntax::ExprKind::unary, {},
        syntax::UnaryExprPayload{
            syntax::UnaryOp::numeric_negate,
            literal_id,
        });
    const syntax::ExprId try_id = exprs.append_try({}, syntax::TryExprPayload{unary_id});

    const syntax::ExprId binary_id = exprs.append_binary({},
        syntax::BinaryExprPayload{
            syntax::BinaryOp::add,
            literal_id,
            unary_id,
        });

    syntax::CallExprPayload call;
    call.callee = generic_id;
    call.args = {literal_id, binary_id};
    const syntax::ExprId call_id = exprs.append_call(syntax::ExprKind::call, {}, call);

    syntax::IfExprPayload if_expr;
    if_expr.condition = literal_id;
    if_expr.condition_pattern = syntax::PatternId{4};
    if_expr.then_expr = name_id;
    if_expr.else_expr = call_id;
    const syntax::ExprId if_id = exprs.append_if({}, if_expr);

    syntax::BlockExprPayload block;
    block.block = syntax::StmtId{5};
    block.result = if_id;
    const syntax::ExprId block_id = exprs.append_block(syntax::ExprKind::unsafe_block, {}, block);

    syntax::MatchExprPayload match;
    match.value = literal_id;
    match.arms = {
        syntax::MatchArm{syntax::PatternId{6}, syntax::ExprId{7}, name_id, {}},
    };
    const syntax::ExprId match_id = exprs.append_match({}, match);

    syntax::ArrayExprPayload array;
    array.elements = {literal_id, name_id};
    array.repeat_value = unary_id;
    array.repeat_count = binary_id;
    const syntax::ExprId array_id = exprs.append_array({}, array);

    const syntax::ExprId tuple_id = exprs.append_tuple({}, std::vector<syntax::ExprId>{literal_id, call_id});

    syntax::FieldExprPayload field;
    field.object = name_id;
    field.field_name = "value";
    const syntax::ExprId field_id = exprs.append_field({}, field);

    syntax::IndexExprPayload index;
    index.object = field_id;
    index.index = literal_id;
    const syntax::ExprId index_id = exprs.append_index({}, index);

    syntax::SliceExprPayload slice;
    slice.object = field_id;
    slice.start = literal_id;
    slice.end = binary_id;
    const syntax::ExprId slice_id = exprs.append_slice({}, slice);

    syntax::StructLiteralExprPayload struct_literal;
    struct_literal.object = name_id;
    struct_literal.scope_name = "pkg";
    struct_literal.name = "Pair";
    struct_literal.type_args = {syntax::TypeId{8}};
    struct_literal.field_inits = {syntax::FieldInit{"left", literal_id, {}}};
    const syntax::ExprId struct_id = exprs.append_struct_literal({}, struct_literal);

    const syntax::ExprId cast_id = exprs.append_cast_like(syntax::ExprKind::pcast, {},
        syntax::CastExprPayload{
            syntax::TypeId{9},
            field_id,
        });

    const syntax::LiteralExprPayload* const literal_payload = exprs.literal_payload(literal_id.value);
    ASSERT_NE(literal_payload, nullptr);
    EXPECT_EQ(literal_payload->text, "42");

    const syntax::NameExprPayload* const name_payload = exprs.name_payload(name_id.value);
    ASSERT_NE(name_payload, nullptr);
    EXPECT_EQ(name_payload->scope_name, "math");
    EXPECT_EQ(name_payload->text, "add");
    ASSERT_EQ(name_payload->type_args.size(), 2U);
    EXPECT_EQ(name_payload->type_args.back().value, 2U);

    const syntax::GenericApplyExprPayload* const generic_payload = exprs.generic_apply_payload(generic_id.value);
    ASSERT_NE(generic_payload, nullptr);
    EXPECT_EQ(generic_payload->callee.value, name_id.value);
    ASSERT_EQ(generic_payload->type_args.size(), 1U);
    EXPECT_EQ(generic_payload->type_args.front().value, 3U);

    const syntax::UnaryExprPayload* const unary_payload = exprs.unary_payload(unary_id.value);
    ASSERT_NE(unary_payload, nullptr);
    EXPECT_EQ(unary_payload->op, syntax::UnaryOp::numeric_negate);
    EXPECT_EQ(unary_payload->operand.value, literal_id.value);

    EXPECT_EQ(exprs.unary_payload(try_id.value), nullptr);
    const syntax::TryExprPayload* const try_payload = exprs.try_payload(try_id.value);
    ASSERT_NE(try_payload, nullptr);
    EXPECT_EQ(try_payload->operand.value, unary_id.value);

    const syntax::BinaryExprPayload* const binary_payload = exprs.binary_payload(binary_id.value);
    ASSERT_NE(binary_payload, nullptr);
    EXPECT_EQ(binary_payload->op, syntax::BinaryOp::add);
    EXPECT_EQ(binary_payload->lhs.value, literal_id.value);
    EXPECT_EQ(binary_payload->rhs.value, unary_id.value);

    const syntax::CallExprPayload* const call_payload = exprs.call_payload(call_id.value);
    ASSERT_NE(call_payload, nullptr);
    EXPECT_EQ(call_payload->callee.value, generic_id.value);
    ASSERT_EQ(call_payload->args.size(), 2U);
    EXPECT_EQ(call_payload->args.back().value, binary_id.value);

    const syntax::IfExprPayload* const if_payload = exprs.if_payload(if_id.value);
    ASSERT_NE(if_payload, nullptr);
    EXPECT_EQ(if_payload->condition.value, literal_id.value);
    EXPECT_EQ(if_payload->condition_pattern.value, 4U);
    EXPECT_EQ(if_payload->then_expr.value, name_id.value);
    EXPECT_EQ(if_payload->else_expr.value, call_id.value);

    const syntax::BlockExprPayload* const block_payload = exprs.block_payload(block_id.value);
    ASSERT_NE(block_payload, nullptr);
    EXPECT_EQ(block_payload->block.value, 5U);
    EXPECT_EQ(block_payload->result.value, if_id.value);
    const base::SourceRange retagged_range{{99}, 3, 9};
    EXPECT_TRUE(exprs.retag_block_expr(block_id.value, syntax::ExprKind::block_expr, retagged_range));
    EXPECT_EQ(exprs.kind(block_id.value), syntax::ExprKind::block_expr);
    EXPECT_EQ(exprs.range(block_id.value).begin, 3U);
    EXPECT_NE(exprs.block_payload(block_id.value), nullptr);
    EXPECT_FALSE(exprs.retag_block_expr(MISSING_EXPR_INDEX, syntax::ExprKind::unsafe_block, retagged_range));
    EXPECT_FALSE(exprs.retag_block_expr(literal_id.value, syntax::ExprKind::unsafe_block, retagged_range));
    EXPECT_FALSE(exprs.retag_block_expr(block_id.value, syntax::ExprKind::call, retagged_range));

    const syntax::MatchExprPayload* const match_payload = exprs.match_payload(match_id.value);
    ASSERT_NE(match_payload, nullptr);
    EXPECT_EQ(match_payload->value.value, literal_id.value);
    ASSERT_EQ(match_payload->arms.size(), 1U);
    EXPECT_EQ(match_payload->arms.front().value.value, name_id.value);

    const syntax::ArrayExprPayload* const array_payload = exprs.array_payload(array_id.value);
    ASSERT_NE(array_payload, nullptr);
    ASSERT_EQ(array_payload->elements.size(), 2U);
    EXPECT_EQ(array_payload->elements.front().value, literal_id.value);
    EXPECT_EQ(array_payload->repeat_value.value, unary_id.value);
    EXPECT_EQ(array_payload->repeat_count.value, binary_id.value);

    const syntax::AstArenaVector<syntax::ExprId>* const tuple_payload = exprs.tuple_elements(tuple_id.value);
    ASSERT_NE(tuple_payload, nullptr);
    ASSERT_EQ(tuple_payload->size(), 2U);
    EXPECT_EQ(tuple_payload->back().value, call_id.value);

    const syntax::FieldExprPayload* const field_payload = exprs.field_payload(field_id.value);
    ASSERT_NE(field_payload, nullptr);
    EXPECT_EQ(field_payload->object.value, name_id.value);
    EXPECT_EQ(field_payload->field_name, "value");

    const syntax::IndexExprPayload* const index_payload = exprs.index_payload(index_id.value);
    ASSERT_NE(index_payload, nullptr);
    EXPECT_EQ(index_payload->object.value, field_id.value);
    EXPECT_EQ(index_payload->index.value, literal_id.value);

    const syntax::SliceExprPayload* const slice_payload = exprs.slice_payload(slice_id.value);
    ASSERT_NE(slice_payload, nullptr);
    EXPECT_EQ(slice_payload->object.value, field_id.value);
    EXPECT_EQ(slice_payload->start.value, literal_id.value);
    EXPECT_EQ(slice_payload->end.value, binary_id.value);

    const syntax::StructLiteralExprPayload* const struct_payload = exprs.struct_literal_payload(struct_id.value);
    ASSERT_NE(struct_payload, nullptr);
    EXPECT_EQ(struct_payload->object.value, name_id.value);
    EXPECT_EQ(struct_payload->scope_name, "pkg");
    EXPECT_EQ(struct_payload->name, "Pair");
    ASSERT_EQ(struct_payload->type_args.size(), 1U);
    EXPECT_EQ(struct_payload->type_args.front().value, 8U);
    ASSERT_EQ(struct_payload->field_inits.size(), 1U);
    EXPECT_EQ(struct_payload->field_inits.front().name, "left");

    const syntax::CastExprPayload* const cast_payload = exprs.cast_payload(cast_id.value);
    ASSERT_NE(cast_payload, nullptr);
    EXPECT_EQ(cast_payload->type.value, 9U);
    EXPECT_EQ(cast_payload->expr.value, field_id.value);

    EXPECT_EQ(exprs.kind(binary_id.value), syntax::ExprKind::binary);
    EXPECT_EQ(exprs.name_payload(literal_id.value), nullptr);
    const syntax::ExprNodeList& const_exprs = exprs;
    EXPECT_EQ(const_exprs.generic_apply_payload(literal_id.value), nullptr);
    EXPECT_EQ(exprs.try_payload(literal_id.value), nullptr);
    EXPECT_EQ(exprs.binary_payload(literal_id.value), nullptr);
    EXPECT_EQ(const_exprs.call_payload(literal_id.value), nullptr);
    EXPECT_EQ(const_exprs.literal_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.name_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.name_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.generic_apply_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.generic_apply_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.unary_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.unary_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.try_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.try_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.binary_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.binary_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.call_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.call_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.if_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.if_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.block_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.block_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.match_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.match_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.array_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.array_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.tuple_elements(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.tuple_elements(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.field_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.field_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.index_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.index_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.slice_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.slice_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.struct_literal_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.struct_literal_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(const_exprs.cast_payload(MISSING_EXPR_INDEX), nullptr);
    EXPECT_EQ(exprs.cast_payload(MISSING_EXPR_INDEX), nullptr);
}

TEST(CoreUnit, ExprNodeListSettersUpdateCompactPayloads)
{
    constexpr base::SourceRange SET_RANGE{{88}, 2, 6};
    constexpr base::u32 SET_TYPE_ID = 12;

    syntax::ExprNodeList exprs;
    exprs.reserve_touched(1);
    const auto append_slot = [&exprs]() {
        return exprs.append_invalid({});
    };

    const syntax::ExprId invalid_slot = append_slot();
    exprs.set_invalid(invalid_slot.value, SET_RANGE);
    EXPECT_EQ(exprs.kind(invalid_slot.value), syntax::ExprKind::invalid);
    EXPECT_EQ(exprs.range(invalid_slot.value).begin, SET_RANGE.begin);

    syntax::GenericApplyExprPayload generic;
    generic.callee = syntax::ExprId{1};
    generic.type_args = {syntax::TypeId{SET_TYPE_ID}};
    const syntax::ExprId generic_slot = append_slot();
    exprs.set_generic_apply(generic_slot.value, SET_RANGE, generic);
    ASSERT_NE(exprs.generic_apply_payload(generic_slot.value), nullptr);
    EXPECT_EQ(exprs.generic_apply_payload(generic_slot.value)->callee.value, 1U);

    syntax::UnaryExprPayload unary;
    unary.op = syntax::UnaryOp::address_of;
    unary.operand = syntax::ExprId{2};
    const syntax::ExprId unary_slot = append_slot();
    exprs.set_unary(unary_slot.value, syntax::ExprKind::unary, SET_RANGE, unary);
    ASSERT_NE(exprs.unary_payload(unary_slot.value), nullptr);
    EXPECT_EQ(exprs.unary_payload(unary_slot.value)->op, syntax::UnaryOp::address_of);

    syntax::TryExprPayload try_expr;
    try_expr.operand = syntax::ExprId{3};
    const syntax::ExprId try_slot = append_slot();
    exprs.set_try(try_slot.value, SET_RANGE, try_expr);
    ASSERT_NE(exprs.try_payload(try_slot.value), nullptr);
    EXPECT_EQ(exprs.try_payload(try_slot.value)->operand.value, 3U);

    syntax::CallExprPayload call;
    call.callee = syntax::ExprId{4};
    call.args = {syntax::ExprId{5}};
    const syntax::ExprId call_slot = append_slot();
    exprs.set_call(call_slot.value, syntax::ExprKind::call, SET_RANGE, call);
    ASSERT_NE(exprs.call_payload(call_slot.value), nullptr);
    EXPECT_EQ(exprs.call_payload(call_slot.value)->args.front().value, 5U);

    syntax::FieldExprPayload field;
    field.object = syntax::ExprId{6};
    field.field_name = "field";
    const syntax::ExprId field_slot = append_slot();
    exprs.set_field(field_slot.value, SET_RANGE, field);
    ASSERT_NE(exprs.field_payload(field_slot.value), nullptr);
    EXPECT_EQ(exprs.field_payload(field_slot.value)->field_name, "field");

    syntax::IndexExprPayload index;
    index.object = syntax::ExprId{7};
    index.index = syntax::ExprId{8};
    const syntax::ExprId index_slot = append_slot();
    exprs.set_index(index_slot.value, SET_RANGE, index);
    ASSERT_NE(exprs.index_payload(index_slot.value), nullptr);
    EXPECT_EQ(exprs.index_payload(index_slot.value)->index.value, 8U);

    syntax::SliceExprPayload slice;
    slice.object = syntax::ExprId{9};
    slice.start = syntax::ExprId{10};
    slice.end = syntax::ExprId{11};
    const syntax::ExprId slice_slot = append_slot();
    exprs.set_slice(slice_slot.value, SET_RANGE, slice);
    ASSERT_NE(exprs.slice_payload(slice_slot.value), nullptr);
    EXPECT_EQ(exprs.slice_payload(slice_slot.value)->end.value, 11U);

    syntax::StructLiteralExprPayload struct_literal;
    struct_literal.object = syntax::ExprId{12};
    struct_literal.name = "Record";
    struct_literal.type_args = {syntax::TypeId{SET_TYPE_ID}};
    const syntax::ExprId struct_slot = append_slot();
    exprs.set_struct_literal(struct_slot.value, SET_RANGE, struct_literal);
    ASSERT_NE(exprs.struct_literal_payload(struct_slot.value), nullptr);
    EXPECT_EQ(exprs.struct_literal_payload(struct_slot.value)->name, "Record");
}

TEST(CoreUnit, AstModuleExpressionWrapperMutatorsDelegateToNodeStorage)
{
    constexpr base::SourceRange SET_RANGE{{90}, 4, 8};
    constexpr base::usize TOKEN_RESERVE_ESTIMATE = 16;

    syntax::AstModule module;
    module.reserve_for_tokens(TOKEN_RESERVE_ESTIMATE);
    const syntax::TypeId type_id = push_primitive_type(module, syntax::PrimitiveTypeKind::i32);
    const syntax::ExprId literal_id = module.push_literal_expr(syntax::ExprKind::integer_literal, SET_RANGE, "1");
    const syntax::StmtId block_stmt = push_expr_stmt(module, literal_id);

    const syntax::TryExprPayload try_push_payload{literal_id};
    const syntax::ExprId pushed_try = module.push_try_expr(SET_RANGE, try_push_payload);
    ASSERT_NE(module.exprs.try_payload(pushed_try.value), nullptr);
    EXPECT_EQ(module.exprs.try_payload(pushed_try.value)->operand.value, literal_id.value);

    const syntax::BlockExprPayload block_payload{block_stmt, literal_id};
    const syntax::ExprId pushed_block = module.push_block_expr(syntax::ExprKind::block_expr, SET_RANGE, block_payload);
    ASSERT_NE(module.exprs.block_payload(pushed_block.value), nullptr);
    EXPECT_EQ(module.exprs.block_payload(pushed_block.value)->block.value, block_stmt.value);

    const auto append_slot = [&module]() {
        return module.push_invalid_expr({});
    };

    const syntax::ExprId invalid_slot = append_slot();
    module.set_invalid_expr(invalid_slot.value, SET_RANGE);
    EXPECT_EQ(module.exprs.kind(invalid_slot.value), syntax::ExprKind::invalid);
    EXPECT_EQ(module.exprs.range(invalid_slot.value).begin, SET_RANGE.begin);

    syntax::GenericApplyExprPayload generic_payload;
    generic_payload.callee = literal_id;
    generic_payload.type_args = {type_id};
    const syntax::ExprId generic_slot = append_slot();
    module.set_generic_apply_expr(generic_slot.value, SET_RANGE, generic_payload);
    ASSERT_NE(module.exprs.generic_apply_payload(generic_slot.value), nullptr);
    EXPECT_EQ(module.exprs.generic_apply_payload(generic_slot.value)->callee.value, literal_id.value);

    syntax::UnaryExprPayload unary_payload;
    unary_payload.op = syntax::UnaryOp::bitwise_not;
    unary_payload.operand = literal_id;
    const syntax::ExprId unary_slot = append_slot();
    module.set_unary_expr(unary_slot.value, syntax::ExprKind::unary, SET_RANGE, unary_payload);
    ASSERT_NE(module.exprs.unary_payload(unary_slot.value), nullptr);
    EXPECT_EQ(module.exprs.unary_payload(unary_slot.value)->op, syntax::UnaryOp::bitwise_not);

    const syntax::ExprId try_slot = append_slot();
    module.set_try_expr(try_slot.value, SET_RANGE, try_push_payload);
    ASSERT_NE(module.exprs.try_payload(try_slot.value), nullptr);
    EXPECT_EQ(module.exprs.try_payload(try_slot.value)->operand.value, literal_id.value);

    syntax::CallExprPayload call_payload;
    call_payload.callee = literal_id;
    call_payload.args = {pushed_try};
    const syntax::ExprId call_slot = append_slot();
    module.set_call_expr(call_slot.value, syntax::ExprKind::call, SET_RANGE, call_payload);
    ASSERT_NE(module.exprs.call_payload(call_slot.value), nullptr);
    EXPECT_EQ(module.exprs.call_payload(call_slot.value)->args.front().value, pushed_try.value);

    syntax::FieldExprPayload field_payload;
    field_payload.object = literal_id;
    field_payload.field_name = "field";
    const syntax::ExprId field_slot = append_slot();
    module.set_field_expr(field_slot.value, SET_RANGE, field_payload);
    ASSERT_NE(module.exprs.field_payload(field_slot.value), nullptr);
    EXPECT_EQ(module.exprs.field_payload(field_slot.value)->field_name, "field");

    syntax::IndexExprPayload index_payload;
    index_payload.object = literal_id;
    index_payload.index = pushed_try;
    const syntax::ExprId index_slot = append_slot();
    module.set_index_expr(index_slot.value, SET_RANGE, index_payload);
    ASSERT_NE(module.exprs.index_payload(index_slot.value), nullptr);
    EXPECT_EQ(module.exprs.index_payload(index_slot.value)->index.value, pushed_try.value);

    syntax::SliceExprPayload slice_payload;
    slice_payload.object = literal_id;
    slice_payload.start = pushed_try;
    slice_payload.end = pushed_block;
    const syntax::ExprId slice_slot = append_slot();
    module.set_slice_expr(slice_slot.value, SET_RANGE, slice_payload);
    ASSERT_NE(module.exprs.slice_payload(slice_slot.value), nullptr);
    EXPECT_EQ(module.exprs.slice_payload(slice_slot.value)->end.value, pushed_block.value);

    syntax::StructLiteralExprPayload struct_payload;
    struct_payload.object = literal_id;
    struct_payload.name = "Record";
    struct_payload.type_args = {type_id};
    const syntax::ExprId struct_slot = append_slot();
    module.set_struct_literal_expr(struct_slot.value, SET_RANGE, struct_payload);
    ASSERT_NE(module.exprs.struct_literal_payload(struct_slot.value), nullptr);
    EXPECT_EQ(module.exprs.struct_literal_payload(struct_slot.value)->name, "Record");

    const syntax::ExprId scalar_unary_slot = append_slot();
    module.set_unary_expr(
        scalar_unary_slot.value, syntax::ExprKind::unary, SET_RANGE, syntax::UnaryOp::logical_not, literal_id);
    ASSERT_NE(module.exprs.unary_payload(scalar_unary_slot.value), nullptr);
    EXPECT_EQ(module.exprs.unary_payload(scalar_unary_slot.value)->operand.value, literal_id.value);

    const syntax::ExprId scalar_try_slot = append_slot();
    module.set_try_expr(scalar_try_slot.value, SET_RANGE, literal_id);
    ASSERT_NE(module.exprs.try_payload(scalar_try_slot.value), nullptr);
    EXPECT_EQ(module.exprs.try_payload(scalar_try_slot.value)->operand.value, literal_id.value);

    const syntax::ExprId scalar_field_slot = append_slot();
    module.set_field_expr(scalar_field_slot.value, SET_RANGE, literal_id, "scalar_field");
    ASSERT_NE(module.exprs.field_payload(scalar_field_slot.value), nullptr);
    EXPECT_EQ(module.exprs.field_payload(scalar_field_slot.value)->field_name, "scalar_field");
    EXPECT_TRUE(syntax::is_valid(module.find_identifier("scalar_field")));

    const syntax::ExprId scalar_index_slot = append_slot();
    module.set_index_expr(scalar_index_slot.value, SET_RANGE, literal_id, pushed_try);
    ASSERT_NE(module.exprs.index_payload(scalar_index_slot.value), nullptr);
    EXPECT_EQ(module.exprs.index_payload(scalar_index_slot.value)->index.value, pushed_try.value);

    const syntax::ExprId scalar_slice_slot = append_slot();
    module.set_slice_expr(scalar_slice_slot.value, SET_RANGE, literal_id, pushed_try, pushed_block);
    ASSERT_NE(module.exprs.slice_payload(scalar_slice_slot.value), nullptr);
    EXPECT_EQ(module.exprs.slice_payload(scalar_slice_slot.value)->end.value, pushed_block.value);

    syntax::AstModule* const module_alias = &module;
    module = *module_alias;
    EXPECT_EQ(module.exprs.size(), scalar_slice_slot.value + 1U);
}

TEST(CoreUnit, ExprNodeListCopyPreservesEveryCompactExpressionPayload)
{
    constexpr base::SourceRange COPY_RANGE{{77}, 1, 5};
    constexpr base::u32 COPY_TYPE_ID = 9;

    syntax::ExprNodeList exprs;
    const syntax::ExprId invalid_id = exprs.append_invalid(COPY_RANGE);
    const syntax::ExprId literal_id = exprs.append_literal(syntax::ExprKind::integer_literal, COPY_RANGE, "7");
    const syntax::ExprId name_id = exprs.append_name(COPY_RANGE, "math", COPY_RANGE, "value", syntax::IdentId{10},
        syntax::IdentId{11}, std::vector<syntax::TypeId>{syntax::TypeId{COPY_TYPE_ID}});
    const syntax::ExprId generic_id =
        exprs.append_generic_apply(COPY_RANGE, name_id, std::vector<syntax::TypeId>{syntax::TypeId{COPY_TYPE_ID}});
    const syntax::ExprId unary_id =
        exprs.append_unary(syntax::ExprKind::unary, COPY_RANGE, syntax::UnaryOp::bitwise_not, literal_id);
    const syntax::ExprId try_id = exprs.append_try(COPY_RANGE, unary_id);
    const syntax::ExprId binary_id = exprs.append_binary(COPY_RANGE, syntax::BinaryOp::mul, literal_id, unary_id);
    const syntax::ExprId call_id = exprs.append_call(
        syntax::ExprKind::call, COPY_RANGE, generic_id, std::vector<syntax::ExprId>{literal_id, binary_id});
    const syntax::ExprId raw_string_id = exprs.append_call(
        syntax::ExprKind::str_from_bytes_unchecked, COPY_RANGE, call_id, std::vector<syntax::ExprId>{literal_id});
    const syntax::ExprId if_id = exprs.append_if(COPY_RANGE, literal_id, syntax::PatternId{1}, call_id, raw_string_id);
    const syntax::ExprId block_id =
        exprs.append_block(syntax::ExprKind::block_expr, COPY_RANGE, syntax::StmtId{2}, if_id);
    const syntax::ExprId unsafe_block_id =
        exprs.append_block(syntax::ExprKind::unsafe_block, COPY_RANGE, syntax::StmtId{3}, block_id);
    const syntax::ExprId match_id = exprs.append_match(COPY_RANGE, literal_id,
        std::vector<syntax::MatchArm>{
            syntax::MatchArm{syntax::PatternId{4}, syntax::ExprId{5}, call_id, COPY_RANGE},
        });
    const syntax::ExprId array_id =
        exprs.append_array(COPY_RANGE, std::vector<syntax::ExprId>{literal_id, name_id}, unary_id, binary_id);
    const syntax::ExprId tuple_id = exprs.append_tuple(COPY_RANGE, std::vector<syntax::ExprId>{literal_id, call_id});
    const syntax::ExprId field_id = exprs.append_field(COPY_RANGE, name_id, "field", syntax::IdentId{6});
    const syntax::ExprId index_id = exprs.append_index(COPY_RANGE, field_id, literal_id);
    const syntax::ExprId slice_id = exprs.append_slice(COPY_RANGE, field_id, literal_id, binary_id);
    const syntax::ExprId struct_id = exprs.append_struct_literal(COPY_RANGE, name_id, "pkg", COPY_RANGE, "Pair",
        syntax::IdentId{7}, syntax::IdentId{8}, std::vector<syntax::TypeId>{syntax::TypeId{COPY_TYPE_ID}},
        std::vector<syntax::FieldInit>{
            syntax::FieldInit{"left", literal_id, COPY_RANGE, syntax::IdentId{9}},
        });
    const syntax::ExprId unknown_call_kind_id = exprs.append_call(
        static_cast<syntax::ExprKind>(99), COPY_RANGE, literal_id, std::vector<syntax::ExprId>{literal_id});

    std::vector<syntax::ExprId> cast_ids;
    for (const syntax::ExprKind kind : {
             syntax::ExprKind::cast,
             syntax::ExprKind::pcast,
             syntax::ExprKind::bcast,
             syntax::ExprKind::size_of,
             syntax::ExprKind::align_of,
             syntax::ExprKind::ptr_addr,
             syntax::ExprKind::paddr,
             syntax::ExprKind::slice_data,
             syntax::ExprKind::slice_len,
             syntax::ExprKind::str_data,
             syntax::ExprKind::str_byte_len,
             syntax::ExprKind::str_is_valid_utf8,
             syntax::ExprKind::str_from_utf8_checked,
         }) {
        cast_ids.push_back(exprs.append_cast_like(kind, COPY_RANGE, syntax::TypeId{COPY_TYPE_ID}, literal_id));
    }

    syntax::ExprNodeList copied = exprs;
    syntax::ExprNodeList assigned;
    assigned = exprs;
    const syntax::ExprNodeList& copied_view = copied;

    ASSERT_EQ(copied_view.size(), exprs.size());
    EXPECT_EQ(copied_view.kind(invalid_id.value), syntax::ExprKind::invalid);
    ASSERT_NE(copied_view.literal_payload(literal_id.value), nullptr);
    EXPECT_EQ(copied_view.literal_payload(literal_id.value)->text, "7");
    ASSERT_NE(copied_view.name_payload(name_id.value), nullptr);
    EXPECT_EQ(copied_view.name_payload(name_id.value)->text, "value");
    ASSERT_NE(copied_view.generic_apply_payload(generic_id.value), nullptr);
    EXPECT_EQ(copied_view.generic_apply_payload(generic_id.value)->callee.value, name_id.value);
    ASSERT_NE(copied_view.unary_payload(unary_id.value), nullptr);
    EXPECT_EQ(copied_view.unary_payload(unary_id.value)->op, syntax::UnaryOp::bitwise_not);
    ASSERT_NE(copied_view.try_payload(try_id.value), nullptr);
    EXPECT_EQ(copied_view.try_payload(try_id.value)->operand.value, unary_id.value);
    ASSERT_NE(copied_view.binary_payload(binary_id.value), nullptr);
    EXPECT_EQ(copied_view.binary_payload(binary_id.value)->op, syntax::BinaryOp::mul);
    ASSERT_NE(copied_view.call_payload(call_id.value), nullptr);
    EXPECT_EQ(copied_view.call_payload(call_id.value)->args.back().value, binary_id.value);
    ASSERT_NE(copied_view.call_payload(raw_string_id.value), nullptr);
    EXPECT_EQ(copied_view.call_payload(raw_string_id.value)->callee.value, call_id.value);
    ASSERT_NE(copied_view.if_payload(if_id.value), nullptr);
    EXPECT_EQ(copied_view.if_payload(if_id.value)->else_expr.value, raw_string_id.value);
    ASSERT_NE(copied_view.block_payload(block_id.value), nullptr);
    EXPECT_EQ(copied_view.block_payload(block_id.value)->result.value, if_id.value);
    ASSERT_NE(copied_view.block_payload(unsafe_block_id.value), nullptr);
    EXPECT_EQ(copied_view.block_payload(unsafe_block_id.value)->block.value, 3U);
    ASSERT_NE(copied_view.match_payload(match_id.value), nullptr);
    EXPECT_EQ(copied_view.match_payload(match_id.value)->arms.front().value.value, call_id.value);
    ASSERT_NE(copied_view.array_payload(array_id.value), nullptr);
    EXPECT_EQ(copied_view.array_payload(array_id.value)->repeat_count.value, binary_id.value);
    ASSERT_NE(copied_view.tuple_elements(tuple_id.value), nullptr);
    EXPECT_EQ(copied_view.tuple_elements(tuple_id.value)->back().value, call_id.value);
    ASSERT_NE(copied_view.field_payload(field_id.value), nullptr);
    EXPECT_EQ(copied_view.field_payload(field_id.value)->field_name, "field");
    ASSERT_NE(copied_view.index_payload(index_id.value), nullptr);
    EXPECT_EQ(copied_view.index_payload(index_id.value)->index.value, literal_id.value);
    ASSERT_NE(copied_view.slice_payload(slice_id.value), nullptr);
    EXPECT_EQ(copied_view.slice_payload(slice_id.value)->end.value, binary_id.value);
    ASSERT_NE(copied_view.struct_literal_payload(struct_id.value), nullptr);
    EXPECT_EQ(copied_view.struct_literal_payload(struct_id.value)->field_inits.front().name, "left");
    EXPECT_EQ(copied_view.kind(unknown_call_kind_id.value), syntax::ExprKind::invalid);
    for (const syntax::ExprId cast_id : cast_ids) {
        ASSERT_NE(copied_view.cast_payload(cast_id.value), nullptr);
        EXPECT_EQ(copied_view.cast_payload(cast_id.value)->type.value, COPY_TYPE_ID);
    }
    EXPECT_EQ(assigned.size(), exprs.size());
}

TEST(CoreUnit, AstDumpPrintsPackageVisibilityForInternalLattice)
{
    syntax::AstModule module;
    module.module_path.parts = {"pkg_visibility_dump"};

    syntax::ModulePath import_path;
    import_path.parts = {"pkg", "internal"};
    module.imports.push_back(syntax::ImportDecl{
        import_path,
        "internal",
        {},
        syntax::Visibility::package_,
        true,
    });

    const syntax::TypeId i32_type = push_primitive_type(module, syntax::PrimitiveTypeKind::i32);

    syntax::ItemNode package_struct;
    package_struct.kind = syntax::ItemKind::struct_decl;
    package_struct.name = "PackageBox";
    package_struct.visibility = syntax::Visibility::package_;
    package_struct.fields.push_back(syntax::FieldDecl{"value", i32_type, {}, syntax::Visibility::package_});
    module.items.push_back(package_struct);

    syntax::ItemNode package_function;
    package_function.kind = syntax::ItemKind::fn_decl;
    package_function.name = "make";
    package_function.visibility = syntax::Visibility::package_;
    package_function.return_type = i32_type;
    module.items.push_back(package_function);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "pub(package) import pkg.internal as internal",
            "item #0 pub(package) struct PackageBox",
            "field pub(package) value : i32",
            "item #1 pub(package) fn make",
        });
}

TEST(CoreUnit, AstDumpPrintsSelectiveUseReexports)
{
    syntax::AstModule module;
    module.module_path.parts = {"use_reexport_dump"};

    syntax::ModulePath type_path;
    type_path.parts = {"pkg", "api"};
    module.reexports.push_back(syntax::UseDecl{
        type_path,
        "Value",
        {},
        "Value",
        {},
        syntax::Visibility::public_,
        true,
    });

    syntax::ModulePath function_path;
    function_path.parts = {"pkg", "api"};
    module.reexports.push_back(syntax::UseDecl{
        function_path,
        "make",
        {},
        "make_value",
        {},
        syntax::Visibility::package_,
        true,
    });

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "pub use pkg.api.Value",
            "pub(package) use pkg.api.make as make_value",
        });
}

TEST(CoreUnit, AstDumpPrintsOriginGenericParamsAndReferenceOrigins)
{
    syntax::AstModule module;
    module.module_path.parts = {"origin_dump"};

    syntax::TypeNode type_param;
    type_param.kind = syntax::TypeKind::named;
    type_param.name = "T";
    const syntax::TypeId type_param_id = module.push_type(type_param);

    syntax::TypeNode reference;
    reference.kind = syntax::TypeKind::reference;
    reference.pointer_mutability = syntax::PointerMutability::mut;
    reference.pointee = type_param_id;
    reference.reference_origin.explicit_ = true;
    reference.reference_origin.names = {"data"};
    const syntax::TypeId reference_id = module.push_type(reference);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "view";
    function.generic_params = {
        syntax::GenericParamDecl{"T", {}},
        syntax::GenericParamDecl{"data", {}, syntax::INVALID_IDENT_ID, syntax::GenericParamKind::origin},
    };
    function.return_type = reference_id;
    static_cast<void>(module.push_item(std::move(function)));

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "item #0 priv fn view[T, origin data]",
            "return &mut[data] T",
        });
}

TEST(CoreUnit, AstDumpPrintsConstGenericParamsArgumentsAndArrayLengths)
{
    syntax::AstModule module;
    module.module_path.parts = {"const_generic_dump"};

    syntax::TypeNode t_type_node;
    t_type_node.kind = syntax::TypeKind::named;
    t_type_node.name = "T";
    const syntax::TypeId t_type = module.push_type(t_type_node);
    const syntax::TypeId usize_type = push_primitive_type(module, syntax::PrimitiveTypeKind::usize);
    const syntax::TypeId i32_type = push_primitive_type(module, syntax::PrimitiveTypeKind::i32);
    const syntax::ExprId n_expr = push_name_expr(module, "N");

    syntax::TypeNode array_type;
    array_type.kind = syntax::TypeKind::array;
    array_type.array_element = t_type;
    array_type.array_length = syntax::ArrayLengthDecl{
        syntax::ArrayLengthKind::const_expr,
        0,
        n_expr,
        {},
    };
    const syntax::TypeId array_t_type = module.push_type(array_type);

    const syntax::ExprId four_expr =
        module.push_literal_expr(syntax::ExprKind::integer_literal, {}, std::string_view{"4"});
    syntax::TypeNode view_i32_4;
    view_i32_4.kind = syntax::TypeKind::named;
    view_i32_4.name = "ArrayView";
    view_i32_4.type_args = {i32_type};
    view_i32_4.generic_args = {
        syntax::GenericArgDecl{syntax::GenericArgKind::type, i32_type, syntax::INVALID_EXPR_ID, {}},
        syntax::GenericArgDecl{syntax::GenericArgKind::const_expr, syntax::INVALID_TYPE_ID, four_expr, {}},
    };
    const syntax::TypeId view_i32_4_type = module.push_type(view_i32_4);

    syntax::ItemNode view;
    view.kind = syntax::ItemKind::struct_decl;
    view.name = "ArrayView";
    view.generic_params = {
        syntax::GenericParamDecl{"T", {}, syntax::INVALID_IDENT_ID, syntax::GenericParamKind::type},
        syntax::GenericParamDecl{"N", {}, syntax::INVALID_IDENT_ID, syntax::GenericParamKind::const_, usize_type},
    };
    view.fields = {syntax::FieldDecl{"value", array_t_type, {}}};
    static_cast<void>(module.push_item(view));

    syntax::ItemNode use;
    use.kind = syntax::ItemKind::fn_decl;
    use.name = "use";
    use.params = {syntax::ParamDecl{"value", view_i32_4_type, {}}};
    use.return_type = usize_type;
    static_cast<void>(module.push_item(use));

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "item #0 priv struct ArrayView[T, const N: usize]",
            "field priv value : [N]T",
            "param value : ArrayView[i32, 4]",
            "return usize",
        });
}

TEST(CoreUnit, AstDumpCoversInvalidAndFallbackLabels)
{
    std::vector<Token> tokens = {
        Token{TokenKind::invalid, {{6}, 0, 1}, "?"},
        Token{static_cast<TokenKind>(255), {{6}, 1, 2}, ""},
    };
    const std::string token_dump = syntax::dump_tokens(tokens);
    expect_contains_all(token_dump,
        {
            "invalid",
            "unknown",
        });

    syntax::AstModule module;
    syntax::TypeNode i32_type;
    i32_type.kind = syntax::TypeKind::primitive;
    i32_type.primitive = syntax::PrimitiveTypeKind::i32;
    module.types.push_back(i32_type);
    syntax::TypeNode bool_type = i32_type;
    bool_type.primitive = syntax::PrimitiveTypeKind::bool_;
    module.types.push_back(bool_type);

    syntax::PatternNode scoped_pattern;
    scoped_pattern.kind = syntax::PatternKind::enum_case;
    scoped_pattern.scoped = true;
    scoped_pattern.enum_name = "Color";
    scoped_pattern.case_name = "red";
    scoped_pattern.binding_names = {"value", "shade"};
    module.patterns.push_back(scoped_pattern);

    static_cast<void>(module.push_invalid_expr({}));

    static_cast<void>(module.exprs.append_cast_like(static_cast<syntax::ExprKind>(255), {}, syntax::CastExprPayload{}));

    syntax::StructLiteralExprPayload struct_literal;
    struct_literal.name = "Pair";
    static_cast<void>(module.push_struct_literal_expr({}, struct_literal));

    static_cast<void>(module.push_match_expr({},
        syntax::MatchExprPayload{
            syntax::ExprId{2},
            {
                syntax::MatchArm{syntax::PatternId{0}, syntax::INVALID_EXPR_ID, syntax::ExprId{1}, {}},
                syntax::MatchArm{syntax::INVALID_PATTERN_ID, syntax::INVALID_EXPR_ID, syntax::ExprId{99}, {}},
            },
        }));

    syntax::StmtNode unknown_stmt;
    unknown_stmt.kind = static_cast<syntax::StmtKind>(255);
    unknown_stmt.init = syntax::ExprId{2};
    module.stmts.push_back(unknown_stmt);

    syntax::StmtNode expr_stmt;
    expr_stmt.kind = syntax::StmtKind::expr;
    expr_stmt.init = syntax::ExprId{3};
    module.stmts.push_back(expr_stmt);

    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = syntax::ExprId{0};
    if_stmt.else_if = syntax::StmtId{0};
    module.stmts.push_back(if_stmt);

    syntax::StmtNode for_stmt;
    for_stmt.kind = syntax::StmtKind::for_;
    for_stmt.condition = syntax::ExprId{0};
    for_stmt.for_init = syntax::StmtId{1};
    for_stmt.for_update = syntax::StmtId{0};
    module.stmts.push_back(for_stmt);

    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.statements = {syntax::StmtId{99}, syntax::StmtId{1}, syntax::StmtId{2}, syntax::StmtId{3}};
    module.stmts.push_back(block);

    syntax::ItemNode broken_struct;
    broken_struct.kind = syntax::ItemKind::struct_decl;
    broken_struct.name = "Broken";
    broken_struct.fields.push_back(syntax::FieldDecl{"bad", syntax::INVALID_TYPE_ID, {}, syntax::Visibility::public_});
    module.items.push_back(broken_struct);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "body";
    function.body = syntax::StmtId{4};
    module.items.push_back(function);

    syntax::ItemNode extern_block;
    extern_block.kind = syntax::ItemKind::extern_block;
    extern_block.extern_items = {syntax::INVALID_ITEM_ID, syntax::ItemId{99}};
    module.items.push_back(extern_block);

    syntax::ItemNode unknown_item;
    unknown_item.kind = static_cast<syntax::ItemKind>(255);
    unknown_item.name = "mystery";
    module.items.push_back(unknown_item);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "field pub bad : <invalid-type>",
            "stmt <invalid>",
            "stmt #0 unknown",
            "expr #0 invalid",
            "expr #1 unknown",
            "struct_literal Pair",
            "match_arm Color.red(value, shade)",
            "match_arm <invalid-pattern>",
            "expr <invalid>",
            "item <invalid>",
            "item #3 priv unknown mystery",
            "for",
        });
}

TEST(CoreUnit, AstDumpCoversSelectorTypePatternAndExpressionLabels)
{
    std::vector<Token> tokens = {
        Token{TokenKind::kw_trait, {{1}, 0, 5}, "trait"},
        Token{TokenKind::kw_where, {{1}, 6, 11}, "where"},
        Token{TokenKind::kw_in, {{1}, 12, 14}, "in"},
        Token{TokenKind::kw_is, {{1}, 15, 17}, "is"},
        Token{TokenKind::kw_strvalid, {{1}, 18, 26}, "strvalid"},
        Token{TokenKind::kw_strfromutf8, {{1}, 27, 39}, "strfromutf8"},
        Token{TokenKind::colon_colon, {{1}, 40, 42}, "::"},
        Token{TokenKind::plus_plus, {{1}, 43, 45}, "++"},
        Token{TokenKind::minus_minus, {{1}, 46, 48}, "--"},
        Token{TokenKind::star_equal, {{1}, 49, 51}, "*="},
        Token{TokenKind::slash_equal, {{1}, 52, 54}, "/="},
        Token{TokenKind::percent_equal, {{1}, 55, 57}, "%="},
        Token{TokenKind::amp_equal, {{1}, 58, 60}, "&="},
        Token{TokenKind::pipe_equal, {{1}, 61, 63}, "|="},
        Token{TokenKind::caret_equal, {{1}, 64, 66}, "^="},
        Token{TokenKind::less_less_equal, {{1}, 67, 70}, "<<="},
        Token{TokenKind::greater_greater_equal, {{1}, 71, 74}, ">>="},
    };
    const std::string token_dump = syntax::dump_tokens(tokens);
    expect_contains_all(token_dump,
        {
            "kw_trait",
            "kw_where",
            "kw_in",
            "kw_is",
            "kw_strvalid",
            "kw_strfromutf8",
            "colon_colon",
            "plus_plus",
            "minus_minus",
            "star_equal",
            "slash_equal",
            "percent_equal",
            "amp_equal",
            "pipe_equal",
            "caret_equal",
            "less_less_equal",
            "greater_greater_equal",
        });

    syntax::AstModule module;
    module.module_path.parts = {"app", "main"};
    syntax::ModulePath core_mem_path;
    core_mem_path.parts = {"core", "mem"};
    module.imports.push_back(syntax::ImportDecl{
        core_mem_path,
        {},
        {},
        syntax::Visibility::private_,
        false,
    });

    const syntax::TypeId i32_type = push_primitive_type(module, syntax::PrimitiveTypeKind::i32);
    const syntax::TypeId bool_type = push_primitive_type(module, syntax::PrimitiveTypeKind::bool_);
    syntax::TypeNode scoped_type;
    scoped_type.kind = syntax::TypeKind::named;
    scoped_type.scope_name = "pkg";
    scoped_type.name = "Scoped";
    const syntax::TypeId scoped_type_id = module.push_type(scoped_type);
    syntax::TypeNode reference_type;
    reference_type.kind = syntax::TypeKind::reference;
    reference_type.pointer_mutability = syntax::PointerMutability::mut;
    reference_type.pointee = scoped_type_id;
    const syntax::TypeId reference_type_id = module.push_type(reference_type);
    syntax::TypeNode fn_type;
    fn_type.kind = syntax::TypeKind::function;
    fn_type.function_call_conv = syntax::FunctionCallConv::c;
    fn_type.function_is_unsafe = true;
    fn_type.function_is_variadic = true;
    fn_type.function_params = {reference_type_id, i32_type};
    fn_type.function_return = bool_type;
    const syntax::TypeId fn_type_id = module.push_type(fn_type);

    syntax::PatternNode x_binding;
    x_binding.kind = syntax::PatternKind::binding;
    x_binding.binding_name = "x";
    const syntax::PatternId x_pattern = module.push_pattern(x_binding);
    syntax::PatternNode y_binding = x_binding;
    y_binding.binding_name = "y";
    const syntax::PatternId y_pattern = module.push_pattern(y_binding);
    syntax::PatternNode tuple_pattern;
    tuple_pattern.kind = syntax::PatternKind::tuple;
    tuple_pattern.elements = {x_pattern, y_pattern};
    const syntax::PatternId tuple_pattern_id = module.push_pattern(tuple_pattern);
    syntax::PatternNode const_pattern;
    const_pattern.kind = syntax::PatternKind::const_;
    const_pattern.binding_name = "LIMIT";
    const syntax::PatternId const_pattern_id = module.push_pattern(const_pattern);
    syntax::PatternNode enum_payload_pattern;
    enum_payload_pattern.kind = syntax::PatternKind::enum_case;
    enum_payload_pattern.scoped = true;
    enum_payload_pattern.enum_name = "Option";
    enum_payload_pattern.case_name = "some";
    enum_payload_pattern.payload_patterns = {x_pattern, y_pattern};
    const syntax::PatternId enum_payload_pattern_id = module.push_pattern(enum_payload_pattern);
    syntax::PatternNode or_pattern;
    or_pattern.kind = syntax::PatternKind::or_pattern;
    or_pattern.alternatives = {const_pattern_id, enum_payload_pattern_id};
    const syntax::PatternId or_pattern_id = module.push_pattern(or_pattern);

    const syntax::ExprId scoped_name_id = push_name_expr(module, "make", "pkg", {i32_type, bool_type});
    const syntax::ExprId float_literal_id = push_float_literal(module, "1.0");
    const syntax::ExprId call_id = module.push_call_expr(syntax::ExprKind::call, {},
        syntax::CallExprPayload{
            scoped_name_id,
            {float_literal_id},
            {},
        });
    const syntax::ExprId generic_apply_id = module.push_generic_apply_expr({},
        syntax::GenericApplyExprPayload{
            scoped_name_id,
            {reference_type_id, fn_type_id},
            {},
        });
    const syntax::ExprId field_id = module.push_field_expr({},
        syntax::FieldExprPayload{
            scoped_name_id,
            "fd",
            syntax::INVALID_IDENT_ID,
        });
    const syntax::ExprId index_id = module.push_index_expr({},
        syntax::IndexExprPayload{
            scoped_name_id,
            float_literal_id,
        });
    const syntax::ExprId slice_id = module.push_slice_expr({},
        syntax::SliceExprPayload{
            scoped_name_id,
            float_literal_id,
            call_id,
        });
    const syntax::ExprId typed_struct_literal_id = module.push_struct_literal_expr({},
        syntax::StructLiteralExprPayload{
            syntax::INVALID_EXPR_ID,
            "pkg",
            {},
            "Box",
            syntax::INVALID_IDENT_ID,
            syntax::INVALID_IDENT_ID,
            {i32_type, bool_type},
            {syntax::FieldInit{"value", call_id, {}}},
            {},
        });
    const syntax::ExprId selector_struct_literal_id = module.push_struct_literal_expr({},
        syntax::StructLiteralExprPayload{
            field_id,
            {},
            {},
            {},
            syntax::INVALID_IDENT_ID,
            syntax::INVALID_IDENT_ID,
            {},
            {syntax::FieldInit{"fd", index_id, {}}},
            {},
        });
    const syntax::ExprId if_expr_id = module.push_if_expr({},
        syntax::IfExprPayload{
            scoped_name_id,
            tuple_pattern_id,
            call_id,
            selector_struct_literal_id,
        });
    const syntax::ExprId strvalid_expr_id = module.push_cast_like_expr(syntax::ExprKind::str_is_valid_utf8, {},
        syntax::CastExprPayload{
            syntax::INVALID_TYPE_ID,
            scoped_name_id,
        });
    const syntax::ExprId strfromutf8_expr_id = module.push_cast_like_expr(syntax::ExprKind::str_from_utf8_checked, {},
        syntax::CastExprPayload{
            syntax::INVALID_TYPE_ID,
            scoped_name_id,
        });
    const syntax::ExprId try_expr_id = module.push_try_expr({}, call_id);

    syntax::StmtNode for_range;
    for_range.kind = syntax::StmtKind::for_range;
    for_range.name = "item";
    for_range.range_start = scoped_name_id;
    for_range.range_end = call_id;
    for_range.range_step = float_literal_id;
    const syntax::StmtId for_range_id = module.push_stmt(for_range);

    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.statements = {
        push_expr_stmt(module, call_id),
        push_expr_stmt(module, generic_apply_id),
        push_expr_stmt(module, field_id),
        push_expr_stmt(module, index_id),
        push_expr_stmt(module, slice_id),
        push_expr_stmt(module, typed_struct_literal_id),
        push_expr_stmt(module, if_expr_id),
        push_expr_stmt(module, strvalid_expr_id),
        push_expr_stmt(module, strfromutf8_expr_id),
        push_expr_stmt(module, try_expr_id),
        for_range_id,
    };
    const syntax::StmtId body_id = module.push_stmt(block);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "dumped";
    function.generic_params = {syntax::GenericParamDecl{"T", {}}, syntax::GenericParamDecl{"U", {}}};
    syntax::GenericConstraintDecl t_constraint;
    t_constraint.param_name = "T";
    t_constraint.capability_names = {"copy", "drop"};
    syntax::GenericConstraintDecl u_constraint;
    u_constraint.param_name = "U";
    u_constraint.capability_names = {"fmt"};
    function.where_constraints = {t_constraint, u_constraint};
    function.return_type = fn_type_id;
    function.body = body_id;
    static_cast<void>(module.push_item(function));

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "LegacyPayload";
    enum_item.enum_cases = {
        syntax::EnumCaseDecl{"wrapped", reference_type_id, {}, {}, {}},
    };
    static_cast<void>(module.push_item(enum_item));

    syntax::ItemNode pattern_user;
    pattern_user.kind = syntax::ItemKind::fn_decl;
    pattern_user.name = "patterns";
    syntax::StmtNode let_stmt;
    let_stmt.kind = syntax::StmtKind::let;
    let_stmt.pattern = or_pattern_id;
    let_stmt.init = scoped_name_id;
    pattern_user.body = module.push_stmt(let_stmt);
    static_cast<void>(module.push_item(pattern_user));

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast,
        {
            "priv import core.mem",
            "expr #1 float_literal `1.0`",
            "expr #2 call",
            "expr #3 generic_apply[&mut pkg.Scoped, unsafe extern c fn(&mut pkg.Scoped, i32, ...) -> bool]",
            "expr #4 field .fd",
            "expr #5 index",
            "expr #6 slice",
            "expr #7 struct_literal pkg.Box[i32, bool]",
            "expr #8 struct_literal <selector>",
            "condition_pattern (x, y)",
            "expr #10 strvalid",
            "expr #11 strfromutf8",
            "expr #12 try_expr",
            "stmt #0 for_range item",
            "item #0 priv fn dumped[T, U] where T: copy + drop, U: fmt",
            "return unsafe extern c fn(&mut pkg.Scoped, i32, ...) -> bool",
            "case wrapped(&mut pkg.Scoped)",
            "const LIMIT | Option.some(x, y)",
        });
}

} // namespace aurex::test
