#include "aurex/ir/lower_ast.hpp"

#include <unordered_map>
#include <utility>

namespace aurex::ir {

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

[[nodiscard]] Linkage item_linkage(const syntax::ItemNode& item) noexcept {
    if (item.is_extern_c) {
        return Linkage::extern_c;
    }
    if (item.is_export_c) {
        return Linkage::export_c;
    }
    return Linkage::internal;
}

[[nodiscard]] sema::TypeHandle expr_type(const sema::CheckedModule& checked, const syntax::ExprId expr) noexcept {
    if (!syntax::is_valid(expr) || expr.value >= checked.expr_types.size()) {
        return sema::invalid_type_handle;
    }
    return checked.expr_types[expr.value];
}

struct CallTarget {
    FunctionId function = invalid_function_id;
    std::string symbol;
};

struct PlaceAddress {
    ValueId address = invalid_value_id;
    bool is_mutable = true;
};

struct LocalBinding {
    ValueId slot = invalid_value_id;
    bool is_mutable = false;
};

struct PendingConstant {
    GlobalConstantId id = invalid_global_constant_id;
    syntax::ExprId initializer = syntax::invalid_expr_id;
    sema::TypeHandle type = sema::invalid_type_handle;
    std::string literal_text;
    bool is_literal = false;
};

class Lowerer final {
public:
    Lowerer(const syntax::AstModule& ast, const sema::CheckedModule& checked)
        : ast_(ast), checked_(checked) {
        module_.types = checked_.types;
        item_functions_.assign(ast_.items.size(), invalid_function_id);
    }

    [[nodiscard]] Module lower() {
        lower_record_layouts();
        declare_global_constants();
        lower_function_declarations();
        lower_global_constant_initializers();
        for (base::u32 index = 0; index < ast_.items.size(); ++index) {
            const syntax::ItemNode& item = ast_.items[index];
            if (item.kind != syntax::ItemKind::fn_decl || item.is_extern_c || !syntax::is_valid(item.body)) {
                continue;
            }
            lower_function_body(item_functions_[index], item);
        }
        return std::move(module_);
    }

private:
    void lower_record_layouts() {
        for (const auto& entry : checked_.structs) {
            const sema::StructInfo& info = entry.second;
            RecordLayout record;
            record.type = info.type;
            record.name = info.name;
            record.symbol = info.c_name;
            record.is_opaque = info.is_opaque;
            for (const sema::StructFieldInfo& field : info.fields) {
                record.fields.push_back(RecordField {
                    field.name,
                    field.type,
                });
            }
            module_.records.push_back(std::move(record));
        }
    }

    void declare_global_constants() {
        for (base::u32 index = 0; index < ast_.items.size(); ++index) {
            const syntax::ItemNode& item = ast_.items[index];
            if (item.kind == syntax::ItemKind::const_decl) {
                GlobalConstant constant;
                constant.name = std::string(item.name);
                constant.symbol = item_symbol(index, item);
                constant.type = syntax_type(item.const_type);
                const GlobalConstantId id = add_global_constant(module_, std::move(constant));
                constant_symbols_[module_.constants[id.value].symbol] = id;
                pending_constants_.push_back(PendingConstant {
                    id,
                    item.const_value,
                    module_.constants[id.value].type,
                    {},
                    false,
                });
                continue;
            }
            if (item.kind != syntax::ItemKind::enum_decl) {
                continue;
            }
            for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
                GlobalConstant constant;
                constant.name = std::string(item.name) + "_" + std::string(enum_case.name);
                constant.symbol = enum_case_symbol(index, item, enum_case);
                constant.type = enum_case_type(constant.symbol);
                const GlobalConstantId id = add_global_constant(module_, std::move(constant));
                constant_symbols_[module_.constants[id.value].symbol] = id;
                pending_constants_.push_back(PendingConstant {
                    id,
                    syntax::invalid_expr_id,
                    module_.constants[id.value].type,
                    std::string(enum_case.value_text),
                    true,
                });
            }
        }
    }

