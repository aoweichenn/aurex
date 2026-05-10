#include "lower_ast_internal.hpp"

#include "aurex/ir/enum_layout.hpp"

namespace aurex::ir::detail {

namespace {

[[nodiscard]] UnaryOp map_unary(const syntax::UnaryOp op) noexcept {
    switch (op) {
    case syntax::UnaryOp::logical_not: return UnaryOp::logical_not;
    case syntax::UnaryOp::numeric_negate: return UnaryOp::numeric_negate;
    case syntax::UnaryOp::bitwise_not: return UnaryOp::bitwise_not;
    case syntax::UnaryOp::address_of: return UnaryOp::address_of;
    case syntax::UnaryOp::dereference: return UnaryOp::dereference;
    }
    return UnaryOp::logical_not;
}

[[nodiscard]] BinaryOp map_binary(const syntax::BinaryOp op) noexcept {
    switch (op) {
    case syntax::BinaryOp::add: return BinaryOp::add;
    case syntax::BinaryOp::sub: return BinaryOp::sub;
    case syntax::BinaryOp::mul: return BinaryOp::mul;
    case syntax::BinaryOp::div: return BinaryOp::div;
    case syntax::BinaryOp::mod: return BinaryOp::mod;
    case syntax::BinaryOp::shl: return BinaryOp::shl;
    case syntax::BinaryOp::shr: return BinaryOp::shr;
    case syntax::BinaryOp::less: return BinaryOp::less;
    case syntax::BinaryOp::less_equal: return BinaryOp::less_equal;
    case syntax::BinaryOp::greater: return BinaryOp::greater;
    case syntax::BinaryOp::greater_equal: return BinaryOp::greater_equal;
    case syntax::BinaryOp::equal: return BinaryOp::equal;
    case syntax::BinaryOp::not_equal: return BinaryOp::not_equal;
    case syntax::BinaryOp::bit_and: return BinaryOp::bit_and;
    case syntax::BinaryOp::bit_xor: return BinaryOp::bit_xor;
    case syntax::BinaryOp::bit_or: return BinaryOp::bit_or;
    case syntax::BinaryOp::logical_and: return BinaryOp::logical_and;
    case syntax::BinaryOp::logical_or: return BinaryOp::logical_or;
    }
    return BinaryOp::add;
}

} // namespace

ValueId Lowerer::lower_short_circuit_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    const ValueId lhs = lower_expr(expr.binary_lhs);
    const BlockId lhs_block = current_block_;

    const BlockId rhs_block = add_block(*current_function_, "logical.rhs" + std::to_string(current_function_->blocks.size()));
    const BlockId exit_block = add_block(*current_function_, "logical.exit" + std::to_string(current_function_->blocks.size()));

    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = lhs;
    if (expr.binary_op == syntax::BinaryOp::logical_and) {
        cond.then_target = rhs_block;
        cond.else_target = exit_block;
    } else {
        cond.then_target = exit_block;
        cond.else_target = rhs_block;
    }
    set_terminator(current_block_, cond);

    current_block_ = rhs_block;
    const ValueId rhs = lower_expr(expr.binary_rhs);
    const BlockId rhs_tail_block = current_block_;
    append_branch_if_open(exit_block);

    current_block_ = exit_block;
    Value result;
    result.kind = ValueKind::phi;
    result.type = expr_type(expr_id);
    result.incoming.push_back(PhiInput {lhs_block, lhs});
    result.incoming.push_back(PhiInput {rhs_tail_block, rhs});
    return append_value(result);
}

