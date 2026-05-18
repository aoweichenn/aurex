#include <ir/lower_ast_internal.hpp>

#include <aurex/ir/enum_layout.hpp>

#include <algorithm>
#include <utility>

namespace aurex::ir::detail {

namespace {

constexpr char IR_ENUM_SYNTHETIC_PAYLOAD_FIELD_PREFIX[] = "_";
constexpr base::usize IR_TRY_SHAPE_CASE_COUNT = 2;

} // namespace

GlobalConstantId Lowerer::enum_case_constant(const std::string_view name) noexcept {
    const IrTextId symbol = this->enum_case_symbol(name);
    const auto found = this->constant_symbols_.find(symbol);
    return found == this->constant_symbols_.end() ? INVALID_GLOBAL_CONSTANT_ID : found->second;
}

IrTextId Lowerer::enum_case_symbol(const std::string_view name) noexcept {
    if (const auto found = enum_cases_by_name_.find(this->ast_.find_identifier(name));
        found != enum_cases_by_name_.end()) {
        return this->module_.intern(found->second->c_name);
    }
    const IrTextId existing = this->module_.find_text(name);
    if (const auto found = this->enum_cases_by_c_name_.find(existing);
        found != this->enum_cases_by_c_name_.end()) {
        return existing;
    }
    return this->module_.intern(name);
}

const sema::EnumCaseInfo* Lowerer::enum_case_info(const IrTextId symbol) const noexcept {
    if (const auto found = this->enum_cases_by_c_name_.find(symbol);
        found != this->enum_cases_by_c_name_.end()) {
        return found->second;
    }
    return nullptr;
}

const sema::EnumCaseInfo* Lowerer::enum_case_by_type_and_case(
    const sema::TypeHandle enum_type,
    const std::string_view case_name
) const noexcept {
    const sema::IdentId case_name_id = this->ast_.find_identifier(case_name);
    const auto found = this->enum_cases_by_type_and_case_.find(EnumCaseTypeKey {enum_type.value, case_name_id});
    if (found != this->enum_cases_by_type_and_case_.end()) {
        return found->second;
    }
    for (const auto& entry : this->enum_cases_by_type_and_case_) {
        if (entry.first.type == enum_type.value && entry.second->case_name == case_name) {
            return entry.second;
        }
    }
    return nullptr;
}

TryShape Lowerer::classify_try_shape(const sema::TypeHandle enum_type) const noexcept {
    if (!sema::is_valid(enum_type) ||
        enum_type.value >= this->module_.types.size() ||
        this->module_.types.get(enum_type).kind != sema::TypeKind::enum_) {
        return {};
    }
    base::usize case_count = 0;
    for (const auto& entry : this->enum_cases_by_type_and_case_) {
        if (entry.first.type == enum_type.value) {
            ++case_count;
        }
    }
    if (case_count != IR_TRY_SHAPE_CASE_COUNT) {
        return {};
    }

    const sema::EnumCaseInfo* const ok_case = this->enum_case_by_type_and_case(enum_type, "ok");
    const sema::EnumCaseInfo* const err_case = this->enum_case_by_type_and_case(enum_type, "err");
    if (ok_case != nullptr &&
        err_case != nullptr &&
        sema::is_valid(ok_case->payload_type) &&
        sema::is_valid(err_case->payload_type)) {
        return TryShape {TryShapeKind::result, ok_case, err_case};
    }

    const sema::EnumCaseInfo* const some_case = this->enum_case_by_type_and_case(enum_type, "some");
    const sema::EnumCaseInfo* const none_case = this->enum_case_by_type_and_case(enum_type, "none");
    if (some_case != nullptr &&
        none_case != nullptr &&
        sema::is_valid(some_case->payload_type) &&
        !sema::is_valid(none_case->payload_type)) {
        return TryShape {TryShapeKind::option, some_case, none_case};
    }

    return {};
}

const syntax::PatternNode* Lowerer::pattern_node(const syntax::PatternId id) const {
    if (!syntax::is_valid(id) || id.value >= ast_.patterns.size()) {
        return nullptr;
    }
    return this->ast_.patterns.ptr(id.value);
}

IrTextId Lowerer::pattern_case_symbol(const syntax::PatternId id) {
    if (syntax::is_valid(id) &&
        this->active_side_tables_.generic != nullptr &&
        this->active_side_tables_.generic->sparse) {
        const base::usize local = this->active_side_tables_.generic->local_pattern_index(id);
        if (local != sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX &&
            this->active_side_tables_.pattern_c_name_ids != nullptr &&
            local < this->active_side_tables_.pattern_c_name_ids->size()) {
            if (const std::string_view c_name =
                    this->checked_.c_name_text((*this->active_side_tables_.pattern_c_name_ids)[local]);
                !c_name.empty()) {
                return this->module_.intern(c_name);
            }
        }
        const auto found = this->active_side_tables_.generic->sparse_pattern_c_name_ids.find(id.value);
        if (found != this->active_side_tables_.generic->sparse_pattern_c_name_ids.end()) {
            if (const std::string_view c_name = this->checked_.c_name_text(found->second); !c_name.empty()) {
                return this->module_.intern(c_name);
            }
        }
    }
    if (syntax::is_valid(id) &&
        this->active_side_tables_.pattern_c_name_ids != nullptr &&
        id.value < this->active_side_tables_.pattern_c_name_ids->size()) {
        const std::string_view c_name = this->checked_.c_name_text((*this->active_side_tables_.pattern_c_name_ids)[id.value]);
        if (!c_name.empty()) {
            return this->module_.intern(c_name);
        }
    }
    const syntax::PatternNode* pattern = pattern_node(id);
    return pattern == nullptr ? INVALID_IR_TEXT_ID : this->module_.intern(pattern->case_name);
}

bool Lowerer::is_irrefutable_pattern(
    const syntax::PatternId id,
    const sema::TypeHandle matched_type
) const {
    struct PatternFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
        bool expanded = false;
    };

