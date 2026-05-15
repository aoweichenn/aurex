#include <aurex/sema/sema.hpp>

#include <string>
#include <utility>

namespace aurex::sema {

namespace {

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module,
    const syntax::ExprId expr,
    const base::SourceRange fallback
) noexcept {
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return fallback;
    }
    return module.exprs.range(expr.value);
}

[[nodiscard]] base::SourceRange merge_ranges(
    const base::SourceRange begin,
    const base::SourceRange end
) noexcept {
    return base::SourceRange {begin.source, begin.begin, end.end};
}

[[nodiscard]] bool postfix_op_is_type_selector_continuation(const syntax::PostfixOp& op) noexcept {
    return op.kind == syntax::PostfixOpKind::select ||
           (op.kind == syntax::PostfixOpKind::bracket && !op.bracket_is_slice);
}

[[nodiscard]] syntax::TypeNode copy_named_type_selector(
    const syntax::TypeNode& source,
    const base::SourceRange range
) {
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::named;
    type.range = range;
    type.scope_name = source.scope_name;
    type.scope_range = source.scope_range;
    type.scope_parts = source.scope_parts;
    type.name = source.name;
    return type;
}

} // namespace

TypeHandle SemanticAnalyzer::analyze_postfix_chain_expr(
    const syntax::ExprId expr_id,
    const TypeHandle expected_type
) {
    const syntax::ExprId materialized = this->materialize_postfix_chain(expr_id);
    if (materialized.value != expr_id.value) {
        return this->analyze_expr(materialized, expected_type);
    }
    return this->analyze_expr(expr_id, this->expr_view(expr_id), expected_type);
}

syntax::ExprId SemanticAnalyzer::materialize_postfix_chain(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->module_.exprs.size()) {
        return syntax::INVALID_EXPR_ID;
    }

    if (this->module_.exprs.kind(expr_id.value) != syntax::ExprKind::postfix_chain) {
        return expr_id;
    }

    std::vector<syntax::ExprId> chains;
    syntax::ExprId current_chain = expr_id;
    while (syntax::is_valid(current_chain) &&
           current_chain.value < this->module_.exprs.size() &&
           this->module_.exprs.kind(current_chain.value) == syntax::ExprKind::postfix_chain) {
        chains.push_back(current_chain);
        const syntax::PostfixChainExprPayload* const payload =
            this->module_.exprs.postfix_chain_payload(current_chain.value);
        current_chain = payload == nullptr ? syntax::INVALID_EXPR_ID : payload->base;
    }

    syntax::ExprId current = current_chain;
    while (!chains.empty()) {
        const syntax::ExprId chain_id = chains.back();
        chains.pop_back();
        syntax::PostfixChainExprPayload chain = this->module_.exprs.take_postfix_chain_payload(chain_id.value);
        std::vector<syntax::PostfixOp> postfix_ops = std::move(chain.ops);
        if (postfix_ops.empty()) {
            continue;
        }
        for (base::usize index = 0; index < postfix_ops.size(); ++index) {
            const bool is_last = index + 1 == postfix_ops.size();
            const syntax::PostfixOp* const next_op = is_last ? nullptr : &postfix_ops[index + 1];
            current = this->materialize_postfix_op(
                chain_id,
                current,
                std::move(postfix_ops[index]),
                next_op,
                is_last
            );
        }
    }
    return current;
}

