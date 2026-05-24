#include "pipeline_stage.hpp"

#include "incremental_cache/common.hpp"

namespace aurex::driver {
namespace {

constexpr std::array<PipelineStageRecord, PIPELINE_STAGE_RECORD_COUNT> PIPELINE_STAGE_RECORDS{{
    {
        PipelineStageId::incremental_cache_lookup,
        "incremental_cache_lookup",
        "CompilerInvocation + incremental cache file",
        "cache reuse decision",
        "incremental_cache.lookup",
        "driver cache reuse result",
        "reads incremental cache fingerprints and query rows",
    },
    {
        PipelineStageId::source_read,
        "source_read",
        "root source path",
        "source text",
        "source.read",
        "driver file IO result",
        "may read file cache",
    },
    {
        PipelineStageId::tokens_lex,
        "tokens_lex",
        "source text",
        "token buffer",
        "tokens.lex",
        "lexer diagnostic sink",
        "feeds token dump only",
    },
    {
        PipelineStageId::tokens_dump,
        "tokens_dump",
        "token buffer",
        "token dump text",
        "tokens.dump",
        "none",
        "none",
    },
    {
        PipelineStageId::lossless_dump,
        "lossless_dump",
        "token buffer with trivia",
        "lossless syntax dump text",
        "lossless.dump",
        "none",
        "none",
    },
    {
        PipelineStageId::module_read,
        "module_read",
        "module source path",
        "module source text",
        "module.read",
        "driver file IO result",
        "may read file cache",
    },
    {
        PipelineStageId::module_lex,
        "module_lex",
        "module source text",
        "module token buffer",
        "module.lex",
        "lexer diagnostic sink",
        "feeds parse and query records",
    },
    {
        PipelineStageId::module_parse,
        "module_parse",
        "module token buffer",
        "module AST",
        "module.parse",
        "parser diagnostic sink",
        "feeds module graph and query records",
    },
    {
        PipelineStageId::module_append,
        "module_append",
        "module AST",
        "combined AST module",
        "module.append",
        "module loader diagnostic sink",
        "feeds module graph records",
    },
    {
        PipelineStageId::ast_dump,
        "ast_dump",
        "combined AST module",
        "AST dump text",
        "ast.dump",
        "none",
        "none",
    },
    {
        PipelineStageId::modules_dump,
        "modules_dump",
        "loaded module records",
        "module graph dump text",
        "modules.dump",
        "none",
        "none",
    },
    {
        PipelineStageId::sema_analyze,
        "sema_analyze",
        "combined AST module",
        "checked module",
        "sema.analyze",
        "semantic diagnostic sink",
        "produces checked query subjects",
    },
    {
        PipelineStageId::incremental_cache_write,
        "incremental_cache_write",
        "sources + modules + AST + checked module",
        "incremental cache file",
        "incremental_cache.write",
        "driver cache write result",
        "writes source fingerprints, module rows, and query rows",
    },
    {
        PipelineStageId::checked_dump,
        "checked_dump",
        "checked module",
        "checked dump text",
        "checked.dump",
        "none",
        "none",
    },
    {
        PipelineStageId::ir_lower,
        "ir_lower",
        "AST + checked module",
        "Aurex IR module",
        "ir.lower",
        "IR lowering result",
        "feeds lower_function_ir query subjects",
    },
    {
        PipelineStageId::ir_pass_pipeline,
        "ir_pass_pipeline",
        "Aurex IR module",
        "optimized Aurex IR module",
        "ir.pass_pipeline",
        "IR verifier/pass result",
        "invalidates downstream backend output",
    },
    {
        PipelineStageId::ir_dump,
        "ir_dump",
        "Aurex IR module",
        "Aurex IR dump text",
        "ir.dump",
        "none",
        "none",
    },
    {
        PipelineStageId::llvm_emit_ir,
        "llvm_emit_ir",
        "Aurex IR module",
        "LLVM IR text",
        "llvm.emit_ir",
        "LLVM backend result",
        "consumes optimized IR",
    },
    {
        PipelineStageId::llvm_ir_dump,
        "llvm_ir_dump",
        "LLVM IR text",
        "LLVM IR dump text",
        "llvm_ir.dump",
        "none",
        "none",
    },
    {
        PipelineStageId::llvm_write_temp,
        "llvm_write_temp",
        "LLVM IR text",
        "temporary LLVM IR file",
        "llvm.write_temp",
        "driver file IO result",
        "none",
    },
    {
        PipelineStageId::native_clang,
        "native_clang",
        "temporary LLVM IR file",
        "native artifact",
        "native.clang",
        "clang invocation result",
        "consumes optimized IR through LLVM IR",
    },
}};

constexpr std::array<PipelineProfileSubeventRecord, PIPELINE_PROFILE_SUBEVENT_RECORD_COUNT>
    PIPELINE_PROFILE_SUBEVENT_RECORDS{{
        {
            PipelineProfileSubeventId::incremental_cache_source_stage_reuse,
            incremental_cache_detail::INCREMENTAL_CACHE_PROFILE_SOURCE_STAGE_REUSE,
            PipelineStageId::incremental_cache_lookup,
        },
        {
            PipelineProfileSubeventId::incremental_cache_query_diff,
            incremental_cache_detail::INCREMENTAL_CACHE_PROFILE_QUERY_DIFF,
            PipelineStageId::incremental_cache_write,
        },
        {
            PipelineProfileSubeventId::incremental_cache_query_plan,
            incremental_cache_detail::INCREMENTAL_CACHE_PROFILE_QUERY_PLAN,
            PipelineStageId::incremental_cache_write,
        },
        {
            PipelineProfileSubeventId::incremental_cache_query_pruning,
            incremental_cache_detail::INCREMENTAL_CACHE_PROFILE_QUERY_PRUNING,
            PipelineStageId::incremental_cache_write,
        },
        {
            PipelineProfileSubeventId::incremental_cache_query_provider_eval,
            incremental_cache_detail::INCREMENTAL_CACHE_PROFILE_QUERY_PROVIDER_EVAL,
            PipelineStageId::incremental_cache_write,
        },
    }};

} // namespace

std::span<const PipelineStageRecord> pipeline_stage_records() noexcept
{
    return std::span<const PipelineStageRecord>(PIPELINE_STAGE_RECORDS.data(), PIPELINE_STAGE_RECORDS.size());
}

std::span<const PipelineProfileSubeventRecord> pipeline_profile_subevent_records() noexcept
{
    return std::span<const PipelineProfileSubeventRecord>(
        PIPELINE_PROFILE_SUBEVENT_RECORDS.data(), PIPELINE_PROFILE_SUBEVENT_RECORDS.size());
}

const PipelineStageRecord& pipeline_stage_record(const PipelineStageId id) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    if (index < PIPELINE_STAGE_RECORDS.size()) {
        return PIPELINE_STAGE_RECORDS[index];
    }
    return PIPELINE_STAGE_RECORDS.front();
}

const PipelineStageRecord* pipeline_stage_record_for_profile_name(const std::string_view profile_name) noexcept
{
    for (const PipelineStageRecord& record : PIPELINE_STAGE_RECORDS) {
        if (record.profile_name == profile_name) {
            return &record;
        }
    }
    return nullptr;
}

const PipelineProfileSubeventRecord* pipeline_profile_subevent_record_for_profile_name(
    const std::string_view profile_name) noexcept
{
    for (const PipelineProfileSubeventRecord& record : PIPELINE_PROFILE_SUBEVENT_RECORDS) {
        if (record.profile_name == profile_name) {
            return &record;
        }
    }
    return nullptr;
}

std::string_view pipeline_stage_profile_name(const PipelineStageId id) noexcept
{
    return pipeline_stage_record(id).profile_name;
}

} // namespace aurex::driver
