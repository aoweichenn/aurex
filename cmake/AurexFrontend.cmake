add_library(m0_lex
    src/lex/lexer.cpp
)
target_link_libraries(m0_lex PUBLIC m0_base m0_syntax)
target_include_directories(m0_lex PUBLIC include)

add_library(m0_parse
    src/parse/parser.cpp
)
target_link_libraries(m0_parse PUBLIC m0_base m0_syntax)
target_include_directories(m0_parse PUBLIC include)
