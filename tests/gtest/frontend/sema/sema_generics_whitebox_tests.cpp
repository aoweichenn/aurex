#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>

#include <gtest/frontend/sema/sema_whitebox_test_support.hpp>

namespace aurex::test {
namespace {

constexpr base::SourceId SEMA_CONST_GENERIC_TEST_SOURCE_ID{716};
constexpr std::string_view SEMA_CONST_GENERIC_TEST_INTERNAL_CONTRACT_PREFIX = "semantic AST contract violation";

[[nodiscard]] syntax::AstModule parse_const_generic_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(SEMA_CONST_GENERIC_TEST_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        ADD_FAILURE() << parsed.error().message;
        return {};
    }
    if (diagnostics.has_error()) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        return {};
    }
    return parsed.take_value();
}

[[nodiscard]] std::string analyze_const_generic_source_failure(const std::string_view source)
{
    syntax::AstModule module = parse_const_generic_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();

    std::string output;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        output += diagnostic.message;
        output += '\n';
        for (const base::DiagnosticChild& child : diagnostic.children) {
            output += child.message;
            output += '\n';
        }
    }
    if (result) {
        ADD_FAILURE() << "expected semantic analysis to fail";
    } else {
        output += result.error().message;
        output += '\n';
    }
    return output;
}

} // namespace

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
    const TypeId usize_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::usize));

    syntax::TypeNode pointer_type;
    pointer_type.kind = syntax::TypeKind::pointer;
    pointer_type.pointee = generic_type;
    const TypeId pointer_generic_type = module.push_type(pointer_type);

    const ExprId array_length_expr = module.push_name_expr({}, "N");
    syntax::TypeNode const_array_type;
    const_array_type.kind = syntax::TypeKind::array;
    const_array_type.array_element = generic_type;
    const_array_type.array_length = syntax::ArrayLengthDecl{
        syntax::ArrayLengthKind::const_expr,
        0,
        array_length_expr,
        {},
    };
    const TypeId array_generic_type = module.push_type(const_array_type);

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
    function_type.function_return = array_generic_type;
    const TypeId function_handle_type = module.push_type(function_type);

    const ExprId type_const_arg_expr = push_integer_text(module, "4");
    syntax::TypeNode box_type_node = named_node("Box");
    box_type_node.type_args = {function_handle_type};
    box_type_node.generic_args = {
        syntax::GenericArgDecl{syntax::GenericArgKind::type, function_handle_type, syntax::INVALID_EXPR_ID, {}},
        syntax::GenericArgDecl{syntax::GenericArgKind::const_expr, syntax::INVALID_TYPE_ID, type_const_arg_expr, {}},
    };
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
    const ExprId name_const_arg_expr = push_integer_text(module, "5");
    syntax::NameExprPayload name_with_mixed_args_payload;
    name_with_mixed_args_payload.text = "with_const";
    name_with_mixed_args_payload.type_args = module.make_expr_list<syntax::TypeId>();
    name_with_mixed_args_payload.type_args.push_back(i32_type);
    name_with_mixed_args_payload.generic_args = module.make_expr_list<syntax::GenericArgDecl>();
    name_with_mixed_args_payload.generic_args.push_back(
        syntax::GenericArgDecl{syntax::GenericArgKind::type, i32_type, syntax::INVALID_EXPR_ID, {}});
    name_with_mixed_args_payload.generic_args.push_back(syntax::GenericArgDecl{
        syntax::GenericArgKind::const_expr,
        syntax::INVALID_TYPE_ID,
        name_const_arg_expr,
        {},
    });
    const ExprId name_with_mixed_args = module.push_name_expr({}, std::move(name_with_mixed_args_payload));
    const ExprId unused_expr = push_integer_text(module, "99");
    const ExprId callee = module.push_name_expr({}, "callee", std::vector<TypeId>{function_handle_type});
    const ExprId generic_apply = push_generic_apply(module, callee, {i32_type});
    const ExprId apply_const_arg_expr = push_integer_text(module, "6");
    syntax::GenericApplyExprPayload mixed_apply_payload;
    mixed_apply_payload.callee = callee;
    mixed_apply_payload.type_args = module.make_expr_list<syntax::TypeId>();
    mixed_apply_payload.type_args.push_back(i32_type);
    mixed_apply_payload.generic_args = module.make_expr_list<syntax::GenericArgDecl>();
    mixed_apply_payload.generic_args.push_back(
        syntax::GenericArgDecl{syntax::GenericArgKind::type, i32_type, syntax::INVALID_EXPR_ID, {}});
    mixed_apply_payload.generic_args.push_back(syntax::GenericArgDecl{
        syntax::GenericArgKind::const_expr,
        syntax::INVALID_TYPE_ID,
        apply_const_arg_expr,
        {},
    });
    const ExprId mixed_generic_apply = module.push_generic_apply_expr({}, std::move(mixed_apply_payload));
    const ExprId try_expr = module.push_try_expr({}, generic_apply);
    const ExprId bool_expr = push_bool(module, "true");
    const ExprId binary_expr = push_binary(module, syntax::BinaryOp::add, name_with_type_arg, generic_apply);
    const ExprId call_expr = push_call(module, callee, {binary_expr, try_expr, name_with_mixed_args, mixed_generic_apply});

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
    const ExprId struct_const_arg_expr = push_integer_text(module, "7");
    syntax::StructLiteralExprPayload struct_literal_payload;
    struct_literal_payload.object = push_name(module, "Box");
    struct_literal_payload.name = "Box";
    struct_literal_payload.type_args = module.make_expr_list<syntax::TypeId>();
    struct_literal_payload.type_args.push_back(box_function_type);
    struct_literal_payload.generic_args = module.make_expr_list<syntax::GenericArgDecl>();
    struct_literal_payload.generic_args.push_back(
        syntax::GenericArgDecl{syntax::GenericArgKind::type, box_function_type, syntax::INVALID_EXPR_ID, {}});
    struct_literal_payload.generic_args.push_back(syntax::GenericArgDecl{
        syntax::GenericArgKind::const_expr,
        syntax::INVALID_TYPE_ID,
        struct_const_arg_expr,
        {},
    });
    struct_literal_payload.field_inits = module.make_expr_list<syntax::FieldInit>();
    struct_literal_payload.field_inits.push_back(syntax::FieldInit{"field", slice_expr, {}});
    const ExprId struct_literal_expr = module.push_struct_literal_expr({}, std::move(struct_literal_payload));
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
    generic_function.generic_params = {
        syntax::GenericParamDecl{"T", {}, module.intern_identifier("T"), syntax::GenericParamKind::type},
        syntax::GenericParamDecl{"N", {}, module.intern_identifier("N"), syntax::GenericParamKind::const_, usize_type},
    };
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
    EXPECT_TRUE(info.expr_span.contains(array_length_expr.value));
    EXPECT_TRUE(info.expr_span.contains(type_const_arg_expr.value));
    EXPECT_TRUE(info.expr_span.contains(name_const_arg_expr.value));
    EXPECT_TRUE(info.expr_span.contains(apply_const_arg_expr.value));
    EXPECT_TRUE(info.expr_span.contains(struct_const_arg_expr.value));
    EXPECT_TRUE(info.expr_span.contains(name_with_mixed_args.value));
    EXPECT_TRUE(info.type_span.contains(usize_type.value));
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
    ASSERT_TRUE(checked_result) << diagnostic_messages(diagnostics) << checked_result.error().message;
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
    EXPECT_EQ(sema::function_display_name(checked.types, signature), "id<i32>");
    EXPECT_TRUE(checked.functions.contains(signature.semantic_key));
    EXPECT_EQ(checked.functions.at(signature.semantic_key).generic_instance_key, instance.generic_instance_key);
    EXPECT_NE(sema::dump_checked_module(checked).find("id<i32>"), std::string::npos);
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

