#include <aurex/sema/sema_messages.hpp>

#include <string>
#include <vector>

#include <sema/internal/sema_control_expression_analyzer.hpp>

namespace aurex::sema {

namespace {

[[nodiscard]] base::SourceRange expr_range_or(
    const syntax::AstModule& module, const syntax::ExprId expr, const base::SourceRange& fallback) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size() ? module.exprs.range(expr.value) : fallback;
}

} // namespace

TypeHandle SemanticAnalyzerCore::ControlExpressionAnalyzer::analyze_try_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr)
{
    if (this->core_.state_.flow.in_const_initializer) {
        this->core_.report_general(expr.range, std::string(SEMA_TRY_CONST_INITIALIZER));
    }

    const TypeHandle source_type = this->core_.analyze_expr(expr.try_operand);
    const TryShape source_shape = this->core_.classify_try_shape(source_type);
    if (source_shape.kind == TryShape::Kind::malformed_result) {
        this->core_.report_general(expr.range, std::string(SEMA_TRY_RESULT_SHAPE));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (source_shape.kind == TryShape::Kind::malformed_option) {
        this->core_.report_general(expr.range, std::string(SEMA_TRY_OPTION_SHAPE));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    if (source_shape.kind == TryShape::Kind::result) {
        const TryShape return_shape =
            this->core_.classify_try_shape(this->core_.state_.flow.current_function_return_type);
        if (return_shape.kind != TryShape::Kind::result || return_shape.failure_case == nullptr) {
            this->core_.report_general(expr.range, std::string(SEMA_TRY_RESULT_RETURN));
            return this->core_.record_expr_type(expr_id, source_shape.success_case->payload_type);
        }
        if (!this->core_.state_.checked.types.same(
                return_shape.failure_case->payload_type, source_shape.failure_case->payload_type)) {
            this->core_.report_general(expr.range, std::string(SEMA_TRY_RESULT_ERR_PAYLOAD));
        }
        return this->core_.record_expr_type(expr_id, source_shape.success_case->payload_type);
    }

    if (source_shape.kind == TryShape::Kind::option) {
        const TryShape return_shape =
            this->core_.classify_try_shape(this->core_.state_.flow.current_function_return_type);
        if (return_shape.kind != TryShape::Kind::option) {
            this->core_.report_general(expr.range, std::string(SEMA_TRY_OPTION_RETURN));
            return this->core_.record_expr_type(expr_id, source_shape.success_case->payload_type);
        }
        return this->core_.record_expr_type(expr_id, source_shape.success_case->payload_type);
    }

    this->core_.report_general(expr.range, std::string(SEMA_TRY_SHAPE));
    return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
}

TypeHandle SemanticAnalyzerCore::ControlExpressionAnalyzer::analyze_if_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (this->core_.state_.flow.in_const_initializer) {
        this->core_.report_general(expr.range, std::string(SEMA_IF_EXPR_CONST_INITIALIZER));
    }
    const TypeHandle condition = this->core_.analyze_expr(expr.condition);
    if (!syntax::is_valid(expr.condition_pattern) && !this->core_.state_.checked.types.is_bool(condition)) {
        this->core_.report_general(expr_range_or(this->core_.ctx_.module, expr.condition, expr.range),
            std::string(SEMA_IF_EXPR_CONDITION_BOOL));
    }
    const auto analyze_then_branch = [&](const TypeHandle expected) {
        if (!syntax::is_valid(expr.condition_pattern)) {
            return this->core_.analyze_expr(expr.then_expr, expected);
        }
        std::vector<PatternBinding> bindings;
        static_cast<void>(this->core_.analyze_pattern(expr.condition_pattern, condition, bindings));
        this->core_.state_.names.symbols.push_scope(bindings.size());
        this->core_.define_pattern_bindings(bindings, false);
        const TypeHandle result = this->core_.analyze_expr(expr.then_expr, expected);
        this->core_.state_.names.symbols.pop_scope();
        return result;
    };

    TypeHandle then_type = INVALID_TYPE_HANDLE;
    TypeHandle else_type = INVALID_TYPE_HANDLE;
    if (!is_valid(expected_type) && this->core_.is_null_result_expr(expr.then_expr)
        && !this->core_.is_null_result_expr(expr.else_expr)) {
        else_type = this->core_.analyze_expr(expr.else_expr);
        then_type = analyze_then_branch(else_type);
    } else {
        then_type = analyze_then_branch(expected_type);
        else_type = this->core_.analyze_expr(expr.else_expr, is_valid(then_type) ? then_type : expected_type);
        if (!is_valid(then_type) && this->core_.state_.checked.types.is_pointer(else_type)
            && this->core_.is_null_result_expr(expr.then_expr)) {
            then_type = analyze_then_branch(else_type);
        }
        if (!is_valid(else_type) && this->core_.state_.checked.types.is_pointer(then_type)
            && this->core_.is_null_result_expr(expr.else_expr)) {
            else_type = this->core_.analyze_expr(expr.else_expr, then_type);
        }
    }
    if (!this->core_.state_.checked.types.same(then_type, else_type)) {
        this->core_.report_type_mismatch(expr.range, std::string(SEMA_IF_EXPR_BRANCH_TYPE), then_type, else_type);
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (is_valid(then_type) && this->core_.state_.checked.types.is_void(then_type)) {
        this->core_.report_general(expr.range, std::string(SEMA_IF_EXPR_VOID));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle then_intrinsic = this->core_.cached_expr_intrinsic_type(expr.then_expr);
    const TypeHandle else_intrinsic = this->core_.cached_expr_intrinsic_type(expr.else_expr);
    TypeHandle intrinsic_type = INVALID_TYPE_HANDLE;
    if (is_valid(then_intrinsic) && is_valid(else_intrinsic)
        && this->core_.state_.checked.types.same(then_intrinsic, else_intrinsic)) {
        intrinsic_type = then_intrinsic;
    } else if (!is_valid(expected_type)) {
        intrinsic_type = then_type;
    }
    return this->core_.record_expr_types(expr_id, intrinsic_type, then_type);
}

TypeHandle SemanticAnalyzerCore::ControlExpressionAnalyzer::analyze_block_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (this->core_.state_.flow.in_const_initializer) {
        this->core_.report_general(expr.range, std::string(SEMA_BLOCK_EXPR_CONST_INITIALIZER));
    }
    if (!syntax::is_valid(expr.block_result)) {
        this->core_.report_general(expr.range, std::string(SEMA_BLOCK_EXPR_FINAL));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    this->core_.state_.names.symbols.push_scope();
    this->core_.analyze_block_statements(expr.block, this->core_.state_.flow.current_function_return_type,
        this->core_.state_.flow.current_return_inference);
    if (!this->core_.block_may_fallthrough(expr.block)) {
        this->core_.report_general(expr.range, std::string(SEMA_BLOCK_EXPR_UNREACHABLE));
    }
    const TypeHandle result = this->core_.analyze_expr(expr.block_result, expected_type);
    this->core_.state_.names.symbols.pop_scope();

    if (!is_valid(result)) {
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (this->core_.state_.checked.types.is_void(result)) {
        this->core_.report_general(expr.range, std::string(SEMA_BLOCK_EXPR_VOID));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle intrinsic_type = this->core_.cached_expr_intrinsic_type(expr.block_result);
    return this->core_.record_expr_types(expr_id, is_valid(intrinsic_type) ? intrinsic_type : result, result);
}

TypeHandle SemanticAnalyzerCore::ControlExpressionAnalyzer::analyze_unsafe_block_expr(
    const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const TypeHandle expected_type)
{
    if (this->core_.state_.flow.in_const_initializer) {
        this->core_.report_general(expr.range, std::string(SEMA_UNSAFE_BLOCK_CONST_INITIALIZER));
    }

    this->core_.state_.names.symbols.push_scope();
    const int previous_unsafe_depth = this->core_.state_.flow.unsafe_context_depth;
    ++this->core_.state_.flow.unsafe_context_depth;
    this->core_.analyze_block_statements(expr.block, this->core_.state_.flow.current_function_return_type,
        this->core_.state_.flow.current_return_inference);
    TypeHandle result = this->core_.state_.checked.types.builtin(BuiltinType::void_);
    if (syntax::is_valid(expr.block_result)) {
        if (!this->core_.block_may_fallthrough(expr.block)) {
            this->core_.report_general(expr.range, std::string(SEMA_BLOCK_EXPR_UNREACHABLE));
        }
        result = this->core_.analyze_expr(expr.block_result, expected_type);
    }
    this->core_.state_.flow.unsafe_context_depth = previous_unsafe_depth;
    this->core_.state_.names.symbols.pop_scope();

    if (!is_valid(result)) {
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (syntax::is_valid(expr.block_result) && this->core_.state_.checked.types.is_void(result)) {
        this->core_.report_general(expr.range, std::string(SEMA_BLOCK_EXPR_VOID));
        return this->core_.record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    const TypeHandle intrinsic_type =
        syntax::is_valid(expr.block_result) ? this->core_.cached_expr_intrinsic_type(expr.block_result) : result;
    return this->core_.record_expr_types(expr_id, is_valid(intrinsic_type) ? intrinsic_type : result, result);
}

SemanticAnalyzerCore::ControlExpressionAnalyzer::ControlExpressionAnalyzer(SemanticAnalyzerCore& core) noexcept
    : core_(core)
{
}

SemanticAnalyzerCore::ControlExpressionAnalyzer SemanticAnalyzerCore::control_expression_analyzer() noexcept
{
    return ControlExpressionAnalyzer(*this);
}

TypeHandle SemanticAnalyzerCore::analyze_try_expr(const syntax::ExprId expr_id, const ExprView& expr)
{
    return this->control_expression_analyzer().analyze_try_expr(expr_id, expr);
}

TypeHandle SemanticAnalyzerCore::analyze_if_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->control_expression_analyzer().analyze_if_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_block_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->control_expression_analyzer().analyze_block_expr(expr_id, expr, expected_type);
}

TypeHandle SemanticAnalyzerCore::analyze_unsafe_block_expr(
    const syntax::ExprId expr_id, const ExprView& expr, const TypeHandle expected_type)
{
    return this->control_expression_analyzer().analyze_unsafe_block_expr(expr_id, expr, expected_type);
}

} // namespace aurex::sema
