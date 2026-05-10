#pragma once

#include <aurex/ir/ir.hpp>

#include <string>

namespace aurex::ir {

[[nodiscard]] std::string dump_module(const Module& module);

} // namespace aurex::ir
