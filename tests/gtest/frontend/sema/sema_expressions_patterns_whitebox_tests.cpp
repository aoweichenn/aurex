#include <gtest/frontend/sema/sema_whitebox_test_support.hpp>

namespace aurex::test {

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
} // namespace aurex::test
