#pragma once

#include <aurex/frontend/macro/early_item_expansion.hpp>

#include <support/frontend_macro_test_support.hpp>
#include <support/frontend_test_support.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::test::early_item_expansion_support {

inline constexpr base::u8 EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION = 243U;
inline constexpr base::u8 EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE = 242U;
inline constexpr base::u32 EARLY_ITEM_EXPANSION_TEST_GENERATED_PART_INDEX_OFFSET = 100'000U;
inline constexpr base::u64 EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT = 2U;

using frontend_macro_support::assign_single_module_ownership;
using frontend_macro_support::expand_source;
using frontend_macro_support::find_item;
using frontend_macro_support::mutated_expansion_result;
using frontend_macro_support::parse_success;
using frontend_macro_support::part_key_table;
using frontend_macro_support::refresh_expansion_result;
using frontend_macro_support::single_part_key_table;

[[nodiscard]] inline const frontend::macro::EarlyItemMacroInput* input_by_attribute(
    const frontend::macro::EarlyItemExpansionResult& result,
    const std::string_view attribute_name) noexcept
{
    const auto found = std::find_if(result.inputs.begin(), result.inputs.end(),
        [attribute_name](const frontend::macro::EarlyItemMacroInput& input) {
            return input.attribute_name == attribute_name;
        });
    return found == result.inputs.end() ? nullptr : &*found;
}

} // namespace aurex::test::early_item_expansion_support
