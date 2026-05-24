#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include <sema/internal/sema_core.hpp>
#include <sema/internal/sema_expression_analyzer.hpp>

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

template <typename T, typename Allocator>
[[nodiscard]] std::span<const T> readonly_span(const std::vector<T, Allocator>& values) noexcept
{
    return {values.data(), values.size()};
}

[[nodiscard]] bool binary_result_uses_operand_type(const syntax::BinaryOp op) noexcept
{
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

[[nodiscard]] bool is_contextual_integer_expr(const syntax::AstModule& module, const syntax::ExprId candidate)
{
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
    const syntax::AstModule& module, const syntax::ExprId expr, const syntax::ExprKind kind) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size() && module.exprs.kind(expr.value) == kind;
}

[[nodiscard]] std::string_view expr_literal_text_or_empty(
    const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return {};
    }
    const syntax::LiteralExprPayload* const literal = module.exprs.literal_payload(expr.value);
    return literal == nullptr ? std::string_view{} : literal->text;
}

[[nodiscard]] bool expr_name_is_unqualified(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return false;
    }
    const syntax::NameExprPayload* const name = module.exprs.name_payload(expr.value);
    return name != nullptr && name->scope_name.empty();
}

[[nodiscard]] syntax::ExprId unary_operand_or_invalid(
    const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return syntax::INVALID_EXPR_ID;
    }
    const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(expr.value);
    return unary == nullptr ? syntax::INVALID_EXPR_ID : unary->operand;
}

[[nodiscard]] bool unary_expr_has_op(
    const syntax::AstModule& module, const syntax::ExprId expr, const syntax::UnaryOp op) noexcept
{
    if (!syntax::is_valid(expr) || expr.value >= module.exprs.size()) {
        return false;
    }
    const syntax::UnaryExprPayload* const unary = module.exprs.unary_payload(expr.value);
    return unary != nullptr && unary->op == op;
}

[[nodiscard]] IntegerLiteralExpr integer_literal_expr(
    const syntax::AstModule& module, const syntax::ExprId candidate) noexcept
{
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
    const IntegerLiteralExpr literal, const base::u64 value, const base::u32 bit_width) noexcept
{
    if (!literal.negated || bit_width == 0) {
        return false;
    }
    const base::u64 min_magnitude = bit_width >= SEMA_I64_BIT_WIDTH
        ? (base::u64{1} << SEMA_I64_SIGN_BIT_INDEX)
        : (base::u64{1} << (bit_width - SEMA_SIGN_BIT_OFFSET));
    return value == min_magnitude;
}

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module, const syntax::ExprId expr, const base::SourceRange& fallback) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size() ? module.exprs.range(expr.value) : fallback;
}

[[nodiscard]] bool is_pointer_or_reference(const TypeTable& types, const TypeHandle type) noexcept
{
    return types.is_pointer(type) || types.is_reference(type);
}