    std::vector<PatternFrame> pending;
    std::vector<bool> results(this->ast_.patterns.size(), false);
    pending.push_back(PatternFrame {id, matched_type, false});
    while (!pending.empty()) {
        const PatternFrame frame = pending.back();
        pending.pop_back();
        const syntax::PatternNode* pattern = this->pattern_node(frame.pattern);
        if (pattern == nullptr) {
            continue;
        }
        if (frame.expanded) {
            bool result = false;
            switch (pattern->kind) {
            case syntax::PatternKind::wildcard:
            case syntax::PatternKind::binding:
                result = true;
                break;
            case syntax::PatternKind::literal:
            case syntax::PatternKind::const_:
            case syntax::PatternKind::enum_case:
                result = false;
                break;
            case syntax::PatternKind::tuple:
                result = sema::is_valid(frame.type) && this->module_.types.is_tuple(frame.type);
                for (const syntax::PatternId element : pattern->elements) {
                    result = result &&
                        syntax::is_valid(element) &&
                        element.value < results.size() &&
                        results[element.value];
                }
                break;
            case syntax::PatternKind::slice: {
                const bool array_type = sema::is_valid(frame.type) && this->module_.types.is_array(frame.type);
                const bool slice_type = sema::is_valid(frame.type) && this->module_.types.is_slice(frame.type);
                result = false;
                if (array_type) {
                    const sema::TypeInfo& array = this->module_.types.get(frame.type);
                    result = pattern->has_slice_rest
                        ? pattern->elements.size() <= array.array_count
                        : pattern->elements.size() == array.array_count;
                } else if (slice_type) {
                    result = pattern->has_slice_rest && pattern->elements.empty();
                }
                for (const syntax::PatternId element : pattern->elements) {
                    result = result &&
                        syntax::is_valid(element) &&
                        element.value < results.size() &&
                        results[element.value];
                }
                break;
            }
            case syntax::PatternKind::struct_:
                result = sema::is_valid(frame.type) && this->module_.types.get(frame.type).kind == sema::TypeKind::struct_;
                for (const syntax::FieldPattern& field : pattern->field_patterns) {
                    result = result &&
                        syntax::is_valid(field.pattern) &&
                        field.pattern.value < results.size() &&
                        results[field.pattern.value];
                }
                break;
            case syntax::PatternKind::or_pattern:
                for (const syntax::PatternId alternative : pattern->alternatives) {
                    if (syntax::is_valid(alternative) &&
                        alternative.value < results.size() &&
                        results[alternative.value]) {
                        result = true;
                        break;
                    }
                }
                break;
            }
            results[frame.pattern.value] = result;
            continue;
        }

        pending.push_back(PatternFrame {frame.pattern, frame.type, true});
        switch (pattern->kind) {
        case syntax::PatternKind::tuple: {
            if (!sema::is_valid(frame.type) || !this->module_.types.is_tuple(frame.type)) {
                break;
            }
            const sema::TypeInfo& tuple = this->module_.types.get(frame.type);
            const base::usize count = std::min(tuple.tuple_elements.size(), pattern->elements.size());
            for (base::usize i = count; i > 0; --i) {
                pending.push_back(PatternFrame {pattern->elements[i - 1], tuple.tuple_elements[i - 1], false});
            }
            break;
        }
        case syntax::PatternKind::slice: {
            if (!sema::is_valid(frame.type) ||
                (!this->module_.types.is_array(frame.type) && !this->module_.types.is_slice(frame.type))) {
                break;
            }
            const sema::TypeInfo& info = this->module_.types.get(frame.type);
            const sema::TypeHandle element_type = info.kind == sema::TypeKind::array
                ? info.array_element
                : info.slice_element;
            for (auto element = pattern->elements.rbegin(); element != pattern->elements.rend(); ++element) {
                pending.push_back(PatternFrame {*element, element_type, false});
            }
            break;
        }
        case syntax::PatternKind::struct_:
            for (auto field = pattern->field_patterns.rbegin(); field != pattern->field_patterns.rend(); ++field) {
                pending.push_back(PatternFrame {
                    field->pattern,
                    this->aggregate_field_type(frame.type, field->name),
                    false,
                });
            }
            break;
        case syntax::PatternKind::or_pattern:
            for (auto alternative = pattern->alternatives.rbegin(); alternative != pattern->alternatives.rend(); ++alternative) {
                pending.push_back(PatternFrame {*alternative, frame.type, false});
            }
            break;
        case syntax::PatternKind::wildcard:
        case syntax::PatternKind::binding:
        case syntax::PatternKind::enum_case:
        case syntax::PatternKind::const_:
        case syntax::PatternKind::literal:
            break;
        }
    }
    return syntax::is_valid(id) && id.value < results.size() && results[id.value];
}

ValueId Lowerer::append_true_value() {
    Value value = this->module_.make_value();
    value.kind = ValueKind::bool_literal;
    value.type = this->module_.types.builtin(sema::BuiltinType::bool_);
    value.text = this->module_.intern("true");
    return this->append_value(value);
}

ValueId Lowerer::append_pattern_source_length(
    const ValueId source_address,
    const sema::TypeHandle source_type
) {
    const sema::TypeHandle usize_type = this->module_.types.builtin(sema::BuiltinType::usize);
    if (!sema::is_valid(source_type)) {
        return this->append_integer_literal("0", usize_type);
    }
    if (this->module_.types.is_array(source_type)) {
        const sema::TypeInfo& array = this->module_.types.get(source_type);
        return this->append_integer_literal(std::to_string(array.array_count), usize_type);
    }
    if (this->module_.types.is_slice(source_type)) {
        const ValueId slice_value =
            this->append_load(source_address, source_type, this->module_.intern("pattern.slice"));
        return this->append_slice_len(slice_value);
    }
    return this->append_integer_literal("0", usize_type);
}