ValueId Lowerer::lower_if_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    if (current_function_ == nullptr || !is_valid(current_block_)) {
        return invalid_value_id;
    }
    const ValueId condition = lower_expr(expr.condition);
    const BlockId then_block = add_block(*current_function_, "if.expr.then" + std::to_string(current_function_->blocks.size()));
    const BlockId else_block = add_block(*current_function_, "if.expr.else" + std::to_string(current_function_->blocks.size()));
    const BlockId join_block = add_block(*current_function_, "if.expr.join" + std::to_string(current_function_->blocks.size()));

    Terminator cond;
    cond.kind = TerminatorKind::cond_branch;
    cond.condition = condition;
    cond.then_target = then_block;
    cond.else_target = else_block;
    set_terminator(current_block_, cond);

    current_block_ = then_block;
    const ValueId then_value = lower_expr(expr.then_expr, expr_type(expr_id));
    const BlockId then_tail_block = current_block_;
    append_branch_if_open(join_block);

    current_block_ = else_block;
    const ValueId else_value = lower_expr(expr.else_expr, expr_type(expr_id));
    const BlockId else_tail_block = current_block_;
    append_branch_if_open(join_block);

    current_block_ = join_block;
    Value result;
    result.kind = ValueKind::phi;
    result.type = expr_type(expr_id);
    result.incoming.push_back(PhiInput {then_tail_block, then_value});
    result.incoming.push_back(PhiInput {else_tail_block, else_value});
    return append_value(result);
}

ValueId Lowerer::lower_block_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    const auto previous_locals = locals_;
    const base::usize scope_depth = defer_scopes_.size();
    defer_scopes_.push_back({});
    lower_block_contents(expr.block);
    const ValueId result = lower_expr(expr.block_result, expr_type(expr_id));
    if (!has_terminator(current_block_)) {
        emit_deferred_scopes(scope_depth);
    }
    defer_scopes_.resize(scope_depth);
    locals_ = previous_locals;
    return result;
}

ValueId Lowerer::lower_expr(const syntax::ExprId expr_id) {
    return lower_expr(expr_id, sema::invalid_type_handle);
}

