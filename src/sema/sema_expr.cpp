#include "aurex/sema/sema.hpp"

#include <algorithm>
#include <unordered_set>

namespace aurex::sema {

namespace {

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
) noexcept {
    if (!syntax::is_valid(candidate) || candidate.value >= module.exprs.size()) {
        return false;
    }
    const syntax::ExprNode& node = module.exprs[candidate.value];
    if (node.kind == syntax::ExprKind::integer_literal) {
        return true;
    }
    if (node.kind == syntax::ExprKind::unary && node.unary_op == syntax::UnaryOp::numeric_negate) {
        return is_contextual_integer_expr(module, node.unary_operand);
    }
    return node.kind == syntax::ExprKind::binary &&
           binary_result_uses_operand_type(node.binary_op) &&
           is_contextual_integer_expr(module, node.binary_lhs) &&
           is_contextual_integer_expr(module, node.binary_rhs);
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
        return 8;
    case BuiltinType::i16:
    case BuiltinType::u16:
        return 16;
    case BuiltinType::i32:
    case BuiltinType::u32:
        return 32;
    case BuiltinType::i64:
    case BuiltinType::u64:
        return 64;
    case BuiltinType::isize:
        return static_cast<base::u32>(sizeof(base::isize) * 8);
    case BuiltinType::usize:
        return static_cast<base::u32>(sizeof(base::usize) * 8);
    default:
        return 0;
    }
}

