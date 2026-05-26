#include <aurex/sema/sema_messages.hpp>

#include <string>
#include <unordered_set>
#include <vector>

#include <sema/internal/sema_projection_aggregate_expression_analyzer.hpp>

namespace aurex::sema {

namespace {

struct IntegerLiteralExpr {
    syntax::ExprId literal = syntax::INVALID_EXPR_ID;
    bool negated = false;
};

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

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module, const syntax::ExprId expr, const base::SourceRange& fallback) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size() ? module.exprs.range(expr.value) : fallback;
}

} // namespace

TypeHandle SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer::analyze_field_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle)
{
    if (syntax::is_valid(expr.object) && expr.object.value < this->core_.ctx_.module.exprs.size()) {
        const ModuleSelector module = this->core_.resolve_module_selector(expr.object, false);
        if (syntax::is_valid(module.module)) {
            return this->core_.analyze_module_member_expr(expr_id, module.module, expr);
        }
        if (module.failed_as_module_selector) {
            static_cast<void>(this->core_.resolve_module_selector(expr.object, true));
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
        const bool object_is_plain_name = expr_name_is_unqualified(this->core_.ctx_.module, expr.object);
        const TypeHandle enum_type = this->core_.resolve_type_selector(expr.object, !object_is_plain_name);
        if (is_valid(enum_type) && this->core_.state_.checked.types.get(enum_type).kind == TypeKind::enum_) {
            const EnumCaseInfo* enum_case =
                this->core_.find_enum_case_by_type_and_case(enum_type, expr.field_name_id, expr.field_name);
            if (enum_case == nullptr) {
                this->core_.report_lookup(expr.range,
                    sema_unknown_scoped_enum_case_message(
                        this->core_.state_.checked.types.display_name(enum_type), expr.field_name));
                this->core_.report_lookup_suggestion(
                    expr.range, this->core_.nearest_enum_case_name(enum_type, expr.field_name));
                return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
            const bool recorded = this->core_.record_no_payload_enum_case_expr(expr_id, *enum_case, expr.range);
            return recorded ? enum_case->type : INVALID_TYPE_HANDLE;
        }
        if (object_is_plain_name && is_valid(enum_type)) {
            this->core_.report_general(expr.range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }

    const PlaceInfo place = this->core_.analyze_place_info(expr_id, true);
    this->core_.require_place_projection_safety(place, expr.range);
    return this->core_.record_expr_type(expr_id, place.type);
}

TypeHandle SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer::analyze_module_member_expr(
    const syntax::ExprId expr_id, const syntax::ModuleId module, const SemanticAnalyzerCore::ExprView& expr)
{
    if (const Symbol* symbol =
            this->core_.find_symbol_in_module(module, expr.field_name_id, expr.field_name, expr.range, false);
        symbol != nullptr) {
        if (symbol->kind == SymbolKind::function) {
            this->core_.record_expr_c_name(expr_id, symbol->c_name);
            return this->core_.record_expr_type(expr_id, this->core_.function_type_from_symbol(*symbol, expr.range));
        }
        this->core_.record_expr_c_name(expr_id, symbol->c_name);
        return this->core_.record_expr_type(expr_id, symbol->type);
    }
    if (is_valid(
            this->core_.find_type_in_module(module, expr.field_name_id, expr.field_name, expr.range, false, false))) {
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (this->core_.generic_type_template_exists_in_module(module, expr.field_name_id, expr.field_name)) {
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    static_cast<void>(this->core_.find_symbol_in_module(module, expr.field_name_id, expr.field_name, expr.range, true));
    return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer::analyze_index_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr)
{
    const PlaceInfo place = this->core_.analyze_place_info(expr_id, true);
    this->core_.require_place_projection_safety(place, expr.range);
    return this->core_.record_expr_type(expr_id, place.type);
}

TypeHandle SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer::analyze_slice_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    struct ConstantSliceBound {
        bool present = false;
        bool valid = false;
        bool negated = false;
        base::u64 value = 0;
        base::SourceRange range;
    };

    const TypeHandle usize_type = this->core_.state_.checked.types.builtin(BuiltinType::usize);
    const auto analyze_bound = [&](const syntax::ExprId bound) {
        if (!syntax::is_valid(bound)) {
            return;
        }
        const TypeHandle bound_type = this->core_.analyze_expr(bound, usize_type);
        if (!this->core_.state_.checked.types.is_integer(bound_type)) {
            this->core_.report_general(
                expr_range_or(this->core_.ctx_.module, bound, expr.range), std::string(SEMA_SLICE_BOUND_INTEGER));
        }
    };
    const auto constant_bound = [&](const syntax::ExprId bound) {
        ConstantSliceBound result;
        if (!syntax::is_valid(bound)) {
            return result;
        }
        result.present = true;
        result.range = expr_range_or(this->core_.ctx_.module, bound, expr.range);
        const IntegerLiteralExpr literal = integer_literal_expr(this->core_.ctx_.module, bound);
        if (!syntax::is_valid(literal.literal)) {
            return result;
        }
        const syntax::LiteralExprPayload* const literal_expr =
            this->core_.ctx_.module.exprs.literal_payload(literal.literal.value);
        result.negated = literal.negated;
        result.valid =
            literal_expr != nullptr && this->core_.parse_integer_literal_text(literal_expr->text, result.value);
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
            this->core_.report_general(start.range, std::string(SEMA_ARRAY_SLICE_BOUND_OUT_OF_BOUNDS));
        }
        if (bound_is_out_of_bounds(end)) {
            this->core_.report_general(end.range, std::string(SEMA_ARRAY_SLICE_BOUND_OUT_OF_BOUNDS));
        }
        if (start.present && end.present && start.valid && end.valid && !start.negated && !end.negated
            && start.value <= array_count && end.value <= array_count && start.value > end.value) {
            this->core_.report_general(expr.range, std::string(SEMA_ARRAY_SLICE_BOUNDS_ORDER));
        }
    };

    const TypeHandle object = this->core_.analyze_expr(expr.object);
    analyze_bound(expr.slice_start);
    analyze_bound(expr.slice_end);
    if (!is_valid(object)) {
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    TypeHandle element = INVALID_TYPE_HANDLE;
    PointerMutability mutability = PointerMutability::const_;
    if (this->core_.state_.checked.types.is_array(object)) {
        const TypeInfo& array = this->core_.state_.checked.types.get(object);
        check_array_slice_bounds(array.array_count);
        element = array.array_element;
        mutability = this->core_.is_writable_place(expr.object) ? PointerMutability::mut : PointerMutability::const_;
    } else if (this->core_.state_.checked.types.is_slice(object)) {
        const TypeInfo& slice = this->core_.state_.checked.types.get(object);
        element = slice.slice_element;
        mutability = slice.slice_mutability;
    } else if (this->core_.state_.checked.types.is_str(object)) {
        return this->core_.record_expr_type(expr_id, this->core_.state_.checked.types.builtin(BuiltinType::str));
    } else {
        this->core_.report_general(expr.range, std::string(SEMA_SLICE_ARRAY_OR_SLICE));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (!this->core_.is_valid_storage_type(element)) {
        this->core_.report_general(expr.range, std::string(SEMA_SLICE_ELEMENT_STORAGE));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle natural_slice = this->core_.state_.checked.types.slice(mutability, element);
    if (this->core_.state_.checked.types.is_slice(expected_type)
        && this->core_.can_assign(expected_type, natural_slice, syntax::INVALID_EXPR_ID)) {
        this->core_.record_coercion(expr_id, natural_slice, expected_type, CoercionKind::slice_to_expected_slice);
        return this->core_.record_expr_types(expr_id, natural_slice, expected_type);
    }
    return this->core_.record_expr_types(expr_id, natural_slice, natural_slice);
}

TypeHandle SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer::analyze_array_literal_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    const bool has_expected_array = this->core_.state_.checked.types.is_array(expected_type);
    TypeHandle element_type = INVALID_TYPE_HANDLE;
    base::u64 expected_count = 0;
    if (is_valid(expected_type)) {
        if (!has_expected_array) {
            this->core_.report_general(expr.range, std::string(SEMA_ARRAY_LITERAL_EXPECTED_TYPE));
        } else {
            const TypeInfo& expected = this->core_.state_.checked.types.get(expected_type);
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
        const bool count_expr_valid = syntax::is_valid(expr.array_repeat_count)
            && expr.array_repeat_count.value < this->core_.ctx_.module.exprs.size();
        if (!count_expr_valid) {
            this->core_.report_general(expr.range, std::string(SEMA_ARRAY_REPEAT_INTEGER));
        } else {
            const base::SourceRange count_range = this->core_.ctx_.module.exprs.range(expr.array_repeat_count.value);
            if (this->core_.ctx_.module.exprs.kind(expr.array_repeat_count.value)
                != syntax::ExprKind::integer_literal) {
                this->core_.report_general(count_range, std::string(SEMA_ARRAY_REPEAT_INTEGER));
            } else if (!this->core_.parse_integer_literal_text(
                           expr_literal_text_or_empty(this->core_.ctx_.module, expr.array_repeat_count),
                           literal_count)) {
                this->core_.report_general(count_range, std::string(SEMA_ARRAY_REPEAT_OUT_OF_RANGE));
            } else {
                count_known = true;
            }
        }
    }

    if (has_expected_array && count_known && literal_count != expected_count) {
        this->core_.report_general(
            expr.range, sema_array_literal_length_mismatch_message(expected_count, literal_count));
    }

    if (!has_expected_array && !is_repeat_literal && expr.array_elements.empty()) {
        this->core_.report_general(expr.range, std::string(SEMA_EMPTY_ARRAY_CONTEXT));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (!has_expected_array) {
        const syntax::ExprId first_value = is_repeat_literal ? expr.array_repeat_value : expr.array_elements.front();
        element_type = this->core_.analyze_expr(first_value);
        if (!is_valid(element_type)) {
            this->core_.report_general(expr.range, std::string(SEMA_ARRAY_ELEMENT_INFER));
            return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
        }
    }

    if (is_repeat_literal) {
        const TypeHandle actual = this->core_.analyze_expr(expr.array_repeat_value, element_type);
        if (!this->core_.can_assign(element_type, actual, expr.array_repeat_value)) {
            this->core_.report_type_mismatch(
                expr.range, std::string(SEMA_ARRAY_REPEAT_TYPE_MISMATCH), element_type, actual);
        }
    } else {
        for (const syntax::ExprId element : expr.array_elements) {
            const TypeHandle actual = this->core_.analyze_expr(element, element_type);
            if (!this->core_.can_assign(element_type, actual, element)) {
                this->core_.report_type_mismatch(expr_range_or(this->core_.ctx_.module, element, expr.range),
                    std::string(SEMA_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH), element_type, actual);
            }
        }
    }

    TypeHandle intrinsic_element_type = INVALID_TYPE_HANDLE;
    if (is_repeat_literal) {
        intrinsic_element_type = this->core_.cached_expr_intrinsic_type(expr.array_repeat_value);
    } else if (!expr.array_elements.empty()) {
        intrinsic_element_type = this->core_.cached_expr_intrinsic_type(expr.array_elements.front());
    }
    if (!is_valid(intrinsic_element_type)) {
        intrinsic_element_type = element_type;
    }

    if (!is_valid(element_type) || !count_known) {
        return this->core_.record_expr_types(
            expr_id, INVALID_TYPE_HANDLE, has_expected_array ? expected_type : INVALID_TYPE_HANDLE);
    }
    const TypeHandle array_type =
        has_expected_array ? expected_type : this->core_.state_.checked.types.array(literal_count, element_type);
    const TypeHandle intrinsic_array_type = is_valid(intrinsic_element_type)
        ? this->core_.state_.checked.types.array(literal_count, intrinsic_element_type)
        : INVALID_TYPE_HANDLE;
    if (!this->core_.is_valid_storage_type(array_type)) {
        this->core_.report_general(expr.range, std::string(SEMA_ARRAY_LITERAL_STORAGE));
    }
    return this->core_.record_expr_types(expr_id, intrinsic_array_type, array_type);
}

TypeHandle SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer::analyze_tuple_literal_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    const bool has_expected_tuple = this->core_.state_.checked.types.is_tuple(expected_type);
    std::vector<TypeHandle> element_types;
    if (is_valid(expected_type)) {
        if (!has_expected_tuple) {
            this->core_.report_general(expr.range, std::string(SEMA_TUPLE_LITERAL_EXPECTED_TYPE));
        } else {
            const TypeInfo& expected = this->core_.state_.checked.types.get(expected_type);
            if (expected.tuple_elements.size() != expr.tuple_elements.size()) {
                this->core_.report_general(expr.range, std::string(SEMA_TUPLE_LITERAL_ARITY));
            }
            element_types.assign(expected.tuple_elements.begin(), expected.tuple_elements.end());
        }
    }

    if (!has_expected_tuple) {
        element_types.assign(expr.tuple_elements.size(), INVALID_TYPE_HANDLE);
    }

    for (base::usize i = 0; i < expr.tuple_elements.size(); ++i) {
        const TypeHandle expected_element = i < element_types.size() ? element_types[i] : INVALID_TYPE_HANDLE;
        const TypeHandle actual = this->core_.analyze_expr(expr.tuple_elements[i], expected_element);
        if (i >= element_types.size()) {
            continue;
        }
        if (has_expected_tuple) {
            if (!this->core_.can_assign(element_types[i], actual, expr.tuple_elements[i])) {
                this->core_.report_type_mismatch(
                    expr_range_or(this->core_.ctx_.module, expr.tuple_elements[i], expr.range),
                    std::string(SEMA_TUPLE_LITERAL_ELEMENT_TYPE_MISMATCH), element_types[i], actual);
            }
        } else {
            element_types[i] = actual;
        }
    }

    if (!has_expected_tuple) {
        for (const TypeHandle element : element_types) {
            if (!is_valid(element)) {
                this->core_.report_general(expr.range, std::string(SEMA_LOCAL_TYPE_INFER));
                return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
            }
        }
    }

    const TypeHandle tuple_type =
        has_expected_tuple ? expected_type : this->core_.state_.checked.types.tuple(element_types);
    std::vector<TypeHandle> intrinsic_element_types;
    intrinsic_element_types.reserve(expr.tuple_elements.size());
    bool has_intrinsic_tuple = true;
    for (base::usize i = 0; i < expr.tuple_elements.size(); ++i) {
        TypeHandle intrinsic = this->core_.cached_expr_intrinsic_type(expr.tuple_elements[i]);
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
        has_intrinsic_tuple ? this->core_.state_.checked.types.tuple(intrinsic_element_types) : INVALID_TYPE_HANDLE;
    if (is_valid(tuple_type) && !this->core_.is_valid_storage_type(tuple_type)) {
        this->core_.report_general(expr.range, std::string(SEMA_TUPLE_LITERAL_STORAGE));
    }
    return this->core_.record_expr_types(expr_id, intrinsic_tuple_type, tuple_type);
}

TypeHandle SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer::analyze_struct_literal_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle)
{
    TypeHandle struct_type = INVALID_TYPE_HANDLE;
    if (syntax::is_valid(expr.object)) {
        struct_type = this->core_.resolve_type_selector(expr.object, true);
    } else {
        NamedTypeSelector selector;
        selector.module = syntax::INVALID_MODULE_ID;
        selector.name = expr.struct_name;
        selector.name_id = expr.struct_name_id;
        selector.range = expr.range;
        selector.type_args.assign(expr.type_args.begin(), expr.type_args.end());
        selector.qualified = false;
        struct_type = this->core_.resolve_named_type_selector_type(selector, false, true);
    }
    if (!is_valid(struct_type)) {
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const StructInfo* info = this->core_.find_struct(struct_type);
    if (info == nullptr || info->is_opaque) {
        this->core_.report_general(expr.range, std::string(SEMA_STRUCT_LITERAL_TYPE));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    std::unordered_set<IdentId, IdentIdHash> initialized_fields;
    for (const syntax::FieldInit& init : expr.field_inits) {
        if (!initialized_fields.insert(init.name_id).second) {
            this->core_.report_duplicate(init.range, sema_duplicate_struct_literal_field_message(init.name));
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
            this->core_.report_lookup(init.range, sema_unknown_struct_literal_field_message(init.name));
            this->core_.report_lookup_suggestion(init.range, this->core_.nearest_field_name(*info, init.name));
            continue;
        }
        if (!this->core_.can_access_module(info->module, field_info->visibility)) {
            this->core_.report_visibility(init.range, sema_private_field_message(init.name));
            continue;
        }
        const TypeHandle actual = this->core_.analyze_expr(init.value, field_info->type);
        if (!this->core_.can_assign(field_info->type, actual, init.value)) {
            this->core_.report_type_mismatch(
                init.range, std::string(SEMA_STRUCT_LITERAL_FIELD_TYPE_MISMATCH), field_info->type, actual);
        }
    }
    for (const StructFieldInfo& field : info->fields) {
        if (!initialized_fields.contains(field.name_id)) {
            this->core_.report_general(expr.range, sema_struct_literal_missing_field_message(field.name));
        }
    }
    return this->core_.record_expr_type(expr_id, struct_type);
}

SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer::ProjectionAggregateExpressionAnalyzer(
    SemanticAnalyzerCore& core) noexcept
    : core_(core)
{
}

SemanticAnalyzerCore::ProjectionAggregateExpressionAnalyzer
SemanticAnalyzerCore::projection_aggregate_expression_analyzer() noexcept
{
    return ProjectionAggregateExpressionAnalyzer(*this);
}

TypeHandle SemanticAnalyzerCore::analyze_field_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->projection_aggregate_expression_analyzer().analyze_field_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_module_member_expr(
    const syntax::ExprId expr_id, const syntax::ModuleId module, const ExprView& expr)
{
    return this->projection_aggregate_expression_analyzer().analyze_module_member_expr(expr_id, module, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_index_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->projection_aggregate_expression_analyzer().analyze_index_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_slice_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->projection_aggregate_expression_analyzer().analyze_slice_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_array_literal_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->projection_aggregate_expression_analyzer().analyze_array_literal_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_tuple_literal_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->projection_aggregate_expression_analyzer().analyze_tuple_literal_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_struct_literal_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->projection_aggregate_expression_analyzer().analyze_struct_literal_expr(expr_id, expr, expected_type);
}

} // namespace aurex::sema
