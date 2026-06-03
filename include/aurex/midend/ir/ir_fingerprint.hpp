#pragma once

#include <aurex/infrastructure/query/query_result.hpp>
#include <aurex/midend/ir/ir.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::ir {

struct FunctionIRUnitFingerprint {
    std::string symbol;
    query::QueryResultFingerprint target_independent_ir;
    query::QueryResultFingerprint llvm_emission_unit;
};

[[nodiscard]] query::QueryResultFingerprint layout_abi_fingerprint(const Module& module);
[[nodiscard]] query::QueryResultFingerprint function_ir_unit_fingerprint(
    const Module& module, const Function& function);
[[nodiscard]] query::QueryResultFingerprint llvm_emission_unit_fingerprint(
    const Module& module, const Function& function);
[[nodiscard]] std::vector<FunctionIRUnitFingerprint> function_ir_unit_fingerprints(const Module& module);
[[nodiscard]] std::optional<FunctionIRUnitFingerprint> function_ir_unit_fingerprint_by_symbol(
    const Module& module, std::string_view symbol);

} // namespace aurex::ir
