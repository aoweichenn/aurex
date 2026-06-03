#include <aurex/frontend/sema/sema_messages.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>
#include <frontend/sema/internal/expressions/private/sema_expression_analyzer.hpp>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_TRY_SHAPE_CASE_COUNT = 2;
constexpr std::string_view SEMA_RESULT_OK_CASE_NAME = "ok";
constexpr std::string_view SEMA_RESULT_ERR_CASE_NAME = "err";
constexpr std::string_view SEMA_OPTION_SOME_CASE_NAME = "some";
constexpr std::string_view SEMA_OPTION_NONE_CASE_NAME = "none";

template <typename T, typename Allocator>
[[nodiscard]] std::span<const T> readonly_span(const std::vector<T, Allocator>& values) noexcept
{
    return {values.data(), values.size()};
}

} // namespace

SemanticAnalyzerCore::TryShape SemanticAnalyzerCore::classify_try_shape(const TypeHandle type) const noexcept
{
    if (!is_valid(type) || type.value >= this->state_.checked.types.size()
        || this->state_.checked.types.get(type).kind != TypeKind::enum_) {
        return {};
    }
    const EnumCaseList* const cases = this->find_enum_cases_by_type(type);
    if (cases == nullptr || cases->size() != SEMA_TRY_SHAPE_CASE_COUNT) {
        return {};
    }

    const IdentId ok_id = this->ctx_.module.find_identifier(SEMA_RESULT_OK_CASE_NAME);
    const IdentId err_id = this->ctx_.module.find_identifier(SEMA_RESULT_ERR_CASE_NAME);
    const EnumCaseInfo* const ok_case = this->find_enum_case_by_type_and_case(type, ok_id, SEMA_RESULT_OK_CASE_NAME);
    const EnumCaseInfo* const err_case = this->find_enum_case_by_type_and_case(type, err_id, SEMA_RESULT_ERR_CASE_NAME);
    if (ok_case != nullptr || err_case != nullptr) {
        TryShape shape;
        shape.kind = ok_case != nullptr && err_case != nullptr && is_valid(ok_case->payload_type)
                && is_valid(err_case->payload_type)
            ? TryShape::Kind::result
            : TryShape::Kind::malformed_result;
        shape.success_case = ok_case;
        shape.failure_case = err_case;
        return shape;
    }

    const IdentId some_id = this->ctx_.module.find_identifier(SEMA_OPTION_SOME_CASE_NAME);
    const IdentId none_id = this->ctx_.module.find_identifier(SEMA_OPTION_NONE_CASE_NAME);
    const EnumCaseInfo* const some_case =
        this->find_enum_case_by_type_and_case(type, some_id, SEMA_OPTION_SOME_CASE_NAME);
    const EnumCaseInfo* const none_case =
        this->find_enum_case_by_type_and_case(type, none_id, SEMA_OPTION_NONE_CASE_NAME);
    if (some_case != nullptr || none_case != nullptr) {
        TryShape shape;
        shape.kind = some_case != nullptr && none_case != nullptr && is_valid(some_case->payload_type)
                && !is_valid(none_case->payload_type)
            ? TryShape::Kind::option
            : TryShape::Kind::malformed_option;
        shape.success_case = some_case;
        shape.failure_case = none_case;
        return shape;
    }

    return {};
}

