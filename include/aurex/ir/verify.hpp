#pragma once

#include "aurex/base/result.hpp"
#include "aurex/ir/ir.hpp"

namespace aurex::ir {

[[nodiscard]] base::Result<void> verify_module(const Module& module);

} // namespace aurex::ir
