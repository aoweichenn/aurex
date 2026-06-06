#include <aurex/application/tooling/ide.hpp>
#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/drop_glue.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::test {
namespace {

constexpr base::SourceId PLACE_STATE_TEST_SOURCE_ID{811};
constexpr syntax::ModuleId PLACE_STATE_TEST_MODULE_ID{0U};
constexpr syntax::ItemId PLACE_STATE_TEST_ITEM_ID{0U};
constexpr sema::IdentId PLACE_STATE_TEST_FUNCTION_NAME_ID{97U};
constexpr sema::IdentId PLACE_STATE_TEST_VALUE_NAME_ID{98U};
constexpr sema::IdentId PLACE_STATE_TEST_FIELD_NAME_ID{99U};
constexpr syntax::ExprId PLACE_STATE_TEST_MOVE_EXPR_ID{0U};
constexpr std::string_view PLACE_STATE_TEST_SOURCE =
    "module place.state;\n"
    "struct Pair {\n"
    "  left: i32;\n"
    "  right: i32;\n"
    "}\n"
    "fn place_state_subject(value: i32) -> i32 {\n"
    "  var pair: Pair = Pair { left: value, right: 1 };\n"
    "  pair.right = pair.left + value;\n"
    "  let borrowed: &i32 = &pair.right;\n"
    "  return *borrowed;\n"
    "}\n"
    "fn main() -> void {}\n";

constexpr std::string_view PLACE_STATE_RETAIN_FALSE_SOURCE =
    "module place_state_retain_false;\n"
    "fn reinitialize[T](value: T, replacement: T) -> T {\n"
    "  var current: T = value;\n"
    "  current = replacement;\n"
    "  return current;\n"
    "}\n"
    "fn main() -> i32 {\n"
    "  return reinitialize[i32](1, 2);\n"
    "}\n";

constexpr std::string_view PLACE_STATE_PARTIAL_FIELD_SOURCE =
    "module place_state_partial_field;\n"
    "struct Box[T] {\n"
    "  left: T;\n"
    "  right: T;\n"
    "}\n"
    "fn consume_left[T](box: Box[T]) -> T {\n"
    "  return box.left;\n"
    "}\n"
    "fn reinit_left[T](box: Box[T], replacement: T) -> Box[T] {\n"
    "  var current: Box[T] = box;\n"
    "  let moved: T = current.left;\n"
    "  current.left = replacement;\n"
    "  return current;\n"
    "}\n"
    "fn main() -> i32 {\n"
    "  return 0;\n"
    "}\n";

constexpr std::string_view PLACE_STATE_PARTIAL_TUPLE_SOURCE =
    "module place_state_partial_tuple;\n"
    "fn consume_first[T](pair: (T, T)) -> T {\n"
    "  return pair.0;\n"
    "}\n"
    "fn reinit_first[T](pair: (T, T), replacement: T) -> (T, T) {\n"
    "  var current: (T, T) = pair;\n"
    "  let moved: T = current.0;\n"
    "  current.0 = replacement;\n"
    "  return current;\n"
    "}\n"
    "fn main() -> i32 {\n"
    "  return 0;\n"
    "}\n";

constexpr std::string_view PLACE_STATE_REFERENCE_FIELD_ASSIGNMENT_SOURCE =
    "module place_state_reference_field_assignment;\n"
    "struct Box[T] {\n"
    "  value: T;\n"
    "}\n"
    "fn overwrite[T](box: &mut Box[T], replacement: T) -> void {\n"
    "  box.value = replacement;\n"
    "}\n"
    "fn main() -> i32 {\n"
    "  return 0;\n"
    "}\n";

constexpr std::string_view PLACE_STATE_GENERIC_RETURN_SOURCE = "module place_state_generic_return;\n"
                                                               "struct Holder[T] {\n"
                                                               "  value: T;\n"
                                                               "}\n"
                                                               "fn id[T](value: T) -> T {\n"
                                                               "  return value;\n"
                                                               "}\n"
                                                               "fn make_holder[T](value: T) -> Holder[T] {\n"
                                                               "  return Holder[T] { value: id(value) };\n"
                                                               "}\n"
                                                               "fn unwrap_holder[T](holder: Holder[T]) -> T {\n"
                                                               "  return id(holder.value);\n"
                                                               "}\n"
                                                               "fn main() -> i32 {\n"
                                                               "  let holder = make_holder(7);\n"
                                                               "  return unwrap_holder(holder) - id[i32](7);\n"
                                                               "}\n";

constexpr std::string_view PLACE_STATE_DEFER_EXIT_REINIT_SOURCE =
    "module place_state_defer_exit_reinit;\n"
    "fn consume[T](value: T) -> void {\n"
    "  let borrowed: &T = &value;\n"
    "  if ptraddr(borrowed) == 0usize {\n"
    "    return;\n"
    "  }\n"
    "}\n"
    "fn inspect[T](value: &T) -> void {\n"
    "  if ptraddr(value) == 0usize {\n"
    "    return;\n"
    "  }\n"
    "}\n"
    "fn exercise[T](value: T, replacement: T) -> void {\n"
    "  var current: T = value;\n"
    "  defer consume(current);\n"
    "  inspect(&current);\n"
    "  current = replacement;\n"
    "}\n"
    "fn main() -> i32 {\n"
    "  return 0;\n"
    "}\n";

constexpr std::string_view PLACE_STATE_BRANCH_RETURN_REINIT_SOURCE =
    "module place_state_branch_return_reinit;\n"
    "fn borrow_then_return[T](value: T) -> T {\n"
    "  let borrowed: &T = &value;\n"
    "  if ptraddr(borrowed) == 0usize {\n"
    "    return value;\n"
    "  }\n"
    "  return value;\n"
    "}\n"
    "fn main() -> i32 {\n"
    "  return 0;\n"
    "}\n";

[[nodiscard]] syntax::AstModule parse_place_state_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(PLACE_STATE_TEST_SOURCE_ID, source, diagnostics);
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

[[nodiscard]] sema::CheckedModule analyze_place_state_source(
    const std::string_view source, const bool retain_body_flow_graphs = true)
{
    syntax::AstModule module = parse_place_state_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticOptions options;
    options.retain_body_flow_graphs = retain_body_flow_graphs;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics, options);
    auto result = analyzer.analyze();
    if (!result) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        ADD_FAILURE() << result.error().message;
        return {};
    }
    return result.take_value();
}

