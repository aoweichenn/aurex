#include "lower_ast_internal.hpp"

#include "aurex/ir/enum_layout.hpp"

#include <utility>

namespace aurex::ir::detail {

GlobalConstantId Lowerer::enum_case_constant(const std::string_view name) const noexcept {
    const std::string symbol = enum_case_symbol(name);
    const auto found = constant_symbols_.find(symbol);
    return found == constant_symbols_.end() ? invalid_global_constant_id : found->second;
}

std::string Lowerer::enum_case_symbol(const std::string_view name) const noexcept {
    if (const auto found = enum_cases_by_name_.find(name); found != enum_cases_by_name_.end()) {
        return found->second->c_name;
    }
    if (const auto found = enum_cases_by_c_name_.find(name); found != enum_cases_by_c_name_.end()) {
        return found->second->c_name;
    }
    return std::string(name);
}

const sema::EnumCaseInfo* Lowerer::enum_case_info(const std::string_view name) const noexcept {
    if (const auto found = enum_cases_by_name_.find(name); found != enum_cases_by_name_.end()) {
        return found->second;
    }
    if (const auto found = enum_cases_by_c_name_.find(name); found != enum_cases_by_c_name_.end()) {
        return found->second;
    }
    return nullptr;
}

const sema::EnumCaseInfo* Lowerer::enum_case_by_type_and_case(
    const sema::TypeHandle enum_type,
    const std::string_view case_name
) const noexcept {
    const auto found = enum_cases_by_type_and_case_.find(EnumCaseTypeKey {enum_type.value, case_name});
    if (found != enum_cases_by_type_and_case_.end()) {
        return found->second;
    }
    return nullptr;
}

const syntax::PatternNode* Lowerer::pattern_node(const syntax::PatternId id) const noexcept {
    if (!syntax::is_valid(id) || id.value >= ast_.patterns.size()) {
        return nullptr;
    }
    return &ast_.patterns[id.value];
}

std::string Lowerer::pattern_case_symbol(const syntax::PatternId id) const {
    if (current_generic_function_instance_ != nullptr && syntax::is_valid(id)) {
        if (const auto found = current_generic_function_instance_->pattern_c_names.find(id.value);
            found != current_generic_function_instance_->pattern_c_names.end()) {
            return found->second;
        }
    }
    if (syntax::is_valid(id) &&
        id.value < checked_.pattern_c_names.size() &&
        !checked_.pattern_c_names[id.value].empty()) {
        return checked_.pattern_c_names[id.value];
    }
    const syntax::PatternNode* pattern = pattern_node(id);
    return pattern == nullptr ? "<invalid>" : std::string(pattern->case_name);
}

bool Lowerer::is_fallback_match_pattern(const syntax::PatternId id) const noexcept {
    const syntax::PatternNode* pattern = pattern_node(id);
    return pattern != nullptr && pattern->kind == syntax::PatternKind::wildcard;
}

ValueId Lowerer::append_match_pattern_condition(
    const syntax::PatternId id,
    const ValueId matched_tag,
    const sema::TypeHandle matched_type,
    const bool payload_enum
) {
    const syntax::PatternNode* pattern = pattern_node(id);
    if (pattern == nullptr) {
        return invalid_value_id;
    }
    if (pattern->kind == syntax::PatternKind::or_pattern) {
        ValueId condition = invalid_value_id;
        for (syntax::PatternId alternative : pattern->alternatives) {
            const ValueId alternative_condition = append_match_pattern_condition(alternative, matched_tag, matched_type, payload_enum);
            if (!is_valid(condition)) {
                condition = alternative_condition;
                continue;
            }
            Value or_value;
            or_value.kind = ValueKind::binary;
            or_value.type = module_.types.builtin(sema::BuiltinType::bool_);
            or_value.binary_op = BinaryOp::logical_or;
            or_value.lhs = condition;
            or_value.rhs = alternative_condition;
            condition = append_value(or_value);
        }
        return condition;
    }
    if (pattern->kind == syntax::PatternKind::literal) {
        Value literal;
        literal.kind = (pattern->case_name == "true" || pattern->case_name == "false")
            ? ValueKind::bool_literal
            : ValueKind::integer_literal;
        literal.type = matched_type;
        literal.text = std::string(pattern->case_name);
        const ValueId literal_id = append_value(literal);

        Value cmp;
        cmp.kind = ValueKind::binary;
        cmp.type = module_.types.builtin(sema::BuiltinType::bool_);
        cmp.binary_op = BinaryOp::equal;
        cmp.lhs = matched_tag;
        cmp.rhs = literal_id;
        return append_value(cmp);
    }

    const std::string case_symbol = pattern_case_symbol(id);
    const ValueId case_id = payload_enum
        ? append_enum_tag_literal(case_symbol, enum_tag_type(module_.types, matched_type))
        : append_enum_case_ref(case_symbol, matched_type);

    Value cmp;
    cmp.kind = ValueKind::binary;
    cmp.type = module_.types.builtin(sema::BuiltinType::bool_);
    cmp.binary_op = BinaryOp::equal;
    cmp.lhs = matched_tag;
    cmp.rhs = case_id;
    return append_value(cmp);
}

ValueId Lowerer::lower_match_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    if (current_function_ == nullptr || !is_valid(current_block_) || expr.match_arms.empty()) {
        return invalid_value_id;
    }

