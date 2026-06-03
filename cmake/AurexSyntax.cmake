add_library(aurex_syntax
    src/frontend/syntax/ast/expr_nodes.cpp
    src/frontend/syntax/ast/item_nodes.cpp
    src/frontend/syntax/ast/module.cpp
    src/frontend/syntax/ast/pattern_nodes.cpp
    src/frontend/syntax/ast/stmt_nodes.cpp
    src/frontend/syntax/ast/type_nodes.cpp
    src/frontend/syntax/ast/dump.cpp
    src/frontend/syntax/core/identifier.cpp
    src/frontend/syntax/core/lossless.cpp
    src/frontend/syntax/core/module.cpp
)
target_link_libraries(aurex_syntax PUBLIC aurex_base)
target_include_directories(aurex_syntax PUBLIC include)
