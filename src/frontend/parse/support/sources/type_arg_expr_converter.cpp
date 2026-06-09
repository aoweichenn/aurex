#include <aurex/frontend/parse/parser_messages.hpp>

#include <array>
#include <string>
#include <utility>
#include <vector>

#include <frontend/parse/support/private/type_arg_expr_converter.hpp>

namespace aurex::parse {
namespace {

using syntax::TokenKind;

constexpr base::usize PARSER_TYPE_EXPR_CHAIN_INLINE_CAPACITY = 8;

struct TypeExprChain {
    std::array<syntax::ExprId, PARSER_TYPE_EXPR_CHAIN_INLINE_CAPACITY> inline_exprs{};
    std::vector<syntax::ExprId> overflow;
    base::usize size = 0;

    void push(const syntax::ExprId value)
    {
        if (this->size < this->inline_exprs.size()) {
            this->inline_exprs[this->size] = value;
        } else {
            this->overflow.push_back(value);
        }
        ++this->size;
    }

    [[nodiscard]] syntax::ExprId at(const base::usize index) const noexcept
    {
        if (index < this->inline_exprs.size()) {
            return this->inline_exprs[index];
        }
        return this->overflow[index - this->inline_exprs.size()];
    }
};

} // namespace

syntax::TypeId TypeArgExprConverter::convert(const syntax::ExprId expr, const bool report_errors)
{
    if (!syntax::is_valid(expr) || expr.value >= this->session_.module.exprs.size()) {
        return syntax::INVALID_TYPE_ID;
    }

    TypeExprChain chain;
    syntax::ExprId current = expr;
    bool failed = false;
    while (!failed) {
        if (!syntax::is_valid(current) || current.value >= this->session_.module.exprs.size()) {
            failed = true;
            break;
        }

        chain.push(current);
        const syntax::ExprKind kind = this->session_.module.exprs.kind(current.value);
        if (kind == syntax::ExprKind::name) {
            break;
        }
        if (kind == syntax::ExprKind::field) {
            const syntax::FieldExprPayload* const payload = this->session_.module.exprs.field_payload(current.value);
            if (payload == nullptr || !syntax::is_valid(payload->object)) {
                failed = true;
                break;
            }
            current = payload->object;
            continue;
        }
        if (kind == syntax::ExprKind::generic_apply) {
            const syntax::GenericApplyExprPayload* const payload =
                this->session_.module.exprs.generic_apply_payload(current.value);
            if (payload == nullptr || !syntax::is_valid(payload->callee)) {
                failed = true;
                break;
            }
            current = payload->callee;
            continue;
        }
        if (kind == syntax::ExprKind::unary) {
            const syntax::UnaryExprPayload* const payload = this->session_.module.exprs.unary_payload(current.value);
            if (payload == nullptr
                || (payload->op != syntax::UnaryOp::address_of && payload->op != syntax::UnaryOp::address_of_mut)
                || !syntax::is_valid(payload->operand)) {
                failed = true;
                break;
            }
            current = payload->operand;
            continue;
        }
        failed = true;
    }

    syntax::TypeId converted = syntax::INVALID_TYPE_ID;
    for (base::usize remaining = chain.size; !failed && remaining > 0; --remaining) {
        const syntax::ExprId chain_expr = chain.at(remaining - 1);
        const syntax::ExprKind kind = this->session_.module.exprs.kind(chain_expr.value);
        switch (kind) {
            case syntax::ExprKind::name: {
                const syntax::NameExprPayload* const payload =
                    this->session_.module.exprs.name_payload(chain_expr.value);
                if (payload == nullptr) {
                    failed = true;
                    break;
                }
                syntax::TypeNode type;
                type.kind = syntax::TypeKind::named;
                type.range = this->session_.module.exprs.range(chain_expr.value);
                type.scope_name = payload->scope_name;
                type.scope_range = payload->scope_range;
                if (!payload->scope_name.empty()) {
                    type.scope_parts.push_back(payload->scope_name);
                }
                type.name = payload->text;
                converted = this->session_.module.push_type(std::move(type));
                break;
            }
            case syntax::ExprKind::field: {
                const syntax::FieldExprPayload* const payload =
                    this->session_.module.exprs.field_payload(chain_expr.value);
                if (payload == nullptr) {
                    failed = true;
                    break;
                }
                converted = this->append_selector(
                    converted, payload->field_name, this->session_.module.exprs.range(chain_expr.value), report_errors);
                failed = !syntax::is_valid(converted);
                break;
            }
            case syntax::ExprKind::generic_apply: {
                const syntax::GenericApplyExprPayload* const payload =
                    this->session_.module.exprs.generic_apply_payload(chain_expr.value);
                if (payload == nullptr || !syntax::is_valid(converted)
                    || converted.value >= this->session_.module.types.size()) {
                    failed = true;
                    break;
                }
                syntax::TypeNode type = this->session_.module.types[converted.value];
                if (type.kind != syntax::TypeKind::named) {
                    failed = true;
                    break;
                }
                type.range = this->session_.module.exprs.range(chain_expr.value);
                type.type_args.assign(payload->type_args.begin(), payload->type_args.end());
                type.generic_args.assign(payload->generic_args.begin(), payload->generic_args.end());
                converted = this->session_.module.push_type(std::move(type));
                break;
            }
            case syntax::ExprKind::unary: {
                const syntax::UnaryExprPayload* const payload =
                    this->session_.module.exprs.unary_payload(chain_expr.value);
                if (payload == nullptr || !syntax::is_valid(converted)) {
                    failed = true;
                    break;
                }
                syntax::TypeNode type;
                type.kind = syntax::TypeKind::reference;
                type.range = this->session_.module.exprs.range(chain_expr.value);
                type.pointer_mutability = payload->op == syntax::UnaryOp::address_of_mut
                    ? syntax::PointerMutability::mut
                    : syntax::PointerMutability::const_;
                type.pointee = converted;
                converted = this->session_.module.push_type(std::move(type));
                break;
            }
            default:
                failed = true;
                break;
        }
    }

    if (!failed && syntax::is_valid(converted)) {
        return syntax::TypeId{converted.value};
    }

    if (report_errors) {
        this->session_.diagnostics.report_at(
            syntax::Token{TokenKind::invalid, this->session_.module.exprs.range(expr.value), {}},
            std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
    }
    return syntax::INVALID_TYPE_ID;
}

syntax::TypeId TypeArgExprConverter::append_selector(
    const syntax::TypeId base, const std::string_view name, const base::SourceRange& range, const bool report_errors)
{
    if (!syntax::is_valid(base) || base.value >= this->session_.module.types.size()) {
        return syntax::INVALID_TYPE_ID;
    }
    const syntax::TypeNode base_type = this->session_.module.types[base.value];
    if (base_type.kind != syntax::TypeKind::named || !base_type.type_args.empty() || !base_type.generic_args.empty()) {
        if (report_errors) {
            this->session_.diagnostics.report_at(
                syntax::Token{TokenKind::invalid, range, {}}, std::string(PARSER_EXPECT_GENERIC_TYPE_ARGUMENT));
        }
        return syntax::INVALID_TYPE_ID;
    }

    syntax::TypeNode type;
    type.kind = syntax::TypeKind::named;
    type.range = range;
    if (base_type.scope_parts.empty()) {
        if (!base_type.scope_name.empty()) {
            type.scope_parts.push_back(base_type.scope_name);
        }
        type.scope_parts.push_back(base_type.name);
    } else {
        type.scope_parts = base_type.scope_parts;
        type.scope_parts.push_back(base_type.name);
    }
    if (!type.scope_parts.empty()) {
        type.scope_name = type.scope_parts.front();
    }
    type.scope_range = base_type.range;
    type.name = name;
    return this->session_.module.push_type(std::move(type));
}

} // namespace aurex::parse