SemanticAnalyzerCore::ExprView SemanticAnalyzerCore::expr_view(const syntax::ExprId expr_id) const noexcept
{
    ExprView view{};
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ctx_.module.exprs.size()) {
        return view;
    }

    view.kind = this->ctx_.module.exprs.kind(expr_id.value);
    view.range = this->ctx_.module.exprs.range(expr_id.value);
    if (const syntax::LiteralExprPayload* const literal = this->ctx_.module.exprs.literal_payload(expr_id.value);
        literal != nullptr) {
        view.text = literal->text;
        return view;
    }
    if (const syntax::CastExprPayload* const cast = this->ctx_.module.exprs.cast_payload(expr_id.value);
        cast != nullptr) {
        view.cast_type = cast->type;
        view.cast_expr = cast->expr;
        return view;
    }

    switch (view.kind) {
        case syntax::ExprKind::name: {
            const syntax::NameExprPayload& payload = *this->ctx_.module.exprs.name_payload(expr_id.value);
            view.scope_name = payload.scope_name;
            view.scope_name_id = payload.scope_name_id;
            view.scope_range = payload.scope_range;
            view.text = payload.text;
            view.text_id = payload.text_id;
            view.type_args = readonly_span(payload.type_args);
            break;
        }
        case syntax::ExprKind::generic_apply: {
            const syntax::GenericApplyExprPayload& payload =
                *this->ctx_.module.exprs.generic_apply_payload(expr_id.value);
            view.callee = payload.callee;
            view.type_args = readonly_span(payload.type_args);
            break;
        }
        case syntax::ExprKind::unary: {
            const syntax::UnaryExprPayload& payload = *this->ctx_.module.exprs.unary_payload(expr_id.value);
            view.unary_op = payload.op;
            view.unary_operand = payload.operand;
            break;
        }
        case syntax::ExprKind::try_expr: {
            const syntax::TryExprPayload& payload = *this->ctx_.module.exprs.try_payload(expr_id.value);
            view.try_operand = payload.operand;
            break;
        }
        case syntax::ExprKind::binary: {
            const syntax::BinaryExprPayload& payload = *this->ctx_.module.exprs.binary_payload(expr_id.value);
            view.binary_op = payload.op;
            view.binary_lhs = payload.lhs;
            view.binary_rhs = payload.rhs;
            break;
        }
        case syntax::ExprKind::call:
        case syntax::ExprKind::str_from_bytes_unchecked: {
            const syntax::CallExprPayload& payload = *this->ctx_.module.exprs.call_payload(expr_id.value);
            view.callee = payload.callee;
            view.args = readonly_span(payload.args);
            break;
        }
        case syntax::ExprKind::if_expr: {
            const syntax::IfExprPayload& payload = *this->ctx_.module.exprs.if_payload(expr_id.value);
            view.condition = payload.condition;
            view.condition_pattern = payload.condition_pattern;
            view.then_expr = payload.then_expr;
            view.else_expr = payload.else_expr;
            break;
        }
        case syntax::ExprKind::block_expr:
        case syntax::ExprKind::unsafe_block: {
            const syntax::BlockExprPayload& payload = *this->ctx_.module.exprs.block_payload(expr_id.value);
            view.block = payload.block;
            view.block_result = payload.result;
            break;
        }
        case syntax::ExprKind::match_expr: {
            const syntax::MatchExprPayload& payload = *this->ctx_.module.exprs.match_payload(expr_id.value);
            view.match_value = payload.value;
            view.match_arms = readonly_span(payload.arms);
            break;
        }
        case syntax::ExprKind::array_literal: {
            const syntax::ArrayExprPayload& payload = *this->ctx_.module.exprs.array_payload(expr_id.value);
            view.array_elements = readonly_span(payload.elements);
            view.array_repeat_value = payload.repeat_value;
            view.array_repeat_count = payload.repeat_count;
            break;
        }
        case syntax::ExprKind::tuple_literal: {
            const syntax::AstArenaVector<syntax::ExprId>& payload =
                *this->ctx_.module.exprs.tuple_elements(expr_id.value);
            view.tuple_elements = readonly_span(payload);
            break;
        }
        case syntax::ExprKind::field: {
            const syntax::FieldExprPayload& payload = *this->ctx_.module.exprs.field_payload(expr_id.value);
            view.object = payload.object;
            view.field_name = payload.field_name;
            view.field_name_id = payload.field_name_id;
            break;
        }
        case syntax::ExprKind::index: {
            const syntax::IndexExprPayload& payload = *this->ctx_.module.exprs.index_payload(expr_id.value);
            view.object = payload.object;
            view.index = payload.index;
            break;
        }
        case syntax::ExprKind::slice: {
            const syntax::SliceExprPayload& payload = *this->ctx_.module.exprs.slice_payload(expr_id.value);
            view.object = payload.object;
            view.slice_start = payload.start;
            view.slice_end = payload.end;
            break;
        }
        case syntax::ExprKind::struct_literal: {
            const syntax::StructLiteralExprPayload& payload =
                *this->ctx_.module.exprs.struct_literal_payload(expr_id.value);
            view.object = payload.object;
            view.scope_name = payload.scope_name;
            view.scope_name_id = payload.scope_name_id;
            view.scope_range = payload.scope_range;
            view.struct_name = payload.name;
            view.struct_name_id = payload.name_id;
            view.type_args = readonly_span(payload.type_args);
            view.field_inits = readonly_span(payload.field_inits);
            break;
        }
        case syntax::ExprKind::invalid:
        case syntax::ExprKind::integer_literal:
        case syntax::ExprKind::float_literal:
        case syntax::ExprKind::bool_literal:
        case syntax::ExprKind::null_literal:
        case syntax::ExprKind::string_literal:
        case syntax::ExprKind::c_string_literal:
        case syntax::ExprKind::raw_string_literal:
        case syntax::ExprKind::byte_string_literal:
        case syntax::ExprKind::byte_literal:
        case syntax::ExprKind::char_literal:
        case syntax::ExprKind::cast:
        case syntax::ExprKind::pcast:
        case syntax::ExprKind::bcast:
        case syntax::ExprKind::size_of:
        case syntax::ExprKind::align_of:
        case syntax::ExprKind::ptr_addr:
        case syntax::ExprKind::paddr:
        case syntax::ExprKind::slice_data:
        case syntax::ExprKind::slice_len:
        case syntax::ExprKind::str_data:
        case syntax::ExprKind::str_byte_len:
        case syntax::ExprKind::str_is_valid_utf8:
        case syntax::ExprKind::str_from_utf8_checked:
            break;
    }
    return view;
}

