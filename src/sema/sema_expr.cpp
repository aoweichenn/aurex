#include <aurex/sema/sema.hpp>

#include <aurex/base/string_literal.hpp>
#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

namespace {

constexpr base::u32 SEMA_INTEGER_BITS_PER_BYTE = 8;
constexpr base::u32 SEMA_I8_BIT_WIDTH = 8;
constexpr base::u32 SEMA_I16_BIT_WIDTH = 16;
constexpr base::u32 SEMA_I32_BIT_WIDTH = 32;
constexpr base::u32 SEMA_I64_BIT_WIDTH = 64;
constexpr base::u32 SEMA_I64_SIGN_BIT_INDEX = 63;
constexpr base::u32 SEMA_SIGN_BIT_OFFSET = 1;
constexpr base::usize SEMA_TRY_SHAPE_CASE_COUNT = 2;
constexpr std::string_view SEMA_RESULT_OK_CASE_NAME = "ok";
constexpr std::string_view SEMA_RESULT_ERR_CASE_NAME = "err";
constexpr std::string_view SEMA_OPTION_SOME_CASE_NAME = "some";
constexpr std::string_view SEMA_OPTION_NONE_CASE_NAME = "none";

struct IntegerLiteralExpr {
    syntax::ExprId literal = syntax::INVALID_EXPR_ID;
    bool negated = false;
};

template <typename T>
[[nodiscard]] std::span<const T> readonly_span(const std::vector<T>& values) noexcept {
    return {values.data(), values.size()};
}

[[nodiscard]] bool binary_result_uses_operand_type(const syntax::BinaryOp op) noexcept {
    switch (op) {
    case syntax::BinaryOp::add:
    case syntax::BinaryOp::sub:
    case syntax::BinaryOp::mul:
    case syntax::BinaryOp::div:
    case syntax::BinaryOp::mod:
    case syntax::BinaryOp::shl:
    case syntax::BinaryOp::shr:
    case syntax::BinaryOp::bit_and:
    case syntax::BinaryOp::bit_xor:
    case syntax::BinaryOp::bit_or:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_contextual_integer_expr(
    const syntax::AstModule& module,
    const syntax::ExprId candidate
) {
    std::vector<syntax::ExprId> pending;
    pending.push_back(candidate);
    while (!pending.empty()) {
        const syntax::ExprId current = pending.back();
        pending.pop_back();
        if (!syntax::is_valid(current) || current.value >= module.exprs.size()) {
            return false;
        }
        const syntax::ExprKind kind = module.exprs.kind(current.value);
        if (kind == syntax::ExprKind::integer_literal) {
            continue;
        }
        if (kind == syntax::ExprKind::unary) {
            const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(current.value);
            if (unary != nullptr && unary->op == syntax::UnaryOp::numeric_negate) {
                pending.push_back(unary->operand);
                continue;
            }
        }
        if (kind == syntax::ExprKind::binary) {
            const syntax::BinaryExprPayload* const binary = module.exprs.binary_payload(current.value);
            if (binary != nullptr && binary_result_uses_operand_type(binary->op)) {
                pending.push_back(binary->lhs);
                pending.push_back(binary->rhs);
                continue;
            }
        }
        return false;
    }
    return true;
}

[[nodiscard]] bool expr_is_kind(
    const syntax::AstModule& module,
    const syntax::ExprId expr,
    const syntax::ExprKind kind
) noexcept {
    return syntax::is_valid(expr) &&
           expr.value < module.exprs.size() &&
           module.exprs.kind(expr.value) == kind;
}

[[nodiscard]] std::string_view expr_literal_text_or_empty(
    const syntax::AstModule& module,
    const syntax::ExprId expr
) noexcept {
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return {};
    }
    const syntax::LiteralExprPayload* const literal = module.exprs.literal_payload(expr.value);
    return literal == nullptr ? std::string_view {} : literal->text;
}

[[nodiscard]] bool expr_name_is_unqualified(
    const syntax::AstModule& module,
    const syntax::ExprId expr
) noexcept {
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return false;
    }
    const syntax::NameExprPayload* const name = module.exprs.name_payload(expr.value);
    return name != nullptr && name->scope_name.empty();
}

[[nodiscard]] syntax::ExprId unary_operand_or_invalid(
    const syntax::AstModule& module,
    const syntax::ExprId expr
) noexcept {
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return syntax::INVALID_EXPR_ID;
    }
    const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(expr.value);
    return unary == nullptr ? syntax::INVALID_EXPR_ID : unary->operand;
}

[[nodiscard]] bool unary_expr_has_op(
    const syntax::AstModule& module,
    const syntax::ExprId expr,
    const syntax::UnaryOp op
) noexcept {
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return false;
    }
    const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(expr.value);
    return unary != nullptr && unary->op == op;
}

[[nodiscard]] IntegerLiteralExpr integer_literal_expr(
    const syntax::AstModule& module,
    const syntax::ExprId candidate
) noexcept {
    if (!syntax::is_valid(candidate) || candidate.value >= module.exprs.size()) {
        return {};
    }
    if (module.exprs.kind(candidate.value) == syntax::ExprKind::integer_literal) {
        return {candidate, false};
    }
    if (unary_expr_has_op(module, candidate, syntax::UnaryOp::numeric_negate)) {
        const syntax::ExprId operand = unary_operand_or_invalid(module, candidate);
        if (expr_is_kind(module, operand, syntax::ExprKind::integer_literal)) {
            return {operand, true};
        }
    }
    return {};
}

[[nodiscard]] bool is_signed_min_integer_literal(
    const IntegerLiteralExpr literal,
    const base::u64 value,
    const base::u32 bit_width
) noexcept {
    if (!literal.negated || bit_width == 0) {
        return false;
    }
    const base::u64 min_magnitude = bit_width >= SEMA_I64_BIT_WIDTH
        ? (base::u64 {1} << SEMA_I64_SIGN_BIT_INDEX)
        : (base::u64 {1} << (bit_width - SEMA_SIGN_BIT_OFFSET));
    return value == min_magnitude;
}

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module,
    const syntax::ExprId expr,
    const base::SourceRange fallback
) noexcept {
    return syntax::is_valid(expr) && expr.value < module.exprs.size()
        ? module.exprs.range(expr.value)
        : fallback;
}

[[nodiscard]] bool is_const_u8_pointer(const TypeTable& types, const TypeHandle type) noexcept {
    if (!types.is_pointer(type)) {
        return false;
    }
    const TypeInfo& pointer = types.get(type);
    return pointer.pointer_mutability == PointerMutability::const_ &&
           types.same(pointer.pointee, types.builtin(BuiltinType::u8));
}

[[nodiscard]] bool is_u8_slice(const TypeTable& types, const TypeHandle type) noexcept {
    return types.is_slice(type) &&
           types.same(types.get(type).slice_element, types.builtin(BuiltinType::u8));
}

[[nodiscard]] bool is_pointer_or_reference(const TypeTable& types, const TypeHandle type) noexcept {
    return types.is_pointer(type) || types.is_reference(type);
}