TEST(CoreUnit, SemanticWhiteBoxGenericBuiltinTypeOperandsRetainInstanceSideTables)
{
    constexpr std::string_view source =
        "module generic_builtin_side_tables;\n"
        "fn builtin_ops<T>(value: T, addr: usize) -> usize where T: Sized + Copy {\n"
        "  let size: usize = sizeof<T>();\n"
        "  let align: usize = alignof<T>();\n"
        "  let raw: *const T = unsafe { ptrat<*const T>(addr) };\n"
        "  let casted: *const T = unsafe { ptrcast<*const T>(raw) };\n"
        "  let recast: *const T = unsafe { bitcast<*const T>(casted) };\n"
        "  if ptraddr(recast) == 0usize { return size; }\n"
        "  return size + align;\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let value: i32 = 1;\n"
        "  return (builtin_ops<i32>(value, ptraddr(&value))) as i32;\n"
        "}\n";

    syntax::AstModule module = parse_const_generic_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << diagnostic_messages(diagnostics) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    const sema::GenericFunctionInstanceInfo* builtin_ops = nullptr;
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        if (instance.signature.name == "builtin_ops") {
            builtin_ops = &instance;
            break;
        }
    }
    ASSERT_NE(builtin_ops, nullptr);
    EXPECT_EQ(sema::function_display_name(checked.types, builtin_ops->signature), "builtin_ops<i32>");

    const sema::GenericSideTables& side_tables = builtin_ops->side_tables;
    EXPECT_TRUE(side_tables.local_dense);
    EXPECT_FALSE(side_tables.syntax_type_handles.empty());
    EXPECT_TRUE(side_tables.sparse_syntax_type_handles.empty());

    auto side_table_has_type_display = [&](const std::string_view expected) {
        for (const TypeHandle handle : side_tables.syntax_type_handles) {
            if (sema::is_valid(handle) && checked.types.display_name(handle) == expected) {
                return true;
            }
        }
        for (const auto& entry : side_tables.sparse_syntax_type_handles) {
            if (sema::is_valid(entry.second) && checked.types.display_name(entry.second) == expected) {
                return true;
            }
        }
        return false;
    };

    auto side_table_has_expr_display = [&](const std::string_view expected) {
        for (const TypeHandle handle : side_tables.expr_types) {
            if (sema::is_valid(handle) && checked.types.display_name(handle) == expected) {
                return true;
            }
        }
        for (const auto& entry : side_tables.sparse_expr_types) {
            if (sema::is_valid(entry.second) && checked.types.display_name(entry.second) == expected) {
                return true;
            }
        }
        return false;
    };

    EXPECT_TRUE(side_table_has_type_display("i32"));
    EXPECT_TRUE(side_table_has_type_display("*const i32"));
    EXPECT_TRUE(side_table_has_expr_display("usize"));
    EXPECT_TRUE(side_table_has_expr_display("*const i32"));

    base::usize layout_type_operand_count = 0;
    base::usize pointer_type_operand_count = 0;
    for (base::usize index = 0; index < analyzer.ctx_.module.exprs.size(); ++index) {
        const syntax::ExprKind kind = analyzer.ctx_.module.exprs.kind(index);
        if (kind != syntax::ExprKind::size_of && kind != syntax::ExprKind::align_of
            && kind != syntax::ExprKind::paddr && kind != syntax::ExprKind::pcast
            && kind != syntax::ExprKind::bcast) {
            continue;
        }
        const syntax::CastExprPayload* const payload = analyzer.ctx_.module.exprs.cast_payload(index);
        ASSERT_NE(payload, nullptr);
        ASSERT_TRUE(syntax::is_valid(payload->type));
        const base::usize local_type_index = side_tables.local_type_index(payload->type);
        ASSERT_NE(local_type_index, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
        ASSERT_LT(local_type_index, side_tables.syntax_type_handles.size());
        ASSERT_TRUE(sema::is_valid(side_tables.syntax_type_handles[local_type_index]));
        const std::string display = checked.types.display_name(side_tables.syntax_type_handles[local_type_index]);
        if (kind == syntax::ExprKind::size_of || kind == syntax::ExprKind::align_of) {
            ++layout_type_operand_count;
            EXPECT_EQ(display, "i32");
        } else {
            ++pointer_type_operand_count;
            EXPECT_EQ(display, "*const i32");
        }
    }
    EXPECT_EQ(layout_type_operand_count, 2U);
    EXPECT_EQ(pointer_type_operand_count, 3U);
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

TEST(CoreUnit, SemanticWhiteBoxGenericParamIdentitiesSkipOriginParamsForOrderedTypeAndConstKeys)
{
    constexpr std::string_view SEMA_TEST_MIXED_PARAM_MODULE = "mixed_param_identity";
    constexpr std::string_view SEMA_TEST_MIXED_PARAM_TEMPLATE = "View";

    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_MIXED_PARAM_MODULE})};
    const TypeId usize_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::usize));
    const IdentId origin_id = module.intern_identifier("data");
    const IdentId type_id = module.intern_identifier("T");
    const IdentId const_id = module.intern_identifier("N");
    const base::SourceRange origin_range{base::SourceId{1U}, 10U, 20U};
    const base::SourceRange type_range{base::SourceId{1U}, 30U, 40U};
    const base::SourceRange const_range{base::SourceId{1U}, 50U, 60U};

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::struct_decl;
    item.name = SEMA_TEST_MIXED_PARAM_TEMPLATE;
    item.name_id = module.intern_identifier(SEMA_TEST_MIXED_PARAM_TEMPLATE);
    item.generic_params = {
        syntax::GenericParamDecl{"data", origin_range, origin_id, syntax::GenericParamKind::origin},
        syntax::GenericParamDecl{"T", type_range, type_id, syntax::GenericParamKind::type},
        syntax::GenericParamDecl{"N", const_range, const_id, syntax::GenericParamKind::const_, usize_type},
    };
    const syntax::ItemId item_id = module.push_item(item);
    module.item_modules[item_id.value] = module_id(0);

    syntax::ItemNode no_origin_item = item;
    no_origin_item.generic_params = {
        syntax::GenericParamDecl{"T", type_range, type_id, syntax::GenericParamKind::type},
        syntax::GenericParamDecl{"N", const_range, const_id, syntax::GenericParamKind::const_, usize_type},
    };
    const syntax::ItemId no_origin_item_id = module.push_item(no_origin_item);
    module.item_modules[no_origin_item_id.value] = module_id(0);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    sema::SemanticAnalyzerCore::GenericTemplateInfo info =
        generic_template_info(analyzer, module_id(0), SEMA_TEST_MIXED_PARAM_TEMPLATE);
    info.item = item_id;
    info.params.clear();
    info.params.push_back(type_id);
    info.ordered_params.clear();
    info.ordered_params.push_back(type_id);
    info.ordered_params.push_back(const_id);
    info.ordered_param_kinds.clear();
    info.ordered_param_kinds.push_back(syntax::GenericParamKind::type);
    info.ordered_param_kinds.push_back(syntax::GenericParamKind::const_);

    sema::GenericParamIdentity no_origin_type_identity = sema::INVALID_GENERIC_PARAM_IDENTITY;
    sema::GenericParamIdentity no_origin_const_identity = sema::INVALID_GENERIC_PARAM_IDENTITY;
    {
        sema::SemanticAnalyzerCore::GenericTemplateInfo no_origin = info;
        no_origin.item = no_origin_item_id;
        no_origin.ordered_param_identities.clear();
        analyzer.populate_generic_param_identities(no_origin);
        no_origin_type_identity = no_origin.param_identities.front();
        no_origin_const_identity = no_origin.ordered_param_identities.back();
    }

    analyzer.populate_generic_param_identities(info);
    ASSERT_EQ(info.param_identities.size(), 1U);
    ASSERT_EQ(info.ordered_param_identities.size(), 2U);
    EXPECT_EQ(info.param_identities.front(), info.ordered_param_identities.front());
    EXPECT_EQ(info.param_identities.front(), no_origin_type_identity);
    EXPECT_EQ(info.ordered_param_identities.back(), no_origin_const_identity);

    analyzer.index_generic_param_query_keys(info, query::DefNamespace::type);
    const query::DefKey owner_key = analyzer.generic_template_query_key(info, query::DefNamespace::type);
    const TypeHandle type_param =
        analyzer.state_.checked.types.generic_param(info.param_identities.front(), SEMA_TEST_GENERIC_PARAM_NAME);
    const std::optional<query::GenericParamKey> type_key =
        analyzer.canonical_generic_param_query_key(info, owner_key, analyzer.state_.checked.types.get(type_param));
    ASSERT_TRUE(type_key.has_value());
    EXPECT_EQ(*type_key, query::generic_param_key(owner_key, 0, query::GenericParamKind::type));

    const auto const_key = analyzer.state_.generics.param_query_keys.find(info.ordered_param_identities.back());
    ASSERT_NE(const_key, analyzer.state_.generics.param_query_keys.end());
    EXPECT_EQ(const_key->second, query::generic_param_key(owner_key, 1, query::GenericParamKind::const_));
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
    ASSERT_TRUE(checked_result) << diagnostic_messages(diagnostics) << checked_result.error().message;
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
    EXPECT_EQ(checked.types.display_name(generic_box->type), "generic_type_display.Box<i32>");

    ASSERT_EQ(checked.generic_enum_instances.size(), 1U);
    const sema::GenericEnumInstanceInfo& generic_maybe_instance = checked.generic_enum_instances.front();
    EXPECT_TRUE(query::is_valid(generic_maybe_instance.generic_instance_key));
    ASSERT_EQ(generic_maybe_instance.generic_instance_key.type_args.size(), 1U);
    EXPECT_EQ(generic_maybe_instance.generic_instance_key.type_args.front(),
        query::canonical_builtin(query::BuiltinTypeKey::i32));
    EXPECT_EQ(checked.types.display_name(generic_maybe_instance.type), "generic_type_display.Maybe<i32>");

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
    EXPECT_EQ(checked.types.display_name(generic_some->type), "generic_type_display.Maybe<i32>");
    EXPECT_EQ(generic_some->name.view().find("[i32]"), std::string::npos);

    ASSERT_EQ(checked.generic_type_alias_instances.size(), 1U);
    const sema::GenericTypeAliasInstanceInfo& alias_instance = checked.generic_type_alias_instances.front();
    EXPECT_TRUE(query::is_valid(alias_instance.generic_instance_key));
    ASSERT_EQ(alias_instance.generic_instance_key.type_args.size(), 1U);
    EXPECT_EQ(
        alias_instance.generic_instance_key.type_args.front(), query::canonical_builtin(query::BuiltinTypeKey::i32));
    EXPECT_EQ(checked.types.display_name(alias_instance.resolved_type), "generic_type_display.Box<i32>");

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("struct priv Box<i32>"), std::string::npos);
    EXPECT_NE(checked_dump.find("case Maybe<i32>_some"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxConstGenericStructInstancesCarryConstArgs)
{
    syntax::AstModule module;
    module.modules = {module_info({"const_generic_struct"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    const TypeId usize_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::usize));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));

    syntax::ItemNode view_item;
    view_item.kind = syntax::ItemKind::struct_decl;
    view_item.name = "ArrayView";
    view_item.name_id = module.intern_identifier("ArrayView");
    view_item.generic_params = {
        syntax::GenericParamDecl{"T", {}, module.intern_identifier("T"), syntax::GenericParamKind::type},
        syntax::GenericParamDecl{
            "N", {}, module.intern_identifier("N"), syntax::GenericParamKind::const_, usize_type},
    };
    view_item.fields = {syntax::FieldDecl{"value", generic_type, {}}};
    const syntax::ItemId view_item_id = module.push_item(view_item);
    module.item_modules[view_item_id.value] = module_id(0);

    const ExprId four_expr = module.push_literal_expr(syntax::ExprKind::integer_literal, {}, "4");
    syntax::TypeNode view_i32_4 = named_node("ArrayView");
    view_i32_4.type_args = {i32_type};
    view_i32_4.generic_args = {
        syntax::GenericArgDecl{syntax::GenericArgKind::type, i32_type, syntax::INVALID_EXPR_ID, {}},
        syntax::GenericArgDecl{syntax::GenericArgKind::const_expr, syntax::INVALID_TYPE_ID, four_expr, {}},
    };
    const TypeId view_i32_4_type = module.push_type(view_i32_4);

    const ExprId zero = push_integer(module);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode use_item;
    use_item.kind = syntax::ItemKind::fn_decl;
    use_item.name = "use";
    use_item.params = {syntax::ParamDecl{"view", view_i32_4_type, {}}};
    use_item.return_type = i32_type;
    use_item.body = body;
    const syntax::ItemId use_item_id = module.push_item(use_item);
    module.item_modules[use_item_id.value] = module_id(0);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    const sema::StructInfo* generic_view = nullptr;
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        if (info.name == "ArrayView" && query::is_valid(info.generic_instance_key)) {
            generic_view = &info;
            break;
        }
    }
    ASSERT_NE(generic_view, nullptr);
    ASSERT_EQ(generic_view->generic_instance_key.type_args.size(), 1U);
    EXPECT_EQ(
        generic_view->generic_instance_key.type_args.front(), query::canonical_builtin(query::BuiltinTypeKey::i32));
    ASSERT_EQ(generic_view->generic_instance_key.const_args.size(), 1U);
    EXPECT_NE(query::debug_string(generic_view->generic_instance_key.const_args.front()),
        query::debug_string(query::StableFingerprint128{}));
    EXPECT_EQ(checked.types.display_name(generic_view->type), "const_generic_struct.ArrayView<i32>");
}

