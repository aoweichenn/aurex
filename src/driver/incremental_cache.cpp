#include <aurex/driver/incremental_cache.hpp>

#include "incremental_cache/query.hpp"

namespace aurex::driver {

base::Result<bool> try_reuse_incremental_check_cache(
    const CompilerInvocation& invocation, CompilationProfiler* const profiler)
{
    return incremental_cache_detail::try_reuse_incremental_check_cache_impl(invocation, profiler);
}

base::Result<void> write_incremental_cache(const CompilerInvocation& invocation, const base::SourceManager& sources,
    const std::span<const ModuleRecord> modules, const syntax::AstModule& ast, const sema::CheckedModule& checked,
    CompilationProfiler* const profiler)
{
    return incremental_cache_detail::write_incremental_cache_impl(invocation, sources, modules, ast, checked, profiler);
}

base::Result<void> write_incremental_cache(const CompilerInvocation& invocation, const base::SourceManager& sources,
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked,
    CompilationProfiler* const profiler)
{
    const syntax::AstModule empty_ast;
    return write_incremental_cache(invocation, sources, modules, empty_ast, checked, profiler);
}

} // namespace aurex::driver
