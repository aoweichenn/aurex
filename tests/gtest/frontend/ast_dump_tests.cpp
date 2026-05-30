#include <aurex/syntax/ast_dump.hpp>
#include <aurex/syntax/token.hpp>

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
    function_item.abi_name = "aurex_map";

    syntax::ItemNodeList items;
    items.push_back(function_item);
    EXPECT_GT(items.arena_blocks(), 0U);
    EXPECT_GT(items.arena_bytes(), 0U);
    items.set_visibility(0, syntax::Visibility::public_);
    EXPECT_EQ(items[0].visibility, syntax::Visibility::public_);
    EXPECT_TRUE(items[0].is_unsafe);
    EXPECT_EQ(items[0].where_constraints.front().capability_names.front(), "Copy");
    syntax::ItemNode moved_item = items.take(0);
    EXPECT_EQ(moved_item.params.front().type.value, 13U);
    EXPECT_EQ(moved_item.impl_type.value, 16U);
    EXPECT_EQ(moved_item.trait_type.value, 17U);
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
    syntax::ItemNodeList source_items;
    source_items.push_back(function_item);
    syntax::ItemNodeList target_items;
    target_items.push_back(syntax::ItemNode{});
    target_items = std::move(source_items);
    EXPECT_EQ(target_items[0].generic_params.front().name, "T");
    EXPECT_EQ(target_items[0].params.front().type.value, 10U);
    EXPECT_EQ(target_items[0].trait_type.value, 12U);
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
    const syntax::ExprId try_id = exprs.append_try({}, unary_id);

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
        });
    const syntax::ExprId generic_apply_id = module.push_generic_apply_expr({},
        syntax::GenericApplyExprPayload{
            scoped_name_id,
            {reference_type_id, fn_type_id},
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