    void lower_function_declarations() {
        for (base::u32 index = 0; index < ast_.items.size(); ++index) {
            const syntax::ItemNode& item = ast_.items[index];
            if (item.kind != syntax::ItemKind::fn_decl) {
                continue;
            }
            Function function;
            function.name = std::string(item.name);
            function.symbol = item_symbol(index, item);
            function.linkage = item_linkage(item);
            function.call_conv = item.is_extern_c || item.is_export_c
                ? AbiCallConv::c
                : AbiCallConv::aurex;
            function.return_type = syntax_type(item.return_type);
            for (const syntax::ParamDecl& param : item.params) {
                function.signature_params.push_back(FunctionParam {
                    std::string(param.name),
                    syntax_type(param.type),
                });
            }
            const FunctionId function_id {static_cast<base::u32>(module_.functions.size())};
            item_functions_[index] = function_id;
            function_symbols_[function.symbol] = function_id;
            module_.functions.push_back(std::move(function));
        }
    }

    void lower_global_constant_initializers() {
        for (const PendingConstant& pending : pending_constants_) {
            if (!is_valid(pending.id) || pending.id.value >= module_.constants.size()) {
                continue;
            }
            ValueId initializer = invalid_value_id;
            if (pending.is_literal) {
                Value value;
                value.kind = ValueKind::integer_literal;
                value.type = pending.type;
                value.text = pending.literal_text;
                initializer = append_value(value);
            } else {
                initializer = lower_expr(pending.initializer, pending.type);
                initializer = coerce_value(initializer, pending.type);
            }
            module_.constants[pending.id.value].initializer = initializer;
        }
    }

    [[nodiscard]] std::string item_symbol(const base::u32 index, const syntax::ItemNode& item) const {
        if (index < checked_.item_c_names.size() && !checked_.item_c_names[index].empty()) {
            return checked_.item_c_names[index];
        }
        if (!item.abi_name.empty()) {
            return std::string(item.abi_name);
        }
        return std::string(item.name);
    }

    [[nodiscard]] std::string enum_case_symbol(
        const base::u32 index,
        const syntax::ItemNode& item,
        const syntax::EnumCaseDecl& enum_case
    ) const {
        return item_symbol(index, item) + "_" + std::string(enum_case.name);
    }

    [[nodiscard]] sema::TypeHandle enum_case_type(const std::string& symbol) const noexcept {
        for (const auto& entry : checked_.enum_cases) {
            if (entry.second.c_name == symbol) {
                return entry.second.type;
            }
        }
        return sema::invalid_type_handle;
    }

    void lower_function_body(const FunctionId function_id, const syntax::ItemNode& item) {
        if (!is_valid(function_id) || function_id.value >= module_.functions.size()) {
            return;
        }
        current_function_ = &module_.functions[function_id.value];
        locals_.clear();
        loop_breaks_.clear();
        loop_continues_.clear();
        current_block_ = add_block(*current_function_, "entry");

        for (const syntax::ParamDecl& param : item.params) {
            Value param_value;
            param_value.kind = ValueKind::param;
            param_value.name = std::string(param.name);
            param_value.type = syntax_type(param.type);
            const ValueId param_id = append_value(param_value);
            current_function_->param_values.push_back(param_id);

            Value slot;
            slot.kind = ValueKind::alloca;
            slot.name = std::string(param.name);
            slot.type = module_.types.pointer(sema::PointerMutability::mut, param_value.type);
            const ValueId slot_id = append_value(slot);
            locals_[std::string(param.name)] = LocalBinding {slot_id, false};
            append_store(slot_id, param_id);
        }

        lower_block(item.body);
        if (!has_terminator(current_block_)) {
            Terminator term;
            term.kind = TerminatorKind::return_;
            set_terminator(current_block_, term);
        }
        current_function_ = nullptr;
    }

    void lower_block(const syntax::StmtId block_id) {
        if (!syntax::is_valid(block_id) || block_id.value >= ast_.stmts.size()) {
            return;
        }
        const syntax::StmtNode& block = ast_.stmts[block_id.value];
        if (block.kind != syntax::StmtKind::block) {
            return;
        }
        for (const syntax::StmtId stmt : block.statements) {
            if (has_terminator(current_block_)) {
                break;
            }
            lower_stmt(stmt);
        }
    }

