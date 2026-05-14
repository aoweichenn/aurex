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
    return module.exprs[expr.value].range;
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

} // namespace

TypeHandle SemanticAnalyzer::analyze_postfix_chain_expr(
    const syntax::ExprId expr_id,
    const TypeHandle expected_type
) {
    const syntax::ExprId materialized = this->materialize_postfix_chain(expr_id);
    if (materialized.value != expr_id.value) {
        return this->analyze_expr(materialized, expected_type);
    }
    return this->analyze_expr(expr_id, this->module_.exprs[expr_id.value], expected_type);
}

syntax::ExprId SemanticAnalyzer::materialize_postfix_chain(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->module_.exprs.size()) {
        return syntax::INVALID_EXPR_ID;
    }

    const syntax::ExprNode chain = this->module_.exprs[expr_id.value];
    if (chain.kind != syntax::ExprKind::postfix_chain) {
        return expr_id;
    }

    syntax::ExprId current = chain.postfix_base;
    for (base::usize index = 0; index < chain.postfix_ops.size(); ++index) {
        if (syntax::is_valid(current) &&
            current.value < this->module_.exprs.size() &&
            this->module_.exprs[current.value].kind == syntax::ExprKind::postfix_chain) {
            current = this->materialize_postfix_chain(current);
        }
        const bool is_last = index + 1 == chain.postfix_ops.size();
        const syntax::PostfixOp* const next_op = is_last ? nullptr : &chain.postfix_ops[index + 1];
        current = this->materialize_postfix_op(
            expr_id,
            current,
            chain.postfix_ops[index],
            next_op,
            is_last
        );
    }
    return current;
}

syntax::ExprId SemanticAnalyzer::materialize_postfix_op(
    const syntax::ExprId chain_expr,
    const syntax::ExprId base,
    const syntax::PostfixOp& op,
    const syntax::PostfixOp* next_op,
    const bool is_last
) {
    syntax::ExprNode node;
    node.range = merge_ranges(expr_range_or(this->module_, base, op.range), op.range);
    switch (op.kind) {
    case syntax::PostfixOpKind::bracket:
        return this->materialize_postfix_bracket_op(chain_expr, base, op, next_op, is_last);
    case syntax::PostfixOpKind::select:
        node.kind = op.name.empty() ? syntax::ExprKind::invalid : syntax::ExprKind::field;
        node.object = base;
        node.field_name = op.name;
        break;
    case syntax::PostfixOpKind::call:
        node.kind = syntax::ExprKind::call;
        node.callee = base;
        node.args = op.args;
        break;
    case syntax::PostfixOpKind::try_:
        node.kind = syntax::ExprKind::try_expr;
        node.unary_operand = base;
        break;
    case syntax::PostfixOpKind::struct_literal:
        node.kind = syntax::ExprKind::struct_literal;
        node.object = base;
        node.field_inits = op.field_inits;
        break;
    }

    if (is_last) {
        this->module_.exprs[chain_expr.value] = std::move(node);
        return chain_expr;
    }
    return this->push_synthetic_expr(std::move(node));
}