ValueId Lowerer::lower_expr(const syntax::ExprId expr_id, const sema::TypeHandle expected_type) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= ast_.exprs.size()) {
        return invalid_value_id;
    }
    const syntax::ExprNode& expr = ast_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::integer_literal:
    case syntax::ExprKind::float_literal:
    case syntax::ExprKind::bool_literal:
    case syntax::ExprKind::byte_literal: {
        Value value;
        if (expr.kind == syntax::ExprKind::byte_literal) {
            value.kind = ValueKind::byte_literal;
        } else if (expr.kind == syntax::ExprKind::bool_literal) {
            value.kind = ValueKind::bool_literal;
        } else if (expr.kind == syntax::ExprKind::float_literal) {
            value.kind = ValueKind::float_literal;
        } else {
            value.kind = ValueKind::integer_literal;
        }
        value.type = expr_type(expr_id);
        value.text = std::string(expr.text);
        return append_value(value);
    }
    case syntax::ExprKind::null_literal: {
        Value value;
        value.kind = ValueKind::null_literal;
        value.type = sema::is_valid(expected_type) ? expected_type : expr_type(expr_id);
        return append_value(value);
    }
    case syntax::ExprKind::string_literal:
    case syntax::ExprKind::c_string_literal: {
        Value value;
        value.kind = expr.kind == syntax::ExprKind::string_literal ? ValueKind::string_literal : ValueKind::c_string_literal;
        value.type = expr_type(expr_id);
        value.text = std::string(expr.text);
        return append_value(value);
    }
    case syntax::ExprKind::name:
        return lower_name(expr_id, expr);
    case syntax::ExprKind::unary: {
        if (expr.unary_op == syntax::UnaryOp::address_of) {
            return coerce_value(lower_place_addr(expr.unary_operand), expr_type(expr_id));
        }
        if (expr.unary_op == syntax::UnaryOp::dereference) {
            Value value;
            value.kind = ValueKind::load;
            value.type = expr_type(expr_id);
            value.object = lower_expr(expr.unary_operand);
            return append_value(value);
        }
        Value value;
        value.kind = ValueKind::unary;
        value.type = expr_type(expr_id);
        value.unary_op = map_unary(expr.unary_op);
        value.lhs = lower_expr(expr.unary_operand);
        return append_value(value);
    }
    case syntax::ExprKind::binary: {
        if (!this->lowering_constant_initializer_ &&
            (expr.binary_op == syntax::BinaryOp::logical_and || expr.binary_op == syntax::BinaryOp::logical_or)) {
            return this->lower_short_circuit_expr(expr_id, expr);
        }
        Value value;
        value.kind = ValueKind::binary;
        value.type = expr_type(expr_id);
        value.binary_op = map_binary(expr.binary_op);
        const sema::TypeHandle lhs_type = expr_type(expr.binary_lhs);
        const sema::TypeHandle rhs_type = expr_type(expr.binary_rhs);
        const sema::TypeHandle lhs_expected = !sema::is_valid(lhs_type) && module_.types.is_pointer(rhs_type)
            ? rhs_type
            : sema::invalid_type_handle;
        const sema::TypeHandle rhs_expected = !sema::is_valid(rhs_type) && module_.types.is_pointer(lhs_type)
            ? lhs_type
            : sema::invalid_type_handle;
        value.lhs = lower_expr(expr.binary_lhs, lhs_expected);
        value.rhs = lower_expr(expr.binary_rhs, rhs_expected);
        return append_value(value);
    }
    case syntax::ExprKind::call: {
        const std::string symbol = call_symbol(expr.callee);
        if (const sema::EnumCaseInfo* enum_case = enum_case_info(symbol);
            enum_case != nullptr && sema::is_valid(enum_case->payload_type)) {
            return lower_enum_constructor(*enum_case, expr.args.empty() ? syntax::invalid_expr_id : expr.args.front());
        }
        Value value;
        value.kind = ValueKind::call;
        value.type = expr_type(expr_id);
        const CallTarget target = call_target(expr.callee);
        value.name = target.symbol;
        value.call_target = target.function;
        base::usize param_offset = 0;
        if (syntax::is_valid(expr.callee) &&
            expr.callee.value < ast_.exprs.size() &&
            ast_.exprs[expr.callee.value].kind == syntax::ExprKind::field &&
            is_valid(target.function) &&
            target.function.value < module_.functions.size() &&
            module_.functions[target.function.value].signature_params.size() == expr.args.size() + 1) {
            const syntax::ExprNode& callee = ast_.exprs[expr.callee.value];
            const sema::TypeHandle receiver_type = expr_type(callee.object);
            const sema::TypeHandle param_type = call_param_type(target.function, 0);
            ValueId receiver = invalid_value_id;
            if (sema::is_valid(param_type) &&
                module_.types.is_pointer(param_type) &&
                (!sema::is_valid(receiver_type) || !module_.types.is_pointer(receiver_type))) {
                const sema::TypeInfo& param_info = module_.types.get(param_type);
                receiver = param_info.pointer_mutability == sema::PointerMutability::mut
                    ? lower_place_addr(callee.object)
                    : lower_object_place_or_value(callee.object).address;
            } else {
                receiver = lower_expr(callee.object, param_type);
            }
            value.args.push_back(coerce_value(receiver, param_type));
            param_offset = 1;
        }
        const bool variadic_call =
            is_valid(target.function) &&
            target.function.value < module_.functions.size() &&
            module_.functions[target.function.value].is_variadic;
        for (base::usize i = 0; i < expr.args.size(); ++i) {
            sema::TypeHandle param_type = call_param_type(target.function, i + param_offset);
            ValueId arg = lower_expr(expr.args[i], param_type);
            if (variadic_call && !sema::is_valid(param_type) && is_valid(arg) && arg.value < module_.values.size()) {
                param_type = variadic_argument_type(module_.values[arg.value].type);
            }
            value.args.push_back(coerce_value(arg, param_type));
        }
        return append_value(value);
    }
    case syntax::ExprKind::try_expr:
        return lower_try_expr(expr_id, expr);
    case syntax::ExprKind::if_expr:
        return lower_if_expr(expr_id, expr);
    case syntax::ExprKind::block_expr:
        return lower_block_expr(expr_id, expr);
    case syntax::ExprKind::match_expr:
        return lower_match_expr(expr_id, expr);
    case syntax::ExprKind::field:
        if (const sema::EnumCaseInfo* enum_case = enum_case_info(value_symbol(expr_id, expr)); enum_case != nullptr) {
            if (!sema::is_valid(enum_case->payload_type) && is_payload_enum(module_.types, enum_case->type)) {
                return lower_enum_constructor(*enum_case, syntax::invalid_expr_id);
            }
            return append_enum_case_ref(enum_case->c_name, enum_case->type);
        }
        [[fallthrough]];
    case syntax::ExprKind::index: {
        Value value;
        value.kind = ValueKind::load;
        value.type = expr_type(expr_id);
        value.object = lower_place_address(expr_id).address;
        return append_value(value);
    }
    case syntax::ExprKind::struct_literal: {
        Value value;
        value.kind = ValueKind::aggregate;
        value.type = expr_type(expr_id);
        for (const syntax::FieldInit& init : expr.field_inits) {
            const sema::TypeHandle field_type = aggregate_field_type(value.type, init.name);
            value.fields.push_back(FieldValue {
                std::string(init.name),
                coerce_value(lower_expr(init.value, field_type), field_type),
            });
        }
        return append_value(value);
    }
    case syntax::ExprKind::cast:
    case syntax::ExprKind::ptr_cast:
    case syntax::ExprKind::bit_cast:
    case syntax::ExprKind::ptr_addr:
    case syntax::ExprKind::ptr_from_addr: {
        Value value;
        value.kind = ValueKind::cast;
        value.type = expr_type(expr_id);
        value.target_type = expr_type(expr_id);
        value.lhs = lower_expr(expr.cast_expr, expr_type(expr.cast_expr));
        if (expr.kind == syntax::ExprKind::ptr_cast) {
            value.cast_kind = CastKind::pointer;
        } else if (expr.kind == syntax::ExprKind::bit_cast) {
            value.cast_kind = CastKind::bitcast;
        } else if (expr.kind == syntax::ExprKind::ptr_addr) {
            value.cast_kind = CastKind::ptr_addr;
        } else if (expr.kind == syntax::ExprKind::ptr_from_addr) {
            value.cast_kind = CastKind::ptr_from_addr;
        }
        return append_value(value);
    }
    case syntax::ExprKind::size_of:
    case syntax::ExprKind::align_of: {
        Value value;
        value.kind = expr.kind == syntax::ExprKind::size_of ? ValueKind::size_of : ValueKind::align_of;
        value.type = expr_type(expr_id);
        value.target_type = syntax_type(expr.cast_type);
        return append_value(value);
    }
    case syntax::ExprKind::str_data:
    case syntax::ExprKind::str_byte_len: {
        Value value;
        value.kind = expr.kind == syntax::ExprKind::str_data ? ValueKind::str_data : ValueKind::str_byte_len;
        value.type = expr_type(expr_id);
        value.object = lower_expr(expr.cast_expr, expr_type(expr.cast_expr));
        return append_value(value);
    }
    case syntax::ExprKind::str_from_bytes_unchecked: {
        Value value;
        value.kind = ValueKind::str_from_bytes_unchecked;
        value.type = expr_type(expr_id);
        if (expr.args.size() == 2) {
            value.args.push_back(lower_expr(expr.args[0], expr_type(expr.args[0])));
            value.args.push_back(lower_expr(expr.args[1], expr_type(expr.args[1])));
        }
        return append_value(value);
    }
    case syntax::ExprKind::invalid:
        return invalid_value_id;
    }
    return invalid_value_id;
}

