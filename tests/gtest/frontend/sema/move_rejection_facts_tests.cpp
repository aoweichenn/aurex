#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::SourceId MOVE_REJECTION_TEST_SOURCE_ID{919};
constexpr base::u32 MOVE_REJECTION_TEST_INVALID_KIND_VALUE = 99;
constexpr std::string_view MOVE_REJECTION_TEST_SOURCE =
    "module move_rejection_facts;\n"
    "struct File {\n"
    "  fd: i32;\n"
    "}\n"
    "impl Drop for File {\n"
    "  fn drop(self: deinit File) -> void {}\n"
    "}\n"
    "enum OptionFile {\n"
    "  some(File),\n"
    "  none,\n"
    "}\n"
    "struct BoxFile {\n"
    "  value: File;\n"
    "}\n"
    "enum ResultFile {\n"
    "  ok(File),\n"
    "  err(i32),\n"
    "}\n"
    "fn unwrap_match(value: OptionFile, fallback: File) -> File {\n"
    "  return match value {\n"
    "    .some(inner) => inner,\n"
    "    .none => fallback,\n"
    "  };\n"
    "}\n"
    "fn unwrap_box(box: BoxFile) -> File {\n"
    "  let BoxFile { value } = box;\n"
    "  return value;\n"
    "}\n"
    "fn unwrap_if(value: OptionFile, fallback: File) -> File {\n"
    "  if value is .some(inner) {\n"
    "    return inner;\n"
    "  }\n"
    "  return fallback;\n"
    "}\n"
    "fn unwrap_if_expr(value: OptionFile, fallback: File) -> File {\n"
    "  return if value is .some(inner) {\n"
    "    inner\n"
    "  } else {\n"
    "    fallback\n"
    "  };\n"
    "}\n"
    "fn unwrap_while(value: OptionFile, fallback: File) -> File {\n"
    "  while value is .some(inner) {\n"
    "    return inner;\n"
    "  }\n"
    "  return fallback;\n"
    "}\n"
    "fn forward(value: ResultFile) -> ResultFile {\n"
    "  let inner: File = value?;\n"
    "  return ResultFile.ok(inner);\n"
    "}\n"
    "fn first(values: []File) -> File {\n"
    "  return values[0];\n"
    "}\n"
    "fn main() -> i32 {\n"
    "  return 0;\n"
    "}\n";

struct MoveRejectionAnalysisSnapshot {
    sema::CheckedModule checked;
    std::string diagnostics;
};

[[nodiscard]] syntax::AstModule parse_move_rejection_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(MOVE_REJECTION_TEST_SOURCE_ID, source, diagnostics);
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

[[nodiscard]] MoveRejectionAnalysisSnapshot analyze_move_rejection_source(const std::string_view source)
{
    syntax::AstModule module = parse_move_rejection_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(std::move(module), diagnostics);
    const auto result = analyzer.analyze();
    EXPECT_FALSE(result);

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages += '\n';
    }
    return MoveRejectionAnalysisSnapshot{analyzer.state_.checked, std::move(messages)};
}

[[nodiscard]] std::optional<sema::FunctionLookupKey> find_move_rejection_function(
    const sema::CheckedModule& checked, const std::string_view name)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.name.view() == name) {
            return entry.first;
        }
    }
    return std::nullopt;
}

[[nodiscard]] const sema::FunctionMoveRejectionFacts* find_move_rejection_facts(
    const sema::CheckedModule& checked, const std::string_view name)
{
    const std::optional<sema::FunctionLookupKey> key = find_move_rejection_function(checked, name);
    if (!key.has_value()) {
        return nullptr;
    }
    const auto facts = checked.move_rejection_facts.find(*key);
    return facts == checked.move_rejection_facts.end() ? nullptr : &facts->second;
}

[[nodiscard]] base::usize count_rejections(
    const sema::FunctionMoveRejectionFacts& facts, const sema::MoveRejectionKind kind)
{
    return static_cast<base::usize>(std::ranges::count_if(facts.rejections, [kind](
        const sema::MoveRejectionFact& fact) {
        return fact.kind == kind;
    }));
}

