#pragma once

#include <aurex/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts.hpp>
#include <aurex/midend/ir/ir.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace aurex::ir {

[[nodiscard]] query::FunctionDynOwnershipRuntimeIrVerifierFacts
function_dyn_ownership_runtime_ir_verifier_facts(const Module& module, const Function& function);
[[nodiscard]] std::vector<query::FunctionDynOwnershipRuntimeIrVerifierFacts>
function_dyn_ownership_runtime_ir_verifier_facts(const Module& module);
[[nodiscard]] std::optional<query::FunctionDynOwnershipRuntimeIrVerifierFacts>
function_dyn_ownership_runtime_ir_verifier_facts_by_symbol(
    const Module& module,
    std::string_view symbol);

} // namespace aurex::ir
