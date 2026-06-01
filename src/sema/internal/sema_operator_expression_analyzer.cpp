#include <aurex/sema/sema_messages.hpp>

#include <string>
#include <vector>

#include <sema/internal/sema_operator_expression_analyzer.hpp>

namespace aurex::sema {

namespace {

constexpr base::u32 SEMA_I8_BIT_WIDTH = 8;
constexpr base::u32 SEMA_I16_BIT_WIDTH = 16;
constexpr base::u32 SEMA_I32_BIT_WIDTH = 32;
constexpr base::u32 SEMA_I64_BIT_WIDTH = 64;
constexpr base::u32 SEMA_I64_SIGN_BIT_INDEX = 63;
constexpr base::u32 SEMA_SIGN_BIT_OFFSET = 1;

struct IntegerLiteralExpr {
    syntax::ExprId literal = syntax::INVALID_EXPR_ID;
    bool negated = false;
};

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

[[nodiscard]] base::u32 integer_bit_width(
    const TypeTable& types, const TypeHandle type, const base::u32 target_pointer_bits) noexcept
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
        case BuiltinType::usize:
            return target_pointer_bits;
        default:
            return 0;
    }
}

} // namespace

TypeHandle SemanticAnalyzerCore::OperatorExpressionAnalyzer::analyze_unary_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (expr.unary_op == syntax::UnaryOp::numeric_negate
        && expr_is_kind(this->core_.ctx_.module, expr.unary_operand, syntax::ExprKind::integer_literal)) {
        const SemanticAnalyzerCore::ExprView operand = this->core_.expr_view(expr.unary_operand);
        const TypeHandle literal_type = this->core_.analyze_negative_integer_literal(
            expr.unary_operand, operand.text, operand.range, expected_type);
        const TypeHandle intrinsic_type = this->core_.cached_expr_intrinsic_type(expr.unary_operand);
        return this->core_.record_expr_types(
            expr_id, is_valid(intrinsic_type) ? intrinsic_type : literal_type, literal_type);
    }

    if (expr.unary_op == syntax::UnaryOp::address_of || expr.unary_op == syntax::UnaryOp::address_of_mut) {
        const PlaceInfo operand_place = this->core_.analyze_place_info(expr.unary_operand, true);
        this->core_.require_place_projection_safety(operand_place, expr.range);
        if (!operand_place.is_place) {
            this->core_.report_general(expr.range, std::string(SEMA_ADDRESS_OF_PLACE));
        }
        if (expr.unary_op == syntax::UnaryOp::address_of_mut) {
            if (!operand_place.is_writable) {
                this->core_.report_general(expr.range, std::string(SEMA_MUTABLE_REFERENCE_PLACE));
            }
            if (!this->core_.is_valid_storage_type(operand_place.type)) {
                this->core_.report_general(expr.range, std::string(SEMA_REFERENCE_STORAGE));
            }
            const TypeHandle reference_type =
                this->core_.state_.checked.types.reference(PointerMutability::mut, operand_place.type);
            return this->core_.record_expr_types(expr_id, reference_type, reference_type);
        }
        if (!this->core_.is_valid_storage_type(operand_place.type)) {
            this->core_.report_general(expr.range, std::string(SEMA_REFERENCE_STORAGE));
        }
        const TypeHandle reference_type =
            this->core_.state_.checked.types.reference(PointerMutability::const_, operand_place.type);
        return this->core_.record_expr_types(expr_id, reference_type, reference_type);
    }

    const TypeHandle operand_expected = ((expr.unary_op == syntax::UnaryOp::numeric_negate
                                             && (this->core_.state_.checked.types.is_integer(expected_type)
                                                 || this->core_.state_.checked.types.is_float(expected_type)))
                                            || (expr.unary_op == syntax::UnaryOp::bitwise_not
                                                && this->core_.state_.checked.types.is_integer(expected_type)))
        ? expected_type
        : INVALID_TYPE_HANDLE;
    const TypeHandle operand = this->core_.analyze_expr(expr.unary_operand, operand_expected);
    if (expr.unary_op == syntax::UnaryOp::logical_not && !this->core_.state_.checked.types.is_bool(operand)) {
        this->core_.report_general(expr.range, std::string(SEMA_LOGICAL_NOT_BOOL));
    }
    if (expr.unary_op == syntax::UnaryOp::numeric_negate
        && !is_signed_integer_type(this->core_.state_.checked.types, operand)
        && !this->core_.state_.checked.types.is_float(operand)) {
        this->core_.report_general(expr.range, std::string(SEMA_NUMERIC_UNARY_OPERAND));
    }
    if (expr.unary_op == syntax::UnaryOp::bitwise_not && !this->core_.state_.checked.types.is_integer(operand)) {
        this->core_.report_general(expr.range, std::string(SEMA_BITWISE_NOT_INTEGER));
    }
    if (expr.unary_op == syntax::UnaryOp::dereference) {
        if (this->core_.state_.checked.types.is_pointer(operand)) {
            this->core_.require_unsafe_context(expr.range, SEMA_UNSAFE_DEREF);
        }
        if (!is_pointer_or_reference(this->core_.state_.checked.types, operand)) {
            this->core_.report_general(expr.range, std::string(SEMA_DEREF_POINTER));
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        const TypeHandle pointee = this->core_.state_.checked.types.get(operand).pointee;
        if (!this->core_.is_valid_storage_type(pointee)) {
            this->core_.report_general(expr.range, std::string(SEMA_DEREF_STORAGE));
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        return this->core_.record_expr_types(expr_id, pointee, pointee);
    }
    const TypeHandle operand_intrinsic = this->core_.cached_expr_intrinsic_type(expr.unary_operand);
    return this->core_.record_expr_types(expr_id, is_valid(operand_intrinsic) ? operand_intrinsic : operand, operand);
}

TypeHandle SemanticAnalyzerCore::OperatorExpressionAnalyzer::analyze_binary_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    const BinaryExprAnalysis analysis = this->core_.analyze_binary_operands(expr, expected_type);
    this->core_.diagnose_binary_operand_mismatch(expr, analysis);
    this->core_.diagnose_binary_literal_hazards(expr, analysis.lhs);
    return this->core_.record_binary_operator_expr(expr_id, expr, analysis);
}