    const sema::TypeHandle matched_type = expr_type(expr.match_value);
    const bool payload_enum = is_payload_enum(module_.types, matched_type);
    const ValueId matched = lower_expr(expr.match_value);
    ValueId matched_slot = invalid_value_id;
    ValueId matched_tag = matched;
    if (payload_enum) {
        matched_slot = append_temp_alloca("match.value", matched_type);
        append_store(matched_slot, matched);
        matched_tag = append_load(
            enum_field_addr(matched_slot, std::string(enum_tag_field_name)),
            enum_tag_type(module_.types, matched_type),
            "match.tag"
        );
    }

    const BlockId join_block = add_block(*current_function_, "match.join" + std::to_string(current_function_->blocks.size()));
    std::vector<PhiInput> incoming;
    incoming.reserve(expr.match_arms.size());

    for (base::usize i = 0; i < expr.match_arms.size(); ++i) {
        const syntax::MatchArm& arm = expr.match_arms[i];
        const syntax::PatternNode* pattern = pattern_node(arm.pattern);
        const bool fallback = is_fallback_match_pattern(arm.pattern);
        const BlockId arm_block = add_block(*current_function_, "match.arm" + std::to_string(current_function_->blocks.size()));
        BlockId next_test_block = invalid_block_id;
        const bool guarded = syntax::is_valid(arm.guard);
        const bool implicit_fallback = !guarded && (fallback || i + 1 == expr.match_arms.size());
        if (implicit_fallback) {
            append_branch_if_open(arm_block);
        } else {
            next_test_block = add_block(*current_function_, "match.next" + std::to_string(current_function_->blocks.size()));
            if (fallback) {
                append_branch_if_open(arm_block);
            } else {
                const ValueId condition = append_match_pattern_condition(arm.pattern, matched_tag, matched_type, payload_enum);
                Terminator cond;
                cond.kind = TerminatorKind::cond_branch;
                cond.condition = condition;
                cond.then_target = arm_block;
                cond.else_target = next_test_block;
                set_terminator(current_block_, cond);
            }
        }

        current_block_ = arm_block;
        const auto previous_locals = locals_;
        bind_match_payload(pattern, arm.pattern, payload_enum, matched_slot);
        BlockId arm_body_block = arm_block;
        if (guarded) {
            arm_body_block = add_block(*current_function_, "match.guard.pass" + std::to_string(current_function_->blocks.size()));
            const ValueId guard_condition = lower_expr(arm.guard);
            Terminator guard;
            guard.kind = TerminatorKind::cond_branch;
            guard.condition = guard_condition;
            guard.then_target = arm_body_block;
            guard.else_target = next_test_block;
            set_terminator(current_block_, guard);
            current_block_ = arm_body_block;
        }
        const ValueId arm_value = lower_expr(arm.value, expr_type(expr_id));
        locals_ = previous_locals;
        incoming.push_back(PhiInput {current_block_, arm_value});
        append_branch_if_open(join_block);
        if (is_valid(next_test_block)) {
            current_block_ = next_test_block;
        }
    }

    current_block_ = join_block;
    Value result;
    result.kind = ValueKind::phi;
    result.type = expr_type(expr_id);
    result.incoming = std::move(incoming);
    return append_value(result);
}

