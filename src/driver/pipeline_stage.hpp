#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace aurex::driver {

enum class PipelineStageId {
    incremental_cache_lookup,
    source_read,
    tokens_lex,
    tokens_dump,
    lossless_dump,
    module_read,
    module_lex,
    module_parse,
    module_append,
    ast_dump,
    modules_dump,
    sema_analyze,
    incremental_cache_write,
    checked_dump,
    ir_lower,
    ir_pass_pipeline,
    ir_dump,
    llvm_emit_ir,
    llvm_ir_dump,
    llvm_write_temp,
    native_clang,
};

inline constexpr std::size_t PIPELINE_STAGE_RECORD_COUNT = 21U;

struct PipelineStageRecord {
    PipelineStageId id;
    std::string_view name;
    std::string_view input;
    std::string_view output;
    std::string_view profile_name;
    std::string_view diagnostic_ownership;
    std::string_view cache_query_impact;
};

[[nodiscard]] std::span<const PipelineStageRecord> pipeline_stage_records() noexcept;
[[nodiscard]] const PipelineStageRecord& pipeline_stage_record(PipelineStageId id) noexcept;
[[nodiscard]] std::string_view pipeline_stage_profile_name(PipelineStageId id) noexcept;

} // namespace aurex::driver
