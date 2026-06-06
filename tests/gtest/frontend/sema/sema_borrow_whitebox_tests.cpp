#include <gtest/frontend/sema/sema_whitebox_test_support.hpp>

namespace aurex::test {
namespace {

[[nodiscard]] bool body_flow_has_path(
    const sema::BodyFlowGraph& graph, const base::u32 from, const base::u32 to)
{
    if (from >= graph.points.size() || to >= graph.points.size()) {
        return false;
    }
    if (from == to) {
        return true;
    }

    std::vector<bool> visited(graph.points.size(), false);
    std::vector<base::u32> pending;
    pending.push_back(from);
    visited[from] = true;
    while (!pending.empty()) {
        const base::u32 point = pending.back();
        pending.pop_back();
        for (const sema::BodyFlowEdge& edge : graph.edges) {
            if (edge.from != point || edge.to >= graph.points.size() || visited[edge.to]) {
                continue;
            }
            if (edge.to == to) {
                return true;
            }
            visited[edge.to] = true;
            pending.push_back(edge.to);
        }
    }
    return false;
}

[[nodiscard]] std::optional<base::u32> body_flow_point_for_kind(
    const sema::BodyFlowGraph& graph, const sema::BodyFlowPointKind kind)
{
    for (base::usize index = 0; index < graph.points.size(); ++index) {
        if (graph.points[index].kind == kind) {
            return base::checked_u32(index, "test body flow point");
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<base::u32> body_flow_point_for_stmt(
    const sema::BodyFlowGraph& graph, const sema::BodyFlowPointKind kind, const syntax::StmtId stmt)
{
    for (base::usize index = 0; index < graph.points.size(); ++index) {
        const sema::BodyFlowPoint& point = graph.points[index];
        if (point.kind == kind && point.stmt.value == stmt.value) {
            return base::checked_u32(index, "test body flow point");
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<base::u32> body_flow_cleanup_point_for(
    const sema::BodyFlowGraph& graph, const IdentId name_id)
{
    for (const sema::BodyFlowAction& action : graph.actions) {
        if (action.kind != sema::BodyFlowActionKind::cleanup_storage || action.place >= graph.places.size()) {
            continue;
        }
        const sema::BodyFlowPlace& place = graph.places[action.place];
        if (place.root_kind == sema::BodyFlowPlaceRootKind::local && place.root_name_id == name_id) {
            return action.point;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<base::u32> body_flow_action_point_for_expr(
    const sema::BodyFlowGraph& graph, const sema::BodyFlowActionKind kind, const ExprId expr)
{
    for (const sema::BodyFlowAction& action : graph.actions) {
        if (action.kind == kind && action.expr.value == expr.value) {
            return action.point;
        }
    }
    return std::nullopt;
}

} // namespace

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

TEST(CoreUnit, SemanticWhiteBoxBodyFlowRunsBlockCleanupInReverseRegistrationOrder)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const ExprId first_init = push_integer(module);
    const syntax::StmtId first_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "first", syntax::INVALID_TYPE_ID, first_init);
    const ExprId sink = push_name(module, SEMA_TEST_BODY_FLOW_SINK_NAME);
    const ExprId source = push_name(module, SEMA_TEST_BODY_FLOW_SOURCE_NAME);
    const ExprId deferred_call = push_call(module, sink, {source});
    syntax::StmtNode defer_stmt;
    defer_stmt.kind = syntax::StmtKind::defer;
    defer_stmt.init = deferred_call;
    const syntax::StmtId defer_stmt_id = module.push_stmt(defer_stmt);
    const ExprId second_init = push_integer(module);
    const syntax::StmtId second_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "second", syntax::INVALID_TYPE_ID, second_init);
    const syntax::StmtId body = push_block(module, {first_stmt, defer_stmt_id, second_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "flow_cleanup_order";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_cleanup_order");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    const std::string dump = sema::dump_body_flow_graph(graph);
    const auto action_point_for = [&graph](const sema::BodyFlowActionKind kind,
                                      const ExprId expr) -> std::optional<base::u32> {
        for (const sema::BodyFlowAction& action : graph.actions) {
            if (action.kind == kind && action.expr.value == expr.value) {
                return action.point;
            }
        }
        return std::nullopt;
    };

    const std::optional<base::u32> first_cleanup =
        body_flow_cleanup_point_for(graph, module.intern_identifier("first"));
    const std::optional<base::u32> second_cleanup =
        body_flow_cleanup_point_for(graph, module.intern_identifier("second"));
    const std::optional<base::u32> defer_cleanup =
        action_point_for(sema::BodyFlowActionKind::cleanup_scope, deferred_call);
    const std::optional<base::u32> deferred_call_point =
        action_point_for(sema::BodyFlowActionKind::call, deferred_call);
    ASSERT_TRUE(first_cleanup.has_value()) << dump;
    ASSERT_TRUE(second_cleanup.has_value()) << dump;
    ASSERT_TRUE(defer_cleanup.has_value()) << dump;
    ASSERT_TRUE(deferred_call_point.has_value()) << dump;

    EXPECT_TRUE(body_flow_has_path(graph, *second_cleanup, *defer_cleanup)) << dump;
    EXPECT_TRUE(body_flow_has_path(graph, *defer_cleanup, *deferred_call_point)) << dump;
    EXPECT_TRUE(body_flow_has_path(graph, *deferred_call_point, *first_cleanup)) << dump;

    const auto defer_exit = std::ranges::find_if(graph.points, [defer_stmt_id](const sema::BodyFlowPoint& point) {
        return point.kind == sema::BodyFlowPointKind::statement_exit && point.stmt.value == defer_stmt_id.value;
    });
    ASSERT_NE(defer_exit, graph.points.end()) << dump;
    const base::u32 defer_exit_point = base::checked_u32(
        static_cast<base::usize>(std::distance(graph.points.begin(), defer_exit)), "test body flow point");
    EXPECT_FALSE(std::ranges::any_of(graph.actions, [defer_exit_point](const sema::BodyFlowAction& action) {
        return action.point == defer_exit_point
            && (action.kind == sema::BodyFlowActionKind::call
                || action.kind == sema::BodyFlowActionKind::read
                || action.kind == sema::BodyFlowActionKind::move_candidate);
    })) << dump;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowReturnCleanupDoesNotFallThroughFullyReturningIf)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const syntax::StmtId tracked_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "tracked", syntax::INVALID_TYPE_ID, push_integer(module));
    const syntax::StmtId then_return = push_return_stmt(module, push_name(module, "then_value"));
    const syntax::StmtId else_return = push_return_stmt(module, push_name(module, "else_value"));

    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = push_bool(module, "true");
    if_stmt.then_block = push_block(module, {then_return});
    if_stmt.else_block = push_block(module, {else_return});
    const syntax::StmtId if_stmt_id = module.push_stmt(if_stmt);

    syntax::StmtNode after_stmt;
    after_stmt.kind = syntax::StmtKind::expr;
    after_stmt.init = push_name(module, "after_return");
    const syntax::StmtId after_stmt_id = module.push_stmt(after_stmt);
    const syntax::StmtId body = push_block(module, {tracked_stmt, if_stmt_id, after_stmt_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "flow_full_return_if_cleanup";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_full_return_if_cleanup");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    const std::string dump = sema::dump_body_flow_graph(graph);
    const std::optional<base::u32> entry = body_flow_point_for_kind(graph, sema::BodyFlowPointKind::entry);
    const std::optional<base::u32> after_entry =
        body_flow_point_for_stmt(graph, sema::BodyFlowPointKind::statement_entry, after_stmt_id);
    const std::optional<base::u32> tracked_cleanup =
        body_flow_cleanup_point_for(graph, module.intern_identifier("tracked"));
    ASSERT_TRUE(entry.has_value()) << dump;
    ASSERT_TRUE(after_entry.has_value()) << dump;
    ASSERT_TRUE(tracked_cleanup.has_value()) << dump;

    EXPECT_FALSE(body_flow_has_path(graph, *entry, *after_entry)) << dump;
    for (const sema::BodyFlowAction& action : graph.actions) {
        if (action.kind == sema::BodyFlowActionKind::return_) {
            EXPECT_TRUE(body_flow_has_path(graph, action.point, *tracked_cleanup)) << dump;
            EXPECT_FALSE(body_flow_has_path(graph, action.point, *after_entry)) << dump;
        }
    }
    EXPECT_EQ(std::ranges::count_if(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::return_;
    }), 2);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowReturnCleanupFollowsFullyReturningElseIf)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const syntax::StmtId tracked_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "tracked", syntax::INVALID_TYPE_ID, push_integer(module));

    syntax::StmtNode else_if_stmt;
    else_if_stmt.kind = syntax::StmtKind::if_;
    else_if_stmt.condition = push_bool(module, "false");
    else_if_stmt.then_block = push_block(module, {push_return_stmt(module, push_name(module, "else_if_then"))});
    else_if_stmt.else_block = push_block(module, {push_return_stmt(module, push_name(module, "else_if_else"))});
    const syntax::StmtId else_if_stmt_id = module.push_stmt(else_if_stmt);

    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = push_bool(module, "true");
    if_stmt.then_block = push_block(module, {push_return_stmt(module, push_name(module, "then_value"))});
    if_stmt.else_if = else_if_stmt_id;
    const syntax::StmtId if_stmt_id = module.push_stmt(if_stmt);

    syntax::StmtNode after_stmt;
    after_stmt.kind = syntax::StmtKind::expr;
    after_stmt.init = push_name(module, "after_else_if_return");
    const syntax::StmtId after_stmt_id = module.push_stmt(after_stmt);
    const syntax::StmtId body = push_block(module, {tracked_stmt, if_stmt_id, after_stmt_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "flow_else_if_return_cleanup";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_else_if_return_cleanup");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    const std::string dump = sema::dump_body_flow_graph(graph);
    const std::optional<base::u32> entry = body_flow_point_for_kind(graph, sema::BodyFlowPointKind::entry);
    const std::optional<base::u32> after_entry =
        body_flow_point_for_stmt(graph, sema::BodyFlowPointKind::statement_entry, after_stmt_id);
    const std::optional<base::u32> tracked_cleanup =
        body_flow_cleanup_point_for(graph, module.intern_identifier("tracked"));
    ASSERT_TRUE(entry.has_value()) << dump;
    ASSERT_TRUE(after_entry.has_value()) << dump;
    ASSERT_TRUE(tracked_cleanup.has_value()) << dump;

    EXPECT_FALSE(body_flow_has_path(graph, *entry, *after_entry)) << dump;
    for (const sema::BodyFlowAction& action : graph.actions) {
        if (action.kind == sema::BodyFlowActionKind::return_) {
            EXPECT_TRUE(body_flow_has_path(graph, action.point, *tracked_cleanup)) << dump;
            EXPECT_FALSE(body_flow_has_path(graph, action.point, *after_entry)) << dump;
        }
    }
    EXPECT_EQ(std::ranges::count_if(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::return_;
    }), 3);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowReturnCleanupScansForRangeBody)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const syntax::StmtId tracked_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "tracked", syntax::INVALID_TYPE_ID, push_integer(module));

    syntax::StmtNode for_range_stmt;
    for_range_stmt.kind = syntax::StmtKind::for_range;
    for_range_stmt.name = "item";
    for_range_stmt.name_id = module.intern_identifier("item");
    for_range_stmt.range_start = push_integer_text(module, "0");
    for_range_stmt.range_end = push_integer_text(module, "3");
    for_range_stmt.body = push_block(module, {push_return_stmt(module, push_name(module, "loop_value"))});
    const syntax::StmtId for_range_stmt_id = module.push_stmt(for_range_stmt);

    syntax::StmtNode after_stmt;
    after_stmt.kind = syntax::StmtKind::expr;
    after_stmt.init = push_name(module, "after_for_range");
    const syntax::StmtId after_stmt_id = module.push_stmt(after_stmt);
    const syntax::StmtId body = push_block(module, {tracked_stmt, for_range_stmt_id, after_stmt_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "flow_for_range_return_cleanup";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_for_range_return_cleanup");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    const std::string dump = sema::dump_body_flow_graph(graph);
    const std::optional<base::u32> entry = body_flow_point_for_kind(graph, sema::BodyFlowPointKind::entry);
    const std::optional<base::u32> after_entry =
        body_flow_point_for_stmt(graph, sema::BodyFlowPointKind::statement_entry, after_stmt_id);
    const IdentId tracked_name_id = module.intern_identifier("tracked");
    ASSERT_TRUE(entry.has_value()) << dump;
    ASSERT_TRUE(after_entry.has_value()) << dump;
    ASSERT_TRUE(body_flow_cleanup_point_for(graph, tracked_name_id).has_value()) << dump;

    EXPECT_TRUE(body_flow_has_path(graph, *entry, *after_entry)) << dump;
    const auto return_action = std::ranges::find_if(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::return_;
    });
    ASSERT_NE(return_action, graph.actions.end()) << dump;
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph, tracked_name_id, return_action](const sema::BodyFlowAction& action) {
        if (action.kind != sema::BodyFlowActionKind::cleanup_storage || action.place >= graph.places.size()) {
            return false;
        }
        const sema::BodyFlowPlace& place = graph.places[action.place];
        return place.root_kind == sema::BodyFlowPlaceRootKind::local && place.root_name_id == tracked_name_id
            && body_flow_has_path(graph, return_action->point, action.point);
    })) << dump;
    EXPECT_FALSE(body_flow_has_path(graph, return_action->point, *after_entry)) << dump;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowReturnCleanupUsesRegisteredPrefixOnly)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const syntax::StmtId before_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "before", syntax::INVALID_TYPE_ID, push_integer(module));