[[nodiscard]] bool is_signed_integer_type(const TypeTable& types, const TypeHandle type) noexcept
{
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

[[nodiscard]] base::u32 integer_bit_width(const TypeTable& types, const TypeHandle type) noexcept
{
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

TypeHandle SemanticAnalyzerCore::analyze_unary_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (expr.unary_op == syntax::UnaryOp::numeric_negate
        && expr_is_kind(this->ctx_.module, expr.unary_operand, syntax::ExprKind::integer_literal)) {
        const SemanticAnalyzerCore::ExprView operand = this->expr_view(expr.unary_operand);
        const TypeHandle literal_type =
            this->analyze_negative_integer_literal(expr.unary_operand, operand.text, operand.range, expected_type);
        const TypeHandle intrinsic_type = this->cached_expr_intrinsic_type(expr.unary_operand);
        return this->record_expr_types(expr_id, is_valid(intrinsic_type) ? intrinsic_type : literal_type, literal_type);
    }

    if (expr.unary_op == syntax::UnaryOp::address_of || expr.unary_op == syntax::UnaryOp::address_of_mut) {
        const PlaceInfo operand_place = this->analyze_place_info(expr.unary_operand, true);
        this->require_place_projection_safety(operand_place, expr.range);
        if (!operand_place.is_place) {
            this->report_general(expr.range, std::string(SEMA_ADDRESS_OF_PLACE));
        }
        if (expr.unary_op == syntax::UnaryOp::address_of_mut) {
            if (!operand_place.is_writable) {
                this->report_general(expr.range, std::string(SEMA_MUTABLE_REFERENCE_PLACE));
            }
            if (!this->is_valid_storage_type(operand_place.type)) {
                this->report_general(expr.range, std::string(SEMA_REFERENCE_STORAGE));
            }
            const TypeHandle reference_type =
                this->state_.checked.types.reference(PointerMutability::mut, operand_place.type);
            return this->record_expr_types(expr_id, reference_type, reference_type);
        }
        if (!this->is_valid_storage_type(operand_place.type)) {
            this->report_general(expr.range, std::string(SEMA_REFERENCE_STORAGE));
        }
        const TypeHandle reference_type =
            this->state_.checked.types.reference(PointerMutability::const_, operand_place.type);
        return this->record_expr_types(expr_id, reference_type, reference_type);
    }

    const TypeHandle operand_expected =
        ((expr.unary_op == syntax::UnaryOp::numeric_negate
             && (this->state_.checked.types.is_integer(expected_type)
                 || this->state_.checked.types.is_float(expected_type)))
            || (expr.unary_op == syntax::UnaryOp::bitwise_not && this->state_.checked.types.is_integer(expected_type)))
        ? expected_type
        : INVALID_TYPE_HANDLE;
    const TypeHandle operand = this->analyze_expr(expr.unary_operand, operand_expected);
    if (expr.unary_op == syntax::UnaryOp::logical_not && !this->state_.checked.types.is_bool(operand)) {
        this->report_general(expr.range, std::string(SEMA_LOGICAL_NOT_BOOL));
    }
    if (expr.unary_op == syntax::UnaryOp::numeric_negate && !is_signed_integer_type(this->state_.checked.types, operand)
        && !this->state_.checked.types.is_float(operand)) {
        this->report_general(expr.range, std::string(SEMA_NUMERIC_UNARY_OPERAND));
    }
    if (expr.unary_op == syntax::UnaryOp::bitwise_not && !this->state_.checked.types.is_integer(operand)) {
        this->report_general(expr.range, std::string(SEMA_BITWISE_NOT_INTEGER));
    }
    if (expr.unary_op == syntax::UnaryOp::dereference) {
        if (this->state_.checked.types.is_pointer(operand)) {
            this->require_unsafe_context(expr.range, SEMA_UNSAFE_DEREF);
        }
        if (!is_pointer_or_reference(this->state_.checked.types, operand)) {
            this->report_general(expr.range, std::string(SEMA_DEREF_POINTER));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        const TypeHandle pointee = this->state_.checked.types.get(operand).pointee;
        if (!this->is_valid_storage_type(pointee)) {
            this->report_general(expr.range, std::string(SEMA_DEREF_STORAGE));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        return this->record_expr_types(expr_id, pointee, pointee);
    }
    const TypeHandle operand_intrinsic = this->cached_expr_intrinsic_type(expr.unary_operand);
    return this->record_expr_types(expr_id, is_valid(operand_intrinsic) ? operand_intrinsic : operand, operand);
}

TypeHandle SemanticAnalyzerCore::analyze_binary_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    const BinaryExprAnalysis analysis = this->analyze_binary_operands(expr, expected_type);
    this->diagnose_binary_operand_mismatch(expr, analysis);
    this->diagnose_binary_literal_hazards(expr, analysis.lhs);
    return this->record_binary_operator_expr(expr_id, expr, analysis);
}

SemanticAnalyzerCore::BinaryExprAnalysis SemanticAnalyzerCore::analyze_binary_operands(
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    BinaryExprAnalysis analysis;
    analysis.operand_expected = binary_result_uses_operand_type(expr.binary_op)
            && (this->state_.checked.types.is_integer(expected_type)
                || this->state_.checked.types.is_float(expected_type))
        ? expected_type
        : INVALID_TYPE_HANDLE;
    if (!is_valid(analysis.operand_expected) && is_contextual_integer_expr(this->ctx_.module, expr.binary_lhs)
        && !is_contextual_integer_expr(this->ctx_.module, expr.binary_rhs)) {
        analysis.rhs = this->analyze_expr(expr.binary_rhs);
        analysis.lhs = this->analyze_expr(expr.binary_lhs, analysis.rhs);
    } else {
        analysis.lhs = this->analyze_expr(expr.binary_lhs, analysis.operand_expected);
        analysis.rhs =
            this->analyze_expr(expr.binary_rhs, is_valid(analysis.lhs) ? analysis.lhs : analysis.operand_expected);
    }

    const bool is_equality = expr.binary_op == syntax::BinaryOp::equal || expr.binary_op == syntax::BinaryOp::not_equal;
    analysis.null_pointer_comparison = is_equality
        && ((this->state_.checked.types.is_pointer(analysis.lhs) && this->is_null_literal(expr.binary_rhs))
            || (this->state_.checked.types.is_pointer(analysis.rhs) && this->is_null_literal(expr.binary_lhs)));

    analysis.result_intrinsic = analysis.lhs;
    if (is_valid(analysis.operand_expected)) {
        const TypeHandle lhs_intrinsic = this->cached_expr_intrinsic_type(expr.binary_lhs);
        const TypeHandle rhs_intrinsic = this->cached_expr_intrinsic_type(expr.binary_rhs);
        if (is_valid(lhs_intrinsic)
            && (!is_valid(rhs_intrinsic) || this->state_.checked.types.same(lhs_intrinsic, rhs_intrinsic))) {
            analysis.result_intrinsic = lhs_intrinsic;
        }
    }

    return analysis;
}

void SemanticAnalyzerCore::diagnose_binary_operand_mismatch(
    const SemanticAnalyzerCore::ExprView& expr, const SemanticAnalyzerCore::BinaryExprAnalysis& analysis) const
{
    if (!this->state_.checked.types.same(analysis.lhs, analysis.rhs) && !analysis.null_pointer_comparison) {
        this->report_general(expr.range, std::string(SEMA_BINARY_OPERANDS_SAME_TYPE));
    }
}

void SemanticAnalyzerCore::diagnose_binary_literal_hazards(
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs) const
{
    this->diagnose_binary_rhs_literal_hazards(expr, lhs);
    this->diagnose_signed_binary_literal_overflow(expr, lhs);
}

void SemanticAnalyzerCore::diagnose_binary_rhs_literal_hazards(
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs) const
{
    const IntegerLiteralExpr rhs_integer_literal = integer_literal_expr(this->ctx_.module, expr.binary_rhs);
    if (!syntax::is_valid(rhs_integer_literal.literal)) {
        return;
    }

    base::u64 rhs_literal_value = 0;
    const std::string_view literal_text = expr_literal_text_or_empty(this->ctx_.module, rhs_integer_literal.literal);
    const base::SourceRange rhs_range = expr_range_or(
        this->ctx_.module, expr.binary_rhs, expr_range_or(this->ctx_.module, rhs_integer_literal.literal, expr.range));
    if (!this->parse_integer_literal_text(literal_text, rhs_literal_value)) {
        return;
    }

    if ((expr.binary_op == syntax::BinaryOp::div || expr.binary_op == syntax::BinaryOp::mod)
        && this->state_.checked.types.is_integer(lhs) && rhs_literal_value == 0) {
        this->report_general(rhs_range,
            expr.binary_op == syntax::BinaryOp::div ? std::string(SEMA_INTEGER_DIVISION_BY_ZERO)
                                                    : std::string(SEMA_INTEGER_MODULO_BY_ZERO));
    }

    if (expr.binary_op != syntax::BinaryOp::shl && expr.binary_op != syntax::BinaryOp::shr) {
        return;
    }

    const base::u32 bit_width = integer_bit_width(this->state_.checked.types, lhs);
    if (rhs_integer_literal.negated && rhs_literal_value != 0) {
        this->report_general(rhs_range, std::string(SEMA_SHIFT_NEGATIVE));
    } else if (!rhs_integer_literal.negated && bit_width != 0 && rhs_literal_value >= bit_width) {
        this->report_general(rhs_range, std::string(SEMA_SHIFT_OUT_OF_RANGE));
    }
}

void SemanticAnalyzerCore::diagnose_signed_binary_literal_overflow(
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs) const
{
    if ((expr.binary_op != syntax::BinaryOp::div && expr.binary_op != syntax::BinaryOp::mod)
        || !is_signed_integer_type(this->state_.checked.types, lhs)) {
        return;
    }

    const IntegerLiteralExpr lhs_integer_literal = integer_literal_expr(this->ctx_.module, expr.binary_lhs);
    const IntegerLiteralExpr rhs_integer_literal = integer_literal_expr(this->ctx_.module, expr.binary_rhs);
    if (!syntax::is_valid(lhs_integer_literal.literal) || !syntax::is_valid(rhs_integer_literal.literal)) {
        return;
    }

    base::u64 lhs_literal_value = 0;
    base::u64 rhs_literal_value = 0;
    if (this->parse_integer_literal_text(
            expr_literal_text_or_empty(this->ctx_.module, lhs_integer_literal.literal), lhs_literal_value)
        && this->parse_integer_literal_text(
            expr_literal_text_or_empty(this->ctx_.module, rhs_integer_literal.literal), rhs_literal_value)
        && is_signed_min_integer_literal(
            lhs_integer_literal, lhs_literal_value, integer_bit_width(this->state_.checked.types, lhs))
        && rhs_integer_literal.negated && rhs_literal_value == 1) {
        this->report_general(expr.range,
            expr.binary_op == syntax::BinaryOp::div ? std::string(SEMA_SIGNED_DIVISION_OVERFLOW)
                                                    : std::string(SEMA_SIGNED_MODULO_OVERFLOW));
    }
}

TypeHandle SemanticAnalyzerCore::record_binary_operator_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const SemanticAnalyzerCore::BinaryExprAnalysis& analysis)
{
    switch (expr.binary_op) {
        case syntax::BinaryOp::less:
        case syntax::BinaryOp::less_equal:
        case syntax::BinaryOp::greater:
        case syntax::BinaryOp::greater_equal:
            return this->record_ordering_binary_expr(expr_id, expr, analysis.lhs);
        case syntax::BinaryOp::equal:
        case syntax::BinaryOp::not_equal:
            return this->record_equality_binary_expr(expr_id, expr, analysis.lhs, analysis.null_pointer_comparison);
        case syntax::BinaryOp::logical_and:
        case syntax::BinaryOp::logical_or:
            return this->record_logical_binary_expr(expr_id, expr, analysis.lhs, analysis.rhs);
        case syntax::BinaryOp::bit_and:
        case syntax::BinaryOp::bit_xor:
        case syntax::BinaryOp::bit_or:
        case syntax::BinaryOp::shl:
        case syntax::BinaryOp::shr:
        case syntax::BinaryOp::mod:
            return this->record_integer_binary_expr(expr_id, expr, analysis.result_intrinsic, analysis.lhs);
        default:
            return this->record_numeric_binary_expr(expr_id, expr, analysis.result_intrinsic, analysis.lhs);
    }
}

TypeHandle SemanticAnalyzerCore::record_ordering_binary_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs)
{
    if (is_valid(lhs) && this->state_.checked.types.get(lhs).kind == TypeKind::generic_param) {
        if (!this->generic_param_has_capability(lhs, CapabilityKind::ord)) {
            this->report_capability(
                expr.range, sema_generic_comparison_operator_message(this->state_.checked.types.display_name(lhs)));
        }
        const TypeHandle bool_type = this->state_.checked.types.builtin(BuiltinType::bool_);
        return this->record_expr_types(expr_id, bool_type, bool_type);
    }
    if (!this->type_supports_ordering_operator(lhs)) {
        this->report_general(expr.range, std::string(SEMA_COMPARISON_NUMERIC));
    }
    const TypeHandle bool_type = this->state_.checked.types.builtin(BuiltinType::bool_);
    return this->record_expr_types(expr_id, bool_type, bool_type);
}

