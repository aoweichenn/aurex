add_library(aurex_syntax
    src/syntax/ast_dump.cpp
    src/syntax/identifier.cpp
    src/syntax/module.cpp
)
target_link_libraries(aurex_syntax PUBLIC aurex_base)
target_include_directories(aurex_syntax PUBLIC include)
