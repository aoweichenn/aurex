add_library(aurex_lex
    src/lex/keyword.cpp
    src/lex/lexer.cpp
    src/lex/lexer_numbers.cpp
    src/lex/lexer_strings.cpp
    src/lex/lexer_trivia.cpp
)
target_link_libraries(aurex_lex PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_lex PUBLIC include)

add_library(aurex_parse
    src/parse/parser_aggregate.cpp
    src/parse/parser_block.cpp
    src/parse/parser_builtin_expr.cpp
    src/parse/parser_container_item.cpp
    src/parse/parser_control_stmt.cpp
    src/parse/parser.cpp
    src/parse/parser_expr.cpp
    src/parse/parser_fn.cpp
    src/parse/parser_item.cpp
    src/parse/parser_module_item.cpp
    src/parse/parser_name_expr.cpp
    src/parse/parser_parts.cpp
    src/parse/parser_pattern.cpp
    src/parse/parser_postfix.cpp
    src/parse/parser_primary.cpp
    src/parse/parser_recovery.cpp
    src/parse/parser_recovery_boundary_sets.cpp
    src/parse/parser_recovery_start_sets.cpp
    src/parse/parser_stmt.cpp
    src/parse/parser_type.cpp
)
target_link_libraries(aurex_parse PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_parse PUBLIC include)
