#include "frontend_pipeline.hpp"

#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/incremental_cache.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/pipeline_stage.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/syntax/ast_dump.hpp>
#include <aurex/syntax/lossless.hpp>

#include <iostream>
#include <span>
#include <string>
#include <utility>

namespace aurex::driver {

namespace {

[[nodiscard]] bool emit_kind_requires_ir_lowering(const EmitKind emit_kind) noexcept
{
    return emit_kind == EmitKind::ir || emit_kind == EmitKind::llvm_ir || emit_kind == EmitKind::assembly
        || emit_kind == EmitKind::object || emit_kind == EmitKind::executable;
}

[[nodiscard]] sema::SemanticOptions make_semantic_options(const EmitKind emit_kind) noexcept
{
    sema::SemanticOptions sema_options;
    sema_options.retain_generic_side_tables = emit_kind_requires_ir_lowering(emit_kind) || emit_kind == EmitKind::typed;
    return sema_options;
}

[[nodiscard]] base::Error remap_diagnostic_loader_error(const base::Error& error)
{
    return {
        error.code == base::ErrorCode::io_error ? base::ErrorCode::parse_error : error.code,
        error.message,
    };
}

} // namespace

FrontendPipeline::FrontendPipeline(CompilationSession& session) noexcept : session_(session)
{
}

base::Result<bool> FrontendPipeline::try_reuse_check_cache()
{
    if (this->session_.invocation().emit_kind != EmitKind::check) {
        return base::Result<bool>::ok(false);
    }
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::incremental_cache_lookup);
    return try_reuse_incremental_check_cache(this->session_.invocation(), this->session_.profiler());
}

base::Result<void> FrontendPipeline::emit_token_or_lossless_output()
{
    auto source_result = [&] {
        ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::source_read);
        return read_text_file(this->session_.invocation().input_path);
    }();
    if (!source_result) {
        return base::Result<void>::fail(source_result.error());
    }

    const base::SourceId source_id = this->session_.sources().add_source(
        this->session_.invocation().input_path.string(), source_result.take_value());
    lex::LexerOptions lexer_options;
    lexer_options.emit_trivia_tokens = this->session_.invocation().emit_kind == EmitKind::lossless;
    lex::Lexer lexer(source_id, this->session_.sources().text(source_id), this->session_.diagnostics(), lexer_options);
    auto token_result = [&] {
        ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::tokens_lex);
        return lexer.tokenize();
    }();
    if (!token_result) {
        this->session_.render_diagnostics_to_stderr();
        return base::Result<void>::fail(token_result.error());
    }

    const PipelineStageId dump_stage = this->session_.invocation().emit_kind == EmitKind::lossless
        ? PipelineStageId::lossless_dump
        : PipelineStageId::tokens_dump;
    ScopedCompilationPhase phase(this->session_.profiler(), dump_stage);
    if (this->session_.invocation().emit_kind == EmitKind::lossless) {
        const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(token_result.value().span());
        std::cout << syntax::dump_lossless_syntax_tree(tree);
        return base::Result<void>::ok();
    }

    std::cout << syntax::dump_tokens(token_result.value());
    return base::Result<void>::ok();
}

base::Result<FrontendModuleOutput> FrontendPipeline::load_modules()
{
    ModuleLoader loader(
        this->session_.invocation(), this->session_.sources(), this->session_.diagnostics(), this->session_.profiler());
    auto ast_result = loader.load_root();
    if (!ast_result) {
        return base::Result<FrontendModuleOutput>::fail(this->session_.diagnostics().diagnostics().empty()
                ? ast_result.error()
                : remap_diagnostic_loader_error(ast_result.error()));
    }

    auto module_records = loader.modules();
    FrontendModuleOutput frontend{
        ast_result.take_value(),
        std::vector<ModuleRecord>(module_records.begin(), module_records.end()),
    };
    return base::Result<FrontendModuleOutput>::ok(std::move(frontend));
}

base::Result<void> FrontendPipeline::dump_ast_output(const syntax::AstModule& ast)
{
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::ast_dump);
    std::cout << syntax::dump_ast(ast);
    return base::Result<void>::ok();
}

base::Result<void> FrontendPipeline::dump_module_graph_output(const std::vector<ModuleRecord>& modules)
{
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::modules_dump);
    std::cout << "modules\n";
    for (const ModuleRecord& record : modules) {
        std::cout << "  " << record.name << " " << record.path.string() << "\n";
    }
    return base::Result<void>::ok();
}

base::Result<sema::CheckedModule> FrontendPipeline::run_semantic_analysis(syntax::AstModule& ast)
{
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::sema_analyze);
    sema::SemanticAnalyzer analyzer(
        ast, this->session_.diagnostics(), make_semantic_options(this->session_.invocation().emit_kind));
    return analyzer.analyze();
}

base::Result<void> FrontendPipeline::write_checked_incremental_cache(
    const std::vector<ModuleRecord>& modules, const syntax::AstModule& ast, const sema::CheckedModule& checked)
{
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::incremental_cache_write);
    return write_incremental_cache(this->session_.invocation(), this->session_.sources(),
        std::span<const ModuleRecord>(modules.data(), modules.size()), ast, checked, this->session_.profiler());
}

} // namespace aurex::driver