ValueId Lowerer::lower_try_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    if (current_function_ == nullptr || !is_valid(current_block_)) {
        return invalid_value_id;
    }

    const sema::TypeHandle source_type = expr_type(expr.unary_operand);
    const sema::TypeHandle result_type = expr_type(expr_id);
    const sema::TypeHandle return_type = current_function_->return_type;

    const sema::EnumCaseInfo* success_case = enum_case_by_type_and_case(source_type, "ok");
    const sema::EnumCaseInfo* failure_case = enum_case_by_type_and_case(source_type, "err");
    const sema::EnumCaseInfo* return_failure_case = enum_case_by_type_and_case(return_type, "err");
    if (success_case == nullptr || failure_case == nullptr || return_failure_case == nullptr) {
        success_case = enum_case_by_type_and_case(source_type, "some");
        failure_case = enum_case_by_type_and_case(source_type, "none");
        return_failure_case = enum_case_by_type_and_case(return_type, "none");
    }
    if (success_case == nullptr || failure_case == nullptr || return_failure_case == nullptr) {
        return invalid_value_id;
    }

    const ValueId source_value = lower_expr(expr.unary_operand);
    const ValueId source_slot = append_temp_alloca("try.value", source_type);
    append_store(source_slot, source_value);

    const sema::TypeHandle tag_type = enum_tag_type(module_.types, source_type);
    const ValueId tag = append_load(enum_field_addr(source_slot, std::string(enum_tag_field_name)), tag_type, "try.tag");
    const ValueId success_tag = append_enum_tag_literal(success_case->c_name, tag_type);

    Value condition;
    condition.kind = ValueKind::binary;
    condition.type = module_.types.builtin(sema::BuiltinType::bool_);
    condition.binary_op = BinaryOp::equal;
    condition.lhs = tag;
    condition.rhs = success_tag;
    const ValueId condition_id = append_value(condition);

    const BlockId success_block = add_block(*current_function_, "try.ok" + std::to_string(current_function_->blocks.size()));
    const BlockId failure_block = add_block(*current_function_, "try.err" + std::to_string(current_function_->blocks.size()));
    const BlockId continue_block = add_block(*current_function_, "try.join" + std::to_string(current_function_->blocks.size()));

    Terminator branch;
    branch.kind = TerminatorKind::cond_branch;
    branch.condition = condition_id;
    branch.then_target = success_block;
    branch.else_target = failure_block;
    set_terminator(current_block_, branch);

    current_block_ = failure_block;
    ValueId return_payload = invalid_value_id;
    if (sema::is_valid(failure_case->payload_type)) {
        return_payload = append_enum_payload_load(source_slot, failure_case->payload_type, "try.err");
    }
    ValueId return_value = append_enum_constructor(*return_failure_case, return_payload);
    return_value = coerce_value(return_value, return_type);
    Terminator ret;
    ret.kind = TerminatorKind::return_;
    ret.value = return_value;
    emit_deferred_scopes(0);
    set_terminator(current_block_, ret);

    current_block_ = success_block;
    const ValueId success_value = append_enum_payload_load(source_slot, result_type, "try.ok");
    append_branch_if_open(continue_block);

    current_block_ = continue_block;
    return success_value;
}

ValueId Lowerer::append_enum_case_ref(const std::string_view case_name, const sema::TypeHandle enum_type) {
    Value case_value;
    case_value.kind = ValueKind::constant_ref;
    case_value.type = enum_type;
    case_value.name = enum_case_symbol(case_name);
    case_value.constant = enum_case_constant(case_name);
    return append_value(case_value);
}

ValueId Lowerer::append_enum_tag_literal(const std::string_view case_name, const sema::TypeHandle tag_type) {
    Value value;
    value.kind = ValueKind::integer_literal;
    value.type = tag_type;
    if (const sema::EnumCaseInfo* info = enum_case_info(case_name); info != nullptr) {
        value.text = info->value_text;
    }
    return append_value(value);
}

ValueId Lowerer::lower_enum_constructor(const sema::EnumCaseInfo& enum_case, const syntax::ExprId payload_expr) {
    const ValueId payload_value = syntax::is_valid(payload_expr)
        ? lower_expr(payload_expr, enum_case.payload_type)
        : invalid_value_id;
    return append_enum_constructor(enum_case, payload_value);
}