ValueId Lowerer::lower_name(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    const std::string name(expr.text);
    const auto local = expr.scope_name.empty() ? locals_.find(name) : locals_.end();
    if (local != locals_.end()) {
        Value value;
        value.kind = ValueKind::load;
        value.name = name;
        value.type = local_load_type(local->second.slot);
        value.object = local->second.slot;
        return append_value(value);
    }
    const std::string symbol = value_symbol(expr_id, expr);
    if (const sema::EnumCaseInfo* enum_case = enum_case_info(symbol);
        enum_case != nullptr &&
        !sema::is_valid(enum_case->payload_type) &&
        is_payload_enum(module_.types, enum_case->type)) {
        return lower_enum_constructor(*enum_case, syntax::invalid_expr_id);
    }
    if (const auto constant = constant_symbols_.find(symbol); constant != constant_symbols_.end()) {
        Value value;
        value.kind = ValueKind::constant_ref;
        value.name = symbol;
        value.constant = constant->second;
        value.type = module_.constants[constant->second.value].type;
        return append_value(value);
    }
    Value value;
    value.kind = ValueKind::load;
    value.name = expr.text.empty() ? "<global>" : std::string(expr.text);
    value.type = sema::invalid_type_handle;
    return append_value(value);
}

ValueId Lowerer::lower_place_addr(const syntax::ExprId expr_id) {
    return lower_place_address(expr_id).address;
}

