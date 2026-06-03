#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/base/text.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::base {

struct SourceId {
    u32 value = 0;
};

struct SourceRange {
    SourceId source{};
    usize begin = 0;
    usize end = 0;

    [[nodiscard]] bool well_formed() const noexcept;
    [[nodiscard]] usize length() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
};

struct SourceLineExtent {
    usize begin = 0;
    usize end = 0;
};

class SourceFile {
public:
    SourceFile(SourceId id, std::string path, std::string text);

    [[nodiscard]] SourceId id() const noexcept;
    [[nodiscard]] std::string_view path() const noexcept;
    [[nodiscard]] std::string_view text() const noexcept;
    [[nodiscard]] LineColumn line_column(usize offset) const noexcept;
    [[nodiscard]] SourceLineExtent line_extent(usize offset) const noexcept;

private:
    [[nodiscard]] usize line_index(usize offset) const noexcept;

    SourceId id_;
    std::string path_;
    std::string text_;
    std::vector<usize> line_starts_;
};

class SourceManager {
public:
    [[nodiscard]] SourceId add_source(std::string path, std::string text);
    [[nodiscard]] const SourceFile& get(SourceId id) const noexcept;
    [[nodiscard]] const SourceFile* try_get(SourceId id) const noexcept;
    [[nodiscard]] std::span<const SourceFile> files() const noexcept;
    [[nodiscard]] std::string_view text(SourceId id) const noexcept;

private:
    std::vector<SourceFile> files_;
};

} // namespace aurex::base