    const ExprId sink = push_name(module, SEMA_TEST_BODY_FLOW_SINK_NAME);
    const ExprId before_defer_call = push_call(module, sink, {push_name(module, "before_source")});
    syntax::StmtNode before_defer_stmt;
    before_defer_stmt.kind = syntax::StmtKind::defer;
    before_defer_stmt.init = before_defer_call;
    const syntax::StmtId before_defer_stmt_id = module.push_stmt(before_defer_stmt);

    const syntax::StmtId return_stmt = push_return_stmt(module, push_name(module, "result"));
    const syntax::StmtId future_stmt =
        push_local_stmt(module, syntax::StmtKind::var, "future", syntax::INVALID_TYPE_ID, push_integer(module));

    const ExprId future_defer_call = push_call(module, sink, {push_name(module, "future_source")});
    syntax::StmtNode future_defer_stmt;
    future_defer_stmt.kind = syntax::StmtKind::defer;
    future_defer_stmt.init = future_defer_call;
    const syntax::StmtId future_defer_stmt_id = module.push_stmt(future_defer_stmt);
    const syntax::StmtId body =
        push_block(module, {before_stmt, before_defer_stmt_id, return_stmt, future_stmt, future_defer_stmt_id});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "flow_return_registered_prefix_cleanup";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_return_registered_prefix_cleanup");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    const std::string dump = sema::dump_body_flow_graph(graph);
    const auto return_action = std::ranges::find_if(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::return_;
    });
    ASSERT_NE(return_action, graph.actions.end()) << dump;

