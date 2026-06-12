#include <aurex/application/driver/file_cache.hpp>
#include <aurex/application/driver/incremental_cache.hpp>
#include <aurex/application/driver/invocation.hpp>
#include <aurex/application/driver/module_loader.hpp>
#include <aurex/application/driver/profile.hpp>
#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/frontend/syntax/core/ast_dump.hpp>
#include <aurex/frontend/syntax/core/lossless.hpp>
#include <aurex/infrastructure/pipeline/stage.hpp>

#include <algorithm>
#include <iostream>
#include <span>
#include <string>
#include <utility>

#include <application/driver/pipeline/private/frontend_pipeline.hpp>

namespace aurex::driver {

namespace {

[[nodiscard]] bool emit_kind_requires_ir_lowering(const EmitKind emit_kind) noexcept
{
    return emit_kind == EmitKind::ir || emit_kind == EmitKind::llvm_ir || emit_kind == EmitKind::assembly
        || emit_kind == EmitKind::object || emit_kind == EmitKind::executable;
}

[[nodiscard]] std::vector<std::vector<query::ModulePartKey>> make_module_part_key_table(
    const std::span<const ModuleRecord> modules)
{
    std::vector<std::vector<query::ModulePartKey>> module_part_keys;
    module_part_keys.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        std::vector<query::ModulePartKey> part_keys;
        if (!module.parts.empty()) {
            base::u32 max_part_index = 0;
            for (const ModulePartRecord& part : module.parts) {
                max_part_index = std::max(max_part_index, part.stable_index);
            }
            part_keys.resize(static_cast<base::usize>(max_part_index) + 1U);
            for (const ModulePartRecord& part : module.parts) {
                if (query::is_valid(part.key)) {
                    part_keys[part.stable_index] = part.key;
                }
            }
        }
        module_part_keys.push_back(std::move(part_keys));
    }
    return module_part_keys;
}

[[nodiscard]] sema::SemanticOptions make_semantic_options(
    const EmitKind emit_kind, const std::span<const ModuleRecord> modules)
{
    sema::SemanticOptions sema_options;
    sema_options.retain_generic_side_tables = emit_kind_requires_ir_lowering(emit_kind) || emit_kind == EmitKind::typed;
    sema_options.retain_body_flow_graphs = emit_kind == EmitKind::checked || emit_kind == EmitKind::typed;
    sema_options.module_packages.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        sema_options.module_packages.push_back(module.package);
    }
    sema_options.module_part_keys = make_module_part_key_table(modules);
    return sema_options;
}

[[nodiscard]] base::Error remap_diagnostic_loader_error(const base::Error& error)
{
    return {
        error.code == base::ErrorCode::io_error ? base::ErrorCode::parse_error : error.code,
        error.message,
    };
}

[[nodiscard]] std::string_view module_part_record_kind_name(const ModulePartRecordKind kind) noexcept
{
    switch (kind) {
        case ModulePartRecordKind::primary:
            return "primary";
        case ModulePartRecordKind::named:
            return "fragment";
    }
    return "unknown";
}

[[nodiscard]] std::string_view module_part_record_display_name(const ModulePartRecord& part) noexcept
{
    if (part.kind == ModulePartRecordKind::primary) {
        return "<primary>";
    }
    return part.name;
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
    std::vector<ModuleRecord> modules(module_records.begin(), module_records.end());
    std::vector<std::vector<query::ModulePartKey>> module_part_keys =
        make_module_part_key_table(std::span<const ModuleRecord>(modules));
    syntax::AstModule ast = ast_result.take_value();
    auto expansion_result = [&] {
        ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::early_item_macro_expand);
        return frontend::macro::expand_early_item_macros_noop(
            ast, std::span<const std::vector<query::ModulePartKey>>(module_part_keys));
    }();
    if (!expansion_result) {
        return base::Result<FrontendModuleOutput>::fail(expansion_result.error());
    }

    FrontendModuleOutput frontend{
        std::move(ast),
        std::move(modules),
        expansion_result.take_value(),
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
        for (const ModulePartRecord& part : record.parts) {
            std::cout << "    part " << module_part_record_display_name(part) << " "
                      << module_part_record_kind_name(part.kind) << " index=" << part.stable_index
                      << " path=" << part.path.string();
            if (query::is_valid(part.key)) {
                std::cout << " key=" << part.key.global_id;
            }
            std::cout << "\n";
        }
    }
    return base::Result<void>::ok();
}

base::Result<sema::CheckedModule> FrontendPipeline::run_semantic_analysis(
    syntax::AstModule& ast, const std::vector<ModuleRecord>& modules)
{
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::sema_analyze);
    sema::SemanticAnalyzer analyzer(ast, this->session_.diagnostics(),
        make_semantic_options(this->session_.invocation().emit_kind, std::span<const ModuleRecord>(modules)));
    return analyzer.analyze();
}

base::Result<void> FrontendPipeline::write_checked_incremental_cache(const std::vector<ModuleRecord>& modules,
    const syntax::AstModule& ast, const sema::CheckedModule& checked, const ir::Module* const lowered_ir)
{
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::incremental_cache_write);
    if (lowered_ir != nullptr) {
        return write_incremental_cache(this->session_.invocation(), this->session_.sources(),
            std::span<const ModuleRecord>(modules.data(), modules.size()), ast, checked, *lowered_ir,
            this->session_.profiler());
    }
    return write_incremental_cache(this->session_.invocation(), this->session_.sources(),
        std::span<const ModuleRecord>(modules.data(), modules.size()), ast, checked, this->session_.profiler());
}

} // namespace aurex::driver