TEST(CoreUnit, SemanticWhiteBoxConstGenericFunctionBodyUsesConstParamArrayLength)
{
    syntax::AstModule module;
    module.modules = {module_info({"const_generic_function"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    const TypeId usize_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::usize));
    const IdentId t_id = module.intern_identifier("T");
    const IdentId n_id = module.intern_identifier("N");

    const ExprId n_length_expr = module.push_name_expr({}, "N");
    syntax::TypeNode array_type;
    array_type.kind = syntax::TypeKind::array;
    array_type.array_element = generic_type;
    array_type.array_length = syntax::ArrayLengthDecl{syntax::ArrayLengthKind::const_expr, 0, n_length_expr, {}};
    const TypeId array_generic_type = module.push_type(array_type);

    const ExprId return_n_expr = module.push_name_expr({}, "N");
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = return_n_expr;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode len_item;
    len_item.kind = syntax::ItemKind::fn_decl;
    len_item.name = "len";
    len_item.name_id = module.intern_identifier("len");
    len_item.generic_params = {
        syntax::GenericParamDecl{"T", {}, t_id, syntax::GenericParamKind::type},
        syntax::GenericParamDecl{"N", {}, n_id, syntax::GenericParamKind::const_, usize_type},
    };
    len_item.params = {syntax::ParamDecl{"value", array_generic_type, {}}};
    len_item.return_type = usize_type;
    len_item.body = body;
    const syntax::ItemId len_item_id = module.push_item(len_item);
    module.item_modules[len_item_id.value] = module_id(0);

    {
        base::DiagnosticSink type_diagnostics;
        sema::SemanticAnalyzerCore type_analyzer(module, type_diagnostics);
        prepare_expr_storage(type_analyzer, module);
        type_analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
        type_analyzer.state_.flow.current_module = module_id(0);

        sema::SemanticAnalyzerCore::GenericContext generic_context = type_analyzer.make_generic_context();
        const sema::GenericParamIdentity t_identity = sema::generic_param_identity_from_text("const_generic_function.T");
        const sema::GenericParamIdentity n_identity = sema::generic_param_identity_from_text("const_generic_function.N");
        generic_context.params.emplace(
            t_id, type_analyzer.state_.checked.types.generic_param(t_identity, "T"));
        generic_context.param_identities.emplace(t_id, t_identity);
        generic_context.const_params.emplace(n_id, type_analyzer.state_.checked.types.builtin(sema::BuiltinType::usize));
        generic_context.const_param_identities.emplace(n_id, n_identity);
        query::StableHashBuilder n_value_builder;
        n_value_builder.mix_string("const_generic_function.N");
        generic_context.const_arg_values.emplace(n_id, n_value_builder.finish());
        type_analyzer.state_.flow.current_generic_context = &generic_context;

        const TypeHandle resolved_array = type_analyzer.resolve_type(array_generic_type);
        ASSERT_TRUE(sema::is_valid(resolved_array)) << diagnostic_messages(type_diagnostics);
        const sema::TypeInfo& array_info = type_analyzer.state_.checked.types.get(resolved_array);
        EXPECT_EQ(array_info.kind, sema::TypeKind::array);
        EXPECT_EQ(array_info.array_length.kind, sema::ArrayLengthKind::const_param);
        EXPECT_EQ(array_info.array_length.const_param_name, "N");
        EXPECT_TRUE(type_analyzer.state_.checked.types.same(
            array_info.array_length.const_param_type,
            type_analyzer.state_.checked.types.builtin(sema::BuiltinType::usize)));
        EXPECT_EQ(type_analyzer.state_.checked.types.get(array_info.array_element).kind, sema::TypeKind::generic_param);
        EXPECT_NE(query::debug_string(array_info.array_length.fingerprint),
            query::debug_string(query::StableFingerprint128{}));
        type_analyzer.state_.flow.current_generic_context = nullptr;
    }

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << diagnostic_messages(diagnostics) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    EXPECT_TRUE(checked.functions.empty());
    static_cast<void>(return_n_expr);
}

TEST(CoreUnit, SemanticWhiteBoxConstGenericRejectsUnsupportedSurfaces)
{
    syntax::AstModule module;
    module.modules = {module_info({"const_generic_reject"})};

    const TypeId str_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::str));
    syntax::ItemNode bad_const_type_item;
    bad_const_type_item.kind = syntax::ItemKind::struct_decl;
    bad_const_type_item.name = "Bad";
    bad_const_type_item.name_id = module.intern_identifier("Bad");
    bad_const_type_item.generic_params = {
        syntax::GenericParamDecl{
            "N", {}, module.intern_identifier("N"), syntax::GenericParamKind::const_, str_type},
    };
    const syntax::ItemId bad_const_type_item_id = module.push_item(bad_const_type_item);
    module.item_modules[bad_const_type_item_id.value] = module_id(0);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    const auto found = std::ranges::find_if(diagnostics.diagnostics(), [](const base::Diagnostic& diagnostic) {
        return diagnostic.message.find("must use an integer, bool, or char type") != std::string::npos;
    });
    EXPECT_NE(found, diagnostics.diagnostics().end());
}

