#pragma once

#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test::frontend_macro_support {

inline constexpr base::SourceId FRONTEND_MACRO_TEST_SOURCE_ID{31U};

[[nodiscard]] inline syntax::AstModule parse_success(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(FRONTEND_MACRO_TEST_SOURCE_ID, source, diagnostics);
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

[[nodiscard]] inline const syntax::ItemNode* find_item(
    const syntax::AstModule& module,
    const std::string_view name) noexcept
{
    for (base::usize index = 0; index < module.items.size(); ++index) {
        const syntax::ItemNode* const item = module.items.ptr(index);
        if (item != nullptr && item->name == name) {
            return item;
        }
    }
    return nullptr;
}

inline void assign_single_module_ownership(syntax::AstModule& module)
{
    if (module.modules.empty()) {
        syntax::ModuleInfo module_info;
        module_info.path = module.module_path;
        module.modules.push_back(std::move(module_info));
    }
    module.item_modules.assign(module.items.size(), syntax::ModuleId{0U});
    module.item_part_indices.assign(module.items.size(), 0U);
}

[[nodiscard]] inline query::PackageKey package_key()
{
    const std::array<std::string_view, 1U> identity{"early-item-expansion-test-package"};
    return query::package_key(identity);
}

[[nodiscard]] inline query::ModuleKey module_key(const query::PackageKey package)
{
    const std::array<std::string_view, 2U> path{"macro", "early_item_expansion"};
    return query::module_key(package, path);
}

[[nodiscard]] inline query::ModulePartKey module_part_key_for_index(const base::u32 part_index)
{
    const query::PackageKey package = package_key();
    const query::ModuleKey module = module_key(package);
    if (part_index == 0U) {
        const query::FileKey file =
            query::file_key(package, "/virtual/tests/macro/early_item_expansion.ax");
        return query::module_part_key(
            module, file, query::ModulePartKind::primary, "<primary>");
    }
    const std::string part_name = "part" + std::to_string(part_index);
    const std::string source_path =
        "/virtual/tests/macro/early_item_expansion/" + part_name + ".ax";
    const query::FileKey file = query::file_key(package, source_path);
    return query::module_part_key(
        module, file, query::ModulePartKind::fragment, part_name, part_index);
}

[[nodiscard]] inline query::ModulePartKey primary_part_key()
{
    return module_part_key_for_index(0U);
}

[[nodiscard]] inline std::vector<std::vector<query::ModulePartKey>> part_key_table(
    const base::u32 part_count)
{
    std::vector<std::vector<query::ModulePartKey>> keys(1U);
    keys.front().reserve(part_count);
    for (base::u32 part_index = 0U; part_index < part_count; ++part_index) {
        keys.front().push_back(module_part_key_for_index(part_index));
    }
    return keys;
}

[[nodiscard]] inline std::vector<std::vector<query::ModulePartKey>> single_part_key_table()
{
    return {{primary_part_key()}};
}

[[nodiscard]] inline frontend::macro::EarlyItemExpansionResult expand_source(
    const std::string_view source)
{
    syntax::AstModule module = parse_success(source);
    assign_single_module_ownership(module);
    std::vector<std::vector<query::ModulePartKey>> part_keys = single_part_key_table();
    auto expanded = frontend::macro::expand_early_item_macros_noop(module, part_keys);
    if (!expanded) {
        ADD_FAILURE() << expanded.error().message;
        return {};
    }
    return expanded.take_value();
}

inline void refresh_expansion_result(frontend::macro::EarlyItemExpansionResult& result)
{
    result.summary = frontend::macro::summarize_early_item_expansion_counts(result);
    result.fingerprint = frontend::macro::early_item_expansion_fingerprint(result);
}

template <typename Mutator>
[[nodiscard]] frontend::macro::EarlyItemExpansionResult mutated_expansion_result(
    const frontend::macro::EarlyItemExpansionResult& baseline,
    Mutator&& mutator)
{
    frontend::macro::EarlyItemExpansionResult result = baseline;
    std::forward<Mutator>(mutator)(result);
    refresh_expansion_result(result);
    return result;
}

} // namespace aurex::test::frontend_macro_support