[[nodiscard]] std::vector<std::string> analyze_place_state_failure_messages(const std::string_view source)
{
    syntax::AstModule module = parse_place_state_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics, {});
    auto result = analyzer.analyze();
    if (result) {
        ADD_FAILURE() << "expected semantic analysis to fail";
    }
    std::vector<std::string> messages;
    messages.reserve(diagnostics.diagnostics().size() + 1U);
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages.push_back(diagnostic.message);
    }
    if (!result) {
        messages.push_back(result.error().message);
    }
    return messages;
}

[[nodiscard]] std::optional<sema::FunctionLookupKey> find_place_state_function(
    const sema::CheckedModule& checked, const std::string_view name)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.name.view() == name) {
            return entry.first;
        }
    }
    for (const sema::GenericTemplateSignatureInfo& signature : checked.generic_template_signatures) {
        if (signature.name_space == query::DefNamespace::value && signature.name.view() == name) {
            return sema::FunctionLookupKey{
                signature.module.value,
                sema::SEMA_LOOKUP_INVALID_KEY_PART,
                signature.name_id,
            };
        }
    }
    return std::nullopt;
}

[[nodiscard]] const sema::FunctionPlaceStateFacts* find_place_state_facts(
    const sema::CheckedModule& checked, const std::string_view name)
{
    const std::optional<sema::FunctionLookupKey> key = find_place_state_function(checked, name);
    if (!key.has_value()) {
        return nullptr;
    }
    const auto found = checked.place_state_facts.find(*key);
    return found == checked.place_state_facts.end() ? nullptr : &found->second;
}

[[nodiscard]] query::TypeCheckBodyAuthority valid_place_state_authority()
{
    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("place-state.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("place-state.syntax"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("place-state.signature"));
    return authority;
}

[[nodiscard]] tooling::IdeSnapshotRequest place_state_ide_request(const std::string_view source)
{
    tooling::IdeSnapshotRequest request;
    request.path = "/workspace/place_state.ax";
    request.text = std::string(source);
    request.package_identity = "place-state-test";
    request.virtual_buffer_identity = "buffer:place-state";
    return request;
}

[[nodiscard]] const tooling::IdeSemanticFact* find_ide_place_state_fact(
    const tooling::IdeSnapshot& snapshot, const std::string_view name)
{
    for (const tooling::IdeSemanticFact& fact : snapshot.query.semantic_facts) {
        if (fact.kind == tooling::IdeSemanticFactKind::place_state
            && fact.query.kind == query::QueryKind::type_check_body && fact.name == name && fact.checked) {
            return &fact;
        }
    }
    return nullptr;
}

struct PlaceStateGraphHarness {
    syntax::AstModule module;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer;
    syntax::ItemNode function;
    sema::FunctionSignature signature;
    sema::FunctionLookupKey key;
    sema::TypeHandle resource_type = sema::INVALID_TYPE_HANDLE;

    PlaceStateGraphHarness() noexcept : analyzer(module, diagnostics)
    {
        this->key = sema::FunctionLookupKey{
            PLACE_STATE_TEST_MODULE_ID.value,
            sema::SEMA_LOOKUP_INVALID_KEY_PART,
            PLACE_STATE_TEST_FUNCTION_NAME_ID,
        };
        this->resource_type = this->analyzer.state_.checked.types.opaque_struct(
            "place_state.Resource", "place_state_Resource");

        this->function.kind = syntax::ItemKind::fn_decl;
        this->function.name = "place_state_graph_subject";
        this->function.name_id = PLACE_STATE_TEST_FUNCTION_NAME_ID;
        this->function.range = base::SourceRange{PLACE_STATE_TEST_SOURCE_ID, 1U, 20U};
        this->function.visibility = syntax::Visibility::private_;
        this->function.params.push_back(syntax::ParamDecl{
            .name = "value",
            .type = syntax::INVALID_TYPE_ID,
            .range = base::SourceRange{PLACE_STATE_TEST_SOURCE_ID, 21U, 26U},
            .name_id = PLACE_STATE_TEST_VALUE_NAME_ID,
        });

        this->signature.name = this->analyzer.state_.checked.intern_text("place_state_graph_subject");
        this->signature.name_id = PLACE_STATE_TEST_FUNCTION_NAME_ID;
        this->signature.semantic_key = this->key;
        this->signature.module = PLACE_STATE_TEST_MODULE_ID;
        this->signature.return_type = this->analyzer.state_.checked.types.builtin(sema::BuiltinType::void_);
        this->signature.param_types.push_back(this->resource_type);
        this->signature.range = this->function.range;
        this->signature.visibility = syntax::Visibility::private_;
        this->signature.has_definition = true;
        this->signature.definition_item = PLACE_STATE_TEST_ITEM_ID;
    }
};

[[nodiscard]] sema::BodyFlowPoint place_state_graph_point(const sema::BodyFlowPointKind kind, const base::u32 offset)
{
    return sema::BodyFlowPoint{
        .kind = kind,
        .range = base::SourceRange{PLACE_STATE_TEST_SOURCE_ID, offset, offset + 1U},
    };
}

[[nodiscard]] sema::BodyFlowPlace place_state_graph_local_place()
{
    return sema::BodyFlowPlace{
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = PLACE_STATE_TEST_VALUE_NAME_ID,
        .projections = {},
        .range = base::SourceRange{PLACE_STATE_TEST_SOURCE_ID, 30U, 35U},
    };
}

[[nodiscard]] sema::BodyFlowPlace place_state_graph_field_place()
{
    sema::BodyFlowPlace place = place_state_graph_local_place();
    place.projections.push_back(sema::BodyFlowPlaceProjection{
        .kind = sema::BodyFlowPlaceProjectionKind::field,
        .field_name_id = PLACE_STATE_TEST_FIELD_NAME_ID,
    });
    return place;
}

[[nodiscard]] sema::BodyFlowPlace place_state_graph_index_place(
    const syntax::ExprId root_expr, const syntax::ExprId index_expr)
{
    sema::BodyFlowPlace place = place_state_graph_local_place();
    place.root_expr = root_expr;
    place.projections.push_back(sema::BodyFlowPlaceProjection{
        .kind = sema::BodyFlowPlaceProjectionKind::index,
        .expr = index_expr,
    });
    return place;
}

[[nodiscard]] sema::BodyFlowAction place_state_graph_action(const sema::BodyFlowActionKind kind, const base::u32 point,
    const base::u32 place, const base::u32 offset, const syntax::ExprId expr = syntax::INVALID_EXPR_ID)
{
    return sema::BodyFlowAction{
        .kind = kind,
        .point = point,
        .place = place,
        .expr = expr,
        .range = base::SourceRange{PLACE_STATE_TEST_SOURCE_ID, offset, offset + 1U},
    };
}

[[nodiscard]] bool place_state_has_violation(
    const sema::FunctionPlaceStateFacts& facts, const sema::PlaceStateViolationKind kind)
{
    return std::ranges::any_of(facts.violations, [kind](const sema::PlaceStateViolation& violation) {
        return violation.kind == kind;
    });
}

[[nodiscard]] bool place_state_emitted_message(
    const base::DiagnosticSink& diagnostics, const std::string_view message)
{
    return std::ranges::any_of(diagnostics.diagnostics(), [message](const base::Diagnostic& diagnostic) {
        return diagnostic.message == message;
    });
}

[[nodiscard]] bool place_state_has_partial_event(
    const sema::FunctionPlaceStateFacts& facts, const sema::PlaceStateEventKind kind)
{
    return std::ranges::any_of(facts.events, [&facts, kind](const sema::PlaceStateEvent& event) {
        return event.kind == kind && event.place < facts.places.size()
            && facts.places[event.place].has_partial_projection;
    });
}

} // namespace