    const std::optional<base::u32> before_cleanup =
        body_flow_cleanup_point_for(graph, module.intern_identifier("before"));
    const std::optional<base::u32> future_cleanup =
        body_flow_cleanup_point_for(graph, module.intern_identifier("future"));
    const std::optional<base::u32> before_defer_cleanup =
        body_flow_action_point_for_expr(graph, sema::BodyFlowActionKind::cleanup_scope, before_defer_call);
    const std::optional<base::u32> before_defer_call_point =
        body_flow_action_point_for_expr(graph, sema::BodyFlowActionKind::call, before_defer_call);
    const std::optional<base::u32> future_defer_cleanup =
        body_flow_action_point_for_expr(graph, sema::BodyFlowActionKind::cleanup_scope, future_defer_call);
    const std::optional<base::u32> future_defer_call_point =
        body_flow_action_point_for_expr(graph, sema::BodyFlowActionKind::call, future_defer_call);
    ASSERT_TRUE(before_cleanup.has_value()) << dump;
    ASSERT_TRUE(before_defer_cleanup.has_value()) << dump;
    ASSERT_TRUE(before_defer_call_point.has_value()) << dump;

    EXPECT_TRUE(body_flow_has_path(graph, return_action->point, *before_defer_cleanup)) << dump;
    EXPECT_TRUE(body_flow_has_path(graph, *before_defer_cleanup, *before_defer_call_point)) << dump;
    EXPECT_TRUE(body_flow_has_path(graph, *before_defer_call_point, *before_cleanup)) << dump;
    EXPECT_FALSE(future_cleanup.has_value()) << dump;
    EXPECT_FALSE(future_defer_cleanup.has_value()) << dump;
    EXPECT_FALSE(future_defer_call_point.has_value()) << dump;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowCollectsTupleElementProjection)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const IdentId source_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_SOURCE_NAME);
    const ExprId source = push_name(module, SEMA_TEST_BODY_FLOW_SOURCE_NAME);
    const ExprId source_first = push_field(module, source, "0");
    const ExprId borrow = push_unary(module, syntax::UnaryOp::address_of, source_first);
    const syntax::StmtId borrow_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, syntax::INVALID_TYPE_ID, borrow);
    const syntax::StmtId body = push_block(module, {borrow_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "flow_tuple_projection";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = analyzer.state_.checked.types.builtin(BuiltinType::bool_);
    const TypeHandle tuple_type = analyzer.state_.checked.types.tuple({i32, bool_type});
    static_cast<void>(analyzer.record_expr_type(source, tuple_type));
    static_cast<void>(analyzer.record_expr_type(source_first, i32));
    static_cast<void>(analyzer.record_expr_type(borrow, analyzer.state_.checked.types.reference(PointerMutability::const_, i32)));

    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_tuple_projection");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    const auto found = analyzer.state_.checked.body_flow_graphs.find(key);
    ASSERT_NE(found, analyzer.state_.checked.body_flow_graphs.end());
    const sema::BodyFlowGraph& graph = found->second;
    const auto borrow_action = std::ranges::find_if(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared;
    });
    ASSERT_NE(borrow_action, graph.actions.end());
    ASSERT_NE(borrow_action->place, sema::SEMA_BODY_FLOW_INVALID_INDEX);
    ASSERT_LT(borrow_action->place, graph.places.size());
    const sema::BodyFlowPlace& place = graph.places[borrow_action->place];
    EXPECT_EQ(place.root_name_id, source_id);
    ASSERT_EQ(place.projections.size(), 1U);
    EXPECT_EQ(place.projections.front().kind, sema::BodyFlowPlaceProjectionKind::tuple_element);
    EXPECT_EQ(place.projections.front().element_index, 0U);

    const std::string dump = sema::dump_body_flow_graph(graph);
    EXPECT_NE(dump.find("tuple_element(-,0,"), std::string::npos) << dump;
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowKeepsNonNumericTupleFieldProjectionAsField)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const IdentId source_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_SOURCE_NAME);
    const IdentId field_id = module.intern_identifier(SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const ExprId source = push_name(module, SEMA_TEST_BODY_FLOW_SOURCE_NAME);
    const ExprId source_field = push_field(module, source, SEMA_TEST_BODY_FLOW_FIELD_NAME);
    const ExprId borrow = push_unary(module, syntax::UnaryOp::address_of, source_field);
    const syntax::StmtId borrow_stmt =
        push_local_stmt(module, syntax::StmtKind::let, SEMA_TEST_BODY_LOAN_REF_NAME, syntax::INVALID_TYPE_ID, borrow);
    const syntax::StmtId body = push_block(module, {borrow_stmt});

    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "flow_tuple_named_projection_fallback";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = analyzer.state_.checked.types.builtin(BuiltinType::bool_);
    const TypeHandle tuple_type = analyzer.state_.checked.types.tuple({i32, bool_type});
    const TypeHandle ref_i32 = analyzer.state_.checked.types.reference(PointerMutability::const_, i32);
    static_cast<void>(analyzer.record_expr_type(source, tuple_type));
    static_cast<void>(analyzer.record_expr_type(source_field, i32));
    static_cast<void>(analyzer.record_expr_type(borrow, ref_i32));

    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_tuple_named_projection_fallback");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    const auto borrow_action = std::ranges::find_if(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared;
    });
    ASSERT_NE(borrow_action, graph.actions.end());
    ASSERT_LT(borrow_action->place, graph.places.size());

    const sema::BodyFlowPlace& place = graph.places[borrow_action->place];
    EXPECT_EQ(place.root_kind, sema::BodyFlowPlaceRootKind::local);
    EXPECT_EQ(place.root_name_id, source_id);
    ASSERT_EQ(place.projections.size(), 1U);
    EXPECT_EQ(place.projections.front().kind, sema::BodyFlowPlaceProjectionKind::field);
    EXPECT_EQ(place.projections.front().field_name_id, field_id);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
}