syntax::ExprId SemanticAnalyzer::materialize_postfix_op(
    const syntax::ExprId chain_expr,
    const syntax::ExprId base,
    syntax::PostfixOp&& op,
    const syntax::PostfixOp* next_op,
    const bool is_last
) {
    const base::SourceRange range = merge_ranges(expr_range_or(this->module_, base, op.range), op.range);
    switch (op.kind) {
    case syntax::PostfixOpKind::bracket:
        return this->materialize_postfix_bracket_op(chain_expr, base, std::move(op), next_op, is_last);
    case syntax::PostfixOpKind::select: {
        if (op.name.empty()) {
            if (is_last) {
                this->module_.set_invalid_expr(chain_expr.value, range);
                return chain_expr;
            }
            const syntax::ExprId id = this->module_.push_invalid_expr(range);
            this->ensure_expr_side_table_size(this->module_.exprs.size());
            return id;
        }
        syntax::FieldExprPayload payload;
        payload.object = base;
        payload.field_name = op.name;
        payload.field_name_id = op.name_id;
        if (is_last) {
            this->module_.set_field_expr(chain_expr.value, range, payload);
            return chain_expr;
        }
        const syntax::ExprId id = this->module_.push_field_expr(range, payload);
        this->ensure_expr_side_table_size(this->module_.exprs.size());
        return id;
    }
    case syntax::PostfixOpKind::call: {
        syntax::CallExprPayload payload;
        payload.callee = base;
        payload.args = std::move(op.args);
        if (is_last) {
            this->module_.set_call_expr(chain_expr.value, syntax::ExprKind::call, range, std::move(payload));
            return chain_expr;
        }
        const syntax::ExprId id =
            this->module_.push_call_expr(syntax::ExprKind::call, range, std::move(payload));
        this->ensure_expr_side_table_size(this->module_.exprs.size());
        return id;
    }
    case syntax::PostfixOpKind::try_: {
        const syntax::UnaryExprPayload payload {
            syntax::UnaryOp::logical_not,
            base,
        };
        if (is_last) {
            this->module_.set_unary_expr(chain_expr.value, syntax::ExprKind::try_expr, range, payload);
            return chain_expr;
        }
        const syntax::ExprId id =
            this->module_.push_unary_expr(syntax::ExprKind::try_expr, range, payload);
        this->ensure_expr_side_table_size(this->module_.exprs.size());
        return id;
    }
    case syntax::PostfixOpKind::struct_literal: {
        syntax::StructLiteralExprPayload payload;
        payload.object = base;
        payload.field_inits = std::move(op.field_inits);
        if (is_last) {
            this->module_.set_struct_literal_expr(chain_expr.value, range, std::move(payload));
            return chain_expr;
        }
        const syntax::ExprId id = this->module_.push_struct_literal_expr(range, std::move(payload));
        this->ensure_expr_side_table_size(this->module_.exprs.size());
        return id;
    }
    }
    return syntax::INVALID_EXPR_ID;
}

syntax::ExprId SemanticAnalyzer::materialize_postfix_bracket_op(
    const syntax::ExprId chain_expr,
    const syntax::ExprId base,
    syntax::PostfixOp&& op,
    const syntax::PostfixOp* next_op,
    const bool is_last
) {
    const base::SourceRange range = merge_ranges(expr_range_or(this->module_, base, op.range), op.range);

    if (op.bracket_is_slice) {
        const syntax::SliceExprPayload payload {
            base,
            op.slice_start,
            op.slice_end,
        };
        if (is_last) {
            this->module_.set_slice_expr(chain_expr.value, range, payload);
            return chain_expr;
        }
        const syntax::ExprId id = this->module_.push_slice_expr(range, payload);
        this->ensure_expr_side_table_size(this->module_.exprs.size());
        return id;
    } else if (this->postfix_bracket_is_generic_apply(base, op, next_op)) {
        syntax::GenericApplyExprPayload payload;
        payload.callee = base;
        payload.type_args = this->postfix_bracket_type_args(op);
        if (is_last) {
            this->module_.set_generic_apply_expr(chain_expr.value, range, std::move(payload));
            return chain_expr;
        }
        const syntax::ExprId id = this->module_.push_generic_apply_expr(range, std::move(payload));
        this->ensure_expr_side_table_size(this->module_.exprs.size());
        return id;
    } else {
        syntax::ExprId index = syntax::INVALID_EXPR_ID;
        if (!op.bracket_args.empty()) {
            index = op.bracket_args.front().expr;
        }
        if (op.bracket_args.size() > 1) {
            this->report(op.range, "index expression expects one argument");
        }
        const syntax::IndexExprPayload payload {
            base,
            index,
        };
        if (is_last) {
            this->module_.set_index_expr(chain_expr.value, range, payload);
            return chain_expr;
        }
        const syntax::ExprId id = this->module_.push_index_expr(range, payload);
        this->ensure_expr_side_table_size(this->module_.exprs.size());
        return id;
    }
}

bool SemanticAnalyzer::postfix_bracket_is_generic_apply(
    const syntax::ExprId base,
    const syntax::PostfixOp& op,
    const syntax::PostfixOp* next_op
) {
    static_cast<void>(next_op);
    if (op.bracket_is_slice) {
        return false;
    }
    for (const syntax::PostfixBracketArg& arg : op.bracket_args) {
        if (syntax::is_valid(arg.type)) {
            return true;
        }
    }
    if (!syntax::is_valid(base) || base.value >= this->module_.exprs.size()) {
        return false;
    }

    const syntax::NameExprPayload* const base_name = this->module_.exprs.name_payload(base.value);
    if (base_name != nullptr &&
        base_name->scope_name.empty() &&
        this->symbols_.find(base_name->text) != nullptr) {
        return false;
    }

    const NamedTypeSelector selector = this->resolve_named_type_selector(base, false);
    if (selector.name.empty()) {
        return false;
    }

    const bool has_generic_type = selector.qualified
        ? this->find_generic_struct_in_module(selector.module, selector.name, selector.range, false) != nullptr ||
              this->find_generic_enum_in_module(selector.module, selector.name, selector.range, false) != nullptr ||
              this->find_generic_type_alias_in_module(selector.module, selector.name, selector.range, false) != nullptr
        : this->find_generic_struct_in_visible_modules(selector.name, selector.range, false) != nullptr ||
              this->find_generic_enum_in_visible_modules(selector.name, selector.range, false) != nullptr ||
              this->find_generic_type_alias_in_visible_modules(selector.name, selector.range, false) != nullptr;
    if (has_generic_type) {
        return true;
    }

    if (is_valid(this->resolve_named_type_selector_type(selector, false, false))) {
        return true;
    }
    if (this->find_generic_function_selector(selector, selector.range, false) != nullptr) {
        return true;
    }
    return this->find_function_selector(base, selector.name, selector.range, false) != nullptr;
}

