#include "aurex/codegen_c/expr_order.hpp"

namespace aurex::codegen_c {

bool expr_contains_call(const syntax::AstModule& module, const syntax::ExprId expr_id) noexcept {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module.exprs.size()) {
        return false;
    }

    const syntax::ExprNode& expr = module.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::call:
        return true;
    case syntax::ExprKind::unary:
        return expr_contains_call(module, expr.unary_operand);
    case syntax::ExprKind::binary:
        return expr_contains_call(module, expr.binary_lhs) || expr_contains_call(module, expr.binary_rhs);
    case syntax::ExprKind::field:
        return expr_contains_call(module, expr.object);
    case syntax::ExprKind::index:
        return expr_contains_call(module, expr.object) || expr_contains_call(module, expr.index);
    case syntax::ExprKind::struct_literal:
        for (const syntax::FieldInit& init : expr.field_inits) {
            if (expr_contains_call(module, init.value)) {
                return true;
            }
        }
        return false;
    case syntax::ExprKind::cast:
    case syntax::ExprKind::ptr_cast:
    case syntax::ExprKind::bit_cast:
    case syntax::ExprKind::ptr_addr:
    case syntax::ExprKind::ptr_from_addr:
        return expr_contains_call(module, expr.cast_expr);
    case syntax::ExprKind::invalid:
    case syntax::ExprKind::integer_literal:
    case syntax::ExprKind::bool_literal:
    case syntax::ExprKind::null_literal:
    case syntax::ExprKind::string_literal:
    case syntax::ExprKind::c_string_literal:
    case syntax::ExprKind::byte_literal:
    case syntax::ExprKind::name:
    case syntax::ExprKind::size_of:
    case syntax::ExprKind::align_of:
        return false;
    }
    return false;
}

} // namespace aurex::codegen_c
