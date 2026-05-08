#pragma once

#include "aurex/syntax/token.hpp"

#include <string_view>

namespace aurex::lex {

[[nodiscard]] syntax::TokenKind keyword_kind(std::string_view text) noexcept;

} // namespace aurex::lex