[[nodiscard]] bool is_signed_integer_type(const TypeTable& types, const TypeHandle type) noexcept {
    if (!types.is_integer(type)) {
        return false;
    }
    const TypeInfo& info = types.get(type);
    if (info.kind != TypeKind::builtin) {
        return false;
    }
    switch (info.builtin) {
    case BuiltinType::i8:
    case BuiltinType::i16:
    case BuiltinType::i32:
    case BuiltinType::i64:
    case BuiltinType::isize:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] base::u32 integer_bit_width(const TypeTable& types, const TypeHandle type) noexcept {
    if (!types.is_integer(type)) {
        return 0;
    }
    const TypeInfo& info = types.get(type);
    if (info.kind != TypeKind::builtin) {
        return 0;
    }
    switch (info.builtin) {
    case BuiltinType::i8:
    case BuiltinType::u8:
        return SEMA_I8_BIT_WIDTH;
    case BuiltinType::i16:
    case BuiltinType::u16:
        return SEMA_I16_BIT_WIDTH;
    case BuiltinType::i32:
    case BuiltinType::u32:
        return SEMA_I32_BIT_WIDTH;
    case BuiltinType::i64:
    case BuiltinType::u64:
        return SEMA_I64_BIT_WIDTH;
    case BuiltinType::isize:
        return static_cast<base::u32>(sizeof(base::isize) * SEMA_INTEGER_BITS_PER_BYTE);
    case BuiltinType::usize:
        return static_cast<base::u32>(sizeof(base::usize) * SEMA_INTEGER_BITS_PER_BYTE);
    default:
        return 0;
    }
}

} // namespace

SemanticAnalyzer::TryShape SemanticAnalyzer::classify_try_shape(const TypeHandle type) const noexcept {
    if (!is_valid(type) ||
        type.value >= this->checked_.types.size() ||
        this->checked_.types.get(type).kind != TypeKind::enum_) {
        return {};
    }
    const EnumCaseList* const cases = this->find_enum_cases_by_type(type);
    if (cases == nullptr || cases->size() != SEMA_TRY_SHAPE_CASE_COUNT) {
        return {};
    }

    const IdentId ok_id = this->module_.find_identifier(SEMA_RESULT_OK_CASE_NAME);
    const IdentId err_id = this->module_.find_identifier(SEMA_RESULT_ERR_CASE_NAME);
    const EnumCaseInfo* const ok_case =
        this->find_enum_case_by_type_and_case(type, ok_id, SEMA_RESULT_OK_CASE_NAME);
    const EnumCaseInfo* const err_case =
        this->find_enum_case_by_type_and_case(type, err_id, SEMA_RESULT_ERR_CASE_NAME);
    if (ok_case != nullptr || err_case != nullptr) {
        TryShape shape;
        shape.kind = ok_case != nullptr &&
                     err_case != nullptr &&
                     is_valid(ok_case->payload_type) &&
                     is_valid(err_case->payload_type)
            ? TryShape::Kind::result
            : TryShape::Kind::malformed_result;
        shape.success_case = ok_case;
        shape.failure_case = err_case;
        return shape;
    }

    const IdentId some_id = this->module_.find_identifier(SEMA_OPTION_SOME_CASE_NAME);
    const IdentId none_id = this->module_.find_identifier(SEMA_OPTION_NONE_CASE_NAME);
    const EnumCaseInfo* const some_case =
        this->find_enum_case_by_type_and_case(type, some_id, SEMA_OPTION_SOME_CASE_NAME);
    const EnumCaseInfo* const none_case =
        this->find_enum_case_by_type_and_case(type, none_id, SEMA_OPTION_NONE_CASE_NAME);
    if (some_case != nullptr || none_case != nullptr) {
        TryShape shape;
        shape.kind = some_case != nullptr &&
                     none_case != nullptr &&
                     is_valid(some_case->payload_type) &&
                     !is_valid(none_case->payload_type)
            ? TryShape::Kind::option
            : TryShape::Kind::malformed_option;
        shape.success_case = some_case;
        shape.failure_case = none_case;
        return shape;
    }

    return {};
}

SemanticAnalyzer::ExprView SemanticAnalyzer::expr_view(const syntax::ExprId expr_id) const noexcept {
    ExprView view;
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->module_.exprs.size()) {
        return view;
    }

    view.kind = this->module_.exprs.kind(expr_id.value);
    view.range = this->module_.exprs.range(expr_id.value);
    if (const syntax::LiteralExprPayload* const literal = this->module_.exprs.literal_payload(expr_id.value);
        literal != nullptr) {
        view.text = literal->text;
        return view;
    }
    if (const syntax::CastExprPayload* const cast = this->module_.exprs.cast_payload(expr_id.value);
        cast != nullptr) {
        view.cast_type = cast->type;
        view.cast_expr = cast->expr;
        return view;
    }

    switch (view.kind) {
    case syntax::ExprKind::name: {
        const syntax::NameExprPayload& payload = *this->module_.exprs.name_payload(expr_id.value);
        view.scope_name = payload.scope_name;
        view.scope_name_id = payload.scope_name_id;
        view.scope_range = payload.scope_range;
        view.text = payload.text;
        view.text_id = payload.text_id;
        view.type_args = readonly_span(payload.type_args);
        break;
    }
    case syntax::ExprKind::generic_apply: {
        const syntax::GenericApplyExprPayload& payload = *this->module_.exprs.generic_apply_payload(expr_id.value);
        view.callee = payload.callee;
        view.type_args = readonly_span(payload.type_args);
        break;
    }
    case syntax::ExprKind::unary:
    case syntax::ExprKind::try_expr: {
        const syntax::UnaryExprPayload& payload = *this->module_.exprs.unary_payload(expr_id.value);
        view.unary_op = payload.op;
        view.unary_operand = payload.operand;
        break;
    }
    case syntax::ExprKind::binary: {
        const syntax::BinaryExprPayload& payload = *this->module_.exprs.binary_payload(expr_id.value);
        view.binary_op = payload.op;
        view.binary_lhs = payload.lhs;
        view.binary_rhs = payload.rhs;
        break;
    }
    case syntax::ExprKind::call:
    case syntax::ExprKind::str_from_bytes_unchecked: {
        const syntax::CallExprPayload& payload = *this->module_.exprs.call_payload(expr_id.value);
        view.callee = payload.callee;
        view.args = readonly_span(payload.args);
        break;
    }
    case syntax::ExprKind::if_expr: {
        const syntax::IfExprPayload& payload = *this->module_.exprs.if_payload(expr_id.value);
        view.condition = payload.condition;
        view.condition_pattern = payload.condition_pattern;
        view.then_expr = payload.then_expr;
        view.else_expr = payload.else_expr;
        break;
    }
    case syntax::ExprKind::block_expr:
    case syntax::ExprKind::unsafe_block: {
        const syntax::BlockExprPayload& payload = *this->module_.exprs.block_payload(expr_id.value);
        view.block = payload.block;
        view.block_result = payload.result;
        break;
    }
    case syntax::ExprKind::match_expr: {
        const syntax::MatchExprPayload& payload = *this->module_.exprs.match_payload(expr_id.value);
        view.match_value = payload.value;
        view.match_arms = readonly_span(payload.arms);
        break;
    }
    case syntax::ExprKind::array_literal: {
        const syntax::ArrayExprPayload& payload = *this->module_.exprs.array_payload(expr_id.value);
        view.array_elements = readonly_span(payload.elements);
        view.array_repeat_value = payload.repeat_value;
        view.array_repeat_count = payload.repeat_count;
        break;
    }
    case syntax::ExprKind::tuple_literal: {
        const std::vector<syntax::ExprId>& payload = *this->module_.exprs.tuple_elements(expr_id.value);
        view.tuple_elements = readonly_span(payload);
        break;
    }
    case syntax::ExprKind::postfix_chain: {
        const syntax::PostfixChainExprPayload& payload = *this->module_.exprs.postfix_chain_payload(expr_id.value);
        view.postfix_base = payload.base;
        view.postfix_ops = readonly_span(payload.ops);
        break;
    }
    case syntax::ExprKind::field: {
        const syntax::FieldExprPayload& payload = *this->module_.exprs.field_payload(expr_id.value);
        view.object = payload.object;
        view.field_name = payload.field_name;
        view.field_name_id = payload.field_name_id;
        break;
    }
    case syntax::ExprKind::index: {
        const syntax::IndexExprPayload& payload = *this->module_.exprs.index_payload(expr_id.value);
        view.object = payload.object;
        view.index = payload.index;
        break;
    }
    case syntax::ExprKind::slice: {
        const syntax::SliceExprPayload& payload = *this->module_.exprs.slice_payload(expr_id.value);
        view.object = payload.object;
        view.slice_start = payload.start;
        view.slice_end = payload.end;
        break;
    }
    case syntax::ExprKind::struct_literal: {
        const syntax::StructLiteralExprPayload& payload = *this->module_.exprs.struct_literal_payload(expr_id.value);
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
    case syntax::ExprKind::str_data:
    case syntax::ExprKind::str_byte_len:
    case syntax::ExprKind::str_is_valid_utf8:
    case syntax::ExprKind::str_from_utf8_checked:
        break;
    }
    return view;
}

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id) {
    return this->analyze_expr(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id, const TypeHandle expected_type) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->module_.exprs.size()) {
        return INVALID_TYPE_HANDLE;
    }

    const TypeHandle cached_type = this->cached_expr_type_for_expected(expr_id, expected_type);
    if (is_valid(cached_type)) {
        return cached_type;
    }
    const TypeHandle analyzed = this->analyze_expr(expr_id, this->expr_view(expr_id), expected_type);
    this->record_expr_expected_type(expr_id, expected_type);
    return analyzed;
}

