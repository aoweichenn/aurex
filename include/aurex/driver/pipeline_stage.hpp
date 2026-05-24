#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace aurex::base {

enum class DiagnosticCategory;

} // namespace aurex::base

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
    count,
};

enum class PipelineProfileSubeventId {
    incremental_cache_source_stage_reuse,
    incremental_cache_query_diff,
    incremental_cache_query_plan,
    incremental_cache_query_pruning,
    incremental_cache_query_provider_eval,
    count,
};

inline constexpr std::size_t PIPELINE_STAGE_RECORD_COUNT = static_cast<std::size_t>(PipelineStageId::count);
inline constexpr std::size_t PIPELINE_PROFILE_SUBEVENT_RECORD_COUNT =
    static_cast<std::size_t>(PipelineProfileSubeventId::count);

struct PipelineStageRecord {
    PipelineStageId id;
    std::string_view name;
    std::string_view input;
    std::string_view output;
    std::string_view profile_name;
    std::string_view diagnostic_ownership;
    std::string_view cache_query_impact;
};

struct PipelineStageMetadata {
    std::string_view id;
    std::string_view profile_name;
    std::string_view input;
    std::string_view output;
    std::string_view diagnostic_ownership;
    std::string_view cache_query_impact;
};

struct PipelineProfileSubeventRecord {
    PipelineProfileSubeventId id;
    std::string_view profile_name;
    PipelineStageId parent_stage;
};

[[nodiscard]] std::span<const PipelineStageRecord> pipeline_stage_records() noexcept;
[[nodiscard]] std::span<const PipelineProfileSubeventRecord> pipeline_profile_subevent_records() noexcept;
[[nodiscard]] const PipelineStageRecord& pipeline_stage_record(PipelineStageId id) noexcept;
[[nodiscard]] const PipelineProfileSubeventRecord& pipeline_profile_subevent_record(
    PipelineProfileSubeventId id) noexcept;
[[nodiscard]] PipelineStageMetadata pipeline_stage_metadata(const PipelineStageRecord& record) noexcept;
[[nodiscard]] PipelineStageMetadata pipeline_stage_metadata(PipelineStageId id) noexcept;
[[nodiscard]] const PipelineStageRecord* pipeline_stage_record_for_profile_name(std::string_view profile_name) noexcept;
[[nodiscard]] std::span<const PipelineStageId> pipeline_stage_ids_for_diagnostic_category(
    base::DiagnosticCategory category) noexcept;
[[nodiscard]] const PipelineProfileSubeventRecord* pipeline_profile_subevent_record_for_profile_name(
    std::string_view profile_name) noexcept;
[[nodiscard]] std::string_view pipeline_stage_profile_name(PipelineStageId id) noexcept;
[[nodiscard]] std::string_view pipeline_profile_subevent_profile_name(PipelineProfileSubeventId id) noexcept;

} // namespace aurex::driver