SemanticAnalyzerCore::BinaryExprAnalysis SemanticAnalyzerCore::OperatorExpressionAnalyzer::analyze_binary_operands(
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    BinaryExprAnalysis analysis;
    analysis.operand_expected = binary_result_uses_operand_type(expr.binary_op)
            && (this->core_.state_.checked.types.is_integer(expected_type)
                || this->core_.state_.checked.types.is_float(expected_type))
        ? expected_type
        : INVALID_TYPE_HANDLE;
    if (!is_valid(analysis.operand_expected) && is_contextual_integer_expr(this->core_.ctx_.module, expr.binary_lhs)
        && !is_contextual_integer_expr(this->core_.ctx_.module, expr.binary_rhs)) {
        analysis.rhs = this->core_.analyze_expr(expr.binary_rhs);
        analysis.lhs = this->core_.analyze_expr(expr.binary_lhs, analysis.rhs);
    } else {
        analysis.lhs = this->core_.analyze_expr(expr.binary_lhs, analysis.operand_expected);
        analysis.rhs = this->core_.analyze_expr(
            expr.binary_rhs, is_valid(analysis.lhs) ? analysis.lhs : analysis.operand_expected);
    }

    const bool is_equality = expr.binary_op == syntax::BinaryOp::equal || expr.binary_op == syntax::BinaryOp::not_equal;
    analysis.null_pointer_comparison = is_equality
        && ((this->core_.state_.checked.types.is_pointer(analysis.lhs) && this->core_.is_null_literal(expr.binary_rhs))
            || (this->core_.state_.checked.types.is_pointer(analysis.rhs)
                && this->core_.is_null_literal(expr.binary_lhs)));

    analysis.result_intrinsic = analysis.lhs;
    if (is_valid(analysis.operand_expected)) {
        const TypeHandle lhs_intrinsic = this->core_.cached_expr_intrinsic_type(expr.binary_lhs);
        const TypeHandle rhs_intrinsic = this->core_.cached_expr_intrinsic_type(expr.binary_rhs);
        if (is_valid(lhs_intrinsic)
            && (!is_valid(rhs_intrinsic) || this->core_.state_.checked.types.same(lhs_intrinsic, rhs_intrinsic))) {
            analysis.result_intrinsic = lhs_intrinsic;
        }
    }

    return analysis;
}

void SemanticAnalyzerCore::OperatorExpressionAnalyzer::diagnose_binary_operand_mismatch(
    const SemanticAnalyzerCore::ExprView& expr, const SemanticAnalyzerCore::BinaryExprAnalysis& analysis) const
{
    if (!this->core_.state_.checked.types.same(analysis.lhs, analysis.rhs) && !analysis.null_pointer_comparison) {
        this->core_.report_general(expr.range, std::string(SEMA_BINARY_OPERANDS_SAME_TYPE));
    }
}

