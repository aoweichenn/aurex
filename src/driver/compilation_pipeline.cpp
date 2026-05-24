#include "compilation_pipeline.hpp"

#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/sema/checked_module.hpp>

#include <string>

#include "backend_pipeline.hpp"
#include "compilation_session.hpp"
#include "frontend_pipeline.hpp"
#include "lowering_pipeline.hpp"

namespace aurex::driver {

namespace {

[[nodiscard]] base::Result<ir::Module> lower_or_fail(
    LoweringPipeline& lowering_pipeline, const syntax::AstModule& ast, const sema::CheckedModule& checked)
{
    auto ir_result = lowering_pipeline.lower_and_optimize(ast, checked);
    if (!ir_result) {
        return base::Result<ir::Module>::fail(ir_result.error());
    }
    return base::Result<ir::Module>::ok(ir_result.take_value());
}

} // namespace

CompilationPipeline::CompilationPipeline(
    const CompilerInvocation& invocation, const LlvmIrEmitter llvm_ir_emitter) noexcept
    : invocation_(invocation), llvm_ir_emitter_(llvm_ir_emitter)
{
}

base::Result<void> CompilationPipeline::run()
{
    CompilationSession session(this->invocation_, this->llvm_ir_emitter_);
    FrontendPipeline frontend_pipeline(session);
    LoweringPipeline lowering_pipeline(session);
    BackendPipeline backend_pipeline(session);

    auto cache_result = frontend_pipeline.try_reuse_check_cache();
    if (!cache_result) {
        return session.finish(base::Result<void>::fail(cache_result.error()));
    }
    if (cache_result.value()) {
        return session.finish(base::Result<void>::ok());
    }

    if (this->invocation_.emit_kind == EmitKind::tokens || this->invocation_.emit_kind == EmitKind::lossless) {
        return session.finish(frontend_pipeline.emit_token_or_lossless_output());
    }

    auto frontend_result = frontend_pipeline.load_modules();
    if (!frontend_result) {
        session.render_diagnostics_to_stderr();
        return session.finish(base::Result<void>::fail(frontend_result.error()));
    }
    FrontendModuleOutput frontend = frontend_result.take_value();

    if (this->invocation_.emit_kind == EmitKind::ast) {
        return session.finish(frontend_pipeline.dump_ast_output(frontend.ast));
    }

    if (this->invocation_.emit_kind == EmitKind::modules) {
        return session.finish(frontend_pipeline.dump_module_graph_output(frontend.modules));
    }

    auto checked_result = frontend_pipeline.run_semantic_analysis(frontend.ast);
    if (!checked_result) {
        session.render_diagnostics_to_stderr();
        return session.finish(base::Result<void>::fail(checked_result.error()));
    }
    sema::CheckedModule checked = checked_result.take_value();

    auto incremental_cache_result =
        frontend_pipeline.write_checked_incremental_cache(frontend.modules, frontend.ast, checked);
    if (!incremental_cache_result) {
        return session.finish(base::Result<void>::fail(incremental_cache_result.error()));
    }

    if (this->invocation_.emit_kind == EmitKind::check) {
        return session.finish(base::Result<void>::ok());
    }

    if (this->invocation_.emit_kind == EmitKind::typed) {
        return session.finish(base::Result<void>::ok());
    }

    if (this->invocation_.emit_kind == EmitKind::checked) {
        return session.finish(lowering_pipeline.dump_checked_output(checked));
    }

    if (this->invocation_.emit_kind == EmitKind::ir) {
        auto ir_result = lower_or_fail(lowering_pipeline, frontend.ast, checked);
        if (!ir_result) {
            return session.finish(base::Result<void>::fail(ir_result.error()));
        }
        return session.finish(lowering_pipeline.dump_ir_output(ir_result.value()));
    }

    if (this->invocation_.emit_kind == EmitKind::llvm_ir) {
        auto ir_result = lower_or_fail(lowering_pipeline, frontend.ast, checked);
        if (!ir_result) {
            return session.finish(base::Result<void>::fail(ir_result.error()));
        }
        return session.finish(backend_pipeline.emit_llvm_ir_output(ir_result.value()));
    }

    if (backend_pipeline.can_emit_native_artifact()) {
        const auto validation_result = backend_pipeline.validate_native_output_request();
        if (!validation_result) {
            return session.finish(base::Result<void>::fail(validation_result.error()));
        }
        auto ir_result = lower_or_fail(lowering_pipeline, frontend.ast, checked);
        if (!ir_result) {
            return session.finish(base::Result<void>::fail(ir_result.error()));
        }
        return session.finish(backend_pipeline.emit_native_output(ir_result.value()));
    }

    return session.finish(
        base::Result<void>::fail({base::ErrorCode::codegen_error, std::string(DRIVER_UNSUPPORTED_EMISSION_MODE)}));
}

} // namespace aurex::driver
