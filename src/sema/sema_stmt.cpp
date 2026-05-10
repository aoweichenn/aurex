#include "aurex/sema/sema.hpp"

namespace aurex::sema {

namespace {

[[nodiscard]] bool is_allowed_expression_statement(
    const syntax::AstModule& module,
    const syntax::ExprId expr_id
) noexcept {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module.exprs.size()) {
        return false;
    }
    const syntax::ExprKind kind = module.exprs[expr_id.value].kind;
    return kind == syntax::ExprKind::call || kind == syntax::ExprKind::try_expr;
}

[[nodiscard]] bool compound_assignment_binary_op(
    const syntax::AssignOp op,
    syntax::BinaryOp& binary_op
) noexcept {
    switch (op) {
    case syntax::AssignOp::add:
        binary_op = syntax::BinaryOp::add;
        return true;
    case syntax::AssignOp::sub:
        binary_op = syntax::BinaryOp::sub;
        return true;
    case syntax::AssignOp::mul:
        binary_op = syntax::BinaryOp::mul;
        return true;
    case syntax::AssignOp::div:
        binary_op = syntax::BinaryOp::div;
        return true;
    case syntax::AssignOp::mod:
        binary_op = syntax::BinaryOp::mod;
        return true;
    case syntax::AssignOp::shl:
        binary_op = syntax::BinaryOp::shl;
        return true;
    case syntax::AssignOp::shr:
        binary_op = syntax::BinaryOp::shr;
        return true;
    case syntax::AssignOp::bit_and:
        binary_op = syntax::BinaryOp::bit_and;
        return true;
    case syntax::AssignOp::bit_xor:
        binary_op = syntax::BinaryOp::bit_xor;
        return true;
    case syntax::AssignOp::bit_or:
        binary_op = syntax::BinaryOp::bit_or;
        return true;
    case syntax::AssignOp::assign:
        return false;
    }
    return false;
}

} // namespace

void SemanticAnalyzer::analyze_function_body(const syntax::ItemNode& function) {
    const std::string key = function_key(function);
    const auto found = checked_.functions.find(key);
    if (found == checked_.functions.end()) {
        return;
    }
    analyze_function_body_with_signature(function, key, found->second, function_body_states_[key], nullptr);
}

void SemanticAnalyzer::analyze_function_body_with_signature(
    const syntax::ItemNode& function,
    const std::string& key,
    const FunctionSignature& signature,
    FunctionBodyState& state,
    const GenericTypeSubstitution* const substitution
) {
    const syntax::ModuleId previous_module = current_module_;
    const TypeHandle previous_function_return_type = current_function_return_type_;
    const GenericTypeSubstitution* const previous_type_substitution = current_type_substitution_;
    ReturnTypeInference* const previous_return_inference = current_return_inference_;
    const int previous_loop_depth = loop_depth_;
    const SymbolTable previous_symbols = symbols_;
    const auto restore_context = [&]() {
        current_module_ = previous_module;
        current_function_return_type_ = previous_function_return_type;
        current_type_substitution_ = previous_type_substitution;
        current_return_inference_ = previous_return_inference;
        loop_depth_ = previous_loop_depth;
        symbols_ = previous_symbols;
    };
    current_module_ = signature.module;
    current_type_substitution_ = substitution;
    symbols_ = SymbolTable {};
    if (signature.has_conflict) {
        restore_context();
        return;
    }
    if (state == FunctionBodyState::analyzing) {
        if (!is_valid(signature.return_type)) {
            report(function.range, "cannot infer recursive function return type without an explicit return type");
        }
        restore_context();
        return;
    }
    if (state == FunctionBodyState::analyzed) {
        restore_context();
        return;
    }
    state = FunctionBodyState::analyzing;
    loop_depth_ = 0;
    const bool infer_return_type = !syntax::is_valid(function.return_type);
    ReturnTypeInference return_inference;
    TypeHandle expected_return = signature.return_type;
    if (infer_return_type) {
        expected_return = invalid_type_handle;
    }
    current_function_return_type_ = expected_return;
    current_return_inference_ = infer_return_type ? &return_inference : nullptr;

    symbols_.push_scope();
    for (base::usize i = 0; i < function.params.size(); ++i) {
        const syntax::ParamDecl& param = function.params[i];
        const TypeHandle param_type = i < signature.param_types.size()
            ? signature.param_types[i]
            : resolve_type(param.type);
        const auto inserted = this->symbols_.insert(Symbol {
            SymbolKind::parameter,
            std::string(param.name),
            {},
            syntax::invalid_module_id,
            param_type,
            param.range,
            false,
            syntax::Visibility::private_,
        }, this->diagnostics_);
        static_cast<void>(inserted);
    }
    analyze_block(function.body, expected_return, infer_return_type ? &return_inference : nullptr);
    symbols_.pop_scope();
    if (infer_return_type) {
        finalize_inferred_return(function, key, return_inference);
        if (is_valid(return_inference.inferred_type) &&
            !checked_.types.is_void(return_inference.inferred_type) &&
            !block_guarantees_return(function.body)) {
            report(function.range, "not all control paths return a value");
        }
    } else if (is_valid(expected_return) &&
        !checked_.types.is_void(expected_return) &&
        !block_guarantees_return(function.body)) {
        report(function.range, "not all control paths return a value");
    }
    state = FunctionBodyState::analyzed;
    restore_context();
}