TEST(CoreUnit, PlaceStateFactsCollectProjectionBorrowAndMoveEvents)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_TEST_SOURCE);
    const sema::FunctionPlaceStateFacts* const facts = find_place_state_facts(checked, "place_state_subject");
    ASSERT_NE(facts, nullptr);
    EXPECT_TRUE(facts->solved);
    EXPECT_FALSE(facts->graph_missing);
    EXPECT_GT(facts->places.size(), 0U);
    EXPECT_GT(facts->events.size(), 0U);
    EXPECT_GT(facts->fingerprint.byte_count, 0U);

    EXPECT_TRUE(std::ranges::any_of(facts->places, [](const sema::PlaceStateFact& fact) {
        return fact.has_partial_projection && (fact.write_count != 0U || fact.reinit_count != 0U);
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::borrow_shared;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::read || event.kind == sema::PlaceStateEventKind::move_candidate;
    }));
}

TEST(CoreUnit, PlaceStateFactsAllowStructFieldMoveAndReinitFromSource)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_PARTIAL_FIELD_SOURCE);

    const sema::FunctionPlaceStateFacts* const consume_facts = find_place_state_facts(checked, "consume_left");
    ASSERT_NE(consume_facts, nullptr);
    EXPECT_TRUE(consume_facts->solved);
    EXPECT_TRUE(consume_facts->violations.empty());
    EXPECT_TRUE(place_state_has_partial_event(*consume_facts, sema::PlaceStateEventKind::move_candidate));
    EXPECT_TRUE(std::ranges::any_of(consume_facts->places, [](const sema::PlaceStateFact& fact) {
        return fact.has_partial_projection && fact.partial_move_count != 0U;
    }));

    const sema::FunctionPlaceStateFacts* const reinit_facts = find_place_state_facts(checked, "reinit_left");
    ASSERT_NE(reinit_facts, nullptr);
    EXPECT_TRUE(reinit_facts->solved);
    EXPECT_TRUE(reinit_facts->violations.empty());
    EXPECT_TRUE(place_state_has_partial_event(*reinit_facts, sema::PlaceStateEventKind::move_candidate));
    EXPECT_TRUE(place_state_has_partial_event(*reinit_facts, sema::PlaceStateEventKind::reinit));
    EXPECT_TRUE(place_state_has_partial_event(*reinit_facts, sema::PlaceStateEventKind::cleanup));
    EXPECT_TRUE(std::ranges::any_of(reinit_facts->places, [](const sema::PlaceStateFact& fact) {
        return fact.has_partial_projection && fact.partial_move_count != 0U;
    }));
    EXPECT_TRUE(std::ranges::any_of(reinit_facts->places, [](const sema::PlaceStateFact& fact) {
        return fact.has_partial_projection && fact.reinit_count != 0U;
    }));
}

TEST(CoreUnit, PlaceStateFactsAllowTupleElementMoveAndReinitFromSource)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_PARTIAL_TUPLE_SOURCE);

    const sema::FunctionPlaceStateFacts* const consume_facts = find_place_state_facts(checked, "consume_first");
    ASSERT_NE(consume_facts, nullptr);
    EXPECT_TRUE(consume_facts->solved);
    EXPECT_TRUE(consume_facts->violations.empty());
    EXPECT_TRUE(place_state_has_partial_event(*consume_facts, sema::PlaceStateEventKind::move_candidate));
    EXPECT_TRUE(std::ranges::any_of(consume_facts->places, [](const sema::PlaceStateFact& fact) {
        return fact.has_partial_projection && fact.partial_move_count != 0U;
    }));

    const sema::FunctionPlaceStateFacts* const reinit_facts = find_place_state_facts(checked, "reinit_first");
    ASSERT_NE(reinit_facts, nullptr);
    EXPECT_TRUE(reinit_facts->solved);
    EXPECT_TRUE(reinit_facts->violations.empty());
    EXPECT_TRUE(place_state_has_partial_event(*reinit_facts, sema::PlaceStateEventKind::move_candidate));
    EXPECT_TRUE(place_state_has_partial_event(*reinit_facts, sema::PlaceStateEventKind::reinit));
    EXPECT_TRUE(place_state_has_partial_event(*reinit_facts, sema::PlaceStateEventKind::cleanup));
    EXPECT_TRUE(std::ranges::any_of(reinit_facts->places, [](const sema::PlaceStateFact& fact) {
        return fact.has_partial_projection && fact.partial_move_count != 0U;
    }));
    EXPECT_TRUE(std::ranges::any_of(reinit_facts->places, [](const sema::PlaceStateFact& fact) {
        return fact.has_partial_projection && fact.reinit_count != 0U;
    }));

    const std::optional<sema::FunctionLookupKey> key = find_place_state_function(checked, "reinit_first");
    ASSERT_TRUE(key.has_value());
    const auto graph = checked.body_flow_graphs.find(*key);
    ASSERT_NE(graph, checked.body_flow_graphs.end());
    EXPECT_TRUE(std::ranges::any_of(graph->second.places, [](const sema::BodyFlowPlace& place) {
        return !place.projections.empty()
            && place.projections.front().kind == sema::BodyFlowPlaceProjectionKind::tuple_element
            && place.projections.front().element_index == 0U;
    }));
}

TEST(CoreUnit, PlaceStateFactsKeepReferenceFieldResourceOverwriteUnsupported)
{
    const std::vector<std::string> messages =
        analyze_place_state_failure_messages(PLACE_STATE_REFERENCE_FIELD_ASSIGNMENT_SOURCE);

    EXPECT_TRUE(std::ranges::any_of(messages, [](const std::string& message) {
        return std::string_view{message} == sema::SEMA_RESOURCE_PLACE_ASSIGNMENT_UNSUPPORTED;
    })) << (messages.empty() ? std::string{"<no diagnostics>"} : messages.front());
}

