#pragma once

#include <aurex/parse/parser_part_ranges.hpp>

namespace aurex::parse {

class ParserPartBase : protected ParserPartRangeReader {
protected:
    explicit ParserPartBase(Parser& parser) noexcept
        : ParserPartRangeReader(parser) {}
};

} // namespace aurex::parse