PlaceAddress Lowerer::lower_place_address(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= ast_.exprs.size()) {
        return {};
    }
    const syntax::ExprNode& expr = ast_.exprs[expr_id.value];
    if (expr.kind == syntax::ExprKind::name && expr.scope_name.empty()) {
        const auto found = locals_.find(std::string(expr.text));
        if (found == locals_.end()) {
            return {};
        }
        return PlaceAddress {found->second.slot, found->second.is_mutable};
    }
    if (expr.kind == syntax::ExprKind::unary && expr.unary_op == syntax::UnaryOp::dereference) {
        return PlaceAddress {lower_expr(expr.unary_operand), pointee_is_mutable(expr.unary_operand)};
    }
    if (expr.kind == syntax::ExprKind::field) {
        const PlaceAddress object = lower_object_place_or_value(expr.object);
        const sema::PointerMutability mutability = object.is_mutable
            ? sema::PointerMutability::mut
            : sema::PointerMutability::const_;
        Value value;
        value.kind = ValueKind::field_addr;
        value.type = module_.types.pointer(mutability, expr_type(expr_id));
        value.name = std::string(expr.field_name);
        value.object = object.address;
        return PlaceAddress {append_value(value), object.is_mutable};
    }
    if (expr.kind == syntax::ExprKind::index) {
        const PlaceAddress object = lower_object_place_or_value(expr.object);
        const sema::PointerMutability mutability = object.is_mutable
            ? sema::PointerMutability::mut
            : sema::PointerMutability::const_;
        Value value;
        value.kind = ValueKind::index_addr;
        value.type = module_.types.pointer(mutability, expr_type(expr_id));
        value.object = object.address;
        value.index = lower_expr(expr.index);
        return PlaceAddress {append_value(value), object.is_mutable};
    }
    return {};
}

PlaceAddress Lowerer::lower_object_place_or_value(const syntax::ExprId expr_id) {
    const sema::TypeHandle type = expr_type(expr_id);
    if (sema::is_valid(type) && module_.types.is_pointer(type)) {
        return PlaceAddress {lower_expr(expr_id), module_.types.get(type).pointer_mutability == sema::PointerMutability::mut};
    }
    const PlaceAddress place = lower_place_address(expr_id);
    if (is_valid(place.address)) {
        return place;
    }
    if (!sema::is_valid(type) || module_.types.is_void(type)) {
        return {};
    }
    const ValueId value = lower_expr(expr_id);
    if (!is_valid(value)) {
        return {};
    }
    const ValueId slot = append_temp_alloca("field.object", type);
    append_store(slot, value);
    return PlaceAddress {slot, false};
}

bool Lowerer::is_local_slot_type(const sema::TypeHandle type) const noexcept {
    return sema::is_valid(type) &&
           module_.types.is_pointer(type) &&
           module_.types.get(type).pointer_mutability == sema::PointerMutability::mut;
}

bool Lowerer::pointee_is_mutable(const syntax::ExprId expr_id) const noexcept {
    const sema::TypeHandle type = expr_type(expr_id);
    return sema::is_valid(type) &&
           module_.types.is_pointer(type) &&
           module_.types.get(type).pointer_mutability == sema::PointerMutability::mut;
}

CallTarget Lowerer::call_target(const syntax::ExprId callee) const {
    const std::string symbol = call_symbol(callee);
    const auto found = function_symbols_.find(symbol);
    if (found != function_symbols_.end()) {
        return CallTarget {found->second, symbol};
    }
    return CallTarget {invalid_function_id, symbol};
}

