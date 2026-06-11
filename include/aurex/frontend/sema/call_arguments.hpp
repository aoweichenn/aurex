#pragma once

#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/syntax/ast/expr_nodes.hpp>

#include <span>

namespace aurex::sema {

template <typename Sequence, typename Payload>
[[nodiscard]] std::span<const syntax::ExprId> ordered_call_args_or_source(
    const Sequence& ordered_args, const Payload& payload) noexcept
{
    if (!ordered_args.empty() || payload.args.empty()) {
        return {ordered_args.data(), ordered_args.size()};
    }
    return {payload.args.data(), payload.args.size()};
}

[[nodiscard]] inline std::span<const syntax::ExprId> checked_ordered_call_args_or_source(
    const CheckedModule& checked, const syntax::ExprId call, const syntax::CallExprPayload& payload) noexcept
{
    if (const FunctionCallBinding* const binding = checked.function_call_binding_for_expr(call); binding != nullptr) {
        return ordered_call_args_or_source(binding->ordered_args, payload);
    }
    if (const TraitMethodCallBinding* const binding = checked.trait_method_call_binding_for_expr(call);
        binding != nullptr) {
        return ordered_call_args_or_source(binding->ordered_args, payload);
    }
    return {payload.args.data(), payload.args.size()};
}

} // namespace aurex::sema
