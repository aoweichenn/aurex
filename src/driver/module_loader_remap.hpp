#pragma once

#include <aurex/syntax/ast.hpp>

namespace aurex::driver {

[[nodiscard]] bool ast_payloads_empty(const syntax::AstModule& module) noexcept;

void move_root_module_into_empty_combined(
    syntax::AstModule& combined, syntax::AstModule&& module, syntax::ModuleId owner_module);

void append_module_into(
    syntax::AstModule& destination, syntax::AstModule&& source, bool keep_imports, syntax::ModuleId owner_module);

} // namespace aurex::driver