    void lower_stmt(const syntax::StmtId stmt_id) {
        if (!syntax::is_valid(stmt_id) || stmt_id.value >= ast_.stmts.size()) {
            return;
        }
        const syntax::StmtNode& stmt = ast_.stmts[stmt_id.value];
        switch (stmt.kind) {
        case syntax::StmtKind::let:
        case syntax::StmtKind::var: {
            const sema::TypeHandle declared_type = syntax_type(stmt.declared_type);
            Value slot;
            slot.kind = ValueKind::alloca;
            slot.name = std::string(stmt.name);
            slot.type = module_.types.pointer(sema::PointerMutability::mut, declared_type);
            const ValueId slot_id = append_value(slot);
            locals_[std::string(stmt.name)] = LocalBinding {slot_id, stmt.kind == syntax::StmtKind::var};
            append_store(slot_id, lower_expr(stmt.init, declared_type));
            break;
        }
        case syntax::StmtKind::assign:
            append_store(lower_place_addr(stmt.lhs), lower_expr(stmt.rhs, expr_type(checked_, stmt.lhs)));
            break;
        case syntax::StmtKind::if_:
            lower_if(stmt);
            break;
        case syntax::StmtKind::while_:
            lower_while(stmt);
            break;
        case syntax::StmtKind::break_: {
            Terminator term;
            term.kind = TerminatorKind::branch;
            term.target = loop_breaks_.empty() ? invalid_block_id : loop_breaks_.back();
            set_terminator(current_block_, term);
            break;
        }
        case syntax::StmtKind::continue_: {
            Terminator term;
            term.kind = TerminatorKind::branch;
            term.target = loop_continues_.empty() ? invalid_block_id : loop_continues_.back();
            set_terminator(current_block_, term);
            break;
        }
        case syntax::StmtKind::return_: {
            Terminator term;
            term.kind = TerminatorKind::return_;
            if (syntax::is_valid(stmt.return_value)) {
                term.value = coerce_value(lower_expr(stmt.return_value, current_function_->return_type), current_function_->return_type);
            }
            set_terminator(current_block_, term);
            break;
        }
        case syntax::StmtKind::expr:
            static_cast<void>(lower_expr(stmt.init));
            break;
        case syntax::StmtKind::block:
            lower_block(stmt_id);
            break;
        }
    }

    void lower_if(const syntax::StmtNode& stmt) {
        const ValueId condition = lower_expr(stmt.condition);
        const BlockId then_block = add_block(*current_function_, "if.then" + std::to_string(current_function_->blocks.size()));
        const BlockId else_block = add_block(*current_function_, "if.else" + std::to_string(current_function_->blocks.size()));
        BlockId join_block = invalid_block_id;
        const auto ensure_join_block = [&]() -> BlockId {
            if (!is_valid(join_block)) {
                join_block = add_block(*current_function_, "if.join" + std::to_string(current_function_->blocks.size()));
            }
            return join_block;
        };

        Terminator cond;
        cond.kind = TerminatorKind::cond_branch;
        cond.condition = condition;
        cond.then_target = then_block;
        cond.else_target = else_block;
        set_terminator(current_block_, cond);

        current_block_ = then_block;
        lower_block(stmt.then_block);
        const bool then_open = !has_terminator(current_block_);
        if (then_open) {
            append_branch_if_open(ensure_join_block());
        }

        current_block_ = else_block;
        if (syntax::is_valid(stmt.else_block)) {
            lower_block(stmt.else_block);
        }
        if (syntax::is_valid(stmt.else_if)) {
            lower_stmt(stmt.else_if);
        }
        const bool else_open = !has_terminator(current_block_);
        if (else_open) {
            append_branch_if_open(ensure_join_block());
        }

        current_block_ = is_valid(join_block) ? join_block : invalid_block_id;
    }

    void lower_while(const syntax::StmtNode& stmt) {
        const BlockId condition_block = add_block(*current_function_, "while.cond" + std::to_string(current_function_->blocks.size()));

        append_branch_if_open(condition_block);
        current_block_ = condition_block;
        const ValueId condition = lower_expr(stmt.condition);
        const BlockId body_block = add_block(*current_function_, "while.body" + std::to_string(current_function_->blocks.size()));
        const BlockId exit_block = add_block(*current_function_, "while.exit" + std::to_string(current_function_->blocks.size()));
        Terminator cond;
        cond.kind = TerminatorKind::cond_branch;
        cond.condition = condition;
        cond.then_target = body_block;
        cond.else_target = exit_block;
        set_terminator(current_block_, cond);

        loop_breaks_.push_back(exit_block);
        loop_continues_.push_back(condition_block);
        current_block_ = body_block;
        lower_block(stmt.body);
        append_branch_if_open(condition_block);
        loop_continues_.pop_back();
        loop_breaks_.pop_back();

        current_block_ = exit_block;
    }