TEST(CoreUnit, PlaceStateFactsIgnoreOwnedGenericReturnBorrowSummary)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_GENERIC_RETURN_SOURCE);

    const sema::FunctionPlaceStateFacts* const make_facts = find_place_state_facts(checked, "make_holder");
    ASSERT_NE(make_facts, nullptr);
    EXPECT_TRUE(make_facts->solved);
    EXPECT_TRUE(make_facts->violations.empty());
    EXPECT_TRUE(std::ranges::any_of(make_facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::move_candidate;
    }));
    EXPECT_FALSE(std::ranges::any_of(make_facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::borrow_shared
            || event.kind == sema::PlaceStateEventKind::borrow_mutable;
    }));

    const sema::FunctionPlaceStateFacts* const unwrap_facts = find_place_state_facts(checked, "unwrap_holder");
    ASSERT_NE(unwrap_facts, nullptr);
    EXPECT_TRUE(unwrap_facts->solved);
    EXPECT_TRUE(unwrap_facts->violations.empty());
    EXPECT_TRUE(place_state_has_partial_event(*unwrap_facts, sema::PlaceStateEventKind::move_candidate));
    EXPECT_FALSE(std::ranges::any_of(unwrap_facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::borrow_shared
            || event.kind == sema::PlaceStateEventKind::borrow_mutable;
    }));
}

TEST(CoreUnit, PlaceStateFactsTreatDeferAsExitCleanupAfterReinit)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_DEFER_EXIT_REINIT_SOURCE);

    const sema::FunctionPlaceStateFacts* const facts = find_place_state_facts(checked, "exercise");
    ASSERT_NE(facts, nullptr);
    EXPECT_TRUE(facts->solved);
    EXPECT_TRUE(facts->violations.empty());
    EXPECT_FALSE(place_state_has_violation(*facts, sema::PlaceStateViolationKind::use_after_move));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::borrow_shared;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::reinit;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::move_candidate;
    }));
}

TEST(CoreUnit, PlaceStateFactsDoNotJoinReturnPathIntoLaterSiblingStatements)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_BRANCH_RETURN_REINIT_SOURCE);

    const sema::FunctionPlaceStateFacts* const facts = find_place_state_facts(checked, "borrow_then_return");
    ASSERT_NE(facts, nullptr);
    EXPECT_TRUE(facts->solved);
    EXPECT_TRUE(facts->violations.empty());
    EXPECT_FALSE(place_state_has_violation(*facts, sema::PlaceStateViolationKind::maybe_uninitialized_use));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::borrow_shared;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::move_candidate;
    }));
}

TEST(CoreUnit, PlaceStateFactsDumpAndCheckedModuleDumpExposeStableFacts)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_TEST_SOURCE);
    const sema::FunctionPlaceStateFacts* const facts = find_place_state_facts(checked, "place_state_subject");
    ASSERT_NE(facts, nullptr);

    const std::string fact_dump = sema::dump_function_place_state_facts(*facts);
    EXPECT_NE(fact_dump.find("place_state_facts function="), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("places:"), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("events:"), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("violations:"), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("enforced=true"), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("drop_flag_live="), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("borrow_shared"), std::string::npos) << fact_dump;

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("place_state_facts"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("place_state "), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("partials="), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("partial_moves="), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("violations="), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("diagnostics="), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("enforced=true"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("event #"), std::string::npos) << checked_dump;
}