void SemanticAnalyzerCore::OperatorExpressionAnalyzer::diagnose_binary_literal_hazards(
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs) const
{
    this->core_.diagnose_binary_rhs_literal_hazards(expr, lhs);
    this->core_.diagnose_signed_binary_literal_overflow(expr, lhs);
}

void SemanticAnalyzerCore::OperatorExpressionAnalyzer::diagnose_binary_rhs_literal_hazards(
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs) const
{
    const IntegerLiteralExpr rhs_integer_literal = integer_literal_expr(this->core_.ctx_.module, expr.binary_rhs);
    if (!syntax::is_valid(rhs_integer_literal.literal)) {
        return;
    }

    base::u64 rhs_literal_value = 0;
    const std::string_view literal_text =
        expr_literal_text_or_empty(this->core_.ctx_.module, rhs_integer_literal.literal);
    const base::SourceRange rhs_range = expr_range_or(this->core_.ctx_.module, expr.binary_rhs,
        expr_range_or(this->core_.ctx_.module, rhs_integer_literal.literal, expr.range));
    if (!this->core_.parse_integer_literal_text(literal_text, rhs_literal_value)) {
        return;
    }

    if ((expr.binary_op == syntax::BinaryOp::div || expr.binary_op == syntax::BinaryOp::mod)
        && this->core_.state_.checked.types.is_integer(lhs) && rhs_literal_value == 0) {
        this->core_.report_general(rhs_range,
            expr.binary_op == syntax::BinaryOp::div ? std::string(SEMA_INTEGER_DIVISION_BY_ZERO)
                                                    : std::string(SEMA_INTEGER_MODULO_BY_ZERO));
    }

    if (expr.binary_op != syntax::BinaryOp::shl && expr.binary_op != syntax::BinaryOp::shr) {
        return;
    }

    const base::u32 bit_width =
        integer_bit_width(this->core_.state_.checked.types, lhs, this->core_.target_pointer_bit_width());
    if (rhs_integer_literal.negated && rhs_literal_value != 0) {
        this->core_.report_general(rhs_range, std::string(SEMA_SHIFT_NEGATIVE));
    } else if (!rhs_integer_literal.negated && bit_width != 0 && rhs_literal_value >= bit_width) {
        this->core_.report_general(rhs_range, std::string(SEMA_SHIFT_OUT_OF_RANGE));
    }
}

void SemanticAnalyzerCore::OperatorExpressionAnalyzer::diagnose_signed_binary_literal_overflow(
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs) const
{
    if ((expr.binary_op != syntax::BinaryOp::div && expr.binary_op != syntax::BinaryOp::mod)
        || !is_signed_integer_type(this->core_.state_.checked.types, lhs)) {
        return;
    }

    const IntegerLiteralExpr lhs_integer_literal = integer_literal_expr(this->core_.ctx_.module, expr.binary_lhs);
    const IntegerLiteralExpr rhs_integer_literal = integer_literal_expr(this->core_.ctx_.module, expr.binary_rhs);
    if (!syntax::is_valid(lhs_integer_literal.literal) || !syntax::is_valid(rhs_integer_literal.literal)) {
        return;
    }

    base::u64 lhs_literal_value = 0;
    base::u64 rhs_literal_value = 0;
    if (this->core_.parse_integer_literal_text(
            expr_literal_text_or_empty(this->core_.ctx_.module, lhs_integer_literal.literal), lhs_literal_value)
        && this->core_.parse_integer_literal_text(
            expr_literal_text_or_empty(this->core_.ctx_.module, rhs_integer_literal.literal), rhs_literal_value)
        && is_signed_min_integer_literal(lhs_integer_literal, lhs_literal_value,
            integer_bit_width(this->core_.state_.checked.types, lhs, this->core_.target_pointer_bit_width()))
        && rhs_integer_literal.negated && rhs_literal_value == 1) {
        this->core_.report_general(expr.range,
            expr.binary_op == syntax::BinaryOp::div ? std::string(SEMA_SIGNED_DIVISION_OVERFLOW)
                                                    : std::string(SEMA_SIGNED_MODULO_OVERFLOW));
    }
}