struct IntegerLiteralExpr {
    syntax::ExprId literal = syntax::invalid_expr_id;
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
    const base::u64 min_magnitude = bit_width >= 64
        ? (base::u64 {1} << 63)
        : (base::u64 {1} << (bit_width - 1));
    return value == min_magnitude;
}

} // namespace

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id) {
    return analyze_expr(expr_id, invalid_type_handle);
}

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id, const TypeHandle expected_type) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return invalid_type_handle;
    }

    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::integer_literal:
        return analyze_integer_literal(expr_id, expr, expected_type);
    case syntax::ExprKind::bool_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
    case syntax::ExprKind::null_literal:
        return record_expr_type(
            expr_id,
            checked_.types.is_pointer(expected_type) ? expected_type : invalid_type_handle
        );
    case syntax::ExprKind::string_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::str));
    case syntax::ExprKind::c_string_literal:
        return record_expr_type(expr_id, checked_.types.pointer(PointerMutability::const_, checked_.types.builtin(BuiltinType::u8)));
    case syntax::ExprKind::byte_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::u8));
    case syntax::ExprKind::name: {
        const Symbol* symbol = nullptr;
        if (!expr.scope_name.empty()) {
            const syntax::ModuleId module = resolve_import_alias(expr.scope_name, expr.scope_range);
            if (!syntax::is_valid(module)) {
                return record_expr_type(expr_id, invalid_type_handle);
            }
            symbol = find_symbol_in_module(module, expr.text, expr.range);
        } else {
            symbol = find_symbol(expr.text, expr.range);
        }
        if (symbol == nullptr) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        if (symbol->kind == SymbolKind::function) {
            report(expr.range, "function name cannot be used as a value: " + std::string(expr.text));
            return record_expr_type(expr_id, invalid_type_handle);
        }
        record_expr_c_name(expr_id, symbol->c_name);
        return record_expr_type(expr_id, symbol->type);
    }
    case syntax::ExprKind::call:
        return analyze_call_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::try_expr:
        return analyze_try_expr(expr_id, expr);
    case syntax::ExprKind::if_expr:
        return analyze_if_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::block_expr:
        return analyze_block_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::match_expr:
        return analyze_match_expr(expr_id, expr, expected_type);
    case syntax::ExprKind::unary: {
        if (expr.unary_op == syntax::UnaryOp::numeric_negate &&
            syntax::is_valid(expr.unary_operand) &&
            expr.unary_operand.value < module_.exprs.size() &&
            module_.exprs[expr.unary_operand.value].kind == syntax::ExprKind::integer_literal) {
            const syntax::ExprNode& operand_expr = module_.exprs[expr.unary_operand.value];
            const TypeHandle literal_type = checked_.types.is_integer(expected_type)
                ? expected_type
                : checked_.types.builtin(BuiltinType::i32);
            if (!negative_integer_literal_fits_type(literal_type, operand_expr.text)) {
                report(
                    expr.range,
                    "integer literal out of range for " + checked_.types.display_name(literal_type)
                );
            }
            static_cast<void>(record_expr_type(expr.unary_operand, literal_type));
            return record_expr_type(expr_id, literal_type);
        }
        const TypeHandle operand_expected =
            ((expr.unary_op == syntax::UnaryOp::numeric_negate &&
              (checked_.types.is_integer(expected_type) || checked_.types.is_float(expected_type))) ||
             (expr.unary_op == syntax::UnaryOp::bitwise_not && checked_.types.is_integer(expected_type)))
                ? expected_type
                : invalid_type_handle;
        const TypeHandle operand = analyze_expr(expr.unary_operand, operand_expected);
        if (expr.unary_op == syntax::UnaryOp::logical_not && !checked_.types.is_bool(operand)) {
            report(expr.range, "logical not requires bool operand");
        }
        if (expr.unary_op == syntax::UnaryOp::numeric_negate &&
            !is_signed_integer_type(checked_.types, operand) &&
            !checked_.types.is_float(operand)) {
            report(expr.range, "numeric unary operator requires signed integer or float operand");
        }
        if (expr.unary_op == syntax::UnaryOp::bitwise_not && !checked_.types.is_integer(operand)) {
            report(expr.range, "bitwise not requires integer operand");
        }
        if (expr.unary_op == syntax::UnaryOp::dereference) {
            if (!checked_.types.is_pointer(operand)) {
                report(expr.range, "dereference requires pointer operand");
                return record_expr_type(expr_id, invalid_type_handle);
            }
            return record_expr_type(expr_id, checked_.types.get(operand).pointee);
        }
        if (expr.unary_op == syntax::UnaryOp::address_of) {
            if (!is_place_expr(expr.unary_operand)) {
                report(expr.range, "address-of requires a place expression");
            }
            const PointerMutability mutability = is_writable_place(expr.unary_operand)
                ? PointerMutability::mut
                : PointerMutability::const_;
            return record_expr_type(expr_id, checked_.types.pointer(mutability, operand));
        }
        return record_expr_type(expr_id, operand);
    }
    case syntax::ExprKind::binary: {
        const TypeHandle operand_expected =
            binary_result_uses_operand_type(expr.binary_op) &&
            (checked_.types.is_integer(expected_type) || checked_.types.is_float(expected_type))
                ? expected_type
                : invalid_type_handle;
        TypeHandle lhs = invalid_type_handle;
        TypeHandle rhs = invalid_type_handle;
        if (!is_valid(operand_expected) &&
            is_contextual_integer_expr(module_, expr.binary_lhs) &&
            !is_contextual_integer_expr(module_, expr.binary_rhs)) {
            rhs = analyze_expr(expr.binary_rhs);
            lhs = analyze_expr(expr.binary_lhs, rhs);
        } else {
            lhs = analyze_expr(expr.binary_lhs, operand_expected);
            rhs = analyze_expr(expr.binary_rhs, is_valid(lhs) ? lhs : operand_expected);
        }
        const bool is_equality =
            expr.binary_op == syntax::BinaryOp::equal ||
            expr.binary_op == syntax::BinaryOp::not_equal;
        const bool is_null_pointer_comparison =
            is_equality &&
            ((checked_.types.is_pointer(lhs) && is_null_literal(expr.binary_rhs)) ||
             (checked_.types.is_pointer(rhs) && is_null_literal(expr.binary_lhs)));
        if (!checked_.types.same(lhs, rhs) && !is_null_pointer_comparison) {
            report(expr.range, "binary operands must have the same type");
        }
        const IntegerLiteralExpr rhs_integer_literal = integer_literal_expr(module_, expr.binary_rhs);
        if (syntax::is_valid(rhs_integer_literal.literal)) {
            base::u64 rhs_literal_value = 0;
            const syntax::ExprNode& literal_expr = module_.exprs[rhs_integer_literal.literal.value];
            const base::SourceRange rhs_range =
                syntax::is_valid(expr.binary_rhs) && expr.binary_rhs.value < module_.exprs.size()
                    ? module_.exprs[expr.binary_rhs.value].range
                    : literal_expr.range;
            if (parse_integer_literal_text(literal_expr.text, rhs_literal_value)) {
                if ((expr.binary_op == syntax::BinaryOp::div || expr.binary_op == syntax::BinaryOp::mod) &&
                    checked_.types.is_integer(lhs) &&
                    rhs_literal_value == 0) {
                    report(
                        rhs_range,
                        expr.binary_op == syntax::BinaryOp::div
                            ? "integer division by zero"
                            : "integer modulo by zero"
                    );
                }
                if (expr.binary_op == syntax::BinaryOp::shl || expr.binary_op == syntax::BinaryOp::shr) {
                    const base::u32 bit_width = integer_bit_width(checked_.types, lhs);
                    if (rhs_integer_literal.negated && rhs_literal_value != 0) {
                        report(rhs_range, "shift amount cannot be negative");
                    } else if (!rhs_integer_literal.negated &&
                               bit_width != 0 &&
                               rhs_literal_value >= bit_width) {
                        report(rhs_range, "shift amount is out of range");
                    }
                }
            }
        }
        if ((expr.binary_op == syntax::BinaryOp::div || expr.binary_op == syntax::BinaryOp::mod) &&
            is_signed_integer_type(checked_.types, lhs)) {
            const IntegerLiteralExpr lhs_integer_literal = integer_literal_expr(module_, expr.binary_lhs);
            if (syntax::is_valid(lhs_integer_literal.literal) &&
                syntax::is_valid(rhs_integer_literal.literal)) {
                base::u64 lhs_literal_value = 0;
                base::u64 rhs_literal_value = 0;
                const syntax::ExprNode& lhs_literal_expr = module_.exprs[lhs_integer_literal.literal.value];
                const syntax::ExprNode& rhs_literal_expr = module_.exprs[rhs_integer_literal.literal.value];
                if (parse_integer_literal_text(lhs_literal_expr.text, lhs_literal_value) &&
                    parse_integer_literal_text(rhs_literal_expr.text, rhs_literal_value) &&
                    is_signed_min_integer_literal(
                        lhs_integer_literal,
                        lhs_literal_value,
                        integer_bit_width(checked_.types, lhs)
                    ) &&
                    rhs_integer_literal.negated &&
                    rhs_literal_value == 1) {
                    report(
                        expr.range,
                        expr.binary_op == syntax::BinaryOp::div
                            ? "signed integer division overflows"
                            : "signed integer modulo overflows"
                    );
                }
            }
        }
        switch (expr.binary_op) {
        case syntax::BinaryOp::less:
        case syntax::BinaryOp::less_equal:
        case syntax::BinaryOp::greater:
        case syntax::BinaryOp::greater_equal:
            if (!checked_.types.is_integer(lhs) && !checked_.types.is_float(lhs)) {
                report(expr.range, "comparison operator requires numeric operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        case syntax::BinaryOp::equal:
        case syntax::BinaryOp::not_equal: {
            const bool scalar =
                checked_.types.is_bool(lhs) ||
                checked_.types.is_integer(lhs) ||
                checked_.types.is_float(lhs) ||
                checked_.types.is_pointer(lhs) ||
                (is_valid(lhs) &&
                 checked_.types.get(lhs).kind == TypeKind::enum_ &&
                 !is_valid(checked_.types.get(lhs).enum_payload_storage));
            if (!scalar && !is_null_pointer_comparison) {
                report(expr.range, "equality operator requires scalar operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        }
        case syntax::BinaryOp::logical_and:
        case syntax::BinaryOp::logical_or:
            if (!checked_.types.is_bool(lhs) || !checked_.types.is_bool(rhs)) {
                report(expr.range, "logical operator requires bool operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        case syntax::BinaryOp::bit_and:
        case syntax::BinaryOp::bit_xor:
        case syntax::BinaryOp::bit_or:
        case syntax::BinaryOp::shl:
        case syntax::BinaryOp::shr:
        case syntax::BinaryOp::mod:
            if (!checked_.types.is_integer(lhs)) {
                report(expr.range, "integer operator requires integer operands");
            }
            return record_expr_type(expr_id, lhs);
        default:
            if (!checked_.types.is_integer(lhs) && !checked_.types.is_float(lhs)) {
                report(expr.range, "binary operator requires numeric operands");
            }
            return record_expr_type(expr_id, lhs);
        }
    }
    case syntax::ExprKind::field: {
        if (syntax::is_valid(expr.object) &&
            expr.object.value < module_.exprs.size() &&
            module_.exprs[expr.object.value].kind == syntax::ExprKind::name) {
            const syntax::ExprNode& object = module_.exprs[expr.object.value];
            const GenericEnumTemplateInfo* enum_template = nullptr;
            if (object.scope_name.empty()) {
                enum_template = find_generic_enum_template_in_visible_modules(object.text, expr.range, false);
            } else {
                const syntax::ModuleId scope_module = resolve_import_alias(object.scope_name, object.scope_range, false);
                if (syntax::is_valid(scope_module)) {
                    enum_template = find_generic_enum_template_in_module(scope_module, object.text, expr.range, false);
                }
            }
            if (enum_template != nullptr) {
                const EnumCaseInfo* enum_case = nullptr;
                if (const GenericEnumInstanceInfo* expected_instance = generic_enum_instance(expected_type);
                    expected_instance != nullptr &&
                    expected_instance->name == enum_template->name &&
                    expected_instance->module.value == enum_template->module.value) {
                    enum_case = find_enum_case_by_type_and_case(expected_type, expr.field_name);
                }
                if (enum_case == nullptr) {
                    enum_case = instantiate_generic_enum_constructor(expr_id, {}, expected_type, true);
                }
                if (enum_case == nullptr) {
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                if (is_valid(enum_case->payload_type)) {
                    report(expr.range, "enum payload constructor requires a call: " + enum_case->name);
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                record_expr_c_name(expr_id, enum_case->c_name);
                return record_expr_type(expr_id, enum_case->type);
            }
            if (!object.scope_name.empty() || !object.type_args.empty()) {
                const TypeHandle enum_type = resolve_associated_type_owner(object, false);
                if (is_valid(enum_type) && checked_.types.get(enum_type).kind == TypeKind::enum_) {
                    const EnumCaseInfo* enum_case = find_enum_case_by_type_and_case(enum_type, expr.field_name);
                    if (enum_case == nullptr) {
                        report(expr.range, "unknown enum case: " + std::string(object.text) + "." + std::string(expr.field_name));
                        return record_expr_type(expr_id, invalid_type_handle);
                    }
                    if (is_valid(enum_case->payload_type)) {
                        report(expr.range, "enum payload constructor requires a call: " + enum_case->name);
                        return record_expr_type(expr_id, invalid_type_handle);
                    }
                    record_expr_c_name(expr_id, enum_case->c_name);
                    return record_expr_type(expr_id, enum_case->type);
                }
            }
            if (object.scope_name.empty()) {
                if (const EnumCaseInfo* enum_case = find_enum_case_by_scoped_name(object.text, expr.field_name, expr.range, false);
                    enum_case != nullptr) {
                    if (is_valid(enum_case->payload_type)) {
                        report(expr.range, "enum payload constructor requires a call: " + enum_case->name);
                        return record_expr_type(expr_id, invalid_type_handle);
                    }
                    record_expr_c_name(expr_id, enum_case->c_name);
                    return record_expr_type(expr_id, enum_case->type);
                }
            }
        }
        TypeHandle object = analyze_expr(expr.object);
        if (is_valid(object) && !checked_.types.is_pointer(object) && !is_place_expr(expr.object)) {
            report(expr.range, "field access requires addressable storage or a pointer receiver");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        if (checked_.types.is_pointer(object)) {
            object = checked_.types.get(object).pointee;
        }
        const StructInfo* info = find_struct(object);
        if (info == nullptr || info->is_opaque) {
            report(expr.range, "field access requires a non-opaque struct value");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        for (const StructFieldInfo& field : info->fields) {
            if (field.name == expr.field_name) {
                if (!can_access(info->module, field.visibility)) {
                    report(expr.range, "field is private: " + std::string(expr.field_name));
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                return record_expr_type(expr_id, field.type);
            }
        }
        report(expr.range, "unknown field: " + std::string(expr.field_name));
        return record_expr_type(expr_id, invalid_type_handle);
    }
    case syntax::ExprKind::index: {
        const TypeHandle object = analyze_expr(expr.object);
        const TypeHandle index = analyze_expr(expr.index);
        if (!checked_.types.is_integer(index)) {
            report(module_.exprs[expr.index.value].range, "array index must be an integer");
        }
        if (checked_.types.is_array(object)) {
            return record_expr_type(expr_id, checked_.types.get(object).array_element);
        }
        if (checked_.types.is_pointer(object)) {
            return record_expr_type(expr_id, checked_.types.get(object).pointee);
        }
        report(expr.range, "indexing requires array or pointer value");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    case syntax::ExprKind::struct_literal: {
        TypeHandle struct_type = invalid_type_handle;
        const bool qualified = !expr.scope_name.empty();
        syntax::ModuleId scope_module = syntax::invalid_module_id;
        if (qualified) {
            scope_module = resolve_import_alias(expr.scope_name, expr.scope_range);
            if (!syntax::is_valid(scope_module)) {
                return record_expr_type(expr_id, invalid_type_handle);
            }
        }
        if (!expr.struct_type_args.empty()) {
            const GenericStructTemplateInfo* template_info = qualified
                ? find_generic_struct_template_in_module(scope_module, expr.struct_name, expr.range, false)
                : find_generic_struct_template_in_visible_modules(expr.struct_name, expr.range, false);
            if (template_info != nullptr) {
                struct_type = instantiate_generic_struct_from_syntax(
                    *template_info,
                    expr.struct_type_args,
                    expr.range,
                    false
                );
            } else {
                report(
                    expr.range,
                    "type arguments require a generic struct: " +
                        (qualified
                            ? std::string(expr.scope_name) + "::" + std::string(expr.struct_name)
                            : std::string(expr.struct_name))
                );
            }
        } else {
            const GenericStructTemplateInfo* template_info = qualified
                ? find_generic_struct_template_in_module(scope_module, expr.struct_name, expr.range, false)
                : find_generic_struct_template_in_visible_modules(expr.struct_name, expr.range, false);
            if (template_info != nullptr) {
                struct_type = infer_generic_struct_literal_type(*template_info, expr, expected_type);
                if (!is_valid(struct_type)) {
                    report(
                        expr.range,
                        "generic struct literal requires explicit type arguments: " +
                            (qualified
                                ? std::string(expr.scope_name) + "::" + template_info->name
                                : template_info->name)
                    );
                    return record_expr_type(expr_id, invalid_type_handle);
                }
            } else {
                struct_type = qualified
                    ? find_type_in_module(scope_module, expr.struct_name, expr.range, false)
                    : find_type_in_visible_modules(expr.struct_name, expr.range, false);
            }
        }
        if (!is_valid(struct_type)) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        const StructInfo* info = find_struct(struct_type);
        if (info == nullptr || info->is_opaque) {
            report(expr.range, "struct literal requires a non-opaque struct type");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        std::unordered_set<std::string> initialized_fields;
        for (const syntax::FieldInit& init : expr.field_inits) {
            if (!initialized_fields.insert(std::string(init.name)).second) {
                report(init.range, "duplicate struct literal field: " + std::string(init.name));
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
                report(init.range, "unknown field in struct literal: " + std::string(init.name));
                continue;
            }
            if (!can_access(info->module, field_info->visibility)) {
                report(init.range, "field is private: " + std::string(init.name));
                continue;
            }
            const TypeHandle actual = analyze_expr(init.value, field_info->type);
            if (!can_assign(field_info->type, actual, init.value)) {
                report(init.range, "struct literal field type mismatch");
            }
        }
        for (const StructFieldInfo& field : info->fields) {
            if (!initialized_fields.contains(field.name)) {
                report(expr.range, "struct literal is missing field: " + field.name);
            }
        }
        return record_expr_type(expr_id, struct_type);
    }
    case syntax::ExprKind::cast:
    case syntax::ExprKind::ptr_cast:
    case syntax::ExprKind::bit_cast: {
        const TypeHandle source = analyze_expr(expr.cast_expr);
        const TypeHandle target = resolve_type(expr.cast_type);
        if (!is_valid_cast(expr.kind, target, source)) {
            report(expr.range, "invalid explicit conversion");
        }
        return record_expr_type(expr_id, target);
    }
    case syntax::ExprKind::size_of:
    case syntax::ExprKind::align_of: {
        const TypeHandle queried = resolve_type(expr.cast_type);
        if (is_valid(queried) && checked_.types.get(queried).kind == TypeKind::opaque_struct) {
            report(expr.range, "opaque struct cannot be queried by size_of or align_of directly");
        } else if (is_valid(queried) && !is_valid_storage_type(queried)) {
            report(expr.range, "size_of and align_of require a valid storage type");
        }
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::usize));
    }
    case syntax::ExprKind::ptr_addr: {
        const TypeHandle value = analyze_expr(expr.cast_expr);
        if (!checked_.types.is_pointer(value)) {
            report(expr.range, "ptr_addr requires a pointer value");
        }
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::usize));
    }
    case syntax::ExprKind::ptr_from_addr: {
        const TypeHandle target = resolve_type(expr.cast_type);
        const TypeHandle address = analyze_expr(expr.cast_expr, checked_.types.builtin(BuiltinType::usize));
        if (!checked_.types.is_pointer(target)) {
            report(expr.range, "ptr_from_addr target type must be a pointer");
        }
        if (!checked_.types.is_integer(address)) {
            report(module_.exprs[expr.cast_expr.value].range, "ptr_from_addr address must be an integer");
        }
        return record_expr_type(expr_id, target);
    }
    case syntax::ExprKind::invalid:
        return record_expr_type(expr_id, invalid_type_handle);
    }
    return record_expr_type(expr_id, invalid_type_handle);
}

TypeHandle SemanticAnalyzer::analyze_try_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    if (in_const_initializer_) {
        report(expr.range, "try expression cannot be used in const initializer");
    }

    const TypeHandle source_type = analyze_expr(expr.unary_operand);
    const GenericEnumInstanceInfo* const source_instance = generic_enum_instance(source_type);
    if (source_instance == nullptr) {
        report(expr.range, "try expression requires Result<T, E> or Option<T>");
        return record_expr_type(expr_id, invalid_type_handle);
    }

    if (source_instance->name == "Result" && source_instance->args.size() == 2) {
        const EnumCaseInfo* const ok_case = find_enum_case_by_type_and_case(source_type, "ok");
        const EnumCaseInfo* const err_case = find_enum_case_by_type_and_case(source_type, "err");
        if (ok_case == nullptr || err_case == nullptr || !is_valid(ok_case->payload_type) || !is_valid(err_case->payload_type)) {
            report(expr.range, "try expression Result type must define ok(T) and err(E) cases");
            return record_expr_type(expr_id, invalid_type_handle);
        }

        const GenericEnumInstanceInfo* const return_instance = generic_enum_instance(current_function_return_type_);
        if (return_instance == nullptr ||
            return_instance->name != "Result" ||
            return_instance->module.value != source_instance->module.value ||
            return_instance->args.size() != 2) {
            report(expr.range, "try expression on Result<T, E> requires enclosing function to return Result<U, E>");
            return record_expr_type(expr_id, ok_case->payload_type);
        }
        if (!checked_.types.same(return_instance->args[1], source_instance->args[1])) {
            report(expr.range, "try expression Result error type must match enclosing Result error type");
        }
        const EnumCaseInfo* const return_err_case = find_enum_case_by_type_and_case(current_function_return_type_, "err");
        if (return_err_case == nullptr || !is_valid(return_err_case->payload_type)) {
            report(expr.range, "enclosing Result return type must define err(E)");
        } else if (!checked_.types.same(return_err_case->payload_type, err_case->payload_type)) {
            report(expr.range, "try expression Result error payload type must match enclosing Result error payload type");
        }
        return record_expr_type(expr_id, ok_case->payload_type);
    }

    if (source_instance->name == "Option" && source_instance->args.size() == 1) {
        const EnumCaseInfo* const some_case = find_enum_case_by_type_and_case(source_type, "some");
        const EnumCaseInfo* const none_case = find_enum_case_by_type_and_case(source_type, "none");
        if (some_case == nullptr || none_case == nullptr || !is_valid(some_case->payload_type) || is_valid(none_case->payload_type)) {
            report(expr.range, "try expression Option type must define some(T) and none cases");
            return record_expr_type(expr_id, invalid_type_handle);
        }

        const GenericEnumInstanceInfo* const return_instance = generic_enum_instance(current_function_return_type_);
        if (return_instance == nullptr ||
            return_instance->name != "Option" ||
            return_instance->module.value != source_instance->module.value ||
            return_instance->args.size() != 1) {
            report(expr.range, "try expression on Option<T> requires enclosing function to return Option<U>");
            return record_expr_type(expr_id, some_case->payload_type);
        }
        const EnumCaseInfo* const return_none_case = find_enum_case_by_type_and_case(current_function_return_type_, "none");
        if (return_none_case == nullptr || is_valid(return_none_case->payload_type)) {
            report(expr.range, "enclosing Option return type must define none");
        }
        return record_expr_type(expr_id, some_case->payload_type);
    }

    report(expr.range, "try expression requires Result<T, E> or Option<T>");
    return record_expr_type(expr_id, invalid_type_handle);
}

TypeHandle SemanticAnalyzer::analyze_if_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (in_const_initializer_) {
        report(expr.range, "if expression cannot be used in const initializer");
    }
    const TypeHandle condition = analyze_expr(expr.condition);
    if (!checked_.types.is_bool(condition)) {
        report(module_.exprs[expr.condition.value].range, "if expression condition must be bool");
    }

    const auto is_null_result_expr = [&](const syntax::ExprId candidate) {
        if (!syntax::is_valid(candidate) || candidate.value >= module_.exprs.size()) {
            return false;
        }
        const syntax::ExprNode& candidate_expr = module_.exprs[candidate.value];
        return candidate_expr.kind == syntax::ExprKind::null_literal ||
            (candidate_expr.kind == syntax::ExprKind::block_expr && is_null_literal(candidate_expr.block_result));
    };
    TypeHandle then_type = invalid_type_handle;
    TypeHandle else_type = invalid_type_handle;
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
        report(expr.range, "if expression branches must have the same type");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    if (is_valid(then_type) && checked_.types.is_void(then_type)) {
        report(expr.range, "if expression branches cannot be void");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    return record_expr_type(expr_id, then_type);
}

TypeHandle SemanticAnalyzer::analyze_block_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (in_const_initializer_) {
        report(expr.range, "block expression cannot be used in const initializer");
    }
    if (!syntax::is_valid(expr.block_result)) {
        report(expr.range, "block expression requires a final expression");
        return record_expr_type(expr_id, invalid_type_handle);
    }

    symbols_.push_scope();
    if (syntax::is_valid(expr.block) && expr.block.value < module_.stmts.size()) {
        const syntax::StmtNode& block = module_.stmts[expr.block.value];
        for (syntax::StmtId child : block.statements) {
            analyze_stmt(child, checked_.types.builtin(BuiltinType::void_), nullptr);
        }
    }
    const TypeHandle result = analyze_expr(expr.block_result, expected_type);
    symbols_.pop_scope();

    if (!is_valid(result)) {
        return record_expr_type(expr_id, invalid_type_handle);
    }
    if (checked_.types.is_void(result)) {
        report(expr.range, "block expression result cannot be void");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    return record_expr_type(expr_id, result);
}

} // namespace aurex::sema