std::vector<syntax::TypeId> SemanticAnalyzer::postfix_bracket_type_args(const syntax::PostfixOp& op) {
    std::vector<syntax::TypeId> args;
    args.reserve(op.bracket_args.size());
    for (const syntax::PostfixBracketArg& arg : op.bracket_args) {
        if (syntax::is_valid(arg.type)) {
            args.push_back(arg.type);
        } else {
            args.push_back(this->postfix_arg_expr_to_type(arg.expr));
        }
    }
    return args;
}

syntax::TypeId SemanticAnalyzer::postfix_chain_expr_to_type(const syntax::ExprId expr) {
    if (!syntax::is_valid(expr) || expr.value >= this->module_.exprs.size()) {
        return syntax::INVALID_TYPE_ID;
    }
    const syntax::PostfixChainExprPayload* const chain = this->module_.exprs.postfix_chain_payload(expr.value);
    if (chain == nullptr) {
        return this->postfix_arg_expr_to_type(expr);
    }

    syntax::TypeId current = this->postfix_arg_expr_to_type(chain->base);
    for (const syntax::PostfixOp& op : chain->ops) {
        if (!syntax::is_valid(current) || current.value >= this->module_.types.size()) {
            return syntax::INVALID_TYPE_ID;
        }
        if (!postfix_op_is_type_selector_continuation(op)) {
            this->report(op.range, "expected generic type argument");
            return syntax::INVALID_TYPE_ID;
        }

        if (op.kind == syntax::PostfixOpKind::select) {
            current = this->append_postfix_type_selector(current, op.name, op.range);
            if (!syntax::is_valid(current)) {
                return syntax::INVALID_TYPE_ID;
            }
            continue;
        }

        const syntax::TypeNode& current_type = this->module_.types[current.value];
        if (current_type.kind != syntax::TypeKind::named) {
            this->report(op.range, "expected generic type argument");
            return syntax::INVALID_TYPE_ID;
        }
        syntax::TypeNode type = copy_named_type_selector(current_type, merge_ranges(current_type.range, op.range));
        type.type_args = this->postfix_bracket_type_args(op);
        current = this->push_synthetic_type(std::move(type));
    }
    return current;
}

syntax::TypeId SemanticAnalyzer::append_postfix_type_selector(
    const syntax::TypeId current,
    const std::string_view name,
    const base::SourceRange range
) {
    if (!syntax::is_valid(current) || current.value >= this->module_.types.size()) {
        return syntax::INVALID_TYPE_ID;
    }

    const syntax::TypeNode& current_type = this->module_.types[current.value];
    if (current_type.kind != syntax::TypeKind::named || !current_type.type_args.empty()) {
        this->report(range, "expected generic type argument");
        return syntax::INVALID_TYPE_ID;
    }

    syntax::TypeNode type;
    type.kind = syntax::TypeKind::named;
    const base::SourceRange previous_range = current_type.range;
    if (current_type.scope_parts.empty()) {
        if (!current_type.scope_name.empty()) {
            type.scope_parts.push_back(current_type.scope_name);
        }
        type.scope_parts.push_back(current_type.name);
    } else {
        type.scope_parts = current_type.scope_parts;
        type.scope_parts.push_back(current_type.name);
    }
    type.scope_name = type.scope_parts.front();
    type.scope_range = previous_range;
    type.name = name;
    type.range = merge_ranges(previous_range, range);
    return this->push_synthetic_type(std::move(type));
}

