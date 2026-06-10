#pragma once

#include <aurex/infrastructure/query/owned_dyn_ir_shape_prototype_gate.hpp>
#include <aurex/midend/ir/ir.hpp>

namespace aurex::ir {

[[nodiscard]] query::OwnedDynIrShapePrototypeGate owned_dyn_ir_shape_prototype_gate(
    const Module& module);

} // namespace aurex::ir