ValueId Lowerer::append_pattern_element_address(
    const ValueId source_address,
    const sema::TypeHandle source_type,
    const ValueId index,
    const sema::TypeHandle element_type
) {
    sema::PointerMutability mutability = sema::PointerMutability::mut;
    ValueId base_address = source_address;
    if (sema::is_valid(source_type) && this->module_.types.is_slice(source_type)) {
        const sema::TypeInfo& slice = this->module_.types.get(source_type);
        mutability = slice.slice_mutability;
        const ValueId slice_value =
            this->append_load(source_address, source_type, this->module_.intern("pattern.slice"));
        base_address = this->append_slice_data(slice_value, slice.slice_mutability, slice.slice_element);
    }
    Value value = this->module_.make_value();
    value.kind = ValueKind::index_addr;
    value.type = this->module_.types.pointer(mutability, element_type);
    value.object = base_address;
    value.index = index;
    return this->append_value(value);
}

ValueId Lowerer::append_pattern_condition(
    const syntax::PatternId id,
    const ValueId source_address,
    const sema::TypeHandle source_type
) {
    const syntax::PatternNode* pattern = this->pattern_node(id);
    if (pattern == nullptr) {
        return this->append_true_value();
    }
    if (pattern->kind == syntax::PatternKind::wildcard || pattern->kind == syntax::PatternKind::binding) {
        return this->append_true_value();
    }
    if (pattern->kind == syntax::PatternKind::or_pattern) {
        ValueId condition = INVALID_VALUE_ID;
        for (const syntax::PatternId alternative : pattern->alternatives) {
            const ValueId alternative_condition = this->append_pattern_condition(alternative, source_address, source_type);
            if (!is_valid(condition)) {
                condition = alternative_condition;
                continue;
            }
            condition = this->append_binary_value(
                BinaryOp::logical_or,
                this->module_.types.builtin(sema::BuiltinType::bool_),
                condition,
                alternative_condition
            );
        }
        return is_valid(condition) ? condition : this->append_true_value();
    }
    if (pattern->kind == syntax::PatternKind::const_) {
        const IrTextId const_symbol = this->pattern_case_symbol(id);
        const auto constant = this->constant_symbols_.find(const_symbol);
        if (constant == this->constant_symbols_.end()) {
            return this->append_true_value();
        }
        const ValueId source = this->append_load(source_address, source_type);
        Value const_ref = this->module_.make_value();
        const_ref.kind = ValueKind::constant_ref;
        const_ref.name = const_symbol;
        const_ref.constant = constant->second;
        const_ref.type = this->module_.constants[constant->second.value].type;
        return this->append_binary_value(
            BinaryOp::equal,
            this->module_.types.builtin(sema::BuiltinType::bool_),
            source,
            this->append_value(const_ref)
        );
    }
    if (pattern->kind == syntax::PatternKind::literal) {
        const ValueId source = this->append_load(source_address, source_type);
        Value literal = this->module_.make_value();
        literal.kind = (pattern->case_name == "true" || pattern->case_name == "false")
            ? ValueKind::bool_literal
            : ValueKind::integer_literal;
        literal.type = source_type;
        literal.text = this->module_.intern(pattern->case_name);
        return this->append_binary_value(
            BinaryOp::equal,
            this->module_.types.builtin(sema::BuiltinType::bool_),
            source,
            this->append_value(literal)
        );
    }
    if (pattern->kind == syntax::PatternKind::tuple) {
        if (!sema::is_valid(source_type) || !this->module_.types.is_tuple(source_type)) {
            return this->append_true_value();
        }
        const sema::TypeInfo& tuple = this->module_.types.get(source_type);
        ValueId condition = INVALID_VALUE_ID;
        const base::usize count = std::min(tuple.tuple_elements.size(), pattern->elements.size());
        for (base::usize i = 0; i < count; ++i) {
            Value field = this->module_.make_value();
            field.kind = ValueKind::field_addr;
            field.name = this->module_.intern(std::to_string(i));
            field.object = source_address;
            field.type = this->module_.types.pointer(sema::PointerMutability::mut, tuple.tuple_elements[i]);
            const ValueId element_condition =
                this->append_pattern_condition(pattern->elements[i], this->append_value(field), tuple.tuple_elements[i]);
            condition = is_valid(condition)
                ? this->append_binary_value(
                    BinaryOp::logical_and,
                    this->module_.types.builtin(sema::BuiltinType::bool_),
                    condition,
                    element_condition
                )
                : element_condition;
        }
        return is_valid(condition) ? condition : this->append_true_value();
    }
    if (pattern->kind == syntax::PatternKind::slice) {
        if (!sema::is_valid(source_type) ||
            (!this->module_.types.is_array(source_type) && !this->module_.types.is_slice(source_type))) {
            return this->append_true_value();
        }
        const sema::TypeInfo& source = this->module_.types.get(source_type);
        const sema::TypeHandle element_type = source.kind == sema::TypeKind::array
            ? source.array_element
            : source.slice_element;
        const sema::TypeHandle usize_type = this->module_.types.builtin(sema::BuiltinType::usize);
        const sema::TypeHandle bool_type = this->module_.types.builtin(sema::BuiltinType::bool_);
        const ValueId length = this->append_pattern_source_length(source_address, source_type);
        const ValueId fixed_count = this->append_integer_literal(std::to_string(pattern->elements.size()), usize_type);
        ValueId condition = this->append_binary_value(
            pattern->has_slice_rest ? BinaryOp::greater_equal : BinaryOp::equal,
            bool_type,
            length,
            fixed_count
        );
        const base::usize rest_index = pattern->has_slice_rest
            ? std::min(pattern->slice_rest_index, pattern->elements.size())
            : pattern->elements.size();
        const base::usize suffix_count = pattern->has_slice_rest
            ? pattern->elements.size() - rest_index
            : 0;
        for (base::usize i = 0; i < pattern->elements.size(); ++i) {
            ValueId index = INVALID_VALUE_ID;
            if (!pattern->has_slice_rest || i < rest_index) {
                index = this->append_integer_literal(std::to_string(i), usize_type);
            } else {
                const base::usize suffix_index = i - rest_index;
                const ValueId suffix_offset =
                    this->append_integer_literal(std::to_string(suffix_count - suffix_index), usize_type);
                index = this->append_binary_value(BinaryOp::sub, usize_type, length, suffix_offset);
            }
            const ValueId element_address =
                this->append_pattern_element_address(source_address, source_type, index, element_type);
            const ValueId element_condition =
                this->append_pattern_condition(pattern->elements[i], element_address, element_type);
            condition = this->append_binary_value(BinaryOp::logical_and, bool_type, condition, element_condition);
        }
        return condition;
    }
    if (pattern->kind == syntax::PatternKind::struct_) {
        ValueId condition = INVALID_VALUE_ID;
        for (const syntax::FieldPattern& field_pattern : pattern->field_patterns) {
            const sema::TypeHandle field_type = this->aggregate_field_type(source_type, field_pattern.name);
            Value field = this->module_.make_value();
            field.kind = ValueKind::field_addr;
            field.name = this->module_.intern(field_pattern.name);
            field.object = source_address;
            field.type = this->module_.types.pointer(sema::PointerMutability::mut, field_type);
            const ValueId field_condition =
                this->append_pattern_condition(field_pattern.pattern, this->append_value(field), field_type);
            condition = is_valid(condition)
                ? this->append_binary_value(
                    BinaryOp::logical_and,
                    this->module_.types.builtin(sema::BuiltinType::bool_),
                    condition,
                    field_condition
                )
                : field_condition;
        }
        return is_valid(condition) ? condition : this->append_true_value();
    }

    const bool payload_enum = is_payload_enum(this->module_.types, source_type);
    const IrTextId case_symbol = this->pattern_case_symbol(id);
    const ValueId source = payload_enum
        ? this->append_load(
            this->enum_field_addr(source_address, this->module_.intern(IR_ENUM_TAG_FIELD_NAME)),
            enum_tag_type(this->module_.types, source_type),
            this->module_.intern("pattern.tag")
        )
        : this->append_load(source_address, source_type);
    const ValueId case_id = payload_enum
        ? this->append_enum_tag_literal(this->module_.text(case_symbol), enum_tag_type(this->module_.types, source_type))
        : this->append_enum_case_ref(this->module_.text(case_symbol), source_type);
    ValueId condition = this->append_binary_value(
        BinaryOp::equal,
        this->module_.types.builtin(sema::BuiltinType::bool_),
        source,
        case_id
    );
    if (!payload_enum || pattern->payload_patterns.empty()) {
        return condition;
    }
    const sema::EnumCaseInfo* info = this->enum_case_info(case_symbol);
    if (info == nullptr || !sema::is_valid(info->payload_type)) {
        return condition;
    }
    const BlockId tag_block = this->current_block_;
    const BlockId payload_block =
        add_block(this->module_, *this->current_function_, "pattern.payload" + std::to_string(this->current_function_->blocks.size()));
    const BlockId exit_block =
        add_block(this->module_, *this->current_function_, "pattern.join" + std::to_string(this->current_function_->blocks.size()));
    Terminator payload_guard;
    payload_guard.kind = TerminatorKind::cond_branch;
    payload_guard.condition = condition;
    payload_guard.then_target = payload_block;
    payload_guard.else_target = exit_block;
    this->set_terminator(this->current_block_, payload_guard);

    this->current_block_ = payload_block;
    const ValueId storage_addr = this->enum_field_addr(source_address, this->module_.intern(IR_ENUM_PAYLOAD_FIELD_NAME));
    Value cast = this->module_.make_value();
    cast.kind = ValueKind::cast;
    cast.type = this->module_.types.pointer(sema::PointerMutability::mut, info->payload_type);
    cast.target_type = cast.type;
    cast.lhs = storage_addr;
    cast.cast_kind = CastKind::pointer;
    const ValueId payload_addr = this->append_value(cast);
    ValueId payload_condition = INVALID_VALUE_ID;
    if (info->payload_types.size() == 1 && pattern->payload_patterns.size() == 1) {
        payload_condition = this->append_pattern_condition(
            pattern->payload_patterns.front(),
            payload_addr,
            info->payload_type
        );
    } else {
        const base::usize count = std::min(info->payload_types.size(), pattern->payload_patterns.size());
        for (base::usize i = 0; i < count; ++i) {
            Value field = this->module_.make_value();
            field.kind = ValueKind::field_addr;
            field.type = this->module_.types.pointer(sema::PointerMutability::mut, info->payload_types[i]);
            field.name = this->module_.intern(std::string(IR_ENUM_SYNTHETIC_PAYLOAD_FIELD_PREFIX) + std::to_string(i));
            field.object = payload_addr;
            const ValueId field_condition =
                this->append_pattern_condition(pattern->payload_patterns[i], this->append_value(field), info->payload_types[i]);
            payload_condition = is_valid(payload_condition)
                ? this->append_binary_value(
                    BinaryOp::logical_and,
                    this->module_.types.builtin(sema::BuiltinType::bool_),
                    payload_condition,
                    field_condition
                )
                : field_condition;
        }
    }
    if (!is_valid(payload_condition)) {
        payload_condition = this->append_true_value();
    }
    const BlockId payload_tail_block = this->current_block_;
    this->append_branch_if_open(exit_block);

    this->current_block_ = exit_block;
    Value result = this->module_.make_value();
    result.kind = ValueKind::phi;
    result.type = this->module_.types.builtin(sema::BuiltinType::bool_);
    result.incoming.push_back(PhiInput {tag_block, condition});
    result.incoming.push_back(PhiInput {payload_tail_block, payload_condition});
    return this->append_value(result);
}