TypeHandle SemanticAnalyzerCore::analyze_name_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr)
{
    const Symbol* symbol = nullptr;
    if (!expr.scope_name.empty()) {
        const syntax::ModuleId module = this->resolve_import_alias(expr.scope_name, expr.scope_range);
        if (!syntax::is_valid(module)) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        symbol = this->find_symbol_in_module(module, expr.text_id, expr.text, expr.range);
    } else {
        symbol = this->find_symbol(expr.text_id, expr.text, expr.range);
    }
    if (symbol == nullptr) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (symbol->kind == SymbolKind::function) {
        this->record_expr_c_name(expr_id, symbol->c_name);
        return this->record_expr_type(expr_id, this->function_type_from_symbol(*symbol, expr.range));
    }
    this->record_expr_c_name(expr_id, symbol->c_name);
    return this->record_expr_type(expr_id, symbol->type);
}

bool SemanticAnalyzerCore::record_no_payload_enum_case_expr(
    const syntax::ExprId expr_id, const EnumCaseInfo& enum_case, const base::SourceRange& range)
{
    if (is_valid(enum_case.payload_type)) {
        this->report_general(range,
            sema_enum_payload_constructor_call_message(enum_case_display_name(this->state_.checked.types, enum_case)));
        static_cast<void>(this->record_expr_type(expr_id, INVALID_TYPE_HANDLE));
        return false;
    }
    this->record_expr_c_name(expr_id, enum_case.c_name);
    static_cast<void>(this->record_expr_type(expr_id, enum_case.type));
    return true;
}

TypeHandle SemanticAnalyzerCore::analyze_generic_apply_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr)
{
    this->report_general(expr.range, std::string(SEMA_EXPLICIT_GENERIC_CALL_SYNTAX));
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

} // namespace aurex::sema
