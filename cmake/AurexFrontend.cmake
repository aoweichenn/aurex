add_library(aurex_lex
    src/lex/keyword.cpp
    src/lex/lexer.cpp
    src/lex/lexer_literals.cpp
    src/lex/lexer_trivia.cpp
)
target_link_libraries(aurex_lex PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_lex PUBLIC include)

add_library(aurex_parse
    src/parse/parser_aggregate.cpp
    src/parse/parser_container_item.cpp
    src/parse/parser.cpp
    src/parse/parser_expr.cpp
    src/parse/parser_fn.cpp
    src/parse/parser_item.cpp
    src/parse/parser_module_item.cpp
    src/parse/parser_parts.cpp
    src/parse/parser_pattern.cpp
    src/parse/parser_primary.cpp
    src/parse/parser_stmt.cpp
    src/parse/parser_type.cpp
)
target_link_libraries(aurex_parse PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_parse PUBLIC include)
