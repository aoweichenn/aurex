#include <gtest/frontend/sema/sema_whitebox_test_support.hpp>

namespace aurex::test {

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
    EXPECT_EQ(
        sema::receiver_access_kind_name(static_cast<sema::ReceiverAccessKind>(SEMA_TEST_INVALID_RESOURCE_KIND_VALUE)),
        "<invalid>");
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
    const std::span<const ExprId> no_ordered_args;
    FunctionSignature invalid_signature = analyzer.state_.checked.make_function_signature();
    analyzer.record_function_call_binding(
        syntax::INVALID_EXPR_ID, syntax::INVALID_EXPR_ID, invalid_signature, 0, no_ordered_args,
        body_loan_test_range(0));
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

    analyzer.record_function_call_binding(
        call_expr, syntax::INVALID_EXPR_ID, signature, 1, no_ordered_args, body_loan_test_range(1));
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
    EXPECT_EQ(std::count(visible_names.begin(), visible_names.end(), SEMA_TEST_SYMBOL_OUTER_NAME),
        static_cast<std::ptrdiff_t>(1));
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
