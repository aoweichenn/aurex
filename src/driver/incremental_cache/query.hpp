#pragma once

#include <aurex/base/result.hpp>
#include <aurex/driver/module_record.hpp>

#include <span>

namespace aurex::base {
class SourceManager;
} // namespace aurex::base

namespace aurex::sema {
struct CheckedModule;
} // namespace aurex::sema

namespace aurex::syntax {
struct AstModule;
} // namespace aurex::syntax

namespace aurex::driver {

class CompilationProfiler;
struct CompilerInvocation;

namespace incremental_cache_detail {

[[nodiscard]] base::Result<bool> try_reuse_incremental_check_cache_impl(
    const CompilerInvocation& invocation, CompilationProfiler* profiler);

[[nodiscard]] base::Result<void> write_incremental_cache_impl(const CompilerInvocation& invocation,
    const base::SourceManager& sources, std::span<const ModuleRecord> modules, const syntax::AstModule& ast,
    const sema::CheckedModule& checked, CompilationProfiler* profiler);

} // namespace incremental_cache_detail

} // namespace aurex::driver
