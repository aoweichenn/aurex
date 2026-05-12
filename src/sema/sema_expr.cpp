#include <aurex/sema/sema.hpp>

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
        const syntax::ExprNode& node = module.exprs[current.value];
        if (node.kind == syntax::ExprKind::integer_literal) {
            continue;
        }
        if (node.kind == syntax::ExprKind::unary && node.unary_op == syntax::UnaryOp::numeric_negate) {
            pending.push_back(node.unary_operand);
            continue;
        }
        if (node.kind == syntax::ExprKind::binary && binary_result_uses_operand_type(node.binary_op)) {
            pending.push_back(node.binary_lhs);
            pending.push_back(node.binary_rhs);
            continue;
        }
        return false;
    }
    return true;
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

struct IntegerLiteralExpr {
    syntax::ExprId literal = syntax::INVALID_EXPR_ID;
    bool negated = false;
};

[[nodiscard]] IntegerLiteralExpr integer_literal_expr(
    const syntax::AstModule& module,
    const syntax::ExprId candidate
) noexcept {
    if (!syntax::is_valid(candidate) || candidate.value >= module.exprs.size()) {
        return {};
    }
    const syntax::ExprNode& node = module.exprs[candidate.value];
    if (node.kind == syntax::ExprKind::integer_literal) {
        return {candidate, false};
    }
    if (node.kind == syntax::ExprKind::unary &&
        node.unary_op == syntax::UnaryOp::numeric_negate &&
        syntax::is_valid(node.unary_operand) &&
        node.unary_operand.value < module.exprs.size() &&
        module.exprs[node.unary_operand.value].kind == syntax::ExprKind::integer_literal) {
        return {node.unary_operand, true};
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

[[nodiscard]] bool is_const_u8_pointer(const TypeTable& types, const TypeHandle type) noexcept {
    if (!types.is_pointer(type)) {
        return false;
    }
    const TypeInfo& pointer = types.get(type);
    return pointer.pointer_mutability == PointerMutability::const_ &&
           types.same(pointer.pointee, types.builtin(BuiltinType::u8));
}

} // namespace

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id) {
    return this->analyze_expr(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id, const TypeHandle expected_type) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->module_.exprs.size()) {
        return INVALID_TYPE_HANDLE;
    }

    const syntax::ExprNode& expr = this->module_.exprs[expr_id.value];
    std::vector<TypeHandle>& expr_types = this->active_expr_types();
    if (expr_id.value < expr_types.size() && is_valid(expr_types[expr_id.value])) {
        return expr_types[expr_id.value];
    }
    return this->analyze_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzer::analyze_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    switch (expr.kind) {
    case syntax::ExprKind::integer_literal:
        return this->analyze_integer_literal(expr_id, expr, expected_type);
    case syntax::ExprKind::float_literal:
        return this->analyze_float_literal(expr_id, expr, expected_type);
    case syntax::ExprKind::bool_literal:
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
    case syntax::ExprKind::null_literal:
        return this->record_expr_type(
            expr_id,
            this->checked_.types.is_pointer(expected_type) ? expected_type : INVALID_TYPE_HANDLE
        );
    case syntax::ExprKind::string_literal:
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
    case syntax::ExprKind::c_string_literal:
        return this->record_expr_type(
            expr_id,
            this->checked_.types.pointer(PointerMutability::const_, this->checked_.types.builtin(BuiltinType::u8))
        );
    case syntax::ExprKind::byte_literal:
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::u8));
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
    case syntax::ExprKind::match_expr:
        return this->analyze_match_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::array_literal:
        return this->analyze_array_literal_expr(expr_id, expr, expected_type);
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
    case syntax::ExprKind::str_from_bytes_unchecked:
        return this->analyze_str_from_bytes_unchecked_expr(expr_id, expr);
    case syntax::ExprKind::invalid:
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_name_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr
) {
    const Symbol* symbol = nullptr;
    if (!expr.scope_name.empty()) {
        const syntax::ModuleId module = this->resolve_import_alias(expr.scope_name, expr.scope_range);
        if (!syntax::is_valid(module)) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        symbol = this->find_symbol_in_module(module, expr.text, expr.range);
    } else {
        symbol = this->find_symbol(expr.text, expr.range);
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

TypeHandle SemanticAnalyzer::analyze_generic_apply_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr
) {
    this->report(expr.range, std::string(SEMA_EXPLICIT_GENERIC_CALL_SYNTAX));
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_unary_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (expr.unary_op == syntax::UnaryOp::numeric_negate &&
        syntax::is_valid(expr.unary_operand) &&
        expr.unary_operand.value < this->module_.exprs.size() &&
        this->module_.exprs[expr.unary_operand.value].kind == syntax::ExprKind::integer_literal) {
        const syntax::ExprNode& operand_expr = this->module_.exprs[expr.unary_operand.value];
        const TypeHandle literal_type = this->checked_.types.is_integer(expected_type)
            ? expected_type
            : this->checked_.types.builtin(BuiltinType::i32);
        if (!this->negative_integer_literal_fits_type(literal_type, operand_expr.text)) {
            this->report(
                expr.range,
                sema_integer_literal_out_of_range_message(this->checked_.types.display_name(literal_type))
            );
        }
        static_cast<void>(this->record_expr_type(expr.unary_operand, literal_type));
        return this->record_expr_type(expr_id, literal_type);
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
        if (!this->checked_.types.is_pointer(operand)) {
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
    if (expr.unary_op == syntax::UnaryOp::address_of) {
        if (!this->is_place_expr(expr.unary_operand)) {
            this->report(expr.range, std::string(SEMA_ADDRESS_OF_PLACE));
        }
        const PointerMutability mutability = this->is_writable_place(expr.unary_operand)
            ? PointerMutability::mut
            : PointerMutability::const_;
        return this->record_expr_type(expr_id, this->checked_.types.pointer(mutability, operand));
    }
    return this->record_expr_type(expr_id, operand);
}

TypeHandle SemanticAnalyzer::analyze_binary_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
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
        const syntax::ExprNode& literal_expr = this->module_.exprs[rhs_integer_literal.literal.value];
        const base::SourceRange rhs_range =
            syntax::is_valid(expr.binary_rhs) && expr.binary_rhs.value < this->module_.exprs.size()
                ? this->module_.exprs[expr.binary_rhs.value].range
                : literal_expr.range;
        if (this->parse_integer_literal_text(literal_expr.text, rhs_literal_value)) {
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
            const syntax::ExprNode& lhs_literal_expr = this->module_.exprs[lhs_integer_literal.literal.value];
            const syntax::ExprNode& rhs_literal_expr = this->module_.exprs[rhs_integer_literal.literal.value];
            if (this->parse_integer_literal_text(lhs_literal_expr.text, lhs_literal_value) &&
                this->parse_integer_literal_text(rhs_literal_expr.text, rhs_literal_value) &&
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
            this->report(expr.range, sema_generic_comparison_operator_message(this->checked_.types.display_name(lhs)));
            return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
        }
        if (!this->checked_.types.is_integer(lhs) && !this->checked_.types.is_float(lhs)) {
            this->report(expr.range, std::string(SEMA_COMPARISON_NUMERIC));
        }
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::bool_));
    case syntax::BinaryOp::equal:
    case syntax::BinaryOp::not_equal: {
        const bool scalar =
            this->checked_.types.is_bool(lhs) ||
            this->checked_.types.is_integer(lhs) ||
            this->checked_.types.is_float(lhs) ||
            this->checked_.types.is_pointer(lhs) ||
            (is_valid(lhs) &&
             this->checked_.types.get(lhs).kind == TypeKind::enum_ &&
             !is_valid(this->checked_.types.get(lhs).enum_payload_storage));
        if (is_valid(lhs) && this->checked_.types.get(lhs).kind == TypeKind::generic_param) {
            this->report(expr.range, sema_generic_equality_operator_message(this->checked_.types.display_name(lhs)));
        } else if (!scalar && !is_null_pointer_comparison) {
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
    const syntax::ExprNode& expr,
    const TypeHandle
) {
    if (syntax::is_valid(expr.object) &&
        expr.object.value < this->module_.exprs.size() &&
        this->module_.exprs[expr.object.value].kind == syntax::ExprKind::name) {
        const syntax::ExprNode& object = this->module_.exprs[expr.object.value];
        if (!object.scope_name.empty()) {
            const TypeHandle enum_type = this->resolve_associated_type_owner(object, false);
            if (is_valid(enum_type) && this->checked_.types.get(enum_type).kind == TypeKind::enum_) {
                const EnumCaseInfo* enum_case = this->find_enum_case_by_type_and_case(enum_type, expr.field_name);
                if (enum_case == nullptr) {
                    this->report(expr.range, sema_unknown_scoped_enum_case_message(object.text, expr.field_name));
                    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
                }
                if (is_valid(enum_case->payload_type)) {
                    this->report(expr.range, sema_enum_payload_constructor_call_message(enum_case->name));
                    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
                }
                this->record_expr_c_name(expr_id, enum_case->c_name);
                return this->record_expr_type(expr_id, enum_case->type);
            }
        }
        if (object.scope_name.empty()) {
            if (const EnumCaseInfo* enum_case = this->find_enum_case_by_scoped_name(object.text, expr.field_name, expr.range, false);
                enum_case != nullptr) {
                if (is_valid(enum_case->payload_type)) {
                    this->report(expr.range, sema_enum_payload_constructor_call_message(enum_case->name));
                    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
                }
                this->record_expr_c_name(expr_id, enum_case->c_name);
                return this->record_expr_type(expr_id, enum_case->type);
            }
        }
    }

    TypeHandle object = this->analyze_expr(expr.object);
    if (this->checked_.types.is_pointer(object)) {
        object = this->checked_.types.get(object).pointee;
    }
    const StructInfo* info = this->find_struct(object);
    if (info == nullptr || info->is_opaque) {
        this->report(expr.range, std::string(SEMA_FIELD_STRUCT_VALUE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    for (const StructFieldInfo& field : info->fields) {
        if (field.name == expr.field_name) {
            if (!this->can_access(info->module, field.visibility)) {
                this->report(expr.range, sema_private_field_message(expr.field_name));
                return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            return this->record_expr_type(expr_id, field.type);
        }
    }
    this->report(expr.range, sema_unknown_field_message(expr.field_name));
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_index_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr
) {
    const TypeHandle object = this->analyze_expr(expr.object);
    const TypeHandle index = this->analyze_expr(expr.index);
    if (!this->checked_.types.is_integer(index)) {
        this->report(this->module_.exprs[expr.index.value].range, std::string(SEMA_ARRAY_INDEX_INTEGER));
    }
    if (!is_valid(object)) {
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (this->checked_.types.is_array(object)) {
        return this->record_expr_type(expr_id, this->checked_.types.get(object).array_element);
    }
    if (this->checked_.types.is_slice(object)) {
        return this->record_expr_type(expr_id, this->checked_.types.get(object).slice_element);
    }
    if (this->checked_.types.is_pointer(object)) {
        const TypeHandle pointee = this->checked_.types.get(object).pointee;
        if (this->checked_.types.is_array(pointee)) {
            this->report(expr.range, std::string(SEMA_INDEX_POINTER_ARRAY_DEREF));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        if (!this->is_valid_storage_type(pointee)) {
            this->report(expr.range, std::string(SEMA_INDEX_POINTER_STORAGE));
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        return this->record_expr_type(expr_id, pointee);
    }
    this->report(expr.range, std::string(SEMA_INDEX_ARRAY_OR_POINTER));
    return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_slice_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
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
                bound.value < this->module_.exprs.size() ? this->module_.exprs[bound.value].range : expr.range,
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
        return this->record_expr_type(expr_id, expected_type);
    }
    return this->record_expr_type(expr_id, natural_slice);
}

TypeHandle SemanticAnalyzer::analyze_array_literal_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
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
            const syntax::ExprNode& count_expr = this->module_.exprs[expr.array_repeat_count.value];
            if (count_expr.kind != syntax::ExprKind::integer_literal) {
                this->report(count_expr.range, std::string(SEMA_ARRAY_REPEAT_INTEGER));
            } else if (!this->parse_integer_literal_text(count_expr.text, literal_count)) {
                this->report(count_expr.range, std::string(SEMA_ARRAY_REPEAT_OUT_OF_RANGE));
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
                    syntax::is_valid(element) && element.value < this->module_.exprs.size()
                        ? this->module_.exprs[element.value].range
                        : expr.range,
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

TypeHandle SemanticAnalyzer::analyze_struct_literal_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle
) {
    TypeHandle struct_type = INVALID_TYPE_HANDLE;
    const bool qualified = !expr.scope_name.empty();
    syntax::ModuleId scope_module = syntax::INVALID_MODULE_ID;
    if (qualified) {
        scope_module = this->resolve_import_alias(expr.scope_name, expr.scope_range);
        if (!syntax::is_valid(scope_module)) {
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }
    if (!expr.type_args.empty()) {
        std::vector<TypeHandle> args;
        args.reserve(expr.type_args.size());
        for (const syntax::TypeId arg : expr.type_args) {
            args.push_back(this->resolve_type(arg));
        }
        const GenericTemplateInfo* generic_struct = qualified
            ? this->find_generic_struct_in_module(scope_module, expr.struct_name, expr.range, false)
            : this->find_generic_struct_in_visible_modules(expr.struct_name, expr.range, false);
        if (generic_struct != nullptr) {
            syntax::TypeNode use_type;
            use_type.kind = syntax::TypeKind::named;
            use_type.scope_name = expr.scope_name;
            use_type.scope_range = expr.scope_range;
            use_type.name = expr.struct_name;
            use_type.range = expr.range;
            struct_type = this->instantiate_generic_struct(*generic_struct, use_type, syntax::INVALID_TYPE_ID, args);
        } else {
            struct_type = qualified
                ? this->find_type_in_module(scope_module, expr.struct_name, expr.range, false, false)
                : this->find_type_in_visible_modules(expr.struct_name, expr.range, false, false);
            if (is_valid(struct_type)) {
                this->report(expr.range, sema_type_not_generic_message(expr.struct_name));
            } else {
                static_cast<void>(qualified
                    ? this->find_generic_struct_in_module(scope_module, expr.struct_name, expr.range, true)
                    : this->find_generic_struct_in_visible_modules(expr.struct_name, expr.range, true));
            }
            return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    } else if (const GenericTemplateInfo* generic_struct = qualified
            ? this->find_generic_struct_in_module(scope_module, expr.struct_name, expr.range, false)
            : this->find_generic_struct_in_visible_modules(expr.struct_name, expr.range, false);
        generic_struct != nullptr) {
        this->report(expr.range, sema_generic_type_requires_args_message(expr.struct_name));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    } else {
        struct_type = qualified
            ? this->find_type_in_module(scope_module, expr.struct_name, expr.range, false)
            : this->find_type_in_visible_modules(expr.struct_name, expr.range, false);
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
    const syntax::ExprNode& expr
) {
    const TypeHandle source = this->analyze_expr(expr.cast_expr);
    const TypeHandle target = this->resolve_type(expr.cast_type);
    if (!this->is_valid_cast(expr.kind, target, source)) {
        this->report(expr.range, std::string(SEMA_INVALID_CONVERSION));
    }
    return this->record_expr_type(expr_id, target);
}

TypeHandle SemanticAnalyzer::analyze_size_or_align_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr
) {
    const TypeHandle queried = this->resolve_type(expr.cast_type);
    if (is_valid(queried) && this->checked_.types.get(queried).kind == TypeKind::generic_param) {
        this->report(expr.range, std::string(SEMA_GENERIC_SIZEOF_ALIGNOF));
    } else if (is_valid(queried) && this->checked_.types.get(queried).kind == TypeKind::opaque_struct) {
        this->report(expr.range, std::string(SEMA_OPAQUE_SIZEOF_ALIGNOF));
    } else if (is_valid(queried) && !this->is_valid_storage_type(queried)) {
        this->report(expr.range, std::string(SEMA_SIZEOF_ALIGNOF_STORAGE));
    }
    return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzer::analyze_ptr_addr_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr
) {
    const TypeHandle value = this->analyze_expr(expr.cast_expr);
    if (!this->checked_.types.is_pointer(value)) {
        this->report(expr.range, std::string(SEMA_PTRADDR_POINTER));
    }
    return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::usize));
}

TypeHandle SemanticAnalyzer::analyze_paddr_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr
) {
    const TypeHandle target = this->resolve_type(expr.cast_type);
    const TypeHandle address = this->analyze_expr(expr.cast_expr, this->checked_.types.builtin(BuiltinType::usize));
    if (!this->checked_.types.is_pointer(target)) {
        this->report(expr.range, std::string(SEMA_PTRAT_POINTER));
    }
    if (!this->checked_.types.is_integer(address)) {
        this->report(this->module_.exprs[expr.cast_expr.value].range, std::string(SEMA_PTRAT_INTEGER));
    }
    return this->record_expr_type(expr_id, target);
}

TypeHandle SemanticAnalyzer::analyze_str_projection_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr
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

TypeHandle SemanticAnalyzer::analyze_str_from_bytes_unchecked_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr
) {
    if (expr.args.size() != 2) {
        this->report(expr.range, std::string(SEMA_STRRAW_ARITY));
        return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
    }
    const TypeHandle data = this->analyze_expr(expr.args[0]);
    const TypeHandle len = this->analyze_expr(expr.args[1], this->checked_.types.builtin(BuiltinType::usize));
    if (!is_const_u8_pointer(this->checked_.types, data)) {
        this->report(this->module_.exprs[expr.args[0].value].range, std::string(SEMA_STRRAW_DATA_POINTER));
    }
    if (!this->checked_.types.is_integer(len)) {
        this->report(this->module_.exprs[expr.args[1].value].range, std::string(SEMA_STRRAW_LENGTH_INTEGER));
    }
    return this->record_expr_type(expr_id, this->checked_.types.builtin(BuiltinType::str));
}

TypeHandle SemanticAnalyzer::analyze_try_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    if (in_const_initializer_) {
        report(expr.range, std::string(SEMA_TRY_CONST_INITIALIZER));
    }

    const TypeHandle source_type = analyze_expr(expr.unary_operand);
    const EnumCaseInfo* const ok_case = find_enum_case_by_type_and_case(source_type, "ok");
    const EnumCaseInfo* const err_case = find_enum_case_by_type_and_case(source_type, "err");
    if (ok_case != nullptr || err_case != nullptr) {
        if (ok_case == nullptr || err_case == nullptr || !is_valid(ok_case->payload_type) || !is_valid(err_case->payload_type)) {
            report(expr.range, std::string(SEMA_TRY_RESULT_SHAPE));
            return record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }

        const EnumCaseInfo* const return_err_case = find_enum_case_by_type_and_case(current_function_return_type_, "err");
        if (return_err_case == nullptr || !is_valid(return_err_case->payload_type)) {
            report(expr.range, std::string(SEMA_TRY_RESULT_RETURN));
            return record_expr_type(expr_id, ok_case->payload_type);
        }
        if (!checked_.types.same(return_err_case->payload_type, err_case->payload_type)) {
            report(expr.range, std::string(SEMA_TRY_RESULT_ERR_PAYLOAD));
        }
        return record_expr_type(expr_id, ok_case->payload_type);
    }

    const EnumCaseInfo* const some_case = find_enum_case_by_type_and_case(source_type, "some");
    const EnumCaseInfo* const none_case = find_enum_case_by_type_and_case(source_type, "none");
    if (some_case != nullptr || none_case != nullptr) {
        if (some_case == nullptr || none_case == nullptr || !is_valid(some_case->payload_type) || is_valid(none_case->payload_type)) {
            report(expr.range, std::string(SEMA_TRY_OPTION_SHAPE));
            return record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }

        const EnumCaseInfo* const return_none_case = find_enum_case_by_type_and_case(current_function_return_type_, "none");
        if (return_none_case == nullptr || is_valid(return_none_case->payload_type)) {
            report(expr.range, std::string(SEMA_TRY_OPTION_RETURN));
            return record_expr_type(expr_id, some_case->payload_type);
        }
        return record_expr_type(expr_id, some_case->payload_type);
    }

    report(expr.range, std::string(SEMA_TRY_SHAPE));
    return record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzer::analyze_if_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (in_const_initializer_) {
        report(expr.range, std::string(SEMA_IF_EXPR_CONST_INITIALIZER));
    }
    const TypeHandle condition = analyze_expr(expr.condition);
    if (!checked_.types.is_bool(condition)) {
        report(module_.exprs[expr.condition.value].range, std::string(SEMA_IF_EXPR_CONDITION_BOOL));
    }

    const auto is_null_result_expr = [&](const syntax::ExprId candidate) {
        if (!syntax::is_valid(candidate) || candidate.value >= module_.exprs.size()) {
            return false;
        }
        const syntax::ExprNode& candidate_expr = module_.exprs[candidate.value];
        return candidate_expr.kind == syntax::ExprKind::null_literal ||
            (candidate_expr.kind == syntax::ExprKind::block_expr && is_null_literal(candidate_expr.block_result));
    };
    TypeHandle then_type = INVALID_TYPE_HANDLE;
    TypeHandle else_type = INVALID_TYPE_HANDLE;
    if (!is_valid(expected_type) && is_null_result_expr(expr.then_expr) && !is_null_result_expr(expr.else_expr)) {
        else_type = analyze_expr(expr.else_expr);
        then_type = analyze_expr(expr.then_expr, else_type);
    } else {
        then_type = analyze_expr(expr.then_expr, expected_type);
        else_type = analyze_expr(expr.else_expr, is_valid(then_type) ? then_type : expected_type);
        if (!is_valid(then_type) && checked_.types.is_pointer(else_type) && is_null_result_expr(expr.then_expr)) {
            then_type = analyze_expr(expr.then_expr, else_type);
        }
        if (!is_valid(else_type) && checked_.types.is_pointer(then_type) && is_null_result_expr(expr.else_expr)) {
            else_type = analyze_expr(expr.else_expr, then_type);
        }
    }
    if (!checked_.types.same(then_type, else_type)) {
        report(expr.range, std::string(SEMA_IF_EXPR_BRANCH_TYPE));
        return record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (is_valid(then_type) && checked_.types.is_void(then_type)) {
        report(expr.range, std::string(SEMA_IF_EXPR_VOID));
        return record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return record_expr_type(expr_id, then_type);
}

TypeHandle SemanticAnalyzer::analyze_block_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (in_const_initializer_) {
        report(expr.range, std::string(SEMA_BLOCK_EXPR_CONST_INITIALIZER));
    }
    if (!syntax::is_valid(expr.block_result)) {
        report(expr.range, std::string(SEMA_BLOCK_EXPR_FINAL));
        return record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    symbols_.push_scope();
    analyze_block_statements(expr.block, current_function_return_type_, current_return_inference_);
    if (!block_may_fallthrough(expr.block)) {
        report(expr.range, std::string(SEMA_BLOCK_EXPR_UNREACHABLE));
    }
    const TypeHandle result = analyze_expr(expr.block_result, expected_type);
    symbols_.pop_scope();

    if (!is_valid(result)) {
        return record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (checked_.types.is_void(result)) {
        report(expr.range, std::string(SEMA_BLOCK_EXPR_VOID));
        return record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return record_expr_type(expr_id, result);
}

} // namespace aurex::sema