TypeHandle SemanticAnalyzer::analyze_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    switch (expr.kind) {
    case syntax::ExprKind::integer_literal:
        return this->analyze_integer_literal(expr_id, expr.text, expr.range, expected_type);
    case syntax::ExprKind::float_literal:
        return this->analyze_float_literal(expr_id, expr.text, expr.range, expected_type);
    case syntax::ExprKind::bool_literal:
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
    case syntax::ExprKind::null_literal:
        if (this->checked_.types.is_pointer(expected_type)) {
            this->record_coercion(expr_id, INVALID_TYPE_HANDLE, expected_type, CoercionKind::null_to_pointer);
            return this->record_expr_type(expr_id, expected_type);
        }
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    case syntax::ExprKind::string_literal:
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
    case syntax::ExprKind::raw_string_literal:
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
    case syntax::ExprKind::c_string_literal:
        return this->record_expr_type(
            expr_id,
            this->checked_.types.pointer(PointerMutability::const_, this->checked_.types.builtin(BuiltinType::u8))
        );
    case syntax::ExprKind::byte_string_literal: {
        const base::StringLiteralDecode decoded =
            base::decode_string_literal(expr.text, base::StringLiteralKind::byte_string);
        for (const base::StringLiteralError& error : decoded.errors) {
            this->report(
                base::SourceRange {
                    expr.range.source,
                    expr.range.begin + error.begin,
                    expr.range.begin + error.end,
                },
                error.message
            );
        }
        return this->record_expr_type(
            expr_id,
            this->checked_.types.array(
                static_cast<base::u64>(decoded.decoded.size()),
                this->checked_.types.builtin(BuiltinType::u8)
            )
        );
    }
    case syntax::ExprKind::byte_literal:
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::u8));
    case syntax::ExprKind::char_literal:
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::char_));
    case syntax::ExprKind::name:
        return this->analyze_name_expr(expr_id, expr);
    case syntax::ExprKind::generic_apply:
        return this->analyze_generic_apply_expr(expr_id, expr);
    case syntax::ExprKind::call:
        return this->analyze_call_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::try_expr:
        return this->analyze_try_expr(expr_id, expr);
    case syntax::ExprKind::if_expr:
        return this->analyze_if_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::block_expr:
        return this->analyze_block_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::unsafe_block:
        return this->analyze_unsafe_block_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::match_expr:
        return this->analyze_match_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::array_literal:
        return this->analyze_array_literal_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::tuple_literal:
        return this->analyze_tuple_literal_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::postfix_chain:
        return this->analyze_postfix_chain_expr(expr_id, expected_type);
    case syntax::ExprKind::unary:
        return this->analyze_unary_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::binary:
        return this->analyze_binary_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::field:
        return this->analyze_field_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::index:
        return this->analyze_index_expr(expr_id, expr);
    case syntax::ExprKind::slice:
        return this->analyze_slice_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::struct_literal:
        return this->analyze_struct_literal_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::cast:
    case syntax::ExprKind::pcast:
    case syntax::ExprKind::bcast:
        return this->analyze_cast_expr(expr_id, expr);
    case syntax::ExprKind::size_of:
    case syntax::ExprKind::align_of:
        return this->analyze_size_or_align_expr(expr_id, expr);
    case syntax::ExprKind::ptr_addr:
        return this->analyze_ptr_addr_expr(expr_id, expr);
    case syntax::ExprKind::paddr:
        return this->analyze_paddr_expr(expr_id, expr);
    case syntax::ExprKind::str_data:
    case syntax::ExprKind::str_byte_len:
        return this->analyze_str_projection_expr(expr_id, expr);
    case syntax::ExprKind::str_is_valid_utf8:
    case syntax::ExprKind::str_from_utf8_checked:
        return this->analyze_str_utf8_slice_expr(expr_id, expr);
    case syntax::ExprKind::str_from_bytes_unchecked:
        return this->analyze_str_from_bytes_unchecked_expr(expr_id, expr);
    case syntax::ExprKind::invalid:
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_name_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
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

bool SemanticAnalyzer::record_no_payload_enum_case_expr(
    const syntax::ExprId expr_id,
    const EnumCaseInfo& enum_case,
    const base::SourceRange range
) {
    if (is_valid(enum_case.payload_type)) {
        this->report(
            range,
            sema_enum_payload_constructor_call_message(enum_case_display_name(this->checked_.types, enum_case))
        );
        static_cast<void>(this->record_expr_type(expr_id, INVALID_TYPE_HANDLE));
        return false;
    }
    this->record_expr_c_name(expr_id, enum_case.c_name);
    static_cast<void>(this->record_expr_type(expr_id, enum_case.type));
    return true;
}

