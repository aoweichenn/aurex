#include <aurex/application/driver/profile.hpp>
#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/pipeline/stage.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <string_view>
#include <sys/resource.h>
#include <system_error>

namespace aurex::driver {

namespace {

constexpr base::usize PROFILE_INITIAL_PHASE_CAPACITY = 64;
constexpr double PROFILE_MILLISECONDS_PER_SECOND = 1000.0;
#if defined(__APPLE__)
constexpr double PROFILE_BYTES_PER_MIB = 1024.0 * 1024.0;
#else
constexpr double PROFILE_KIB_PER_MIB = 1024.0;
#endif
constexpr int PROFILE_JSON_PRECISION = 3;
constexpr char PROFILE_JSON_QUOTE = '"';
constexpr char PROFILE_JSON_BACKSLASH = '\\';
constexpr char PROFILE_JSON_NEWLINE = '\n';
constexpr char PROFILE_JSON_CARRIAGE_RETURN = '\r';
constexpr char PROFILE_JSON_TAB = '\t';
constexpr unsigned int PROFILE_JSON_CONTROL_CHAR_LIMIT = 0x20U;
constexpr unsigned int PROFILE_JSON_NIBBLE_BITS = 4U;
constexpr unsigned int PROFILE_JSON_LOW_NIBBLE_MASK = 0x0fU;
constexpr char PROFILE_JSON_HEX_DIGITS[] = "0123456789abcdef";
constexpr std::string_view PROFILE_OUTPUT_CREATE_DIR_FAILED = "failed to create profile output directory";
constexpr std::string_view PROFILE_OUTPUT_OPEN_FAILED = "failed to open profile output file";
constexpr std::string_view PROFILE_OUTPUT_WRITE_FAILED = "failed to write profile output file";
constexpr std::string_view PROFILE_FORMAT = "aurex-profile-v1";

enum class ProfileStageMetadataProfileField {
    omit,
    include,
};

[[nodiscard]] double current_rss_mib() noexcept
{
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0.0;
    }
#if defined(__APPLE__)
    return static_cast<double>(usage.ru_maxrss) / PROFILE_BYTES_PER_MIB;
#else
    return static_cast<double>(usage.ru_maxrss) / PROFILE_KIB_PER_MIB;
#endif
}

[[nodiscard]] double elapsed_ms(const std::chrono::steady_clock::duration elapsed) noexcept
{
    return std::chrono::duration<double>(elapsed).count() * PROFILE_MILLISECONDS_PER_SECOND;
}

void write_json_escaped(std::ostream& out, const std::string_view text)
{
    out << PROFILE_JSON_QUOTE;
    for (const char raw_byte : text) {
        const auto byte = static_cast<unsigned int>(static_cast<unsigned char>(raw_byte));
        switch (byte) {
            case PROFILE_JSON_QUOTE:
                out << "\\\"";
                break;
            case PROFILE_JSON_BACKSLASH:
                out << "\\\\";
                break;
            case PROFILE_JSON_NEWLINE:
                out << "\\n";
                break;
            case PROFILE_JSON_CARRIAGE_RETURN:
                out << "\\r";
                break;
            case PROFILE_JSON_TAB:
                out << "\\t";
                break;
            default:
                if (byte < PROFILE_JSON_CONTROL_CHAR_LIMIT) {
                    out << "\\u00"
                        << PROFILE_JSON_HEX_DIGITS[(byte >> PROFILE_JSON_NIBBLE_BITS) & PROFILE_JSON_LOW_NIBBLE_MASK]
                        << PROFILE_JSON_HEX_DIGITS[byte & PROFILE_JSON_LOW_NIBBLE_MASK];
                } else {
                    out << static_cast<char>(byte);
                }
                break;
        }
    }
    out << PROFILE_JSON_QUOTE;
}

void write_stage_metadata(std::ostream& output, const std::string_view field_name, const PipelineStageMetadata metadata,
    const ProfileStageMetadataProfileField profile_field)
{
    output << "      \"";
    output << field_name;
    output << "\": {\n";
    output << "        \"id\": ";
    write_json_escaped(output, metadata.id);
    output << ",\n";
    if (profile_field == ProfileStageMetadataProfileField::include) {
        output << "        \"profile\": ";
        write_json_escaped(output, metadata.profile_name);
        output << ",\n";
    }
    output << "        \"input\": ";
    write_json_escaped(output, metadata.input);
    output << ",\n";
    output << "        \"output\": ";
    write_json_escaped(output, metadata.output);
    output << ",\n";
    output << "        \"diagnostic_ownership\": ";
    write_json_escaped(output, metadata.diagnostic_ownership);
    output << ",\n";
    output << "        \"cache_query_impact\": ";
    write_json_escaped(output, metadata.cache_query_impact);
    output << "\n";
    output << "      },\n";
}

void write_profile_phase_stage_metadata(std::ostream& output, const PipelineProfilePhaseClassification classification)
{
    switch (classification.kind) {
        case PipelineProfilePhaseKind::driver_stage:
            if (classification.stage != nullptr) {
                write_stage_metadata(output, "stage", pipeline_stage_metadata(*classification.stage),
                    ProfileStageMetadataProfileField::omit);
            }
            break;
        case PipelineProfilePhaseKind::profile_subevent:
            if (classification.parent_stage != nullptr) {
                write_stage_metadata(output, "parent_stage", pipeline_stage_metadata(*classification.parent_stage),
                    ProfileStageMetadataProfileField::include);
            }
            break;
        case PipelineProfilePhaseKind::unknown:
            break;
    }
}

[[nodiscard]] double total_elapsed_ms(const std::span<const CompilationPhaseProfile> phases) noexcept
{
    double total = 0.0;
    for (const CompilationPhaseProfile& phase : phases) {
        total += phase.elapsed_ms;
    }
    return total;
}

[[nodiscard]] double max_rss_mib_after(const std::span<const CompilationPhaseProfile> phases) noexcept
{
    double max_rss = 0.0;
    for (const CompilationPhaseProfile& phase : phases) {
        max_rss = std::max(max_rss, phase.rss_mib_after);
    }
    return max_rss;
}

} // namespace

