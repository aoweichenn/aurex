#pragma once

#include <aurex/midend/ir/ir.hpp>

#include <vector>

namespace aurex::ir {

[[nodiscard]] std::vector<ValueId> collect_function_value_closure(const Module& module, const Function& function);

} // namespace aurex::ir
