#include <aurex/infrastructure/base/text.hpp>

namespace aurex::base {

namespace {

constexpr char TEXT_NEWLINE_CHAR = '\n';
constexpr usize TEXT_FIRST_LINE = 1;
constexpr usize TEXT_FIRST_COLUMN = 1;

} // namespace

LineColumn line_column(const std::string_view text, const usize offset) noexcept
{
    LineColumn result{};
    result.line = TEXT_FIRST_LINE;
    result.column = TEXT_FIRST_COLUMN;
    const usize limit = offset < text.size() ? offset : text.size();
    for (usize i = 0; i < limit; ++i) {
        if (text[i] == TEXT_NEWLINE_CHAR) {
            ++result.line;
            result.column = TEXT_FIRST_COLUMN;
        } else {
            ++result.column;
        }
    }
    return result;
}

} // namespace aurex::base