TEST(CoreUnit, SemanticWhiteBoxBodyFlowCoversSyntheticMatchAndBorrowCarriers)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const ExprId match_value = push_integer(module);
    const ExprId empty_match = module.push_match_expr({}, match_value, std::vector<syntax::MatchArm>{});
    syntax::StmtNode match_stmt;
    match_stmt.kind = syntax::StmtKind::expr;
    match_stmt.init = empty_match;
    const syntax::StmtId match_stmt_id = module.push_stmt(match_stmt);

    const ExprId array_source = push_name(module, "array_source");
    const ExprId array_view = push_name(module, "array_view");
    const ExprId array_call = push_call(module, array_view, {array_source});
    const syntax::StmtId array_call_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "array_result", syntax::INVALID_TYPE_ID, array_call);

    const ExprId slice_source = push_name(module, "slice_source");
    const ExprId slice_view = module.push_slice_expr({},
        syntax::SliceExprPayload{slice_source, syntax::INVALID_EXPR_ID, syntax::INVALID_EXPR_ID});
    const syntax::StmtId slice_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "slice_result", syntax::INVALID_TYPE_ID, slice_view);

    const syntax::StmtId body = push_block(module, {match_stmt_id, array_call_stmt, slice_stmt});
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "flow_borrow_carrier_edges";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = types.reference(PointerMutability::const_, i32);
    const TypeHandle array_ref_i32 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, ref_i32);
    const TypeHandle mut_slice_i32 = types.slice(PointerMutability::mut, i32);
    analyzer.state_.checked.expr_types[match_value.value] = i32;
    analyzer.state_.checked.expr_types[empty_match.value] = i32;
    analyzer.state_.checked.expr_types[array_source.value] = ref_i32;
    analyzer.state_.checked.expr_types[array_call.value] = array_ref_i32;
    analyzer.state_.checked.expr_types[slice_source.value] = mut_slice_i32;
    analyzer.state_.checked.expr_types[slice_view.value] = mut_slice_i32;

    const IdentId array_view_id = module.intern_identifier("array_view");
    const sema::FunctionLookupKey callee_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        array_view_id,
    };
    sema::FunctionCallBinding binding = analyzer.state_.checked.make_function_call_binding();
    binding.call_expr = array_call;
    binding.callee_expr = array_view;
    binding.function_key = callee_key;
    binding.return_type = array_ref_i32;
    analyzer.state_.checked.append_function_call_binding(binding);

    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "flow_borrow_carrier_edges");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::branch;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared && action.place < graph.places.size()
            && graph.places[action.place].root_kind == sema::BodyFlowPlaceRootKind::unknown;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph](const sema::BodyFlowAction& action) {
        return action.kind == sema::BodyFlowActionKind::borrow_shared && action.place < graph.places.size()
            && graph.places[action.place].root_kind == sema::BodyFlowPlaceRootKind::local;
    }));
    EXPECT_TRUE(std::ranges::any_of(graph.actions, [&graph](const sema::BodyFlowAction& action) {
        if (action.kind != sema::BodyFlowActionKind::borrow_mutable || action.place >= graph.places.size()) {
            return false;
        }
        const sema::BodyFlowPlace& place = graph.places[action.place];
        return place.root_kind == sema::BodyFlowPlaceRootKind::local
            && !place.projections.empty()
            && place.projections.back().kind == sema::BodyFlowPlaceProjectionKind::dereference;
    }));
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
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