ValueId Lowerer::lower_match_expr(const syntax::ExprId expr_id, const ExprView& expr) {
    if (current_function_ == nullptr || !is_valid(current_block_) || expr.match_arms.empty()) {
        return INVALID_VALUE_ID;
    }

    const sema::TypeHandle matched_type = expr_type(expr.match_value);
    const ValueId matched = lower_expr(expr.match_value);
    const ValueId matched_slot = append_temp_alloca("match.value", matched_type);
    append_store(matched_slot, matched);

    const BlockId join_block = add_block(this->module_, *current_function_, "match.join" + std::to_string(current_function_->blocks.size()));
    std::vector<PhiInput> incoming;
    incoming.reserve(expr.match_arms.size());

    for (base::usize i = 0; i < expr.match_arms.size(); ++i) {
        const syntax::MatchArm& arm = expr.match_arms[i];
        const bool fallback = is_irrefutable_pattern(arm.pattern, matched_type);
        const BlockId arm_block = add_block(this->module_, *current_function_, "match.arm" + std::to_string(current_function_->blocks.size()));
        BlockId next_test_block = INVALID_BLOCK_ID;
        const bool guarded = syntax::is_valid(arm.guard);
        const bool implicit_fallback = !guarded && (fallback || i + 1 == expr.match_arms.size());
        if (implicit_fallback) {
            append_branch_if_open(arm_block);
        } else {
            next_test_block = add_block(this->module_, *current_function_, "match.next" + std::to_string(current_function_->blocks.size()));
            if (fallback) {
                append_branch_if_open(arm_block);
            } else {
                const ValueId condition = append_pattern_condition(arm.pattern, matched_slot, matched_type);
                Terminator cond;
                cond.kind = TerminatorKind::cond_branch;
                cond.condition = condition;
                cond.then_target = arm_block;
                cond.else_target = next_test_block;
                set_terminator(current_block_, cond);
            }
        }

        current_block_ = arm_block;
        this->push_local_scope();
        bind_pattern_locals(arm.pattern, matched_slot, matched_type);
        BlockId arm_body_block = arm_block;
        if (guarded) {
            arm_body_block = add_block(this->module_, *current_function_, "match.guard.pass" + std::to_string(current_function_->blocks.size()));
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
        this->pop_local_scope();
        incoming.push_back(PhiInput {current_block_, arm_value});
        append_branch_if_open(join_block);
        if (is_valid(next_test_block)) {
            current_block_ = next_test_block;
        }
    }

    current_block_ = join_block;
    Value result = this->module_.make_value();
    result.kind = ValueKind::phi;
    result.type = expr_type(expr_id);
    result.incoming.reserve(incoming.size());
    result.incoming.insert(result.incoming.end(), incoming.begin(), incoming.end());
    return append_value(result);
}

ValueId Lowerer::lower_try_expr(const syntax::ExprId expr_id, const ExprView& expr) {
    if (current_function_ == nullptr || !is_valid(current_block_)) {
        return INVALID_VALUE_ID;
    }

    const sema::TypeHandle source_type = expr_type(expr.try_operand);
    const sema::TypeHandle result_type = expr_type(expr_id);
    const sema::TypeHandle return_type = current_function_->return_type;

    const TryShape source_shape = this->classify_try_shape(source_type);
    const TryShape return_shape = this->classify_try_shape(return_type);
    if (source_shape.kind == TryShapeKind::none ||
        source_shape.kind != return_shape.kind ||
        source_shape.success_case == nullptr ||
        source_shape.failure_case == nullptr ||
        return_shape.failure_case == nullptr) {
        return INVALID_VALUE_ID;
    }
    const sema::EnumCaseInfo* const success_case = source_shape.success_case;
    const sema::EnumCaseInfo* const failure_case = source_shape.failure_case;
    const sema::EnumCaseInfo* const return_failure_case = return_shape.failure_case;

    const ValueId source_value = lower_expr(expr.try_operand);
    const ValueId source_slot = append_temp_alloca("try.value", source_type);
    append_store(source_slot, source_value);

    const sema::TypeHandle tag_type = enum_tag_type(module_.types, source_type);
    const ValueId tag = append_load(
        enum_field_addr(source_slot, this->module_.intern(IR_ENUM_TAG_FIELD_NAME)),
        tag_type,
        this->module_.intern("try.tag")
    );
    const ValueId success_tag = append_enum_tag_literal(success_case->c_name, tag_type);

    Value condition = this->module_.make_value();
    condition.kind = ValueKind::binary;
    condition.type = module_.types.builtin(sema::BuiltinType::bool_);
    condition.binary_op = BinaryOp::equal;
    condition.lhs = tag;
    condition.rhs = success_tag;
    const ValueId condition_id = append_value(condition);

    const BlockId success_block = add_block(this->module_, *current_function_, "try.ok" + std::to_string(current_function_->blocks.size()));
    const BlockId failure_block = add_block(this->module_, *current_function_, "try.err" + std::to_string(current_function_->blocks.size()));
    const BlockId continue_block = add_block(this->module_, *current_function_, "try.join" + std::to_string(current_function_->blocks.size()));

    Terminator branch;
    branch.kind = TerminatorKind::cond_branch;
    branch.condition = condition_id;
    branch.then_target = success_block;
    branch.else_target = failure_block;
    set_terminator(current_block_, branch);

    current_block_ = failure_block;
    ValueId return_payload = INVALID_VALUE_ID;
    if (sema::is_valid(failure_case->payload_type)) {
        return_payload =
            append_enum_payload_load(source_slot, failure_case->payload_type, this->module_.intern("try.err"));
    }
    ValueId return_value = append_enum_constructor(*return_failure_case, return_payload);
    return_value = coerce_value(return_value, return_type);
    Terminator ret;
    ret.kind = TerminatorKind::return_;
    ret.value = return_value;
    emit_deferred_scopes(0);
    set_terminator(current_block_, ret);

    current_block_ = success_block;
    const ValueId success_value =
        append_enum_payload_load(source_slot, result_type, this->module_.intern("try.ok"));
    append_branch_if_open(continue_block);

    current_block_ = continue_block;
    return success_value;
}

ValueId Lowerer::append_enum_case_ref(const std::string_view case_name, const sema::TypeHandle enum_type) {
    Value case_value = this->module_.make_value();
    case_value.kind = ValueKind::constant_ref;
    case_value.type = enum_type;
    case_value.name = this->enum_case_symbol(case_name);
    case_value.constant = this->enum_case_constant(case_name);
    return append_value(case_value);
}

ValueId Lowerer::append_enum_tag_literal(const std::string_view case_name, const sema::TypeHandle tag_type) {
    Value value = this->module_.make_value();
    value.kind = ValueKind::integer_literal;
    value.type = tag_type;
    if (const sema::EnumCaseInfo* info = this->enum_case_info(this->enum_case_symbol(case_name)); info != nullptr) {
        value.text = this->module_.intern(info->value_text);
    }
    return append_value(value);
}

ValueId Lowerer::lower_enum_constructor(const sema::EnumCaseInfo& enum_case, const syntax::ExprId payload_expr) {
    const sema::TypeHandle expected_payload_type = enum_case.payload_types.empty()
        ? enum_case.payload_type
        : enum_case.payload_types.front();
    const ValueId payload_value = syntax::is_valid(payload_expr)
        ? lower_expr(payload_expr, expected_payload_type)
        : INVALID_VALUE_ID;
    return append_enum_constructor(enum_case, payload_value);
}

ValueId Lowerer::lower_enum_constructor_call(
    const sema::EnumCaseInfo& enum_case,
    const ExprView& expr
) {
    if (enum_case.payload_types.empty()) {
        return this->append_enum_constructor(enum_case, INVALID_VALUE_ID);
    }
    if (enum_case.payload_types.size() == 1) {
        return this->lower_enum_constructor(
            enum_case,
            expr.args.empty() ? syntax::INVALID_EXPR_ID : expr.args.front()
        );
    }

    Value aggregate = this->module_.make_value();
    aggregate.kind = ValueKind::aggregate;
    aggregate.type = enum_case.payload_type;
    const base::usize field_count = std::min(expr.args.size(), enum_case.payload_types.size());
    aggregate.fields.reserve(field_count);
    for (base::usize i = 0; i < field_count; ++i) {
        const ValueId value = this->coerce_value(
            this->lower_expr(expr.args[i], enum_case.payload_types[i]),
            enum_case.payload_types[i]
        );
        aggregate.fields.push_back(FieldValue {
            this->module_.intern(std::string(IR_ENUM_SYNTHETIC_PAYLOAD_FIELD_PREFIX) + std::to_string(i)),
            value,
        });
    }
    return this->append_enum_constructor(enum_case, this->append_value(aggregate));
}

ValueId Lowerer::append_enum_constructor(const sema::EnumCaseInfo& enum_case, const ValueId payload_value) {
    Value tag = this->module_.make_value();
    tag.kind = ValueKind::integer_literal;
    tag.type = enum_tag_type(module_.types, enum_case.type);
    tag.text = this->module_.intern(enum_case.value_text);
    const ValueId tag_id = append_value(tag);

    Value payload = this->module_.make_value();
    if (is_valid(payload_value)) {
        payload.kind = ValueKind::undef;
        payload.type = enum_payload_storage_type(module_.types, enum_case.type);
        const ValueId storage_undef = append_value(payload);
        const ValueId storage_slot = append_temp_alloca("enum.payload.storage", payload.type);
        append_store(storage_slot, storage_undef);

        Value cast = this->module_.make_value();
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

    Value result = this->module_.make_value();
    result.kind = ValueKind::aggregate;
    result.type = enum_case.type;
    result.fields.push_back(FieldValue {this->module_.intern(IR_ENUM_TAG_FIELD_NAME), tag_id});
    result.fields.push_back(FieldValue {this->module_.intern(IR_ENUM_PAYLOAD_FIELD_NAME), payload_id});
    return append_value(result);
}

ValueId Lowerer::append_enum_payload_load(
    const ValueId enum_slot,
    const sema::TypeHandle payload_type,
    const IrTextId name
) {
    const ValueId storage_addr = enum_field_addr(enum_slot, this->module_.intern(IR_ENUM_PAYLOAD_FIELD_NAME));
    Value cast = this->module_.make_value();
    cast.kind = ValueKind::cast;
    cast.type = module_.types.pointer(sema::PointerMutability::mut, payload_type);
    cast.target_type = cast.type;
    cast.lhs = storage_addr;
    cast.cast_kind = CastKind::pointer;
    const ValueId payload_addr = append_value(cast);
    return append_load(payload_addr, payload_type, name);
}

ValueId Lowerer::append_temp_alloca(const std::string_view name, const sema::TypeHandle value_type) {
    Value slot = this->module_.make_value();
    slot.kind = ValueKind::alloca;
    slot.name = this->module_.intern(name);
    slot.type = module_.types.pointer(sema::PointerMutability::mut, value_type);
    return append_value(slot);
}

ValueId Lowerer::append_integer_literal(const std::string_view text, const sema::TypeHandle value_type) {
    Value value = this->module_.make_value();
    value.kind = ValueKind::integer_literal;
    value.type = value_type;
    value.text = this->module_.intern(text);
    return append_value(value);
}

ValueId Lowerer::append_binary_value(
    const BinaryOp op,
    const sema::TypeHandle type,
    const ValueId lhs,
    const ValueId rhs
) {
    Value value = this->module_.make_value();
    value.kind = ValueKind::binary;
    value.type = type;
    value.binary_op = op;
    value.lhs = lhs;
    value.rhs = rhs;
    return append_value(value);
}

ValueId Lowerer::append_load(const ValueId address, const sema::TypeHandle value_type, const IrTextId name) {
    Value value = this->module_.make_value();
    value.kind = ValueKind::load;
    value.name = name;
    value.type = value_type;
    value.object = address;
    return append_value(value);
}

ValueId Lowerer::enum_field_addr(const ValueId object, const IrTextId field_name) {
    const sema::TypeHandle enum_type = local_load_type(object);
    Value value = this->module_.make_value();
    value.kind = ValueKind::field_addr;
    value.name = field_name;
    value.object = object;
    value.type = module_.types.pointer(
        sema::PointerMutability::mut,
        aggregate_field_type(enum_type, this->module_.text(field_name))
    );
    return append_value(value);
}

void Lowerer::bind_pattern_locals(
    const syntax::PatternId pattern_id,
    const ValueId source_address,
    const sema::TypeHandle source_type
) {
    this->bind_pattern_locals_with_mutability(pattern_id, source_address, source_type, false);
}

void Lowerer::collect_pattern_binding_slots(
    const syntax::PatternId pattern_id,
    const sema::TypeHandle source_type,
    const bool is_mutable,
    std::unordered_map<sema::IdentId, PatternBindingSlot, sema::IdentIdHash>& slots
) {
    struct PatternTypeFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    };

    std::vector<PatternTypeFrame> pending;
    pending.push_back(PatternTypeFrame {pattern_id, source_type});
    while (!pending.empty()) {
        const PatternTypeFrame frame = pending.back();
        pending.pop_back();
        const syntax::PatternNode* pattern = this->pattern_node(frame.pattern);
        if (pattern == nullptr) {
            continue;
        }
        switch (pattern->kind) {
        case syntax::PatternKind::wildcard:
        case syntax::PatternKind::literal:
        case syntax::PatternKind::const_:
            break;
        case syntax::PatternKind::binding: {
            if (slots.contains(pattern->binding_name_id)) {
                break;
            }
            const ValueId slot = this->append_temp_alloca(pattern->binding_name, frame.type);
            this->bind_local(pattern->binding_name_id, LocalBinding {slot, is_mutable});
            slots.emplace(
                pattern->binding_name_id,
                PatternBindingSlot {this->module_.intern(pattern->binding_name), pattern->binding_name_id, slot, frame.type}
            );
            break;
        }
        case syntax::PatternKind::tuple: {
            if (!sema::is_valid(frame.type) || !this->module_.types.is_tuple(frame.type)) {
                break;
            }
            const sema::TypeInfo& tuple = this->module_.types.get(frame.type);
            const base::usize count = std::min(tuple.tuple_elements.size(), pattern->elements.size());
            for (base::usize i = count; i > 0; --i) {
                const base::usize element_index = i - 1;
                pending.push_back(PatternTypeFrame {
                    pattern->elements[element_index],
                    tuple.tuple_elements[element_index],
                });
            }
            break;
        }
        case syntax::PatternKind::slice: {
            if (!sema::is_valid(frame.type) ||
                (!this->module_.types.is_array(frame.type) && !this->module_.types.is_slice(frame.type))) {
                break;
            }
            const sema::TypeInfo& info = this->module_.types.get(frame.type);
            const sema::TypeHandle element_type = info.kind == sema::TypeKind::array
                ? info.array_element
                : info.slice_element;
            for (auto element = pattern->elements.rbegin(); element != pattern->elements.rend(); ++element) {
                pending.push_back(PatternTypeFrame {*element, element_type});
            }
            break;
        }
        case syntax::PatternKind::struct_:
            for (auto field_pattern = pattern->field_patterns.rbegin();
                 field_pattern != pattern->field_patterns.rend();
                 ++field_pattern) {
                pending.push_back(PatternTypeFrame {
                    field_pattern->pattern,
                    this->aggregate_field_type(frame.type, field_pattern->name),
                });
            }
            break;
        case syntax::PatternKind::enum_case: {
            if (pattern->payload_patterns.empty()) {
                break;
            }
            const sema::EnumCaseInfo* info = this->enum_case_info(this->pattern_case_symbol(frame.pattern));
            if (info == nullptr || !sema::is_valid(info->payload_type)) {
                break;
            }
            if (info->payload_types.size() == 1 && pattern->payload_patterns.size() == 1) {
                pending.push_back(PatternTypeFrame {pattern->payload_patterns.front(), info->payload_type});
                break;
            }
            const base::usize count = std::min(info->payload_types.size(), pattern->payload_patterns.size());
            for (base::usize i = count; i > 0; --i) {
                const base::usize payload_index = i - 1;
                pending.push_back(PatternTypeFrame {
                    pattern->payload_patterns[payload_index],
                    info->payload_types[payload_index],
                });
            }
            break;
        }
        case syntax::PatternKind::or_pattern:
            if (!pattern->alternatives.empty()) {
                pending.push_back(PatternTypeFrame {pattern->alternatives.front(), frame.type});
            }
            break;
        }
    }
}

void Lowerer::store_pattern_bindings(
    const syntax::PatternId pattern_id,
    const ValueId source_address,
    const sema::TypeHandle source_type,
    const std::unordered_map<sema::IdentId, PatternBindingSlot, sema::IdentIdHash>& slots
) {
    struct PatternFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        ValueId address = INVALID_VALUE_ID;
        sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    };

    std::vector<PatternFrame> pending;
    pending.push_back(PatternFrame {pattern_id, source_address, source_type});
    while (!pending.empty()) {
        const PatternFrame frame = pending.back();
        pending.pop_back();
        const syntax::PatternNode* pattern = this->pattern_node(frame.pattern);
        if (pattern == nullptr) {
            continue;
        }
        switch (pattern->kind) {
        case syntax::PatternKind::wildcard:
        case syntax::PatternKind::literal:
        case syntax::PatternKind::const_:
            break;
        case syntax::PatternKind::binding: {
            const auto slot = slots.find(pattern->binding_name_id);
            if (slot != slots.end()) {
                this->append_store(slot->second.slot, this->append_load(frame.address, frame.type, slot->second.name));
            }
            break;
        }
        case syntax::PatternKind::tuple: {
            if (!sema::is_valid(frame.type) || !this->module_.types.is_tuple(frame.type)) {
                break;
            }
            const sema::TypeInfo& tuple = this->module_.types.get(frame.type);
            const base::usize count = std::min(tuple.tuple_elements.size(), pattern->elements.size());
            for (base::usize i = count; i > 0; --i) {
                const base::usize element_index = i - 1;
                Value field = this->module_.make_value();
                field.kind = ValueKind::field_addr;
                field.name = this->module_.intern(std::to_string(element_index));
                field.object = frame.address;
                field.type = this->module_.types.pointer(sema::PointerMutability::mut, tuple.tuple_elements[element_index]);
                pending.push_back(PatternFrame {
                    pattern->elements[element_index],
                    this->append_value(field),
                    tuple.tuple_elements[element_index],
                });
            }
            break;
        }
        case syntax::PatternKind::slice: {
            if (!sema::is_valid(frame.type) ||
                (!this->module_.types.is_array(frame.type) && !this->module_.types.is_slice(frame.type))) {
                break;
            }
            const sema::TypeInfo& info = this->module_.types.get(frame.type);
            const sema::TypeHandle element_type = info.kind == sema::TypeKind::array
                ? info.array_element
                : info.slice_element;
            const sema::TypeHandle usize_type = this->module_.types.builtin(sema::BuiltinType::usize);
            const ValueId length = this->append_pattern_source_length(frame.address, frame.type);
            const base::usize rest_index = pattern->has_slice_rest
                ? std::min(pattern->slice_rest_index, pattern->elements.size())
                : pattern->elements.size();
            const base::usize suffix_count = pattern->has_slice_rest
                ? pattern->elements.size() - rest_index
                : 0;
            for (base::usize i = pattern->elements.size(); i > 0; --i) {
                const base::usize element_index = i - 1;
                ValueId index = INVALID_VALUE_ID;
                if (!pattern->has_slice_rest || element_index < rest_index) {
                    index = this->append_integer_literal(std::to_string(element_index), usize_type);
                } else {
                    const base::usize suffix_index = element_index - rest_index;
                    const ValueId suffix_offset =
                        this->append_integer_literal(std::to_string(suffix_count - suffix_index), usize_type);
                    index = this->append_binary_value(BinaryOp::sub, usize_type, length, suffix_offset);
                }
                pending.push_back(PatternFrame {
                    pattern->elements[element_index],
                    this->append_pattern_element_address(frame.address, frame.type, index, element_type),
                    element_type,
                });
            }
            break;
        }
        case syntax::PatternKind::struct_:
            for (auto field_pattern = pattern->field_patterns.rbegin();
                 field_pattern != pattern->field_patterns.rend();
                 ++field_pattern) {
                const sema::TypeHandle field_type = this->aggregate_field_type(frame.type, field_pattern->name);
                Value field = this->module_.make_value();
                field.kind = ValueKind::field_addr;
                field.name = this->module_.intern(field_pattern->name);
                field.object = frame.address;
                field.type = this->module_.types.pointer(sema::PointerMutability::mut, field_type);
                pending.push_back(PatternFrame {
                    field_pattern->pattern,
                    this->append_value(field),
                    field_type,
                });
            }
            break;
        case syntax::PatternKind::enum_case: {
            if (pattern->payload_patterns.empty()) {
                break;
            }
            const sema::EnumCaseInfo* info = this->enum_case_info(this->pattern_case_symbol(frame.pattern));
            if (info == nullptr || !sema::is_valid(info->payload_type)) {
                break;
            }
            const ValueId storage_addr =
                this->enum_field_addr(frame.address, this->module_.intern(IR_ENUM_PAYLOAD_FIELD_NAME));
            Value cast = this->module_.make_value();
            cast.kind = ValueKind::cast;
            cast.type = this->module_.types.pointer(sema::PointerMutability::mut, info->payload_type);
            cast.target_type = cast.type;
            cast.lhs = storage_addr;
            cast.cast_kind = CastKind::pointer;
            const ValueId payload_addr = this->append_value(cast);
            if (info->payload_types.size() == 1 && pattern->payload_patterns.size() == 1) {
                pending.push_back(PatternFrame {
                    pattern->payload_patterns.front(),
                    payload_addr,
                    info->payload_type,
                });
                break;
            }
            const base::usize count = std::min(info->payload_types.size(), pattern->payload_patterns.size());
            for (base::usize i = count; i > 0; --i) {
                const base::usize payload_index = i - 1;
                Value field = this->module_.make_value();
                field.kind = ValueKind::field_addr;
                field.type = this->module_.types.pointer(sema::PointerMutability::mut, info->payload_types[payload_index]);
                field.name =
                    this->module_.intern(std::string(IR_ENUM_SYNTHETIC_PAYLOAD_FIELD_PREFIX) + std::to_string(payload_index));
                field.object = payload_addr;
                pending.push_back(PatternFrame {
                    pattern->payload_patterns[payload_index],
                    this->append_value(field),
                    info->payload_types[payload_index],
                });
            }
            break;
        }
        case syntax::PatternKind::or_pattern:
            break;
        }
    }
}