    [[nodiscard]] ValueId lower_short_circuit_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
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
        result.type = expr_type(checked_, expr_id);
        result.incoming.push_back(PhiInput {lhs_block, lhs});
        result.incoming.push_back(PhiInput {rhs_tail_block, rhs});
        return append_value(result);
    }

    [[nodiscard]] ValueId lower_expr(const syntax::ExprId expr_id) {
        return lower_expr(expr_id, sema::invalid_type_handle);
    }

    [[nodiscard]] ValueId lower_expr(const syntax::ExprId expr_id, const sema::TypeHandle expected_type) {
        if (!syntax::is_valid(expr_id) || expr_id.value >= ast_.exprs.size()) {
            return invalid_value_id;
        }
        const syntax::ExprNode& expr = ast_.exprs[expr_id.value];
        switch (expr.kind) {
        case syntax::ExprKind::integer_literal:
        case syntax::ExprKind::bool_literal:
        case syntax::ExprKind::byte_literal: {
            Value value;
            value.kind = expr.kind == syntax::ExprKind::byte_literal ? ValueKind::byte_literal :
                (expr.kind == syntax::ExprKind::bool_literal ? ValueKind::bool_literal : ValueKind::integer_literal);
            value.type = expr_type(checked_, expr_id);
            value.text = std::string(expr.text);
            return append_value(value);
        }
        case syntax::ExprKind::null_literal: {
            Value value;
            value.kind = ValueKind::null_literal;
            value.type = sema::is_valid(expected_type) ? expected_type : expr_type(checked_, expr_id);
            return append_value(value);
        }
        case syntax::ExprKind::string_literal:
        case syntax::ExprKind::c_string_literal: {
            Value value;
            value.kind = expr.kind == syntax::ExprKind::string_literal ? ValueKind::string_literal : ValueKind::c_string_literal;
            value.type = expr_type(checked_, expr_id);
            value.text = std::string(expr.text);
            return append_value(value);
        }
        case syntax::ExprKind::name:
            return lower_name(expr_id, expr);
        case syntax::ExprKind::unary: {
            if (expr.unary_op == syntax::UnaryOp::address_of) {
                return coerce_value(lower_place_addr(expr.unary_operand), expr_type(checked_, expr_id));
            }
            if (expr.unary_op == syntax::UnaryOp::dereference) {
                Value value;
                value.kind = ValueKind::load;
                value.type = expr_type(checked_, expr_id);
                value.object = lower_expr(expr.unary_operand);
                return append_value(value);
            }
            Value value;
            value.kind = ValueKind::unary;
            value.type = expr_type(checked_, expr_id);
            value.unary_op = map_unary(expr.unary_op);
            value.lhs = lower_expr(expr.unary_operand);
            return append_value(value);
        }
        case syntax::ExprKind::binary: {
            if (expr.binary_op == syntax::BinaryOp::logical_and || expr.binary_op == syntax::BinaryOp::logical_or) {
                return lower_short_circuit_expr(expr_id, expr);
            }
            Value value;
            value.kind = ValueKind::binary;
            value.type = expr_type(checked_, expr_id);
            value.binary_op = map_binary(expr.binary_op);
            const sema::TypeHandle lhs_type = expr_type(checked_, expr.binary_lhs);
            const sema::TypeHandle rhs_type = expr_type(checked_, expr.binary_rhs);
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
            Value value;
            value.kind = ValueKind::call;
            value.type = expr_type(checked_, expr_id);
            const CallTarget target = call_target(expr.callee);
            value.name = target.symbol;
            value.call_target = target.function;
            for (base::usize i = 0; i < expr.args.size(); ++i) {
                const sema::TypeHandle param_type = call_param_type(target.function, i);
                value.args.push_back(coerce_value(lower_expr(expr.args[i], param_type), param_type));
            }
            return append_value(value);
        }
        case syntax::ExprKind::field:
        case syntax::ExprKind::index: {
            Value value;
            value.kind = ValueKind::load;
            value.type = expr_type(checked_, expr_id);
            value.object = lower_place_address(expr_id).address;
            return append_value(value);
        }
        case syntax::ExprKind::struct_literal: {
            Value value;
            value.kind = ValueKind::aggregate;
            value.type = expr_type(checked_, expr_id);
            for (const syntax::FieldInit& init : expr.field_inits) {
                value.fields.push_back(FieldValue {
                    std::string(init.name),
                    lower_expr(init.value, aggregate_field_type(value.type, init.name)),
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
            value.type = expr_type(checked_, expr_id);
            value.target_type = expr_type(checked_, expr_id);
            value.lhs = lower_expr(expr.cast_expr, expr_type(checked_, expr.cast_expr));
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
            value.type = expr_type(checked_, expr_id);
            value.target_type = syntax_type(expr.cast_type);
            return append_value(value);
        }
        case syntax::ExprKind::invalid:
            return invalid_value_id;
        }
        return invalid_value_id;
    }

    [[nodiscard]] ValueId lower_name(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
        const std::string name(expr.text);
        const auto local = locals_.find(name);
        if (local != locals_.end()) {
            Value value;
            value.kind = ValueKind::load;
            value.name = name;
            value.type = local_load_type(local->second.slot);
            value.object = local->second.slot;
            return append_value(value);
        }
        const std::string symbol = value_symbol(expr_id, expr);
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

    [[nodiscard]] ValueId lower_place_addr(const syntax::ExprId expr_id) {
        return lower_place_address(expr_id).address;
    }

    [[nodiscard]] PlaceAddress lower_place_address(const syntax::ExprId expr_id) {
        if (!syntax::is_valid(expr_id) || expr_id.value >= ast_.exprs.size()) {
            return {};
        }
        const syntax::ExprNode& expr = ast_.exprs[expr_id.value];
        if (expr.kind == syntax::ExprKind::name) {
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
            value.type = module_.types.pointer(mutability, expr_type(checked_, expr_id));
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
            value.type = module_.types.pointer(mutability, expr_type(checked_, expr_id));
            value.object = object.address;
            value.index = lower_expr(expr.index);
            return PlaceAddress {append_value(value), object.is_mutable};
        }
        return {};
    }

    [[nodiscard]] PlaceAddress lower_object_place_or_value(const syntax::ExprId expr_id) {
        const sema::TypeHandle type = expr_type(checked_, expr_id);
        if (sema::is_valid(type) && module_.types.is_pointer(type)) {
            return PlaceAddress {lower_expr(expr_id), module_.types.get(type).pointer_mutability == sema::PointerMutability::mut};
        }
        return lower_place_address(expr_id);
    }

    [[nodiscard]] bool is_local_slot_type(const sema::TypeHandle type) const noexcept {
        return sema::is_valid(type) &&
               module_.types.is_pointer(type) &&
               module_.types.get(type).pointer_mutability == sema::PointerMutability::mut;
    }

    [[nodiscard]] bool pointee_is_mutable(const syntax::ExprId expr_id) const noexcept {
        const sema::TypeHandle type = expr_type(checked_, expr_id);
        return sema::is_valid(type) &&
               module_.types.is_pointer(type) &&
               module_.types.get(type).pointer_mutability == sema::PointerMutability::mut;
    }

    [[nodiscard]] CallTarget call_target(const syntax::ExprId callee) const {
        const std::string symbol = call_symbol(callee);
        const auto found = function_symbols_.find(symbol);
        if (found != function_symbols_.end()) {
            return CallTarget {found->second, symbol};
        }
        return CallTarget {invalid_function_id, symbol};
    }

    [[nodiscard]] std::string call_symbol(const syntax::ExprId callee) const {
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

    [[nodiscard]] std::string value_symbol(const syntax::ExprId expr_id, const syntax::ExprNode& expr) const {
        if (syntax::is_valid(expr_id) &&
            expr_id.value < checked_.expr_c_names.size() &&
            !checked_.expr_c_names[expr_id.value].empty()) {
            return checked_.expr_c_names[expr_id.value];
        }
        return std::string(expr.text);
    }

    [[nodiscard]] sema::TypeHandle call_param_type(const FunctionId function_id, const base::usize index) const noexcept {
        if (!is_valid(function_id) || function_id.value >= module_.functions.size()) {
            return sema::invalid_type_handle;
        }
        const Function& function = module_.functions[function_id.value];
        if (index >= function.signature_params.size()) {
            return sema::invalid_type_handle;
        }
        return function.signature_params[index].type;
    }

    [[nodiscard]] sema::TypeHandle syntax_type(const syntax::TypeId type) const noexcept {
        if (!syntax::is_valid(type) || type.value >= checked_.syntax_type_handles.size()) {
            return sema::invalid_type_handle;
        }
        return checked_.syntax_type_handles[type.value];
    }

    [[nodiscard]] sema::TypeHandle aggregate_field_type(
        const sema::TypeHandle aggregate_type,
        const std::string_view name
    ) const noexcept {
        const RecordField* field = find_record_field(module_, aggregate_type, std::string(name));
        return field == nullptr ? sema::invalid_type_handle : field->type;
    }

    [[nodiscard]] sema::TypeHandle local_load_type(const ValueId slot) const noexcept {
        if (!is_valid(slot) || slot.value >= module_.values.size()) {
            return sema::invalid_type_handle;
        }
        const sema::TypeHandle slot_type = module_.values[slot.value].type;
        if (!sema::is_valid(slot_type) || !module_.types.is_pointer(slot_type)) {
            return sema::invalid_type_handle;
        }
        return module_.types.get(slot_type).pointee;
    }

    [[nodiscard]] ValueId coerce_value(const ValueId value_id, const sema::TypeHandle target_type) {
        if (!is_valid(value_id) || value_id.value >= module_.values.size()) {
            return value_id;
        }
        const sema::TypeHandle source_type = module_.values[value_id.value].type;
        if (!sema::is_valid(source_type) && module_.values[value_id.value].kind == ValueKind::null_literal &&
            sema::is_valid(target_type) && module_.types.is_pointer(target_type)) {
            module_.values[value_id.value].type = target_type;
            return value_id;
        }
        if (!sema::is_valid(target_type) || !sema::is_valid(source_type) || module_.types.same(source_type, target_type)) {
            return value_id;
        }
        if (module_.types.is_integer(source_type) && module_.types.is_integer(target_type)) {
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

    [[nodiscard]] ValueId append_value(const Value& value) {
        const ValueId id = add_value(module_, value);
        if (current_function_ != nullptr && is_valid(current_block_)) {
            current_function_->blocks[current_block_.value].values.push_back(id);
        }
        return id;
    }

    void append_store(const ValueId target, const ValueId source) {
        Value value;
        value.kind = ValueKind::store;
        value.type = module_.types.builtin(sema::BuiltinType::void_);
        value.object = target;
        value.lhs = coerce_value(source, local_load_type(target));
        static_cast<void>(append_value(value));
    }

    void append_branch_if_open(const BlockId target) {
        if (has_terminator(current_block_)) {
            return;
        }
        Terminator term;
        term.kind = TerminatorKind::branch;
        term.target = target;
        set_terminator(current_block_, term);
    }

    [[nodiscard]] bool has_terminator(const BlockId block) const {
        if (current_function_ == nullptr || !is_valid(block) || block.value >= current_function_->blocks.size()) {
            return true;
        }
        return current_function_->blocks[block.value].terminator.kind != TerminatorKind::none;
    }

    void set_terminator(const BlockId block, const Terminator& terminator) {
        if (current_function_ == nullptr || !is_valid(block) || block.value >= current_function_->blocks.size()) {
            return;
        }
        current_function_->blocks[block.value].terminator = terminator;
    }

    const syntax::AstModule& ast_;
    const sema::CheckedModule& checked_;
    Module module_;
    Function* current_function_ = nullptr;
    BlockId current_block_ = invalid_block_id;
    std::unordered_map<std::string, LocalBinding> locals_;
    std::unordered_map<std::string, FunctionId> function_symbols_;
    std::unordered_map<std::string, GlobalConstantId> constant_symbols_;
    std::vector<PendingConstant> pending_constants_;
    std::vector<FunctionId> item_functions_;
    std::vector<BlockId> loop_breaks_;
    std::vector<BlockId> loop_continues_;
};

} // namespace

base::Result<Module> lower_ast(const syntax::AstModule& ast, const sema::CheckedModule& checked) {
    Lowerer lowerer(ast, checked);
    return base::Result<Module>::ok(lowerer.lower());
}

} // namespace aurex::ir