TEST(CoreUnit, SemanticWhiteBoxConstGenericRejectsForwardedParamTypeMismatch)
{
    const std::string output = analyze_const_generic_source_failure(
        "module const_generic_forward_mismatch;\n"
        "struct Target<T, const N: usize> {\n"
        "    value: T;\n"
        "}\n"
        "struct Forward<T, const B: bool> {\n"
        "    target: Target<T, B>;\n"
        "}\n"
        "fn use(value: Forward<i32, true>) -> i32 {\n"
        "    return 0;\n"
        "}\n");

    EXPECT_NE(output.find(sema::SEMA_CONST_GENERIC_ARGUMENT_TYPE_MISMATCH), std::string::npos) << output;
    EXPECT_NE(output.find("expected type: usize"), std::string::npos) << output;
    EXPECT_NE(output.find("actual type: bool"), std::string::npos) << output;
}

TEST(CoreUnit, SemanticWhiteBoxConstGenericRejectsConstArgForTypeParamWithNamedDiagnostic)
{
    const std::string output = analyze_const_generic_source_failure(
        "module const_generic_type_param_const_arg;\n"
        "struct ArrayBox<T, const N: usize> {\n"
        "    value: T;\n"
        "}\n"
        "fn use() -> i32 {\n"
        "    let value: ArrayBox<1, 4> = ArrayBox<i32, 4> { value: 1 };\n"
        "    return 0;\n"
        "}\n");

    EXPECT_NE(output.find("generic parameter `T` expects a type argument"), std::string::npos) << output;
    EXPECT_EQ(output.find(sema::SEMA_CONST_GENERIC_ARGUMENT_UNSUPPORTED), std::string::npos) << output;
    EXPECT_EQ(output.find(SEMA_CONST_GENERIC_TEST_INTERNAL_CONTRACT_PREFIX), std::string::npos) << output;
}