TypeHandle SemanticAnalyzer::analyze_generic_apply_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    this->report(expr.range, std::string(SEMA_EXPLICIT_GENERIC_CALL_SYNTAX));
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_unary_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    if (expr.unary_op == syntax::UnaryOp::numeric_negate &&
        expr_is_kind(this->module_, expr.unary_operand, syntax::ExprKind::integer_literal)) {
        const SemanticAnalyzer::ExprView operand = this->expr_view(expr.unary_operand);
        const TypeHandle literal_type =
            this->analyze_negative_integer_literal(expr.unary_operand, operand.text, operand.range, expected_type);
        return this->record_expr_type(expr_id, literal_type);
    }

    if (expr.unary_op == syntax::UnaryOp::address_of ||
        expr.unary_op == syntax::UnaryOp::address_of_mut) {
        const PlaceInfo operand_place = this->analyze_place_info(expr.unary_operand, true);
        this->require_place_projection_safety(operand_place, expr.range);
        if (!operand_place.is_place) {
            this->report(expr.range, std::string(SEMA_ADDRESS_OF_PLACE));
        }
        if (expr.unary_op == syntax::UnaryOp::address_of_mut) {
            if (!operand_place.is_writable) {
                this->report(expr.range, std::string(SEMA_MUTABLE_REFERENCE_PLACE));
            }
            if (!this->is_valid_storage_type(operand_place.type)) {
                this->report(expr.range, std::string(SEMA_REFERENCE_STORAGE));
            }
            return this->record_expr_type(
                expr_id,
                this->checked_.types.reference(PointerMutability::mut, operand_place.type)
            );
        }
        if (!this->is_valid_storage_type(operand_place.type)) {
            this->report(expr.range, std::string(SEMA_REFERENCE_STORAGE));
        }
        return this->record_expr_type(
            expr_id,
            this->checked_.types.reference(PointerMutability::const_, operand_place.type)
        );
    }

    const TypeHandle operand_expected =
        ((expr.unary_op == syntax::UnaryOp::numeric_negate &&
          (this->checked_.types.is_integer(expected_type) || this->checked_.types.is_float(expected_type))) ||
         (expr.unary_op == syntax::UnaryOp::bitwise_not && this->checked_.types.is_integer(expected_type)))
            ? expected_type
            : INVALID_TYPE_HANDLE;
    const TypeHandle operand = this->analyze_expr(expr.unary_operand, operand_expected);
    if (expr.unary_op == syntax::UnaryOp::logical_not && !this->checked_.types.is_bool(operand)) {
        this->report(expr.range, std::string(SEMA_LOGICAL_NOT_BOOL));
    }
    if (expr.unary_op == syntax::UnaryOp::numeric_negate &&
        !is_signed_integer_type(this->checked_.types, operand) &&
        !this->checked_.types.is_float(operand)) {
        this->report(expr.range, std::string(SEMA_NUMERIC_UNARY_OPERAND));
    }
    if (expr.unary_op == syntax::UnaryOp::bitwise_not && !this->checked_.types.is_integer(operand)) {
        this->report(expr.range, std::string(SEMA_BITWISE_NOT_INTEGER));
    }
    if (expr.unary_op == syntax::UnaryOp::dereference) {
        if (this->checked_.types.is_pointer(operand)) {
            this->require_unsafe_context(expr.range, SEMA_UNSAFE_DEREF);
        }
        if (!is_pointer_or_reference(this->checked_.types, operand)) {
            this->report(expr.range, std::string(SEMA_DEREF_POINTER));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        const TypeHandle pointee = this->checked_.types.get(operand).pointee;
        if (!this->is_valid_storage_type(pointee)) {
            this->report(expr.range, std::string(SEMA_DEREF_STORAGE));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        return this->record_expr_type(expr_id, pointee);
    }
    return this->record_expr_type(expr_id, operand);
}

TypeHandle SemanticAnalyzer::analyze_binary_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    const TypeHandle operand_expected =
        binary_result_uses_operand_type(expr.binary_op) &&
        (this->checked_.types.is_integer(expected_type) || this->checked_.types.is_float(expected_type))
            ? expected_type
            : INVALID_TYPE_HANDLE;
    TypeHandle lhs = INVALID_TYPE_HANDLE;
    TypeHandle rhs = INVALID_TYPE_HANDLE;
    if (!is_valid(operand_expected) &&
        is_contextual_integer_expr(this->module_, expr.binary_lhs) &&
        !is_contextual_integer_expr(this->module_, expr.binary_rhs)) {
        rhs = this->analyze_expr(expr.binary_rhs);
        lhs = this->analyze_expr(expr.binary_lhs, rhs);
    } else {
        lhs = this->analyze_expr(expr.binary_lhs, operand_expected);
        rhs = this->analyze_expr(expr.binary_rhs, is_valid(lhs) ? lhs : operand_expected);
    }

    const bool is_equality =
        expr.binary_op == syntax::BinaryOp::equal ||
        expr.binary_op == syntax::BinaryOp::not_equal;
    const bool is_null_pointer_comparison =
        is_equality &&
        ((this->checked_.types.is_pointer(lhs) && this->is_null_literal(expr.binary_rhs)) ||
         (this->checked_.types.is_pointer(rhs) && this->is_null_literal(expr.binary_lhs)));
    if (!this->checked_.types.same(lhs, rhs) && !is_null_pointer_comparison) {
        this->report(expr.range, std::string(SEMA_BINARY_OPERANDS_SAME_TYPE));
    }

    const IntegerLiteralExpr rhs_integer_literal = integer_literal_expr(this->module_, expr.binary_rhs);
    if (syntax::is_valid(rhs_integer_literal.literal)) {
        base::u64 rhs_literal_value = 0;
        const std::string_view literal_text = expr_literal_text_or_empty(this->module_, rhs_integer_literal.literal);
        const base::SourceRange rhs_range =
            expr_range_or(this->module_, expr.binary_rhs, expr_range_or(this->module_, rhs_integer_literal.literal, expr.range));
        if (this->parse_integer_literal_text(literal_text, rhs_literal_value)) {
            if ((expr.binary_op == syntax::BinaryOp::div || expr.binary_op == syntax::BinaryOp::mod) &&
                this->checked_.types.is_integer(lhs) &&
                rhs_literal_value == 0) {
                this->report(
                    rhs_range,
                    expr.binary_op == syntax::BinaryOp::div
                        ? std::string(SEMA_INTEGER_DIVISION_BY_ZERO)
                        : std::string(SEMA_INTEGER_MODULO_BY_ZERO)
                );
            }
            if (expr.binary_op == syntax::BinaryOp::shl || expr.binary_op == syntax::BinaryOp::shr) {
                const base::u32 bit_width = integer_bit_width(this->checked_.types, lhs);
                if (rhs_integer_literal.negated && rhs_literal_value != 0) {
                    this->report(rhs_range, std::string(SEMA_SHIFT_NEGATIVE));
                } else if (!rhs_integer_literal.negated &&
                           bit_width != 0 &&
                           rhs_literal_value >= bit_width) {
                    this->report(rhs_range, std::string(SEMA_SHIFT_OUT_OF_RANGE));
                }
            }
        }
    }

    if ((expr.binary_op == syntax::BinaryOp::div || expr.binary_op == syntax::BinaryOp::mod) &&
        is_signed_integer_type(this->checked_.types, lhs)) {
        const IntegerLiteralExpr lhs_integer_literal = integer_literal_expr(this->module_, expr.binary_lhs);
        if (syntax::is_valid(lhs_integer_literal.literal) &&
            syntax::is_valid(rhs_integer_literal.literal)) {
            base::u64 lhs_literal_value = 0;
            base::u64 rhs_literal_value = 0;
            if (this->parse_integer_literal_text(
                    expr_literal_text_or_empty(this->module_, lhs_integer_literal.literal),
                    lhs_literal_value
                ) &&
                this->parse_integer_literal_text(
                    expr_literal_text_or_empty(this->module_, rhs_integer_literal.literal),
                    rhs_literal_value
                ) &&
                is_signed_min_integer_literal(
                    lhs_integer_literal,
                    lhs_literal_value,
                    integer_bit_width(this->checked_.types, lhs)
                ) &&
                rhs_integer_literal.negated &&
                rhs_literal_value == 1) {
                this->report(
                    expr.range,
                    expr.binary_op == syntax::BinaryOp::div
                        ? std::string(SEMA_SIGNED_DIVISION_OVERFLOW)
                        : std::string(SEMA_SIGNED_MODULO_OVERFLOW)
                );
            }
        }
    }

    switch (expr.binary_op) {
    case syntax::BinaryOp::less:
    case syntax::BinaryOp::less_equal:
    case syntax::BinaryOp::greater:
    case syntax::BinaryOp::greater_equal:
        if (is_valid(lhs) && this->checked_.types.get(lhs).kind == TypeKind::generic_param) {
            if (!this->generic_param_has_capability(lhs, CapabilityKind::ord)) {
                this->report(expr.range, sema_generic_comparison_operator_message(this->checked_.types.display_name(lhs)));
            }
            return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
        }
        if (!this->type_supports_ordering_operator(lhs)) {
            this->report(expr.range, std::string(SEMA_COMPARISON_NUMERIC));
        }
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
    case syntax::BinaryOp::equal:
    case syntax::BinaryOp::not_equal: {
        if (is_valid(lhs) && this->checked_.types.get(lhs).kind == TypeKind::generic_param) {
            if (!this->generic_param_has_capability(lhs, CapabilityKind::eq)) {
                this->report(expr.range, sema_generic_equality_operator_message(this->checked_.types.display_name(lhs)));
            }
        } else if (!this->type_supports_equality_operator(lhs) && !is_null_pointer_comparison) {
            this->report(expr.range, std::string(SEMA_EQUALITY_SCALAR));
        }
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
    }
    case syntax::BinaryOp::logical_and:
    case syntax::BinaryOp::logical_or:
        if (!this->checked_.types.is_bool(lhs) || !this->checked_.types.is_bool(rhs)) {
            this->report(expr.range, std::string(SEMA_LOGICAL_BOOL));
        }
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
    case syntax::BinaryOp::bit_and:
    case syntax::BinaryOp::bit_xor:
    case syntax::BinaryOp::bit_or:
    case syntax::BinaryOp::shl:
    case syntax::BinaryOp::shr:
    case syntax::BinaryOp::mod:
        if (is_valid(lhs) && this->checked_.types.get(lhs).kind == TypeKind::generic_param) {
            this->report(expr.range, sema_generic_integer_operator_message(this->checked_.types.display_name(lhs)));
            return this->record_expr_type(expr_id, lhs);
        }
        if (!this->checked_.types.is_integer(lhs)) {
            this->report(expr.range, std::string(SEMA_INTEGER_OPERATOR_INTEGER));
        }
        return this->record_expr_type(expr_id, lhs);
    default:
        if (is_valid(lhs) && this->checked_.types.get(lhs).kind == TypeKind::generic_param) {
            this->report(expr.range, sema_generic_operator_message(this->checked_.types.display_name(lhs)));
            return this->record_expr_type(expr_id, lhs);
        }
        if (!this->checked_.types.is_integer(lhs) && !this->checked_.types.is_float(lhs)) {
            this->report(expr.range, std::string(SEMA_BINARY_NUMERIC));
        }
        return this->record_expr_type(expr_id, lhs);
    }
}

