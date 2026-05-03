add_library(aurex_lex
    src/lex/lexer.cpp
)
target_link_libraries(aurex_lex PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_lex PUBLIC include)

add_library(aurex_parse
    src/parse/parser.cpp
)
target_link_libraries(aurex_parse PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_parse PUBLIC include)