void SemanticAnalyzer::analyze_block(
    const syntax::StmtId block,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    symbols_.push_scope();
    analyze_block_statements(block, expected_return, return_inference);
    symbols_.pop_scope();
}

void SemanticAnalyzer::analyze_block_statements(
    const syntax::StmtId block,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    if (!syntax::is_valid(block) || block.value >= module_.stmts.size()) {
        return;
    }
    const syntax::StmtNode& stmt = module_.stmts[block.value];
    if (stmt.kind != syntax::StmtKind::block) {
        return;
    }
    for (syntax::StmtId child : stmt.statements) {
        analyze_stmt(child, expected_return, return_inference);
    }
}

TypeHandle SemanticAnalyzer::analyze_assignment_target(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return invalid_type_handle;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    if (expr.kind != syntax::ExprKind::name || !expr.scope_name.empty()) {
        return analyze_expr(expr_id);
    }
    const Symbol* symbol = find_symbol(expr.text, expr.range);
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

void SemanticAnalyzer::analyze_stmt(
    const syntax::StmtId stmt_id,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    if (!syntax::is_valid(stmt_id) || stmt_id.value >= module_.stmts.size()) {
        return;
    }
    const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
    switch (stmt.kind) {
    case syntax::StmtKind::let:
    case syntax::StmtKind::var: {
        const bool has_declared_type = syntax::is_valid(stmt.declared_type);
        const TypeHandle declared_type = has_declared_type ? resolve_type(stmt.declared_type) : invalid_type_handle;
        const TypeHandle init = analyze_expr(stmt.init, declared_type);
        const TypeHandle local_type = has_declared_type ? declared_type : init;
        record_stmt_local_type(stmt_id, local_type);
        if (!has_declared_type && !is_valid(local_type)) {
            report(stmt.range, "local variable type cannot be inferred");
        }
        if (is_valid(local_type) && !is_valid_storage_type(local_type)) {
            report(stmt.range, "local variable type is not valid storage");
        }
        if (has_declared_type && !can_assign(local_type, init, stmt.init)) {
            report(stmt.range, "initializer type does not match declared type");
        }
        const auto inserted = this->symbols_.insert(Symbol {
            SymbolKind::local,
            std::string(stmt.name),
            {},
            syntax::invalid_module_id,
            local_type,
            stmt.range,
            stmt.kind == syntax::StmtKind::var,
            syntax::Visibility::private_,
        }, this->diagnostics_);
        static_cast<void>(inserted);
        break;
    }
    case syntax::StmtKind::assign: {
        if (!is_writable_place(stmt.lhs)) {
            report(module_.exprs[stmt.lhs.value].range, "left side of assignment must be writable");
        }
        const TypeHandle lhs = analyze_assignment_target(stmt.lhs);
        syntax::BinaryOp binary_op = syntax::BinaryOp::add;
        if (compound_assignment_binary_op(stmt.assign_op, binary_op)) {
            syntax::ExprNode binary;
            binary.kind = syntax::ExprKind::binary;
            binary.range = stmt.range;
            binary.binary_op = binary_op;
            binary.binary_lhs = stmt.lhs;
            binary.binary_rhs = stmt.rhs;
            const TypeHandle result = analyze_expr(syntax::invalid_expr_id, binary, lhs);
            if (!can_assign(lhs, result, stmt.rhs)) {
                report(stmt.range, "compound assignment type mismatch");
            }
        } else {
            const TypeHandle rhs = analyze_expr(stmt.rhs, lhs);
            if (!can_assign(lhs, rhs, stmt.rhs)) {
                report(stmt.range, "assignment type mismatch");
            }
        }
        if (checked_.types.contains_array(lhs)) {
            report(stmt.range, "array or array-containing type cannot be assigned");
        }
        break;
    }
    case syntax::StmtKind::if_: {
        const TypeHandle condition = analyze_expr(stmt.condition);
        if (!checked_.types.is_bool(condition)) {
            report(module_.exprs[stmt.condition.value].range, "if condition must be bool");
        }
        analyze_block(stmt.then_block, expected_return, return_inference);
        if (syntax::is_valid(stmt.else_block)) {
            analyze_block(stmt.else_block, expected_return, return_inference);
        }
        if (syntax::is_valid(stmt.else_if)) {
            analyze_stmt(stmt.else_if, expected_return, return_inference);
        }
        break;
    }
    case syntax::StmtKind::while_: {
        const TypeHandle condition = analyze_expr(stmt.condition);
        if (!checked_.types.is_bool(condition)) {
            report(module_.exprs[stmt.condition.value].range, "while condition must be bool");
        }
        ++loop_depth_;
        analyze_block(stmt.body, expected_return, return_inference);
        --loop_depth_;
        break;
    }
    case syntax::StmtKind::for_: {
        symbols_.push_scope();
        if (syntax::is_valid(stmt.for_init)) {
            analyze_stmt(stmt.for_init, expected_return, return_inference);
        }
        if (syntax::is_valid(stmt.condition)) {
            const TypeHandle condition = analyze_expr(stmt.condition);
            if (!checked_.types.is_bool(condition)) {
                report(module_.exprs[stmt.condition.value].range, "for condition must be bool");
            }
        }
        ++loop_depth_;
        analyze_block(stmt.body, expected_return, return_inference);
        if (syntax::is_valid(stmt.for_update)) {
            analyze_stmt(stmt.for_update, expected_return, return_inference);
        }
        --loop_depth_;
        symbols_.pop_scope();
        break;
    }
    case syntax::StmtKind::return_: {
        const TypeHandle actual = syntax::is_valid(stmt.return_value)
            ? analyze_expr(stmt.return_value, expected_return)
            : checked_.types.builtin(BuiltinType::void_);
        if (return_inference != nullptr) {
            record_inferred_return(stmt_id, actual, *return_inference);
        } else if (is_valid(actual) &&
            is_valid(expected_return) &&
            !can_assign(expected_return, actual, stmt.return_value)) {
            report(stmt.range, "return type mismatch");
        }
        break;
    }
    case syntax::StmtKind::expr:
        if (syntax::is_valid(stmt.init) &&
            stmt.init.value < module_.exprs.size() &&
            !is_allowed_expression_statement(module_, stmt.init)) {
            report(module_.exprs[stmt.init.value].range, "expression statement must be a function call or try expression");
        }
        static_cast<void>(analyze_expr(stmt.init));
        break;
    case syntax::StmtKind::block:
        analyze_block(stmt_id, expected_return, return_inference);
        break;
    case syntax::StmtKind::break_:
    case syntax::StmtKind::continue_:
        if (loop_depth_ == 0) {
            report(stmt.range, "break and continue are only valid inside loops");
        }
        break;
    case syntax::StmtKind::defer:
        if (!syntax::is_valid(stmt.init) ||
            stmt.init.value >= module_.exprs.size() ||
            module_.exprs[stmt.init.value].kind != syntax::ExprKind::call) {
            report(stmt.range, "defer statement must be a function call");
            break;
        }
        static_cast<void>(analyze_expr(stmt.init));
        break;
    }
}

bool SemanticAnalyzer::block_guarantees_return(const syntax::StmtId block_id) const noexcept {
    if (!syntax::is_valid(block_id) || block_id.value >= module_.stmts.size()) {
        return false;
    }
    const syntax::StmtNode& block = module_.stmts[block_id.value];
    if (block.kind != syntax::StmtKind::block) {
        return stmt_guarantees_return(block_id);
    }
    for (const syntax::StmtId child : block.statements) {
        if (stmt_guarantees_return(child)) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::stmt_guarantees_return(const syntax::StmtId stmt_id) const noexcept {
    if (!syntax::is_valid(stmt_id) || stmt_id.value >= module_.stmts.size()) {
        return false;
    }
    const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
    switch (stmt.kind) {
    case syntax::StmtKind::return_:
        return true;
    case syntax::StmtKind::block:
        return block_guarantees_return(stmt_id);
    case syntax::StmtKind::if_: {
        if (!block_guarantees_return(stmt.then_block)) {
            return false;
        }
        if (syntax::is_valid(stmt.else_block)) {
            return block_guarantees_return(stmt.else_block);
        }
        if (syntax::is_valid(stmt.else_if)) {
            return stmt_guarantees_return(stmt.else_if);
        }
        return false;
    }
    default:
        return false;
    }
}

bool SemanticAnalyzer::block_may_fallthrough(const syntax::StmtId block_id) const noexcept {
    if (!syntax::is_valid(block_id) || block_id.value >= module_.stmts.size()) {
        return true;
    }
    const syntax::StmtNode& block = module_.stmts[block_id.value];
    if (block.kind != syntax::StmtKind::block) {
        return stmt_may_fallthrough(block_id);
    }
    for (const syntax::StmtId child : block.statements) {
        if (!stmt_may_fallthrough(child)) {
            return false;
        }
    }
    return true;
}

bool SemanticAnalyzer::stmt_may_fallthrough(const syntax::StmtId stmt_id) const noexcept {
    if (!syntax::is_valid(stmt_id) || stmt_id.value >= module_.stmts.size()) {
        return true;
    }
    const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
    switch (stmt.kind) {
    case syntax::StmtKind::return_:
    case syntax::StmtKind::break_:
    case syntax::StmtKind::continue_:
        return false;
    case syntax::StmtKind::block:
        return block_may_fallthrough(stmt_id);
    case syntax::StmtKind::if_: {
        const bool then_falls_through = block_may_fallthrough(stmt.then_block);
        if (syntax::is_valid(stmt.else_block)) {
            return then_falls_through || block_may_fallthrough(stmt.else_block);
        }
        if (syntax::is_valid(stmt.else_if)) {
            return then_falls_through || stmt_may_fallthrough(stmt.else_if);
        }
        return true;
    }
    default:
        return true;
    }
}

void SemanticAnalyzer::record_inferred_return(
    const syntax::StmtId stmt_id,
    const TypeHandle actual,
    ReturnTypeInference& inference
) {
    inference.returns.push_back(stmt_id);
    if (!is_valid(actual)) {
        if (syntax::is_valid(stmt_id) && stmt_id.value < module_.stmts.size()) {
            const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
            report(stmt.range, "function return type cannot be inferred");
        }
        return;
    }
    if (!is_valid(inference.inferred_type)) {
        inference.inferred_type = actual;
        return;
    }
    if (!checked_.types.same(inference.inferred_type, actual)) {
        if (syntax::is_valid(stmt_id) && stmt_id.value < module_.stmts.size()) {
            const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
            report(stmt.range, "inferred function return types do not match");
        }
    }
}

void SemanticAnalyzer::finalize_inferred_return(
    const syntax::ItemNode& function,
    const std::string& key,
    ReturnTypeInference& inference
) {
    TypeHandle return_type = inference.inferred_type;
    if (inference.returns.empty()) {
        return_type = checked_.types.builtin(BuiltinType::void_);
    }
    if (!is_valid(return_type)) {
        return_type = checked_.types.builtin(BuiltinType::void_);
    }
    validate_function_return_type(function, return_type);
    if (const auto found = checked_.functions.find(key); found != checked_.functions.end()) {
        found->second.return_type = return_type;
    }
    if (const auto global = global_values_.find(key); global != global_values_.end()) {
        global->second.type = return_type;
    }
}

void SemanticAnalyzer::validate_function_return_type(const syntax::ItemNode& function, const TypeHandle return_type) {
    if (checked_.types.is_array(return_type)) {
        report(function.range, "array type cannot be used as a function return type");
    }
    if (checked_.types.contains_array(return_type)) {
        report(function.range, "struct containing array cannot be returned by value");
    }
}

void SemanticAnalyzer::ensure_function_return_known(
    const FunctionSignature& signature,
    const base::SourceRange use_range
) {
    if (is_valid(signature.return_type) || signature.is_extern_c) {
        return;
    }
    const std::string key = signature.is_method
        ? method_key(signature.module, signature.method_owner_type, signature.name)
        : module_key(signature.module, signature.name);
    const FunctionBodyState state = function_body_states_.contains(key)
        ? function_body_states_.at(key)
        : FunctionBodyState::not_started;
    if (state == FunctionBodyState::analyzing) {
        report(use_range, "cannot infer recursive function return type without an explicit return type");
        return;
    }
    const auto item_found = function_definition_items_.find(key);
    if (item_found == function_definition_items_.end() ||
        !syntax::is_valid(item_found->second) ||
        item_found->second.value >= module_.items.size()) {
        report(use_range, "function return type cannot be inferred");
        return;
    }
    analyze_function_body(module_.items[item_found->second.value]);
}

} // namespace aurex::sema