TEST(CoreUnit, SemanticWhiteBoxConstGenericRejectsTypeArgForConstParamWithNamedDiagnostic)
{
    const std::string output = analyze_const_generic_source_failure(
        "module const_generic_const_param_type_arg;\n"
        "struct ArrayBox<T, const N: usize> {\n"
        "    value: T;\n"
        "}\n"
        "fn use() -> i32 {\n"
        "    let value: ArrayBox<i32, bool> = ArrayBox<i32, 4> { value: 1 };\n"
        "    return 0;\n"
        "}\n");

    EXPECT_NE(output.find("generic parameter `N` expects a const argument"), std::string::npos) << output;
    EXPECT_EQ(output.find(sema::SEMA_CONST_GENERIC_ARGUMENT_UNSUPPORTED), std::string::npos) << output;
    EXPECT_EQ(output.find(SEMA_CONST_GENERIC_TEST_INTERNAL_CONTRACT_PREFIX), std::string::npos) << output;
}

TEST(CoreUnit, SemanticWhiteBoxConstGenericRejectsFunctionTypeArgForConstParamWithNamedDiagnostic)
{
    const std::string output = analyze_const_generic_source_failure(
        "module const_generic_function_const_param_type_arg;\n"
        "fn id<T, const N: usize>(value: T) -> T {\n"
        "    return value;\n"
        "}\n"
        "fn use() -> i32 {\n"
        "    return id<i32, bool>(1);\n"
        "}\n");

    EXPECT_NE(output.find("generic parameter `N` expects a const argument"), std::string::npos) << output;
    EXPECT_EQ(output.find(sema::SEMA_CONST_GENERIC_ARGUMENT_UNSUPPORTED), std::string::npos) << output;
    EXPECT_EQ(output.find(SEMA_CONST_GENERIC_TEST_INTERNAL_CONTRACT_PREFIX), std::string::npos) << output;
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
} // namespace aurex::test
