#pragma once

#include <aurex/base/result.hpp>
#include <aurex/base/source.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/sema/checked_module.hpp>

#include <span>

namespace aurex::driver {

[[nodiscard]] base::Result<bool> try_reuse_incremental_check_cache(
    const CompilerInvocation& invocation
);

[[nodiscard]] base::Result<void> write_incremental_cache(
    const CompilerInvocation& invocation,
    const base::SourceManager& sources,
    std::span<const ModuleRecord> modules,
    const sema::CheckedModule& checked
);

} // namespace aurex::driver