TypeHandle SemanticAnalyzerCore::OperatorExpressionAnalyzer::record_binary_operator_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const SemanticAnalyzerCore::BinaryExprAnalysis& analysis)
{
    switch (expr.binary_op) {
        case syntax::BinaryOp::less:
        case syntax::BinaryOp::less_equal:
        case syntax::BinaryOp::greater:
        case syntax::BinaryOp::greater_equal:
            return this->core_.record_ordering_binary_expr(expr_id, expr, analysis.lhs);
        case syntax::BinaryOp::equal:
        case syntax::BinaryOp::not_equal:
            return this->core_.record_equality_binary_expr(
                expr_id, expr, analysis.lhs, analysis.null_pointer_comparison);
        case syntax::BinaryOp::logical_and:
        case syntax::BinaryOp::logical_or:
            return this->core_.record_logical_binary_expr(expr_id, expr, analysis.lhs, analysis.rhs);
        case syntax::BinaryOp::bit_and:
        case syntax::BinaryOp::bit_xor:
        case syntax::BinaryOp::bit_or:
        case syntax::BinaryOp::shl:
        case syntax::BinaryOp::shr:
        case syntax::BinaryOp::mod:
            return this->core_.record_integer_binary_expr(expr_id, expr, analysis.result_intrinsic, analysis.lhs);
        default:
            return this->core_.record_numeric_binary_expr(expr_id, expr, analysis.result_intrinsic, analysis.lhs);
    }
}

TypeHandle SemanticAnalyzerCore::OperatorExpressionAnalyzer::record_ordering_binary_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs)
{
    if (is_valid(lhs) && this->core_.state_.checked.types.get(lhs).kind == TypeKind::generic_param) {
        if (!this->core_.generic_param_has_capability(lhs, CapabilityKind::ord)) {
            this->core_.report_capability(expr.range,
                sema_generic_comparison_operator_message(this->core_.state_.checked.types.display_name(lhs)));
        }
        const TypeHandle bool_type = this->core_.state_.checked.types.builtin(BuiltinType::bool_);
        return this->core_.record_expr_types(expr_id, bool_type, bool_type);
    }
    if (!this->core_.type_supports_ordering_operator(lhs)) {
        this->core_.report_general(expr.range, std::string(SEMA_COMPARISON_NUMERIC));
    }
    const TypeHandle bool_type = this->core_.state_.checked.types.builtin(BuiltinType::bool_);
    return this->core_.record_expr_types(expr_id, bool_type, bool_type);
}

TypeHandle SemanticAnalyzerCore::OperatorExpressionAnalyzer::record_equality_binary_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs, const bool null_pointer_comparison)
{
    if (is_valid(lhs) && this->core_.state_.checked.types.get(lhs).kind == TypeKind::generic_param) {
        if (!this->core_.generic_param_has_capability(lhs, CapabilityKind::eq)) {
            this->core_.report_capability(
                expr.range, sema_generic_equality_operator_message(this->core_.state_.checked.types.display_name(lhs)));
        }
    } else if (!this->core_.type_supports_equality_operator(lhs) && !null_pointer_comparison) {
        this->core_.report_general(expr.range, std::string(SEMA_EQUALITY_SCALAR));
    }
    const TypeHandle bool_type = this->core_.state_.checked.types.builtin(BuiltinType::bool_);
    return this->core_.record_expr_types(expr_id, bool_type, bool_type);
}

TypeHandle SemanticAnalyzerCore::OperatorExpressionAnalyzer::record_logical_binary_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle lhs, const TypeHandle rhs)
{
    if (!this->core_.state_.checked.types.is_bool(lhs) || !this->core_.state_.checked.types.is_bool(rhs)) {
        this->core_.report_general(expr.range, std::string(SEMA_LOGICAL_BOOL));
    }
    const TypeHandle bool_type = this->core_.state_.checked.types.builtin(BuiltinType::bool_);
    return this->core_.record_expr_types(expr_id, bool_type, bool_type);
}

TypeHandle SemanticAnalyzerCore::OperatorExpressionAnalyzer::record_integer_binary_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle result_intrinsic, const TypeHandle lhs)
{
    if (is_valid(lhs) && this->core_.state_.checked.types.get(lhs).kind == TypeKind::generic_param) {
        this->core_.report_capability(
            expr.range, sema_generic_integer_operator_message(this->core_.state_.checked.types.display_name(lhs)));
        return this->core_.record_expr_types(expr_id, result_intrinsic, lhs);
    }
    if (!this->core_.state_.checked.types.is_integer(lhs)) {
        this->core_.report_general(expr.range, std::string(SEMA_INTEGER_OPERATOR_INTEGER));
    }
    return this->core_.record_expr_types(expr_id, result_intrinsic, lhs);
}

