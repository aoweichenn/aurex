#pragma once

#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/infrastructure/query/dyn_abi_facts.hpp>

namespace aurex::sema {

[[nodiscard]] query::FunctionDynAbiFacts checked_dyn_abi_facts(const CheckedModule& checked);

} // namespace aurex::sema