TypeHandle SemanticAnalyzerCore::record_equality_binary_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs, const bool null_pointer_comparison)
{
    if (is_valid(lhs) && this->state_.checked.types.get(lhs).kind == TypeKind::generic_param) {
        if (!this->generic_param_has_capability(lhs, CapabilityKind::eq)) {
            this->report_capability(
                expr.range, sema_generic_equality_operator_message(this->state_.checked.types.display_name(lhs)));
        }
    } else if (!this->type_supports_equality_operator(lhs) && !null_pointer_comparison) {
        this->report_general(expr.range, std::string(SEMA_EQUALITY_SCALAR));
    }
    const TypeHandle bool_type = this->state_.checked.types.builtin(BuiltinType::bool_);
    return this->record_expr_types(expr_id, bool_type, bool_type);
}

TypeHandle SemanticAnalyzerCore::record_logical_binary_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs, const TypeHandle rhs)
{
    if (!this->state_.checked.types.is_bool(lhs) || !this->state_.checked.types.is_bool(rhs)) {
        this->report_general(expr.range, std::string(SEMA_LOGICAL_BOOL));
    }
    const TypeHandle bool_type = this->state_.checked.types.builtin(BuiltinType::bool_);
    return this->record_expr_types(expr_id, bool_type, bool_type);
}