TypeHandle SemanticAnalyzerCore::OperatorExpressionAnalyzer::record_numeric_binary_expr(const syntax::ExprId expr_id,
    const SemanticAnalyzerCore::ExprView& expr, const TypeHandle result_intrinsic, const TypeHandle lhs)
{
    if (is_valid(lhs) && this->core_.state_.checked.types.get(lhs).kind == TypeKind::generic_param) {
        this->core_.report_capability(
            expr.range, sema_generic_operator_message(this->core_.state_.checked.types.display_name(lhs)));
        return this->core_.record_expr_types(expr_id, result_intrinsic, lhs);
    }
    if (!this->core_.state_.checked.types.is_integer(lhs) && !this->core_.state_.checked.types.is_float(lhs)) {
        this->core_.report_general(expr.range, std::string(SEMA_BINARY_NUMERIC));
    }
    return this->core_.record_expr_types(expr_id, result_intrinsic, lhs);
}

SemanticAnalyzerCore::OperatorExpressionAnalyzer::OperatorExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept
    : core_(core)
{
}

SemanticAnalyzerCore::OperatorExpressionAnalyzer SemanticAnalyzerCore::operator_expression_analyzer() noexcept
{
    return OperatorExpressionAnalyzer(*this);
}

TypeHandle SemanticAnalyzerCore::analyze_unary_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->operator_expression_analyzer().analyze_unary_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_binary_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->operator_expression_analyzer().analyze_binary_expr(expr_id, expr, expected_type);
}

SemanticAnalyzerCore::BinaryExprAnalysis SemanticAnalyzerCore::analyze_binary_operands(
    const ExprView& expr, const TypeHandle expected_type)
{
    return this->operator_expression_analyzer().analyze_binary_operands(expr, expected_type);
}

void SemanticAnalyzerCore::diagnose_binary_operand_mismatch(
    const ExprView& expr, const BinaryExprAnalysis& analysis) const
{
    return OperatorExpressionAnalyzer(const_cast<SemanticAnalyzerCore&>(*this))
        .diagnose_binary_operand_mismatch(expr, analysis);
}

void SemanticAnalyzerCore::diagnose_binary_literal_hazards(const ExprView& expr, const TypeHandle lhs) const
{
    return OperatorExpressionAnalyzer(const_cast<SemanticAnalyzerCore&>(*this))
        .diagnose_binary_literal_hazards(expr, lhs);
}

void SemanticAnalyzerCore::diagnose_binary_rhs_literal_hazards(const ExprView& expr, const TypeHandle lhs) const
{
    return OperatorExpressionAnalyzer(const_cast<SemanticAnalyzerCore&>(*this))
        .diagnose_binary_rhs_literal_hazards(expr, lhs);
}

void SemanticAnalyzerCore::diagnose_signed_binary_literal_overflow(const ExprView& expr, const TypeHandle lhs) const
{
    return OperatorExpressionAnalyzer(const_cast<SemanticAnalyzerCore&>(*this))
        .diagnose_signed_binary_literal_overflow(expr, lhs);
}

TypeHandle SemanticAnalyzerCore::record_binary_operator_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const BinaryExprAnalysis& analysis)
{
    return this->operator_expression_analyzer().record_binary_operator_expr(expr_id, expr, analysis);
}

TypeHandle SemanticAnalyzerCore::record_ordering_binary_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle lhs)
{
    return this->operator_expression_analyzer().record_ordering_binary_expr(expr_id, expr, lhs);
}

TypeHandle SemanticAnalyzerCore::record_equality_binary_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle lhs, const bool null_pointer_comparison)
{
    return this->operator_expression_analyzer().record_equality_binary_expr(
        expr_id, expr, lhs, null_pointer_comparison);
}

TypeHandle SemanticAnalyzerCore::record_logical_binary_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle lhs, const TypeHandle rhs)
{
    return this->operator_expression_analyzer().record_logical_binary_expr(expr_id, expr, lhs, rhs);
}

TypeHandle SemanticAnalyzerCore::record_integer_binary_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle result_intrinsic, const TypeHandle lhs)
{
    return this->operator_expression_analyzer().record_integer_binary_expr(expr_id, expr, result_intrinsic, lhs);
}

TypeHandle SemanticAnalyzerCore::record_numeric_binary_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle result_intrinsic, const TypeHandle lhs)
{
    return this->operator_expression_analyzer().record_numeric_binary_expr(expr_id, expr, result_intrinsic, lhs);
}

} // namespace aurex::sema