void expect_all_rejections_have_emitted_diagnostics(const sema::FunctionMoveRejectionFacts& facts)
{
    EXPECT_TRUE(std::ranges::all_of(facts.rejections, [](const sema::MoveRejectionFact& rejection) {
        return rejection.diagnostic_emitted && sema::is_valid(rejection.tracked_type)
            && rejection.resource_fingerprint.byte_count != 0U;
    }));
}

[[nodiscard]] sema::MoveRejectionFact make_synthetic_move_rejection(
    const sema::MoveRejectionKind kind, const bool diagnostic_emitted, const base::u32 id)
{
    return sema::MoveRejectionFact{
        .kind = kind,
        .expr = syntax::ExprId{id},
        .stmt = syntax::StmtId{id + 1U},
        .pattern = syntax::PatternId{id + 2U},
        .tracked_type = sema::TypeHandle{id + 3U},
        .resource_fingerprint = query::stable_fingerprint(std::string_view{"synthetic.move.rejection"}),
        .diagnostic_emitted = diagnostic_emitted,
        .range = base::SourceRange{MOVE_REJECTION_TEST_SOURCE_ID, id, id + 1U},
    };
}

} // namespace

TEST(CoreUnit, MoveRejectionFactsRecordReachableUnsupportedPayloadAndIndexRejections)
{
    const MoveRejectionAnalysisSnapshot snapshot = analyze_move_rejection_source(MOVE_REJECTION_TEST_SOURCE);

    EXPECT_NE(snapshot.diagnostics.find(sema::SEMA_MOVE_PATTERN_PAYLOAD_UNSUPPORTED), std::string::npos)
        << snapshot.diagnostics;
    EXPECT_NE(snapshot.diagnostics.find(sema::SEMA_MOVE_TRY_PAYLOAD_UNSUPPORTED), std::string::npos)
        << snapshot.diagnostics;
    EXPECT_NE(snapshot.diagnostics.find(sema::SEMA_MOVE_INDEXED_ELEMENT_UNSUPPORTED), std::string::npos)
        << snapshot.diagnostics;

    const sema::FunctionMoveRejectionFacts* const match_facts =
        find_move_rejection_facts(snapshot.checked, "unwrap_match");
    ASSERT_NE(match_facts, nullptr);
    EXPECT_EQ(count_rejections(*match_facts, sema::MoveRejectionKind::pattern_payload), 1U);
    EXPECT_TRUE(syntax::is_valid(match_facts->rejections.front().expr));
    EXPECT_TRUE(syntax::is_valid(match_facts->rejections.front().pattern));
    expect_all_rejections_have_emitted_diagnostics(*match_facts);

    const sema::FunctionMoveRejectionFacts* const let_facts =
        find_move_rejection_facts(snapshot.checked, "unwrap_box");
    ASSERT_NE(let_facts, nullptr);
    EXPECT_EQ(count_rejections(*let_facts, sema::MoveRejectionKind::pattern_payload), 1U);
    EXPECT_TRUE(syntax::is_valid(let_facts->rejections.front().stmt));
    EXPECT_TRUE(syntax::is_valid(let_facts->rejections.front().pattern));
    expect_all_rejections_have_emitted_diagnostics(*let_facts);

    for (const std::string_view function : {"unwrap_if", "unwrap_if_expr", "unwrap_while"}) {
        const sema::FunctionMoveRejectionFacts* const facts = find_move_rejection_facts(snapshot.checked, function);
        ASSERT_NE(facts, nullptr) << function;
        EXPECT_EQ(count_rejections(*facts, sema::MoveRejectionKind::pattern_payload), 1U) << function;
        EXPECT_TRUE(syntax::is_valid(facts->rejections.front().pattern)) << function;
        expect_all_rejections_have_emitted_diagnostics(*facts);
    }

    const sema::FunctionMoveRejectionFacts* const try_facts =
        find_move_rejection_facts(snapshot.checked, "forward");
    ASSERT_NE(try_facts, nullptr);
    EXPECT_EQ(count_rejections(*try_facts, sema::MoveRejectionKind::try_payload), 1U);
    EXPECT_TRUE(syntax::is_valid(try_facts->rejections.front().expr));
    EXPECT_FALSE(syntax::is_valid(try_facts->rejections.front().pattern));
    expect_all_rejections_have_emitted_diagnostics(*try_facts);

    const sema::FunctionMoveRejectionFacts* const index_facts =
        find_move_rejection_facts(snapshot.checked, "first");
    ASSERT_NE(index_facts, nullptr);
    EXPECT_EQ(count_rejections(*index_facts, sema::MoveRejectionKind::indexed_element), 1U);
    EXPECT_TRUE(syntax::is_valid(index_facts->rejections.front().expr));
    EXPECT_FALSE(syntax::is_valid(index_facts->rejections.front().pattern));
    expect_all_rejections_have_emitted_diagnostics(*index_facts);

    EXPECT_EQ(find_move_rejection_facts(snapshot.checked, "main"), nullptr);
}

