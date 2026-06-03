#pragma once

#include <aurex/infrastructure/base/result.hpp>
#include <aurex/midend/ir/ir.hpp>

namespace aurex::ir {

[[nodiscard]] base::Result<void> verify_module(const Module& module);

} // namespace aurex::ir
