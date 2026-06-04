#include <aurex/application/tooling/ide.hpp>
#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/drop_glue.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::SourceId PLACE_STATE_TEST_SOURCE_ID{811};
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

[[nodiscard]] sema::CheckedModule analyze_place_state_source(const std::string_view source)
{
    syntax::AstModule module = parse_place_state_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticOptions options;
    options.retain_body_flow_graphs = true;
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

[[nodiscard]] std::optional<sema::FunctionLookupKey> find_place_state_function(
    const sema::CheckedModule& checked, const std::string_view name)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.name.view() == name) {
            return entry.first;
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
        return fact.has_partial_projection && fact.write_count != 0;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::borrow_shared;
    }));
    EXPECT_TRUE(std::ranges::any_of(facts->events, [](const sema::PlaceStateEvent& event) {
        return event.kind == sema::PlaceStateEventKind::read || event.kind == sema::PlaceStateEventKind::move_candidate;
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
    EXPECT_NE(fact_dump.find("borrow_shared"), std::string::npos) << fact_dump;

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("place_state_facts"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("place_state "), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("partials="), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("event #"), std::string::npos) << checked_dump;
}

TEST(CoreUnit, PlaceStateFactsDumpCoversMissingGraphAndDropFacts)
{
    sema::FunctionPlaceStateFacts facts;
    facts.function = sema::FunctionLookupKey{7U, sema::SEMA_LOOKUP_INVALID_KEY_PART, sema::INVALID_IDENT_ID};
    facts.graph_missing = true;
    facts.solved = false;
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

    const query::StableFingerprint128 before = sema::function_place_state_facts_fingerprint(facts);
    facts.fingerprint = before;
    const std::string dump = sema::dump_function_place_state_facts(facts);

    EXPECT_NE(before.byte_count, 0U);
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::drop), "drop");
    EXPECT_NE(dump.find("function=7:"), std::string::npos) << dump;
    EXPECT_NE(dump.find(":- places=2"), std::string::npos) << dump;
    EXPECT_NE(dump.find("solved=false"), std::string::npos) << dump;
    EXPECT_NE(dump.find("graph_missing=true"), std::string::npos) << dump;
    EXPECT_NE(dump.find("needs_drop=true"), std::string::npos) << dump;
    EXPECT_NE(dump.find("partial=false"), std::string::npos) << dump;
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
    EXPECT_NE(checked_dump.find("graph_missing=true"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("type=<invalid>"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("event #0 drop"), std::string::npos) << checked_dump;

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
    EXPECT_GT(authority.place_state_fingerprint.byte_count, 0U);

    const query::QueryResultFingerprint baseline = query::type_check_body_result_fingerprint(authority);
    query::TypeCheckBodyAuthority changed = authority;
    changed.place_state_event_count += 1U;
    EXPECT_NE(query::type_check_body_result_fingerprint(changed), baseline);
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

    const base::usize offset = PLACE_STATE_TEST_SOURCE.find("place_state_subject");
    ASSERT_NE(offset, std::string_view::npos);
    const std::optional<tooling::IdeHoverInfo> hover = tooling::hover_at_offset(snapshot, offset);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->label.find("place_state=places="), std::string::npos) << hover->label;
}

TEST(CoreUnit, PlaceStateEnumNameFallbacksAreStable)
{
    EXPECT_EQ(sema::place_state_initialization_name(sema::PlaceStateInitialization::unknown), "unknown");
    EXPECT_EQ(sema::place_state_move_state_name(sema::PlaceStateMoveState::maybe_moved), "maybe_moved");
    EXPECT_EQ(sema::place_state_drop_state_name(sema::PlaceStateDropState::dropped), "dropped");
    EXPECT_EQ(sema::place_state_event_kind_name(sema::PlaceStateEventKind::cleanup), "cleanup");
    EXPECT_EQ(sema::place_state_initialization_name(static_cast<sema::PlaceStateInitialization>(255U)), "<invalid>");
    EXPECT_EQ(sema::place_state_move_state_name(static_cast<sema::PlaceStateMoveState>(255U)), "<invalid>");
    EXPECT_EQ(sema::place_state_drop_state_name(static_cast<sema::PlaceStateDropState>(255U)), "<invalid>");
    EXPECT_EQ(sema::place_state_event_kind_name(static_cast<sema::PlaceStateEventKind>(255U)), "<invalid>");
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
