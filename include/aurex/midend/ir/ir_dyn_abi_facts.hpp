#pragma once

#include <aurex/infrastructure/query/dyn_abi_facts.hpp>
#include <aurex/midend/ir/ir.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace aurex::ir {

[[nodiscard]] query::FunctionDynAbiFacts function_dyn_abi_facts(const Module& module, const Function& function);
[[nodiscard]] std::vector<query::FunctionDynAbiFacts> function_dyn_abi_facts(const Module& module);
[[nodiscard]] std::optional<query::FunctionDynAbiFacts> function_dyn_abi_facts_by_symbol(
    const Module& module, std::string_view symbol);

} // namespace aurex::ir