std::string Lowerer::call_symbol(const syntax::ExprId callee) const {
    if (current_generic_function_instance_ != nullptr && syntax::is_valid(callee)) {
        if (const auto found = current_generic_function_instance_->expr_c_names.find(callee.value);
            found != current_generic_function_instance_->expr_c_names.end()) {
            return found->second;
        }
    }
    if (syntax::is_valid(callee) &&
        callee.value < checked_.expr_c_names.size() &&
        !checked_.expr_c_names[callee.value].empty()) {
        return checked_.expr_c_names[callee.value];
    }
    if (syntax::is_valid(callee) && callee.value < ast_.exprs.size()) {
        return std::string(ast_.exprs[callee.value].text);
    }
    return "<invalid>";
}

std::string Lowerer::value_symbol(const syntax::ExprId expr_id, const syntax::ExprNode& expr) const {
    if (current_generic_function_instance_ != nullptr && syntax::is_valid(expr_id)) {
        if (const auto found = current_generic_function_instance_->expr_c_names.find(expr_id.value);
            found != current_generic_function_instance_->expr_c_names.end()) {
            return found->second;
        }
    }
    if (syntax::is_valid(expr_id) &&
        expr_id.value < checked_.expr_c_names.size() &&
        !checked_.expr_c_names[expr_id.value].empty()) {
        return checked_.expr_c_names[expr_id.value];
    }
    if (expr.kind == syntax::ExprKind::field) {
        return std::string(expr.field_name);
    }
    return std::string(expr.text);
}

sema::TypeHandle Lowerer::call_param_type(const FunctionId function_id, const base::usize index) const noexcept {
    if (!is_valid(function_id) || function_id.value >= module_.functions.size()) {
        return sema::invalid_type_handle;
    }
    const Function& function = module_.functions[function_id.value];
    if (index >= function.signature_params.size()) {
        return sema::invalid_type_handle;
    }
    return function.signature_params[index].type;
}

sema::TypeHandle Lowerer::variadic_argument_type(const sema::TypeHandle source_type) const noexcept {
    if (!sema::is_valid(source_type)) {
        return source_type;
    }
    const sema::TypeInfo& info = module_.types.get(source_type);
    if (info.kind != sema::TypeKind::builtin) {
        return source_type;
    }
    switch (info.builtin) {
    case sema::BuiltinType::bool_:
    case sema::BuiltinType::i8:
    case sema::BuiltinType::u8:
    case sema::BuiltinType::i16:
    case sema::BuiltinType::u16:
        return module_.types.builtin(sema::BuiltinType::i32);
    case sema::BuiltinType::f32:
        return module_.types.builtin(sema::BuiltinType::f64);
    default:
        return source_type;
    }
}

sema::TypeHandle Lowerer::expr_type(const syntax::ExprId expr) const noexcept {
    if (current_generic_function_instance_ != nullptr && syntax::is_valid(expr)) {
        if (const auto found = current_generic_function_instance_->expr_types.find(expr.value);
            found != current_generic_function_instance_->expr_types.end()) {
            return found->second;
        }
    }
    if (!syntax::is_valid(expr) || expr.value >= checked_.expr_types.size()) {
        return sema::invalid_type_handle;
    }
    return checked_.expr_types[expr.value];
}

sema::TypeHandle Lowerer::syntax_type(const syntax::TypeId type) const noexcept {
    if (current_generic_function_instance_ != nullptr && syntax::is_valid(type)) {
        if (const auto found = current_generic_function_instance_->syntax_type_handles.find(type.value);
            found != current_generic_function_instance_->syntax_type_handles.end()) {
            return found->second;
        }
    }
    if (!syntax::is_valid(type) || type.value >= checked_.syntax_type_handles.size()) {
        return sema::invalid_type_handle;
    }
    return checked_.syntax_type_handles[type.value];
}