TEST(CoreUnit, PlaceStateFactsDumpCoversMissingGraphAndDropFacts)
{
    sema::FunctionPlaceStateFacts facts;
    facts.function = sema::FunctionLookupKey{7U, sema::SEMA_LOOKUP_INVALID_KEY_PART, sema::INVALID_IDENT_ID};
    facts.graph_missing = true;
    facts.solved = false;
    facts.diagnostic_mode_enforced = true;
    facts.part_index = 3U;
    facts.places.push_back(sema::PlaceStateFact{
        .place = 0U,
        .root_kind = sema::BodyFlowPlaceRootKind::none,
        .root_name_id = sema::INVALID_IDENT_ID,
        .root_expr = syntax::INVALID_EXPR_ID,
        .type = sema::TypeHandle{4U},
        .projection_count = 0U,
        .first_action_point = 10U,
        .last_action_point = 12U,
        .last_write_point = sema::SEMA_BODY_FLOW_INVALID_INDEX,
        .last_move_point = sema::SEMA_BODY_FLOW_INVALID_INDEX,
        .last_drop_point = 12U,
        .read_count = 0U,
        .write_count = 0U,
        .reinit_count = 0U,
        .move_candidate_count = 0U,
        .drop_count = 1U,
        .cleanup_count = 0U,
        .borrow_count = 0U,
        .initialization = sema::PlaceStateInitialization::uninitialized,
        .move_state = sema::PlaceStateMoveState::none,
        .drop_state = sema::PlaceStateDropState::dropped,
        .needs_drop = true,
        .has_partial_projection = false,
    });
    facts.places.push_back(sema::PlaceStateFact{
        .place = 1U,
        .root_kind = sema::BodyFlowPlaceRootKind::local,
        .root_name_id = sema::IdentId{11U},
        .root_expr = syntax::ExprId{5U},
        .type = sema::TypeHandle{8U},
        .projection_count = 1U,
        .first_action_point = 20U,
        .last_action_point = 20U,
        .last_write_point = 20U,
        .last_move_point = sema::SEMA_BODY_FLOW_INVALID_INDEX,
        .last_drop_point = sema::SEMA_BODY_FLOW_INVALID_INDEX,
        .read_count = 0U,
        .write_count = 1U,
        .reinit_count = 0U,
        .move_candidate_count = 0U,
        .drop_count = 0U,
        .cleanup_count = 0U,
        .borrow_count = 0U,
        .initialization = sema::PlaceStateInitialization::initialized,
        .move_state = sema::PlaceStateMoveState::none,
        .drop_state = sema::PlaceStateDropState::none,
        .needs_drop = false,
        .has_partial_projection = true,
    });
    facts.events.push_back(sema::PlaceStateEvent{
        .kind = sema::PlaceStateEventKind::drop,
        .place = 0U,
        .action = 2U,
        .point = 12U,
        .type = sema::TypeHandle{4U},
        .range = base::SourceRange{PLACE_STATE_TEST_SOURCE_ID, 4U, 9U},
    });
    facts.violations.push_back(sema::PlaceStateViolation{
        .kind = sema::PlaceStateViolationKind::use_after_move,
        .place = 0U,
        .action = 2U,
        .point = 12U,
        .related_place = 0U,
        .related_action = 1U,
        .diagnostic_emitted = true,
        .range = base::SourceRange{PLACE_STATE_TEST_SOURCE_ID, 4U, 9U},
        .related_range = base::SourceRange{PLACE_STATE_TEST_SOURCE_ID, 2U, 3U},
    });

    const query::StableFingerprint128 before = sema::function_place_state_facts_fingerprint(facts);
    facts.fingerprint = before;
    const std::string dump = sema::dump_function_place_state_facts(facts);

    EXPECT_NE(before.byte_count, 0U);
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::drop), "drop");
    EXPECT_NE(dump.find("function=7:"), std::string::npos) << dump;
    EXPECT_NE(dump.find(":- places=2"), std::string::npos) << dump;
    EXPECT_NE(dump.find("violations=1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("solved=false"), std::string::npos) << dump;
    EXPECT_NE(dump.find("enforced=true"), std::string::npos) << dump;
    EXPECT_NE(dump.find("graph_missing=true"), std::string::npos) << dump;
    EXPECT_NE(dump.find("needs_drop=true"), std::string::npos) << dump;
    EXPECT_NE(dump.find("partial=false"), std::string::npos) << dump;
    EXPECT_NE(dump.find("drop_flag_live="), std::string::npos) << dump;
    EXPECT_NE(dump.find("violations:"), std::string::npos) << dump;
    EXPECT_NE(dump.find("v0 use_after_move"), std::string::npos) << dump;
    EXPECT_NE(dump.find("drop place=0"), std::string::npos) << dump;

    sema::FunctionPlaceStateFacts checked_dump_facts = facts;
    for (sema::PlaceStateFact& fact : checked_dump_facts.places) {
        fact.type = sema::INVALID_TYPE_HANDLE;
    }
    for (sema::PlaceStateEvent& event : checked_dump_facts.events) {
        event.type = sema::INVALID_TYPE_HANDLE;
    }
    sema::CheckedModule synthetic_checked;
    synthetic_checked.place_state_facts.emplace(checked_dump_facts.function, checked_dump_facts);
    const std::string checked_dump = sema::dump_checked_module(synthetic_checked);
    EXPECT_NE(checked_dump.find("place_state 7:"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find(":- places=2"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("solved=false"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("enforced=true"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("graph_missing=true"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("type=<invalid>"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("event #0 drop"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("violation #0 use_after_move"), std::string::npos) << checked_dump;

    facts.events.front().point += 1U;
    EXPECT_NE(sema::function_place_state_facts_fingerprint(facts), before);
}

TEST(CoreUnit, PlaceStateFactsPopulateTypeCheckBodyAuthority)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_TEST_SOURCE);
    const std::optional<sema::FunctionLookupKey> key =
        find_place_state_function(checked, "place_state_subject");
    ASSERT_TRUE(key.has_value());

    query::TypeCheckBodyAuthority authority = valid_place_state_authority();
    sema::populate_type_check_body_borrow_authority(authority, checked, *key);
    EXPECT_TRUE(authority.has_place_state_facts);
    EXPECT_GT(authority.place_state_place_count, 0U);
    EXPECT_GT(authority.place_state_event_count, 0U);
    EXPECT_GT(authority.place_state_partial_projection_count, 0U);
    EXPECT_TRUE(authority.place_state_has_partial_projection);
    EXPECT_TRUE(authority.place_state_has_borrow);
    EXPECT_EQ(authority.place_state_partial_move_count, 0U);
    EXPECT_EQ(authority.place_state_skipped_drop_count, 0U);
    EXPECT_EQ(authority.place_state_violation_count, 0U);
    EXPECT_EQ(authority.place_state_emitted_diagnostic_count, 0U);
    EXPECT_FALSE(authority.place_state_has_partial_move);
    EXPECT_FALSE(authority.place_state_has_skipped_drop);
    EXPECT_FALSE(authority.place_state_has_violation);
    EXPECT_FALSE(authority.place_state_has_emitted_diagnostics);
    EXPECT_GT(authority.place_state_fingerprint.byte_count, 0U);

    const query::QueryResultFingerprint baseline = query::type_check_body_result_fingerprint(authority);
    query::TypeCheckBodyAuthority changed = authority;
    changed.place_state_event_count += 1U;
    EXPECT_NE(query::type_check_body_result_fingerprint(changed), baseline);

    query::TypeCheckBodyAuthority b2_changed = authority;
    b2_changed.place_state_partial_move_count = 1U;
    b2_changed.place_state_skipped_drop_count = 1U;
    b2_changed.place_state_violation_count = 1U;
    b2_changed.place_state_emitted_diagnostic_count = 1U;
    b2_changed.place_state_has_partial_move = true;
    b2_changed.place_state_has_skipped_drop = true;
    b2_changed.place_state_has_violation = true;
    b2_changed.place_state_has_emitted_diagnostics = true;
    EXPECT_NE(query::type_check_body_result_fingerprint(b2_changed), baseline);
}

TEST(CoreUnit, PlaceStateFactsRetainFalsePrecheckCollectsResourceMoves)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_RETAIN_FALSE_SOURCE, false);
    const std::optional<sema::FunctionLookupKey> key = find_place_state_function(checked, "reinitialize");
    ASSERT_TRUE(key.has_value());

    const auto facts = checked.place_state_facts.find(*key);
    ASSERT_NE(facts, checked.place_state_facts.end());
    EXPECT_TRUE(facts->second.solved);
    EXPECT_FALSE(facts->second.graph_missing);
    EXPECT_GT(facts->second.places.size(), 0U);
    EXPECT_GT(facts->second.events.size(), 0U);
    EXPECT_FALSE(checked.body_flow_graphs.contains(*key));
}

TEST(CoreUnit, PlaceStateFactsEnforceDropAfterDropAndUseAfterDrop)
{
    PlaceStateGraphHarness harness;
    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 40U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 41U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 42U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 43U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.actions = {
        place_state_graph_action(sema::BodyFlowActionKind::drop, 2U, 0U, 44U),
        place_state_graph_action(sema::BodyFlowActionKind::read, 3U, 0U, 45U),
        place_state_graph_action(sema::BodyFlowActionKind::drop, 3U, 0U, 46U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    EXPECT_TRUE(facts->second.solved);
    EXPECT_TRUE(facts->second.diagnostic_mode_enforced);
    EXPECT_TRUE(place_state_has_violation(facts->second, sema::PlaceStateViolationKind::use_after_move));
    EXPECT_TRUE(place_state_has_violation(facts->second, sema::PlaceStateViolationKind::double_drop));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_USE_AFTER_MOVE));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_DOUBLE_DROP));
    EXPECT_TRUE(std::ranges::any_of(facts->second.events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::drop;
    }));
}

TEST(CoreUnit, PlaceStateFactsEnforcePartialMoveWholePlaceAccess)
{
    PlaceStateGraphHarness harness;
    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 50U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 51U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 52U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 53U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 54U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 4U},
        sema::BodyFlowEdge{.from = 4U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.places.push_back(place_state_graph_field_place());
    graph.actions = {
        place_state_graph_action(sema::BodyFlowActionKind::drop, 2U, 1U, 55U),
        place_state_graph_action(sema::BodyFlowActionKind::read, 3U, 0U, 56U),
        place_state_graph_action(sema::BodyFlowActionKind::drop, 4U, 0U, 57U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    EXPECT_TRUE(place_state_has_violation(facts->second, sema::PlaceStateViolationKind::use_after_partial_move));
    EXPECT_TRUE(place_state_has_violation(facts->second, sema::PlaceStateViolationKind::drop_after_partial_move));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_USE_AFTER_PARTIAL_MOVE));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_DROP_AFTER_PARTIAL_MOVE));
    EXPECT_TRUE(std::ranges::any_of(facts->second.places, [](const sema::PlaceStateFact& fact) {
        return fact.is_partially_moved || fact.last_partial_move_point != sema::SEMA_BODY_FLOW_INVALID_INDEX;
    }));
}