syntax::TypeId SemanticAnalyzer::postfix_arg_expr_to_type(const syntax::ExprId expr) {
    if (!syntax::is_valid(expr) || expr.value >= this->module_.exprs.size()) {
        return syntax::INVALID_TYPE_ID;
    }

    const syntax::ExprKind kind = this->module_.exprs.kind(expr.value);
    switch (kind) {
    case syntax::ExprKind::name: {
        const syntax::NameExprPayload* const node = this->module_.exprs.name_payload(expr.value);
        if (node == nullptr) {
            break;
        }
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::named;
        type.range = this->module_.exprs.range(expr.value);
        type.scope_name = node->scope_name;
        type.scope_range = node->scope_range;
        if (!node->scope_name.empty()) {
            type.scope_parts.push_back(node->scope_name);
        }
        type.name = node->text;
        return this->push_synthetic_type(std::move(type));
    }
    case syntax::ExprKind::field: {
        const syntax::FieldExprPayload* const node = this->module_.exprs.field_payload(expr.value);
        if (node == nullptr || !syntax::is_valid(node->object) || node->object.value >= this->module_.exprs.size()) {
            break;
        }
        const syntax::NameExprPayload* const object = this->module_.exprs.name_payload(node->object.value);
        if (object == nullptr || !object->scope_name.empty()) {
            break;
        }
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::named;
        type.range = this->module_.exprs.range(expr.value);
        type.scope_name = object->text;
        type.scope_range = this->module_.exprs.range(node->object.value);
        type.scope_parts.push_back(object->text);
        type.name = node->field_name;
        return this->push_synthetic_type(std::move(type));
    }
    case syntax::ExprKind::generic_apply: {
        const syntax::GenericApplyExprPayload* const node = this->module_.exprs.generic_apply_payload(expr.value);
        if (node == nullptr) {
            break;
        }
        syntax::TypeId callee = this->postfix_arg_expr_to_type(node->callee);
        if (!syntax::is_valid(callee) || callee.value >= this->module_.types.size()) {
            break;
        }
        const syntax::TypeNode& callee_type = this->module_.types[callee.value];
        if (callee_type.kind != syntax::TypeKind::named) {
            break;
        }
        syntax::TypeNode type = copy_named_type_selector(callee_type, this->module_.exprs.range(expr.value));
        type.type_args = node->type_args;
        return this->push_synthetic_type(std::move(type));
    }
    case syntax::ExprKind::postfix_chain:
        return this->postfix_chain_expr_to_type(expr);
    case syntax::ExprKind::unary:
        if (const syntax::UnaryExprPayload* const node = this->module_.exprs.unary_payload(expr.value);
            node != nullptr &&
            (node->op == syntax::UnaryOp::address_of || node->op == syntax::UnaryOp::address_of_mut)) {
            const syntax::TypeId pointee = this->postfix_arg_expr_to_type(node->operand);
            if (!syntax::is_valid(pointee)) {
                break;
            }
            syntax::TypeNode type;
            type.kind = syntax::TypeKind::reference;
            type.range = this->module_.exprs.range(expr.value);
            type.pointer_mutability = node->op == syntax::UnaryOp::address_of_mut
                ? syntax::PointerMutability::mut
                : syntax::PointerMutability::const_;
            type.pointee = pointee;
            return this->push_synthetic_type(std::move(type));
        }
        break;
    default:
        break;
    }

    this->report(this->module_.exprs.range(expr.value), "expected generic type argument");
    return syntax::INVALID_TYPE_ID;
}

syntax::TypeId SemanticAnalyzer::push_synthetic_type(syntax::TypeNode node) {
    const syntax::TypeId id = this->module_.push_type(std::move(node));
    this->ensure_type_side_table_size(this->module_.types.size());
    return id;
}

void SemanticAnalyzer::ensure_expr_side_table_size(const base::usize size) {
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        static_cast<void>(size);
        return;
    }
    std::vector<TypeHandle>& expr_types = this->active_expr_types();
    std::vector<TypeHandle>& expr_expected_types = this->active_expr_expected_types();
    std::vector<std::string>& expr_c_names = this->active_expr_c_names();
    if (expr_types.size() < size) {
        expr_types.resize(size, INVALID_TYPE_HANDLE);
    }
    if (expr_expected_types.size() < size) {
        expr_expected_types.resize(size, INVALID_TYPE_HANDLE);
    }
    if (expr_c_names.size() < size) {
        expr_c_names.resize(size);
    }
}

void SemanticAnalyzer::ensure_type_side_table_size(const base::usize size) {
    if (this->current_side_tables_.side_tables != nullptr && this->current_side_tables_.side_tables->sparse) {
        static_cast<void>(size);
        return;
    }
    std::vector<TypeHandle>& syntax_type_handles = this->active_syntax_type_handles();
    if (syntax_type_handles.size() < size) {
        syntax_type_handles.resize(size, INVALID_TYPE_HANDLE);
    }
}

} // namespace aurex::sema
