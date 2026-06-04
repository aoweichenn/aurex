add_library(aurex_lex
    src/frontend/lex/sources/keyword.cpp
    src/frontend/lex/sources/lexer.cpp
    src/frontend/lex/sources/lexer_numbers.cpp
    src/frontend/lex/sources/lexer_strings.cpp
    src/frontend/lex/sources/lexer_trivia.cpp
    src/frontend/lex/sources/token_buffer.cpp
)
target_link_libraries(aurex_lex PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_lex
    PUBLIC include
    PRIVATE src
)

add_library(aurex_parse
    src/frontend/parse/support/sources/bracket_suffix_classifier.cpp
    src/frontend/parse/support/sources/type_arg_expr_converter.cpp
    src/frontend/parse/core/lossless_parse.cpp
    src/frontend/parse/grammar/parser_aggregate.cpp
    src/frontend/parse/grammar/parser_block.cpp
    src/frontend/parse/grammar/parser_builtin_expr.cpp
    src/frontend/parse/grammar/parser_container_item.cpp
    src/frontend/parse/grammar/parser_control_stmt.cpp
    src/frontend/parse/core/parser_cursor.cpp
    src/frontend/parse/core/parser_diagnostics.cpp
    src/frontend/parse/core/parser.cpp
    src/frontend/parse/grammar/parser_expr.cpp
    src/frontend/parse/grammar/parser_fn.cpp
    src/frontend/parse/grammar/parser_item.cpp
    src/frontend/parse/grammar/parser_module_item.cpp
    src/frontend/parse/grammar/parser_name_expr.cpp
    src/frontend/parse/core/parser_part_core.cpp
    src/frontend/parse/core/parser_part_ranges.cpp
    src/frontend/parse/core/parser_part_router.cpp
    src/frontend/parse/grammar/parser_pattern.cpp
    src/frontend/parse/grammar/parser_postfix.cpp
    src/frontend/parse/grammar/parser_primary.cpp
    src/frontend/parse/recovery/sources/parser_recovery.cpp
    src/frontend/parse/recovery/sources/parser_recovery_boundary_sets.cpp
    src/frontend/parse/recovery/sources/parser_recovery_delimiter_sets.cpp
    src/frontend/parse/recovery/sources/parser_recovery_start_sets.cpp
    src/frontend/parse/grammar/parser_stmt.cpp
    src/frontend/parse/grammar/parser_trait.cpp
    src/frontend/parse/grammar/parser_type.cpp
)
target_link_libraries(aurex_parse PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_parse
    PUBLIC include
    PRIVATE src
)