TypeHandle SemanticAnalyzer::analyze_field_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle
) {
    if (syntax::is_valid(expr.object) &&
        expr.object.value < this->module_.exprs.size()) {
        const ModuleSelector module = this->resolve_module_selector(expr.object, false);
        if (syntax::is_valid(module.module)) {
            return this->analyze_module_member_expr(expr_id, module.module, expr);
        }
        if (module.failed_as_module_selector) {
            static_cast<void>(this->resolve_module_selector(expr.object, true));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        const bool object_is_plain_name = expr_name_is_unqualified(this->module_, expr.object);
        const TypeHandle enum_type = this->resolve_type_selector(expr.object, !object_is_plain_name);
        if (is_valid(enum_type) && this->checked_.types.get(enum_type).kind == TypeKind::enum_) {
            const EnumCaseInfo* enum_case =
                this->find_enum_case_by_type_and_case(enum_type, expr.field_name_id, expr.field_name);
            if (enum_case == nullptr) {
                this->report(
                    expr.range,
                    sema_unknown_scoped_enum_case_message(this->checked_.types.display_name(enum_type), expr.field_name)
                );
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            const bool recorded = this->record_no_payload_enum_case_expr(expr_id, *enum_case, expr.range);
            return recorded ? enum_case->type : INVALID_TYPE_HANDLE;
        }
        if (object_is_plain_name && is_valid(enum_type)) {
            this->report(expr.range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }

    const PlaceInfo place = this->analyze_place_info(expr_id, true);
    this->require_place_projection_safety(place, expr.range);
    return this->record_expr_type(expr_id, place.type);
}

TypeHandle SemanticAnalyzer::analyze_module_member_expr(
    const syntax::ExprId expr_id,
    const syntax::ModuleId module,
    const SemanticAnalyzer::ExprView& expr
) {
    if (const Symbol* symbol =
            this->find_symbol_in_module(module, expr.field_name_id, expr.field_name, expr.range, false);
        symbol != nullptr) {
        if (symbol->kind == SymbolKind::function) {
            this->record_expr_c_name(expr_id, symbol->c_name);
            return this->record_expr_type(expr_id, this->function_type_from_symbol(*symbol, expr.range));
        }
        this->record_expr_c_name(expr_id, symbol->c_name);
        return this->record_expr_type(expr_id, symbol->type);
    }
    if (is_valid(this->find_type_in_module(module, expr.field_name_id, expr.field_name, expr.range, false, false))) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (this->generic_type_template_exists_in_module(module, expr.field_name_id, expr.field_name)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    static_cast<void>(this->find_symbol_in_module(module, expr.field_name_id, expr.field_name, expr.range, true));
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_index_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    const PlaceInfo place = this->analyze_place_info(expr_id, true);
    this->require_place_projection_safety(place, expr.range);
    return this->record_expr_type(expr_id, place.type);
}

TypeHandle SemanticAnalyzer::analyze_slice_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    const TypeHandle usize_type = this->checked_.types.builtin(BuiltinType::usize);
    const auto analyze_bound = [&](const syntax::ExprId bound) {
        if (!syntax::is_valid(bound)) {
            return;
        }
        const TypeHandle bound_type = this->analyze_expr(bound, usize_type);
        if (!this->checked_.types.is_integer(bound_type)) {
            this->report(
                expr_range_or(this->module_, bound, expr.range),
                std::string(SEMA_SLICE_BOUND_INTEGER)
            );
        }
    };

    const TypeHandle object = this->analyze_expr(expr.object);
    analyze_bound(expr.slice_start);
    analyze_bound(expr.slice_end);
    if (!is_valid(object)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    TypeHandle element = INVALID_TYPE_HANDLE;
    PointerMutability mutability = PointerMutability::const_;
    if (this->checked_.types.is_array(object)) {
        element = this->checked_.types.get(object).array_element;
        mutability = this->is_writable_place(expr.object) ? PointerMutability::mut : PointerMutability::const_;
    } else if (this->checked_.types.is_slice(object)) {
        const TypeInfo& slice = this->checked_.types.get(object);
        element = slice.slice_element;
        mutability = slice.slice_mutability;
    } else if (this->checked_.types.is_str(object)) {
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
    } else {
        this->report(expr.range, std::string(SEMA_SLICE_ARRAY_OR_SLICE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (!this->is_valid_storage_type(element)) {
        this->report(expr.range, std::string(SEMA_SLICE_ELEMENT_STORAGE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle natural_slice = this->checked_.types.slice(mutability, element);
    if (this->checked_.types.is_slice(expected_type) &&
        this->can_assign(expected_type, natural_slice, syntax::INVALID_EXPR_ID)) {
        this->record_coercion(expr_id, natural_slice, expected_type, CoercionKind::slice_to_expected_slice);
        return this->record_expr_type(expr_id, expected_type);
    }
    return this->record_expr_type(expr_id, natural_slice);
}

TypeHandle SemanticAnalyzer::analyze_array_literal_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    const bool has_expected_array = this->checked_.types.is_array(expected_type);
    TypeHandle element_type = INVALID_TYPE_HANDLE;
    base::u64 expected_count = 0;
    if (is_valid(expected_type)) {
        if (!has_expected_array) {
            this->report(expr.range, std::string(SEMA_ARRAY_LITERAL_EXPECTED_TYPE));
        } else {
            const TypeInfo& expected = this->checked_.types.get(expected_type);
            element_type = expected.array_element;
            expected_count = expected.array_count;
        }
    }

    const bool is_repeat_literal =
        syntax::is_valid(expr.array_repeat_value) ||
        syntax::is_valid(expr.array_repeat_count);
    base::u64 literal_count = static_cast<base::u64>(expr.array_elements.size());
    bool count_known = !is_repeat_literal;
    if (is_repeat_literal) {
        count_known = false;
        const bool count_expr_valid =
            syntax::is_valid(expr.array_repeat_count) &&
            expr.array_repeat_count.value < this->module_.exprs.size();
        if (!count_expr_valid) {
            this->report(expr.range, std::string(SEMA_ARRAY_REPEAT_INTEGER));
        } else {
            const base::SourceRange count_range = this->module_.exprs.range(expr.array_repeat_count.value);
            if (this->module_.exprs.kind(expr.array_repeat_count.value) != syntax::ExprKind::integer_literal) {
                this->report(count_range, std::string(SEMA_ARRAY_REPEAT_INTEGER));
            } else if (!this->parse_integer_literal_text(
                           expr_literal_text_or_empty(this->module_, expr.array_repeat_count),
                           literal_count
                       )) {
                this->report(count_range, std::string(SEMA_ARRAY_REPEAT_OUT_OF_RANGE));
            } else {
                count_known = true;
            }
        }
    }

    if (has_expected_array && count_known && literal_count != expected_count) {
        this->report(
            expr.range,
            sema_array_literal_length_mismatch_message(expected_count, literal_count)
        );
    }

    if (!has_expected_array && !is_repeat_literal && expr.array_elements.empty()) {
        this->report(expr.range, std::string(SEMA_EMPTY_ARRAY_CONTEXT));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (!has_expected_array) {
        const syntax::ExprId first_value = is_repeat_literal
            ? expr.array_repeat_value
            : expr.array_elements.front();
        element_type = this->analyze_expr(first_value);
        if (!is_valid(element_type)) {
            this->report(expr.range, std::string(SEMA_ARRAY_ELEMENT_INFER));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }

    if (is_repeat_literal) {
        const TypeHandle actual = this->analyze_expr(expr.array_repeat_value, element_type);
        if (!this->can_assign(element_type, actual, expr.array_repeat_value)) {
            this->report(expr.range, std::string(SEMA_ARRAY_REPEAT_TYPE_MISMATCH));
        }
    } else {
        for (const syntax::ExprId element : expr.array_elements) {
            const TypeHandle actual = this->analyze_expr(element, element_type);
            if (!this->can_assign(element_type, actual, element)) {
                this->report(
                    expr_range_or(this->module_, element, expr.range),
                    std::string(SEMA_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH)
                );
            }
        }
    }

    if (!is_valid(element_type) || !count_known) {
        return this->record_expr_type(expr_id, has_expected_array ? expected_type : INVALID_TYPE_HANDLE);
    }
    const TypeHandle array_type = has_expected_array
        ? expected_type
        : this->checked_.types.array(literal_count, element_type);
    if (!this->is_valid_storage_type(array_type)) {
        this->report(expr.range, std::string(SEMA_ARRAY_LITERAL_STORAGE));
    }
    return this->record_expr_type(expr_id, array_type);
}

TypeHandle SemanticAnalyzer::analyze_tuple_literal_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    const bool has_expected_tuple = this->checked_.types.is_tuple(expected_type);
    std::vector<TypeHandle> element_types;
    if (is_valid(expected_type)) {
        if (!has_expected_tuple) {
            this->report(expr.range, std::string(SEMA_TUPLE_LITERAL_EXPECTED_TYPE));
        } else {
            const TypeInfo& expected = this->checked_.types.get(expected_type);
            if (expected.tuple_elements.size() != expr.tuple_elements.size()) {
                this->report(expr.range, std::string(SEMA_TUPLE_LITERAL_ARITY));
            }
            element_types.assign(expected.tuple_elements.begin(), expected.tuple_elements.end());
        }
    }

    if (!has_expected_tuple) {
        element_types.assign(expr.tuple_elements.size(), INVALID_TYPE_HANDLE);
    }

    for (base::usize i = 0; i < expr.tuple_elements.size(); ++i) {
        const TypeHandle expected_element = i < element_types.size()
            ? element_types[i]
            : INVALID_TYPE_HANDLE;
        const TypeHandle actual = this->analyze_expr(expr.tuple_elements[i], expected_element);
        if (i >= element_types.size()) {
            continue;
        }
        if (has_expected_tuple) {
            if (!this->can_assign(element_types[i], actual, expr.tuple_elements[i])) {
                this->report(
                    expr_range_or(this->module_, expr.tuple_elements[i], expr.range),
                    std::string(SEMA_TUPLE_LITERAL_ELEMENT_TYPE_MISMATCH)
                );
            }
        } else {
            element_types[i] = actual;
        }
    }

    if (!has_expected_tuple) {
        for (const TypeHandle element : element_types) {
            if (!is_valid(element)) {
                this->report(expr.range, std::string(SEMA_LOCAL_TYPE_INFER));
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
        }
    }

    const TypeHandle tuple_type = has_expected_tuple
        ? expected_type
        : this->checked_.types.tuple(element_types);
    if (is_valid(tuple_type) && !this->is_valid_storage_type(tuple_type)) {
        this->report(expr.range, std::string(SEMA_TUPLE_LITERAL_STORAGE));
    }
    return this->record_expr_type(expr_id, tuple_type);
}

TypeHandle SemanticAnalyzer::analyze_struct_literal_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle
) {
    TypeHandle struct_type = INVALID_TYPE_HANDLE;
    if (syntax::is_valid(expr.object)) {
        struct_type = this->resolve_type_selector(expr.object, true);
    } else {
        NamedTypeSelector selector;
        selector.module = syntax::INVALID_MODULE_ID;
        selector.name = expr.struct_name;
        selector.range = expr.range;
        selector.type_args.assign(expr.type_args.begin(), expr.type_args.end());
        selector.qualified = false;
        struct_type = this->resolve_named_type_selector_type(selector, false, true);
    }
    if (!is_valid(struct_type)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const StructInfo* info = this->find_struct(struct_type);
    if (info == nullptr || info->is_opaque) {
        this->report(expr.range, std::string(SEMA_STRUCT_LITERAL_TYPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    std::unordered_set<std::string> initialized_fields;
    for (const syntax::FieldInit& init : expr.field_inits) {
        if (!initialized_fields.insert(std::string(init.name)).second) {
            this->report(init.range, sema_duplicate_struct_literal_field_message(init.name));
            continue;
        }
        const StructFieldInfo* field_info = nullptr;
        for (const StructFieldInfo& field : info->fields) {
            if (field.name == init.name) {
                field_info = &field;
                break;
            }
        }
        if (field_info == nullptr) {
            this->report(init.range, sema_unknown_struct_literal_field_message(init.name));
            continue;
        }
        if (!this->can_access(info->module, field_info->visibility)) {
            this->report(init.range, sema_private_field_message(init.name));
            continue;
        }
        const TypeHandle actual = this->analyze_expr(init.value, field_info->type);
        if (!this->can_assign(field_info->type, actual, init.value)) {
            this->report(init.range, std::string(SEMA_STRUCT_LITERAL_FIELD_TYPE_MISMATCH));
        }
    }
    for (const StructFieldInfo& field : info->fields) {
        if (!initialized_fields.contains(field.name)) {
            this->report(expr.range, sema_struct_literal_missing_field_message(field.name));
        }
    }
    return this->record_expr_type(expr_id, struct_type);
}

TypeHandle SemanticAnalyzer::analyze_cast_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    if (expr.kind == syntax::ExprKind::pcast) {
        this->require_unsafe_context(expr.range, SEMA_UNSAFE_PTRCAST);
    } else if (expr.kind == syntax::ExprKind::bcast) {
        this->require_unsafe_context(expr.range, SEMA_UNSAFE_BITCAST);
    }
    const TypeHandle target = this->resolve_type(expr.cast_type);
    const TypeHandle source = expr.kind == syntax::ExprKind::pcast
        ? this->analyze_expr(expr.cast_expr, target)
        : this->analyze_expr(expr.cast_expr);
    if (!this->is_valid_cast(expr.kind, target, source)) {
        this->report(expr.range, std::string(SEMA_INVALID_CONVERSION));
    }
    return this->record_expr_type(expr_id, target);
}

TypeHandle SemanticAnalyzer::analyze_size_or_align_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    const TypeHandle queried = this->resolve_type(expr.cast_type);
    if (is_valid(queried) && this->checked_.types.get(queried).kind == TypeKind::generic_param) {
        if (!this->generic_param_has_capability(queried, CapabilityKind::sized)) {
            this->report(expr.range, std::string(SEMA_GENERIC_SIZEOF_ALIGNOF));
        }
    } else if (is_valid(queried) && this->checked_.types.get(queried).kind == TypeKind::opaque_struct) {
        this->report(expr.range, std::string(SEMA_OPAQUE_SIZEOF_ALIGNOF));
    } else if (is_valid(queried) && !this->is_valid_storage_type(queried)) {
        this->report(expr.range, std::string(SEMA_SIZEOF_ALIGNOF_STORAGE));
    }
    return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzer::analyze_ptr_addr_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    const TypeHandle value = this->analyze_expr(expr.cast_expr);
    if (!is_pointer_or_reference(this->checked_.types, value)) {
        this->report(expr.range, std::string(SEMA_PTRADDR_POINTER));
    }
    return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzer::analyze_paddr_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    this->require_unsafe_context(expr.range, SEMA_UNSAFE_PTRAT);
    const TypeHandle target = this->resolve_type(expr.cast_type);
    const TypeHandle address = this->analyze_expr(expr.cast_expr, this->checked_.types.builtin(BuiltinType::usize));
    if (!this->checked_.types.is_pointer(target)) {
        this->report(expr.range, std::string(SEMA_PTRAT_POINTER));
    }
    if (!this->checked_.types.is_integer(address)) {
        this->report(expr_range_or(this->module_, expr.cast_expr, expr.range), std::string(SEMA_PTRAT_INTEGER));
    }
    return this->record_expr_type(expr_id, target);
}

TypeHandle SemanticAnalyzer::analyze_str_projection_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    const TypeHandle value = this->analyze_expr(expr.cast_expr);
    if (!this->checked_.types.is_str(value)) {
        this->report(
            expr.range,
            expr.kind == syntax::ExprKind::str_data ? std::string(SEMA_STRPTR_STR) : std::string(SEMA_STRBLEN_STR)
        );
    }
    if (expr.kind == syntax::ExprKind::str_data) {
        return this->record_expr_type(
            expr_id,
            this->checked_.types.pointer(PointerMutability::const_, this->checked_.types.builtin(BuiltinType::u8))
        );
    }
    return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzer::analyze_str_utf8_slice_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    const TypeHandle bytes = this->analyze_expr(expr.cast_expr);
    if (!is_u8_slice(this->checked_.types, bytes)) {
        this->report(expr.range, std::string(SEMA_STR_UTF8_SLICE));
    }
    if (expr.kind == syntax::ExprKind::str_is_valid_utf8) {
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
    }
    return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
}

TypeHandle SemanticAnalyzer::analyze_str_from_bytes_unchecked_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr
) {
    this->require_unsafe_context(expr.range, SEMA_UNSAFE_STRRAW);
    if (expr.args.size() != 2) {
        this->report(expr.range, std::string(SEMA_STRRAW_ARITY));
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
    }
    const TypeHandle data = this->analyze_expr(expr.args[0]);
    const TypeHandle len = this->analyze_expr(expr.args[1], this->checked_.types.builtin(BuiltinType::usize));
    if (!is_const_u8_pointer(this->checked_.types, data)) {
        this->report(expr_range_or(this->module_, expr.args[0], expr.range), std::string(SEMA_STRRAW_DATA_POINTER));
    }
    if (!this->checked_.types.is_integer(len)) {
        this->report(expr_range_or(this->module_, expr.args[1], expr.range), std::string(SEMA_STRRAW_LENGTH_INTEGER));
    }
    return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
}

TypeHandle SemanticAnalyzer::analyze_try_expr(const syntax::ExprId expr_id, const SemanticAnalyzer::ExprView& expr) {
    if (this->in_const_initializer_) {
        this->report(expr.range, std::string(SEMA_TRY_CONST_INITIALIZER));
    }

    const TypeHandle source_type = this->analyze_expr(expr.unary_operand);
    const TryShape source_shape = this->classify_try_shape(source_type);
    if (source_shape.kind == TryShape::Kind::malformed_result) {
        this->report(expr.range, std::string(SEMA_TRY_RESULT_SHAPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (source_shape.kind == TryShape::Kind::malformed_option) {
        this->report(expr.range, std::string(SEMA_TRY_OPTION_SHAPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (source_shape.kind == TryShape::Kind::result) {
        const TryShape return_shape = this->classify_try_shape(this->current_function_return_type_);
        if (return_shape.kind != TryShape::Kind::result || return_shape.failure_case == nullptr) {
            this->report(expr.range, std::string(SEMA_TRY_RESULT_RETURN));
            return this->record_expr_type(expr_id, source_shape.success_case->payload_type);
        }
        if (!this->checked_.types.same(return_shape.failure_case->payload_type, source_shape.failure_case->payload_type)) {
            this->report(expr.range, std::string(SEMA_TRY_RESULT_ERR_PAYLOAD));
        }
        return this->record_expr_type(expr_id, source_shape.success_case->payload_type);
    }

    if (source_shape.kind == TryShape::Kind::option) {
        const TryShape return_shape = this->classify_try_shape(this->current_function_return_type_);
        if (return_shape.kind != TryShape::Kind::option) {
            this->report(expr.range, std::string(SEMA_TRY_OPTION_RETURN));
            return this->record_expr_type(expr_id, source_shape.success_case->payload_type);
        }
        return this->record_expr_type(expr_id, source_shape.success_case->payload_type);
    }

    this->report(expr.range, std::string(SEMA_TRY_SHAPE));
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_if_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    if (this->in_const_initializer_) {
        this->report(expr.range, std::string(SEMA_IF_EXPR_CONST_INITIALIZER));
    }
    const TypeHandle condition = this->analyze_expr(expr.condition);
    if (!syntax::is_valid(expr.condition_pattern) && !this->checked_.types.is_bool(condition)) {
        this->report(expr_range_or(this->module_, expr.condition, expr.range), std::string(SEMA_IF_EXPR_CONDITION_BOOL));
    }
    const auto analyze_then_branch = [&](const TypeHandle expected) {
        if (!syntax::is_valid(expr.condition_pattern)) {
            return this->analyze_expr(expr.then_expr, expected);
        }
        std::vector<PatternBinding> bindings;
        static_cast<void>(this->analyze_pattern(expr.condition_pattern, condition, bindings));
        this->symbols_.push_scope(bindings.size());
        this->define_pattern_bindings(bindings, false);
        const TypeHandle result = this->analyze_expr(expr.then_expr, expected);
        this->symbols_.pop_scope();
        return result;
    };

    TypeHandle then_type = INVALID_TYPE_HANDLE;
    TypeHandle else_type = INVALID_TYPE_HANDLE;
    if (!is_valid(expected_type) &&
        this->is_null_result_expr(expr.then_expr) &&
        !this->is_null_result_expr(expr.else_expr)) {
        else_type = this->analyze_expr(expr.else_expr);
        then_type = analyze_then_branch(else_type);
    } else {
        then_type = analyze_then_branch(expected_type);
        else_type = this->analyze_expr(expr.else_expr, is_valid(then_type) ? then_type : expected_type);
        if (!is_valid(then_type) &&
            this->checked_.types.is_pointer(else_type) &&
            this->is_null_result_expr(expr.then_expr)) {
            then_type = analyze_then_branch(else_type);
        }
        if (!is_valid(else_type) &&
            this->checked_.types.is_pointer(then_type) &&
            this->is_null_result_expr(expr.else_expr)) {
            else_type = this->analyze_expr(expr.else_expr, then_type);
        }
    }
    if (!this->checked_.types.same(then_type, else_type)) {
        this->report(expr.range, std::string(SEMA_IF_EXPR_BRANCH_TYPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (is_valid(then_type) && this->checked_.types.is_void(then_type)) {
        this->report(expr.range, std::string(SEMA_IF_EXPR_VOID));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return this->record_expr_type(expr_id, then_type);
}

TypeHandle SemanticAnalyzer::analyze_block_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    if (this->in_const_initializer_) {
        this->report(expr.range, std::string(SEMA_BLOCK_EXPR_CONST_INITIALIZER));
    }
    if (!syntax::is_valid(expr.block_result)) {
        this->report(expr.range, std::string(SEMA_BLOCK_EXPR_FINAL));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    this->symbols_.push_scope();
    this->analyze_block_statements(expr.block, this->current_function_return_type_, this->current_return_inference_);
    if (!this->block_may_fallthrough(expr.block)) {
        this->report(expr.range, std::string(SEMA_BLOCK_EXPR_UNREACHABLE));
    }
    const TypeHandle result = this->analyze_expr(expr.block_result, expected_type);
    this->symbols_.pop_scope();

    if (!is_valid(result)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (this->checked_.types.is_void(result)) {
        this->report(expr.range, std::string(SEMA_BLOCK_EXPR_VOID));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return this->record_expr_type(expr_id, result);
}

TypeHandle SemanticAnalyzer::analyze_unsafe_block_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    if (this->in_const_initializer_) {
        this->report(expr.range, std::string(SEMA_UNSAFE_BLOCK_CONST_INITIALIZER));
    }

    this->symbols_.push_scope();
    const int previous_unsafe_depth = this->unsafe_context_depth_;
    ++this->unsafe_context_depth_;
    this->analyze_block_statements(expr.block, this->current_function_return_type_, this->current_return_inference_);
    TypeHandle result = this->checked_.types.builtin(BuiltinType::void_);
    if (syntax::is_valid(expr.block_result)) {
        if (!this->block_may_fallthrough(expr.block)) {
            this->report(expr.range, std::string(SEMA_BLOCK_EXPR_UNREACHABLE));
        }
        result = this->analyze_expr(expr.block_result, expected_type);
    }
    this->unsafe_context_depth_ = previous_unsafe_depth;
    this->symbols_.pop_scope();

    if (!is_valid(result)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (syntax::is_valid(expr.block_result) && this->checked_.types.is_void(result)) {
        this->report(expr.range, std::string(SEMA_BLOCK_EXPR_VOID));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return this->record_expr_type(expr_id, result);
}

} // namespace aurex::sema
