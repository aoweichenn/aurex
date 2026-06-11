#pragma once

#include <aurex/infrastructure/query/owned_dyn_runtime_lowering_abi_gate.hpp>
#include <aurex/midend/ir/ir.hpp>

namespace aurex::ir {

[[nodiscard]] query::OwnedDynRuntimeLoweringAbiGate owned_dyn_runtime_lowering_abi_gate(
    const Module& module);

} // namespace aurex::ir
