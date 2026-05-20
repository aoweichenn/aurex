add_library(aurex_syntax
    src/syntax/ast_expr_nodes.cpp
    src/syntax/ast_item_nodes.cpp
    src/syntax/ast_module.cpp
    src/syntax/ast_pattern_nodes.cpp
    src/syntax/ast_stmt_nodes.cpp
    src/syntax/ast_type_nodes.cpp
    src/syntax/ast_dump.cpp
    src/syntax/identifier.cpp
    src/syntax/lossless.cpp
    src/syntax/module.cpp
)
target_link_libraries(aurex_syntax PUBLIC aurex_base)
target_include_directories(aurex_syntax PUBLIC include)