TEST(CoreUnit, SemanticWhiteBoxBodyFlowCoversInferredContractsAndSummaryUnknownEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({SEMA_TEST_BODY_FLOW_MODULE_NAME})};

    const ExprId inferred_source = push_name(module, "inferred_source");
    const ExprId inferred_callee = push_name(module, "inferred_contract_view");
    const ExprId inferred_call = push_call(module, inferred_callee, {inferred_source});
    const syntax::StmtId inferred_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "inferred_out", syntax::INVALID_TYPE_ID, inferred_call);

    const ExprId summary_source = push_name(module, "summary_source");
    const ExprId summary_callee = push_name(module, "summary_unknown_view");
    const ExprId summary_call = push_call(module, summary_callee, {summary_source});
    const syntax::StmtId summary_stmt =
        push_local_stmt(module, syntax::StmtKind::let, "summary_out", syntax::INVALID_TYPE_ID, summary_call);

    const syntax::StmtId body = push_block(module, {inferred_stmt, summary_stmt});
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "contract_summary_unknown_edges";
    function.body = body;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    const TypeHandle i32 = analyzer.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle ref_i32 = analyzer.state_.checked.types.reference(PointerMutability::const_, i32);

    const sema::FunctionLookupKey inferred_callee_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        module.intern_identifier("inferred_contract_view"),
    };
    sema::FunctionCallBinding inferred_binding = analyzer.state_.checked.make_function_call_binding();
    inferred_binding.call_expr = inferred_call;
    inferred_binding.callee_expr = inferred_callee;
    inferred_binding.function_key = inferred_callee_key;
    inferred_binding.return_type = ref_i32;
    analyzer.state_.checked.append_function_call_binding(inferred_binding);

    sema::FunctionBorrowContract inferred_contract;
    inferred_contract.function = inferred_callee_key;
    inferred_contract.source = sema::FunctionBorrowContractSource::inferred;
    inferred_contract.return_type = ref_i32;
    inferred_contract.return_type_can_contain_borrow = true;
    inferred_contract.return_selectors = {
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::parameter,
            .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
            .name_id = module.intern_identifier("inferred_source"),
            .range = body_loan_test_range(0),
        },
        sema::BorrowContractSelector{
            .kind = sema::BorrowContractSelectorKind::unknown,
            .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
            .range = body_loan_test_range(1),
        },
    };
    analyzer.state_.checked.borrow_contracts[inferred_callee_key] = inferred_contract;

    const sema::FunctionLookupKey summary_callee_key{
        SEMA_TEST_ROOT_MODULE_INDEX,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        module.intern_identifier("summary_unknown_view"),
    };
    sema::FunctionCallBinding summary_binding = analyzer.state_.checked.make_function_call_binding();
    summary_binding.call_expr = summary_call;
    summary_binding.callee_expr = summary_callee;
    summary_binding.function_key = summary_callee_key;
    summary_binding.return_type = ref_i32;
    analyzer.state_.checked.append_function_call_binding(summary_binding);

    sema::FunctionBorrowSummary summary;
    summary.function = summary_callee_key;
    summary.return_type = ref_i32;
    summary.return_type_can_contain_borrow = true;
    summary.has_unknown_return_origin = true;
    summary.return_origins = {
        sema::FunctionBorrowReturnOrigin{.origin_index = SEMA_TEST_BORROW_CONTRACT_INVALID_ORIGIN_INDEX},
    };
    analyzer.state_.checked.borrow_summaries[summary_callee_key] = summary;

    prepare_expr_storage(analyzer, module);
    analyzer.state_.checked.expr_types[inferred_source.value] = i32;
    analyzer.state_.checked.expr_types[inferred_call.value] = ref_i32;
    analyzer.state_.checked.expr_types[summary_source.value] = i32;
    analyzer.state_.checked.expr_types[summary_call.value] = ref_i32;

    const sema::FunctionLookupKey key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "contract_summary_unknown_edges");
    sema::SemanticAnalyzerCore::BodyFlowAnalyzer(analyzer).collect(function, key);

    ASSERT_TRUE(analyzer.state_.checked.body_flow_graphs.contains(key));
    const sema::BodyFlowGraph& graph = analyzer.state_.checked.body_flow_graphs.at(key);
    const base::usize unknown_borrow_count =
        static_cast<base::usize>(std::ranges::count_if(graph.actions, [&graph](const sema::BodyFlowAction& action) {
            return action.kind == sema::BodyFlowActionKind::borrow_shared && action.place < graph.places.size()
                && graph.places[action.place].root_kind == sema::BodyFlowPlaceRootKind::unknown;
        }));
    EXPECT_GE(unknown_borrow_count, 2U);
    EXPECT_TRUE(diagnostics.diagnostics().empty()) << diagnostic_messages(diagnostics);
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
    EXPECT_EQ(
        sema::body_flow_place_projection_kind_name(sema::BodyFlowPlaceProjectionKind::tuple_element), "tuple_element");
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
    const IdentId tuple_id = module.intern_identifier("tuple_value");

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
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = tuple_id,
        .root_expr = syntax::ExprId{5},
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::tuple_element,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = 0U,
        }},
        .range = body_loan_test_range(5),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = tuple_id,
        .root_expr = syntax::ExprId{5},
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::tuple_element,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = 1U,
        }},
        .range = body_loan_test_range(6),
    });
    graph.places.push_back(sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = value_id,
        .root_expr = syntax::ExprId{6},
        .projections = {sema::BodyFlowPlaceProjection{
            .kind = sema::BodyFlowPlaceProjectionKind::index,
            .expr = syntax::ExprId{7},
        }},
        .range = body_loan_test_range(7),
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
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 7,
        .range = body_loan_test_range(6),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_mutable,
        .point = 0,
        .place = 2,
        .range = body_loan_test_range(7),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::read,
        .point = 0,
        .place = 3,
        .range = body_loan_test_range(8),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 4,
        .range = body_loan_test_range(9),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 4,
        .range = body_loan_test_range(10),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::borrow_shared,
        .point = 0,
        .place = 5,
        .range = body_loan_test_range(11),
    });
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::write,
        .point = 0,
        .place = 6,
        .range = body_loan_test_range(12),
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
    EXPECT_TRUE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.kind == sema::BodyLoanConflictKind::write && conflict.place == 7;
    }));
    EXPECT_FALSE(std::ranges::any_of(result.conflicts, [](const sema::BodyLoanConflict& conflict) {
        return conflict.place == 6;
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
    EXPECT_EQ(summary.origins[summary.storage_escapes.front().origin_index].kind, sema::BorrowSummaryOriginKind::local);
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
} // namespace aurex::test