TypeHandle SemanticAnalyzerCore::record_integer_binary_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle result_intrinsic, const TypeHandle lhs)
{
    if (is_valid(lhs) && this->state_.checked.types.get(lhs).kind == TypeKind::generic_param) {
        this->report_capability(
            expr.range, sema_generic_integer_operator_message(this->state_.checked.types.display_name(lhs)));
        return this->record_expr_types(expr_id, result_intrinsic, lhs);
    }
    if (!this->state_.checked.types.is_integer(lhs)) {
        this->report_general(expr.range, std::string(SEMA_INTEGER_OPERATOR_INTEGER));
    }
    return this->record_expr_types(expr_id, result_intrinsic, lhs);
}

TypeHandle SemanticAnalyzerCore::record_numeric_binary_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle result_intrinsic, const TypeHandle lhs)
{
    if (is_valid(lhs) && this->state_.checked.types.get(lhs).kind == TypeKind::generic_param) {
        this->report_capability(
            expr.range, sema_generic_operator_message(this->state_.checked.types.display_name(lhs)));
        return this->record_expr_types(expr_id, result_intrinsic, lhs);
    }
    if (!this->state_.checked.types.is_integer(lhs) && !this->state_.checked.types.is_float(lhs)) {
        this->report_general(expr.range, std::string(SEMA_BINARY_NUMERIC));
    }
    return this->record_expr_types(expr_id, result_intrinsic, lhs);
}

