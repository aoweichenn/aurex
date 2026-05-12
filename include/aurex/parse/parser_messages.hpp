#pragma once

#include <string_view>

namespace aurex::parse {

inline constexpr std::string_view PARSER_M2_GENERIC_BOUNDS_UNSUPPORTED =
    "generic bounds are not part of M2 syntax";

inline constexpr std::string_view PARSER_M2_GENERIC_ENUM_UNSUPPORTED =
    "generic enums are not part of M2 syntax";

inline constexpr std::string_view PARSER_M2_GENERIC_TYPE_ALIAS_UNSUPPORTED =
    "generic type aliases are not part of M2 syntax";

inline constexpr std::string_view PARSER_M2_GENERIC_WHERE_UNSUPPORTED =
    "where clauses are not part of M2 syntax";

inline constexpr std::string_view PARSER_M2_LEGACY_ANGLE_GENERIC_UNSUPPORTED =
    "Aurex generics use '[' and ']'; '<' and '>' are not generic delimiters";

inline constexpr std::string_view PARSER_M2_RANGE_FOR_ONLY_RANGE =
    "M2 range-for only supports range(...); generic iteration is not part of M2 syntax";

inline constexpr std::string_view PARSER_M2_EXPLICIT_GENERIC_CALL_SYNTAX =
    "explicit generic calls use '::[...]', for example id::[i32](...)";

inline constexpr std::string_view PARSER_M2_IF_EXPR_REQUIRES_ELSE =
    "if expression requires an else branch";

inline constexpr std::string_view PARSER_M2_BLOCK_RESULT_ASSIGNMENT =
    "assignment cannot be used as block result";

} // namespace aurex::parse
