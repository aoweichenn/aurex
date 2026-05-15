#include <aurex/syntax/ast_dump.hpp>
#include <aurex/syntax/token.hpp>
#include <support/frontend_test_support.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace aurex::test {
namespace {

using syntax::Token;
using syntax::TokenKind;

constexpr std::size_t AST_COMPACT_HEADER_MAX_BYTES = 32;
constexpr std::size_t AST_FAT_NODE_HEADER_RATIO = 4;

[[nodiscard]] syntax::TypeId push_primitive_type(
    syntax::AstModule& module,
    const syntax::PrimitiveTypeKind kind
) {
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.primitive = kind;
    return module.push_type(type);
}

[[nodiscard]] syntax::StmtId push_expr_stmt(syntax::AstModule& module, const syntax::ExprId expr) {
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::expr;
    stmt.init = expr;
    return module.push_stmt(stmt);
}

} // namespace

TEST(CoreUnit, AstStorageUsesCompactHeaders) {
    EXPECT_LE(sizeof(syntax::TypeNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LE(sizeof(syntax::ExprNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LE(sizeof(syntax::PatternNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LE(sizeof(syntax::StmtNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LE(sizeof(syntax::ItemNodeHeader), AST_COMPACT_HEADER_MAX_BYTES);
    EXPECT_LT(sizeof(syntax::TypeNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::TypeNode));
    EXPECT_LT(sizeof(syntax::ExprNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::ExprNode));
    EXPECT_LT(sizeof(syntax::PatternNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::PatternNode));
    EXPECT_LT(sizeof(syntax::StmtNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::StmtNode));
    EXPECT_LT(sizeof(syntax::ItemNodeHeader) * AST_FAT_NODE_HEADER_RATIO, sizeof(syntax::ItemNode));
}

TEST(CoreUnit, AstModuleInternsNativeIdentifierIdsAcrossNodesAndMetadata) {
    syntax::AstModule module;
    module.module_path.parts = {"app", "main"};
    syntax::ImportDecl import;
    import.path.parts = {"core", "mem"};
    import.alias = "mem";
    module.imports.push_back(import);
    syntax::ModuleInfo module_info;
    module_info.path.parts = {"lib", "math"};
    syntax::ResolvedImport resolved_import;
    resolved_import.module = syntax::ModuleId {0};
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

    syntax::ExprNode name_expr;
    name_expr.kind = syntax::ExprKind::name;
    name_expr.scope_name = "mem";
    name_expr.text = "value";
    const syntax::ExprId name_expr_id = module.push_expr(name_expr);

    syntax::ExprNode field_expr;
    field_expr.kind = syntax::ExprKind::field;
    field_expr.object = name_expr_id;
    field_expr.field_name = "len";
    const syntax::ExprId field_expr_id = module.push_expr(field_expr);

    syntax::ExprNode struct_literal;
    struct_literal.kind = syntax::ExprKind::struct_literal;
    struct_literal.scope_name = "core";
    struct_literal.struct_name = "Buffer";
    syntax::FieldInit count_init;
    count_init.name = "count";
    count_init.value = name_expr_id;
    struct_literal.field_inits = {count_init};
    const syntax::ExprId struct_literal_id = module.push_expr(struct_literal);

    syntax::PostfixOp postfix_select;
    postfix_select.kind = syntax::PostfixOpKind::select;
    postfix_select.name = "next";
    syntax::ExprNode postfix_expr;
    postfix_expr.kind = syntax::ExprKind::postfix_chain;
    postfix_expr.postfix_base = name_expr_id;
    postfix_expr.postfix_ops = {postfix_select};
    const syntax::ExprId postfix_expr_id = module.push_expr(postfix_expr);

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
    function.generic_params = {syntax::GenericParamDecl {"T", {}}};
    syntax::GenericConstraintDecl copy_constraint;
    copy_constraint.param_name = "T";
    copy_constraint.capability_names = {"Copy"};
    function.where_constraints = {copy_constraint};
    function.params = {syntax::ParamDecl {"value", type_id, {}}};
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

    const syntax::PostfixChainExprPayload* const postfix_payload =
        module.exprs.postfix_chain_payload(postfix_expr_id.value);
    ASSERT_NE(postfix_payload, nullptr);
    ASSERT_EQ(postfix_payload->ops.size(), 1U);
    EXPECT_EQ(postfix_payload->ops.front().name_id, module.find_identifier("next"));

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

    ASSERT_EQ(module.module_path.part_ids.size(), 2U);
    EXPECT_EQ(module.module_path.part_ids.front(), module.find_identifier("app"));
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
}

TEST(CoreUnit, CompactAstStorageRoundTripsAndMovesPayloads) {
    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_is_unsafe = true;
    function_type.function_params = {syntax::TypeId {1}, syntax::TypeId {2}};
    function_type.function_return = syntax::TypeId {3};

    syntax::TypeNodeList types;
    types.push_back(function_type);
    EXPECT_TRUE(types[0].function_is_unsafe);
    EXPECT_EQ(types[0].function_params.size(), 2U);
    syntax::TypeNode moved_type = types.take(0);
    EXPECT_TRUE(moved_type.function_is_unsafe);
    EXPECT_EQ(moved_type.function_params.back().value, 2U);

    syntax::ExprNode call_expr;
    call_expr.kind = syntax::ExprKind::call;
    call_expr.callee = syntax::ExprId {4};
    call_expr.args = {syntax::ExprId {5}, syntax::ExprId {6}};

    syntax::ExprNodeList exprs;
    exprs.push_back(call_expr);
    EXPECT_EQ(exprs[0].args.size(), 2U);
    syntax::ExprNode moved_expr = exprs.take(0);
    EXPECT_EQ(moved_expr.callee.value, 4U);
    EXPECT_EQ(moved_expr.args.back().value, 6U);

    syntax::ExprNode typed_name;
    typed_name.kind = syntax::ExprKind::name;
    typed_name.text = "make";
    typed_name.type_args = {syntax::TypeId {8}, syntax::TypeId {9}};
    syntax::ExprNodeList names;
    names.push_back(typed_name);
    EXPECT_EQ(names[0].type_args.back().value, 9U);
    syntax::ExprNode moved_name = names.take(0);
    EXPECT_EQ(moved_name.type_args.front().value, 8U);

    syntax::ExprNode bool_expr;
    bool_expr.kind = syntax::ExprKind::bool_literal;
    bool_expr.text = "true";
    syntax::ExprNode null_expr;
    null_expr.kind = syntax::ExprKind::null_literal;
    null_expr.text = "null";
    syntax::ExprNodeList literals;
    literals.push_back(bool_expr);
    literals.push_back(null_expr);
    EXPECT_EQ(literals[0].text, "true");
    EXPECT_EQ(literals[1].text, "null");

    syntax::PatternNode enum_pattern;
    enum_pattern.kind = syntax::PatternKind::enum_case;
    enum_pattern.scoped = true;
    enum_pattern.case_name = "some";
    enum_pattern.payload_patterns = {syntax::PatternId {7}};
    enum_pattern.binding_names = {"value"};

    syntax::PatternNodeList patterns;
    patterns.push_back(enum_pattern);
    EXPECT_EQ(patterns[0].binding_names.front(), "value");
    syntax::PatternNode moved_pattern = patterns.take(0);
    EXPECT_TRUE(moved_pattern.scoped);
    EXPECT_EQ(moved_pattern.payload_patterns.front().value, 7U);
    EXPECT_EQ(moved_pattern.binding_names.front(), "value");

    syntax::StmtNode block_stmt;
    block_stmt.kind = syntax::StmtKind::block;
    block_stmt.statements = {syntax::StmtId {10}, syntax::StmtId {11}, syntax::StmtId {12}};
    syntax::StmtNodeList stmts;
    stmts.push_back(block_stmt);
    EXPECT_EQ(stmts[0].statements.size(), 3U);
    syntax::StmtNode moved_stmt = stmts.take(0);
    EXPECT_EQ(moved_stmt.statements.back().value, 12U);

    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "map";
    function_item.generic_params = {syntax::GenericParamDecl {"T", {}}};
    syntax::GenericConstraintDecl copy_constraint;
    copy_constraint.param_name = "T";
    copy_constraint.capability_names = {"Copy"};
    function_item.where_constraints = {copy_constraint};
    function_item.params = {syntax::ParamDecl {"value", syntax::TypeId {13}, {}}};
    function_item.return_type = syntax::TypeId {14};
    function_item.body = syntax::StmtId {15};
    function_item.impl_type = syntax::TypeId {16};
    function_item.is_unsafe = true;
    function_item.abi_name = "aurex_map";

    syntax::ItemNodeList items;
    items.push_back(function_item);
    items.set_visibility(0, syntax::Visibility::public_);
    EXPECT_EQ(items[0].visibility, syntax::Visibility::public_);
    EXPECT_TRUE(items[0].is_unsafe);
    EXPECT_EQ(items[0].where_constraints.front().capability_names.front(), "Copy");
    syntax::ItemNode moved_item = items.take(0);
    EXPECT_EQ(moved_item.params.front().type.value, 13U);
    EXPECT_EQ(moved_item.impl_type.value, 16U);
    EXPECT_EQ(moved_item.abi_name, "aurex_map");

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "Option";
    enum_item.enum_base_type = syntax::TypeId {17};
    enum_item.enum_cases = {
        syntax::EnumCaseDecl {"some", syntax::TypeId {18}, {syntax::TypeId {18}}, {}, {}},
        syntax::EnumCaseDecl {"none", syntax::INVALID_TYPE_ID, {}, "0", {}},
    };

    syntax::ItemNodeList enum_items;
    enum_items.push_back(enum_item);
    EXPECT_EQ(enum_items[0].enum_cases.front().payload_types.front().value, 18U);
    syntax::ItemNode moved_enum = enum_items.take(0);
    EXPECT_EQ(moved_enum.enum_base_type.value, 17U);
    EXPECT_EQ(moved_enum.enum_cases.back().value_text, "0");
}

TEST(CoreUnit, ExprNodeListPayloadAccessorsExposeCompactPayloads) {
    syntax::ExprNodeList exprs;

    syntax::ExprNode literal;
    literal.kind = syntax::ExprKind::integer_literal;
    literal.text = "42";
    const syntax::ExprId literal_id = exprs.append(literal);

    syntax::ExprNode name;
    name.kind = syntax::ExprKind::name;
    name.scope_name = "math";
    name.text = "add";
    name.type_args = {syntax::TypeId {1}, syntax::TypeId {2}};
    const syntax::ExprId name_id = exprs.append(name);

    syntax::ExprNode generic;
    generic.kind = syntax::ExprKind::generic_apply;
    generic.callee = name_id;
    generic.type_args = {syntax::TypeId {3}};
    const syntax::ExprId generic_id = exprs.append(generic);

    syntax::ExprNode unary;
    unary.kind = syntax::ExprKind::unary;
    unary.unary_op = syntax::UnaryOp::numeric_negate;
    unary.unary_operand = literal_id;
    const syntax::ExprId unary_id = exprs.append(unary);

    syntax::ExprNode binary;
    binary.kind = syntax::ExprKind::binary;
    binary.binary_op = syntax::BinaryOp::add;
    binary.binary_lhs = literal_id;
    binary.binary_rhs = unary_id;
    const syntax::ExprId binary_id = exprs.append(binary);

    syntax::ExprNode call;
    call.kind = syntax::ExprKind::call;
    call.callee = generic_id;
    call.args = {literal_id, binary_id};
    const syntax::ExprId call_id = exprs.append(call);

    syntax::ExprNode if_expr;
    if_expr.kind = syntax::ExprKind::if_expr;
    if_expr.condition = literal_id;
    if_expr.condition_pattern = syntax::PatternId {4};
    if_expr.then_expr = name_id;
    if_expr.else_expr = call_id;
    const syntax::ExprId if_id = exprs.append(if_expr);

    syntax::ExprNode block;
    block.kind = syntax::ExprKind::unsafe_block;
    block.block = syntax::StmtId {5};
    block.block_result = if_id;
    const syntax::ExprId block_id = exprs.append(block);

    syntax::ExprNode match;
    match.kind = syntax::ExprKind::match_expr;
    match.match_value = literal_id;
    match.match_arms = {
        syntax::MatchArm {syntax::PatternId {6}, syntax::ExprId {7}, name_id, {}},
    };
    const syntax::ExprId match_id = exprs.append(match);

    syntax::ExprNode array;
    array.kind = syntax::ExprKind::array_literal;
    array.array_elements = {literal_id, name_id};
    array.array_repeat_value = unary_id;
    array.array_repeat_count = binary_id;
    const syntax::ExprId array_id = exprs.append(array);

    syntax::ExprNode tuple;
    tuple.kind = syntax::ExprKind::tuple_literal;
    tuple.tuple_elements = {literal_id, call_id};
    const syntax::ExprId tuple_id = exprs.append(tuple);

    syntax::PostfixOp postfix_op;
    postfix_op.kind = syntax::PostfixOpKind::call;
    postfix_op.args = {literal_id, name_id};
    syntax::ExprNode postfix;
    postfix.kind = syntax::ExprKind::postfix_chain;
    postfix.postfix_base = name_id;
    postfix.postfix_ops = {postfix_op};
    const syntax::ExprId postfix_id = exprs.append(postfix);

    syntax::ExprNode field;
    field.kind = syntax::ExprKind::field;
    field.object = name_id;
    field.field_name = "value";
    const syntax::ExprId field_id = exprs.append(field);

    syntax::ExprNode index;
    index.kind = syntax::ExprKind::index;
    index.object = field_id;
    index.index = literal_id;
    const syntax::ExprId index_id = exprs.append(index);

    syntax::ExprNode slice;
    slice.kind = syntax::ExprKind::slice;
    slice.object = field_id;
    slice.slice_start = literal_id;
    slice.slice_end = binary_id;
    const syntax::ExprId slice_id = exprs.append(slice);

    syntax::ExprNode struct_literal;
    struct_literal.kind = syntax::ExprKind::struct_literal;
    struct_literal.object = name_id;
    struct_literal.scope_name = "pkg";
    struct_literal.struct_name = "Pair";
    struct_literal.type_args = {syntax::TypeId {8}};
    struct_literal.field_inits = {syntax::FieldInit {"left", literal_id, {}}};
    const syntax::ExprId struct_id = exprs.append(struct_literal);

    syntax::ExprNode cast;
    cast.kind = syntax::ExprKind::pcast;
    cast.cast_type = syntax::TypeId {9};
    cast.cast_expr = field_id;
    const syntax::ExprId cast_id = exprs.append(cast);

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
    const base::SourceRange retagged_range {{99}, 3, 9};
    EXPECT_TRUE(exprs.retag_block_expr(block_id.value, syntax::ExprKind::block_expr, retagged_range));
    EXPECT_EQ(exprs.kind(block_id.value), syntax::ExprKind::block_expr);
    EXPECT_EQ(exprs.range(block_id.value).begin, 3U);
    EXPECT_NE(exprs.block_payload(block_id.value), nullptr);
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

    const std::vector<syntax::ExprId>* const tuple_payload = exprs.tuple_elements(tuple_id.value);
    ASSERT_NE(tuple_payload, nullptr);
    ASSERT_EQ(tuple_payload->size(), 2U);
    EXPECT_EQ(tuple_payload->back().value, call_id.value);

    const syntax::PostfixChainExprPayload* const postfix_payload = exprs.postfix_chain_payload(postfix_id.value);
    ASSERT_NE(postfix_payload, nullptr);
    EXPECT_EQ(postfix_payload->base.value, name_id.value);
    ASSERT_EQ(postfix_payload->ops.size(), 1U);
    EXPECT_EQ(postfix_payload->ops.front().args.back().value, name_id.value);
    syntax::PostfixChainExprPayload moved_postfix = exprs.take_postfix_chain_payload(postfix_id.value);
    EXPECT_EQ(moved_postfix.base.value, name_id.value);
    ASSERT_EQ(moved_postfix.ops.size(), 1U);
    EXPECT_EQ(moved_postfix.ops.front().args.front().value, literal_id.value);
    EXPECT_TRUE(exprs.take_postfix_chain_payload(literal_id.value).ops.empty());

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
    EXPECT_EQ(exprs.literal_payload(999), nullptr);
}

TEST(CoreUnit, AstDumpCoversInvalidAndFallbackLabels) {
    std::vector<Token> tokens = {
        Token {TokenKind::invalid, {{6}, 0, 1}, "?"},
        Token {static_cast<TokenKind>(255), {{6}, 1, 2}, ""},
    };
    const std::string token_dump = syntax::dump_tokens(tokens);
    expect_contains_all(token_dump, {
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

    syntax::ExprNode invalid_expr;
    invalid_expr.kind = syntax::ExprKind::invalid;
    module.exprs.push_back(invalid_expr);

    syntax::ExprNode unknown_expr;
    unknown_expr.kind = static_cast<syntax::ExprKind>(255);
    module.exprs.push_back(unknown_expr);

    syntax::ExprNode struct_literal;
    struct_literal.kind = syntax::ExprKind::struct_literal;
    struct_literal.struct_name = "Pair";
    module.exprs.push_back(struct_literal);

    syntax::ExprNode match_expr;
    match_expr.kind = syntax::ExprKind::match_expr;
    match_expr.match_value = syntax::ExprId {2};
    match_expr.match_arms = {
        syntax::MatchArm {syntax::PatternId {0}, syntax::INVALID_EXPR_ID, syntax::ExprId {1}, {}},
        syntax::MatchArm {syntax::INVALID_PATTERN_ID, syntax::INVALID_EXPR_ID, syntax::ExprId {99}, {}},
    };
    module.exprs.push_back(match_expr);

    syntax::StmtNode unknown_stmt;
    unknown_stmt.kind = static_cast<syntax::StmtKind>(255);
    unknown_stmt.init = syntax::ExprId {2};
    module.stmts.push_back(unknown_stmt);

    syntax::StmtNode expr_stmt;
    expr_stmt.kind = syntax::StmtKind::expr;
    expr_stmt.init = syntax::ExprId {3};
    module.stmts.push_back(expr_stmt);

    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = syntax::ExprId {0};
    if_stmt.else_if = syntax::StmtId {0};
    module.stmts.push_back(if_stmt);

    syntax::StmtNode for_stmt;
    for_stmt.kind = syntax::StmtKind::for_;
    for_stmt.condition = syntax::ExprId {0};
    for_stmt.for_init = syntax::StmtId {1};
    for_stmt.for_update = syntax::StmtId {0};
    module.stmts.push_back(for_stmt);

    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.statements = {syntax::StmtId {99}, syntax::StmtId {1}, syntax::StmtId {2}, syntax::StmtId {3}};
    module.stmts.push_back(block);

    syntax::ItemNode broken_struct;
    broken_struct.kind = syntax::ItemKind::struct_decl;
    broken_struct.name = "Broken";
    broken_struct.fields.push_back(syntax::FieldDecl {"bad", syntax::INVALID_TYPE_ID, {}, syntax::Visibility::public_});
    module.items.push_back(broken_struct);

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "body";
    function.body = syntax::StmtId {4};
    module.items.push_back(function);

    syntax::ItemNode extern_block;
    extern_block.kind = syntax::ItemKind::extern_block;
    extern_block.extern_items = {syntax::INVALID_ITEM_ID, syntax::ItemId {99}};
    module.items.push_back(extern_block);

    syntax::ItemNode unknown_item;
    unknown_item.kind = static_cast<syntax::ItemKind>(255);
    unknown_item.name = "mystery";
    module.items.push_back(unknown_item);

    const std::string ast = syntax::dump_ast(module);
    expect_contains_all(ast, {
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

TEST(CoreUnit, AstDumpCoversSelectorTypePatternAndExpressionLabels) {
    std::vector<Token> tokens = {
        Token {TokenKind::kw_where, {{1}, 0, 5}, "where"},
        Token {TokenKind::kw_in, {{1}, 6, 8}, "in"},
        Token {TokenKind::kw_is, {{1}, 9, 11}, "is"},
        Token {TokenKind::kw_strvalid, {{1}, 12, 20}, "strvalid"},
        Token {TokenKind::kw_strfromutf8, {{1}, 21, 33}, "strfromutf8"},
        Token {TokenKind::colon_colon, {{1}, 34, 36}, "::"},
        Token {TokenKind::plus_plus, {{1}, 37, 39}, "++"},
        Token {TokenKind::minus_minus, {{1}, 40, 42}, "--"},
        Token {TokenKind::star_equal, {{1}, 43, 45}, "*="},
        Token {TokenKind::slash_equal, {{1}, 46, 48}, "/="},
        Token {TokenKind::percent_equal, {{1}, 49, 51}, "%="},
        Token {TokenKind::amp_equal, {{1}, 52, 54}, "&="},
        Token {TokenKind::pipe_equal, {{1}, 55, 57}, "|="},
        Token {TokenKind::caret_equal, {{1}, 58, 60}, "^="},
        Token {TokenKind::less_less_equal, {{1}, 61, 64}, "<<="},
        Token {TokenKind::greater_greater_equal, {{1}, 65, 68}, ">>="},
    };
    const std::string token_dump = syntax::dump_tokens(tokens);
    expect_contains_all(token_dump, {
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
    module.imports.push_back(syntax::ImportDecl {
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

    syntax::ExprNode scoped_name;
    scoped_name.kind = syntax::ExprKind::name;
    scoped_name.scope_name = "pkg";
    scoped_name.text = "make";
    scoped_name.type_args = {i32_type, bool_type};
    const syntax::ExprId scoped_name_id = module.push_expr(scoped_name);
    syntax::ExprNode float_literal;
    float_literal.kind = syntax::ExprKind::float_literal;
    float_literal.text = "1.0";
    const syntax::ExprId float_literal_id = module.push_expr(float_literal);
    syntax::ExprNode call;
    call.kind = syntax::ExprKind::call;
    call.callee = scoped_name_id;
    call.args = {float_literal_id};
    const syntax::ExprId call_id = module.push_expr(call);
    syntax::ExprNode generic_apply;
    generic_apply.kind = syntax::ExprKind::generic_apply;
    generic_apply.callee = scoped_name_id;
    generic_apply.type_args = {reference_type_id, fn_type_id};
    const syntax::ExprId generic_apply_id = module.push_expr(generic_apply);
    syntax::ExprNode field;
    field.kind = syntax::ExprKind::field;
    field.object = scoped_name_id;
    field.field_name = "fd";
    const syntax::ExprId field_id = module.push_expr(field);
    syntax::ExprNode index;
    index.kind = syntax::ExprKind::index;
    index.object = scoped_name_id;
    index.index = float_literal_id;
    const syntax::ExprId index_id = module.push_expr(index);
    syntax::ExprNode slice;
    slice.kind = syntax::ExprKind::slice;
    slice.object = scoped_name_id;
    slice.slice_start = float_literal_id;
    slice.slice_end = call_id;
    const syntax::ExprId slice_id = module.push_expr(slice);
    syntax::ExprNode typed_struct_literal;
    typed_struct_literal.kind = syntax::ExprKind::struct_literal;
    typed_struct_literal.scope_name = "pkg";
    typed_struct_literal.struct_name = "Box";
    typed_struct_literal.type_args = {i32_type, bool_type};
    typed_struct_literal.field_inits = {syntax::FieldInit {"value", call_id, {}}};
    const syntax::ExprId typed_struct_literal_id = module.push_expr(typed_struct_literal);
    syntax::ExprNode selector_struct_literal;
    selector_struct_literal.kind = syntax::ExprKind::struct_literal;
    selector_struct_literal.object = field_id;
    selector_struct_literal.field_inits = {syntax::FieldInit {"fd", index_id, {}}};
    const syntax::ExprId selector_struct_literal_id = module.push_expr(selector_struct_literal);
    syntax::ExprNode if_expr;
    if_expr.kind = syntax::ExprKind::if_expr;
    if_expr.condition = scoped_name_id;
    if_expr.condition_pattern = tuple_pattern_id;
    if_expr.then_expr = call_id;
    if_expr.else_expr = selector_struct_literal_id;
    const syntax::ExprId if_expr_id = module.push_expr(if_expr);
    syntax::ExprNode strvalid_expr;
    strvalid_expr.kind = syntax::ExprKind::str_is_valid_utf8;
    strvalid_expr.args = {scoped_name_id, float_literal_id};
    const syntax::ExprId strvalid_expr_id = module.push_expr(strvalid_expr);
    syntax::ExprNode strfromutf8_expr = strvalid_expr;
    strfromutf8_expr.kind = syntax::ExprKind::str_from_utf8_checked;
    const syntax::ExprId strfromutf8_expr_id = module.push_expr(strfromutf8_expr);
    syntax::ExprNode try_expr;
    try_expr.kind = syntax::ExprKind::try_expr;
    try_expr.callee = call_id;
    const syntax::ExprId try_expr_id = module.push_expr(try_expr);

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
    function.generic_params = {syntax::GenericParamDecl {"T", {}}, syntax::GenericParamDecl {"U", {}}};
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
        syntax::EnumCaseDecl {"wrapped", reference_type_id, {}, {}, {}},
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
    expect_contains_all(ast, {
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
