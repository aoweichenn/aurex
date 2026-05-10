#pragma once

#include <aurex/base/integer.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::base {

struct SourceId {
    u32 value = 0;
};

struct SourceRange {
    SourceId source {};
    usize begin = 0;
    usize end = 0;

    [[nodiscard]] usize length() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
};

class SourceFile {
public:
    SourceFile(SourceId id, std::string path, std::string text);

    [[nodiscard]] SourceId id() const noexcept;
    [[nodiscard]] std::string_view path() const noexcept;
    [[nodiscard]] std::string_view text() const noexcept;

private:
    SourceId id_;
    std::string path_;
    std::string text_;
};

class SourceManager {
public:
    [[nodiscard]] SourceId add_source(std::string path, std::string text);
    [[nodiscard]] const SourceFile& get(SourceId id) const noexcept;
    [[nodiscard]] std::string_view text(SourceId id) const noexcept;

private:
    std::vector<SourceFile> files_;
};

} // namespace aurex::base