ValueId Lowerer::append_enum_constructor(const sema::EnumCaseInfo& enum_case, const ValueId payload_value) {
    Value tag;
    tag.kind = ValueKind::integer_literal;
    tag.type = enum_tag_type(module_.types, enum_case.type);
    tag.text = enum_case.value_text;
    const ValueId tag_id = append_value(tag);

    Value payload;
    if (is_valid(payload_value)) {
        payload.kind = ValueKind::undef;
        payload.type = enum_payload_storage_type(module_.types, enum_case.type);
        const ValueId storage_undef = append_value(payload);
        const ValueId storage_slot = append_temp_alloca("enum.payload.storage", payload.type);
        append_store(storage_slot, storage_undef);

        Value cast;
        cast.kind = ValueKind::cast;
        cast.type = module_.types.pointer(sema::PointerMutability::mut, enum_case.payload_type);
        cast.target_type = cast.type;
        cast.lhs = storage_slot;
        cast.cast_kind = CastKind::pointer;
        const ValueId payload_addr = append_value(cast);
        append_store(payload_addr, coerce_value(payload_value, enum_case.payload_type));

        payload.kind = ValueKind::load;
        payload.type = enum_payload_storage_type(module_.types, enum_case.type);
        payload.object = storage_slot;
    } else {
        payload.kind = ValueKind::undef;
        payload.type = enum_payload_storage_type(module_.types, enum_case.type);
    }
    const ValueId payload_id = append_value(payload);

    Value result;
    result.kind = ValueKind::aggregate;
    result.type = enum_case.type;
    result.fields.push_back(FieldValue {std::string(enum_tag_field_name), tag_id});
    result.fields.push_back(FieldValue {std::string(enum_payload_field_name), payload_id});
    return append_value(result);
}

ValueId Lowerer::append_enum_payload_load(
    const ValueId enum_slot,
    const sema::TypeHandle payload_type,
    const std::string& name
) {
    const ValueId storage_addr = enum_field_addr(enum_slot, std::string(enum_payload_field_name));
    Value cast;
    cast.kind = ValueKind::cast;
    cast.type = module_.types.pointer(sema::PointerMutability::mut, payload_type);
    cast.target_type = cast.type;
    cast.lhs = storage_addr;
    cast.cast_kind = CastKind::pointer;
    const ValueId payload_addr = append_value(cast);
    return append_load(payload_addr, payload_type, name);
}

ValueId Lowerer::append_temp_alloca(const std::string& name, const sema::TypeHandle value_type) {
    Value slot;
    slot.kind = ValueKind::alloca;
    slot.name = name;
    slot.type = module_.types.pointer(sema::PointerMutability::mut, value_type);
    return append_value(slot);
}

ValueId Lowerer::append_load(const ValueId address, const sema::TypeHandle value_type, const std::string& name) {
    Value value;
    value.kind = ValueKind::load;
    value.name = name;
    value.type = value_type;
    value.object = address;
    return append_value(value);
}

ValueId Lowerer::enum_field_addr(const ValueId object, const std::string& field_name) {
    const sema::TypeHandle enum_type = local_load_type(object);
    Value value;
    value.kind = ValueKind::field_addr;
    value.name = field_name;
    value.object = object;
    value.type = module_.types.pointer(sema::PointerMutability::mut, aggregate_field_type(enum_type, field_name));
    return append_value(value);
}

void Lowerer::bind_payload_arm(const syntax::PatternNode& pattern, const sema::EnumCaseInfo& info, const ValueId matched_slot) {
    if (!sema::is_valid(info.payload_type)) {
        return;
    }
    const ValueId storage_addr = enum_field_addr(matched_slot, std::string(enum_payload_field_name));
    Value cast;
    cast.kind = ValueKind::cast;
    cast.type = module_.types.pointer(sema::PointerMutability::mut, info.payload_type);
    cast.target_type = cast.type;
    cast.lhs = storage_addr;
    cast.cast_kind = CastKind::pointer;
    const ValueId payload_addr = append_value(cast);
    const ValueId payload_value = append_load(payload_addr, info.payload_type, std::string(pattern.binding_name));
    const ValueId slot = append_temp_alloca(std::string(pattern.binding_name), info.payload_type);
    locals_[std::string(pattern.binding_name)] = LocalBinding {slot, false};
    append_store(slot, payload_value);
}

void Lowerer::bind_match_payload(
    const syntax::PatternNode* pattern,
    const syntax::PatternId pattern_id,
    const bool payload_enum,
    const ValueId matched_slot
) {
    if (!payload_enum || pattern == nullptr || pattern->binding_name.empty()) {
        return;
    }
    if (const sema::EnumCaseInfo* info = enum_case_info(pattern_case_symbol(pattern_id)); info != nullptr) {
        bind_payload_arm(*pattern, *info, matched_slot);
    }
}

} // namespace aurex::ir::detail