syntax::ExprId SemanticAnalyzer::materialize_postfix_bracket_op(
    const syntax::ExprId chain_expr,
    const syntax::ExprId base,
    const syntax::PostfixOp& op,
    const syntax::PostfixOp* next_op,
    const bool is_last
) {
    syntax::ExprNode node;
    node.range = merge_ranges(expr_range_or(this->module_, base, op.range), op.range);
    node.object = base;

    if (op.bracket_is_slice) {
        node.kind = syntax::ExprKind::slice;
        node.slice_start = op.slice_start;
        node.slice_end = op.slice_end;
    } else if (this->postfix_bracket_is_generic_apply(base, op, next_op)) {
        node.kind = syntax::ExprKind::generic_apply;
        node.callee = base;
        node.type_args = this->postfix_bracket_type_args(op);
    } else {
        node.kind = syntax::ExprKind::index;
        if (!op.bracket_args.empty()) {
            node.index = op.bracket_args.front().expr;
        }
        if (op.bracket_args.size() > 1) {
            this->report(op.range, "index expression expects one argument");
        }
    }

    if (is_last) {
        this->module_.exprs[chain_expr.value] = std::move(node);
        return chain_expr;
    }
    return this->push_synthetic_expr(std::move(node));
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

    const syntax::ExprNode& base_expr = this->module_.exprs[base.value];
    if (base_expr.kind == syntax::ExprKind::name &&
        base_expr.scope_name.empty() &&
        this->symbols_.find(base_expr.text) != nullptr) {
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
    const syntax::ExprNode chain = this->module_.exprs[expr.value];
    if (chain.kind != syntax::ExprKind::postfix_chain) {
        return this->postfix_arg_expr_to_type(expr);
    }

    syntax::TypeId current = this->postfix_arg_expr_to_type(chain.postfix_base);
    for (const syntax::PostfixOp& op : chain.postfix_ops) {
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

        syntax::TypeNode type = this->module_.types[current.value];
        type.range = merge_ranges(type.range, op.range);
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

    syntax::TypeNode type = this->module_.types[current.value];
    if (type.kind != syntax::TypeKind::named || !type.type_args.empty()) {
        this->report(range, "expected generic type argument");
        return syntax::INVALID_TYPE_ID;
    }

    const base::SourceRange previous_range = type.range;
    if (type.scope_parts.empty()) {
        if (!type.scope_name.empty()) {
            type.scope_parts.push_back(type.scope_name);
        }
        type.scope_parts.push_back(type.name);
    } else {
        type.scope_parts.push_back(type.name);
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

    const syntax::ExprNode node = this->module_.exprs[expr.value];
    switch (node.kind) {
    case syntax::ExprKind::name: {
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::named;
        type.range = node.range;
        type.scope_name = node.scope_name;
        type.scope_range = node.scope_range;
        if (!node.scope_name.empty()) {
            type.scope_parts.push_back(node.scope_name);
        }
        type.name = node.text;
        return this->push_synthetic_type(std::move(type));
    }
    case syntax::ExprKind::field: {
        if (!syntax::is_valid(node.object) || node.object.value >= this->module_.exprs.size()) {
            break;
        }
        const syntax::ExprNode& object = this->module_.exprs[node.object.value];
        if (object.kind != syntax::ExprKind::name || !object.scope_name.empty()) {
            break;
        }
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::named;
        type.range = node.range;
        type.scope_name = object.text;
        type.scope_range = object.range;
        type.scope_parts.push_back(object.text);
        type.name = node.field_name;
        return this->push_synthetic_type(std::move(type));
    }
    case syntax::ExprKind::generic_apply: {
        syntax::TypeId callee = this->postfix_arg_expr_to_type(node.callee);
        if (!syntax::is_valid(callee) || callee.value >= this->module_.types.size()) {
            break;
        }
        syntax::TypeNode type = this->module_.types[callee.value];
        type.range = node.range;
        type.type_args = node.type_args;
        return this->push_synthetic_type(std::move(type));
    }
    case syntax::ExprKind::postfix_chain:
        return this->postfix_chain_expr_to_type(expr);
    case syntax::ExprKind::unary:
        if (node.unary_op == syntax::UnaryOp::address_of ||
            node.unary_op == syntax::UnaryOp::address_of_mut) {
            const syntax::TypeId pointee = this->postfix_arg_expr_to_type(node.unary_operand);
            if (!syntax::is_valid(pointee)) {
                break;
            }
            syntax::TypeNode type;
            type.kind = syntax::TypeKind::reference;
            type.range = node.range;
            type.pointer_mutability = node.unary_op == syntax::UnaryOp::address_of_mut
                ? syntax::PointerMutability::mut
                : syntax::PointerMutability::const_;
            type.pointee = pointee;
            return this->push_synthetic_type(std::move(type));
        }
        break;
    default:
        break;
    }

    this->report(node.range, "expected generic type argument");
    return syntax::INVALID_TYPE_ID;
}

syntax::ExprId SemanticAnalyzer::push_synthetic_expr(syntax::ExprNode node) {
    const syntax::ExprId id = this->module_.push_expr(std::move(node));
    this->ensure_expr_side_table_size(this->module_.exprs.size());
    return id;
}

syntax::TypeId SemanticAnalyzer::push_synthetic_type(syntax::TypeNode node) {
    const syntax::TypeId id = this->module_.push_type(std::move(node));
    this->ensure_type_side_table_size(this->module_.types.size());
    return id;
}

void SemanticAnalyzer::ensure_expr_side_table_size(const base::usize size) {
    std::vector<TypeHandle>& expr_types = this->active_expr_types();
    std::vector<std::string>& expr_c_names = this->active_expr_c_names();
    if (expr_types.size() < size) {
        expr_types.resize(size, INVALID_TYPE_HANDLE);
    }
    if (expr_c_names.size() < size) {
        expr_c_names.resize(size);
    }
}

void SemanticAnalyzer::ensure_type_side_table_size(const base::usize size) {
    std::vector<TypeHandle>& syntax_type_handles = this->active_syntax_type_handles();
    if (syntax_type_handles.size() < size) {
        syntax_type_handles.resize(size, INVALID_TYPE_HANDLE);
    }
}

} // namespace aurex::sema