CompilationProfiler::CompilationProfiler(const bool enabled)
    : enabled_(enabled), last_rss_mib_(enabled ? current_rss_mib() : 0.0)
{
    if (this->enabled_) {
        this->phases_.reserve(PROFILE_INITIAL_PHASE_CAPACITY);
    }
}

bool CompilationProfiler::enabled() const noexcept
{
    return this->enabled_;
}

void CompilationProfiler::record(
    const std::string_view name, const std::string_view detail, const std::chrono::steady_clock::duration elapsed)
{
    if (!this->enabled_) {
        return;
    }
    const double rss_mib = current_rss_mib();
    this->phases_.push_back(CompilationPhaseProfile{
        std::string(name),
        std::string(detail),
        elapsed_ms(elapsed),
        rss_mib,
        rss_mib - this->last_rss_mib_,
    });
    this->last_rss_mib_ = rss_mib;
}

void CompilationProfiler::record(const std::string_view name, const std::chrono::steady_clock::duration elapsed)
{
    this->record(name, {}, elapsed);
}

void CompilationProfiler::record(
    const PipelineStageId stage, const std::string_view detail, const std::chrono::steady_clock::duration elapsed)
{
    this->record(pipeline_stage_profile_name(stage), detail, elapsed);
}

void CompilationProfiler::record(const PipelineStageId stage, const std::chrono::steady_clock::duration elapsed)
{
    this->record(stage, {}, elapsed);
}

void CompilationProfiler::record(const PipelineProfileSubeventId subevent, const std::string_view detail,
    const std::chrono::steady_clock::duration elapsed)
{
    this->record(pipeline_profile_subevent_profile_name(subevent), detail, elapsed);
}

void CompilationProfiler::record(
    const PipelineProfileSubeventId subevent, const std::chrono::steady_clock::duration elapsed)
{
    this->record(subevent, {}, elapsed);
}

std::span<const CompilationPhaseProfile> CompilationProfiler::phases() const noexcept
{
    return this->phases_;
}

base::Result<void> CompilationProfiler::write_json(const std::filesystem::path& path) const
{
    if (!this->enabled_) {
        return base::Result<void>::ok();
    }
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            return base::Result<void>::fail({base::ErrorCode::io_error, std::string(PROFILE_OUTPUT_CREATE_DIR_FAILED)});
        }
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return base::Result<void>::fail({base::ErrorCode::io_error, std::string(PROFILE_OUTPUT_OPEN_FAILED)});
    }

    output << std::fixed << std::setprecision(PROFILE_JSON_PRECISION);
    output << "{\n";
    output << "  \"format\": ";
    write_json_escaped(output, PROFILE_FORMAT);
    output << ",\n";
    output << "  \"totals\": {\n";
    output << "    \"elapsed_ms\": " << total_elapsed_ms(this->phases_) << ",\n";
    output << "    \"max_rss_mib_after\": " << max_rss_mib_after(this->phases_) << "\n";
    output << "  },\n";
    output << "  \"phases\": [\n";
    for (base::usize i = 0; i < this->phases_.size(); ++i) {
        const CompilationPhaseProfile& phase = this->phases_[i];
        output << "    {\n";
        output << "      \"name\": ";
        write_json_escaped(output, phase.name);
        output << ",\n";
        write_profile_phase_stage_metadata(output, pipeline_profile_phase_classification(phase.name));
        output << "      \"detail\": ";
        write_json_escaped(output, phase.detail);
        output << ",\n";
        output << "      \"elapsed_ms\": " << phase.elapsed_ms << ",\n";
        output << "      \"rss_mib_after\": " << phase.rss_mib_after << ",\n";
        output << "      \"rss_delta_mib\": " << phase.rss_delta_mib << "\n";
        output << "    }";
        if (i + 1 < this->phases_.size()) {
            output << ',';
        }
        output << "\n";
    }
    output << "  ]\n";
    output << "}\n";

    if (!output) {
        return base::Result<void>::fail({base::ErrorCode::io_error, std::string(PROFILE_OUTPUT_WRITE_FAILED)});
    }
    return base::Result<void>::ok();
}

ScopedCompilationPhase::ScopedCompilationPhase(
    CompilationProfiler* const profiler, const std::string_view name, const std::string_view detail) noexcept
    : profiler_(profiler), name_(name), detail_(detail), started_(std::chrono::steady_clock::now())
{
}

ScopedCompilationPhase::ScopedCompilationPhase(
    CompilationProfiler* const profiler, const PipelineStageId stage, const std::string_view detail) noexcept
    : ScopedCompilationPhase(profiler, pipeline_stage_profile_name(stage), detail)
{
}

ScopedCompilationPhase::~ScopedCompilationPhase()
{
    if (this->profiler_ == nullptr || !this->profiler_->enabled()) {
        return;
    }
    this->profiler_->record(this->name_, this->detail_, std::chrono::steady_clock::now() - this->started_);
}

} // namespace aurex::driver
