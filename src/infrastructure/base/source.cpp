#include <aurex/infrastructure/base/source.hpp>

#include <algorithm>
#include <cassert>
#include <utility>

namespace aurex::base {

namespace {

constexpr char SOURCE_NEWLINE_CHAR = '\n';
constexpr char SOURCE_CARRIAGE_RETURN_CHAR = '\r';
constexpr usize SOURCE_FIRST_LINE = 1;
constexpr usize SOURCE_FIRST_COLUMN = 1;
constexpr std::string_view SOURCE_MANAGER_ID_CONTEXT = "source manager source id";

[[nodiscard]] std::vector<usize> build_line_starts(const std::string_view text)
{
    std::vector<usize> starts;
    starts.push_back(0);
    for (usize i = 0; i < text.size(); ++i) {
        if (text[i] == SOURCE_NEWLINE_CHAR) {
            starts.push_back(i + 1);
        }
    }
    return starts;
}

} // namespace

bool SourceRange::well_formed() const noexcept
{
    return this->begin <= this->end;
}

usize SourceRange::length() const noexcept
{
    return this->well_formed() ? this->end - this->begin : 0;
}

bool SourceRange::empty() const noexcept
{
    return this->well_formed() && this->begin == this->end;
}

SourceFile::SourceFile(const SourceId id, std::string path, std::string text)
    : id_(id), path_(std::move(path)), text_(std::move(text)), line_starts_(build_line_starts(this->text_))
{
}

SourceId SourceFile::id() const noexcept
{
    return this->id_;
}

std::string_view SourceFile::path() const noexcept
{
    return this->path_;
}

std::string_view SourceFile::text() const noexcept
{
    return this->text_;
}

LineColumn SourceFile::line_column(const usize offset) const noexcept
{
    const usize index = this->line_index(offset);
    const usize clamped = std::min(offset, this->text_.size());
    return LineColumn{
        index + SOURCE_FIRST_LINE,
        clamped - this->line_starts_[index] + SOURCE_FIRST_COLUMN,
    };
}

SourceLineExtent SourceFile::line_extent(const usize offset) const noexcept
{
    const usize index = this->line_index(offset);
    const usize begin = this->line_starts_[index];
    usize end = index + 1 < this->line_starts_.size() ? this->line_starts_[index + 1] : this->text_.size();
    if (end > begin && this->text_[end - 1] == SOURCE_NEWLINE_CHAR) {
        --end;
    }
    if (end > begin && this->text_[end - 1] == SOURCE_CARRIAGE_RETURN_CHAR) {
        --end;
    }
    return SourceLineExtent{begin, end};
}

usize SourceFile::line_index(const usize offset) const noexcept
{
    const usize clamped = std::min(offset, this->text_.size());
    const auto found = std::upper_bound(this->line_starts_.begin(), this->line_starts_.end(), clamped);
    return static_cast<usize>((found - this->line_starts_.begin()) - 1);
}

SourceId SourceManager::add_source(std::string path, std::string text)
{
    const SourceId id{checked_u32(this->files_.size(), SOURCE_MANAGER_ID_CONTEXT)};
    this->files_.emplace_back(id, std::move(path), std::move(text));
    return id;
}

const SourceFile& SourceManager::get(const SourceId id) const noexcept
{
    assert(id.value < this->files_.size());
    return this->files_[id.value];
}

const SourceFile* SourceManager::try_get(const SourceId id) const noexcept
{
    return id.value < this->files_.size() ? &this->files_[id.value] : nullptr;
}

std::span<const SourceFile> SourceManager::files() const noexcept
{
    return this->files_;
}

std::string_view SourceManager::text(const SourceId id) const noexcept
{
    const SourceFile* const file = this->try_get(id);
    return file == nullptr ? std::string_view{} : file->text();
}

} // namespace aurex::base
