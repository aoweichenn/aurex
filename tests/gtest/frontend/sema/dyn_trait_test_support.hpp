#pragma once

#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace aurex::test {

[[nodiscard]] inline syntax::AstModule parse_dyn_trait_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer({191}, source, diagnostics);
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

[[nodiscard]] inline sema::CheckedModule analyze_dyn_trait_source(const std::string_view source)
{
    syntax::AstModule module = parse_dyn_trait_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
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

[[nodiscard]] inline bool dyn_trait_diagnostics_contain(
    const base::DiagnosticSink& diagnostics, const std::string_view message)
{
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        if (diagnostic.message.find(message) != std::string::npos) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline std::string analyze_dyn_trait_source_failure(const std::string_view source)
{
    syntax::AstModule module = parse_dyn_trait_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();

    std::string output;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        output += diagnostic.message;
        output += '\n';
    }
    if (result) {
        ADD_FAILURE() << "expected semantic analysis to fail";
        return output;
    }
    output += result.error().message;
    output += '\n';
    return output;
}

inline void expect_dyn_trait_source_diagnostic(
    const std::string_view source, const std::string_view diagnostic)
{
    const std::string output = analyze_dyn_trait_source_failure(source);
    EXPECT_NE(output.find(diagnostic), std::string::npos) << "missing diagnostic: " << diagnostic << "\n" << output;
}

[[nodiscard]] inline std::optional<sema::FunctionLookupKey> find_dyn_trait_function(
    const sema::CheckedModule& checked, const std::string_view name)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.name.view() == name) {
            return entry.first;
        }
    }
    return std::nullopt;
}

inline void expect_trait_object_authority_matches_checked(
    const sema::CheckedModule& checked, const std::string_view function_name)
{
    const std::optional<sema::FunctionLookupKey> key = find_dyn_trait_function(checked, function_name);
    ASSERT_TRUE(key.has_value());
    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("dyn.checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("dyn.body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("dyn.signature"));
    sema::populate_type_check_body_borrow_authority(authority, checked, *key);

    EXPECT_TRUE(authority.has_trait_object_facts);
    EXPECT_EQ(authority.trait_object_method_slot_count, checked.trait_object_method_slots.size());
    EXPECT_EQ(authority.trait_object_callability_count, checked.trait_object_callability.size());
    EXPECT_EQ(authority.vtable_layout_count, checked.vtable_layouts.size());
    EXPECT_EQ(authority.trait_object_coercion_count, checked.trait_object_coercions.size());
    EXPECT_EQ(authority.trait_supertrait_edge_count, checked.trait_supertrait_edges.size());
    EXPECT_EQ(authority.trait_object_upcast_coercion_count, checked.trait_object_upcast_coercions.size());
    EXPECT_EQ(authority.principal_set_composition_count,
        checked.principal_set_composition_facts.summary.principal_set_count);
    EXPECT_EQ(authority.principal_set_composition_principal_count,
        checked.principal_set_composition_facts.summary.principal_count);
    EXPECT_EQ(authority.principal_set_composition_projection_count,
        checked.principal_set_composition_facts.summary.projection_count);
    EXPECT_EQ(authority.trait_object_fingerprint, sema::trait_object_facts_fingerprint(checked));
    EXPECT_NE(authority.trait_object_fingerprint.byte_count, 0U);
    if (authority.principal_set_composition_count != 0) {
        EXPECT_TRUE(authority.has_principal_set_composition_facts);
        EXPECT_EQ(authority.principal_set_composition_fingerprint,
            query::principal_set_composition_facts_fingerprint(checked.principal_set_composition_facts));
        EXPECT_NE(authority.principal_set_composition_fingerprint.byte_count, 0U);
    }
}

} // namespace aurex::test