TypeHandle SemanticAnalyzerCore::analyze_field_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle)
{
    if (syntax::is_valid(expr.object) && expr.object.value < this->ctx_.module.exprs.size()) {
        const ModuleSelector module = this->resolve_module_selector(expr.object, false);
        if (syntax::is_valid(module.module)) {
            return this->analyze_module_member_expr(expr_id, module.module, expr);
        }
        if (module.failed_as_module_selector) {
            static_cast<void>(this->resolve_module_selector(expr.object, true));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        const bool object_is_plain_name = expr_name_is_unqualified(this->ctx_.module, expr.object);
        const TypeHandle enum_type = this->resolve_type_selector(expr.object, !object_is_plain_name);
        if (is_valid(enum_type) && this->state_.checked.types.get(enum_type).kind == TypeKind::enum_) {
            const EnumCaseInfo* enum_case =
                this->find_enum_case_by_type_and_case(enum_type, expr.field_name_id, expr.field_name);
            if (enum_case == nullptr) {
                this->report_lookup(expr.range,
                    sema_unknown_scoped_enum_case_message(
                        this->state_.checked.types.display_name(enum_type), expr.field_name));
                this->report_lookup_suggestion(expr.range, this->nearest_enum_case_name(enum_type, expr.field_name));
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            const bool recorded = this->record_no_payload_enum_case_expr(expr_id, *enum_case, expr.range);
            return recorded ? enum_case->type : INVALID_TYPE_HANDLE;
        }
        if (object_is_plain_name && is_valid(enum_type)) {
            this->report_general(expr.range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }

    const PlaceInfo place = this->analyze_place_info(expr_id, true);
    this->require_place_projection_safety(place, expr.range);
    return this->record_expr_type(expr_id, place.type);
}

TypeHandle SemanticAnalyzerCore::analyze_module_member_expr(
    const syntax::ExprId expr_id, const syntax::ModuleId module, const SemanticAnalyzerCore::ExprView& expr)
{
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

TypeHandle SemanticAnalyzerCore::analyze_index_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr)
{
    const PlaceInfo place = this->analyze_place_info(expr_id, true);
    this->require_place_projection_safety(place, expr.range);
    return this->record_expr_type(expr_id, place.type);
}

TypeHandle SemanticAnalyzerCore::analyze_slice_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    struct ConstantSliceBound {
        bool present = false;
        bool valid = false;
        bool negated = false;
        base::u64 value = 0;
        base::SourceRange range;
    };

    const TypeHandle usize_type = this->state_.checked.types.builtin(BuiltinType::usize);
    const auto analyze_bound = [&](const syntax::ExprId bound) {
        if (!syntax::is_valid(bound)) {
            return;
        }
        const TypeHandle bound_type = this->analyze_expr(bound, usize_type);
        if (!this->state_.checked.types.is_integer(bound_type)) {
            this->report_general(
                expr_range_or(this->ctx_.module, bound, expr.range), std::string(SEMA_SLICE_BOUND_INTEGER));
        }
    };
    const auto constant_bound = [&](const syntax::ExprId bound) {
        ConstantSliceBound result;
        if (!syntax::is_valid(bound)) {
            return result;
        }
        result.present = true;
        result.range = expr_range_or(this->ctx_.module, bound, expr.range);
        const IntegerLiteralExpr literal = integer_literal_expr(this->ctx_.module, bound);
        if (!syntax::is_valid(literal.literal)) {
            return result;
        }
        const syntax::LiteralExprPayload* const literal_expr =
            this->ctx_.module.exprs.literal_payload(literal.literal.value);
        result.negated = literal.negated;
        result.valid = literal_expr != nullptr && this->parse_integer_literal_text(literal_expr->text, result.value);
        return result;
    };
    const auto check_array_slice_bounds = [&](const base::u64 array_count) {
        const ConstantSliceBound start = constant_bound(expr.slice_start);
        const ConstantSliceBound end = constant_bound(expr.slice_end);
        const auto bound_is_out_of_bounds = [array_count](const ConstantSliceBound& bound) {
            return bound.present && bound.valid
                && ((bound.negated && bound.value != 0) || (!bound.negated && bound.value > array_count));
        };
        if (bound_is_out_of_bounds(start)) {
            this->report_general(start.range, std::string(SEMA_ARRAY_SLICE_BOUND_OUT_OF_BOUNDS));
        }
        if (bound_is_out_of_bounds(end)) {
            this->report_general(end.range, std::string(SEMA_ARRAY_SLICE_BOUND_OUT_OF_BOUNDS));
        }
        if (start.present && end.present && start.valid && end.valid && !start.negated && !end.negated
            && start.value <= array_count && end.value <= array_count && start.value > end.value) {
            this->report_general(expr.range, std::string(SEMA_ARRAY_SLICE_BOUNDS_ORDER));
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
    if (this->state_.checked.types.is_array(object)) {
        const TypeInfo& array = this->state_.checked.types.get(object);
        check_array_slice_bounds(array.array_count);
        element = array.array_element;
        mutability = this->is_writable_place(expr.object) ? PointerMutability::mut : PointerMutability::const_;
    } else if (this->state_.checked.types.is_slice(object)) {
        const TypeInfo& slice = this->state_.checked.types.get(object);
        element = slice.slice_element;
        mutability = slice.slice_mutability;
    } else if (this->state_.checked.types.is_str(object)) {
        return this->record_expr_type(expr_id, this->state_.checked.types.builtin(BuiltinType::str));
    } else {
        this->report_general(expr.range, std::string(SEMA_SLICE_ARRAY_OR_SLICE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (!this->is_valid_storage_type(element)) {
        this->report_general(expr.range, std::string(SEMA_SLICE_ELEMENT_STORAGE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle natural_slice = this->state_.checked.types.slice(mutability, element);
    if (this->state_.checked.types.is_slice(expected_type)
        && this->can_assign(expected_type, natural_slice, syntax::INVALID_EXPR_ID)) {
        this->record_coercion(expr_id, natural_slice, expected_type, CoercionKind::slice_to_expected_slice);
        return this->record_expr_types(expr_id, natural_slice, expected_type);
    }
    return this->record_expr_types(expr_id, natural_slice, natural_slice);
}

TypeHandle SemanticAnalyzerCore::analyze_array_literal_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    const bool has_expected_array = this->state_.checked.types.is_array(expected_type);
    TypeHandle element_type = INVALID_TYPE_HANDLE;
    base::u64 expected_count = 0;
    if (is_valid(expected_type)) {
        if (!has_expected_array) {
            this->report_general(expr.range, std::string(SEMA_ARRAY_LITERAL_EXPECTED_TYPE));
        } else {
            const TypeInfo& expected = this->state_.checked.types.get(expected_type);
            element_type = expected.array_element;
            expected_count = expected.array_count;
        }
    }

    const bool is_repeat_literal =
        syntax::is_valid(expr.array_repeat_value) || syntax::is_valid(expr.array_repeat_count);
    base::u64 literal_count = static_cast<base::u64>(expr.array_elements.size());
    bool count_known = !is_repeat_literal;
    if (is_repeat_literal) {
        count_known = false;
        const bool count_expr_valid =
            syntax::is_valid(expr.array_repeat_count) && expr.array_repeat_count.value < this->ctx_.module.exprs.size();
        if (!count_expr_valid) {
            this->report_general(expr.range, std::string(SEMA_ARRAY_REPEAT_INTEGER));
        } else {
            const base::SourceRange count_range = this->ctx_.module.exprs.range(expr.array_repeat_count.value);
            if (this->ctx_.module.exprs.kind(expr.array_repeat_count.value) != syntax::ExprKind::integer_literal) {
                this->report_general(count_range, std::string(SEMA_ARRAY_REPEAT_INTEGER));
            } else if (!this->parse_integer_literal_text(
                           expr_literal_text_or_empty(this->ctx_.module, expr.array_repeat_count), literal_count)) {
                this->report_general(count_range, std::string(SEMA_ARRAY_REPEAT_OUT_OF_RANGE));
            } else {
                count_known = true;
            }
        }
    }

    if (has_expected_array && count_known && literal_count != expected_count) {
        this->report_general(expr.range, sema_array_literal_length_mismatch_message(expected_count, literal_count));
    }

    if (!has_expected_array && !is_repeat_literal && expr.array_elements.empty()) {
        this->report_general(expr.range, std::string(SEMA_EMPTY_ARRAY_CONTEXT));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (!has_expected_array) {
        const syntax::ExprId first_value = is_repeat_literal ? expr.array_repeat_value : expr.array_elements.front();
        element_type = this->analyze_expr(first_value);
        if (!is_valid(element_type)) {
            this->report_general(expr.range, std::string(SEMA_ARRAY_ELEMENT_INFER));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }

    if (is_repeat_literal) {
        const TypeHandle actual = this->analyze_expr(expr.array_repeat_value, element_type);
        if (!this->can_assign(element_type, actual, expr.array_repeat_value)) {
            this->report_type_mismatch(expr.range, std::string(SEMA_ARRAY_REPEAT_TYPE_MISMATCH), element_type, actual);
        }
    } else {
        for (const syntax::ExprId element : expr.array_elements) {
            const TypeHandle actual = this->analyze_expr(element, element_type);
            if (!this->can_assign(element_type, actual, element)) {
                this->report_type_mismatch(expr_range_or(this->ctx_.module, element, expr.range),
                    std::string(SEMA_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH), element_type, actual);
            }
        }
    }

    TypeHandle intrinsic_element_type = INVALID_TYPE_HANDLE;
    if (is_repeat_literal) {
        intrinsic_element_type = this->cached_expr_intrinsic_type(expr.array_repeat_value);
    } else if (!expr.array_elements.empty()) {
        intrinsic_element_type = this->cached_expr_intrinsic_type(expr.array_elements.front());
    }
    if (!is_valid(intrinsic_element_type)) {
        intrinsic_element_type = element_type;
    }

    if (!is_valid(element_type) || !count_known) {
        return this->record_expr_types(
            expr_id, INVALID_TYPE_HANDLE, has_expected_array ? expected_type : INVALID_TYPE_HANDLE);
    }
    const TypeHandle array_type =
        has_expected_array ? expected_type : this->state_.checked.types.array(literal_count, element_type);
    const TypeHandle intrinsic_array_type = is_valid(intrinsic_element_type)
        ? this->state_.checked.types.array(literal_count, intrinsic_element_type)
        : INVALID_TYPE_HANDLE;
    if (!this->is_valid_storage_type(array_type)) {
        this->report_general(expr.range, std::string(SEMA_ARRAY_LITERAL_STORAGE));
    }
    return this->record_expr_types(expr_id, intrinsic_array_type, array_type);
}

TypeHandle SemanticAnalyzerCore::analyze_tuple_literal_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    const bool has_expected_tuple = this->state_.checked.types.is_tuple(expected_type);
    std::vector<TypeHandle> element_types;
    if (is_valid(expected_type)) {
        if (!has_expected_tuple) {
            this->report_general(expr.range, std::string(SEMA_TUPLE_LITERAL_EXPECTED_TYPE));
        } else {
            const TypeInfo& expected = this->state_.checked.types.get(expected_type);
            if (expected.tuple_elements.size() != expr.tuple_elements.size()) {
                this->report_general(expr.range, std::string(SEMA_TUPLE_LITERAL_ARITY));
            }
            element_types.assign(expected.tuple_elements.begin(), expected.tuple_elements.end());
        }
    }

    if (!has_expected_tuple) {
        element_types.assign(expr.tuple_elements.size(), INVALID_TYPE_HANDLE);
    }

    for (base::usize i = 0; i < expr.tuple_elements.size(); ++i) {
        const TypeHandle expected_element = i < element_types.size() ? element_types[i] : INVALID_TYPE_HANDLE;
        const TypeHandle actual = this->analyze_expr(expr.tuple_elements[i], expected_element);
        if (i >= element_types.size()) {
            continue;
        }
        if (has_expected_tuple) {
            if (!this->can_assign(element_types[i], actual, expr.tuple_elements[i])) {
                this->report_type_mismatch(expr_range_or(this->ctx_.module, expr.tuple_elements[i], expr.range),
                    std::string(SEMA_TUPLE_LITERAL_ELEMENT_TYPE_MISMATCH), element_types[i], actual);
            }
        } else {
            element_types[i] = actual;
        }
    }

    if (!has_expected_tuple) {
        for (const TypeHandle element : element_types) {
            if (!is_valid(element)) {
                this->report_general(expr.range, std::string(SEMA_LOCAL_TYPE_INFER));
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
        }
    }

    const TypeHandle tuple_type = has_expected_tuple ? expected_type : this->state_.checked.types.tuple(element_types);
    std::vector<TypeHandle> intrinsic_element_types;
    intrinsic_element_types.reserve(expr.tuple_elements.size());
    bool has_intrinsic_tuple = true;
    for (base::usize i = 0; i < expr.tuple_elements.size(); ++i) {
        TypeHandle intrinsic = this->cached_expr_intrinsic_type(expr.tuple_elements[i]);
        if (!is_valid(intrinsic) && i < element_types.size()) {
            intrinsic = element_types[i];
        }
        if (!is_valid(intrinsic)) {
            has_intrinsic_tuple = false;
            break;
        }
        intrinsic_element_types.push_back(intrinsic);
    }
    const TypeHandle intrinsic_tuple_type =
        has_intrinsic_tuple ? this->state_.checked.types.tuple(intrinsic_element_types) : INVALID_TYPE_HANDLE;
    if (is_valid(tuple_type) && !this->is_valid_storage_type(tuple_type)) {
        this->report_general(expr.range, std::string(SEMA_TUPLE_LITERAL_STORAGE));
    }
    return this->record_expr_types(expr_id, intrinsic_tuple_type, tuple_type);
}

TypeHandle SemanticAnalyzerCore::analyze_struct_literal_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle)
{
    TypeHandle struct_type = INVALID_TYPE_HANDLE;
    if (syntax::is_valid(expr.object)) {
        struct_type = this->resolve_type_selector(expr.object, true);
    } else {
        NamedTypeSelector selector;
        selector.module = syntax::INVALID_MODULE_ID;
        selector.name = expr.struct_name;
        selector.name_id = expr.struct_name_id;
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
        this->report_general(expr.range, std::string(SEMA_STRUCT_LITERAL_TYPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    std::unordered_set<IdentId, IdentIdHash> initialized_fields;
    for (const syntax::FieldInit& init : expr.field_inits) {
        if (!initialized_fields.insert(init.name_id).second) {
            this->report_duplicate(init.range, sema_duplicate_struct_literal_field_message(init.name));
            continue;
        }
        const StructFieldInfo* field_info = nullptr;
        for (const StructFieldInfo& field : info->fields) {
            if (field.name_id == init.name_id) {
                field_info = &field;
                break;
            }
        }
        if (field_info == nullptr) {
            this->report_lookup(init.range, sema_unknown_struct_literal_field_message(init.name));
            this->report_lookup_suggestion(init.range, this->nearest_field_name(*info, init.name));
            continue;
        }
        if (!this->can_access(info->module, field_info->visibility)) {
            this->report_visibility(init.range, sema_private_field_message(init.name));
            continue;
        }
        const TypeHandle actual = this->analyze_expr(init.value, field_info->type);
        if (!this->can_assign(field_info->type, actual, init.value)) {
            this->report_type_mismatch(
                init.range, std::string(SEMA_STRUCT_LITERAL_FIELD_TYPE_MISMATCH), field_info->type, actual);
        }
    }
    for (const StructFieldInfo& field : info->fields) {
        if (!initialized_fields.contains(field.name_id)) {
            this->report_general(expr.range, sema_struct_literal_missing_field_message(field.name));
        }
    }
    return this->record_expr_type(expr_id, struct_type);
}

TypeHandle SemanticAnalyzerCore::analyze_try_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr)
{
    if (this->state_.flow.in_const_initializer) {
        this->report_general(expr.range, std::string(SEMA_TRY_CONST_INITIALIZER));
    }

    const TypeHandle source_type = this->analyze_expr(expr.try_operand);
    const TryShape source_shape = this->classify_try_shape(source_type);
    if (source_shape.kind == TryShape::Kind::malformed_result) {
        this->report_general(expr.range, std::string(SEMA_TRY_RESULT_SHAPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (source_shape.kind == TryShape::Kind::malformed_option) {
        this->report_general(expr.range, std::string(SEMA_TRY_OPTION_SHAPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (source_shape.kind == TryShape::Kind::result) {
        const TryShape return_shape = this->classify_try_shape(this->state_.flow.current_function_return_type);
        if (return_shape.kind != TryShape::Kind::result || return_shape.failure_case == nullptr) {
            this->report_general(expr.range, std::string(SEMA_TRY_RESULT_RETURN));
            return this->record_expr_type(expr_id, source_shape.success_case->payload_type);
        }
        if (!this->state_.checked.types.same(
                return_shape.failure_case->payload_type, source_shape.failure_case->payload_type)) {
            this->report_general(expr.range, std::string(SEMA_TRY_RESULT_ERR_PAYLOAD));
        }
        return this->record_expr_type(expr_id, source_shape.success_case->payload_type);
    }

    if (source_shape.kind == TryShape::Kind::option) {
        const TryShape return_shape = this->classify_try_shape(this->state_.flow.current_function_return_type);
        if (return_shape.kind != TryShape::Kind::option) {
            this->report_general(expr.range, std::string(SEMA_TRY_OPTION_RETURN));
            return this->record_expr_type(expr_id, source_shape.success_case->payload_type);
        }
        return this->record_expr_type(expr_id, source_shape.success_case->payload_type);
    }

    this->report_general(expr.range, std::string(SEMA_TRY_SHAPE));
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzerCore::analyze_if_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (this->state_.flow.in_const_initializer) {
        this->report_general(expr.range, std::string(SEMA_IF_EXPR_CONST_INITIALIZER));
    }
    const TypeHandle condition = this->analyze_expr(expr.condition);
    if (!syntax::is_valid(expr.condition_pattern) && !this->state_.checked.types.is_bool(condition)) {
        this->report_general(
            expr_range_or(this->ctx_.module, expr.condition, expr.range), std::string(SEMA_IF_EXPR_CONDITION_BOOL));
    }
    const auto analyze_then_branch = [&](const TypeHandle expected) {
        if (!syntax::is_valid(expr.condition_pattern)) {
            return this->analyze_expr(expr.then_expr, expected);
        }
        std::vector<PatternBinding> bindings;
        static_cast<void>(this->analyze_pattern(expr.condition_pattern, condition, bindings));
        this->state_.names.symbols.push_scope(bindings.size());
        this->define_pattern_bindings(bindings, false);
        const TypeHandle result = this->analyze_expr(expr.then_expr, expected);
        this->state_.names.symbols.pop_scope();
        return result;
    };

    TypeHandle then_type = INVALID_TYPE_HANDLE;
    TypeHandle else_type = INVALID_TYPE_HANDLE;
    if (!is_valid(expected_type) && this->is_null_result_expr(expr.then_expr)
        && !this->is_null_result_expr(expr.else_expr)) {
        else_type = this->analyze_expr(expr.else_expr);
        then_type = analyze_then_branch(else_type);
    } else {
        then_type = analyze_then_branch(expected_type);
        else_type = this->analyze_expr(expr.else_expr, is_valid(then_type) ? then_type : expected_type);
        if (!is_valid(then_type) && this->state_.checked.types.is_pointer(else_type)
            && this->is_null_result_expr(expr.then_expr)) {
            then_type = analyze_then_branch(else_type);
        }
        if (!is_valid(else_type) && this->state_.checked.types.is_pointer(then_type)
            && this->is_null_result_expr(expr.else_expr)) {
            else_type = this->analyze_expr(expr.else_expr, then_type);
        }
    }
    if (!this->state_.checked.types.same(then_type, else_type)) {
        this->report_type_mismatch(expr.range, std::string(SEMA_IF_EXPR_BRANCH_TYPE), then_type, else_type);
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (is_valid(then_type) && this->state_.checked.types.is_void(then_type)) {
        this->report_general(expr.range, std::string(SEMA_IF_EXPR_VOID));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle then_intrinsic = this->cached_expr_intrinsic_type(expr.then_expr);
    const TypeHandle else_intrinsic = this->cached_expr_intrinsic_type(expr.else_expr);
    TypeHandle intrinsic_type = INVALID_TYPE_HANDLE;
    if (is_valid(then_intrinsic) && is_valid(else_intrinsic)
        && this->state_.checked.types.same(then_intrinsic, else_intrinsic)) {
        intrinsic_type = then_intrinsic;
    } else if (!is_valid(expected_type)) {
        intrinsic_type = then_type;
    }
    return this->record_expr_types(expr_id, intrinsic_type, then_type);
}

TypeHandle SemanticAnalyzerCore::analyze_block_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (this->state_.flow.in_const_initializer) {
        this->report_general(expr.range, std::string(SEMA_BLOCK_EXPR_CONST_INITIALIZER));
    }
    if (!syntax::is_valid(expr.block_result)) {
        this->report_general(expr.range, std::string(SEMA_BLOCK_EXPR_FINAL));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    this->state_.names.symbols.push_scope();
    this->analyze_block_statements(
        expr.block, this->state_.flow.current_function_return_type, this->state_.flow.current_return_inference);
    if (!this->block_may_fallthrough(expr.block)) {
        this->report_general(expr.range, std::string(SEMA_BLOCK_EXPR_UNREACHABLE));
    }
    const TypeHandle result = this->analyze_expr(expr.block_result, expected_type);
    this->state_.names.symbols.pop_scope();

    if (!is_valid(result)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (this->state_.checked.types.is_void(result)) {
        this->report_general(expr.range, std::string(SEMA_BLOCK_EXPR_VOID));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle intrinsic_type = this->cached_expr_intrinsic_type(expr.block_result);
    return this->record_expr_types(expr_id, is_valid(intrinsic_type) ? intrinsic_type : result, result);
}

TypeHandle SemanticAnalyzerCore::analyze_unsafe_block_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (this->state_.flow.in_const_initializer) {
        this->report_general(expr.range, std::string(SEMA_UNSAFE_BLOCK_CONST_INITIALIZER));
    }

    this->state_.names.symbols.push_scope();
    const int previous_unsafe_depth = this->state_.flow.unsafe_context_depth;
    ++this->state_.flow.unsafe_context_depth;
    this->analyze_block_statements(
        expr.block, this->state_.flow.current_function_return_type, this->state_.flow.current_return_inference);
    TypeHandle result = this->state_.checked.types.builtin(BuiltinType::void_);
    if (syntax::is_valid(expr.block_result)) {
        if (!this->block_may_fallthrough(expr.block)) {
            this->report_general(expr.range, std::string(SEMA_BLOCK_EXPR_UNREACHABLE));
        }
        result = this->analyze_expr(expr.block_result, expected_type);
    }
    this->state_.flow.unsafe_context_depth = previous_unsafe_depth;
    this->state_.names.symbols.pop_scope();

    if (!is_valid(result)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (syntax::is_valid(expr.block_result) && this->state_.checked.types.is_void(result)) {
        this->report_general(expr.range, std::string(SEMA_BLOCK_EXPR_VOID));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle intrinsic_type =
        syntax::is_valid(expr.block_result) ? this->cached_expr_intrinsic_type(expr.block_result) : result;
    return this->record_expr_types(expr_id, is_valid(intrinsic_type) ? intrinsic_type : result, result);
}

} // namespace aurex::sema