sema::TypeHandle Lowerer::stmt_local_type(const syntax::StmtId stmt) const noexcept {
    if (current_generic_function_instance_ != nullptr && syntax::is_valid(stmt)) {
        if (const auto found = current_generic_function_instance_->stmt_local_types.find(stmt.value);
            found != current_generic_function_instance_->stmt_local_types.end()) {
            return found->second;
        }
    }
    if (!syntax::is_valid(stmt) || stmt.value >= checked_.stmt_local_types.size()) {
        return sema::invalid_type_handle;
    }
    return checked_.stmt_local_types[stmt.value];
}

sema::TypeHandle Lowerer::aggregate_field_type(
    const sema::TypeHandle aggregate_type,
    const std::string_view name
) const noexcept {
    const RecordField* field = find_record_field(module_, aggregate_type, std::string(name));
    return field == nullptr ? sema::invalid_type_handle : field->type;
}

sema::TypeHandle Lowerer::local_load_type(const ValueId slot) const noexcept {
    if (!is_valid(slot) || slot.value >= module_.values.size()) {
        return sema::invalid_type_handle;
    }
    const sema::TypeHandle slot_type = module_.values[slot.value].type;
    if (!sema::is_valid(slot_type) || !module_.types.is_pointer(slot_type)) {
        return sema::invalid_type_handle;
    }
    return module_.types.get(slot_type).pointee;
}

ValueId Lowerer::coerce_value(const ValueId value_id, const sema::TypeHandle target_type) {
    if (!is_valid(value_id) || value_id.value >= module_.values.size()) {
        return value_id;
    }
    const sema::TypeHandle source_type = module_.values[value_id.value].type;
    if (!sema::is_valid(source_type) &&
        module_.values[value_id.value].kind == ValueKind::null_literal &&
        sema::is_valid(target_type) &&
        module_.types.is_pointer(target_type)) {
        module_.values[value_id.value].type = target_type;
        return value_id;
    }
    if (!sema::is_valid(target_type) || !sema::is_valid(source_type) || module_.types.same(source_type, target_type)) {
        return value_id;
    }
    const bool source_numeric =
        module_.types.is_integer(source_type) ||
        module_.types.is_float(source_type) ||
        module_.types.is_bool(source_type);
    const bool target_numeric =
        module_.types.is_integer(target_type) ||
        module_.types.is_float(target_type) ||
        module_.types.is_bool(target_type);
    if (source_numeric && target_numeric) {
        Value value;
        value.kind = ValueKind::cast;
        value.type = target_type;
        value.target_type = target_type;
        value.lhs = value_id;
        value.cast_kind = CastKind::numeric;
        return append_value(value);
    }
    if (is_local_slot_type(source_type) && module_.types.is_pointer(target_type)) {
        Value value;
        value.kind = ValueKind::cast;
        value.type = target_type;
        value.target_type = target_type;
        value.lhs = value_id;
        value.cast_kind = CastKind::pointer;
        return append_value(value);
    }
    return value_id;
}

ValueId Lowerer::append_value(const Value& value) {
    const ValueId id = add_value(module_, value);
    if (current_function_ != nullptr && is_valid(current_block_)) {
        current_function_->blocks[current_block_.value].values.push_back(id);
    }
    return id;
}

void Lowerer::append_store(const ValueId target, const ValueId source) {
    Value value;
    value.kind = ValueKind::store;
    value.type = module_.types.builtin(sema::BuiltinType::void_);
    value.object = target;
    value.lhs = coerce_value(source, local_load_type(target));
    static_cast<void>(append_value(value));
}

void Lowerer::append_branch_if_open(const BlockId target) {
    if (has_terminator(current_block_)) {
        return;
    }
    Terminator term;
    term.kind = TerminatorKind::branch;
    term.target = target;
    set_terminator(current_block_, term);
}

bool Lowerer::has_terminator(const BlockId block) const {
    if (current_function_ == nullptr || !is_valid(block) || block.value >= current_function_->blocks.size()) {
        return true;
    }
    return current_function_->blocks[block.value].terminator.kind != TerminatorKind::none;
}

void Lowerer::set_terminator(const BlockId block, const Terminator& terminator) {
    if (current_function_ == nullptr || !is_valid(block) || block.value >= current_function_->blocks.size()) {
        return;
    }
    current_function_->blocks[block.value].terminator = terminator;
}

} // namespace aurex::ir::detail