void Lowerer::bind_pattern_locals_with_mutability(
    const syntax::PatternId pattern_id,
    const ValueId source_address,
    const sema::TypeHandle source_type,
    const bool is_mutable
) {
    const syntax::PatternNode* pattern = this->pattern_node(pattern_id);
    if (pattern == nullptr) {
        return;
    }

    std::unordered_map<sema::IdentId, PatternBindingSlot, sema::IdentIdHash> slots;
    this->collect_pattern_binding_slots(pattern_id, source_type, is_mutable, slots);
    if (pattern->kind != syntax::PatternKind::or_pattern) {
        this->store_pattern_bindings(pattern_id, source_address, source_type, slots);
        return;
    }
    if (pattern->alternatives.empty()) {
        return;
    }

    const BlockId join_block =
        add_block(this->module_, *this->current_function_, "pattern.bind.join" + std::to_string(this->current_function_->blocks.size()));
    for (base::usize i = 0; i < pattern->alternatives.size(); ++i) {
        const syntax::PatternId alternative = pattern->alternatives[i];
        const BlockId alternative_block =
            add_block(this->module_, *this->current_function_, "pattern.bind.alt" + std::to_string(this->current_function_->blocks.size()));
        BlockId next_block = INVALID_BLOCK_ID;
        if (i + 1 == pattern->alternatives.size()) {
            this->append_branch_if_open(alternative_block);
        } else {
            next_block =
                add_block(this->module_, *this->current_function_, "pattern.bind.next" + std::to_string(this->current_function_->blocks.size()));
            const ValueId condition = this->append_pattern_condition(alternative, source_address, source_type);
            Terminator branch;
            branch.kind = TerminatorKind::cond_branch;
            branch.condition = condition;
            branch.then_target = alternative_block;
            branch.else_target = next_block;
            this->set_terminator(this->current_block_, branch);
        }

        this->current_block_ = alternative_block;
        this->store_pattern_bindings(alternative, source_address, source_type, slots);
        this->append_branch_if_open(join_block);
        if (is_valid(next_block)) {
            this->current_block_ = next_block;
        }
    }
    this->current_block_ = join_block;
}

} // namespace aurex::ir::detail
