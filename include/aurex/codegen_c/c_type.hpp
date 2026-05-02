#pragma once

#include "aurex/sema/sema.hpp"
#include "aurex/syntax/ast.hpp"

#include <string>

namespace aurex::codegen_c {

[[nodiscard]] std::string c_primitive_name(syntax::PrimitiveTypeKind primitive);
[[nodiscard]] sema::TypeHandle type_handle_for_syntax_type(const sema::CheckedModule& checked, syntax::TypeId type) noexcept;
[[nodiscard]] std::string format_c_type(const sema::TypeTable& types, sema::TypeHandle type, std::string declarator = {});
[[nodiscard]] std::string format_c_type(
    const syntax::AstModule& module,
    const sema::CheckedModule& checked,
    syntax::TypeId type,
    std::string declarator = {}
);

} // namespace aurex::codegen_c
