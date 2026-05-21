#pragma once

#include <aurex/base/result.hpp>
#include <aurex/base/source.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/syntax/ast.hpp>

#include <span>

namespace aurex::driver {

class CompilationProfiler;

namespace incremental_cache_detail {

[[nodiscard]] base::Result<bool> try_reuse_incremental_check_cache_impl(
    const CompilerInvocation& invocation, CompilationProfiler* profiler);

[[nodiscard]] base::Result<void> write_incremental_cache_impl(const CompilerInvocation& invocation,
    const base::SourceManager& sources, std::span<const ModuleRecord> modules, const syntax::AstModule& ast,
    const sema::CheckedModule& checked, CompilationProfiler* profiler);

} // namespace incremental_cache_detail

} // namespace aurex::driver