TEST(CoreUnit, PlaceStateFactsEnforceDropAfterMove)
{
    PlaceStateGraphHarness harness;
    harness.analyzer.record_expr_owned_use_mode(PLACE_STATE_TEST_MOVE_EXPR_ID, sema::OwnedUseMode::owned_consume);

    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 60U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 61U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 62U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 63U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.actions = {
        place_state_graph_action(
            sema::BodyFlowActionKind::move_candidate, 2U, 0U, 64U, PLACE_STATE_TEST_MOVE_EXPR_ID),
        place_state_graph_action(sema::BodyFlowActionKind::drop, 3U, 0U, 65U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    EXPECT_TRUE(place_state_has_violation(facts->second, sema::PlaceStateViolationKind::drop_after_move));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_DROP_AFTER_MOVE));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_MOVE_OCCURRED));
}

TEST(CoreUnit, PlaceStateFactsEnforceMaybeUninitializedAfterMoveJoin)
{
    PlaceStateGraphHarness harness;
    harness.analyzer.record_expr_owned_use_mode(PLACE_STATE_TEST_MOVE_EXPR_ID, sema::OwnedUseMode::owned_consume);

    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 70U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 71U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 72U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 73U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 0U, .to = 3U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.actions = {
        place_state_graph_action(
            sema::BodyFlowActionKind::move_candidate, 2U, 0U, 74U, PLACE_STATE_TEST_MOVE_EXPR_ID),
        place_state_graph_action(sema::BodyFlowActionKind::read, 3U, 0U, 75U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    EXPECT_TRUE(place_state_has_violation(facts->second, sema::PlaceStateViolationKind::maybe_uninitialized_use));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_MAYBE_UNINITIALIZED_USE));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_MOVE_OCCURRED));
}

TEST(CoreUnit, PlaceStateFactsEnforceBorrowAfterPartialMove)
{
    PlaceStateGraphHarness harness;
    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 80U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 81U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 82U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 83U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.places.push_back(place_state_graph_field_place());
    graph.actions = {
        place_state_graph_action(sema::BodyFlowActionKind::drop, 2U, 1U, 84U),
        place_state_graph_action(sema::BodyFlowActionKind::borrow_shared, 3U, 0U, 85U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    EXPECT_TRUE(place_state_has_violation(facts->second, sema::PlaceStateViolationKind::use_after_partial_move));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_USE_AFTER_PARTIAL_MOVE));
    EXPECT_TRUE(std::ranges::any_of(facts->second.events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::borrow_shared;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->second.violations, [](const sema::PlaceStateViolation& violation) {
        return violation.kind == sema::PlaceStateViolationKind::use_after_partial_move && violation.diagnostic_emitted;
    }));
}

