#pragma once

#include <aurex/frontend/sema/type.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>

namespace aurex::sema {

struct ArrayRepeatRuntimeSemantics {
    bool has_repeat_value = false;
    bool count_known = false;
    bool value_is_evaluated = false;
    base::u64 count = 0;
};

[[nodiscard]] inline ArrayRepeatRuntimeSemantics array_repeat_runtime_semantics(
    const syntax::AstModule& module, const TypeTable& types, const TypeHandle array_expr_type,
    const syntax::ExprId array_expr) noexcept
{
    ArrayRepeatRuntimeSemantics semantics;
    if (!syntax::is_valid(array_expr) || array_expr.value >= module.exprs.size()) {
        return semantics;
    }
    const syntax::ArrayExprPayload* const payload = module.exprs.array_payload(array_expr.value);
    if (payload == nullptr || !syntax::is_valid(payload->repeat_value)) {
        return semantics;
    }
    semantics.has_repeat_value = true;
    semantics.value_is_evaluated = true;

    if (!is_valid(array_expr_type) || array_expr_type.value >= types.size()) {
        return semantics;
    }
    const TypeInfo& array = types.get(array_expr_type);
    if (array.kind != TypeKind::array) {
        return semantics;
    }

    semantics.count_known = true;
    semantics.count = array.array_count;
    semantics.value_is_evaluated = array.array_count != 0;
    return semantics;
}

[[nodiscard]] inline bool array_repeat_value_should_be_visited(
    const ArrayRepeatRuntimeSemantics& semantics) noexcept
{
    return semantics.has_repeat_value && (!semantics.count_known || semantics.value_is_evaluated);
}

} // namespace aurex::sema
