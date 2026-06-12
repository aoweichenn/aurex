#pragma once

#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/infrastructure/query/macro_expansion_facts.hpp>
#include <aurex/infrastructure/query/query_key.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::frontend::macro {

enum class EarlyItemExpansionDisposition : base::u8 {
    builtin_derive_passthrough = 1,
    blocked_unimplemented_attribute,
};

struct EarlyItemMacroInput {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    std::string attribute_name;
    base::SourceRange attribute_range{};
    base::SourceRange token_tree_range{};
    bool has_token_tree = false;
    base::u64 token_count = 0;
    query::ModulePartKey attached_part;
    query::StableFingerprint128 token_tree_fingerprint;
    query::StableFingerprint128 query_key_fingerprint;
    EarlyItemExpansionDisposition disposition =
        EarlyItemExpansionDisposition::blocked_unimplemented_attribute;
};

struct GeneratedModulePartPlaceholder {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    base::u32 generated_stable_index = 0;
    query::SourceRole source_role = query::SourceRole::generated;
    query::ModulePartKind part_kind = query::ModulePartKind::generated;
    query::ModulePartKey source_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 output_fingerprint;
    bool parsed = false;
    bool merged = false;
    bool produced_user_generated_code = false;
};

struct ExpansionSourceMapPlaceholder {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 attribute_index = 0;
    base::SourceRange attribute_range{};
    base::SourceRange token_tree_range{};
    query::StableFingerprint128 expansion_origin;
    bool real_source_map = false;
    bool debug_trace_available = false;
};

struct EarlyItemExpansionSummary {
    base::u64 macro_input_count = 0;
    base::u64 attribute_input_count = 0;
    base::u64 builtin_derive_passthrough_count = 0;
    base::u64 blocked_attribute_count = 0;
    base::u64 generated_part_placeholder_count = 0;
    base::u64 source_map_placeholder_count = 0;
    base::u64 parsed_generated_part_count = 0;
    base::u64 merged_generated_part_count = 0;
    base::u64 user_generated_code_count = 0;
    base::u64 standard_library_required_count = 0;
    base::u64 runtime_required_count = 0;
    base::u64 external_process_required_count = 0;
};

struct EarlyItemExpansionResult {
    std::string name;
    query::MacroExpansionPlan plan;
    std::vector<EarlyItemMacroInput> inputs;
    std::vector<GeneratedModulePartPlaceholder> generated_parts;
    std::vector<ExpansionSourceMapPlaceholder> source_maps;
    EarlyItemExpansionSummary summary;
    query::StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view early_item_expansion_disposition_name(
    EarlyItemExpansionDisposition disposition) noexcept;
[[nodiscard]] bool is_valid(EarlyItemExpansionDisposition disposition) noexcept;
[[nodiscard]] bool is_valid(const EarlyItemMacroInput& input) noexcept;
[[nodiscard]] bool is_valid(const GeneratedModulePartPlaceholder& placeholder) noexcept;
[[nodiscard]] bool is_valid(const ExpansionSourceMapPlaceholder& placeholder) noexcept;
[[nodiscard]] bool is_valid(const EarlyItemExpansionSummary& summary, const EarlyItemExpansionResult& result) noexcept;
[[nodiscard]] bool is_valid(const EarlyItemExpansionResult& result) noexcept;

[[nodiscard]] EarlyItemExpansionSummary summarize_early_item_expansion_counts(
    const EarlyItemExpansionResult& result) noexcept;
[[nodiscard]] query::StableFingerprint128 early_item_expansion_fingerprint(
    const EarlyItemExpansionResult& result) noexcept;
[[nodiscard]] std::string summarize_early_item_expansion(const EarlyItemExpansionResult& result);
[[nodiscard]] std::string dump_early_item_expansion(const EarlyItemExpansionResult& result);

[[nodiscard]] base::Result<EarlyItemExpansionResult> expand_early_item_macros_noop(
    const syntax::AstModule& ast,
    std::span<const std::vector<query::ModulePartKey>> module_part_keys,
    const query::MacroExpansionPlan& plan = query::m21c_macro_expansion_plan_baseline());

} // namespace aurex::frontend::macro
