#include <aurex/base/text.hpp>

namespace aurex::base {

LineColumn line_column(const std::string_view text, const usize offset) noexcept {
    LineColumn result {};
    const usize limit = offset < text.size() ? offset : text.size();
    for (usize i = 0; i < limit; ++i) {
        if (text[i] == '\n') {
            ++result.line;
            result.column = 1;
        } else {
            ++result.column;
        }
    }
    return result;
}

} // namespace aurex::base
