#pragma once

#include <aurex/infrastructure/query/cleanup_marker_facts.hpp>
#include <aurex/midend/ir/ir.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace aurex::ir {

[[nodiscard]] query::CleanupMarkerPolicy query_cleanup_marker_policy(CleanupAbiPolicy policy) noexcept;
[[nodiscard]] query::FunctionCleanupMarkerFacts function_cleanup_marker_facts(
    const Module& module, const Function& function);
[[nodiscard]] std::vector<query::FunctionCleanupMarkerFacts> function_cleanup_marker_facts(const Module& module);
[[nodiscard]] std::optional<query::FunctionCleanupMarkerFacts> function_cleanup_marker_facts_by_symbol(
    const Module& module, std::string_view symbol);

} // namespace aurex::ir
