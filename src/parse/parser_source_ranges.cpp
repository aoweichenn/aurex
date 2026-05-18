#include <aurex/parse/parser.hpp>

namespace aurex::parse {

base::SourceRange Parser::merge(const base::SourceRange& begin, const base::SourceRange& end) const noexcept
{
    return base::SourceRange{begin.source, begin.begin, end.end};
}

} // namespace aurex::parse
