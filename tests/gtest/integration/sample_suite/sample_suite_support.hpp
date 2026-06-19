#pragma once

#include <aurex/application/driver/invocation.hpp>

#include <support/test_support.hpp>

#include <string_view>

namespace aurex::test {

[[nodiscard]] driver::CompilerInvocation sample_invocation(const fs::path& src, driver::EmitKind emit_kind);
[[nodiscard]] bool is_native_emit_kind(driver::EmitKind emit_kind) noexcept;
[[nodiscard]] bool sample_uses_import_path(const fs::path& src);

void add_sample_import_path(driver::CompilerInvocation& invocation);
void configure_sample_imports_if_needed(driver::CompilerInvocation& invocation);
void compile_sample_native(
    const fs::path& src, const fs::path& output, driver::EmitKind emit_kind, bool use_sample_imports);

void verify_positive_samples_llvm_ir();
void verify_const_enum_lowering();
void verify_generic_builtin_ir();
void run_positive_runtime_smoke_sample(std::string_view area, std::string_view filename);
void verify_import_runtime_samples();

} // namespace aurex::test