TEST(CoreUnit, MoveRejectionFactsDumpCopyFingerprintAndAuthorityAreStable)
{
    const MoveRejectionAnalysisSnapshot snapshot = analyze_move_rejection_source(MOVE_REJECTION_TEST_SOURCE);
    const std::optional<sema::FunctionLookupKey> key = find_move_rejection_function(snapshot.checked, "forward");
    ASSERT_TRUE(key.has_value());
    const sema::FunctionMoveRejectionFacts* const facts = find_move_rejection_facts(snapshot.checked, "forward");
    ASSERT_NE(facts, nullptr);

    EXPECT_EQ(sema::move_rejection_kind_name(sema::MoveRejectionKind::indexed_element), "indexed_element");
    EXPECT_EQ(sema::move_rejection_kind_name(sema::MoveRejectionKind::pattern_payload), "pattern_payload");
    EXPECT_EQ(sema::move_rejection_kind_name(sema::MoveRejectionKind::try_payload), "try_payload");

    const query::StableFingerprint128 baseline = sema::function_move_rejection_facts_fingerprint(*facts);
    EXPECT_EQ(facts->fingerprint, baseline);

    sema::FunctionMoveRejectionFacts range_changed = *facts;
    range_changed.rejections.front().range.begin += 1U;
    EXPECT_EQ(sema::function_move_rejection_facts_fingerprint(range_changed), baseline);

    sema::FunctionMoveRejectionFacts kind_changed = *facts;
    kind_changed.rejections.front().kind = sema::MoveRejectionKind::pattern_payload;
    EXPECT_NE(sema::function_move_rejection_facts_fingerprint(kind_changed), baseline);

    sema::FunctionMoveRejectionFacts emitted_changed = *facts;
    emitted_changed.rejections.front().diagnostic_emitted = false;
    EXPECT_NE(sema::function_move_rejection_facts_fingerprint(emitted_changed), baseline);

    const std::string fact_dump = sema::dump_function_move_rejection_facts(*facts);
    EXPECT_NE(fact_dump.find("move_rejection_facts function="), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("try_payload"), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("emitted=true"), std::string::npos) << fact_dump;
    EXPECT_NE(fact_dump.find("fingerprint="), std::string::npos) << fact_dump;

    sema::FunctionMoveRejectionFacts summary_facts = *facts;
    sema::MoveRejectionFact pattern_rejection = summary_facts.rejections.front();
    pattern_rejection.kind = sema::MoveRejectionKind::pattern_payload;
    pattern_rejection.diagnostic_emitted = false;
    sema::MoveRejectionFact indexed_rejection = summary_facts.rejections.front();
    indexed_rejection.kind = sema::MoveRejectionKind::indexed_element;
    summary_facts.rejections.push_back(pattern_rejection);
    summary_facts.rejections.push_back(indexed_rejection);
    const std::string summary = sema::summarize_function_move_rejection_facts(summary_facts);
    EXPECT_NE(summary.find("move_rejection_facts rejections=3"), std::string::npos) << summary;
    EXPECT_NE(summary.find("pattern_payload=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("try_payload=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("indexed_element=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("diagnostics=2"), std::string::npos) << summary;

    sema::CheckedModule copied = snapshot.checked;
    const sema::FunctionMoveRejectionFacts* const copied_facts = find_move_rejection_facts(copied, "forward");
    ASSERT_NE(copied_facts, nullptr);
    EXPECT_EQ(copied_facts->fingerprint, baseline);

    const std::string checked_dump = sema::dump_checked_module(copied);
    EXPECT_NE(checked_dump.find("move_rejection_facts"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("move_rejection_fact"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("try_payload"), std::string::npos) << checked_dump;

    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("move.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("move.body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("move.signature"));
    sema::populate_type_check_body_borrow_authority(authority, copied, *key);

    EXPECT_TRUE(authority.has_move_rejection_facts);
    EXPECT_EQ(authority.move_rejection_count, 1U);
    EXPECT_EQ(authority.move_rejection_try_payload_count, 1U);
    EXPECT_EQ(authority.move_rejection_pattern_payload_count, 0U);
    EXPECT_EQ(authority.move_rejection_indexed_element_count, 0U);
    EXPECT_TRUE(authority.move_rejection_has_try_payload);
    EXPECT_FALSE(authority.move_rejection_has_pattern_payload);
    EXPECT_FALSE(authority.move_rejection_has_indexed_element);
    EXPECT_TRUE(authority.move_rejection_has_emitted_diagnostics);
    EXPECT_EQ(authority.move_rejection_fingerprint, baseline);
    EXPECT_TRUE(query::is_valid(query::type_check_body_result_fingerprint(authority)));
}

TEST(CoreUnit, MoveRejectionFactsWhiteboxCoverInvalidAndMixedAuthorityProjection)
{
    EXPECT_EQ(sema::move_rejection_kind_name(
                  static_cast<sema::MoveRejectionKind>(MOVE_REJECTION_TEST_INVALID_KIND_VALUE)),
        "<invalid>");

    sema::FunctionLookupKey function;
    function.module = 1U;
    function.owner_type = 2U;
    function.name = syntax::IdentId{3U};

    sema::FunctionMoveRejectionFacts facts;
    facts.function = function;
    facts.part_index = 4U;
    facts.rejections = std::vector<sema::MoveRejectionFact>{
        make_synthetic_move_rejection(sema::MoveRejectionKind::indexed_element, false, 10U),
        make_synthetic_move_rejection(sema::MoveRejectionKind::pattern_payload, true, 20U),
        make_synthetic_move_rejection(sema::MoveRejectionKind::try_payload, true, 30U),
    };
    facts.fingerprint = sema::function_move_rejection_facts_fingerprint(facts);

    const std::string mixed_dump = sema::dump_function_move_rejection_facts(facts);
    EXPECT_NE(mixed_dump.find("indexed_element"), std::string::npos) << mixed_dump;
    EXPECT_NE(mixed_dump.find("pattern_payload"), std::string::npos) << mixed_dump;
    EXPECT_NE(mixed_dump.find("try_payload"), std::string::npos) << mixed_dump;
    EXPECT_NE(mixed_dump.find("emitted=false"), std::string::npos) << mixed_dump;

    sema::FunctionMoveRejectionFacts unnamed_facts = facts;
    unnamed_facts.function.name = syntax::INVALID_IDENT_ID;
    const std::string unnamed_dump = sema::dump_function_move_rejection_facts(unnamed_facts);
    EXPECT_NE(unnamed_dump.find("function=1:2:-"), std::string::npos) << unnamed_dump;

    sema::CheckedModule checked;
    checked.move_rejection_facts.emplace(function, facts);
    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("move_rejection_fact 1:2:#3"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("emitted=false"), std::string::npos) << checked_dump;

    query::TypeCheckBodyAuthority authority;
    sema::populate_type_check_body_borrow_authority(authority, checked, function);
    EXPECT_TRUE(authority.has_move_rejection_facts);
    EXPECT_EQ(authority.move_rejection_count, 3U);
    EXPECT_EQ(authority.move_rejection_indexed_element_count, 1U);
    EXPECT_EQ(authority.move_rejection_pattern_payload_count, 1U);
    EXPECT_EQ(authority.move_rejection_try_payload_count, 1U);
    EXPECT_TRUE(authority.move_rejection_has_indexed_element);
    EXPECT_TRUE(authority.move_rejection_has_pattern_payload);
    EXPECT_TRUE(authority.move_rejection_has_try_payload);
    EXPECT_TRUE(authority.move_rejection_has_emitted_diagnostics);
    EXPECT_EQ(authority.move_rejection_fingerprint, facts.fingerprint);
}

} // namespace aurex::test