TEST(CoreUnit, PlaceStateFactsNormalizeIndexedProjectionAliases)
{
    PlaceStateGraphHarness harness;
    const sema::TypeHandle array_type = harness.analyzer.state_.checked.types.array(2U, harness.resource_type);
    harness.signature.param_types.front() = array_type;
    harness.analyzer.state_.checked.expr_owned_use_modes.resize(8U, sema::OwnedUseMode::none);
    harness.analyzer.record_expr_owned_use_mode(syntax::ExprId{2U}, sema::OwnedUseMode::owned_consume);

    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 86U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 87U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 88U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 89U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_index_place(syntax::ExprId{1U}, syntax::ExprId{4U}));
    graph.places.push_back(place_state_graph_index_place(syntax::ExprId{3U}, syntax::ExprId{5U}));
    graph.actions = {
        place_state_graph_action(sema::BodyFlowActionKind::move_candidate, 2U, 0U, 90U, syntax::ExprId{2U}),
        place_state_graph_action(sema::BodyFlowActionKind::read, 3U, 1U, 91U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    ASSERT_EQ(facts->second.places.size(), 2U);
    EXPECT_TRUE(place_state_has_violation(facts->second, sema::PlaceStateViolationKind::use_after_move));
    EXPECT_TRUE(place_state_emitted_message(harness.diagnostics, sema::SEMA_PLACE_STATE_USE_AFTER_MOVE));
    EXPECT_EQ(facts->second.places[1].move_candidate_count, 1U);
    EXPECT_EQ(facts->second.places[1].read_count, 1U);
    EXPECT_TRUE(facts->second.places[1].has_partial_projection);
}

TEST(CoreUnit, PlaceStateFactsSkipCleanupAfterMoveWithoutLiveDropFlag)
{
    PlaceStateGraphHarness harness;
    harness.analyzer.record_expr_owned_use_mode(PLACE_STATE_TEST_MOVE_EXPR_ID, sema::OwnedUseMode::owned_consume);

    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 90U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 91U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 92U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 93U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.actions = {
        place_state_graph_action(
            sema::BodyFlowActionKind::move_candidate, 2U, 0U, 94U, PLACE_STATE_TEST_MOVE_EXPR_ID),
        place_state_graph_action(sema::BodyFlowActionKind::cleanup_storage, 3U, 0U, 95U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    ASSERT_FALSE(facts->second.places.empty());
    EXPECT_TRUE(facts->second.violations.empty());
    EXPECT_EQ(facts->second.places.front().cleanup_count, 1U);
    EXPECT_EQ(facts->second.places.front().skipped_drop_count, 1U);
    EXPECT_EQ(facts->second.places.front().drop_state, sema::PlaceStateDropState::none);
    EXPECT_FALSE(facts->second.places.front().drop_flag_live);
    EXPECT_TRUE(std::ranges::any_of(facts->second.events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::cleanup;
    }));
}

TEST(CoreUnit, PlaceStateFactsMarkMissingGraphWhenResourceSignatureNeedsCheck)
{
    PlaceStateGraphHarness harness;

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    EXPECT_TRUE(facts->second.graph_missing);
    EXPECT_FALSE(facts->second.solved);
    EXPECT_TRUE(facts->second.places.empty());
    EXPECT_TRUE(facts->second.events.empty());
    EXPECT_GT(facts->second.fingerprint.byte_count, 0U);
}

TEST(CoreUnit, PlaceStateFactsSkipControlActionsAndMapCleanupAndMutableBorrows)
{
    PlaceStateGraphHarness harness;
    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 100U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 101U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 102U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.actions = {
        place_state_graph_action(sema::BodyFlowActionKind::call, 2U, 0U, 103U),
        place_state_graph_action(sema::BodyFlowActionKind::return_, 2U, 0U, 104U),
        place_state_graph_action(sema::BodyFlowActionKind::branch, 2U, 0U, 105U),
        place_state_graph_action(sema::BodyFlowActionKind::read, 2U, 7U, 106U),
        place_state_graph_action(sema::BodyFlowActionKind::cleanup_scope, 2U, 0U, 107U),
        place_state_graph_action(sema::BodyFlowActionKind::borrow_mutable, 2U, 0U, 108U),
        place_state_graph_action(sema::BodyFlowActionKind::call_receiver_reserve, 2U, 0U, 109U),
        place_state_graph_action(sema::BodyFlowActionKind::call_receiver_activate, 2U, 0U, 110U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    EXPECT_TRUE(facts->second.solved);
    EXPECT_EQ(facts->second.events.size(), 4U);
    EXPECT_TRUE(std::ranges::any_of(facts->second.events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::cleanup;
    }));
    const base::usize mutable_borrow_count =
        static_cast<base::usize>(std::ranges::count_if(facts->second.events, [](const sema::PlaceStateEvent& event) {
            return event.kind == sema::PlaceStateEventKind::borrow_mutable;
        }));
    EXPECT_EQ(mutable_borrow_count, 3U);
}

TEST(CoreUnit, PlaceStateFactsReinitializePartialDropRestoresWholePlace)
{
    PlaceStateGraphHarness harness;
    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 120U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 121U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 122U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 123U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 124U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 4U},
        sema::BodyFlowEdge{.from = 4U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.places.push_back(place_state_graph_field_place());
    graph.actions = {
        place_state_graph_action(sema::BodyFlowActionKind::drop, 2U, 1U, 125U),
        place_state_graph_action(sema::BodyFlowActionKind::reinit, 3U, 1U, 126U),
        place_state_graph_action(sema::BodyFlowActionKind::read, 4U, 0U, 127U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    ASSERT_GE(facts->second.places.size(), 2U);
    EXPECT_TRUE(facts->second.violations.empty());
    EXPECT_FALSE(facts->second.places[0].is_partially_moved);
    EXPECT_EQ(facts->second.places[0].move_state, sema::PlaceStateMoveState::none);
    EXPECT_TRUE(std::ranges::any_of(facts->second.events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::reinit;
    }));
}

TEST(CoreUnit, PlaceStateFactsMergeNonConsumingMoveCandidatesAtJoin)
{
    PlaceStateGraphHarness harness;
    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 130U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 131U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 132U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 133U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 0U, .to = 3U},
        sema::BodyFlowEdge{.from = 2U, .to = 3U},
        sema::BodyFlowEdge{.from = 3U, .to = 1U},
    };
    graph.places.push_back(place_state_graph_local_place());
    graph.actions = {
        place_state_graph_action(sema::BodyFlowActionKind::move_candidate, 2U, 0U, 134U),
        place_state_graph_action(sema::BodyFlowActionKind::read, 3U, 0U, 135U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    EXPECT_TRUE(facts->second.violations.empty());
    EXPECT_TRUE(std::ranges::any_of(facts->second.events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::move_candidate;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->second.places, [](const sema::PlaceStateFact& fact) {
        return fact.move_candidate_count == 1U;
    }));
}

TEST(CoreUnit, PlaceStateFactsReadUnknownLocalBecomesMaybeInitialized)
{
    PlaceStateGraphHarness harness;
    sema::BodyFlowPlace unknown_local = place_state_graph_local_place();
    unknown_local.root_name_id = PLACE_STATE_TEST_FIELD_NAME_ID;

    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        place_state_graph_point(sema::BodyFlowPointKind::entry, 140U),
        place_state_graph_point(sema::BodyFlowPointKind::exit, 141U),
        place_state_graph_point(sema::BodyFlowPointKind::sequence, 142U),
    };
    graph.edges = {
        sema::BodyFlowEdge{.from = 0U, .to = 2U},
        sema::BodyFlowEdge{.from = 2U, .to = 1U},
    };
    graph.places.push_back(unknown_local);
    graph.actions = {
        place_state_graph_action(sema::BodyFlowActionKind::read, 2U, 0U, 143U),
    };
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_place_states(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.place_state_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.place_state_facts.end());
    ASSERT_FALSE(facts->second.places.empty());
    EXPECT_EQ(facts->second.places.front().read_count, 1U);
    EXPECT_TRUE(std::ranges::any_of(facts->second.events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::read;
    }));
}

TEST(CoreUnit, PlaceStateFactsCopyMoveAndIdeProjectionPreserveFacts)
{
    const sema::CheckedModule checked = analyze_place_state_source(PLACE_STATE_TEST_SOURCE);
    const sema::FunctionPlaceStateFacts* const facts = find_place_state_facts(checked, "place_state_subject");
    ASSERT_NE(facts, nullptr);
    const query::StableFingerprint128 original_fingerprint = facts->fingerprint;

    sema::CheckedModule copied = checked;
    const sema::FunctionPlaceStateFacts* const copied_facts =
        find_place_state_facts(copied, "place_state_subject");
    ASSERT_NE(copied_facts, nullptr);
    EXPECT_EQ(copied_facts->fingerprint, original_fingerprint);

    sema::CheckedModule moved = std::move(copied);
    const sema::FunctionPlaceStateFacts* const moved_facts = find_place_state_facts(moved, "place_state_subject");
    ASSERT_NE(moved_facts, nullptr);
    EXPECT_EQ(moved_facts->fingerprint, original_fingerprint);

    const tooling::IdeSnapshot snapshot = tooling::build_ide_snapshot(place_state_ide_request(PLACE_STATE_TEST_SOURCE));
    ASSERT_TRUE(snapshot.checked_semantics);
    EXPECT_FALSE(snapshot.has_errors);
    const tooling::IdeSemanticFact* const ide_fact = find_ide_place_state_fact(snapshot, "place_state_subject");
    ASSERT_NE(ide_fact, nullptr);
    EXPECT_NE(ide_fact->detail.find("place_state places="), std::string::npos) << ide_fact->detail;
    EXPECT_NE(ide_fact->detail.find("events="), std::string::npos) << ide_fact->detail;
    EXPECT_NE(ide_fact->detail.find("partials="), std::string::npos) << ide_fact->detail;
    EXPECT_NE(ide_fact->detail.find("partial_moves="), std::string::npos) << ide_fact->detail;
    EXPECT_NE(ide_fact->detail.find("violations="), std::string::npos) << ide_fact->detail;
    EXPECT_NE(ide_fact->detail.find("diagnostics="), std::string::npos) << ide_fact->detail;
    EXPECT_NE(ide_fact->detail.find("enforced="), std::string::npos) << ide_fact->detail;

    const base::usize offset = PLACE_STATE_TEST_SOURCE.find("place_state_subject");
    ASSERT_NE(offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->label.find("place_state=places="), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("/partial_moves="), std::string::npos) << hover->label;
    EXPECT_NE(hover->label.find("/violations="), std::string::npos) << hover->label;
}

TEST(CoreUnit, PlaceStateEnumNameFallbacksAreStable)
{
    EXPECT_EQ(sema::place_state_initialization_name(sema::PlaceStateInitialization::unknown), "unknown");
    EXPECT_EQ(sema::place_state_initialization_name(sema::PlaceStateInitialization::initialized), "initialized");
    EXPECT_EQ(
        sema::place_state_initialization_name(sema::PlaceStateInitialization::maybe_initialized), "maybe_initialized");
    EXPECT_EQ(sema::place_state_initialization_name(sema::PlaceStateInitialization::uninitialized), "uninitialized");
    EXPECT_EQ(sema::place_state_move_state_name(sema::PlaceStateMoveState::none), "none");
    EXPECT_EQ(sema::place_state_move_state_name(sema::PlaceStateMoveState::move_candidate), "move_candidate");
    EXPECT_EQ(sema::place_state_move_state_name(sema::PlaceStateMoveState::maybe_moved), "maybe_moved");
    EXPECT_EQ(sema::place_state_drop_state_name(sema::PlaceStateDropState::none), "none");
    EXPECT_EQ(sema::place_state_drop_state_name(sema::PlaceStateDropState::drop_pending), "drop_pending");
    EXPECT_EQ(sema::place_state_drop_state_name(sema::PlaceStateDropState::dropped), "dropped");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::read), "read");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::write), "write");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::reinit), "reinit");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::move_candidate), "move_candidate");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::drop), "drop");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::cleanup), "cleanup");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::borrow_shared), "borrow_shared");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::borrow_mutable), "borrow_mutable");
    EXPECT_EQ(sema::place_state_violation_kind_name(sema::PlaceStateViolationKind::use_after_move), "use_after_move");
    EXPECT_EQ(sema::place_state_violation_kind_name(sema::PlaceStateViolationKind::maybe_uninitialized_use),
        "maybe_uninitialized_use");
    EXPECT_EQ(sema::place_state_violation_kind_name(sema::PlaceStateViolationKind::use_after_partial_move),
        "use_after_partial_move");
    EXPECT_EQ(
        sema::place_state_violation_kind_name(sema::PlaceStateViolationKind::drop_after_move), "drop_after_move");
    EXPECT_EQ(sema::place_state_violation_kind_name(sema::PlaceStateViolationKind::drop_after_partial_move),
        "drop_after_partial_move");
    EXPECT_EQ(sema::place_state_violation_kind_name(sema::PlaceStateViolationKind::double_drop), "double_drop");
    EXPECT_EQ(sema::place_state_violation_message(sema::PlaceStateViolationKind::use_after_move),
        sema::SEMA_PLACE_STATE_USE_AFTER_MOVE);
    EXPECT_EQ(sema::place_state_violation_message(sema::PlaceStateViolationKind::maybe_uninitialized_use),
        sema::SEMA_PLACE_STATE_MAYBE_UNINITIALIZED_USE);
    EXPECT_EQ(sema::place_state_violation_message(sema::PlaceStateViolationKind::use_after_partial_move),
        sema::SEMA_PLACE_STATE_USE_AFTER_PARTIAL_MOVE);
    EXPECT_EQ(sema::place_state_violation_message(sema::PlaceStateViolationKind::drop_after_move),
        sema::SEMA_PLACE_STATE_DROP_AFTER_MOVE);
    EXPECT_EQ(sema::place_state_violation_message(sema::PlaceStateViolationKind::drop_after_partial_move),
        sema::SEMA_PLACE_STATE_DROP_AFTER_PARTIAL_MOVE);
    EXPECT_EQ(
        sema::place_state_violation_message(sema::PlaceStateViolationKind::double_drop), sema::SEMA_PLACE_STATE_DOUBLE_DROP);
    EXPECT_EQ(sema::place_state_initialization_name(static_cast<sema::PlaceStateInitialization>(255U)), "<invalid>");
    EXPECT_EQ(sema::place_state_move_state_name(static_cast<sema::PlaceStateMoveState>(255U)), "<invalid>");
    EXPECT_EQ(sema::place_state_drop_state_name(static_cast<sema::PlaceStateDropState>(255U)), "<invalid>");
    EXPECT_EQ(sema::place_state_event_kind_name(static_cast<sema::PlaceStateEventKind>(255U)), "<invalid>");
    EXPECT_EQ(sema::place_state_violation_kind_name(static_cast<sema::PlaceStateViolationKind>(255U)), "<invalid>");
    EXPECT_EQ(sema::place_state_violation_message(static_cast<sema::PlaceStateViolationKind>(255U)),
        sema::SEMA_PLACE_STATE_USE_AFTER_MOVE);
}

TEST(CoreUnit, DropGluePublicResultErrorIsStableForInvalidRoot)
{
    const base::Result<sema::DropGluePlan> plan =
        sema::build_drop_glue_plan(sema::CheckedModule{}, sema::INVALID_TYPE_HANDLE);
    ASSERT_FALSE(plan);
    EXPECT_EQ(plan.error().code, base::ErrorCode::internal_error);
    EXPECT_NE(plan.error().message.find("invalid drop-glue root type"), std::string::npos);
}

} // namespace aurex::test
