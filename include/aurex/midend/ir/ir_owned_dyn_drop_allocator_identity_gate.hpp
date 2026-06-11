#pragma once

#include <aurex/infrastructure/query/owned_dyn_drop_allocator_identity_gate.hpp>
#include <aurex/midend/ir/ir.hpp>

namespace aurex::ir {

[[nodiscard]] query::OwnedDynDropAllocatorIdentityGate owned_dyn_drop_allocator_identity_gate(const Module& module);

} // namespace aurex::ir
